// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// Who may call the run family (doc: #asio-invariant-6): a call nested in a handler, one made from
// a JavaScript event while run waits, and one made while the bridge delivers are each refused,
// asserting and returning 0 having run nothing; the program's assertion handler counts the
// assertions when they are on, and with them off each refusal still returns 0. A refused call
// leaves the run around it running, and its refusal flagged for the next nested call. An
// exception that leaves run clears the flag, so that the next run after its catch is no nested
// call. Each public member admits once: a timed form whose handlers span several turns, or that
// waits on a timer alone, asserts nothing. And a context of its own with no reactor, as xactor's
// driver drains, is polled from a handler without a refusal.

#include <webcpp/asio/browser/bridge.hpp>
#include <webcpp/asio/io_context.hpp>
#include <webcpp/asio/steady_timer.hpp>

#include "browser_values.hpp"

#include <webcpp/test/exit_status.hpp>

#include <emscripten/em_asm.h>
#include <emscripten/em_macros.h>
#include <emscripten/val.h>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/system/error_code.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <stdexcept>

namespace {

using guard_type = boost::asio::executor_work_guard<webcpp::asio::io_context::executor_type>;

/** The assertions the run family made, which the program's assertion handler counts. */
struct assertions {
    std::size_t failed = 0;
};

// The assertion handler is a free function Boost declares and the program defines, which only
// a global reaches.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
assertions seen;

}  // namespace

#ifdef BOOST_ENABLE_ASSERT_HANDLER
namespace boost {

void assertion_failed(const char* /*expr*/, const char* /*function*/, const char* /*file*/,
                      long /*line*/) {
    ++seen.failed;
}

void assertion_failed_msg(const char* /*expr*/, const char* /*msg*/, const char* /*function*/,
                          const char* /*file*/, long /*line*/) {
    ++seen.failed;
}

}  // namespace boost
#endif

/**
 What a JavaScript setTimeout calls while run waits: a call of run from an event, and then a post
 that lets run return, as an event may.
*/
extern "C" EMSCRIPTEN_KEEPALIVE void test_run_from_an_event(webcpp::asio::io_context* io,
                                                            std::size_t* result,
                                                            guard_type* guard) {
    *result = io->run();  // lint-run: the call the run family refuses
    boost::asio::post(*io, [guard] { guard->reset(); });
}

namespace {

using namespace std::chrono_literals;

#ifdef BOOST_DISABLE_ASSERTS
constexpr std::size_t asserted = 0;
#else
constexpr std::size_t asserted = 1;
#endif

/** A value no member of the run family returns here, so that one never called is seen. */
constexpr std::size_t untouched = 99;

// A bounded nested call: admitted, it runs the other handler and returns 1 at its deadline (a
// nested run would wait for good, the running handler counting as outstanding work).
void a_nested_run_is_refused() {
    webcpp::asio::io_context io;
    std::size_t nested = untouched;
    bool after = false;
    boost::asio::post(io, [&] {
        nested = io.run_for(100ms);  // lint-run: the nested call refused
    });
    boost::asio::post(io, [&after] { after = true; });
    const std::size_t before = seen.failed;
    BOOST_TEST_EQ(io.run(), 2U);  // lint-run: the test's event loop
    BOOST_TEST_EQ(nested, 0U);
    BOOST_TEST(after);
    BOOST_TEST_EQ(seen.failed - before, asserted);
}

// Every member but run, nested one after the other in a single run, each bounded: a refusal
// leaves the run around it flagged, so that the next nested call is refused too, and the last
// handler runs in the outer run.
void every_nested_member_is_refused_in_turn() {
    webcpp::asio::io_context io;
    constexpr std::size_t members = 5;
    std::array<std::size_t, members> results{};
    results.fill(untouched);
    bool last = false;
    const auto deadline = std::chrono::steady_clock::now() + 100ms;
    boost::asio::post(io, [&] {
        results[0] = io.run_one();  // lint-run: the nested call refused
    });
    boost::asio::post(io, [&] {
        results[1] = io.run_for(100ms);  // lint-run: the nested call refused
    });
    boost::asio::post(io, [&] { results[2] = io.run_until(deadline); });
    boost::asio::post(io, [&] { results[3] = io.run_one_for(100ms); });
    boost::asio::post(io, [&] { results[4] = io.run_one_until(deadline); });
    boost::asio::post(io, [&last] { last = true; });
    const std::size_t before = seen.failed;
    BOOST_TEST_EQ(io.run(), members + 1);  // lint-run: the test's event loop
    for (const std::size_t result : results) {
        BOOST_TEST_EQ(result, 0U);
    }
    BOOST_TEST(last);
    BOOST_TEST_EQ(seen.failed - before, members * asserted);
}

// The guard keeps run waiting until the event's own post resets it: nothing races the event.
void a_call_from_an_event_while_run_waits_is_refused() {
    webcpp::asio::io_context io;
    std::size_t from_event = untouched;
    guard_type guard(io.get_executor());
    EM_ASM(
        { setTimeout(function() { _test_run_from_an_event($0, $1, $2); }, 0); }, &io, &from_event,
        &guard);
    const std::size_t before = seen.failed;
    BOOST_TEST_EQ(io.run(), 1U);  // lint-run: the test's event loop
    BOOST_TEST_EQ(from_event, 0U);
    BOOST_TEST_EQ(seen.failed - before, asserted);
}

/** A registration whose delivery calls run, as an allocator inside an event could. */
class runs_inside final : public webcpp::asio::browser::registration {
public:
    explicit runs_inside(webcpp::asio::io_context& io) : io_(io) {}

    bool deliver(const emscripten::val& /*event*/) override {
        result = io_.run();  // lint-run: the in-delivery call refused
        return true;
    }

    void shutdown() noexcept override {}

    std::size_t result = untouched;

private:
    webcpp::asio::io_context& io_;
};

// No run is active: the bridge's delivery alone refuses the call. A handler is ready, so that an
// admitted run would run it and return 1, and the refused call leaves it for the next run.
void a_call_while_the_bridge_delivers_is_refused() {
    webcpp::asio::io_context io;
    runs_inside r(io);
    auto& bridge = boost::asio::use_service<webcpp::asio::browser::bridge>(io);
    bool ran = false;
    boost::asio::post(io, [&ran] { ran = true; });
    const std::size_t before = seen.failed;
    bridge.function(bridge.add(r))(0);
    BOOST_TEST_EQ(r.result, 0U);
    BOOST_TEST(!ran);
    BOOST_TEST(!io.stopped());
    BOOST_TEST_EQ(seen.failed - before, asserted);
    BOOST_TEST_EQ(io.run(), 1U);  // lint-run: the test's event loop
    BOOST_TEST(ran);
}

#ifndef WEBCPP_ASIO_NO_EXCEPTIONS
// The exception leaves run, and the next run is no nested call.
void an_exception_out_of_run_clears_the_flag() {
    webcpp::asio::io_context io;
    webcpp::asio::steady_timer later(io, 20ms);
    later.async_wait([](boost::system::error_code) { throw std::runtime_error("from a handler"); });
    bool caught = false;
    try {
        io.run();  // lint-run: the test's event loop
    } catch (const std::runtime_error&) {
        caught = true;
    }
    BOOST_TEST(caught);
    std::size_t ran = 0;
    boost::asio::post(io, [&ran] { ++ran; });
    io.restart();
    const std::size_t before = seen.failed;
    BOOST_TEST_EQ(io.run(), 1U);  // lint-run: the test's event loop
    BOOST_TEST_EQ(ran, 1U);
    BOOST_TEST_EQ(seen.failed, before);
}
#endif

// Each public member admits once: run_for spans several turns (100 ready handlers, a limit of
// 10) and then waits on a timer alone; run_until, run_one_for and run_one_until each run one. No
// assertion is counted, and every handler runs.
void the_timed_forms_are_not_refused() {
    webcpp::asio::io_context io(webcpp::asio::io_context_options{.max_handlers_per_turn = 10});
    const std::size_t before = seen.failed;
    std::size_t ran = 0;
    for (std::size_t i = 0; i < 100; ++i) {
        boost::asio::post(io, [&ran] { ++ran; });
    }
    BOOST_TEST_EQ(io.run_for(5s), 100U);  // lint-run: the test's event loop
    io.restart();
    webcpp::asio::steady_timer timer(io, 20ms);
    timer.async_wait([&ran](boost::system::error_code) { ++ran; });
    BOOST_TEST_EQ(io.run_for(5s), 1U);  // lint-run: the test's event loop
    io.restart();
    boost::asio::post(io, [&ran] { ++ran; });
    BOOST_TEST_EQ(io.run_until(std::chrono::steady_clock::now() + 5s), 1U);
    io.restart();
    boost::asio::post(io, [&ran] { ++ran; });
    BOOST_TEST_EQ(io.run_one_for(5s), 1U);
    io.restart();
    boost::asio::post(io, [&ran] { ++ran; });
    BOOST_TEST_EQ(io.run_one_until(std::chrono::steady_clock::now() + 5s), 1U);
    BOOST_TEST_EQ(ran, 104U);
    BOOST_TEST_EQ(seen.failed, before);
}

// xactor's driver drains a Boost io_context{1} of its own with poll: with no timer and no
// socket it has no reactor, so its poll never suspends, and polling it from a handler of the
// program's context is no call of this run family.
void a_reactor_less_context_polled_from_a_handler() {
    webcpp::asio::io_context io;
    boost::asio::io_context driver{1};
    std::size_t driven = 0;
    boost::asio::post(driver, [&driven] { ++driven; });
    std::size_t polled = untouched;
    boost::asio::post(io, [&] { polled = driver.poll(); });
    const std::size_t before = seen.failed;
    BOOST_TEST_EQ(io.run(), 1U);  // lint-run: the test's event loop
    BOOST_TEST_EQ(polled, 1U);
    BOOST_TEST_EQ(driven, 1U);
    BOOST_TEST_EQ(seen.failed, before);
}

}  // namespace

int main() {
    a_nested_run_is_refused();
    every_nested_member_is_refused_in_turn();
    a_call_from_an_event_while_run_waits_is_refused();
    a_call_while_the_bridge_delivers_is_refused();
    the_timed_forms_are_not_refused();
    a_reactor_less_context_polled_from_a_handler();
#ifndef WEBCPP_ASIO_NO_EXCEPTIONS
    an_exception_out_of_run_clears_the_flag();
#endif
    return webcpp::test::exit_status(boost::report_errors());
}
