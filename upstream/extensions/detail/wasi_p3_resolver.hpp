// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)
#ifndef WEBCPP_ASIO_DETAIL_WASI_P3_RESOLVER_HPP
#define WEBCPP_ASIO_DETAIL_WASI_P3_RESOLVER_HPP
#if defined(__wasip3__)

#include <unistd.h>
#include <wasi/wasip3.h>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/detail/bind_handler.hpp>
#include <boost/asio/detail/handler_alloc_helpers.hpp>
#include <boost/asio/detail/handler_work.hpp>
#include <boost/asio/detail/select_reactor.hpp>
#include <boost/asio/detail/socket_ops.hpp>
#include <boost/asio/detail/wasi_waitable.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/basic_resolver_query.hpp>
#include <boost/asio/ip/basic_resolver_results.hpp>
#include <boost/asio/ip/detail/endpoint.hpp>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

// This adapter is included only by the checked WASIp3 Boost overlay.
namespace boost {
namespace asio {
BOOST_ASIO_INLINE_NAMESPACE_BEGIN

namespace detail {

struct wasi_resolver_state;

struct wasi_resolver_node {
    wasi_resolver_state* owner = nullptr;
    wasi_resolver_node* next = nullptr;
    void (*cancel)(wasi_resolver_node*) = nullptr;
    void detach();
};

struct wasi_resolver_state {
    wasi_resolver_node* head = nullptr;

    void cancel_all() {
        while (head) {
            auto* node = head;
            node->detach();
            node->cancel(node);
        }
    }

    ~wasi_resolver_state() { cancel_all(); }
};

inline void wasi_resolver_node::detach() {
    if (!owner)
        return;
    auto** current = &owner->head;
    while (*current != this)
        current = &(*current)->next;
    *current = next;
    owner = nullptr;
    next = nullptr;
}

inline void wasi_resolver_construct(std::shared_ptr<void>& impl, execution_context& context) {
    impl = std::allocate_shared<wasi_resolver_state>(
        execution_context::allocator<wasi_resolver_state>(context));
}

inline void wasi_resolver_cancel(std::shared_ptr<void>& impl) {
    if (impl)
        static_cast<wasi_resolver_state*>(impl.get())->cancel_all();
}

inline boost::system::error_code wasi_resolver_error(unsigned char error) {
    switch (error) {
        case IP_NAME_LOOKUP_ERROR_CODE_ACCESS_DENIED: return boost::asio::error::access_denied;
        case IP_NAME_LOOKUP_ERROR_CODE_INVALID_ARGUMENT:
            return boost::asio::error::invalid_argument;
        case IP_NAME_LOOKUP_ERROR_CODE_NAME_UNRESOLVABLE: return boost::asio::error::host_not_found;
        case IP_NAME_LOOKUP_ERROR_CODE_TEMPORARY_RESOLVER_FAILURE:
            return boost::asio::error::host_not_found_try_again;
        default: return boost::asio::error::no_recovery;
    }
}

template <class Protocol, class Handler, class IoExecutor>
class wasi_resolve_op : public reactor_op, public wasi_resolver_node {
    using query_type = boost::asio::ip::basic_resolver_query<Protocol>;
    using results_type = boost::asio::ip::basic_resolver_results<Protocol>;
    using endpoint_type = typename Protocol::endpoint;
    using allocator_type = associated_allocator_t<Handler>;
    using char_allocator =
        typename std::allocator_traits<allocator_type>::template rebind_alloc<char>;
    using endpoint_allocator =
        typename std::allocator_traits<allocator_type>::template rebind_alloc<endpoint_type>;
    using string_type = std::basic_string<char, std::char_traits<char>, char_allocator>;
    using slot_type = associated_cancellation_slot_t<Handler>;

    select_reactor& reactor_;
    select_reactor::per_descriptor_data reactor_data_{};
    string_type host_;
    string_type service_;
    addrinfo_type hints_;
    std::vector<endpoint_type, endpoint_allocator> endpoints_;
    Handler handler_;
    handler_work<Handler, IoExecutor> work_;
    slot_type slot_;
    webcpp_asio_wasip3::waitable_descriptor descriptor_{};
    webcpp_asio_wasip3::lookup_result result_{};
    int fd_ = -1;
    bool has_result_ = false;
    bool started_ = false;
    bool waiting_ = false;
    bool cancelled_ = false;
    unsigned short port_ = 0;
    bool allow_v4_ = true;
    bool allow_v6_ = true;

    struct slot_cancel {
        wasi_resolve_op* operation;

        void operator()(cancellation_type_t type) {
            if (type != cancellation_type::none)
                operation->request_cancel();
        }
    };

    void request_cancel() {
        cancelled_ = true;
        if (waiting_)
            reactor_.cancel_ops_after_poll(fd_, reactor_data_);
    }

    // Only numeric address parsing and the libc service table are used here.
    // AI_NUMERICHOST prevents this helper from ever initiating host DNS.
    void numeric(const char* host, addrinfo_type hints,
                 std::vector<endpoint_type, endpoint_allocator>& output,
                 boost::system::error_code& ec) {
        hints.ai_flags |= AI_NUMERICHOST;
        addrinfo_type* addresses = nullptr;
        socket_ops::getaddrinfo(host, service_.c_str(), hints, &addresses, ec);

        struct address_owner {
            addrinfo_type* value;

            ~address_owner() {
                if (value)
                    socket_ops::freeaddrinfo(value);
            }
        } owner{addresses};

        if (ec)
            return;
        for (auto* entry = addresses; entry; entry = entry->ai_next) {
            if (entry->ai_family != AF_INET && entry->ai_family != AF_INET6)
                continue;
            endpoint_type endpoint;
            if (entry->ai_addrlen > endpoint.capacity())
                continue;
            std::memcpy(endpoint.data(), entry->ai_addr, entry->ai_addrlen);
            endpoint.resize(entry->ai_addrlen);
            output.push_back(endpoint);
        }
    }

    void append_endpoint(const ip::detail::endpoint& source) {
        endpoint_type endpoint;
        if (source.size() <= endpoint.capacity()) {
            std::memcpy(endpoint.data(), source.data(), source.size());
            endpoint.resize(source.size());
            endpoints_.push_back(endpoint);
        }
    }

    bool begin() {
        started_ = true;
        if (cancelled_) {
            ec_ = boost::asio::error::operation_aborted;
            return false;
        }
        // WASI returns addresses, not canonical names or IPv4 mapping policy.
        if (hints_.ai_flags & (AI_CANONNAME | AI_V4MAPPED | AI_ALL)) {
            ec_ = boost::asio::error::operation_not_supported;
            return false;
        }
        numeric(host_.empty() ? nullptr : host_.c_str(), hints_, endpoints_, ec_);
        if (!ec_ || (hints_.ai_flags & AI_NUMERICHOST))
            return false;
        if (ec_ != boost::asio::error::host_not_found &&
            ec_ != boost::asio::error::host_not_found_try_again)
            return false;

        // Validate the service and implement AI_ADDRCONFIG using libc's local
        // numeric-family checks, before starting the asynchronous name lookup.
        auto local_hints = hints_;
        local_hints.ai_family = AF_INET;
        boost::system::error_code v4_error, v6_error;
        numeric("127.0.0.1", local_hints, endpoints_, v4_error);
        allow_v4_ = !v4_error && hints_.ai_family != AF_INET6;
        if (!endpoints_.empty())
            port_ = socket_ops::network_to_host_short(
                reinterpret_cast<const sockaddr_in*>(endpoints_.front().data())->sin_port);
        endpoints_.clear();
        local_hints.ai_family = AF_INET6;
        numeric("::1", local_hints, endpoints_, v6_error);
        allow_v6_ = !v6_error && hints_.ai_family != AF_INET;
        if (!endpoints_.empty())
            port_ = socket_ops::network_to_host_short(
                reinterpret_cast<const sockaddr_in*>(endpoints_.front().data())->sin_port);
        endpoints_.clear();
        if (!allow_v4_ && !allow_v6_) {
            ec_ = hints_.ai_family == AF_INET6 ? v6_error : v4_error;
            if (!ec_)
                ec_ = boost::asio::error::host_not_found;
            return false;
        }
        ec_.clear();
        auto status = webcpp_asio_wasip3::start(host_.data(), host_.size(), result_);
        descriptor_.state = WASIP3_SUBTASK_STATE(status);
        descriptor_.handle = WASIP3_SUBTASK_HANDLE(status);
        if (descriptor_.state == WASIP3_SUBTASK_RETURNED) {
            has_result_ = true;
            perform_lookup(this);
            return false;
        }
        fd_ = webcpp_asio_wasip3::open(descriptor_);
        if (fd_ < 0) {
            ec_ = boost::system::error_code(errno, boost::asio::error::get_system_category());
            return false;
        }
        int const registration = reactor_.register_descriptor(fd_, reactor_data_);
        if (registration != 0) {
            ec_ =
                boost::system::error_code(registration, boost::asio::error::get_system_category());
            return false;
        }
        waiting_ = true;
        reactor_.start_op(select_reactor::read_op, fd_, reactor_data_, this, false, false);
        return true;
    }

public:
    BOOST_ASIO_DEFINE_HANDLER_PTR(wasi_resolve_op);

    wasi_resolve_op(wasi_resolver_state& state, execution_context& context, query_type const& query,
                    Handler& handler, IoExecutor const& executor)
        : reactor_op({}, &perform_lookup, &complete),
          reactor_(boost::asio::use_service<select_reactor>(context)),
          host_(query.host_name().c_str(), char_allocator(get_associated_allocator(handler))),
          service_(query.service_name().c_str(), char_allocator(get_associated_allocator(handler))),
          hints_(query.hints()),
          endpoints_(endpoint_allocator(get_associated_allocator(handler))),
          handler_(static_cast<Handler&&>(handler)),
          work_(handler_, executor),
          slot_(get_associated_cancellation_slot(handler_)) {
        reactor_.init_task();
        if (slot_.is_connected())
            slot_.template emplace<slot_cancel>(this);
        owner = &state;
        next = state.head;
        state.head = this;
        cancel = [](wasi_resolver_node* node) {
            static_cast<wasi_resolve_op*>(node)->request_cancel();
        };
    }

    ~wasi_resolve_op() {
        detach();
        if (slot_.is_connected())
            slot_.clear();
        // No reactor operations refer to the descriptor at completion/shutdown.
        if (fd_ >= 0)
            ::close(fd_);
        webcpp_asio_wasip3::release(descriptor_);
        if (has_result_ || descriptor_.state == WASIP3_SUBTASK_RETURNED)
            webcpp_asio_wasip3::free_result(result_);
    }

    static status perform_lookup(reactor_op* base) {
        auto& self = *static_cast<wasi_resolve_op*>(base);
        if (self.descriptor_.state != WASIP3_SUBTASK_RETURNED)
            return not_done;
        self.has_result_ = true;
        const auto* data = self.result_.bytes.data();
        if (data[0] != std::byte{0}) {
            self.ec_ = wasi_resolver_error(static_cast<unsigned char>(data[sizeof(void*)]));
            return done;
        }
        ip_name_lookup_list_ip_address_t addresses;
        std::memcpy(&addresses, data + sizeof(void*), sizeof(addresses));
        for (std::size_t index = 0; index != addresses.len; ++index) {
            const auto& address = addresses.ptr[index];
            if (address.tag == SOCKETS_IP_ADDRESS_IPV4 && self.allow_v4_) {
                const auto& a = address.val.ipv4;
                self.append_endpoint(
                    ip::detail::endpoint(ip::address_v4({a.f0, a.f1, a.f2, a.f3}), self.port_));
            } else if (address.tag == SOCKETS_IP_ADDRESS_IPV6 && self.allow_v6_) {
                const auto& a = address.val.ipv6;
                const std::uint16_t words[] = {a.f0, a.f1, a.f2, a.f3, a.f4, a.f5, a.f6, a.f7};
                ip::address_v6::bytes_type bytes{};
                for (std::size_t i = 0; i != 8; ++i) {
                    bytes[2 * i] = static_cast<unsigned char>(words[i] >> 8);
                    bytes[2 * i + 1] = static_cast<unsigned char>(words[i]);
                }
                self.append_endpoint(ip::detail::endpoint(ip::address_v6(bytes), self.port_));
            }
        }
        if (self.endpoints_.empty())
            self.ec_ = boost::asio::error::host_not_found;
        return done;
    }

    static void complete(void* completion_owner, operation* base, boost::system::error_code const&,
                         std::size_t) {
        auto* self = static_cast<wasi_resolve_op*>(base);
        ptr memory = {std::addressof(self->handler_), self, self};
        if (completion_owner && !self->started_ && self->begin()) {
            memory.v = memory.p = nullptr;
            return;
        }
        self->waiting_ = false;
        if (self->cancelled_)
            self->ec_ = boost::asio::error::operation_aborted;
        handler_work<Handler, IoExecutor> work(
            static_cast<handler_work<Handler, IoExecutor>&&>(self->work_));
        binder2<Handler, boost::system::error_code, results_type> handler(self->handler_, self->ec_,
                                                                          results_type{});
        memory.h = std::addressof(handler.handler_);
        if (completion_owner && !self->ec_) {
            // Boost's public result container retains its upstream allocator.
            handler.arg2_ =
                results_type::create(self->endpoints_.begin(), self->endpoints_.end(),
                                     std::string(self->host_.data(), self->host_.size()),
                                     std::string(self->service_.data(), self->service_.size()));
        }
        memory.reset();
        if (completion_owner)
            work.complete(handler, handler.handler_);
    }
};

template <class Protocol, class Handler, class IoExecutor>
void wasi_async_resolve(std::shared_ptr<void>& impl, execution_context& context,
                        typename boost::asio::ip::basic_resolver_query<Protocol> const& query,
                        Handler& handler, IoExecutor const& executor) {
    if (!impl)
        wasi_resolver_construct(impl, context);
    using op = wasi_resolve_op<Protocol, Handler, IoExecutor>;
    typename op::ptr memory = {std::addressof(handler), op::ptr::allocate(handler), nullptr};
    memory.p = new (memory.v)
        op(*static_cast<wasi_resolver_state*>(impl.get()), context, query, handler, executor);
    boost::asio::use_service<select_reactor>(context).post_immediate_completion(memory.p, false);
    memory.v = memory.p = nullptr;
}

}  // namespace detail

BOOST_ASIO_INLINE_NAMESPACE_END
}  // namespace asio
}  // namespace boost
#endif  // __wasip3__
#endif  // WEBCPP_ASIO_DETAIL_WASI_P3_RESOLVER_HPP
