#ifndef SHARED_COLLECTION_HPP
#define SHARED_COLLECTION_HPP

#include <scoped_connection_pool.hpp>
#include <sorted_vector.hpp>
#include "netcom_base.hpp"

using shared_collection_id_t = std::uint16_t;

namespace request {
    NETCOM_PACKET(observe_shared_collection) {
        shared_collection_id_t id;
        struct answer {};
        struct failure {};
    };

    NETCOM_PACKET(get_shared_collection_id) {
        std::string name;
        struct answer {
            shared_collection_id_t id;
        };
        struct failure {
            enum class reason {
                no_such_collection
            } rsn;
        };
    };
}

namespace message {
    NETCOM_PACKET(leave_shared_collection) {
        shared_collection_id_t id;
    };
    NETCOM_PACKET(shared_collection_add) {
        shared_collection_id_t id;
    };
    NETCOM_PACKET(shared_collection_remove) {
        shared_collection_id_t id;
    };
}

#ifndef NO_AUTOGEN
#include "autogen/packets/shared_collection.hpp"
#endif

namespace shared_collection_exception {
    struct too_many_collections : public netcom_exception::base {
        too_many_collections() :
            netcom_exception::base("error.netcom.too_many_shared_collections") {}
    };

    template<typename E>
    struct missing_make_collection_delegate : public netcom_exception::base {
        missing_make_collection_delegate() :
            netcom_exception::base("error.netcom.missing_make_collection_delegate") {}
    };

    template<typename E>
    struct wrong_collection_type : public netcom_exception::base {
        wrong_collection_type() :
            netcom_exception::base("error.netcom.wrong_collection_type") {}
    };

    template<typename E>
    struct uninitialized_collection : public netcom_exception::base {
        uninitialized_collection() :
            netcom_exception::base("error.netcom.uninitialized_collection") {}
    };

    template<typename E>
    struct uninitialized_observer : public netcom_exception::base {
        uninitialized_observer() :
            netcom_exception::base("error.netcom.uninitialized_observer") {}
    };
}

class shared_collection_factory;

template<typename ElementTraits>
class shared_collection;

template<typename ElementTraits>
class shared_collection_observer;

namespace netcom_impl {
    template<typename T>
    struct get_register_packet {
        template <typename U> static typename U::register_packet
            dummy(typename U::register_packet*);
        template <typename U> static ctl::empty_t dummy(...);
        using type = decltype(dummy<T>(0));
    };
    template<typename T>
    struct get_register_failed_packet {
        template <typename U> static typename U::register_failed_packet
            dummy(typename U::register_failed_packet*);
        template <typename U> static ctl::empty_t dummy(...);
        using type = decltype(dummy<T>(0));
    };

    template<typename T>
    struct shared_collection_traits_proxy {
        /// Request to join the collection (optional)
        using register_packet = typename get_register_packet<T>::type;
        /// Reason for failure to join the collection (optional)
        using register_failed_packet = typename get_register_failed_packet<T>::type;
        /// Request to send the full collection
        using full_packet = typename T::full_packet;
        /// Message sent when a new object is added
        using add_packet = typename T::add_packet;
        /// Message sent when an object is removed
        using remove_packet = typename T::remove_packet;
    };

    class shared_collection_base {
        shared_collection_factory& factory_;

    protected :
        using observe_request = netcom_base::request_t<request::observe_shared_collection>;
        using leave_message = netcom_base::message_t<message::leave_shared_collection>;

        netcom_base& net_;
        ctl::sorted_vector<actor_id_t> clients_;
        bool connected_ = false;

        virtual void register_and_send_collection_(observe_request&& req) const = 0;
        virtual void check_valid_() const = 0;

    public :
        const shared_collection_id_t id;
        const std::string            name;

        shared_collection_base(shared_collection_factory& factory, netcom_base& net,
            const std::string& name, shared_collection_id_t id);

        virtual ~shared_collection_base();
        void destroy();
        void connect();
        void disconnect();
        bool is_connected() const;
        void register_client(observe_request&& req);
        void unregister_client(actor_id_t cid);
    };

    template<typename ElementTraits>
    class shared_collection_impl : public shared_collection_base {
        using proxy = shared_collection_traits_proxy<ElementTraits>;

        /// Request to join the collection
        using register_collection_packet = typename proxy::register_packet;
        /// Reason for failure to join the collection
        using register_collection_failed_packet = typename proxy::register_failed_packet;
        /// Request to send the full collection
        using full_collection_packet = typename proxy::full_packet;
        /// Message sent when a new object is added
        using add_collection_element_packet = typename proxy::add_packet;
        /// Message sent when an object is removed
        using remove_collection_element_packet = typename proxy::remove_packet;

        void register_and_send_collection_(observe_request&& req) const override {
            register_collection_packet r;
            register_collection_failed_packet rf;
            req.packet >> r;
            if (register_client.empty() || register_client(r, rf)) {
                full_collection_packet p;
                make_collection_packet(p);
                req.answer_custom(std::move(p));
            } else {
                req.fail_custom(std::move(rf));
            }
        }

        void check_valid_() const override {
            if (make_collection_packet.empty()) {
                throw shared_collection_exception::missing_make_collection_delegate<ElementTraits>{};
            }
        }

    public :
        shared_collection_impl(shared_collection_factory& factory, netcom_base& net,
            const std::string& name, shared_collection_id_t id) :
            shared_collection_base(factory, net, name, id) {}

        ctl::delegate<bool(const register_collection_packet& reg,
            register_collection_failed_packet& failure)> register_client;
        ctl::delegate<void(full_collection_packet& req)> make_collection_packet;

        template<typename ... Args>
        void add_item(Args&& ... args) const {
            if (!connected_) return;
            auto p = net_.create_custom_message<message::shared_collection_add>(
                id, make_packet<add_collection_element_packet>(std::forward<Args>(args)...)
            );

            for (auto cid : clients_) {
                p.to = cid;
                net_.send(p);
            }
        }

        template<typename ... Args>
        void remove_item(Args&& ... args) const {
            if (!connected_) return;
            auto p = net_.create_custom_message<message::shared_collection_remove>(
                id, make_packet<remove_collection_element_packet>(std::forward<Args>(args)...)
            );

            for (auto cid : clients_) {
                p.to = cid;
                net_.send(p);
            }
        }
    };

    class shared_collection_observer_dispatcher_base {
    protected :
        using add_message    = netcom_base::message_t<message::shared_collection_add>;
        using remove_message = netcom_base::message_t<message::shared_collection_remove>;

        netcom_base& net_;

    public :
        shared_collection_observer_dispatcher_base(netcom_base& net, shared_collection_id_t id) :
            net_(net), id(id) {}

        virtual ~shared_collection_observer_dispatcher_base() = default;

        const shared_collection_id_t id;

        virtual void add_item(const add_message& msg) = 0;
        virtual void remove_item(const remove_message& msg) = 0;
    };

    template<typename ElementTraits>
    class shared_collection_observer_dispatcher : public shared_collection_observer_dispatcher_base {
        using proxy = shared_collection_traits_proxy<ElementTraits>;

        /// Message sent when a new object is added
        using add_collection_element_packet = typename proxy::add_packet;
        /// Message sent when an object is removed
        using remove_collection_element_packet = typename proxy::remove_packet;

        friend class shared_collection_observer<ElementTraits>;

        signal_t<void(const add_collection_element_packet&)>    add_signal_;
        signal_t<void(const remove_collection_element_packet&)> remove_signal_;

    public :
        shared_collection_observer_dispatcher(netcom_base& net, shared_collection_id_t id) :
            shared_collection_observer_dispatcher_base(net, id) {}

        void add_item(const add_message& msg) override {
            add_collection_element_packet add;
            msg.packet.view() >> add;
            add_signal_.dispatch(add);
        }

        void remove_item(const remove_message& msg) override {
            remove_collection_element_packet rem;
            msg.packet.view() >> rem;
            remove_signal_.dispatch(rem);
        }

        shared_collection_observer<ElementTraits> create_observer() {
            return shared_collection_observer<ElementTraits>(*this, net_);
        }
    };
}

/// Collection of objects shared over the network.
/** Note that this class does not actually hold any element in memory. It just provides the
    interface for clients to "connect" to a collection of elements, i.e. :
     - upon "connection", a client is sent the whole content of the collection
     - the client is notified each time an element is added or removed from the collection
     - the client can "disconnect" at any time from the collection to stop receiving updates

    This class only takes care of the "connection" problem. The actual data that gets sent over
    the network when the full collection is shared, or when an object is added/removed is set
    by the owner of the shared_collection. See add_item(), remove_item() and
    make_collection_packet.

    The network packets that are sent are configured through the ElementTraits template
    argument. It must define:
     - full_packet: packet sent by clients to request the full collection
     - add_packet: packet sent to clients when a new object is added
     - remove_packet: packet sent to clients when an object is removed

    This is the server side class. The complementary interface for clients is provided by
    shared_collection_observer.
**/
template<typename ElementTraits>
class shared_collection {
    using proxy = netcom_impl::shared_collection_traits_proxy<ElementTraits>;
    using register_collection_packet = typename proxy::register_packet;
    using register_collection_failed_packet = typename proxy::register_failed_packet;
    using full_collection_packet = typename proxy::full_packet;

    friend class shared_collection_factory;

    netcom_impl::shared_collection_impl<ElementTraits>* impl_ = nullptr;

    /// Construct a disconnected collection.
    /** One has to call connect() in order for this collection to be visible on the network.
        In its default state, the collection cannot accept connections of clients.
    **/
    shared_collection(netcom_impl::shared_collection_impl<ElementTraits>& impl) : impl_(&impl) {}

    void check_() const {
        if (!impl_) {
            throw shared_collection_exception::uninitialized_collection<ElementTraits>{};
        }
    }

    void destroy_() {
        if (!impl_) return;
        impl_->destroy();
        impl_ = nullptr;
    }

public :
    /// Construct an invalid collection.
    /** Collections are created by a shared_collection_factory.
    **/
    shared_collection() = default;

    shared_collection(const shared_collection&) = delete;
    shared_collection(shared_collection&& c) noexcept : impl_(c.impl_) {
        c.impl_ = nullptr;
    }

    shared_collection& operator=(const shared_collection&) = delete;
    shared_collection& operator=(shared_collection&& c) {
        destroy_();
        std::swap(impl_, c.impl_);
        return *this;
    }

    ~shared_collection() {
        destroy_();
    }

    /// Make this collection visible on the network.
    /** The collection can now accept connections of clients.
        If the collection was already connected, it is first disconnected then re-connected,
        thus all clients are lost.
    **/
    void connect() {
        check_();
        impl_->connect();
    }

    /// Remove this collection from the network.
    /**
    **/
    void disconnect() {
        if (!impl_) return;
        impl_->disconnect();
    }

    bool is_connected() const {
        if (!impl_) return false;
        return impl_->is_connected();
    }

    shared_collection_id_t id() const {
        check_();
        return impl_->id;
    }

    template<typename F>
    void make_collection_packet(F&& func) {
        static_assert(ctl::argument_count<F>::value == 1,
            "make_collection_packet function can only take one argument (collection packet)");
        using ArgType = typename std::decay<ctl::functor_argument<F>>::type;
        static_assert(std::is_same<ArgType, full_collection_packet>::value,
            "argument of make_collection_packet function must be a <trait>::full_packet");

        check_();
        impl_->make_collection_packet = std::forward<F>(func);
    }

private :
    template<typename F>
    void register_client_(F&& func, std::integral_constant<std::size_t,2>) {
        using ArgTypes = ctl::functor_arguments<F>;
        using Arg1 = ctl::type_list_element<0,ArgTypes>;
        using Arg2 = ctl::type_list_element<1,ArgTypes>;
        static_assert(std::is_same<Arg1, register_collection_packet>::value,
            "first argument of register_client function must be a <trait>::register_packet");
        static_assert(std::is_same<Arg2, register_collection_failed_packet>::value,
            "second argument of register_client function must be a <trait>::register_failed_packet");

        impl_->register_client = std::forward<F>(func);
    }

    template<typename F>
    void register_client_(F&& func, std::integral_constant<std::size_t,1>) {
        using ArgType = typename std::decay<ctl::functor_argument<F>>::type;
        static_assert(std::is_empty<register_collection_failed_packet>::value,
            "register_client function is missing failure packet in its signature (the failure "
            "packet is not empty and thus cannot be ignored)");
        static_assert(std::is_same<ArgType, register_collection_packet>::value,
            "first argument of register_client function must be a <trait>::register_packet");

        impl_->register_client = [func](const register_collection_packet& reg,
            register_collection_failed_packet& failure) {
            return func(reg);
        };
    }

public :
    template<typename F>
    void register_client(F&& func) {
        static_assert(ctl::argument_count<F>::value == 1 || ctl::argument_count<F>::value == 2,
            "register_client function can only take one or two arguments (input request packet "
            "and an optional output failure packet");

        check_();
        register_client_(std::forward<F>(func), ctl::argument_count<F>{});
    }

    template<typename ... Args>
    void add_item(Args&& ... args) const {
        check_();
        impl_->add_item(std::forward<Args>(args)...);
    }

    template<typename ... Args>
    void remove_item(Args&& ... args) const {
        check_();
        impl_->remove_item(std::forward<Args>(args)...);
    }
};

template<typename ElementTraits>
class shared_collection_observer {
    using proxy = netcom_impl::shared_collection_traits_proxy<ElementTraits>;
    using register_collection_packet = typename proxy::register_packet;
    using register_collection_failed_packet = typename proxy::register_failed_packet;
    using full_collection_packet = typename proxy::full_packet;
    using add_collection_element_packet = typename proxy::add_packet;
    using remove_collection_element_packet = typename proxy::remove_packet;

    using observe_answer = netcom_base::request_answer_t<request::observe_shared_collection>;

    using dispatcher_t = netcom_impl::shared_collection_observer_dispatcher<ElementTraits>;

    friend dispatcher_t;

    dispatcher_t* dispatcher_ = nullptr;
    netcom_base* net_ = nullptr;

    scoped_connection_pool pool_;

    actor_id_t aid_;
    bool connected_ = false;

    explicit shared_collection_observer(dispatcher_t& d, netcom_base& net) :
        dispatcher_(&d), net_(&net) {}

public :
    shared_collection_observer() = default;

    shared_collection_observer(const shared_collection_observer&) = delete;
    shared_collection_observer(shared_collection_observer&& o) :
        dispatcher_(o.dispatcher_), net_(o.net_), pool_(std::move(o.pool_)), aid_(o.aid_),
        connected_(o.connected_) {
        o.dispatcher_ = nullptr;
        o.net_ = nullptr;
        o.connected_ = false;
    }

    shared_collection_observer& operator=(const shared_collection_observer&) = delete;
    shared_collection_observer& operator=(shared_collection_observer&& o) {
        disconnect();
        dispatcher_ = o.dispatcher_; o.dispatcher_ = nullptr;
        net_ = o.net_; o.net_ = nullptr;
        pool_ = std::move(o.pool_);
        aid_  = o.aid_;
        connected_ = o.connected_; o.connected_ = false;
        return *this;
    }

    ~shared_collection_observer() noexcept {
        disconnect();
    }

    /// Triggered when the full collection is received.
    signal_t<void(const full_collection_packet&)> on_received;

    /// Triggered if registration to the collection failed.
    signal_t<void(const register_collection_failed_packet&)> on_register_fail;

    /// Triggered if the collection does not exist.
    signal_t<void()> on_register_unhandled;

    /// Triggered when a new item is added to the collection.
    signal_t<void(const add_collection_element_packet&)> on_add_item;

    /// Triggered when an item is removed from the collection.
    signal_t<void(const remove_collection_element_packet&)> on_remove_item;

    template<typename T = register_collection_packet>
    void connect(actor_id_t aid, T&& arg = register_collection_packet()) {
        using ArgType = typename std::decay<T>::type;
        static_assert(std::is_same<ArgType, register_collection_packet>::value,
            "argument of connect() must be a <trait>::register_packet");

        if (!net_ || !dispatcher_) {
            throw shared_collection_exception::uninitialized_observer<ElementTraits>{};
        }

        disconnect();

        aid_ = aid;

        serialized_packet p;
        p << make_packet<request::observe_shared_collection>(id());
        p << std::forward<T>(arg);
        pool_ << net_->send_custom_request<request::observe_shared_collection>(
            aid_, std::move(p),
            [this](const observe_answer& msg) {
                if (msg.failed) {
                    if (msg.unhandled) {
                        on_register_unhandled.dispatch();
                    } else {
                        register_collection_failed_packet fail;
                        msg.packet.view() >> fail;
                        on_register_fail.dispatch(fail);
                    }
                } else {
                    full_collection_packet ans;
                    msg.packet.view() >> ans;

                    pool_ << dispatcher_->add_signal_.connect(
                        [this](const add_collection_element_packet& p) {
                            on_add_item.dispatch(p);
                        }
                    );

                    pool_ << dispatcher_->remove_signal_.connect(
                        [this](const remove_collection_element_packet& p) {
                            on_remove_item.dispatch(p);
                        }
                    );

                    connected_ = true;

                    on_received.dispatch(ans);
                }
            }
        );
    }

    void disconnect() {
        if (!connected_) return;

        net_->send_message(aid_, make_packet<message::leave_shared_collection>(id()));
        pool_.stop_all();
        connected_ = false;
    }

    bool is_connected() const {
        return connected_;
    }

    shared_collection_id_t id() const {
        if (!dispatcher_) return 0;
        return dispatcher_->id;
    }
};

class shared_collection_factory {
    friend class netcom_impl::shared_collection_base;

    netcom_base& net_;

    using collection_container = ctl::sorted_vector<
        std::unique_ptr<netcom_impl::shared_collection_base>,
        mem_var_comp(&netcom_impl::shared_collection_base::id)
    >;
    collection_container collections_;
    ctl::unique_id_provider<shared_collection_id_t> id_provider_;

    using observer_container = ctl::sorted_vector<
        std::unique_ptr<netcom_impl::shared_collection_observer_dispatcher_base>,
        mem_var_comp(&netcom_impl::shared_collection_observer_dispatcher_base::id)
    >;
    observer_container observers_;

    scoped_connection_pool pool_;

    void destroy_(shared_collection_id_t id);

public :
    explicit shared_collection_factory(netcom_base& net);

    template<typename T>
    shared_collection<T> make_shared_collection(const std::string& name) {
        using collection_t = netcom_impl::shared_collection_impl<T>;

        shared_collection_id_t id;
        if (!id_provider_.make_id(id)) {
            throw shared_collection_exception::too_many_collections{};
        }

        auto p = std::make_unique<collection_t>(*this, net_, name, id);
        collection_t* cptr = p.get();
        collections_.insert(std::move(p));

        return shared_collection<T>(*cptr);
    }

    template<typename T>
    shared_collection_observer<T> make_shared_collection_observer(shared_collection_id_t id) {
        using dispatcher_t = netcom_impl::shared_collection_observer_dispatcher<T>;

        dispatcher_t* dptr = nullptr;
        auto iter = observers_.find(id);
        if (iter == observers_.end()) {
            auto p = std::make_unique<dispatcher_t>(net_, id);
            dptr = p.get();
            observers_.insert(std::move(p));
        } else {
            dptr = dynamic_cast<dispatcher_t*>(iter->get());
            if (!dptr) {
                throw shared_collection_exception::wrong_collection_type<T>{};
            }
        }

        return dptr->create_observer();
    }

    void clear();
};

#endif
