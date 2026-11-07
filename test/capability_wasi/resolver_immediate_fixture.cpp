// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <wasi/wasip3.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>

// The linker --wrap protocol requires these exact native boundary symbols.
// NOLINTNEXTLINE(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" std::uint32_t __real_webcpp_asio_wasip3_lookup(const char*, std::size_t, void*);

// Reuse the real resolver test with an immediately completed localhost lookup.
// Keep its context, allocator, cancellation and malformed-name paths intact.
// NOLINTNEXTLINE(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" std::uint32_t __wrap_webcpp_asio_wasip3_lookup(const char* name, std::size_t size,
                                                          void* result) {
    if (size != 9 || std::memcmp(name, "localhost", 9) != 0) {
        return __real_webcpp_asio_wasip3_lookup(name, size, result);
    }
    auto* address = static_cast<ip_name_lookup_ip_address_t*>(
        std::calloc(1, sizeof(ip_name_lookup_ip_address_t)));
    address->tag = SOCKETS_IP_ADDRESS_IPV4;
    address->val.ipv4 = {.f0 = 127, .f1 = 0, .f2 = 0, .f3 = 1};
    const ip_name_lookup_list_ip_address_t addresses{.ptr = address, .len = 1};
    auto* bytes = static_cast<unsigned char*>(result);
    bytes[0] = 0;
    std::memcpy(bytes + sizeof(void*), &addresses, sizeof(addresses));
    return WASIP3_SUBTASK_RETURNED;
}
