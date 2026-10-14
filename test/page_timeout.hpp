// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#ifndef WEBCPP_TEST_ASIO_PAGE_TIMEOUT_HPP
#define WEBCPP_TEST_ASIO_PAGE_TIMEOUT_HPP

#ifdef __EMSCRIPTEN__
#include <emscripten/em_asm.h>
#endif

namespace webcpp::test {

#ifdef __EMSCRIPTEN__
/**
 A JavaScript setTimeout(0), armed at construction, whose running only JavaScript records, so
 that a timeout that runs after its case ended touches nothing.

 Tip: when it runs is the host's: node delays a setTimeout(0) by 1 ms, a browser clamps a nested
 one to 4 ms and throttles a background tab's to 1 s or more. A test asserts where it runs, at a
 yield, never after how many handlers.
*/
class page_timeout {
public:
    page_timeout()
        : id_(EM_ASM_INT({
              const state = {ran : false};
              globalThis.webcppTimeouts = globalThis.webcppTimeouts || [];
              const id = globalThis.webcppTimeouts.push(state) - 1;
              setTimeout(function() { state.ran = true; }, 0);
              return id;
          })) {}

    /** Whether the timeout has run. */
    [[nodiscard]] bool ran() const {
        return EM_ASM_INT({ return globalThis.webcppTimeouts[$0].ran ? 1 : 0; }, id_) != 0;
    }

private:
    int id_;
};
#endif

/**
 Counts the run family's yields from its construction: every MessagePort message the page
 posts, which only the yield posts in these tests.

 Tip: deterministic where a setTimeout(0) is not, since a handler that reads a new count runs
 after the yield's message was delivered: the yield resumes only in its onmessage. A test that
 posts MessagePort messages of its own cannot use it. Natively there is no page, and the count
 stays 0.
*/
class yield_counter {
public:
    yield_counter() : start_(posted()) {}

    /** The yields since construction. */
    [[nodiscard]] int count() const { return posted() - start_; }

private:
    // Every MessagePort message posted so far, counted from the first call.
    static int posted() {
#ifdef __EMSCRIPTEN__
        return EM_ASM_INT({
            if (!globalThis.webcppYieldsCounted) {
                globalThis.webcppYieldsCounted = true;
                globalThis.webcppYields = 0;
                const post = MessagePort.prototype.postMessage;
                MessagePort.prototype.postMessage = function() {
                    ++globalThis.webcppYields;
                    return post.apply(this, arguments);
                };
            }
            return globalThis.webcppYields;
        });
#else
        return 0;
#endif
    }

    int start_;
};

}  // namespace webcpp::test

#endif  // WEBCPP_TEST_ASIO_PAGE_TIMEOUT_HPP
