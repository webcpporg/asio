// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)
// See accompanying file LICENSE_1_0.txt or https://www.boost.org/LICENSE_1_0.txt.
#include <emscripten/em_js.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <webcpp/asio/browser/async_await.hpp>
#include <webcpp/asio/buffer.hpp>
#include <webcpp/asio/io_context.hpp>
#include <webcpp/asio/steady_timer.hpp>
#include <webcpp/beast/http.hpp>

// clang-format off
EM_JS(emscripten::EM_VAL, remote_number, (), {
    return Emval.toHandle(new Promise(resolve => setTimeout(() => resolve(32), 5)));
});
// clang-format on

int main() {
    webcpp::asio::io_context io;
    int number = 0;
    bool elapsed = false;
    webcpp::asio::browser::async_await(
        io.get_executor(), emscripten::val::take_ownership(remote_number()),
        [&](boost::system::error_code error, const emscripten::val& value) {
            if (!error) {
                number = 10 + value.as<int>();
            }
        });
    webcpp::asio::steady_timer timer(io, std::chrono::milliseconds(2));
    timer.async_wait([&](boost::system::error_code error) { elapsed = !error; });
    io.run();  // lint-run: the consumer owns the suspension-enabled entry stack
    webcpp::beast::http::request_parser<webcpp::beast::http::string_body> parser;
    const std::string message = "GET /wasm HTTP/1.1\r\nHost: localhost\r\n\r\n";
    boost::system::error_code error;
    parser.put(webcpp::asio::buffer(message), error);
    const bool passed = number == 42 && elapsed && !error && parser.get().target() == "/wasm";
    std::puts(passed ? "distribution browser Promise/timer/HTTP: passed"
                     : "distribution browser: failed");
    std::exit(passed ? 0 : 1);
}
