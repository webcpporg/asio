// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// Native aliases preserve Boost type identity. WebAssembly contexts retain the
// existing options while their separately shipped backend is prepared (doc: #asio-names).

#include <webcpp/asio.hpp>

#include <webcpp/test/exit_status.hpp>

#include <boost/core/lightweight_test.hpp>
#include <webcpp/asio/io_context.hpp>
#include <webcpp/asio/post.hpp>
#include <webcpp/asio/steady_timer.hpp>
#include <webcpp/asio/system_timer.hpp>

#include <chrono>
#include <type_traits>

// The build supplies an independent expectation for configuration regressions.
#ifndef WEBCPP_TEST_TURN_LIMIT
#define WEBCPP_TEST_TURN_LIMIT 100
#endif

static_assert(std::is_base_of_v<webcpp::asio_backend::io_context, webcpp::asio::io_context>);
#ifdef WEBCPP_TARGET_NATIVE
static_assert(std::is_same_v<webcpp::asio::io_context, boost::asio::io_context>);
#endif
static_assert(std::is_convertible_v<webcpp::asio::io_context&, webcpp::asio_backend::io_context&>);
static_assert(std::is_same_v<webcpp::asio::io_context::executor_type,
                             webcpp::asio_backend::io_context::executor_type>);
static_assert(std::is_same_v<webcpp::asio::steady_timer, webcpp::asio_backend::steady_timer>);
static_assert(std::is_same_v<webcpp::asio::system_timer, webcpp::asio_backend::system_timer>);
static_assert(
    std::is_same_v<webcpp::asio::basic_waitable_timer<std::chrono::steady_clock>,
                   webcpp::asio_backend::basic_waitable_timer<std::chrono::steady_clock>>);

#ifndef __EMSCRIPTEN__
static_assert(
    std::is_same_v<webcpp::asio::basic_stream_socket<webcpp::asio::ip::tcp>,
                   webcpp::asio_backend::basic_stream_socket<webcpp::asio_backend::ip::tcp>>);
static_assert(
    std::is_same_v<webcpp::asio::basic_datagram_socket<webcpp::asio::ip::udp>,
                   webcpp::asio_backend::basic_datagram_socket<webcpp::asio_backend::ip::udp>>);
static_assert(
    std::is_same_v<webcpp::asio::basic_socket_acceptor<webcpp::asio::ip::tcp>,
                   webcpp::asio_backend::basic_socket_acceptor<webcpp::asio_backend::ip::tcp>>);
static_assert(
    std::is_same_v<webcpp::asio::ip::tcp::resolver, webcpp::asio_backend::ip::tcp::resolver>);
static_assert(std::is_same_v<webcpp::asio::ip::tcp, webcpp::asio_backend::ip::tcp>);
static_assert(std::is_same_v<webcpp::asio::ip::udp, webcpp::asio_backend::ip::udp>);
static_assert(std::is_same_v<webcpp::asio::ip::tcp::socket, webcpp::asio_backend::ip::tcp::socket>);
static_assert(std::is_same_v<webcpp::asio::ip::udp::socket, webcpp::asio_backend::ip::udp::socket>);
static_assert(
    std::is_same_v<webcpp::asio::ip::tcp::acceptor, webcpp::asio_backend::ip::tcp::acceptor>);
#endif

namespace {

void options_are_kept() {
#ifndef WEBCPP_TARGET_NATIVE
    webcpp::asio::io_context io;
    BOOST_TEST_EQ(io.max_handlers_per_turn(), WEBCPP_TEST_TURN_LIMIT);
    io.max_handlers_per_turn(250);
    BOOST_TEST_EQ(io.max_handlers_per_turn(), 250U);
    io.max_handlers_per_turn(0);
    BOOST_TEST_EQ(io.max_handlers_per_turn(), 1U);
    const webcpp::asio::io_context configured(
        webcpp::asio::io_context_options{.max_handlers_per_turn = 64});
    BOOST_TEST_EQ(configured.max_handlers_per_turn(), 64U);
    const webcpp::asio::io_context zero(
        webcpp::asio::io_context_options{.max_handlers_per_turn = 0});
    BOOST_TEST_EQ(zero.max_handlers_per_turn(), 1U);
#endif
}

void a_posted_handler_runs() {
    webcpp::asio::io_context io;
    int ran = 0;
    webcpp::asio::post(io, [&ran] { ++ran; });
    BOOST_TEST_EQ(io.run(), 1U);  // lint-run: the test's event loop
    BOOST_TEST_EQ(ran, 1);
}

// Boost's idiom, a concurrency hint of 1, with the options or without.
void a_concurrency_hint_is_boost_s() {
    webcpp::asio::io_context io{1};
#ifndef WEBCPP_TARGET_NATIVE
    BOOST_TEST_EQ(io.max_handlers_per_turn(), WEBCPP_TEST_TURN_LIMIT);
    const webcpp::asio::io_context hinted(
        WEBCPP_ASIO_BACKEND_CONCURRENCY_HINT_1,
        webcpp::asio::io_context_options{.max_handlers_per_turn = 8});
    BOOST_TEST_EQ(hinted.max_handlers_per_turn(), 8U);
#endif
    int ran = 0;
    webcpp::asio::post(io, [&ran] { ++ran; });
    BOOST_TEST_EQ(io.run(), 1U);  // lint-run: the test's event loop
    BOOST_TEST_EQ(ran, 1);
}

}  // namespace

int main() {
    options_are_kept();
    a_posted_handler_runs();
    a_concurrency_hint_is_boost_s();
    return webcpp::test::exit_status(boost::report_errors());
}
