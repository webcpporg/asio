// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#ifndef WEBCPP_ASIO_DETAIL_POST_COMPLETION_HPP
#define WEBCPP_ASIO_DETAIL_POST_COMPLETION_HPP

#include <webcpp/asio/config.hpp>

#include WEBCPP_ASIO_BACKEND_HEADER(any_io_executor.hpp)  // lint-world: preserve an erased executor
#include WEBCPP_ASIO_BACKEND_HEADER(io_context.hpp)  // lint-world: recover an ordinary context executor without erasing its allocator
#include WEBCPP_ASIO_BACKEND_HEADER(post.hpp)  // lint-world: completion always enters through a post
#include <utility>

namespace webcpp::asio::detail {
/** Posts without erasing an ordinary io_context executor's allocator property. */
template <class Handler>
void post_completion(const ::webcpp::asio_backend::any_io_executor& executor, Handler&& handler) {
    using plain = ::webcpp::asio_backend::io_context::executor_type;
    using tracked = decltype(::webcpp::asio_backend::prefer(
        std::declval<plain>(), ::webcpp::asio_backend::execution::outstanding_work_t::tracked));
    if (const auto* concrete = executor.target<plain>()) {
        ::webcpp::asio_backend::post(*concrete, std::forward<Handler>(handler));
    } else if (const auto* concrete = executor.target<tracked>()) {
        ::webcpp::asio_backend::post(*concrete, std::forward<Handler>(handler));
    } else {
        // Keep custom executor and strand semantics; they own their dispatch allocation policy.
        ::webcpp::asio_backend::post(executor, std::forward<Handler>(handler));
    }
}
}  // namespace webcpp::asio::detail
#endif  // WEBCPP_ASIO_DETAIL_POST_COMPLETION_HPP
