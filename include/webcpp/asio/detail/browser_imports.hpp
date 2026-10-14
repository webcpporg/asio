// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#ifndef WEBCPP_ASIO_DETAIL_BROWSER_IMPORTS_HPP
#define WEBCPP_ASIO_DETAIL_BROWSER_IMPORTS_HPP

#include <webcpp/asio/config.hpp>

/**
 The JavaScript functions asio calls on emscripten, declared for the code that calls them.

 Tip: <webcpp/asio/browser/src.hpp> defines them, once per program.
*/
#include <emscripten/em_macros.h>
#include <emscripten/val.h>

#include <cstddef>
#include <cstdint>

// The JavaScript functions <webcpp/asio/browser/src.hpp> defines, and the export it defines, which
// every program on emscripten links once (src/browser_src.cpp).
extern "C" {
/** Suspends until a MessageChannel message: a macrotask, never a microtask. */
void webcpp_asio_yield() EM_IMPORT(__asyncjs__webcpp_asio_yield);
/** The JavaScript function that delivers an event to the bridge's registration key. */
emscripten::EM_VAL webcpp_asio_function(emscripten::EM_VAL token, void* bridge, std::uint64_t key)
    EM_IMPORT(webcpp_asio_function);
/** Settles fulfil with {ok, value} once promise settles. */
void webcpp_asio_await(emscripten::EM_VAL promise, emscripten::EM_VAL fulfil)
    EM_IMPORT(webcpp_asio_await);
/** A JavaScript string of size bytes of UTF-8 at data, which need not end with a null. */
emscripten::EM_VAL webcpp_asio_string(const char* data, std::size_t size)
    EM_IMPORT(webcpp_asio_string);
/** Reports error, a handle the caller keeps, to the page (reportError). */
void webcpp_asio_report(emscripten::EM_VAL error) EM_IMPORT(webcpp_asio_report);

/**
 Delivers event, a handle the caller keeps and frees, to the registration key of bridge.

 Tip: the export the bridge's JavaScript functions call, which <webcpp/asio/browser/src.hpp>
 defines; declared here so that the bridge can name it as the one caller of its delivery.
*/
void webcpp_asio_deliver(void* bridge, std::uint64_t key, emscripten::EM_VAL event);
}

#endif  // WEBCPP_ASIO_DETAIL_BROWSER_IMPORTS_HPP
