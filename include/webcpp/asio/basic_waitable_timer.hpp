// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#ifndef WEBCPP_ASIO_BASIC_WAITABLE_TIMER_HPP
#define WEBCPP_ASIO_BASIC_WAITABLE_TIMER_HPP

#include <webcpp/asio/config.hpp>

#include WEBCPP_ASIO_BACKEND_HEADER(any_io_executor.hpp)  // lint-world: Boost's timers are asio's
#include WEBCPP_ASIO_BACKEND_HEADER(basic_waitable_timer.hpp)  // lint-world: Boost's timers are asio's
#include WEBCPP_ASIO_BACKEND_HEADER(wait_traits.hpp)  // lint-world: Boost's timers are asio's

namespace webcpp::asio {

/**
 Boost.Asio's timer on Clock, Boost's own: on emscripten it waits in the reactor's timer queue,
 which the run family waits on.

 @tparam Clock A clock of std::chrono's, or one that meets the same requirements.
 @tparam WaitTraits How a time on Clock becomes the duration the timer waits for.
 @tparam Executor The executor the timer's operations run on.
*/
template <class Clock, class WaitTraits = ::webcpp::asio_backend::wait_traits<Clock>,
          class Executor = ::webcpp::asio_backend::any_io_executor>
using basic_waitable_timer =
    ::webcpp::asio_backend::basic_waitable_timer<Clock, WaitTraits, Executor>;

}  // namespace webcpp::asio

#endif  // WEBCPP_ASIO_BASIC_WAITABLE_TIMER_HPP
