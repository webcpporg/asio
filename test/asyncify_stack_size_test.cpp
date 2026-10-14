// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// The stack Asyncify saves a suspension in, as the program was linked with it (doc:
// #asio-building-asyncify-stack): asio's measured size by default, and the size a program asks
// for with the feature asyncify-stack-size when it asks for more. WEBCPP_TEST_ASIO_STACK_SIZE is
// the size the test's Jamfile expects; a size below asio's is refused before this runs.

#include <webcpp/asio/io_context.hpp>

#include <webcpp/test/exit_status.hpp>

#include <emscripten/em_asm.h>
#include <boost/core/lightweight_test.hpp>

#include <cstddef>

int main() {
    const auto linked = static_cast<std::size_t>(EM_ASM_INT({ return Asyncify.StackSize; }));
    BOOST_TEST_EQ(linked, std::size_t{WEBCPP_TEST_ASIO_STACK_SIZE});
    return webcpp::test::exit_status(boost::report_errors());
}
