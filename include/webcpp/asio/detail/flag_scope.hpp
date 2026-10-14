// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#ifndef WEBCPP_ASIO_DETAIL_FLAG_SCOPE_HPP
#define WEBCPP_ASIO_DETAIL_FLAG_SCOPE_HPP

#include <webcpp/asio/config.hpp>

namespace webcpp::asio::detail {

/**
 Sets a flag for its lifetime, and puts back the value the flag had however the scope ends, a C++
 exception included.

 Tip: putting back rather than clearing keeps a flag set by an outer scope set when an inner one
 ends, so that a scope made inside another can never clear what the outer one holds.
*/
class flag_scope {
public:
    /**
     Sets flag until the scope ends.

     @param flag The flag, which outlives the scope.
    */
    explicit flag_scope(bool& flag) noexcept : flag_(flag), previous_(flag) { flag_ = true; }

    /** Not copyable: a copy would put the flag back a second time. */
    flag_scope(const flag_scope&) = delete;
    /** Not copy-assignable: the scope sets one flag for as long as it lives. */
    flag_scope& operator=(const flag_scope&) = delete;
    /** Not movable: a moved-from scope would still put the flag back. */
    flag_scope(flag_scope&&) = delete;
    /** Not move-assignable: the scope sets one flag for as long as it lives. */
    flag_scope& operator=(flag_scope&&) = delete;

    /** Puts the flag's previous value back. */
    ~flag_scope() { flag_ = previous_; }

private:
    bool& flag_;
    bool previous_;
};

}  // namespace webcpp::asio::detail

#endif  // WEBCPP_ASIO_DETAIL_FLAG_SCOPE_HPP
