// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <array>
#include <boost/core/lightweight_test.hpp>
#include <boost/system/system_error.hpp>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory_resource>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <webcpp/asio/deferred.hpp>
#include <webcpp/asio/io_context.hpp>
#include <webcpp/asio/ip/basic_resolver.hpp>
#include <webcpp/asio/ip/basic_resolver_entry.hpp>
#include <webcpp/asio/ip/basic_resolver_results.hpp>
#include <webcpp/asio/ip/tcp.hpp>
#include <webcpp/asio/ip/udp.hpp>
#include <webcpp/test/exit_status.hpp>

namespace {
using resolver = webcpp::asio::ip::tcp::resolver;

void check_entry(const resolver::results_type& results, std::string_view host,
                 std::string_view service, std::uint16_t port) {
    BOOST_TEST_EQ(results.size(), 1U);
    if (results.empty()) {
        return;
    }
    BOOST_TEST(results.begin()->host_name() == host);
    BOOST_TEST(results.begin()->service_name() == service);
    BOOST_TEST(results.begin()->endpoint().address().is_unspecified());
    BOOST_TEST_EQ(results.begin()->endpoint().port(), port);
}

void services_and_errors() {
    webcpp::asio::io_context io;
    resolver r(io);

    struct expected_port {
        std::string_view service;
        bool found;
        std::uint16_t port;
    };

    constexpr auto services = std::to_array<expected_port>({
        {.service = "80", .found = true, .port = 80},
        {.service = "0", .found = true, .port = 0},
        {.service = "65535", .found = true, .port = 65535},
        {.service = "65536", .found = false, .port = 0},
        {.service = "http", .found = true, .port = 80},
        {.service = "ws", .found = true, .port = 80},
        {.service = "https", .found = true, .port = 443},
        {.service = "wss", .found = true, .port = 443},
        {.service = "WS", .found = false, .port = 0},
        {.service = "80x", .found = false, .port = 0},
        {.service = "", .found = true, .port = 0},
        {.service = "-1", .found = false, .port = 0},
        {.service = "+80", .found = false, .port = 0},
        {.service = " 80", .found = false, .port = 0},
        {.service = "99999999999999999999999", .found = false, .port = 0},
    });
    boost::system::error_code ec;
    for (const auto& entry : services) {
        const auto results = r.resolve("example.com", entry.service, ec);
        if (entry.found) {
            BOOST_TEST(!ec);
            check_entry(results, "example.com", entry.service, entry.port);
        } else {
            BOOST_TEST(ec == webcpp::asio::error::service_not_found);
            BOOST_TEST(results.empty());
        }
    }
    BOOST_TEST(r.resolve("h", "https", resolver::numeric_service, ec).empty());
    BOOST_TEST(ec == webcpp::asio::error::service_not_found);
    check_entry(r.resolve("h", "443", resolver::numeric_service, ec), "h", "443", 443);
    BOOST_TEST(!ec);
    const auto entry = r.resolve("example.com", "https", ec);
    webcpp::asio::ip::tcp::socket socket(io);
    BOOST_TEST(socket.connect(*entry.begin(), ec) == webcpp::asio::error::operation_not_supported);
    const webcpp::asio::ip::tcp::endpoint endpoint(webcpp::asio::ip::address_v4::loopback(), 80);
    BOOST_TEST(r.resolve(endpoint, ec).empty());
    BOOST_TEST(ec == webcpp::asio::error::operation_not_supported);
    webcpp::asio::ip::udp::resolver udp(io);
    const auto datagram = udp.resolve("example.com", "53", ec);
    BOOST_TEST(!ec);
    BOOST_TEST_EQ(datagram.begin()->endpoint().port(), 53);
#ifndef WEBCPP_ASIO_NO_EXCEPTIONS
    bool caught = false;
    try {
        (void)r.resolve("h", "nope");
    } catch (const boost::system::system_error& error) {
        caught = error.code() == webcpp::asio::error::service_not_found;
    }
    BOOST_TEST(caught);
#endif
}

void asynchronous_results() {
    webcpp::asio::io_context io;
    resolver r(io.get_executor());
    BOOST_TEST(r.get_executor() == io.get_executor());
    bool returned = false;
    unsigned calls = 0;
    auto handler = [&](boost::system::error_code ec, const resolver::results_type& results) {
        BOOST_TEST(returned);
        BOOST_TEST(!ec);
        check_entry(results, "example.com", "https", 443);
        ++calls;
    };
    r.async_resolve("example.com", "https", handler);
    std::string host = "example.com";
    std::string service = "https";
    auto deferred = r.async_resolve(host, service, webcpp::asio::deferred);
    host = "changed";
    service = "nope";
    std::move(deferred)(handler);
    r.async_resolve("h", "https", resolver::numeric_service,
                    [&](boost::system::error_code ec, const resolver::results_type& results) {
                        BOOST_TEST(returned);
                        BOOST_TEST(ec == webcpp::asio::error::service_not_found);
                        BOOST_TEST(results.empty());
                        ++calls;
                    });
    const webcpp::asio::ip::tcp::endpoint endpoint(webcpp::asio::ip::address_v4::loopback(), 80);
    auto reverse = r.async_resolve(endpoint, webcpp::asio::deferred);
    std::move(reverse)([&](boost::system::error_code ec, const resolver::results_type& results) {
        BOOST_TEST(returned);
        BOOST_TEST(ec == webcpp::asio::error::operation_not_supported);
        BOOST_TEST(results.empty());
        ++calls;
    });
    r.cancel();  // Results already computed remain queued.
    BOOST_TEST_EQ(calls, 0U);
    returned = true;
    io.run();  // lint-run: the resolver test drains posted completions
    BOOST_TEST_EQ(calls, 4U);
}

/** The page's results hold their one entry, under the names Boost gives the native ones. */
void results_hold_their_entry() {
    using results_type = resolver::results_type;
    static_assert(std::is_same_v<results_type,
                                 webcpp::asio::ip::basic_resolver_results<webcpp::asio::ip::tcp>>);
    static_assert(std::is_same_v<results_type::value_type,
                                 webcpp::asio::ip::basic_resolver_entry<webcpp::asio::ip::tcp>>);
    static_assert(std::is_same_v<webcpp::asio::ip::udp::resolver::results_type,
                                 webcpp::asio::ip::basic_resolver_results<webcpp::asio::ip::udp>>);
    static_assert(std::forward_iterator<results_type::const_iterator>);
    static_assert(std::ranges::forward_range<const results_type>);
    static_assert(std::is_nothrow_copy_constructible_v<results_type>);
    webcpp::asio::io_context io;
    resolver r(io);
    const results_type none;
    BOOST_TEST(none.empty());
    BOOST_TEST_EQ(none.size(), 0U);
    BOOST_TEST_EQ(none.max_size(), 1U);
    BOOST_TEST(none.begin() == none.end());

    // The longest name DNS has, 253 bytes and the final dot, and a service of any length.
    boost::system::error_code ec;
    const std::string longest = std::string(253, 'a') + '.';
    const std::string padded = std::string(100, '0') + "443";
    auto results = r.resolve(longest, padded, ec);
    BOOST_TEST(!ec);
    check_entry(results, longest, padded, 443);
    check_entry(r.resolve("h", "000", ec), "h", "000", 0);
    BOOST_TEST(!ec);
    std::size_t visited = 0;
    for (const auto& entry : results) {
        const webcpp::asio::ip::tcp::endpoint endpoint = entry;
        BOOST_TEST_EQ(endpoint.port(), 443);
        ++visited;
    }
    BOOST_TEST_EQ(visited, 1U);
    BOOST_TEST_EQ(std::distance(results.cbegin(), results.cend()), 1);

    // A copy is its own: equal to its source, and unchanged when the source changes.
    auto copy = results;
    BOOST_TEST(copy == results);
    results = r.resolve("other.example", "80", ec);
    BOOST_TEST(copy != results);
    check_entry(copy, longest, padded, 443);
    auto moved = std::move(copy);
    check_entry(moved, longest, padded, 443);
    moved.swap(results);
    check_entry(moved, "other.example", "80", 80);
    check_entry(results, longest, padded, 443);

    // The names are copied into the caller's allocator.
    std::array<std::byte, 1024> storage{};
    std::pmr::monotonic_buffer_resource arena(storage.data(), storage.size(),
                                              std::pmr::null_memory_resource());
    const std::pmr::polymorphic_allocator<char> allocator(&arena);
    BOOST_TEST(results.begin()->host_name(allocator) == std::string_view(longest));
    BOOST_TEST(results.begin()->service_name(allocator) == std::string_view(padded));

    // A longer host is no name DNS would find, whichever way it is resolved.
    const std::string too_long = longest + 'a';
    BOOST_TEST(r.resolve(too_long, "80", ec).empty());
    BOOST_TEST(ec == webcpp::asio::error::host_not_found);
    unsigned calls = 0;
    r.async_resolve(too_long, "80",
                    [&](boost::system::error_code posted, const results_type& found) {
                        BOOST_TEST(posted == webcpp::asio::error::host_not_found);
                        BOOST_TEST(found.empty());
                        ++calls;
                    });
    io.run();  // lint-run: the resolver test drains the posted refusal
    BOOST_TEST_EQ(calls, 1U);
#ifndef WEBCPP_ASIO_NO_EXCEPTIONS
    bool caught = false;
    try {
        (void)r.resolve(too_long, "80");
    } catch (const boost::system::system_error& error) {
        caught = error.code() == webcpp::asio::error::host_not_found;
    }
    BOOST_TEST(caught);
#endif
}
}  // namespace

int main() {
    services_and_errors();
    asynchronous_results();
    results_hold_their_entry();
    return webcpp::test::exit_status(boost::report_errors());
}
