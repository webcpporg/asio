// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#ifdef WEBCPP_TEST_LOCAL_BASIC_ENDPOINT
// expected-error@webcpp/asio/local/basic_endpoint.hpp:* {{local sockets require native}}
#include <webcpp/asio/local/basic_endpoint.hpp>
#elif defined(WEBCPP_TEST_LOCAL_CONNECT_PAIR)
// expected-error@webcpp/asio/local/connect_pair.hpp:* {{local sockets require native}}
#include <webcpp/asio/local/connect_pair.hpp>
#elif defined(WEBCPP_TEST_LOCAL_DATAGRAM_PROTOCOL)
// expected-error@webcpp/asio/local/datagram_protocol.hpp:* {{local sockets require native}}
#include <webcpp/asio/local/datagram_protocol.hpp>
#elif defined(WEBCPP_TEST_LOCAL_SEQ_PACKET_PROTOCOL)
// expected-error@webcpp/asio/local/seq_packet_protocol.hpp:* {{local sockets require native}}
#include <webcpp/asio/local/seq_packet_protocol.hpp>
#elif defined(WEBCPP_TEST_LOCAL_STREAM_PROTOCOL)
// expected-error@webcpp/asio/local/stream_protocol.hpp:* {{local sockets require native}}
#include <webcpp/asio/local/stream_protocol.hpp>
#elif defined(WEBCPP_TEST_LOCAL_UMBRELLA)
#include <webcpp/asio.hpp>

// A portable umbrella must not expose a capability the public config disables.
using local_endpoint = webcpp::asio::local::stream_protocol::endpoint;
#else
#include <webcpp/asio/config.hpp>

#if WEBCPP_ASIO_HAS_LOCAL_SOCKETS
#include <webcpp/asio/local/basic_endpoint.hpp>
#include <webcpp/asio/local/connect_pair.hpp>
#include <webcpp/asio/local/datagram_protocol.hpp>
#include <webcpp/asio/local/seq_packet_protocol.hpp>
#include <webcpp/asio/local/stream_protocol.hpp>

#include <type_traits>

static_assert(
    std::is_same_v<webcpp::asio::local::stream_protocol, boost::asio::local::stream_protocol>);
static_assert(
    std::is_same_v<webcpp::asio::local::datagram_protocol, boost::asio::local::datagram_protocol>);
static_assert(std::is_same_v<webcpp::asio::local::seq_packet_protocol,
                             boost::asio::local::seq_packet_protocol>);
static_assert(
    std::is_same_v<webcpp::asio::local::basic_endpoint<webcpp::asio::local::stream_protocol>,
                   boost::asio::local::stream_protocol::endpoint>);
#endif
#endif
