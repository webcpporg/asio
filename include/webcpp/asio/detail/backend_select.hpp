// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)
// See accompanying file LICENSE_1_0.txt or https://www.boost.org/LICENSE_1_0.txt.

#ifndef WEBCPP_ASIO_DETAIL_BACKEND_SELECT_HPP
#define WEBCPP_ASIO_DETAIL_BACKEND_SELECT_HPP

#include <webcpp/config.hpp>

#ifdef WEBCPP_TARGET_NATIVE
// Keep header-name tokens intact; parentheses are invalid in them.
// clang-format off
/**
 Selects an implementation header for internal use.

 @param header The path relative to the backend include root.
*/
#define WEBCPP_ASIO_BACKEND_HEADER(header) <boost/asio/header>  // NOLINT(bugprone-macro-parentheses) lint-world: native networking
// clang-format on
#include <boost/asio/detail/config.hpp>  // lint-world: native forwarding retains Boost networking

namespace boost::asio {}

namespace webcpp {
namespace asio_backend = ::boost::asio;
}
#else
// Keep header-name tokens intact; parentheses are invalid in them.
// clang-format off
/**
 Selects an implementation header for internal use.

 @param header The path relative to the backend include root.
*/
#define WEBCPP_ASIO_BACKEND_HEADER(header) <webcpp/asio/detail/backend/header>  // NOLINT(bugprone-macro-parentheses)
// clang-format on

// A Wasm build has threads only when the compiler selects the pthread ABI.
// Unlike an include-path overlay, this choice is available to every build system.
#if !defined(_REENTRANT) && !defined(__EMSCRIPTEN_PTHREADS__)
#ifndef WEBCPP_ASIO_BACKEND_DISABLE_THREADS
/** Disables internal thread support when the compiler has no pthread ABI. */
#define WEBCPP_ASIO_BACKEND_DISABLE_THREADS
#endif
#endif
#include <webcpp/asio/detail/backend/detail/config.hpp>
#endif

namespace webcpp::asio {
using namespace ::webcpp::asio_backend;
}

#endif  // WEBCPP_ASIO_DETAIL_BACKEND_SELECT_HPP
