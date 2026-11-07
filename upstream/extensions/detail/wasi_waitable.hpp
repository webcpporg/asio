// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)
#ifndef WEBCPP_ASIO_DETAIL_WASI_WAITABLE_HPP
#define WEBCPP_ASIO_DETAIL_WASI_WAITABLE_HPP
#include <wasi/wasip3.h>
#include <array>
#include <cstddef>
#include <cstdint>

namespace webcpp_asio_wasip3 {
// Stable storage is owned by the resolver operation until descriptor close.
struct waitable_descriptor {
    alignas(std::max_align_t) std::array<std::byte, 32> opaque{};
    wasip3_subtask_t handle = 0;
    wasip3_subtask_state_t state = WASIP3_SUBTASK_RETURNED;
};

// Pinned canonical result layout for result<list<ip-address>, error-code>.
struct lookup_result {
    alignas(void*) std::array<std::byte, 5 * sizeof(void*)> bytes{};
};

int open(waitable_descriptor& storage);
void release(waitable_descriptor& storage);
wasip3_subtask_status_t start(const char* host, std::size_t size, lookup_result& result);
void free_result(lookup_result& result);
}  // namespace webcpp_asio_wasip3
#endif
