// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// The scope that flags a run family call (doc: #asio-invariant-6): it sets its flag for its
// lifetime and puts back the value the flag had, so that a scope ending inside another leaves
// the outer one's flag set, and one left by an exception puts it back too.

#include <webcpp/asio/detail/flag_scope.hpp>

#include <webcpp/test/exit_status.hpp>

#include <boost/core/lightweight_test.hpp>

#include <stdexcept>

namespace {

void a_scope_sets_its_flag_and_clears_it() {
    bool flag = false;
    {
        const webcpp::asio::detail::flag_scope scope(flag);
        BOOST_TEST(flag);
    }
    BOOST_TEST(!flag);
}

void an_inner_scope_leaves_the_outer_flag_set() {
    bool flag = false;
    {
        const webcpp::asio::detail::flag_scope outer(flag);
        {
            const webcpp::asio::detail::flag_scope inner(flag);
            BOOST_TEST(flag);
        }
        BOOST_TEST(flag);
    }
    BOOST_TEST(!flag);
}

#ifndef WEBCPP_ASIO_NO_EXCEPTIONS
void an_exception_puts_the_flag_back() {
    bool flag = false;
    bool caught = false;
    try {
        const webcpp::asio::detail::flag_scope scope(flag);
        throw std::runtime_error("inside the scope");
    } catch (const std::runtime_error&) {
        caught = true;
    }
    BOOST_TEST(caught);
    BOOST_TEST(!flag);
}
#endif

}  // namespace

int main() {
    a_scope_sets_its_flag_and_clears_it();
    an_inner_scope_leaves_the_outer_flag_set();
#ifndef WEBCPP_ASIO_NO_EXCEPTIONS
    an_exception_puts_the_flag_back();
#endif
    return webcpp::test::exit_status(boost::report_errors());
}
