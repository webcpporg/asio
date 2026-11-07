// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/asio/io_context.hpp>
#include <webcpp/asio/steady_timer.hpp>

#include <webcpp/test/exit_status.hpp>

#include <boost/core/lightweight_test.hpp>
#include <webcpp/asio/as_tuple.hpp>
#include <webcpp/asio/awaitable.hpp>
#include <webcpp/asio/co_spawn.hpp>
#include <webcpp/asio/this_coro.hpp>
#include <webcpp/asio/use_awaitable.hpp>

#include <chrono>
#include <exception>

namespace asio = webcpp::asio;

namespace {

asio::awaitable<int> wait_for_timer() {
    const auto executor = co_await asio::this_coro::executor;
    webcpp::asio::steady_timer timer(executor, std::chrono::milliseconds(5));
    auto [error] = co_await timer.async_wait(asio::as_tuple(asio::use_awaitable));
    BOOST_TEST(!error);
    co_return error ? 0 : 42;
}

}  // namespace

int main() {
    webcpp::asio::io_context context;
    int completions = 0;
    asio::co_spawn(context, wait_for_timer(), [&](const std::exception_ptr& error, int value) {
        BOOST_TEST(!error);
        BOOST_TEST_EQ(value, 42);
        ++completions;
    });
    context.run();  // lint-run: resume an awaitable after a timer completion
    BOOST_TEST_EQ(completions, 1);
    return webcpp::test::exit_status(boost::report_errors());
}
