// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// The reactor exists from construction: a run that has work and no timer or socket of the
// program's waits in the reactor's select, which a post made from a browser event wakes.
// Without it Boost's run waits in ::pause(), which nothing wakes, and this test fails with the
// test support's line (measured with Emscripten 6.0.11). Work posted before run waits for run
// (doc: #asio-invariant-2).

#include <webcpp/asio/io_context.hpp>

#include <webcpp/test/exit_status.hpp>

#include <emscripten/em_asm.h>
#include <emscripten/em_macros.h>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/post.hpp>
#include <boost/core/lightweight_test.hpp>

#include <optional>

namespace {

/** An io_context held by outstanding work that only a browser event releases. */
struct waiting {
    webcpp::asio::io_context io;
    std::optional<boost::asio::executor_work_guard<webcpp::asio::io_context::executor_type>> guard{
        io.get_executor()};
    int released = 0;
};

}  // namespace

// Called by a JavaScript setTimeout while main is suspended in run: it posts, nothing more.
extern "C" EMSCRIPTEN_KEEPALIVE void test_release(waiting* state) {
    boost::asio::post(state->io, [state] {
        ++state->released;
        state->guard.reset();
    });
}

namespace {

void a_post_from_an_event_wakes_run() {
    waiting state;
    // A function expression: clang-format splits an arrow function's => into = >.
    EM_ASM({ setTimeout(function() { _test_release($0); }, 20); }, &state);
    BOOST_TEST_EQ(state.io.run(), 1U);  // lint-run: the test's event loop, woken by an event
    BOOST_TEST_EQ(state.released, 1);
}

void work_posted_before_run_waits_for_it() {
    webcpp::asio::io_context io;
    int ran = 0;
    boost::asio::post(io, [&ran] { ++ran; });
    BOOST_TEST_EQ(ran, 0);
    BOOST_TEST_EQ(io.run(), 1U);  // lint-run: the test's event loop
    BOOST_TEST_EQ(ran, 1);
}

}  // namespace

int main() {
    a_post_from_an_event_wakes_run();
    work_posted_before_run_waits_for_it();
    return webcpp::test::exit_status(boost::report_errors());
}
