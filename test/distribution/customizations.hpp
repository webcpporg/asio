// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#ifndef WEBCPP_TEST_DISTRIBUTION_CUSTOMIZATIONS_HPP
#define WEBCPP_TEST_DISTRIBUTION_CUSTOMIZATIONS_HPP

// Upstream and shipped completion-token declarations must coexist.
#include <boost/asio/async_result.hpp>
#include <utility>
#include <webcpp/asio/associated_executor.hpp>
#include <webcpp/asio/async_result.hpp>
#include <webcpp/asio/io_context.hpp>
#include <webcpp/asio/steady_timer.hpp>

struct cancellation_token {
    webcpp::asio::io_context::executor_type executor;
    int* completions;
    bool* cancelled;
};

struct cancellation_handler {
    cancellation_token token;

    void operator()(boost::system::error_code error) const {
        ++*token.completions;
        *token.cancelled = error == webcpp::asio::error::operation_aborted;
    }
};

// Qualified specializations at global scope name the actual selected template,
// including when the native facade imports it from Boost with a using directive.
template <class DefaultExecutor>
struct webcpp::asio::associated_executor<cancellation_handler, DefaultExecutor> {
    using type = webcpp::asio::io_context::executor_type;

    static type get(const cancellation_handler& handler,
                    const DefaultExecutor& = DefaultExecutor()) noexcept {
        return handler.token.executor;
    }
};

template <>
struct webcpp::asio::async_result<cancellation_token, void(boost::system::error_code)> {
    using completion_handler_type = cancellation_handler;
    using return_type = void;

    template <class Initiation, class RawToken, class... Args>
    static void initiate(Initiation&& initiation, RawToken&& token, Args&&... args) {
        std::forward<Initiation>(initiation)(cancellation_handler{token},
                                             std::forward<Args>(args)...);
    }
};

void cancel_from_peer(webcpp::asio::io_context& context, webcpp::asio::steady_timer& timer,
                      int& completions);

#endif
