// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#ifndef WEBCPP_ASIO_BROWSER_ASYNC_WAIT_EVENT_HPP
#define WEBCPP_ASIO_BROWSER_ASYNC_WAIT_EVENT_HPP

#include <webcpp/asio/config.hpp>

#if !WEBCPP_ASIO_HAS_BROWSER_BRIDGE
#error \
    "webcpp::asio::browser::async_wait_event requires the Emscripten browser bridge; use native/WASI Asio operations on this target"
#else

#include WEBCPP_ASIO_BACKEND_HEADER(any_io_executor.hpp)  // lint-world: the operation's executor
#include WEBCPP_ASIO_BACKEND_HEADER(append.hpp)  // lint-world: preserve handler associations when posting values
#include WEBCPP_ASIO_BACKEND_HEADER(associated_allocator.hpp)  // lint-world: operation storage belongs to its handler
#include WEBCPP_ASIO_BACKEND_HEADER(associated_cancellation_slot.hpp)  // lint-world: cancel an outstanding operation
#include WEBCPP_ASIO_BACKEND_HEADER(associated_executor.hpp)  // lint-world: retain the handler executor while pending
#include WEBCPP_ASIO_BACKEND_HEADER(async_result.hpp)  // lint-world: completion token initiation
#include WEBCPP_ASIO_BACKEND_HEADER(error.hpp)         // lint-world: Asio's cancellation error
#include WEBCPP_ASIO_BACKEND_HEADER(executor_work_guard.hpp)  // lint-world: keep both contexts alive until completion
#include WEBCPP_ASIO_BACKEND_HEADER(post.hpp)  // lint-world: completions only post to the event loop
#include <boost/smart_ptr/allocate_unique.hpp>
#include <cstdint>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>
#include <webcpp/asio/browser/bridge.hpp>

namespace webcpp::asio::browser {
namespace detail {
/** One event wait, owned by its bridge until completion or shutdown. */
template <class Handler, class Executor>
class event_operation final : public registration {
public:
    /** Allocates this operation with the handler's associated allocator. */
    using storage_allocator =
        std::allocator_traits<::webcpp::asio_backend::associated_allocator_t<Handler>>::
            template rebind_alloc<event_operation>;

    /** Constructs a pending wait that owns outstanding work on executor. */
    event_operation(bridge& owner, const Executor& executor, Handler handler)
        : bridge_(owner),
          work_(::webcpp::asio_backend::prefer(
              executor, ::webcpp::asio_backend::execution::outstanding_work_t::tracked)),
          completion_work_(::webcpp::asio_backend::get_associated_executor(handler, executor)),
          handler_(std::move(handler)) {}

    /** Removes a listener and registration, including when initiation throws. */
    ~event_operation() { detach(); }

    /** Not copyable: the bridge and cancellation callback refer to this address. */
    event_operation(const event_operation&) = delete;
    /** Not copy-assignable: the registration has one owner. */
    event_operation& operator=(const event_operation&) = delete;
    /** Not movable: registered callbacks refer to this address. */
    event_operation(event_operation&&) = delete;
    /** Not move-assignable: registered callbacks refer to this address. */
    event_operation& operator=(event_operation&&) = delete;

    /** Registers a one-shot listener and cancellation callback. */
    void start(const emscripten::val& target, const emscripten::val& type) {
        target_ = target;
        type_ = type;
        id_ = bridge_.add(*this);
        auto slot = ::webcpp::asio_backend::get_associated_cancellation_slot(handler_);
        if (slot.is_connected()) {
            slot.assign([this](::webcpp::asio_backend::cancellation_type kind) {
                if ((kind & ::webcpp::asio_backend::cancellation_type::all) !=
                    ::webcpp::asio_backend::cancellation_type::none) {
                    complete(::webcpp::asio_backend::error::operation_aborted,
                             emscripten::val::undefined());
                }
            });
        }
        function_ = bridge_.function(id_);
        auto options = emscripten::val::object();
        options.set("once", true);
        target_.call<void>("addEventListener", type_, function_, options);
    }

    /** Posts the next event and releases the one-shot listener. */
    bool deliver(const emscripten::val& event) override {
        complete({}, event);
        return true;
    }

    /** Destroys the pending handler without calling it when its context shuts down. */
    void shutdown() noexcept override {
        destroy(storage_allocator(::webcpp::asio_backend::get_associated_allocator(handler_)));
    }

private:
    /** Removes the listener, cancellation callback and bridge key once. */
    void detach() {
        if (id_ != 0) {
            ::webcpp::asio_backend::get_associated_cancellation_slot(handler_).clear();
            bridge_.remove(id_);
            id_ = 0;
            if (!function_.isUndefined()) {
                target_.call<void>("removeEventListener", type_, function_);
            }
        }
    }

    /** Removes registration before freeing state and posting its completion. */
    void complete(boost::system::error_code ec, emscripten::val value) {
        detach();
        const storage_allocator allocator(
            ::webcpp::asio_backend::get_associated_allocator(handler_));
        const auto cleanup = [allocator](event_operation* operation) {
            operation->destroy(allocator);
        };
        std::unique_ptr<event_operation, decltype(cleanup)> owned(this, cleanup);
        auto handler = std::move(handler_);
        const auto executor = work_;
        const auto completion_work = completion_work_;
        owned.reset();
        ::webcpp::asio_backend::post(
            executor, ::webcpp::asio_backend::append(std::move(handler), ec, std::move(value)));
    }

    /** Destroys and deallocates this wait through its associated allocator. */
    void destroy(storage_allocator allocator) noexcept {
        std::allocator_traits<storage_allocator>::destroy(allocator, this);
        std::allocator_traits<storage_allocator>::deallocate(allocator, this, 1);
    }

    bridge& bridge_;
    decltype(::webcpp::asio_backend::prefer(
        std::declval<Executor>(),
        ::webcpp::asio_backend::execution::outstanding_work_t::tracked)) work_;
    ::webcpp::asio_backend::executor_work_guard<
        ::webcpp::asio_backend::associated_executor_t<Handler, Executor>>
        completion_work_;
    Handler handler_;
    std::uint64_t id_ = 0;
    emscripten::val target_ = emscripten::val::undefined();
    emscripten::val type_ = emscripten::val::undefined();
    emscripten::val function_ = emscripten::val::undefined();
};

/** Allocates and starts a wait once the completion token initiates it. */
struct initiate_event {
    /** Starts the operation with handler, executor, target and event type. */
    template <class Handler, class Executor>
    void operator()(Handler&& handler, const Executor& executor, const emscripten::val& target,
                    const emscripten::val& type) const {
        using operation = event_operation<std::decay_t<Handler>, Executor>;
        auto& context =
            ::webcpp::asio_backend::query(executor, ::webcpp::asio_backend::execution::context);
        auto& owner = ::webcpp::asio_backend::use_service<bridge>(context);
        const typename operation::storage_allocator allocator(
            ::webcpp::asio_backend::get_associated_allocator(handler));
        auto pending = boost::allocate_unique<operation>(allocator, owner, executor,
                                                         std::forward<Handler>(handler));
        pending->start(target, type);
        (void)pending.release();
    }
};
}  // namespace detail

/**
 Asynchronously receives the next event of type on a JavaScript EventTarget.

 Completion is posted through the bridge. Terminal, partial or total cancellation
 removes the listener and posts operation_aborted and undefined. A pending wait
 owns outstanding work. Context destruction removes the listener and destroys its
 handler silently. The event type is copied even when initiation is deferred.

 @tparam Executor The concrete or type-erased executor of the owning context.
 @tparam CompletionToken A token for void(boost::system::error_code, emscripten::val).
 @param executor The executor whose context owns the wait.
 @param target The JavaScript EventTarget.
 @param type The event type, which need not end in a null byte.
 @param token The completion token.
 @return The result determined by token.
 @see "Await one browser event", in the guide.
*/
template <class CompletionToken, class Executor = ::webcpp::asio_backend::any_io_executor>
auto async_wait_event(const Executor& executor, emscripten::val target, std::string_view type,
                      CompletionToken&& token) {
    auto name = emscripten::val::take_ownership(webcpp_asio_string(type.data(), type.size()));
    return ::webcpp::asio_backend::async_initiate<CompletionToken,
                                                  void(boost::system::error_code, emscripten::val)>(
        detail::initiate_event{}, token, executor, std::move(target), std::move(name));
}
}  // namespace webcpp::asio::browser

#endif  // WEBCPP_ASIO_HAS_BROWSER_BRIDGE
#endif  // WEBCPP_ASIO_BROWSER_ASYNC_WAIT_EVENT_HPP
