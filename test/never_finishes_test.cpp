// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// A main that never finishes fails on both variants rather than exit 0 when node finds nothing
// pending (doc: #asio-building-exit-status): here Boost's own io_context, without asio's
// reactor, held by work that nothing releases, so that run waits for good. Its status, 1 with
// the test support's line, is in its .output.

#include <webcpp/test/exit_status.hpp>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>

int main() {
    boost::asio::io_context io;
    const auto guard = boost::asio::make_work_guard(io);
    io.run();  // lint-run: the event loop that never returns, which the test pins
    return webcpp::test::exit_status(0);
}
