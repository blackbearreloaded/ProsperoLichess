// ProsperoLichess - Tiny localhost HTTP/1.1 server for network-layer tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <arpa/inet.h>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <mutex>
#include <netinet/in.h>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace pch::test
{

inline bool send_text(int fd, const std::string &text)
{
    std::size_t done = 0;
    while (done < text.size())
    {
        const ssize_t sent = ::send(fd, text.data() + done, text.size() - done, MSG_NOSIGNAL);
        if (sent <= 0)
            return false;
        done += static_cast<std::size_t>(sent);
    }
    return true;
}

// Connects to 127.0.0.1:port, sends raw bytes and returns everything received
// until the peer closes (or 3 s pass).
inline std::string raw_exchange(std::uint16_t port, const std::string &bytes)
{
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    std::string reply;
    if (::connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0)
    {
        send_text(fd, bytes);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        char buffer[2048];
        while (std::chrono::steady_clock::now() < deadline)
        {
            pollfd entry{fd, POLLIN, 0};
            if (::poll(&entry, 1, 100) <= 0)
                continue;
            const ssize_t got = ::recv(fd, buffer, sizeof(buffer), 0);
            if (got <= 0)
                break;
            reply.append(buffer, static_cast<std::size_t>(got));
        }
    }
    ::close(fd);
    return reply;
}

// Routes:
//   GET  /hello    200 "hello world"
//   ANY  /echo     200 "method=..\nauth=..\nctype=..\nua=..\nbody=.."
//   GET  /stream   chunked NDJSON: n:1, pause, keep-alive, n:2 (split), pause, n:3
//   GET  /hang     chunked: n:1 then nothing until the client disconnects
//   GET  /limited  429
class TestServer
{
  public:
    static constexpr int kStreamPauseMs = 400;

    TestServer()
    {
        listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        const int one = 1;
        setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::bind(listen_fd_, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0 ||
            ::listen(listen_fd_, 16) != 0)
        {
            std::perror("test server");
            std::abort();
        }
        socklen_t length = sizeof(address);
        getsockname(listen_fd_, reinterpret_cast<sockaddr *>(&address), &length);
        port_ = ntohs(address.sin_port);
        acceptor_ = std::thread([this] { accept_loop(); });
    }

    ~TestServer()
    {
        stop_.store(true);
        acceptor_.join();
        std::vector<std::thread> workers;
        {
            std::lock_guard<std::mutex> guard(mutex_);
            workers.swap(workers_);
        }
        for (std::thread &worker : workers)
            worker.join();
        ::close(listen_fd_);
    }

    std::uint16_t port() const
    {
        return port_;
    }
    std::string url(const std::string &path) const
    {
        return "http://127.0.0.1:" + std::to_string(port_) + path;
    }
    int hang_disconnects() const
    {
        return hang_disconnects_.load();
    }

  private:
    struct Parsed
    {
        std::string method;
        std::string path;
        std::map<std::string, std::string> headers; // lower-case names
        std::string body;
    };

    void accept_loop()
    {
        while (!stop_.load())
        {
            pollfd entry{listen_fd_, POLLIN, 0};
            if (::poll(&entry, 1, 50) <= 0)
                continue;
            const int fd = ::accept(listen_fd_, nullptr, nullptr);
            if (fd < 0)
                continue;
            std::lock_guard<std::mutex> guard(mutex_);
            workers_.emplace_back(
                [this, fd]
                {
                    handle(fd);
                    ::close(fd);
                });
        }
    }

    bool read_request(int fd, Parsed *out)
    {
        std::string data;
        char buffer[4096];
        std::size_t head_end = std::string::npos;
        while (head_end == std::string::npos)
        {
            pollfd entry{fd, POLLIN, 0};
            if (::poll(&entry, 1, 50) <= 0)
            {
                if (stop_.load())
                    return false;
                continue;
            }
            const ssize_t got = ::recv(fd, buffer, sizeof(buffer), 0);
            if (got <= 0)
                return false;
            data.append(buffer, static_cast<std::size_t>(got));
            head_end = data.find("\r\n\r\n");
        }
        const std::string head = data.substr(0, head_end);
        std::size_t line_end = head.find("\r\n");
        const std::string line = head.substr(0, line_end);
        const std::size_t sp1 = line.find(' ');
        const std::size_t sp2 = line.find(' ', sp1 + 1);
        out->method = line.substr(0, sp1);
        out->path = line.substr(sp1 + 1, sp2 - sp1 - 1);
        while (line_end != std::string::npos && line_end < head.size())
        {
            const std::size_t start = line_end + 2;
            line_end = head.find("\r\n", start);
            const std::string header = head.substr(start, line_end - start);
            const std::size_t colon = header.find(':');
            if (colon == std::string::npos)
                continue;
            std::string name = header.substr(0, colon);
            for (char &c : name)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            std::size_t value_start = colon + 1;
            while (value_start < header.size() && header[value_start] == ' ')
                ++value_start;
            out->headers[name] = header.substr(value_start);
        }
        out->body = data.substr(head_end + 4);
        const auto length = out->headers.find("content-length");
        const std::size_t want = length == out->headers.end() ? 0 : std::stoul(length->second);
        while (out->body.size() < want)
        {
            const ssize_t got = ::recv(fd, buffer, sizeof(buffer), 0);
            if (got <= 0)
                return false;
            out->body.append(buffer, static_cast<std::size_t>(got));
        }
        return true;
    }

    static std::string simple(int status, const char *reason, const std::string &body)
    {
        return "HTTP/1.1 " + std::to_string(status) + " " + reason +
               "\r\nContent-Type: text/plain\r\nContent-Length: " + std::to_string(body.size()) +
               "\r\nConnection: close\r\n\r\n" + body;
    }

    static bool chunk(int fd, const std::string &data)
    {
        char size[32];
        std::snprintf(size, sizeof(size), "%zx\r\n", data.size());
        return send_text(fd, size + data + "\r\n");
    }

    bool pause(int ms)
    {
        const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
        while (std::chrono::steady_clock::now() < until)
        {
            if (stop_.load())
                return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return true;
    }

    void handle(int fd)
    {
        Parsed request;
        if (!read_request(fd, &request))
            return;
        const auto header = [&](const char *name)
        {
            const auto found = request.headers.find(name);
            return found == request.headers.end() ? std::string() : found->second;
        };
        static const std::string kChunkedHead = "HTTP/1.1 200 OK\r\nContent-Type: "
                                                "application/x-ndjson\r\nTransfer-Encoding: "
                                                "chunked\r\nConnection: close\r\n\r\n";
        if (request.path == "/hello")
        {
            send_text(fd, simple(200, "OK", "hello world"));
        }
        else if (request.path == "/echo")
        {
            send_text(fd, simple(200, "OK",
                                 "method=" + request.method + "\nauth=" + header("authorization") +
                                     "\nctype=" + header("content-type") +
                                     "\nua=" + header("user-agent") + "\nbody=" + request.body));
        }
        else if (request.path == "/stream")
        {
            send_text(fd, kChunkedHead);
            chunk(fd, "{\"n\":1}\n");
            if (!pause(kStreamPauseMs))
                return;
            chunk(fd, "\n");
            chunk(fd, "{\"n\"");
            chunk(fd, ":2}\r");
            chunk(fd, "\n");
            if (!pause(kStreamPauseMs))
                return;
            chunk(fd, "{\"n\":3}\n");
            send_text(fd, "0\r\n\r\n");
        }
        else if (request.path == "/hang")
        {
            send_text(fd, kChunkedHead);
            chunk(fd, "{\"n\":1}\n");
            char byte;
            while (!stop_.load())
            {
                pollfd entry{fd, POLLIN, 0};
                if (::poll(&entry, 1, 20) > 0 && ::recv(fd, &byte, 1, 0) <= 0)
                {
                    hang_disconnects_.fetch_add(1);
                    break;
                }
            }
        }
        else if (request.path == "/limited")
        {
            send_text(fd, simple(429, "Too Many Requests", "{\"error\":\"rate limited\"}"));
        }
        else
        {
            send_text(fd, simple(404, "Not Found", "missing"));
        }
    }

    int listen_fd_ = -1;
    std::uint16_t port_ = 0;
    std::atomic<bool> stop_{false};
    std::atomic<int> hang_disconnects_{0};
    std::thread acceptor_;
    std::mutex mutex_;
    std::vector<std::thread> workers_;
};

} // namespace pch::test
