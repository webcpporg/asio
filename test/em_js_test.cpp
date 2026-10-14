// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// Every EM_JS of asio and of its tests loads: EM_JS copies its C parameters' names into
// JavaScript, and a name JavaScript reserves links and then stops the program at load, before
// main, whether or not the function is ever called (measured with Emscripten 6.0.11). This
// program loads them all, asio's through /webcpp/asio//asio and the tests' through
// browser_values.hpp, and calls one of each to see it answer.

#include <webcpp/asio.hpp>

#include "browser_values.hpp"

#include <webcpp/test/exit_status.hpp>

#include <emscripten/val.h>
#include <boost/core/lightweight_test.hpp>

#include <string>

int main() {
    const auto text = emscripten::val::take_ownership(webcpp_asio_string("asio, more", 4));
    BOOST_TEST_EQ(text.as<std::string>(), "asio");
    test_count_reports();
    BOOST_TEST_EQ(test_reported(), 0);
    return webcpp::test::exit_status(boost::report_errors());
}
