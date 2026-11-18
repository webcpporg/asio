// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#ifndef WEBCPP_ASIO_IP_BASIC_RESOLVER_RESULTS_HPP
#define WEBCPP_ASIO_IP_BASIC_RESOLVER_RESULTS_HPP
#include <webcpp/asio/config.hpp>

#ifndef WEBCPP_TARGET_EMSCRIPTEN
#include WEBCPP_ASIO_BACKEND_HEADER(ip/basic_resolver_results.hpp)  // lint-world: native resolver results

namespace webcpp::asio::ip {
using namespace ::webcpp::asio_backend::ip;
}  // namespace webcpp::asio::ip
#else
#include <cstddef>
#include <string_view>
#include <utility>
#include <webcpp/asio/ip/basic_resolver_entry.hpp>

namespace webcpp::asio::ip {
template <class InternetProtocol, class Executor>
class basic_resolver;

/**
 The results of a page's resolution: none, or the one entry it makes, held in place.

 A page's resolver keeps a host and a service for the page's transport and looks up no name, so
 that it makes at most one entry, which these results hold in the object itself: they allocate
 nothing when they are made, copied or posted with a completion, unlike Boost's results, which
 allocate their list of entries and its shared count. They are a forward range of
 `basic_resolver_entry`, as Boost's are; two results are equal when they hold equal entries.
 @tparam InternetProtocol The internet protocol.
 @see "Sockets and transports", in the guide.
*/
template <class InternetProtocol>
class basic_resolver_results {
public:
    /** The internet protocol type. */
    using protocol_type = InternetProtocol;
    /** The protocol endpoint type. */
    using endpoint_type = InternetProtocol::endpoint;
    /** The entry type. */
    using value_type = basic_resolver_entry<InternetProtocol>;
    /** A reference to a constant entry. */
    using const_reference = const value_type&;
    /** A reference to an entry. */
    using reference = value_type&;
    /** An iterator over the constant entries. */
    using const_iterator = const value_type*;
    /** An iterator over the entries, which are constant. */
    using iterator = const_iterator;
    /** The iterators' difference type. */
    using difference_type = std::ptrdiff_t;
    /** The size type. */
    using size_type = std::size_t;

    /** Constructs empty results. */
    basic_resolver_results() noexcept = default;

    /**
     Returns the number of entries.
     @return One, or zero for empty results.
    */
    [[nodiscard]] size_type size() const noexcept { return size_; }

    /**
     Returns the greatest number of entries results may hold.
     @return One.
    */
    // Boost's results expose max_size as a non-static member.
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    [[nodiscard]] size_type max_size() const noexcept { return 1; }

    /**
     Returns whether the results hold no entry.
     @return Whether they are empty.
    */
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

    /**
     Returns an iterator to the first entry.
     @return The iterator, equal to `end()` for empty results.
    */
    [[nodiscard]] const_iterator begin() const noexcept { return &entry_; }

    /**
     Returns an iterator past the last entry.
     @return The iterator.
    */
    [[nodiscard]] const_iterator end() const noexcept { return &entry_ + size_; }

    /**
     Returns an iterator to the first entry.
     @return The iterator, equal to `cend()` for empty results.
    */
    [[nodiscard]] const_iterator cbegin() const noexcept { return begin(); }

    /**
     Returns an iterator past the last entry.
     @return The iterator.
    */
    [[nodiscard]] const_iterator cend() const noexcept { return end(); }

    /**
     Exchanges the entries of two results.
     @param that The other results.
    */
    void swap(basic_resolver_results& that) noexcept {
        std::swap(entry_, that.entry_);
        std::swap(size_, that.size_);
    }

    /**
     Compares two results by their entries.
     @param a The first results.
     @param b The second results.
     @return Whether both are empty, or both hold equal entries.
    */
    friend bool operator==(const basic_resolver_results& a,
                           const basic_resolver_results& b) noexcept {
        return a.size_ == b.size_ && (a.size_ == 0 || a.entry_ == b.entry_);
    }

private:
    template <class, class>
    friend class basic_resolver;

    /** Constructs the results of one entry, whose names the resolver has checked. */
    basic_resolver_results(const endpoint_type& endpoint, std::string_view host,
                           std::string_view service) noexcept
        : entry_(endpoint, host, service), size_(1) {}

    value_type entry_{};
    size_type size_ = 0;
};
}  // namespace webcpp::asio::ip
#endif
#endif  // WEBCPP_ASIO_IP_BASIC_RESOLVER_RESULTS_HPP
