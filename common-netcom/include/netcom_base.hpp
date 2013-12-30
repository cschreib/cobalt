#ifndef NETCOM_BASE_HPP
#define NETCOM_BASE_HPP

#include <stdexcept>
#include <variadic.hpp>
#include <lock_free_queue.hpp>
#include <member_comparator.hpp>
#include <sorted_vector.hpp>
#include <std_addon.hpp>
#include <signal.hpp>
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

namespace netcom_exception {
    struct base : public std::runtime_error {
        explicit base(const std::string& s);
        explicit base(const char* s);
        virtual ~base() noexcept;
    };

    // Exception thrown when too many requests are issued at once.
    // Each request is given a unique ID, so that the netcom class can easily route the packets to
    // the proper callback functions. This exception is thrown when it is not possible to produce a
    // unique ID anymore. If this happens, then there is most likely a bug: either the receiver does not
    // answer to any request, and they pile up here awaiting for an answer, or you are sending more
    // requests than what the receiver can handle in a given time interval.
    // Can be thrown by: netcom::send_request().
    struct too_many_requests : public base {
        too_many_requests() : base("error.netcom.too_many_requests") {}
    };

    // Exception thrown when a given request type is watched by more than one watcher. Only one watcher
    // can answer to a request: if multiple watchers were allowed, one would not know how to pick the
    // one that should answer. This is a bug of the user code.
    template<typename RequestType>
    struct request_already_watched : public base {
        request_already_watched() : base("error.netcom.request_already_watched") {}
    };

    // Exception thrown when more than one answer is sent for a given request. This is a bug in the user
    // code.
    template<typename RequestType>
    struct request_already_answered : public base {
        request_already_answered() : base("error.netcom.request_already_answered") {}
    };

    // Exception thrown when a request is received and sent to a registered handler, but this handler
    // does not provide any answer (even a failure). This is a bug in the user code.
    template<typename RequestType>
    struct request_not_answered : public base {
        request_not_answered() : base("error.netcom.request_not_answered") {}
    };

    // Exception thrown when working on packets with unspecified origin/recipient. This is a bug.
    struct invalid_actor : public base {
        invalid_actor() : base("error.netcom.invalid_actor") {}
    };
}

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

    // Public object that encapsulates request answering.
    template<typename RequestType>
    struct request_t {
        static_assert(packet_impl::is_packet<RequestType>::value,
            "template argument of request_t must be a packet");

        using packet_t = RequestType;
        using answer_t = typename RequestType::answer;
        using failure_t = typename RequestType::failure;

    private :
        template<typename P>
        friend struct request_signal_impl;

        request_t(netcom_base& net, actor_id_t aid, in_packet_t&& p) :
            net_(net), aid_(aid), answered_(false), arg(arg_) {
            p >> rid_;
            p >> arg_;
        }

        request_t(const request_t&) = delete;
        request_t(request_t&&) = delete;
        request_t& operator= (const request_t&) = delete;
        request_t& operator= (request_t&&) = delete;

        ~request_t() {
            if (!answered_) throw netcom_exception::request_not_answered<RequestType>();
        }

        netcom_base& net_;
        actor_id_t aid_;
        request_id_t rid_;
        mutable bool answered_;
        packet_t arg_;

    public :
        const packet_t& arg;

        actor_id_t from() const {
            return aid_;
        }

        template<typename enable = typename std::enable_if<std::is_empty<answer_t>::value>::type>
        void answer() const;
        void answer(typename RequestType::answer&& answer) const;

        template<typename enable = typename std::enable_if<std::is_empty<failure_t>::value>::type>
        void fail() const;
        void fail(typename RequestType::failure&& fail) const;
    };

    template<typename T>
    struct is_request : std::false_type {};

    template<typename T>
    struct is_request<request_t<T>> : std::true_type {};

    struct message_signal_t {
        explicit message_signal_t(packet_id_t id_) : id(id_) {}
        virtual ~message_signal_t() = default;
        virtual void dispatch(in_packet_t&& p) = 0;
        virtual bool empty() const = 0;
        virtual void clear() = 0;
        const packet_id_t id;
    };

    template<typename P>
    struct message_signal_impl : message_signal_t {
        signal_t<const P&> signal;

        message_signal_impl() : message_signal_t(P::packet_id__) {}

        void dispatch(in_packet_t&& p) override {
            P tp;
            p.impl >> tp;
            signal.dispatch(tp);
        }

        bool empty() const override {
            return signal.empty();
        }

        void clear() override {
            signal.clear();
        }
    };

    struct request_signal_t {
        explicit request_signal_t(packet_id_t id_) : id(id_) {}
        virtual ~request_signal_t() = default;
        virtual void dispatch(netcom_base& net, in_packet_t&& p) = 0;
        virtual bool empty() const = 0;
        virtual void clear() = 0;
        const packet_id_t id;
    };

    template<typename P>
    struct request_signal_impl : request_signal_t {
        signal_t<const request_t<P>&> signal;

        request_signal_impl() : request_signal_t(P::packet_id__) {}

        void dispatch(netcom_base& net, in_packet_t&& p) override {
            request_t<P> r(net, p.from, std::move(p));
            signal.dispatch(r);
        }

        bool empty() const override {
            return signal.empty();
        }

        void clear() override {
            signal.clear();
        }
    };

    class answer_connection : public signal_connection_t {
    public :
        template<typename ... Args>
        answer_connection(Args&& ... args) : signal_connection_t(std::forward<Args>(args)...) {}

        netcom_base* net;
        request_id_t rid;

        void stop() override;
    };

    struct answer_signal_t {
        explicit answer_signal_t(request_id_t id_) : id(id_) {}
        virtual ~answer_signal_t() = default;
        virtual void dispatch_answer(in_packet_t&& p) = 0;
        virtual void dispatch_fail(in_packet_t&& p) = 0;
        virtual void dispatch_unhandled() = 0;
        virtual bool empty() const = 0;
        virtual void clear() = 0;
        const request_id_t id;
    };

    template<typename P>
    struct answer_signal_impl : answer_signal_t {
        using answer_t = typename P::answer;
        using failure_t = typename P::failure;

        signal_t<const answer_t&> answer_signal;
        signal_t<const failure_t&> failure_signal;
        signal_t<> unhandled_signal;

        explicit answer_signal_impl(request_id_t id) : answer_signal_t(id) {}

        void dispatch_answer(in_packet_t&& p) override {
            answer_t a;
            p >> a;
            answer_signal.dispatch(a);
        }

        void dispatch_fail(in_packet_t&& p) override {
            failure_t f;
            p >> f;
            failure_signal.dispatch(f);
        }

        void dispatch_unhandled() override {
            unhandled_signal.dispatch();
        }

        bool empty() const override {
            return answer_signal.empty();
        }

        void clear() override {
            answer_signal.clear();
            failure_signal.clear();
            unhandled_signal.clear();
        }
    };
}

/// Contains all available watch policies for netcom_base.
namespace watch_policy {
    /// No policy (default).
    struct none {};

    /// Only call the slot once.
    struct once {
        template<typename CO, typename F, typename ... Args>
        struct adaptor {
            explicit adaptor(F&& f_) : f(f_) {}
            F f;

            void operator() (CO& c, Args... args) {
                f(args...);
                c.stop();
            }
        };
    };
}

namespace netcom_impl {
    template<typename CO, template<typename, typename...> class M, typename F>
    struct make_slot_type_;

    template<typename CO, template<typename, typename...> class M, typename R, typename T, typename ... Args>
    struct make_slot_type_<CO, M, R (T::*)(Args...)> {
        using type = M<CO, T, Args...>;
    };

    template<typename CO, template<typename, typename...> class M, typename R, typename T, typename ... Args>
    struct make_slot_type_<CO, M, R (T::*)(Args...) const> {
        using type = M<CO, T, Args...>;
    };

    template<typename CO, typename P, typename F>
    using make_slot_type = typename make_slot_type_<
        CO, P::template adaptor, decltype(&F::operator())
    >::type;

    template<typename CO, typename P>
    struct make_slot {
        template<typename F>
        static make_slot_type<CO,P,F> make(F&& f) {
            return make_slot_type<CO,P,F>(std::forward<F>(f));
        }
    };

    template<typename CO>
    struct make_slot<CO, watch_policy::none> {
        template<typename F>
        static F&& make(F&& f) {
            return std::forward<F>(f);
        }
    };
}

/// Base class of network communication.
/** This class must be inherited from. The derived class must then take care of filling the input
    packet queue (input_) and consuming the output packet queue (output_). These two queues are
    thread safe, and can thus be filled/consumed in another thread. The rest of the code is not
    guaranteed to be thread safe, and doesn't use any thread.
    In particular, this is true for all the callback functions that are registered to requests,
    their answers or simple messages. These are called sequentially in process_packets(), in a
    synchronous way.
**/
class netcom_base {
public :
    netcom_base();

    virtual ~netcom_base() = default;

    using in_packet_t = netcom_impl::in_packet_t;
    using out_packet_t = netcom_impl::out_packet_t;

    /// Invalid ID.
    static const actor_id_t invalid_actor_id = -1;
    /// Virtual ID that represents oneself.
    static const actor_id_t self_actor_id = 0;
    /// Virtual ID that encompasses all actors except oneself.
    static const actor_id_t all_actor_id = 1;
    /// ID of the server.
    static const actor_id_t server_actor_id = 2;
    /// ID that will be attributed to the first client.
    /** Next clients will be attributed IDs greater than this one.
    **/
    static const actor_id_t first_actor_id = 3;

    // Import helper classes in this scope for the user
    template<typename T>
    using request_t = netcom_impl::request_t<T>;

protected :
    /// Packet input queue
    /** Filled by derivate when recieved from the network, consumed by netcom_base
        (process_packets()).
    **/
    ctl::lock_free_queue<in_packet_t>  input_;

    /// Packet output queue
    /** Filled by netcom_base (send_message(), send_request()), consumed by derivate to send them
        over the network.
    **/
    ctl::lock_free_queue<out_packet_t> output_;

private :
    using message_signal_t = netcom_impl::message_signal_t;
    using request_signal_t = netcom_impl::request_signal_t;
    using answer_signal_t = netcom_impl::answer_signal_t;
    template<typename T>
    using message_signal_impl = netcom_impl::message_signal_impl<T>;
    template<typename T>
    using request_signal_impl = netcom_impl::request_signal_impl<T>;
    template<typename T>
    using answer_signal_impl = netcom_impl::answer_signal_impl<T>;

    bool clearing_ = false;

    // Message signals
    using message_signal_container = ctl::sorted_vector<
        std::unique_ptr<message_signal_t>, mem_var_comp(&message_signal_t::id)
    >;
    message_signal_container message_signals_;

    // Request signals
    using request_signal_container = ctl::sorted_vector<
        std::unique_ptr<request_signal_t>, mem_var_comp(&request_signal_t::id)
    >;
    request_signal_container request_signals_;

    // Answer signals
    using answer_signal_container = ctl::sorted_vector<
        std::unique_ptr<answer_signal_t>, mem_var_comp(&answer_signal_t::id)
    >;
    answer_signal_container answer_signals_;
    ctl::sorted_vector<request_id_t, std::greater<request_id_t>> available_request_ids_;

private :
    // Send a packet to the output queue
    void send_(out_packet_t&& p);

private :
    // Request manipulation
    friend class netcom_impl::answer_connection;
    void stop_request_(request_id_t id);
    void free_request_id_(request_id_t id);
    request_id_t make_request_id_();

    // Signal manipulation
    template<typename T>
    answer_signal_impl<T>& get_answer_signal_(request_id_t id) {
        auto iter = answer_signals_.insert(std::unique_ptr<answer_signal_t>(
            new answer_signal_impl<T>(id)
        ));
        return static_cast<answer_signal_impl<T>&>(**iter);
    }

    template<typename T>
    message_signal_impl<T>& get_message_signal_() {
        auto iter = message_signals_.find(T::packet_id__);
        if (iter == message_signals_.end()) {
            iter = message_signals_.insert(std::unique_ptr<message_signal_t>(
                new message_signal_impl<T>()
            ));
        }

        return static_cast<message_signal_impl<T>&>(**iter);
    }

    template<typename T>
    request_signal_impl<T>& get_request_signal_() {
        auto iter = request_signals_.find(T::packet_id__);
        if (iter == request_signals_.end()) {
            iter = request_signals_.insert(std::unique_ptr<request_signal_t>(
                new request_signal_impl<T>()
            ));
        }

        return static_cast<request_signal_impl<T>&>(**iter);
    }

private :
    // Packet processing
    bool process_message_(in_packet_t&& p);
    bool process_request_(in_packet_t&& p);
    bool process_answer_(in_packet_t&& p);
    bool process_failure_(in_packet_t&& p);
    bool process_unhandled_(in_packet_t&& p);

protected :
    /// Create a message packet without sending it
    template<typename MessageType, typename ... Args>
    out_packet_t create_message_(MessageType&& msg) {
        out_packet_t p;
        p << netcom_impl::packet_type::message;
        p << MessageType::packet_id__;
        p << msg;
        return p;
    }

    /// Create a request packet without sending it
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

    /// Clear all incoming and outgoing packets without processing them, and clear all signals.
    /** When this function is called, the class is in the same state as when constructed.
        It modifies the input_ and output_ queues in a non thread safe way.
    **/
    void clear_all_();

public :
    /// Distributes all the received packets to the registered callback functions.
    /** Should be called often enough so that packets are treated as soon as they arrive, for
        example inside the game loop.
    **/
    void process_packets();

    /// Send a message to a given actor.
    template<typename MessageType>
    void send_message(actor_id_t aid, MessageType&& msg = MessageType()) {
        out_packet_t p = create_message_(std::forward<MessageType>(msg));
        p.to = aid;
        send_(std::move(p));
    }

    /// Send a request with the provided arguments, and register a slot to handle the answer.
    /** The callback function will be called by process_packets() when the corresponding answer is
        received, if the request has been properly issued. If the request fails, no action will be
        taken.
    **/
    template<typename RequestType, typename FR>
    signal_connection_t& send_request(actor_id_t aid, RequestType&& req, FR&& receive_func) {
        static_assert(ctl::argument_count<FR>::value == 1,
            "answer reception handler can only take one argument (packet::answer)");
        using AnswerType = typename std::decay<ctl::functor_argument<FR>>::type;
        static_assert(std::is_same<AnswerType, typename RequestType::answer>::value,
            "answer reception handler can only take a packet::answer as argument");

        request_id_t rid;
        out_packet_t p = create_request_(rid, std::forward<RequestType>(req));
        p.to = aid;

        answer_signal_impl<RequestType>& netsig = get_answer_signal_<RequestType>(rid);
        netcom_impl::answer_connection& ac = netsig.answer_signal.template connect<
            netcom_impl::answer_connection>(
            [=](signal_connection_t& c, ctl::functor_argument<FR> a) {
                receive_func(a);
                c.stop();
            }
        );

        send_(std::move(p));

        return ac;
    }

    /// Send a request with the provided arguments, and register three slots to handle the answer.
    /** The first slot is called in case the request is successfuly answered, the second is called
        in case of failure, and the last one is called in case the request could not be received on
        the other side. These callback functions will be called by process_packets() when the
        corresponding answer is received.
    **/
    template<typename RequestType, typename FR, typename FF, typename UF = decltype(ctl::no_op)>
    signal_connection_t& send_request(actor_id_t aid, RequestType&& req, FR&& receive_func,
        FF&& failure_func, UF&& unhandled_func = ctl::no_op) {
        static_assert(ctl::argument_count<FR>::value == 1,
            "answer reception handler can only take one argument (packet::answer)");
        using AnswerType = typename std::decay<ctl::functor_argument<FR>>::type;
        static_assert(std::is_same<AnswerType, typename RequestType::answer>::value,
            "answer reception handler can only take a packet::answer as argument");
        static_assert(ctl::argument_count<FF>::value == 1,
            "failure reception handler can only take one argument (packet::failure)");
        using FailureType = typename std::decay<ctl::functor_argument<FF>>::type;
        static_assert(std::is_same<FailureType, typename RequestType::failure>::value,
            "failure reception handler can only take a packet::failure as argument");
        static_assert(ctl::argument_count<UF>::value == 0,
            "unhandled request reception handler cannot have arguments");

        request_id_t rid;
        out_packet_t p = create_request_(rid, std::forward<RequestType>(req));
        p.to = aid;

        answer_signal_impl<RequestType>& netsig = get_answer_signal_<RequestType>(rid);
        netsig.failure_signal.connect(std::forward<FF>(failure_func));
        netsig.unhandled_signal.connect(std::forward<UF>(unhandled_func));
        netcom_impl::answer_connection& ac = netsig.answer_signal.template connect<
            netcom_impl::answer_connection>(
            [=](netcom_impl::answer_connection& c, ctl::functor_argument<FR> a) {
                receive_func(a);
                c.stop();
            }
        );

        send_(std::move(p));

        return ac;
    }

public :
    /// Register a slot to be called whenever a given message packet is received.
    /** The slot will be called by process_packets() when the corresponding message is received, and
        has to take only one argument whose type is that of the corresponding packet.
    **/
    template<typename WP = watch_policy::none, typename FR>
    signal_connection_t& watch_message(FR&& receive_func) {
        static_assert(ctl::argument_count<FR>::value == 1,
            "message reception handler can only take one argument");
        using MessageType = typename std::decay<ctl::functor_argument<FR>>::type;
        static_assert(packet_impl::is_packet<MessageType>::value,
            "message reception handler argument must be a packet");

        message_signal_impl<MessageType>& netsig = get_message_signal_<MessageType>();
        return netsig.signal.connect(netcom_impl::make_slot<signal_connection_t,WP>::make(
            std::forward<FR>(receive_func)
        ));
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

    template<typename>
    friend struct netcom_impl::request_t;

public :
    /// Register a slot to be called whenever a given request packet is received.
    /** The slot will be called by process_packets() when the corresponding request is received, and
        will have to answer it either by giving a proper answer or by signaling failure. It must
        take only one argument: a request_t<>.
    **/
    template<typename WP = watch_policy::none, typename FR>
    signal_connection_t& watch_request(FR&& receive_func) {
        static_assert(ctl::argument_count<FR>::value == 1,
            "request reception handler can only take one argument");
        using Arg = typename std::decay<ctl::functor_argument<FR>>::type;
        static_assert(netcom_impl::is_request<Arg>::value,
            "message reception handler argument must be a request_t");

        using RequestType = typename Arg::packet_t;

        request_signal_impl<RequestType>& netsig = get_request_signal_<RequestType>();
        if (!netsig.empty()) throw netcom_exception::request_already_watched<RequestType>();

        return netsig.signal.connect(netcom_impl::make_slot<signal_connection_t,WP>::make(
            std::forward<FR>(receive_func)
        ));
    }
};

namespace netcom_impl {
    template<typename RequestType>
    void request_t<RequestType>::answer(typename RequestType::answer&& answer) const {
        if (answered_) throw netcom_exception::request_already_answered<RequestType>();
        net_.send_answer_<RequestType>(aid_, rid_, std::move(answer));
        answered_ = true;
    }

    template<typename RequestType>
    void request_t<RequestType>::fail(typename RequestType::failure&& fail) const {
        if (answered_) throw netcom_exception::request_already_answered<RequestType>();
        net_.send_failure_<RequestType>(aid_, rid_, std::move(fail));
        answered_ = true;
    }

    template<typename RequestType>
    template<typename enable>
    void request_t<RequestType>::answer() const {
        if (answered_) throw netcom_exception::request_already_answered<RequestType>();
        net_.send_answer_<RequestType>(aid_, rid_, answer_t{});
        answered_ = true;
    }

    template<typename RequestType>
    template<typename enable>
    void request_t<RequestType>::fail() const {
        if (answered_) throw netcom_exception::request_already_answered<RequestType>();
        net_.send_failure_<RequestType>(aid_, rid_, failure_t{});
        answered_ = true;
    }
}

#endif
