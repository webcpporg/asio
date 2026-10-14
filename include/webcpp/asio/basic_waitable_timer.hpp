// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#ifndef WEBCPP_ASIO_BASIC_WAITABLE_TIMER_HPP
#define WEBCPP_ASIO_BASIC_WAITABLE_TIMER_HPP

#include <webcpp/asio/config.hpp>

#include <boost/asio/any_io_executor.hpp>       // lint-world: Boost's timers are asio's
#include <boost/asio/basic_waitable_timer.hpp>  // lint-world: Boost's timers are asio's
#include <boost/asio/wait_traits.hpp>           // lint-world: Boost's timers are asio's

namespace webcpp::asio {

/**
 Boost.Asio's timer on Clock, Boost's own: on emscripten it waits in the reactor's timer queue,
 which the run family waits on.

 @tparam Clock A clock of std::chrono's, or one that meets the same requirements.
 @tparam WaitTraits How a time on Clock becomes the duration the timer waits for.
 @tparam Executor The executor the timer's operations run on.
*/
template <class Clock, class WaitTraits = boost::asio::wait_traits<Clock>,
          class Executor = boost::asio::any_io_executor>
using basic_waitable_timer = boost::asio::basic_waitable_timer<Clock, WaitTraits, Executor>;

}  // namespace webcpp::asio

#endif  // WEBCPP_ASIO_BASIC_WAITABLE_TIMER_HPP
