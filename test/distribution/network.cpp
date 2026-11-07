// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)
// See accompanying file LICENSE_1_0.txt or https://www.boost.org/LICENSE_1_0.txt.

#include <webcpp/asio.hpp>
#include <webcpp/asio/buffer.hpp>
#include <webcpp/asio/ip/address.hpp>
#include <webcpp/asio/read.hpp>
#include <webcpp/asio/write.hpp>

#include <array>
#include <chrono>
#include <cstdio>
#include <string>

int main() {
    namespace net = webcpp::asio;
    using tcp = net::ip::tcp;
    using udp = net::ip::udp;
    using namespace std::chrono_literals;
    net::io_context io;
    int completions = 0;
    int failures = 0;
    net::steady_timer deadline(io, 5s);
    auto check = [&](bool good) {
        if (!good) {
            ++failures;
        }
        if (++completions == 8) {
            deadline.cancel();
        }
    };
    deadline.async_wait([&](boost::system::error_code ec) {
        if (!ec) {
            ++failures;
            io.stop();
        }
    });

    tcp::acceptor listener(io, {tcp::v4(), 0});
    tcp::socket server(io), client(io);
    std::array<char, 4> tcp_in{};
    const std::array<char, 4> tcp_out{'t', 'c', 'p', '!'};
    listener.async_accept(server, [&](boost::system::error_code ec) {
        check(!ec);
        if (!ec) {
            net::async_read(server, net::buffer(tcp_in),
                            [&](boost::system::error_code error, std::size_t count) {
                                check(!error && count == tcp_in.size() && tcp_in == tcp_out);
                            });
        }
    });
    client.async_connect({net::ip::make_address("127.0.0.1"), listener.local_endpoint().port()},
                         [&](boost::system::error_code ec) {
                             check(!ec);
                             if (!ec) {
                                 net::async_write(
                                     client, net::buffer(tcp_out),
                                     [&](boost::system::error_code error, std::size_t count) {
                                         check(!error && count == tcp_out.size());
                                     });
                             }
                         });

    udp::socket receiver(io, {udp::v4(), 0}), sender(io, udp::v4());
    udp::endpoint peer;
    const std::array<char, 4> udp_out{'u', 'd', 'p', '!'};
    std::array<char, 4> udp_in{};
    receiver.async_receive_from(net::buffer(udp_in), peer,
                                [&](boost::system::error_code ec, std::size_t count) {
                                    check(!ec && count == udp_in.size() && udp_in == udp_out);
                                });
    sender.async_send_to(net::buffer(udp_out),
                         {net::ip::make_address("127.0.0.1"), receiver.local_endpoint().port()},
                         [&](boost::system::error_code ec, std::size_t count) {
                             check(!ec && count == udp_out.size());
                         });

    tcp::resolver resolver(io);
    bool initiating = true;
    resolver.async_resolve("localhost", "80", [&](boost::system::error_code ec, auto results) {
        check(!initiating && !ec && !results.empty());
    });
    initiating = false;
    net::steady_timer timer(io, 2ms);
    timer.async_wait([&](boost::system::error_code ec) { check(!ec); });
    io.run();  // lint-run: the consumer owns its event loop
    const bool passed = failures == 0 && completions == 8;
    std::printf("distribution timer/TCP/UDP/DNS: %s (%d completions, %d failures)\n",
                passed ? "passed" : "failed", completions, failures);
    return passed ? 0 : 1;
}
