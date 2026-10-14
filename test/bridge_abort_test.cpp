// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// After the runtime aborted, a trap such as Asyncify's stack overflow or an abort() of its own,
// the bridge's JavaScript function enters WebAssembly no more (doc: #asio-building-asyncify-stack):
// an event called from JavaScript after the abort reaches no registration, as Emscripten's own
// callbacks do nothing once the runtime aborted.

#include <webcpp/asio/browser/bridge.hpp>
#include <webcpp/asio/io_context.hpp>

#include <webcpp/test/exit_status.hpp>

#include <emscripten/em_js.h>
#include <emscripten/val.h>
#include <boost/core/lightweight_test.hpp>

#include <cstddef>
#include <cstdint>

// Aborts the runtime, as a trap does, catches what abort() throws, and calls callback from
// JavaScript with 1; then puts the runtime back, so that the test can report what it saw.
// clang-format off
EM_JS(void, test_call_after_abort, (emscripten::EM_VAL callback), {
    const call = Emval.toValue(callback);
    try {
        abort('the test aborts the runtime');
    } catch (error) {
        if (!(error instanceof WebAssembly.RuntimeError)) {
            throw error;
        }
    }
    try {
        call(1);
    } finally {
        ABORT = false;
    }
});
// clang-format on

namespace {

/** A registration that counts the events delivered to it. */
class counter final : public webcpp::asio::browser::registration {
public:
    bool deliver(const emscripten::val& /*event*/) override {
        ++delivered;
        return false;
    }

    void shutdown() noexcept override {}

    std::size_t delivered = 0;
};

void an_event_after_an_abort_enters_nothing() {
    webcpp::asio::io_context io;
    counter listener;
    auto& bridge = boost::asio::use_service<webcpp::asio::browser::bridge>(io);
    const std::uint64_t key = bridge.add(listener);
    const emscripten::val deliver = bridge.function(key);
    deliver(0);  // the control: before the abort, an event is delivered
    BOOST_TEST_EQ(listener.delivered, 1U);
    test_call_after_abort(deliver.as_handle());
    BOOST_TEST_EQ(listener.delivered, 1U);
    bridge.remove(key);
}

}  // namespace

int main() {
    an_event_after_an_abort_enters_nothing();
    return webcpp::test::exit_status(boost::report_errors());
}
