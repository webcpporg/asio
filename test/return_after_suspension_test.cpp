// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// A main that suspended in run and then returns 1, without webcpp::test::exit_status, fails on
// both variants (doc: #asio-building-exit-status): JSPI keeps the 1; Asyncify loses it, and
// node would exit 0, which the test support turns into 1 with its line. Its status is in its
// .output.

#include <webcpp/asio/io_context.hpp>
#include <webcpp/asio/steady_timer.hpp>

#include <boost/system/error_code.hpp>

#include <chrono>

int main() {
    webcpp::asio::io_context io;
    webcpp::asio::steady_timer timer{io, std::chrono::milliseconds{20}};
    timer.async_wait([](const boost::system::error_code& /*error*/) {});
    io.run();  // lint-run: the event loop that suspends once, on the timer
    return 1;
}
