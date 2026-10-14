// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// The bridge (doc: #asio-invariant-4 and #asio-invariant-5): an event posts and wakes a
// suspended run; events delivered before run wait for it; a listener with nothing pending lets
// run return; an event of a removed key, and one of a destroyed io_context, find nothing, and
// the context's destruction shuts its registrations down; a C++ exception, and a JavaScript
// error, escaping a delivery are each reported, with the stack pointer where it was and the
// event's handle freed; the bridge says it delivers exactly while a delivery is in progress, an
// outer one around a nested one included; and a trap, 'unwind' and an ExitStatus are left to
// Emscripten. Every case runs in each variant and twin: with WebAssembly exceptions, where a
// JavaScript error runs the destructors of the frames it leaves, and in the three others, where
// it skips them.

#include <webcpp/asio/browser/bridge.hpp>
#include <webcpp/asio/io_context.hpp>

#include "browser_values.hpp"

#include <webcpp/test/exit_status.hpp>

#include <emscripten/stack.h>
#include <emscripten/val.h>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/post.hpp>
#include <boost/core/lightweight_test.hpp>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>

namespace {

using guard_type = boost::asio::executor_work_guard<webcpp::asio::io_context::executor_type>;

webcpp::asio::browser::bridge& bridge_of(webcpp::asio::io_context& io) {
    return boost::asio::use_service<webcpp::asio::browser::bridge>(io);
}

/** A registration that posts a count of the events it receives, until it is done. */
class counter final : public webcpp::asio::browser::registration {
public:
    counter(webcpp::asio::io_context& io, int once_after) : io_(io), once_after_(once_after) {}

    bool deliver(const emscripten::val& event) override {
        delivering = bridge_of(io_).delivering();
        const int value = event.as<int>();
        boost::asio::post(io_, [this, value] {
            ++received;
            last = value;
            if (release != nullptr) {
                release->reset();
            }
        });
        return once_after_ != 0 && ++delivered_ == once_after_;
    }

    void shutdown() noexcept override { shut_down = true; }

    int received = 0;
    int last = 0;
    bool delivering = false;  // what the bridge said during the last delivery
    bool shut_down = false;
    guard_type* release = nullptr;  // reset by the handler an event posts

private:
    webcpp::asio::io_context& io_;
    int once_after_;
    int delivered_ = 0;
};

// The guard keeps run waiting until the event's own handler resets it: nothing races the event.
void an_event_posts_and_wakes_run() {
    webcpp::asio::io_context io;
    counter listener(io, 1);
    auto& bridge = bridge_of(io);
    const std::uint64_t key = bridge.add(listener);
    BOOST_TEST(key >= 1);
    auto guard = boost::asio::make_work_guard(io);
    listener.release = &guard;
    test_call_after(bridge.function(key).as_handle(), 20, 7);
    BOOST_TEST_EQ(io.run(), 1U);  // lint-run: the test's event loop
    BOOST_TEST_EQ(listener.received, 1);
    BOOST_TEST_EQ(listener.last, 7);
    BOOST_TEST(listener.delivering);
    BOOST_TEST(!bridge.delivering());
}

void events_before_run_wait_for_it() {
    // tag::bridge[]
    webcpp::asio::io_context io;
    counter listener(io, 0);  // a registration, which posts each event it receives
    auto& bridge = boost::asio::use_service<webcpp::asio::browser::bridge>(io);
    const std::uint64_t key = bridge.add(listener);
    const emscripten::val deliver = bridge.function(key);  // for JavaScript to call
    deliver(1);
    deliver(2);
    BOOST_TEST_EQ(listener.received, 0);  // posted, and waiting for run
    BOOST_TEST_EQ(io.run(), 2U);          // lint-run: the test's event loop
    BOOST_TEST_EQ(listener.received, 2);
    bridge.remove(key);
    // end::bridge[]
}

// A listener is no outstanding work: run returns with one registered and nothing pending.
void run_returns_with_a_listener_and_nothing_pending() {
    webcpp::asio::io_context io;
    counter listener(io, 0);
    auto& bridge = bridge_of(io);
    const std::uint64_t key = bridge.add(listener);
    BOOST_TEST_EQ(io.run(), 0U);  // lint-run: the test's event loop
    bridge.function(key)(3);
    io.restart();
    BOOST_TEST_EQ(io.run(), 1U);  // lint-run: the test's event loop
    BOOST_TEST_EQ(listener.last, 3);
    bridge.remove(key);
}

// A registration that is done is removed: its next event finds nothing.
void a_done_registration_is_removed() {
    webcpp::asio::io_context io;
    counter listener(io, 2);
    auto& bridge = bridge_of(io);
    const emscripten::val deliver = bridge.function(bridge.add(listener));
    deliver(1);
    deliver(2);
    deliver(3);
    BOOST_TEST_EQ(io.run(), 2U);  // lint-run: the test's event loop
    BOOST_TEST_EQ(listener.received, 2);
    BOOST_TEST_EQ(listener.last, 2);
}

// An event of a removed key, and one of a destroyed context, find nothing; destroying the
// context shuts its registrations down. The registrations outlive the context, as their owners
// must.
void late_events_find_nothing() {
    auto io = std::make_unique<webcpp::asio::io_context>();
    counter removed(*io, 0);
    counter kept(*io, 0);
    auto& bridge = bridge_of(*io);
    const std::uint64_t removed_key = bridge.add(removed);
    const emscripten::val stale = bridge.function(removed_key);
    bridge.remove(removed_key);
    stale(1);
    BOOST_TEST_EQ(io->run(), 0U);  // lint-run: the test's event loop
    BOOST_TEST_EQ(removed.received, 0);
    const std::uint64_t kept_key = bridge.add(kept);
    BOOST_TEST(kept_key > removed_key);
    const emscripten::val orphan = bridge.function(kept_key);
    io.reset();
    BOOST_TEST(kept.shut_down);
    BOOST_TEST(!removed.shut_down);
    orphan(2);  // its bridge is gone: the function does nothing
    stale(3);
    BOOST_TEST_EQ(kept.received, 0);
    webcpp::asio::io_context other;
    BOOST_TEST_EQ(other.run(), 0U);  // lint-run: the test's event loop
}

#ifndef WEBCPP_ASIO_NO_EXCEPTIONS
/** A registration whose delivery throws, as an allocator that fails would. */
class thrower final : public webcpp::asio::browser::registration {
public:
    bool deliver(const emscripten::val& /*event*/) override {
        throw std::runtime_error("delivery failed");
    }

    void shutdown() noexcept override {}
};

// 3000 escapes in a row, called from JavaScript: each is reported, WebAssembly's stack pointer
// is where it was, and each event's handle is freed.
void an_escaping_exception_is_reported_and_the_stack_restored() {
    test_count_reports();
    webcpp::asio::io_context io;
    thrower failing;
    auto& bridge = bridge_of(io);
    const std::uint64_t key = bridge.add(failing);
    const emscripten::val deliver = bridge.function(key);
    const int handles = test_emval_handles();
    const auto before = emscripten_stack_get_current();
    test_call_times(deliver.as_handle(), 3000);
    BOOST_TEST_EQ(emscripten_stack_get_current(), before);
    BOOST_TEST_EQ(test_emval_handles(), handles);
    BOOST_TEST_EQ(test_reported(), 3000);
    BOOST_TEST(!bridge.delivering());
    bridge.remove(key);
}
#endif

/** A registration whose delivery calls a JavaScript function that throws. */
class javascript_thrower final : public webcpp::asio::browser::registration {
public:
    javascript_thrower() = default;

    explicit javascript_thrower(emscripten::EM_VAL throwing)
        : throwing_(emscripten::val::take_ownership(throwing)) {}

    bool deliver(const emscripten::val& /*event*/) override {
        throwing_();
        return false;
    }

    void shutdown() noexcept override {}

private:
    emscripten::val throwing_ = emscripten::val::take_ownership(test_throwing_function());
};

// The JavaScript boundary of the bridge's function: a JavaScript error leaves WebAssembly
// through it, past the export's catch (...), in every build (measured with Emscripten 6.0.11),
// so that the boundary's catch, its stackRestore, its reportError, the count of deliveries it
// puts back and the handle it frees are what this case proves. 3000 in a row, called from
// JavaScript.
void a_javascript_error_is_reported_and_the_stack_restored() {
    test_count_reports();
    webcpp::asio::io_context io;
    javascript_thrower failing;
    auto& bridge = bridge_of(io);
    const std::uint64_t key = bridge.add(failing);
    const emscripten::val deliver = bridge.function(key);
    const int handles = test_emval_handles();
    const auto before = emscripten_stack_get_current();
    test_call_times(deliver.as_handle(), 3000);
    BOOST_TEST_EQ(emscripten_stack_get_current(), before);
    BOOST_TEST_EQ(test_emval_handles(), handles);
    BOOST_TEST_EQ(test_reported(), 3000);
    BOOST_TEST(!bridge.delivering());
    bridge.remove(key);
}

/** A registration whose delivery dispatches an event, at once, to another function. */
class dispatcher final : public webcpp::asio::browser::registration {
public:
    dispatcher(webcpp::asio::browser::bridge& bridge, emscripten::val inner)
        : bridge_(bridge), inner_(std::move(inner)) {}

    bool deliver(const emscripten::val& event) override {
        inner_(event);
        still_delivering = bridge_.delivering();
        return false;
    }

    void shutdown() noexcept override {}

    bool still_delivering = false;  // whether the bridge still delivered after the inner returned

private:
    webcpp::asio::browser::bridge& bridge_;
    emscripten::val inner_;
};

// A delivery that dispatches an event whose delivery fails with a JavaScript error: the inner
// boundary reports it and leaves the outer delivery delivering, and nothing delivers once the
// outer one has.
void a_nested_error_leaves_the_outer_delivery_delivering() {
    test_count_reports();
    webcpp::asio::io_context io;
    javascript_thrower failing;
    auto& bridge = bridge_of(io);
    const std::uint64_t failing_key = bridge.add(failing);
    dispatcher outer(bridge, bridge.function(failing_key));
    const std::uint64_t outer_key = bridge.add(outer);
    bridge.function(outer_key)(1);
    BOOST_TEST(outer.still_delivering);
    BOOST_TEST(!bridge.delivering());
    BOOST_TEST_EQ(test_reported(), 1);
    bridge.remove(outer_key);
    bridge.remove(failing_key);
}

// A delivery that dispatches another, which succeeds: the outer one still delivers after the
// inner one returns, and nothing delivers once the outer one has.
void a_nested_delivery_leaves_the_outer_one_delivering() {
    webcpp::asio::io_context io;
    counter inner(io, 0);
    auto& bridge = bridge_of(io);
    const std::uint64_t inner_key = bridge.add(inner);
    dispatcher outer(bridge, bridge.function(inner_key));
    const std::uint64_t outer_key = bridge.add(outer);
    bridge.function(outer_key)(5);
    BOOST_TEST(inner.delivering);
    BOOST_TEST(outer.still_delivering);
    BOOST_TEST(!bridge.delivering());
    BOOST_TEST_EQ(io.run(), 1U);  // lint-run: the test's event loop
    BOOST_TEST_EQ(inner.last, 5);
    bridge.remove(outer_key);
    bridge.remove(inner_key);
}

// What the boundary leaves to Emscripten: a trap goes on to the caller, with the stack pointer
// put back, unreported; 'unwind' and an ExitStatus end the delivery unreported. Each leaves the
// bridge no longer delivering, and frees the event's handle.
void a_trap_escapes_and_the_runtime_s_own_pass_unreported() {
    test_count_reports();
    webcpp::asio::io_context io;
    javascript_thrower trap(test_throwing_runtime(1));
    javascript_thrower unwind(test_throwing_runtime(2));
    javascript_thrower exit(test_throwing_runtime(3));
    auto& bridge = bridge_of(io);
    const std::uint64_t trap_key = bridge.add(trap);
    const std::uint64_t unwind_key = bridge.add(unwind);
    const std::uint64_t exit_key = bridge.add(exit);
    const emscripten::val trap_function = bridge.function(trap_key);
    const emscripten::val unwind_function = bridge.function(unwind_key);
    const emscripten::val exit_function = bridge.function(exit_key);
    const int handles = test_emval_handles();
    const auto before = emscripten_stack_get_current();
    BOOST_TEST_EQ(test_call_caught(trap_function.as_handle()), 1);
    BOOST_TEST_EQ(test_call_caught(unwind_function.as_handle()), 0);
    BOOST_TEST_EQ(test_call_caught(exit_function.as_handle()), 0);
    BOOST_TEST_EQ(emscripten_stack_get_current(), before);
    BOOST_TEST_EQ(test_emval_handles(), handles);
    BOOST_TEST_EQ(test_reported(), 0);
    BOOST_TEST(!bridge.delivering());
    bridge.remove(trap_key);
    bridge.remove(unwind_key);
    bridge.remove(exit_key);
}

}  // namespace

int main() {
    an_event_posts_and_wakes_run();
    events_before_run_wait_for_it();
    run_returns_with_a_listener_and_nothing_pending();
    a_done_registration_is_removed();
    late_events_find_nothing();
#ifndef WEBCPP_ASIO_NO_EXCEPTIONS
    an_escaping_exception_is_reported_and_the_stack_restored();
#endif
    a_javascript_error_is_reported_and_the_stack_restored();
    a_nested_error_leaves_the_outer_delivery_delivering();
    a_nested_delivery_leaves_the_outer_one_delivering();
    a_trap_escapes_and_the_runtime_s_own_pass_unreported();
    return webcpp::test::exit_status(boost::report_errors());
}
