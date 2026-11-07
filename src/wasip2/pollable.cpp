// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <wasi/wasip2.h>
#include <cerrno>
#include <cstddef>
#include <new>
#include <string_view>
#include <webcpp/asio/detail/wasi_pollable_descriptor.hpp>

// These private declarations are pinned to the wasi-libc shipped in SDK 34.0.
// Keep the bridge in one translation unit; no private ABI reaches a user header.
extern "C" {
// The pinned C headers require this exact macro name.
// NOLINTNEXTLINE(readability-identifier-naming)
#define hidden __attribute__((visibility("hidden")))
#include "wasi-libc/wasi/descriptor_table.h"
#undef hidden
}

#if !defined(__wasip2__) || __wasi_sdk_major__ != 34
#error "The WASIp2 pollable bridge requires the pinned wasi-sdk 34.0 ABI"
#endif
static_assert(std::string_view(__wasi_sdk_version__) == "34.0",
              "Revalidate the pollable bridge before changing wasi-sdk");

namespace {
using storage_type = webcpp::asio::detail::wasi_pollable_descriptor;

struct descriptor_storage {
    descriptor_refcnt_t references;
    storage_type* owner;
};

static_assert(offsetof(descriptor_storage, references) == 0);
static_assert(sizeof(descriptor_storage) <= sizeof(storage_type::opaque));
static_assert(alignof(descriptor_storage) <= alignof(storage_type));

void release_storage(void* value) {
    // The Asio operation owns storage and releases it only after close.
    auto* descriptor = static_cast<descriptor_storage*>(value);
    descriptor->owner->active = false;
    descriptor->~descriptor_storage();
}

int register_poll(void* value, poll_state_t* state, short events) {
    if ((static_cast<unsigned>(events) & ~static_cast<unsigned>(POLLRDNORM)) != 0) {
        errno = EOPNOTSUPP;
        return -1;
    }
    return __wasilibc_poll_add(
        state, POLLRDNORM,
        poll_borrow_pollable_t{static_cast<descriptor_storage*>(value)->owner->handle});
}

constexpr descriptor_vtable_t operations = []() constexpr noexcept {
    descriptor_vtable_t value{};
    value.free = release_storage;
    value.poll_register = register_poll;
    return value;
}();
}  // namespace

extern "C" int webcpp_asio_wasi_pollable_descriptor(storage_type* storage) {
    if (storage == nullptr || storage->active) {
        errno = EINVAL;
        return -1;
    }
    auto* descriptor =
        ::new (storage->opaque.data()) descriptor_storage{.references = {0}, .owner = storage};
    storage->active = true;
    return descriptor_table_insert(
        {.data = &descriptor->references, .vtable = const_cast<descriptor_vtable_t*>(&operations)});
}
