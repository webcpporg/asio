// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/asio/post.hpp>
#include <webcpp/beast/http.hpp>
#include <webcpp/cobalt/task.hpp>
#include "customizations.hpp"

void cancel_from_peer(webcpp::asio::io_context& context, webcpp::asio::steady_timer& timer,
                      int& completions) {
    // Public Beast and Cobalt headers share the same selected Asio declarations.
    webcpp::beast::http::request<webcpp::beast::http::empty_body> request;
    request.method(webcpp::beast::http::verb::get);
    webcpp::asio::post(context, [&timer, &completions, method = request.method()] {
        if (method == webcpp::beast::http::verb::get && timer.cancel() == 1) {
            ++completions;
        }
    });
}
