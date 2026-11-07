// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include "customizations.hpp"
#include <chrono>
#include <cstdio>
#include <type_traits>

static_assert(
    std::is_same_v<WEBCPP_ASIO_HANDLER_TYPE(cancellation_token, void(boost::system::error_code)),
                   cancellation_handler>);

int main() {
    webcpp::asio::io_context context;
    webcpp::asio::steady_timer timer(context, std::chrono::hours(1));
    int completions = 0;
    bool cancelled = false;
    timer.async_wait(cancellation_token{context.get_executor(), &completions, &cancelled});
    cancel_from_peer(context, timer, completions);
    context.run();  // lint-run: the consumer owns this context and drives both translation units
    if (!cancelled || completions != 2) {
        return 1;
    }
    std::puts("distribution public token/trait specializations and multiple TUs: passed");
}
