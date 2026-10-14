// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// Through the bridge, a program can neither deliver an event outside the count of deliveries in
// progress that its JavaScript functions keep, nor set what delivering() reads (doc:
// #asio-invariant-4): deliver is private, and delivering() takes no value. As it is, this file
// compiles, the control; with WEBCPP_TEST_ASIO_DELIVER or WEBCPP_TEST_ASIO_SET_DELIVERING
// defined, it must not.

#include <webcpp/asio/browser/bridge.hpp>
#include <webcpp/asio/io_context.hpp>

#include <emscripten/val.h>
#include <boost/asio/execution_context.hpp>

namespace {

[[maybe_unused]] bool reach(webcpp::asio::io_context& io) {
    // Not const: a refusal must be refused for its access, never for a const bridge.
    // NOLINTNEXTLINE(misc-const-correctness)
    auto& bridge = boost::asio::use_service<webcpp::asio::browser::bridge>(io);
#ifdef WEBCPP_TEST_ASIO_DELIVER
    bridge.deliver(1, emscripten::val::undefined());
#elif defined(WEBCPP_TEST_ASIO_SET_DELIVERING)
    bridge.delivering(false);
#endif
    return bridge.delivering();
}

}  // namespace
