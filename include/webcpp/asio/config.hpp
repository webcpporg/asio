// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 The configuration of asio, which each of its headers includes first: whether
 asio is built without exceptions.

 @see "Testing", in the guide.
*/
#ifndef WEBCPP_ASIO_CONFIG_HPP
#define WEBCPP_ASIO_CONFIG_HPP

#include <boost/config.hpp>

// MrDocs parses asio as a build with exceptions does, where the macro is not
// defined: this definition, undone at once, is the one the reference lists, and
// the parse goes on with every API that throws in it.
#ifdef __MRDOCS__
/**
 Defined when asio is built without exceptions: automatically when the
 compiler has none (`BOOST_NO_EXCEPTIONS`), or by the developer to disable them
 in a build that has them.

 asio raises an exception only where Boost.Asio's own interface throws: the
 throwing overloads of its sockets, resolver and `ssl::context` (an
 `error_code&` overload stands beside each), which are absent while this is
 defined.

 @see "Testing", in the guide.
*/
#define WEBCPP_ASIO_NO_EXCEPTIONS
#undef WEBCPP_ASIO_NO_EXCEPTIONS
#endif

// asio's build defines it when a program asks for less Asyncify stack than asio's measured
// suspensions need (doc: #asio-building-asyncify-stack).
#ifdef WEBCPP_ASIO_ASYNCIFY_STACK_SIZE_REFUSED
#error \
    "asyncify-stack-size is below 8192 bytes, the least that asio's measured suspensions need, half of it left to the program: ask for 8192 or more"
#endif

#if defined(BOOST_NO_EXCEPTIONS) && !defined(WEBCPP_ASIO_NO_EXCEPTIONS)
#define WEBCPP_ASIO_NO_EXCEPTIONS
#endif

#endif  // WEBCPP_ASIO_CONFIG_HPP
