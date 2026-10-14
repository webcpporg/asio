// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#ifndef WEBCPP_ASIO_STEADY_TIMER_HPP
#define WEBCPP_ASIO_STEADY_TIMER_HPP

#include <webcpp/asio/config.hpp>

#include <boost/asio/steady_timer.hpp>  // lint-world: Boost's timers are asio's

namespace webcpp::asio {

/**
 Boost.Asio's timer on the monotonic clock, Boost's own: on emscripten it waits in the reactor's
 timer queue, which the run family waits on.
*/
using boost::asio::steady_timer;  // lint-world: Boost's timers are asio's

}  // namespace webcpp::asio

#endif  // WEBCPP_ASIO_STEADY_TIMER_HPP
