// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// A separately compiled acceptor must receive the common composition fix too.
#include <webcpp/test/exit_status.hpp>

#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/cobalt/io/acceptor.hpp>
#include <boost/cobalt/main.hpp>
#include <boost/cobalt/promise.hpp>
#include <boost/cobalt/result.hpp>
#include <boost/cobalt/this_coro.hpp>
#include <boost/core/lightweight_test.hpp>

#include <chrono>
#include <cstdio>
#include <cstdlib>

namespace cobalt = boost::cobalt;
namespace io = cobalt::io;

namespace {

cobalt::promise<void> accept_stream(io::acceptor& listener) {
    const auto [error, socket] = co_await cobalt::as_tuple(listener.accept());
    BOOST_TEST(error == boost::asio::error::operation_aborted);
    BOOST_TEST(!socket.is_open());
}

}  // namespace

cobalt::main co_main([[maybe_unused]] int argc, [[maybe_unused]] char** argv) {
    // NOLINTNEXTLINE(readability-static-accessed-through-instance)
    const auto exec = co_await cobalt::this_coro::executor;
    boost::asio::steady_timer watchdog(exec, std::chrono::seconds(5));
    watchdog.async_wait([](boost::system::error_code error) {
        if (!error) {
            static_cast<void>(std::fputs("pending accept did not receive cancellation\n", stderr));
            std::abort();
        }
    });
    io::acceptor listener({io::tcp_v4, "127.0.0.1", 0}, exec);
    // The promise eagerly initiates accept. No client connects to this listener.
    auto pending = accept_stream(listener);
    BOOST_TEST(!pending.ready());
    boost::asio::post(exec, [&pending] { pending.cancel(); });
    co_await pending;
    watchdog.cancel();
    std::puts("cobalt: pending composed accept cancelled through its promise");
    co_return webcpp::test::exit_status(boost::report_errors());
}
