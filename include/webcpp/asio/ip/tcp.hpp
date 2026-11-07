// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#ifndef WEBCPP_ASIO_IP_TCP_HPP
#define WEBCPP_ASIO_IP_TCP_HPP
#include <webcpp/asio/config.hpp>

#include <webcpp/asio/ip/basic_resolver.hpp>
#ifndef WEBCPP_TARGET_EMSCRIPTEN
#include WEBCPP_ASIO_BACKEND_HEADER(ip/tcp.hpp)  // lint-world: native protocol alias

namespace webcpp::asio::ip {
using namespace ::webcpp::asio_backend::ip;
/** Boost's native TCP protocol. */
using ::webcpp::asio_backend::ip::tcp;
}  // namespace webcpp::asio::ip
#else
#include WEBCPP_ASIO_BACKEND_HEADER(detail/socket_types.hpp)  // lint-world: portable protocol constants
#include WEBCPP_ASIO_BACKEND_HEADER(ip/basic_endpoint.hpp)  // lint-world: Boost endpoint layout for the browser protocol
#include <webcpp/asio/basic_socket_acceptor.hpp>
#include <webcpp/asio/basic_stream_socket.hpp>

namespace webcpp::asio::ip {
using namespace ::webcpp::asio_backend::ip;

/** The browser TCP protocol, with operations supplied by a transport. */
class tcp {
public:
    /** The endpoint type. */
    using endpoint = ::webcpp::asio_backend::ip::basic_endpoint<tcp>;
    /** The resolver type. */
    using resolver = basic_resolver<tcp>;
    /** The socket type. */
    using socket = basic_stream_socket<tcp>;
    /** The acceptor type. */
    using acceptor = basic_socket_acceptor<tcp>;

    /** Returns the IPv4 protocol.
     @return The IPv4 protocol. */
    static tcp v4() noexcept { return tcp(WEBCPP_ASIO_BACKEND_OS_DEF(AF_INET)); }

    /** Returns the IPv6 protocol.
     @return The IPv6 protocol. */
    static tcp v6() noexcept { return tcp(WEBCPP_ASIO_BACKEND_OS_DEF(AF_INET6)); }

    /** Returns the socket type constant.
     @return The socket type. */
    // Match the InternetProtocol member signature used by Boost.
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    [[nodiscard]] int type() const noexcept { return WEBCPP_ASIO_BACKEND_OS_DEF(SOCK_STREAM); }

    /** Returns the protocol number.
     @return The protocol number. */
    // Match the InternetProtocol member signature used by Boost.
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    [[nodiscard]] int protocol() const noexcept { return WEBCPP_ASIO_BACKEND_OS_DEF(IPPROTO_TCP); }

    /** Returns the address family.
     @return The address family. */
    [[nodiscard]] int family() const noexcept { return family_; }

    /** Compares address families.
     @param a The first protocol.
     @param b The second protocol.
     *
     @return Whether they match. */
    friend bool operator==(const tcp& a, const tcp& b) noexcept = default;

private:
    /** Constructs a protocol for an address family. */
    explicit tcp(int family) : family_(family) {}

    int family_;
};
}  // namespace webcpp::asio::ip
#endif
#endif  // WEBCPP_ASIO_IP_TCP_HPP
