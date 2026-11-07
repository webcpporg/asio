// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string_view>
#include <webcpp/asio/detail/backend/detail/wasi_waitable.hpp>
extern "C" {
// The pinned libc declarations require this exact macro spelling.
// NOLINTNEXTLINE(readability-identifier-naming)
#define hidden __attribute__((visibility("hidden")))
#include "wasi-libc/wasi/descriptor_table.h"
#undef hidden
// Native asynchronous lowering of the SDK's existing WIT lookup interface.
__attribute__((__import_module__("wasi:sockets/ip-name-lookup@0.3.0"),
               __import_name__("[async-lower]resolve-addresses"))) std::uint32_t
webcpp_asio_wasip3_lookup(const char*, std::size_t, void*);
}
#if !defined(__wasip3__) || __wasi_sdk_major__ != 34
#error "The WASIp3 waitable bridge requires the pinned wasi-sdk 34.0 ABI"
#endif
static_assert(std::string_view(__wasi_sdk_version__) == "34.0");

namespace webcpp_asio_wasip3 {
namespace {
struct descriptor_storage {
    descriptor_refcnt_t references;
    waitable_descriptor* owner;
};

static_assert(offsetof(descriptor_storage, references) == 0);
static_assert(sizeof(void*) == 4);
static_assert(sizeof(lookup_result) == 20);
static_assert(sizeof(descriptor_storage) <= sizeof(waitable_descriptor::opaque));
static_assert(alignof(descriptor_storage) <= alignof(waitable_descriptor));

void destroy(void* value) {
    static_cast<descriptor_storage*>(value)->~descriptor_storage();
}

// poll_ready_t requires a mutable event pointer in the pinned SDK ABI.
// NOLINTNEXTLINE(misc-const-correctness)
void ready(void* value, poll_state_t* state, wasip3_event_t* event) {
    if (event == nullptr) {
        return;
    }
    auto& storage = *static_cast<waitable_descriptor*>(value);
    storage.state = static_cast<wasip3_subtask_state_t>(event->code);
    // STARTING -> STARTED is progress, not completion. The reactor will wait
    // on the same handle again and keep normal timer/socket progress.
    if (storage.state == WASIP3_SUBTASK_RETURNED ||
        storage.state == WASIP3_SUBTASK_RETURNED_CANCELLED) {
        __wasilibc_poll_ready(state, POLLRDNORM);
    }
}

int register_poll(void* value, poll_state_t* state, short events) {
    if ((static_cast<unsigned>(events) & ~static_cast<unsigned>(POLLRDNORM)) != 0) {
        errno = EOPNOTSUPP;
        return -1;
    }
    auto& storage = *static_cast<descriptor_storage*>(value)->owner;
    if (storage.state == WASIP3_SUBTASK_RETURNED ||
        storage.state == WASIP3_SUBTASK_RETURNED_CANCELLED) {
        __wasilibc_poll_ready(state, POLLRDNORM);
        return 0;
    }
    return __wasilibc_poll_add(state, storage.handle, ready, &storage);
}

constexpr descriptor_vtable_t operations = [] {
    descriptor_vtable_t value{};
    value.free = destroy;
    value.poll_register = register_poll;
    return value;
}();
}  // namespace

int open(waitable_descriptor& storage) {
    auto* descriptor =
        ::new (storage.opaque.data()) descriptor_storage{.references = {0}, .owner = &storage};
    return descriptor_table_insert(
        {.data = &descriptor->references, .vtable = const_cast<descriptor_vtable_t*>(&operations)});
}

void release(waitable_descriptor& storage) {
    if (storage.handle == 0u) {
        return;
    }
    if (storage.state != WASIP3_SUBTASK_RETURNED &&
        storage.state != WASIP3_SUBTASK_RETURNED_CANCELLED) {
        auto const status = wasip3_subtask_cancel(storage.handle);
        storage.state = WASIP3_SUBTASK_STATE(status);
    }
    wasip3_subtask_drop(storage.handle);
    storage.handle = 0;
}

wasip3_subtask_status_t start(const char* host, std::size_t size, lookup_result& result) {
    return webcpp_asio_wasip3_lookup(host, size, result.bytes.data());
}

void free_result(lookup_result& result) {
    const auto* data = result.bytes.data();
    if (data[0] == std::byte{0}) {
        ip_name_lookup_list_ip_address_t addresses;
        std::memcpy(&addresses, data + sizeof(void*), sizeof(addresses));
        ip_name_lookup_list_ip_address_free(&addresses);
    } else if (data[sizeof(void*)] == std::byte{IP_NAME_LOOKUP_ERROR_CODE_OTHER} &&
               data[2 * sizeof(void*)] != std::byte{0}) {
        wasip3_string_t message;
        std::memcpy(&message, data + (3 * sizeof(void*)), sizeof(message));
        wasip3_string_free(&message);
    }
    result.bytes.fill(std::byte{0});
}
}  // namespace webcpp_asio_wasip3
