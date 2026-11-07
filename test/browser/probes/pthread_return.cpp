// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// A dependency-only witness: a suspended pthread entry must return its own value.
#include <emscripten.h>
#include <pthread.h>
#include <cerrno>
#include <cstdio>

// clang-format off
EM_ASYNC_JS(int, suspend_worker, (), {
    await new Promise(resolve => setTimeout(resolve, 20));
    return 42;
});

// clang-format on

extern "C" EMSCRIPTEN_KEEPALIVE int try_join_worker(pthread_t thread, void** value) {
    return pthread_tryjoin_np(thread, value);
}

// clang-format off
EM_ASYNC_JS(int, join_worker, (pthread_t thread, void** value, int busy), {
    const deadline = performance.now() + 5000;
    let status;
    while ((status = _try_join_worker(thread, value)) === busy) {
        if (performance.now() > deadline) throw new Error('worker did not exit');
        await new Promise(resolve => setTimeout(resolve, 5));
    }
    return status;
});

// clang-format on

namespace {
void* worker(void* argument) {
    auto& result = *static_cast<int*>(argument);
    result = suspend_worker();
    std::printf("worker suspended result %d\n", result);
    return argument;
}
}  // namespace

int main() {
    pthread_t thread{};
    int result = 0;
    const int created = pthread_create(&thread, nullptr, worker, &result);
    if (created != 0) {
        return created;
    }
    void* value = nullptr;
    const int joined = join_worker(thread, &value, EBUSY);
    // Preserve the raw-return diagnostic separately from explicit result storage.
    int returned = -1;
    if (value == nullptr) {
        returned = 0;
    } else if (value == &result) {
        returned = result;
    }
    std::printf("join result %d value %d\n", joined, returned);
    const int status = (joined != 0 || value != &result || result != 42) ? 1 : 0;
    EM_ASM(
        {
            if (Module['webcppTestFinished']) {
                Module['webcppTestFinished']($0);
            }
        },
        status);
    std::puts("worker runtime complete");
    return status;
}
