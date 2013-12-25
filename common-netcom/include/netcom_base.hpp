#ifndef NETCOM_BASE_HPP
#define NETCOM_BASE_HPP

#include <stdexcept>
#include <variadic.hpp>
#include <lock_free_queue.hpp>
#include <member_comparator.hpp>
#include <sorted_vector.hpp>
#include <ptr_vector.hpp>
#include <std_addon.hpp>
#include "packet.hpp"

// Unique ID attributed to any request.
// It is external, but only used explicitly by the sender. It must thus only be unique from the
// point of view of the sender.
using request_id_t = std::uint16_t;
// Unique ID associated to an actor.
// The server can communicate to several different actors (the clients), while the clients can only
// communicate with the server. Nevertheless, actor IDs can circulate over the network if a client
// wants to send a message to another client, using the server as a bridge.
using actor_id_t = std::uint16_t;

class netcom_base;

// List of messages that can be sent by this class
namespace message {
    NETCOM_PACKET(unhandled_message) {
        packet_id_t message_id;
    };

    NETCOM_PACKET(unhandled_request) {
        packet_id_t request_id;
    };

    NETCOM_PACKET(unhandled_answer) {
        request_id_t request_id;
    };

    NETCOM_PACKET(unhandled_failure) {
        request_id_t request_id;
    };
}

// List of requests that one can send with this class
namespace request {
    NETCOM_PACKET(ping) {
        struct answer {};
        struct failure {};
    };
}

#ifndef NO_AUTOGEN
#include "autogen/packets/netcom_base.hpp"
#endif

// Exception thrown when too many requests are issued at once.
// Each request is given a unique ID, so that the netcom class can easily route the packets to
// the proper callback functions. This exception is thrown when it is not possible to produce a
// unique ID anymore. If this happens, then there is most likely a bug: either the receiver does not
// answer to any request, and they pile up here awaiting for an answer, or you are sending more
// requests than what the receiver can handle in a given time interval.
// Can be thrown by: netcom::send_request().
struct too_many_requests : public std::runtime_error {
    too_many_requests() : std::runtime_error("error.netcom.too_many_requests") {}
};

// Exception thrown when too many watches are active at the same time.
// Each time a watch is created on a given packet, it is assigned a unique ID for easy external
// manipulation. This exception is thrown when it is not possible to produce a unique ID anymore.
// The data type of this ID should be chosen so that this situation never happens, so this is a bug.
struct too_many_watches : public std::runtime_error {
    too_many_watches() : std::runtime_error("error.netcom.too_many_watches") {}
};

// Exception thrown when a given request type is watched by more than one watcher. Only one watcher
// can answer to a request: if multiple watchers were allowed, one would not know how to pick the
// one that should answer. This is a bug of the user code.
template<typename RequestType>
struct request_already_watched : public std::runtime_error {
    request_already_watched() : std::runtime_error("error.netcom.request_already_watched") {}
};

// Exception thrown when more than one answer is sent for a given request. This is a bug in the user
// code.
template<typename RequestType>
struct request_already_answered : public std::runtime_error {
    request_already_answered() : std::runtime_error("error.netcom.request_already_answered") {}
};

// Exception thrown when a request is received and sent to a registered handler, but this handler
// does not provide any answer (even a failure). This is a bug in the user code.
template<typename RequestType>
struct request_not_answered : public std::runtime_error {
    request_not_answered() : std::runtime_error("error.netcom.request_not_answered") {}
};

// Exception thrown when working on packets with unspecified origin/recipient. This is a bug.
struct invalid_actor : public std::runtime_error {
    invalid_actor() : std::runtime_error("error.netcom.invalid_actor") {}
};

// Implementation details that should not filter out of the netcom class.
namespace netcom_impl {
    struct in_packet_t {
        in_packet_t() : from(-1) {}
        explicit in_packet_t(actor_id_t from_) : from(from_) {}
        in_packet_t(const in_packet_t&) = default;
        in_packet_t(in_packet_t&&) = default;
        in_packet_t& operator= (const in_packet_t&) = default;
        in_packet_t& operator= (in_packet_t&&) = default;

        template<typename T>
        in_packet_t& operator << (T&& t) {
            impl << std::forward<T>(t);
            return *this;
        }

        template<typename T>
        in_packet_t& operator >> (T& t) {
            impl >> t;
            return *this;
        }

        actor_id_t from;
        sf::Packet impl;
    };

    struct out_packet_t {
        out_packet_t() : to(-1) {}
        explicit out_packet_t(actor_id_t to_) : to(to_) {}
        out_packet_t(const out_packet_t&) = default;
        out_packet_t(out_packet_t&&) = default;
        out_packet_t& operator= (const out_packet_t&) = default;
        out_packet_t& operator= (out_packet_t&&) = default;

        template<typename T>
        out_packet_t& operator << (T&& t) {
            impl << std::forward<T>(t);
            return *this;
        }

        template<typename T>
        out_packet_t& operator >> (T& t) {
            impl >> t;
            return *this;
        }

        in_packet_t& to_input() {
            return *reinterpret_cast<in_packet_t*>(this);
        }

        actor_id_t to;
        sf::Packet impl;
    };

    // General type of a packet.
    enum class packet_type : std::uint8_t {
        message,          // simple message, does not expects any answer
        request,          // asks for a value or an action, and expects an answer
        answer,           // answer to a successful request
        failure,          // answer to a failed request
        unhandled_request // request cannot be handled
    };

    // Unique ID attributed to any message watch.
    using watch_id_t = std::uint16_t;
    struct request_pool_t;

    // Base class for internal storage of requests.
    // This internal request must take care of de-serializing the received packet in order to call
    // the associated callback function(s), whether the request has been issued (receive()) or not
    // (fail()).
    struct request_t {
        explicit request_t(request_id_t id_) : id(id_), canceled(false), pool(nullptr) {}
        virtual ~request_t();

        request_t(const request_t&) = delete;
        request_t(request_t&&) = delete;
        request_t& operator= (const request_t&) = delete;
        request_t& operator= (request_t&&) = delete;

        virtual void receive(in_packet_t&&) = 0;
        virtual void fail(in_packet_t&&) = 0;
        virtual void unhandled() = 0;

        const request_id_t id;
        bool canceled;
        request_pool_t* pool;
    };

    // Implementation of a request with no handling of the failure case.
    template<typename RType, typename RFunc>
    struct request_only : public request_t {
        request_only(request_id_t id_, RFunc&& f) :
            request_t(id_), receive_func_(std::move(f)) {}

    private :
        RFunc receive_func_;

    public :
        void receive(in_packet_t&& p) override {
            typename RType::answer a;
            p >> a;
            receive_func_(a);
        }

        void fail(in_packet_t&& p) override {}

        void unhandled() override {}
    };

    // Full implementation of a request, with success and failure cases.
    template<typename RType, typename RFunc, typename FFunc, typename UHFunc>
    struct request_or_fail : public request_only<RType, RFunc> {
        request_or_fail(request_id_t id_, RFunc&& rf, FFunc&& ff, UHFunc&& uf) :
            request_only<RType, RFunc>(id_, std::move(rf)), fail_func_(std::move(ff)),
            unhandled_func_(std::move(uf)) {}

    private :
        FFunc fail_func_;
        UHFunc unhandled_func_;

    public :
        void fail(in_packet_t&& p) override {
            typename RType::failure f;
            p >> f;
            fail_func_(f);
        }

        void unhandled() override {
            unhandled_func_();
        }
    };

    // Public object that encapsulates request answering.
    template<typename RequestType>
    struct netcom_request_t {
        using packet_t = RequestType;
        using answer_t = typename RequestType::answer;
        using failure_t = typename RequestType::failure;

    private :
        template <typename, typename>
        friend class request_watch_impl;
        friend class ::netcom_base;

        netcom_request_t(netcom_base& net, actor_id_t aid, in_packet_t&& p) :
            net_(net), aid_(aid), answered_(false), args(args_) {
            p >> rid_;
            p >> args_;
        }

        netcom_request_t(const netcom_request_t&) = delete;
        netcom_request_t(netcom_request_t&&) = delete;
        netcom_request_t& operator= (const netcom_request_t&) = delete;
        netcom_request_t& operator= (netcom_request_t&&) = delete;

        ~netcom_request_t() {
            if (!answered_) throw request_not_answered<RequestType>();
        }

        netcom_base& net_;
        actor_id_t aid_;
        request_id_t rid_;
        bool answered_;
        packet_t args_;

    public :
        const packet_t& args;

        actor_id_t from() const {
            return aid_;
        }

        template<typename enable = typename std::enable_if<std::is_empty<answer_t>::value>::type>
        void answer();
        void answer(typename RequestType::answer&& answer);

        template<typename enable = typename std::enable_if<std::is_empty<failure_t>::value>::type>
        void fail();
        void fail(typename RequestType::failure&& fail);
    };

    // RAII class to keep requests alive.
    // This object is returned by netcom::send_request(). When it is destroyed and if auto
    // cancellation is enabled (auto_cancel(), disabled by default), the associated request is
    // canceled, and its callback function(s) are not called, even if the receiver eventually
    // answers. The request can also be canceled manually using the cancel() member function. The
    // destructor then does nothing.
    // It is safe to manipulate this object by value, since it is non-copyable and has move
    // semantic.
    struct request_keeper_t {
        request_keeper_t(const request_keeper_t&) = delete;
        request_keeper_t(request_keeper_t&& k) noexcept : net_(k.net_), rid_(k.rid_),
            empty_(k.empty_), auto_cancel_(k.auto_cancel_) {
            k.empty_ = true;
        }

        request_keeper_t& operator= (const request_keeper_t&) = delete;
        request_keeper_t& operator= (request_keeper_t&& k) noexcept {
            net_ = k.net_;
            rid_ = k.rid_;
            empty_ = k.empty_;
            auto_cancel_ = k.auto_cancel_;
            k.empty_ = true;
            return *this;
        }

        ~request_keeper_t() {
            if (auto_cancel_) {
                notify_pool_(nullptr);
                cancel();
            }
        }

        void auto_cancel(bool autoc = true) {
            auto_cancel_ = autoc;
        }

        void cancel();

        bool canceled() const;

        packet_id_t id() const {
            return id_;
        }

        request_id_t rid() const {
            return rid_;
        }

    private :
        friend class ::netcom_base;
        friend struct request_pool_t;

        void notify_pool_(request_pool_t* pool);

        request_keeper_t(netcom_base& net, packet_id_t id, request_id_t rid) :
            net_(&net), id_(id), rid_(rid), empty_(false), auto_cancel_(false) {}

        netcom_base* net_ = nullptr;
        packet_id_t id_;
        request_id_t rid_;
        mutable bool empty_ = true;
        bool auto_cancel_;
    };

    // The request pool is an optional helper class that owns a list of pending requests, keeping
    // them alive as long as no answer is received (or until they are explicitly canceled by the
    // user). This class is automatically informed when one of its request is answered or canceled
    // so there is never any "dead" requests in the list.
    struct request_pool_t {
        request_pool_t() = default;
        request_pool_t(const request_pool_t&) = delete;
        request_pool_t(request_pool_t&&) = default;
        request_pool_t& operator= (const request_pool_t&) = delete;
        request_pool_t& operator= (request_pool_t&&) = default;

        ~request_pool_t() {
            for (auto& r : pool_) {
                r.notify_pool_(nullptr);
            }
        }

        request_pool_t& operator << (request_keeper_t&& k) {
            add(std::move(k));
            return *this;
        }

        void add(request_keeper_t&& r) {
            pool_.push_back(std::move(r));
            pool_.back().notify_pool_(this);
        }

        void cancel_all() {
            for (auto& r : pool_) {
                r.notify_pool_(nullptr);
                r.cancel();
            }

            pool_.clear();
            pool_.shrink_to_fit();
        }

        bool empty() const {
            return pool_.empty();
        }

        void merge(request_pool_t&& p) {
            for (auto& r : p.pool_) {
                pool_.push_back(std::move(r));
            }

            p.pool_.clear();
            p.pool_.shrink_to_fit();
        }

    private :
        friend struct request_t;

        void remove_(request_id_t id) {
            for (auto iter = pool_.begin(); iter != pool_.end(); ++iter) {
                if (iter->rid() == id) {
                    pool_.erase(iter);
                    break;
                }
            }
        }

        ptr_vector<request_keeper_t> pool_;
    };

    // Base class for request watches.
    // This object must take care of de-serializing the received packet in order to call the
    // associated callback function(s) to answer the request.
    struct request_watch_t {
        request_watch_t(packet_id_t id_) : id(id_), canceled(false), held_(false) {}
        virtual ~request_watch_t() {}

        request_watch_t(const request_watch_t&) = delete;
        request_watch_t(request_watch_t&&) = delete;
        request_watch_t& operator= (const request_watch_t&) = delete;
        request_watch_t& operator= (request_watch_t&&) = delete;

        virtual void receive_and_answer(netcom_base& net, in_packet_t&&) = 0;

        bool held() const {
            return held_;
        }

        void hold() {
            held_ = true;
        }

        void hold(in_packet_t&& p) {
            held_packets_.push_back(std::move(p));
        }

        void release(netcom_base& net);

        const packet_id_t id;
        bool canceled;

    private :
        bool held_;
        std::vector<in_packet_t> held_packets_;
    };

    // Implementation for a given request type
    template<typename RType, typename RFunc>
    struct request_watch_impl : public request_watch_t {
        request_watch_impl(RFunc&& rf) :
            request_watch_t(RType::packet_id__), receive_func_(rf) {}

    private :
        RFunc receive_func_;

    public :
        void receive_and_answer(netcom_base& net, in_packet_t&& p) override {
            netcom_request_t<RType> req(net, p.from, std::move(p));
            receive_func_(req);
        }
    };

    // Base class for message watches.
    // This object must take care of de-serializing the received packet in order to call the
    // associated callback function(s).
    struct message_watch_t {
        explicit message_watch_t(watch_id_t id_) : id(id_), canceled(false), held_(false) {}
        virtual ~message_watch_t() {}

        message_watch_t(const message_watch_t&) = delete;
        message_watch_t(message_watch_t&&) = delete;
        message_watch_t& operator= (const message_watch_t&) = delete;
        message_watch_t& operator= (message_watch_t&&) = delete;

        virtual void receive(in_packet_t&&) = 0;

        bool held() const {
            return held_;
        }

        void hold() {
            held_ = true;
        }

        void hold(in_packet_t&& p) {
            held_packets_.push_back(std::move(p));
        }

        void release() {
            if (!canceled) {
                for (auto& p : held_packets_) {
                    receive(std::move(p));
                }
            }

            held_packets_.clear();
            held_ = false;
        }

        const watch_id_t id;
        bool canceled;

    private :
        bool held_;
        std::vector<in_packet_t> held_packets_;
    };

    // Implementation for a given message type
    template<typename MType, typename RFunc>
    struct message_watch_impl : public message_watch_t {
        message_watch_impl(watch_id_t id_, RFunc&& rf) :
            message_watch_t(id_), receive_func_(rf) {}

    private :
        RFunc receive_func_;

    public :
        void receive(in_packet_t&& p) override {
            MType msg;
            p >> msg;
            receive_func_(msg);
        }
    };

    // Simple wrapper around a vector of message watches (cheap multimap).
    struct message_watch_group_t {
        explicit message_watch_group_t(packet_id_t id_) : id(id_) {}

        message_watch_group_t(const message_watch_group_t&) = delete;
        message_watch_group_t(message_watch_group_t&&) = default;
        message_watch_group_t& operator= (const message_watch_group_t&) = delete;
        message_watch_group_t& operator= (message_watch_group_t&&) = default;

        packet_id_t id;

        using container_type = sorted_vector<
            std::unique_ptr<netcom_impl::message_watch_t>,
            mem_var_comp(&netcom_impl::message_watch_t::id)
        >;
        container_type group;
    };

    // Base class of watch selectors.
    struct selector_base_t {
        explicit selector_base_t(netcom_base& net) : net_(net) {}
        virtual ~selector_base_t() {}

        selector_base_t(const selector_base_t&) = delete;
        selector_base_t(selector_base_t&&) = delete;
        selector_base_t& operator= (const selector_base_t&) = delete;
        selector_base_t& operator= (selector_base_t&&) = delete;

        virtual void cancel() = 0;
        virtual bool canceled() const = 0;
        virtual void hold() = 0;
        virtual bool held() const = 0;
        virtual void release() = 0;
        virtual packet_id_t id() const = 0;

    protected :
        netcom_base& net_;
    };

    // Request watch selector.
    template<typename RequestType>
    struct request_selector_t : public selector_base_t {
        explicit request_selector_t(netcom_base& net) :
            selector_base_t(net), cancelled_(false), held_(false) {}

        ~request_selector_t() {
            release();
        }

        void cancel() override;

        bool canceled() const override;

        bool held() const override {
            return held_;
        }

        void hold() override;

        void release() override;

        packet_id_t id() const override {
            return RequestType::packet_id__;
        }

    private :
        mutable bool cancelled_;
        bool held_;
    };

    // Message watch selector.
    template<typename MessageType>
    struct message_selector_t : public selector_base_t {
        message_selector_t(netcom_base& net, watch_id_t id) :
            selector_base_t(net), id_(id), cancelled_(false), held_(false) {}

        void cancel() override;

        bool canceled() const override;

        bool held() const override {
            return held_;
        }

        void hold() override;

        void release() override;

        packet_id_t id() const override {
            return MessageType::packet_id__;
        }

    private :
        watch_id_t id_;
        mutable bool cancelled_;
        bool held_;
    };

    // Wrapper class to handle cancellation, holding and releasing of message/request watches.
    // When a new packet is being watched, either via netcom::watch_message() or
    // netcom::watch_request(), a watch_keeper_t is returned, and is the only accessible handle on
    // the internal watch system. One can:
    //  1) Cancel the watch, by calling 'cancel()'. Packets will not be processed by this watch
    //     anymore. It will be destroyed automatically when possible by the netcom.
    //  2) Temporarily hold back the incoming packets by calling 'hold()'. They will be stored in a
    //     queue as soon as they arrive, and processed later, only when the user calls 'release()'.
    //     Holding is then disabled and the situation goes back to normal, packets being processed
    //     as they arrive.
    //
    // Note that, like request_keeper_t, this class can automatically cancel its watched item on
    // destruction. The major difference here is that this is the *default behavior*. To disable it,
    // one has to call 'auto_cancel(false)', but this is not recommended: unlike requests which get
    // automatically canceled as soon as their answer has arrived, watchers are potentially
    // eternal, and can only be canceled manually through the use of this watch_keeper_t. Hence,
    // if auto_cancel is disabled and this watch_keeper_t is destroyed, there is no way to cancel
    // the watched item any more, and it will remain active until the netcom class is destroyed.
    struct watch_keeper_t {
        watch_keeper_t(const watch_keeper_t&) = delete;
        watch_keeper_t(watch_keeper_t&& w) = default;
        watch_keeper_t& operator= (const watch_keeper_t&) = delete;
        watch_keeper_t& operator= (watch_keeper_t&& w) = default;

        ~watch_keeper_t() {
            if (auto_cancel_) cancel();
        }

        void auto_cancel(bool autoc = true) {
            auto_cancel_ = autoc;
        }

        void cancel() {
            if (sel_) {
                sel_->cancel();
                sel_ = nullptr;
            }
        }

        bool canceled() const {
            if (!sel_) return true;
            return sel_->canceled();
        }

        bool held() {
            if (!sel_) return false;
            return sel_->held();
        }

        void hold() {
            if (sel_) sel_->hold();
        }

        void release() {
            if (sel_) sel_->release();
        }

        packet_id_t id() const {
            return id_;
        }

    private :
        friend class ::netcom_base;

        explicit watch_keeper_t(std::unique_ptr<selector_base_t>&& sel) :
            sel_(std::move(sel)), id_(sel_ ? sel_->id() : 0), auto_cancel_(true) {}

        std::unique_ptr<selector_base_t> sel_;
        packet_id_t id_;
        bool auto_cancel_;
    };

    // The watch pool is a tool to make it easier to manage the lifetime of watch keepers.
    // It is basically just a container that provides clearer semantics and functions that affect
    // all watchers at once (hold_all, release_all, ...).
    // Watch keepers that are stored here are *exclusively* managed by this class. The only
    // exception to this rule is when the whole netcom is stopped: watchers are all killed, and all
    // pools are automatically notified, so the user has nothing to do.
    struct watch_pool_t {
        explicit watch_pool_t(netcom_base& net);
        ~watch_pool_t();

        watch_pool_t(const watch_pool_t&) = delete;
        watch_pool_t(watch_pool_t&&) = default;
        watch_pool_t& operator= (const watch_pool_t&) = delete;
        watch_pool_t& operator= (watch_pool_t&&) = default;

        watch_pool_t& operator << (watch_keeper_t&& w) {
            add(std::move(w));
            return *this;
        }

        void add(watch_keeper_t&& w) {
            watchers_.push_back(std::move(w));
            if (held_) {
                watchers_.back().hold();
            }
        }

        bool empty() const {
            return watchers_.empty();
        }

        bool held() {
            return held_;
        }

        void cancel_all() {
            watchers_.clear();
            watchers_.shrink_to_fit();
        }

        template<typename T>
        void cancel_all() {
            cancel_all(T::packet_id__);
        }

        void cancel_all(packet_id_t id) {
            watchers_.erase(
                std::remove_if(watchers_.begin(), watchers_.end(), [&](const watch_keeper_t& w) {
                    return w.id() == id;
                }),
                watchers_.end()
            );
        }

        void hold_all() {
            if (held_) return;
            for (auto& w : watchers_) {
                w.hold();
            }
        }

        template<typename T>
        void hold_all() {
            hold_all(T::packet_id__);
        }

        void hold_all(packet_id_t id) {
            if (held_) return;
            for (auto& w : watchers_) {
                if (w.id() == id) {
                    w.hold();
                }
            }
        }

        void release_all() {
            if (!held_) return;
            for (auto& w : watchers_) {
                w.release();
            }
        }

        template<typename T>
        void release_all() {
            release_all(T::packet_id__);
        }

        void release_all(packet_id_t id) {
            for (auto& w : watchers_) {
                if (w.id() == id) {
                    w.release();
                }
            }
        }

        void merge(watch_pool_t&& pool) {
            if (held_ && !pool.held_) {
                pool.hold_all();
            }

            watchers_.insert(watchers_.end(),
                std::make_move_iterator(pool.watchers_.begin()),
                std::make_move_iterator(pool.watchers_.end())
            );

            pool.watchers_.clear();
            pool.watchers_.shrink_to_fit();
        }

    private :
        netcom_base& net_;
        bool held_;
        std::vector<watch_keeper_t> watchers_;
    };
}

// Base class of network communication.
// This class must be inherited from. The derived class must then take care of filling the input
// packet queue (input_) and consuming the output packet queue (output_). These two queues are
// thread safe, and can thus be filled/consumed in another thread. The rest of the code is not
// guaranteed to be thread safe, and doesn't use any thread.
// In particular, all the callback functions that are registered to requests, their answers or
// simple messages. These are called sequentially either when one calls 'process_packets()' or when
// one releases a watch, in a synchronous way.
class netcom_base {
public :
    netcom_base();

    virtual ~netcom_base() = default;

    using in_packet_t = netcom_impl::in_packet_t;
    using out_packet_t = netcom_impl::out_packet_t;

    static const actor_id_t invalid_actor_id = -1;
    static const actor_id_t self_actor_id = 0;
    static const actor_id_t all_actor_id = 1;
    static const actor_id_t server_actor_id = 2;
    static const actor_id_t first_actor_id = 3;

    // Import helper classes in this scope for the user
    using request_keeper_t = netcom_impl::request_keeper_t;
    using request_pool_t = netcom_impl::request_pool_t;
    using watch_keeper_t = netcom_impl::watch_keeper_t;
    using watch_pool_t = netcom_impl::watch_pool_t;
    template<typename T>
    using request_t = netcom_impl::netcom_request_t<T>;

protected :
    // Packet input and output queues
    lock_free_queue<in_packet_t>  input_;
    lock_free_queue<out_packet_t> output_;

private :
    template<typename>
    friend struct netcom_impl::request_selector_t;
    template<typename>
    friend struct netcom_impl::message_selector_t;
    friend struct netcom_impl::request_watch_t;
    friend struct netcom_impl::watch_pool_t;

    // Requests that have been made by this actor.
    using request_container = sorted_vector<
        std::unique_ptr<netcom_impl::request_t>,
        mem_var_comp(&netcom_impl::request_t::id)
    >;
    request_container requests_;
    sorted_vector<request_id_t, std::greater<request_id_t>> available_request_ids_;

    // Message watches created by this actor.
    using message_watch_container = sorted_vector<
        netcom_impl::message_watch_group_t,
        mem_var_comp(&netcom_impl::message_watch_group_t::id)
    >;
    message_watch_container message_watches_;
    sorted_vector<netcom_impl::watch_id_t, std::greater<request_id_t>> available_mwatch_ids_;

    // Request watches created by this actor.
    using request_watch_container = sorted_vector<
        std::unique_ptr<netcom_impl::request_watch_t>,
        mem_var_comp(&netcom_impl::request_watch_t::id)
    >;
    request_watch_container request_watches_;

    // Watch pools used by this actor.
    sorted_vector<watch_pool_t*> watch_pools_;

private :
    // Send a packet to the output queue
    void send_(out_packet_t&& p);

private :
    friend struct netcom_impl::request_keeper_t;

    // Request manipulation
    bool request_cancelled_(request_id_t id) const;
    void request_notify_pool_(request_id_t id, request_pool_t* pool);
    void cancel_request_(request_id_t id);
    void cancel_request_(request_container::iterator iter);
    void free_request_id_(request_id_t id);
    request_id_t make_request_id_();

private :
    // Message watcher manipulation
    bool message_watch_cancelled_(packet_id_t id, netcom_impl::watch_id_t wid);
    void cancel_message_watch_(packet_id_t id, netcom_impl::watch_id_t wid);
    void cancel_message_watch_(netcom_impl::message_watch_group_t::container_type::iterator iter);
    void hold_message_watch_(packet_id_t id, netcom_impl::watch_id_t wid);
    void release_message_watch_(packet_id_t id, netcom_impl::watch_id_t wid);
    void free_message_watch_id_(netcom_impl::watch_id_t id);
    netcom_impl::watch_id_t make_message_watch_id_();

private :
    // Request watcher manipulation
    bool request_watch_cancelled_(packet_id_t id);
    void cancel_request_watch_(packet_id_t id);
    void cancel_request_watch_(request_watch_container::iterator iter);
    void hold_request_watch_(packet_id_t id);
    void release_request_watch_(packet_id_t id);

private :
    // Watch pool registration
    void register_watch_pool_(watch_pool_t* pool);
    void unregister_watch_pool_(watch_pool_t* pool);

private :
    // Packet processing
    bool process_message_(in_packet_t&& p);
    bool process_request_(in_packet_t&& p);
    bool process_answer_(in_packet_t&& p);
    bool process_failure_(in_packet_t&& p);
    bool process_unhandled_(in_packet_t&& p);

protected :
    // Packet creation functions
    template<typename MessageType, typename ... Args>
    out_packet_t create_message_(MessageType&& msg) {
        out_packet_t p;
        p << netcom_impl::packet_type::message;
        p << MessageType::packet_id__;
        p << msg;
        return p;
    }

    template<typename MessageType, typename ... Args>
    out_packet_t create_message_(Args&& ... args) {
        out_packet_t p;
        p << netcom_impl::packet_type::message;
        p << MessageType::packet_id__;
        p << make_packet<MessageType>(std::forward<Args>(args)...);
        return p;
    }

    template<typename RequestType, typename ... Args>
    out_packet_t create_request_(request_id_t& rid, RequestType&& req) {
        rid = make_request_id_();

        out_packet_t p;
        p << netcom_impl::packet_type::request;
        p << RequestType::packet_id__;
        p << rid;
        p << req;
        return p;
    }

    template<typename RequestType, typename ... Args>
    out_packet_t create_request_(request_id_t& rid, Args&& ... args) {
        rid = make_request_id_();

        out_packet_t p;
        p << netcom_impl::packet_type::request;
        p << RequestType::packet_id__;
        p << rid;
        p << make_packet<RequestType>(std::forward<Args>(args)...);
        return p;
    }

    // Clear all incoming and outgoing packets without processing them, and clear all requests and
    // watchers. When this function is called, the class is in the same state as when constructed.
    // Should be called in a thread safe environment.
    void clear_all_();

public :
    // Distributes all the received packets to the registered callback functions.
    // Should be called often enough so that packets are treated as soon as they arrive, for example
    // inside the game loop.
    void process_packets();

    // Send a message with the provided arguments.
    template<typename MessageType>
    void send_message(actor_id_t aid, MessageType&& msg = MessageType()) {
        out_packet_t p = create_message_(std::forward<MessageType>(msg));
        p.to = aid;
        send_(std::move(p));
    }

    // Send a request with the provided arguments, and register a callback function to handle the
    // response. The callback function will be called by process_packets() when the corresponding
    // answer is received, if the request has been properly issued. If the request fails, no action
    // will be taken. The request can be canceled by the returned request_keeper_t at any time if
    // not needed anymore.
    template<typename RequestType, typename FR>
    request_keeper_t send_request(actor_id_t aid, RequestType&& req, FR&& receive_func) {
        static_assert(std::is_same<function_argument<FR>, typename RequestType::answer>::value,
            "wrong argument of 'reception' handler");

        request_id_t rid;
        out_packet_t p = create_request_(rid, std::forward<RequestType>(req));
        p.to = aid;

        using request_type = netcom_impl::request_only<RequestType, FR>;
        requests_.insert(std::make_unique<request_type>(rid, std::move(receive_func)));

        send_(std::move(p));

        return request_keeper_t(*this, RequestType::packet_id__, rid);
    }

    // Send a request with the provided arguments, and register three callback functions to handle
    // the response (one in case of success, another in case of failure, and the last one in case
    // the request could not be received on the other side). The callback functions will be called
    // by process_packets() when the corresponding answer is received. The request can be canceled
    // by the returned request_keeper_t at any time if not needed anymore.
    template<typename RequestType, typename FR, typename FF, typename UF = decltype(no_op)>
    request_keeper_t send_request(actor_id_t aid, RequestType&& req, FR&& receive_func,
        FF&& failure_func, UF&& unhandled_func = no_op) {
        static_assert(std::is_same<function_argument<FR>, typename RequestType::answer>::value,
            "wrong argument of 'reception' handler");
        static_assert(std::is_same<function_argument<FF>, typename RequestType::failure>::value,
            "wrong argument of 'failure' handler");
        static_assert(argument_count<UF>::value == 0,
            "'unhandled' handler cannot have arguments");

        request_id_t rid;
        out_packet_t p = create_request_(rid, std::forward<RequestType>(req));
        p.to = aid;

        using request_type = netcom_impl::request_or_fail<RequestType, FR, FF, UF>;
        requests_.insert(std::make_unique<request_type>(
            rid, std::move(receive_func), std::move(failure_func), std::move(unhandled_func)
        ));

        send_(std::move(p));

        return request_keeper_t(*this, RequestType::packet_id__, rid);
    }

    template<typename FR>
    watch_keeper_t watch_message(FR&& receive_func) {
        static_assert(argument_count<FR>::value == 1,
            "message reception handler can only take one argument");

        using MessageType = typename std::decay<function_argument<FR>>::type;

        netcom_impl::watch_id_t wid = make_message_watch_id_();

        auto iter = message_watches_.find(MessageType::packet_id__);
        if (iter == message_watches_.end()) {
            iter = message_watches_.insert(
                netcom_impl::message_watch_group_t(MessageType::packet_id__)
            );
        }

        using watch_type = netcom_impl::message_watch_impl<MessageType, FR>;
        iter->group.insert(std::make_unique<watch_type>(wid, std::forward<FR>(receive_func)));

        return watch_keeper_t(
            std::make_unique<netcom_impl::message_selector_t<MessageType>>(*this, wid)
        );
    }

private :
    // Send an answer to a request with the provided arguments.
    template<typename RequestType>
    void send_answer_(actor_id_t aid, request_id_t id, typename RequestType::answer&& answer) {
        out_packet_t p(aid);
        p << netcom_impl::packet_type::answer;
        p << id;
        p << answer;
        send_(std::move(p));
    }

    // Send a failure message to a request with the provided arguments.
    template<typename RequestType>
    void send_failure_(actor_id_t aid, request_id_t id, typename RequestType::failure&& fail) {
        out_packet_t p(aid);
        p << netcom_impl::packet_type::failure;
        p << id;
        p << fail;
        send_(std::move(p));
    }

    // Send an "unhandled request" packet when no answer can be given.
    void send_unhandled_(in_packet_t&& p) {
        request_id_t id;
        p >> id;

        out_packet_t op(p.from);
        op << netcom_impl::packet_type::unhandled_request;
        op << id;
        send_(std::move(op));
    }

public :
    template<typename>
    friend struct netcom_impl::netcom_request_t;

    template<typename FR>
    watch_keeper_t watch_request(FR&& receive_func) {
        static_assert(argument_count<FR>::value == 1,
            "request reception handler can only take one argument");

        using Arg = typename std::decay<function_argument<FR>>::type;
        using RequestType = typename Arg::packet_t;

        auto iter = request_watches_.find(RequestType::packet_id__);
        if (iter != request_watches_.end()) throw request_already_watched<RequestType>();

        using watch_type = netcom_impl::request_watch_impl<RequestType, FR>;
        request_watches_.insert(std::make_unique<watch_type>(std::forward<FR>(receive_func)));

        return watch_keeper_t(
            std::make_unique<netcom_impl::request_selector_t<RequestType>>(*this)
        );
    }
};

namespace netcom_impl {
    template<typename RequestType>
    void netcom_request_t<RequestType>::answer(typename RequestType::answer&& answer) {
        if (answered_) throw request_already_answered<RequestType>();
        net_.send_answer_<RequestType>(aid_, rid_, std::move(answer));
        answered_ = true;
    }

    template<typename RequestType>
    void netcom_request_t<RequestType>::fail(typename RequestType::failure&& fail) {
        if (answered_) throw request_already_answered<RequestType>();
        net_.send_failure_<RequestType>(aid_, rid_, std::move(fail));
        answered_ = true;
    }

    template<typename RequestType>
    template<typename enable>
    void netcom_request_t<RequestType>::answer() {
        if (answered_) throw request_already_answered<RequestType>();
        net_.send_answer_<RequestType>(aid_, rid_, answer_t{});
        answered_ = true;
    }

    template<typename RequestType>
    template<typename enable>
    void netcom_request_t<RequestType>::fail() {
        if (answered_) throw request_already_answered<RequestType>();
        net_.send_failure_<RequestType>(aid_, rid_, failure_t{});
        answered_ = true;
    }

    template<typename RequestType>
    void request_selector_t<RequestType>::cancel() {
        if (cancelled_) return;
        net_.cancel_request_watch_(RequestType::packet_id__);
        cancelled_ = true;
    }

    template<typename RequestType>
    bool request_selector_t<RequestType>::canceled() const {
        if (cancelled_) return true;
        cancelled_ = net_.request_watch_cancelled_(RequestType::packet_id__);
        return cancelled_;
    }

    template<typename RequestType>
    void request_selector_t<RequestType>::hold() {
        if (cancelled_ || held_) return;
        net_.hold_request_watch_(RequestType::packet_id__);
        held_ = true;
    }

    template<typename RequestType>
    void request_selector_t<RequestType>::release() {
        if (!held_) return;
        net_.release_request_watch_(RequestType::packet_id__);
        held_ = false;
    }

    template<typename MessageType>
    void message_selector_t<MessageType>::cancel() {
        if (cancelled_) return;
        net_.cancel_message_watch_(MessageType::packet_id__, id_);
        cancelled_ = true;
    }

    template<typename MessageType>
    bool message_selector_t<MessageType>::canceled() const {
        if (cancelled_) return true;
        cancelled_ = net_.message_watch_cancelled_(MessageType::packet_id__, id_);
        return cancelled_;
    }

    template<typename MessageType>
    void message_selector_t<MessageType>::hold() {
        if (cancelled_ || held_) return;
        net_.hold_message_watch_(MessageType::packet_id__, id_);
        held_ = true;
    }

    template<typename MessageType>
    void message_selector_t<MessageType>::release() {
        if (!held_) return;
        net_.release_message_watch_(MessageType::packet_id__, id_);
        held_ = false;
    }
}

#endif
