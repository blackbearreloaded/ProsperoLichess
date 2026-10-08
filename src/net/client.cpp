// ProsperoLichess - Asynchronous HTTP client: worker lanes, NDJSON streams, rate gate.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "net/client.hpp"

#include "net/ndjson.hpp"
#include "net/redact.hpp"
#include "platform/ps5/system.hpp"

#include <utility>

namespace pch::net
{

namespace
{

bool has_header(const Request &request, std::string_view name)
{
    for (const Header &header : request.headers)
    {
        if (iequals(header.name, name))
            return true;
    }
    return false;
}

int wait_slice_ms(std::int64_t remaining_us)
{
    const std::int64_t ms = remaining_us / 1000 + 1;
    return ms > 100 ? 100 : static_cast<int>(ms);
}

} // namespace

std::string user_agent(std::string_view version)
{
    std::string agent = "ProsperoLichess/";
    agent += version.empty() ? std::string_view("dev") : version;
    agent += " (PS5; +https://github.com/blackbearreloaded/ProsperoLichess)";
    return agent;
}

Client::Client(Backend &backend, ClientOptions options)
    : backend_(backend), options_(std::move(options)), user_agent_(user_agent(options_.version))
{
    workers_[0].owner = this;
    workers_[0].lane = Lane::general;
    workers_[1].owner = this;
    workers_[1].lane = Lane::priority;
}

Client::~Client()
{
    shutdown();
}

bool Client::start(std::string *error)
{
    {
        Guard<Mutex> guard(mutex_);
        if (running_)
            return true;
    }
    if (!backend_.init(error))
        return false;
    {
        Guard<Mutex> guard(mutex_);
        stopping_ = false;
    }
    bool created = true;
    for (Worker &worker : workers_)
    {
        worker.cancel.reset();
        // Never name threads: pthread_setname_np hangs on PS5.
        if (pthread_create(&worker.thread, nullptr, &Client::worker_main, &worker) != 0)
        {
            created = false;
            break;
        }
        worker.started = true;
    }
    if (!created)
    {
        {
            Guard<Mutex> guard(mutex_);
            stopping_ = true;
            wake_.broadcast();
        }
        for (Worker &worker : workers_)
        {
            if (worker.started)
                pthread_join(worker.thread, nullptr);
            worker.started = false;
        }
        if (error != nullptr)
            *error = "cannot start network worker thread";
        return false;
    }
    Guard<Mutex> guard(mutex_);
    running_ = true;
    return true;
}

void Client::shutdown()
{
    std::vector<std::unique_ptr<Stream>> streams;
    {
        Guard<Mutex> guard(mutex_);
        if (!running_)
            return;
        running_ = false;
        stopping_ = true;
        for (Worker &worker : workers_)
        {
            worker.queue.clear();
            worker.cancel.cancel();
        }
        for (auto &stream : streams_)
            stream->cancel.cancel();
        streams.swap(streams_);
        wake_.broadcast();
    }
    for (Worker &worker : workers_)
    {
        if (worker.started)
            pthread_join(worker.thread, nullptr);
        worker.started = false;
    }
    for (auto &stream : streams)
        pthread_join(stream->thread, nullptr);
    Guard<Mutex> guard(mutex_);
    results_.clear();
}

bool Client::running() const
{
    Guard<Mutex> guard(mutex_);
    return running_;
}

void Client::set_token(std::string token)
{
    Guard<Mutex> guard(mutex_);
    token_ = std::move(token);
}

void Client::clear_token()
{
    Guard<Mutex> guard(mutex_);
    token_.clear();
}

bool Client::has_token() const
{
    Guard<Mutex> guard(mutex_);
    return !token_.empty();
}

void Client::add_default_headers(Request &request) const
{
    if (!has_header(request, "User-Agent"))
        request.headers.push_back({"User-Agent", user_agent_});
    if (request.authorize && !token_.empty() && !has_header(request, "Authorization"))
        request.headers.push_back({"Authorization", "Bearer " + token_});
}

std::uint64_t Client::submit(Lane lane, Request request)
{
    Guard<Mutex> guard(mutex_);
    if (!running_)
        return 0;
    add_default_headers(request);
    Worker &worker = workers_[lane == Lane::priority ? 1 : 0];
    const std::uint64_t id = next_id_++;
    worker.queue.push_back({id, std::move(request)});
    wake_.broadcast();
    return id;
}

std::uint64_t Client::open_stream(Request request)
{
    Guard<Mutex> guard(mutex_);
    if (!running_)
        return 0;
    reap_streams();
    std::size_t live = 0;
    for (const auto &stream : streams_)
        live += stream->done.load(std::memory_order_acquire) ? 0 : 1;
    if (live >= options_.max_streams)
        return 0;
    add_default_headers(request);
    auto stream = std::make_unique<Stream>();
    stream->owner = this;
    stream->id = next_id_++;
    stream->request = std::move(request);
    stream->last_activity_us.store(sys::monotonic_us(), std::memory_order_relaxed);
    if (pthread_create(&stream->thread, nullptr, &Client::stream_main, stream.get()) != 0)
        return 0;
    const std::uint64_t id = stream->id;
    streams_.push_back(std::move(stream));
    return id;
}

void Client::close_stream(std::uint64_t id)
{
    Guard<Mutex> guard(mutex_);
    for (auto &stream : streams_)
    {
        if (stream->id == id)
            stream->cancel.cancel();
    }
    wake_.broadcast();
}

std::size_t Client::open_streams() const
{
    Guard<Mutex> guard(mutex_);
    std::size_t live = 0;
    for (const auto &stream : streams_)
        live += stream->done.load(std::memory_order_acquire) ? 0 : 1;
    return live;
}

std::int64_t Client::stream_idle_ms(std::uint64_t id) const
{
    Guard<Mutex> guard(mutex_);
    for (const auto &stream : streams_)
    {
        if (stream->id == id && !stream->done.load(std::memory_order_acquire))
            return (sys::monotonic_us() - stream->last_activity_us.load()) / 1000;
    }
    return -1;
}

bool Client::poll(Result *out)
{
    Guard<Mutex> guard(mutex_);
    reap_streams();
    if (results_.empty() || out == nullptr)
        return false;
    *out = std::move(results_.front());
    results_.pop_front();
    return true;
}

double Client::seconds_until_resume() const
{
    const std::int64_t remaining = gate_remaining_us();
    return remaining > 0 ? static_cast<double>(remaining) / 1e6 : 0.0;
}

std::int64_t Client::gate_remaining_us() const
{
    return resume_at_us_.load(std::memory_order_acquire) - sys::monotonic_us();
}

void Client::reap_streams()
{
    for (std::size_t i = 0; i < streams_.size();)
    {
        if (streams_[i]->done.load(std::memory_order_acquire))
        {
            // The thread has published its last result and only has to return.
            pthread_join(streams_[i]->thread, nullptr);
            streams_.erase(streams_.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        ++i;
    }
}

void Client::note_response(const Response &response)
{
    if (response.status != 429)
        return;
    const std::int64_t until = sys::monotonic_us() + options_.rate_limit_pause_ms * 1000;
    std::int64_t current = resume_at_us_.load();
    while (current < until && !resume_at_us_.compare_exchange_weak(current, until))
    {
    }
    sys::log("net: HTTP 429, pausing general requests for %lld ms",
             static_cast<long long>(options_.rate_limit_pause_ms));
}

void Client::push(Result result)
{
    Guard<Mutex> guard(mutex_);
    if (stopping_)
        return;
    results_.push_back(std::move(result));
}

void *Client::worker_main(void *context)
{
    auto *worker = static_cast<Worker *>(context);
    worker->owner->run_worker(*worker);
    return nullptr;
}

void *Client::stream_main(void *context)
{
    auto *stream = static_cast<Stream *>(context);
    stream->owner->run_stream(*stream);
    return nullptr;
}

void Client::run_worker(Worker &worker)
{
    for (;;)
    {
        Job job;
        {
            Guard<Mutex> guard(mutex_);
            for (;;)
            {
                if (stopping_)
                    return;
                if (worker.queue.empty())
                {
                    wake_.wait(mutex_);
                    continue;
                }
                const std::int64_t paused = worker.lane == Lane::general ? gate_remaining_us() : 0;
                if (paused > 0)
                {
                    wake_.wait_for_ms(mutex_, wait_slice_ms(paused));
                    continue;
                }
                break;
            }
            job = std::move(worker.queue.front());
            worker.queue.pop_front();
        }
        Response response = backend_.perform(job.request, &worker.cancel);
        note_response(response);
        if (!response.error.empty())
        {
            sys::log("net: %s %s failed: %s", method_name(job.request.method),
                     redact(job.request.url).c_str(), response.error.c_str());
        }
        Result result;
        result.kind = Result::Kind::response;
        result.id = job.id;
        result.response = std::move(response);
        push(std::move(result));
    }
}

void Client::run_stream(Stream &stream)
{
    // New streams respect the rate gate too (a cancel or shutdown ends the wait).
    {
        Guard<Mutex> guard(mutex_);
        for (;;)
        {
            const std::int64_t paused = gate_remaining_us();
            if (stopping_ || stream.cancel.cancelled() || paused <= 0)
                break;
            wake_.wait_for_ms(mutex_, wait_slice_ms(paused));
        }
    }
    Response response;
    if (stream.cancel.cancelled())
    {
        response.error = "cancelled";
    }
    else
    {
        NdjsonSplitter splitter(options_.max_line_bytes);
        const NdjsonSplitter::LineFn deliver = [&](std::string_view line)
        {
            Result result;
            result.kind = Result::Kind::stream_line;
            result.id = stream.id;
            result.line.assign(line);
            push(std::move(result));
        };
        response = backend_.stream(
            stream.request,
            [&](std::string_view chunk)
            {
                stream.last_activity_us.store(sys::monotonic_us(), std::memory_order_relaxed);
                splitter.feed(chunk, deliver);
                return !stream.cancel.cancelled();
            },
            stream.cancel);
        if (stream.cancel.cancelled())
            response.error = "cancelled";
        else if (response.error.empty())
            splitter.flush(deliver);
        if (splitter.oversize_lines() != 0)
        {
            sys::log("net: stream %llu dropped %llu oversize line(s)",
                     static_cast<unsigned long long>(stream.id),
                     static_cast<unsigned long long>(splitter.oversize_lines()));
        }
        note_response(response);
    }
    Result result;
    result.kind = Result::Kind::stream_closed;
    result.id = stream.id;
    result.response = std::move(response);
    push(std::move(result));
    stream.done.store(true, std::memory_order_release);
}

} // namespace pch::net
