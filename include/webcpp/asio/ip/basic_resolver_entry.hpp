// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#ifndef WEBCPP_ASIO_IP_BASIC_RESOLVER_ENTRY_HPP
#define WEBCPP_ASIO_IP_BASIC_RESOLVER_ENTRY_HPP
#include <webcpp/asio/config.hpp>

#ifndef WEBCPP_TARGET_EMSCRIPTEN
#include WEBCPP_ASIO_BACKEND_HEADER(ip/basic_resolver_entry.hpp)  // lint-world: native resolver entries

namespace webcpp::asio::ip {
using namespace ::webcpp::asio_backend::ip;
}  // namespace webcpp::asio::ip
#else
#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace webcpp::asio::ip {
template <class InternetProtocol>
class basic_resolver_results;

/**
 An entry of a page's resolution: an endpoint, and the host and service names it was resolved
 from, which the entry holds in place.

 A page looks up no name: its resolver keeps the host and the service for the page's transport,
 which connects to them, with an unspecified address. The entry allocates nothing, nor do the
 results that hold it: the host is at most `max_host_name_size` bytes, and the service, empty,
 one of the names the resolver knows or digits, is kept as the digits of its port and the count
 of its leading zeros, which give it back as it was.
 @tparam InternetProtocol The internet protocol.
 @see basic_resolver_results, which holds the resolution's one entry.
*/
template <class InternetProtocol>
class basic_resolver_entry {
public:
    /** The internet protocol type. */
    using protocol_type = InternetProtocol;
    /** The protocol endpoint type. */
    using endpoint_type = InternetProtocol::endpoint;

    /** The longest host an entry holds: a name of DNS's 253 bytes, and its final dot. */
    static constexpr std::size_t max_host_name_size = 254;

    /** Constructs an entry with an unspecified endpoint and empty names. */
    basic_resolver_entry() noexcept = default;

    /**
     Returns the endpoint.
     @return The endpoint, whose address is unspecified.
    */
    [[nodiscard]] endpoint_type endpoint() const noexcept { return endpoint_; }

    /**
     Converts to the endpoint.
     @return The endpoint, whose address is unspecified.
    */
    // Boost's entry converts implicitly, which the range algorithms rely on.
    // NOLINTNEXTLINE(misc-explicit-constructor)
    operator endpoint_type() const noexcept { return endpoint_; }

    /**
     Returns the host name as it was resolved.
     @return A copy of the host name.
    */
    [[nodiscard]] std::string host_name() const { return std::string(host_view()); }

    /**
     Returns the host name as it was resolved, in an allocator's string.
     @tparam Allocator The string's allocator type.
     @param allocator The allocator of the returned string.
     @return A copy of the host name.
    */
    template <class Allocator>
    [[nodiscard]] std::basic_string<char, std::char_traits<char>, Allocator> host_name(
        const Allocator& allocator = Allocator()) const {
        return std::basic_string<char, std::char_traits<char>, Allocator>(host_view(), allocator);
    }

    /**
     Returns the service name as it was resolved.
     @return A copy of the service name.
    */
    [[nodiscard]] std::string service_name() const { return service_name(std::allocator<char>()); }

    /**
     Returns the service name as it was resolved, in an allocator's string.
     @tparam Allocator The string's allocator type.
     @param allocator The allocator of the returned string.
     @return A copy of the service name.
    */
    template <class Allocator>
    [[nodiscard]] std::basic_string<char, std::char_traits<char>, Allocator> service_name(
        const Allocator& allocator = Allocator()) const {
        std::basic_string<char, std::char_traits<char>, Allocator> name(allocator);
        name.reserve(zeros_ + service_size_);
        name.append(zeros_, '0');
        name.append(service_.data(), service_size_);
        return name;
    }

    /**
     Compares two entries: their endpoints, and their names as they were resolved.
     @param a The first entry.
     @param b The second entry.
     @return Whether they are equal.
    */
    friend bool operator==(const basic_resolver_entry& a, const basic_resolver_entry& b) noexcept {
        return a.endpoint_ == b.endpoint_ && a.host_view() == b.host_view() &&
               a.zeros_ == b.zeros_ && a.service_view() == b.service_view();
    }

private:
    friend class basic_resolver_results<InternetProtocol>;

    /** The longest service kept as it is: `https`, or the five digits of a port. */
    static constexpr std::size_t max_service_size = 5;

    /**
     Constructs an entry from an endpoint, a host of at most `max_host_name_size` bytes, and a
     service that is empty, one of the resolver's names, or digits whose value is the port.
    */
    basic_resolver_entry(const endpoint_type& endpoint, std::string_view host,
                         std::string_view service) noexcept
        : endpoint_(endpoint), host_size_(static_cast<std::uint8_t>(host.size())) {
        assert(host.size() <= max_host_name_size);
        std::ranges::copy(host, host_.begin());
        if (!service.empty() && service.find_first_not_of("0123456789") == std::string_view::npos) {
            // Digits: those of the port, after the zeros that lead them.
            const auto converted =
                std::to_chars(service_.data(), service_.data() + service_.size(), endpoint.port());
            service_size_ = static_cast<std::uint8_t>(converted.ptr - service_.data());
            assert(service.size() >= service_size_);
            zeros_ = service.size() - service_size_;
        } else {
            assert(service.size() <= max_service_size);
            std::ranges::copy(service, service_.begin());
            service_size_ = static_cast<std::uint8_t>(service.size());
        }
    }

    [[nodiscard]] std::string_view host_view() const noexcept { return {host_.data(), host_size_}; }

    [[nodiscard]] std::string_view service_view() const noexcept {
        return {service_.data(), service_size_};
    }

    endpoint_type endpoint_{};
    std::array<char, max_host_name_size> host_{};
    std::size_t zeros_ = 0;
    std::array<char, max_service_size> service_{};
    std::uint8_t host_size_ = 0;
    std::uint8_t service_size_ = 0;
};
}  // namespace webcpp::asio::ip
#endif
#endif  // WEBCPP_ASIO_IP_BASIC_RESOLVER_ENTRY_HPP
