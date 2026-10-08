// ProsperoLichess - Asynchronous HTTP client: worker lanes, NDJSON streams, rate gate.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "net/http.hpp"
#include "net/sync.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <pthread.h>
#include <string>
#include <string_view>
#include <vector>

namespace pch::net
{

enum class Lane : std::uint8_t
{
    general,  // serial REST traffic; paused by the rate gate
    priority, // game moves and other latency-critical calls; never paused
};

struct Result
{
    enum class Kind : std::uint8_t
    {
        response,      // a submit() finished: response is set
        stream_line,   // one non-empty NDJSON line: line is set
        stream_closed, // a stream ended (EOF, error or close_stream): response is set
    };
    Kind kind = Kind::response;
    std::uint64_t id = 0;
    Response response;
    std::string line;
};

struct ClientOptions
{
    // App version for the default User-Agent ("dev" when empty).
    std::string version;
    // How long an HTTP 429 pauses the general lane and new stream opens.
    std::int64_t rate_limit_pause_ms = 60000;
    std::size_t max_streams = 4;
    std::size_t max_line_bytes = 256u * 1024u;
};

// "ProsperoLichess/<version> (PS5; +https://github.com/blackbearreloaded/ProsperoLichess)"
std::string user_agent(std::string_view version);

// Every public method is meant for the main (UI) thread and never blocks on the
// network; work happens on pthreads and comes back through poll().
class Client
{
  public:
    explicit Client(Backend &backend, ClientOptions options = {});
    ~Client();
    Client(const Client &) = delete;
    Client &operator=(const Client &) = delete;

    // Initialises the backend and starts both lanes. Idempotent.
    bool start(std::string *error);
    // Aborts in-flight transfers, drops queued work and joins every thread.
    void shutdown();
    bool running() const;

    // Adds "Authorization: Bearer <token>" to requests submitted afterwards.
    void set_token(std::string token);
    void clear_token();
    bool has_token() const;

    // Queues a request; the Result::Kind::response with this id arrives via poll().
    // 0 when the client is not running.
    std::uint64_t submit(Lane lane, Request request);

    // Opens an NDJSON stream on its own thread. Lines arrive as stream_line
    // results, then exactly one stream_closed. 0 when not running or when
    // max_streams are already open.
    std::uint64_t open_stream(Request request);
    // Cancels promptly; stream_closed (error "cancelled") still follows.
    void close_stream(std::uint64_t id);
    std::size_t open_streams() const;
    // Milliseconds since the stream last received any byte (keep-alive
    // newlines included), or -1 for an unknown/finished stream.
    std::int64_t stream_idle_ms(std::uint64_t id) const;

    // Pops the next result; false when there is none.
    bool poll(Result *out);

    // Remaining rate-limit pause after a 429, 0 when not paused.
    double seconds_until_resume() const;

  private:
    struct Job
    {
        std::uint64_t id = 0;
        Request request;
    };
    struct Worker
    {
        Client *owner = nullptr;
        Lane lane = Lane::general;
        pthread_t thread{};
        bool started = false;
        std::deque<Job> queue;
        CancelToken cancel;
    };
    struct Stream
    {
        Client *owner = nullptr;
        std::uint64_t id = 0;
        Request request;
        pthread_t thread{};
        CancelToken cancel;
        std::atomic<bool> done{false};
        std::atomic<std::int64_t> last_activity_us{0};
    };

    static void *worker_main(void *context);
    static void *stream_main(void *context);
    void run_worker(Worker &worker);
    void run_stream(Stream &stream);
    void add_default_headers(Request &request) const; // requires mutex_
    void note_response(const Response &response);
    void push(Result result);
    std::int64_t gate_remaining_us() const;
    void reap_streams(); // requires mutex_; joins finished stream threads

    Backend &backend_;
    ClientOptions options_;
    std::string user_agent_;
    mutable Mutex mutex_;
    CondVar wake_;
    bool running_ = false;
    bool stopping_ = false;
    std::string token_;
    std::uint64_t next_id_ = 1;
    Worker workers_[2];
    std::vector<std::unique_ptr<Stream>> streams_;
    std::deque<Result> results_;
    std::atomic<std::int64_t> resume_at_us_{0};
};

} // namespace pch::net
