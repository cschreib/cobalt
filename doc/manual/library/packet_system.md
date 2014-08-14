# Table of content

- [The interface](#the-interface)
    - [Forewords](#forewords)
    - [The `netcom_base` class](#the-netcom_base-class)
    - [Messages](#messages)
    - [Requests](#requests)
- [The implementation](#the-implementation)
    - [The `netcom_base` class](#the-netcom_base-class-1)
    - [Packets](#packets)
    - [Messages](#messages-1)
    - [Requests](#requests-1)

# The interface

## Forewords

For network communication, _cobalt_ uses the TCP protocol and a packet based system. The interface is client/server agnostic, i.e. it does not rely on the existence of a "server" and a "client", and more generically considers the interactions between different _actors_, each identified by its _actor ID_. However, for the sake of clarity and because _cobalt_ uses a client/server architecture, we will most of the time give examples with two actors named "client" and "server" in what follows.

## The `netcom_base` class

To communicate over the network, all actors use the `netcom_base` class. This is an abstract class that only provides the communication interface: the actual connection to the network and sending of data is handled by a implementation class derived from `netcom_base`, and which may be specific to the actor. For example, the implementation is different between the server and the clients: the server accepts multiple incoming connections from different clients, while clients are simply connected to the server and will therefore only communicate with it directly. In the examples that follow, we will consider that each actor has its own instance of `netcom_base`, and we will refer to it as the `net` object.

## Messages

The low level interface `netcom_base::send(out_packet&&)` allows sending arbitrary data through the network using packets (with the SFML implementation [sf::Packet](http://sfml-dev.org/documentation/2.0/classsf_1_1Packet.php) for serialization). An output packet (of type `netcom_base::out_packet`) contains the ID of the actor that the packet is to be sent to, along with the actual packet data. Here is an example:

```c++
// Create a new packet to be sent to the server
netcom_base::out_packet out(netcom_base::server_actor_id);
// The data to send
std::int32_t i = 12; std::string s = "hello";
// Serialize the data inside the packet
out << i << s;
// Send the packet
net.send(std::move(out));
```

You may refer to the [SFML documentation](http://sfml-dev.org/documentation/2.0/classsf_1_1Packet.php) for a more thorough documentation of the SFML packet class. Note in particular the use of _fixed size integers_ (`std::int32_t`), that are interpreted the same way regardless of the host CPU architecture. This is mandatory to preserve the meaning of a packet across multiple platforms: while most systems have `sizeof(int) == 4`, this is not guaranteed by the C++ standard, but `sizeof(std::int32_t) == 4` is always true.

However, when a packet is sent this way from one actor to another, the recipient needs to know how to interpret the received data: is this a chat message, the username and password of a client, or something else?

To disentangle between all the possibilities, we need to append at the beginning of each packet an integer that uniquely identifies the packet type. This number is called the _packet ID_. For example, all packets containing a message to be displayed in the chat would have ID `0`, while all packets containing a client's credentials would have ID `1`, and so on and so forth. Therefore, when the server receives a packet from a client, it first reads the packet ID from the received packet, and it can then forward the handling of this packet to the corresponding function.

Let's consider a simple example. A client wants to send a packet to the server to display a simple text message in the chat. We will call this packet `send_chat_message`. In practice, one first needs to define the packet type. It is mandatory to put it inside the `message` namespace (it is allowed to used nested namespaces as well, e.g. `message::chat` if you wish). This is done with the macro `NETCOM_PACKET`:

```c++
namespace message {
    NETCOM_PACKET(send_chat_message) {
        std::uint8_t channel; // the chat channel to send this message to
        std::string text;     // the text to display in the chat
    };
}
```

This packet definition needs to be the same for both the client and the server, and so it should ideally be placed inside a header that is included on both sides. Then, to create and send the packet, the client uses the `netcom_base::send_message(actor_id_t, MessageType&&)` function:

```c++
net.send_message(
    // Recipient
    netcom_base::server_actor_id,
    // Message data
    make_packet<message::send_chat_message>(3, "hello world!!!")
);
```

Here we are sending the text "hello world!!!" on the 3rd chat channel. On the other side, the server has to react when it receives such a packet. To do so, we register a handling function using the `netcom_base::watch_message(F&&)`:

```c++
net.watch_message(
    // Function to call when receiving the message
    [this](const message::send_chat_message& msg) {
        // Find the corresponding channel
        channel& c = this->get_chat_channel(msg.channel);
        // Print the text
        c.print(msg.text);
    }
);
```

The above code is fine, but may introduce some bugs if, for some reason, the object pointed by `this` is destroyed. In this case, and if the `netcom_base` class receives a new packet, it will try to invoke the above function, and the program will crash because `this` does not exist anymore. To prevent this, the `watch_message` function returns a _connection object_ (see the signal/slot manual), that can be stored inside a `scoped_connection_pool`:

```c++
class channel_manager {
    scoped_connection_pool pool;

    channel& get_chat_channel(std::uint8_t id);

public :
    channel_manager(netcom_base& net) {
        pool << net.watch_message([this](const message::send_chat_message& msg) {
            channel& c = this->get_chat_channel(msg.channel);
            c.print(msg.text);
        });
    }
};
```

On some occasions, it may be useful to know which actor the message came from. In our example, we may want to append the username of the client to the chat message. This is done by modifying the argument of the handling lambda function and wrapping the packet_type inside a `netcom_base::message_t<T>` that gives you access to the underlying `netcom_base::in_packet_t`:

```c++
pool << net.watch_message([this](const netcom_base::message_t<message::send_chat_message>& msg) {
    // Find the corresponding channel
    channel& c = this->get_chat_channel(msg.arg.channel);
    // Get the username of the sender
    std::string name = this->get_client_username(msg.packet.from);
    // Print the text
    c.print(name+"> "+msg.arg.text);
});
```

With this alternative syntax, the packet data is available in `msg.arg`. Not only does this allow you to get the actor ID of the sender (`msg.packet.from`), but it allows you to perform additional read operations on the received packet (`msg.packet`). This is useful if you happen to need a packet that has no fixed structure, and that has to be read dynamically. Because it is less efficient and more error prone, this should be avoided as much as possible, but there are some cases where it might be the best available solution.

Lastly, there are different _watch policies_. The default is `watch_policy::none`, and it implies that the registered handling function will be called as long as the connection object is alive (i.e. in our example as long as the `channel_manager` is alive). Sometimes this is not a desirable behavior. Let us consider another example where the client just connected to the server, and awaits the greeting message:

```c++
namespace message {
    namespace server {
        NETCOM_PACKET(greetings) {
            std::string text;
        };
    }
}
```

The client only wants to receive this message once, and discard any subsequent packets of the same type. To do so, the client can use `watch_policy::once` when registering the handling function:

```c++
net.watch_message<watch_policy::once>([this](const message::server::greetings& msg) {
    console.print("Greetings from server:");
    console.print(msg.text);
});
```

This way, the function will be called at most once, and then it will be removed from the packet watching system automatically.

This is the simplest form of communication that is exposed through the _cobalt_ interface: one actor sending some information to another. Packets sent this way are called _messages_. When an actor sends a message to another, it does not care whether the other actor has properly received it or not, nor does it expect any answer: the packet just carries an information, and it's totally up to the receiver to decide what to do with it.

Therefore, it is possible for the receiver to register multiple functions to call when receiving a given message. They will just be called sequentially.

## Requests

Although all other forms of communication can be decomposed as a sequence of such messages, it can be more convenient and more efficient to implement them natively using other protocols. In particular, the second most basic communication element is the _request_: one actor asks a question, or a favor, to another actor. For example, a client may ask the server to send him the list of connected players, or to add him to the list of administrators. But not every actor has the right to request anything: each request optionally comes with a list of required _credentials_, that senders have to possess in order to issue them.

When a request is issued, the sender expects to receive some other packet in return, that we call here the _return packet_. In the _cobalt_ interface, this packet can be of four different types:

1. _Unhandled request_: the receiver does not know how to answer this request, i.e. no handling code is currently registered. For example, this may happen if a client asks the server to move a ship after the game is over.
2. _Insufficient credentials_: the sender does not have the right to issue this request. For example, a client asks the server to shutdown, but does not have administrator rights.
3. _Failed request_: the receiver could not issue this request because of some other reason. For example, a client asks the server to move a ship to an invalid position.
4. _The actual answer_.

As for messages, requests are identified using a unique packet ID. However, when the return packet is received, the sender may have already issued several other requests, so there must be a way to unambiguously match the return packet to the corresponding handling code. To do so, the sender assigns a _request ID_ to his original request, that the receiver will also attach to the return packet. This ID is unique from the point of view of the sender only, and it completely orthogonal to the packet ID. For example, if the sender sends two requests to move two ships, the packet ID will be the same, but each will have a different request ID. A consequence of this system is that the receiver may only register _one_ function to handle all given requests of the same type, else the sender would receive multiple answers without knowing which is the right one.

Again, let us consider an example where a client wants the server to send him the list of ships that are located at a given position.

Declaring a request packet is similar to a message packet, the only differences being that requests may have credential requirements, and that they also need to define the packet structure of the possible answers. In our example, we will call the request packet `send_object_list_at_position`. Here also, it is mandatory to put the packet declaration inside the `request` namespace, and the declaration is made using the `NETCOM_PACKET` macro:

```c++
// Suppose we have a ship class
struct ship {
    std::uint32_t id;
    std::string name;
    std::string model;
    // Other data ...
};

// Serialization and de-serialization operators
packet_t::base& operator << (packet_t::base& p, const ship& s) {
    p << id << name << model;
    // Other data ...;
    return p;
}
packet_t::base& operator >> (packet_t::base& p, ship& s) {
    p >> id >> name >> model;
    // Other data ...;
    return p;
}

// The packet itself
namespace request {
    NETCOM_PACKET(send_object_list_at_position) {
        // First list the request information
        // Here, simply the position at which to look
        std::int32_t x, y;

        // The answer packet (has to be called 'answer')
        struct answer {
            std::vector<ship> list;
        };

        // The failure packet (has to be called 'failure')
        struct failure {
            enum class reason {
                invalid_position,
                // ...
            } rsn;
        };
    };
}
```

From the client point of view, the request is then sent using the `netcom_base::send_request(actor_id_t, R&&, FR&&)`:

```c++
pool << net.send_request(
    // Recipient
    netcom_base::server_actor_id,
    // The request parameters
    make_packet<request::send_object_list_at_position>(10, -50),
    // The function to call when receiving the answer
    [this](const netcom_base::request_answer_t<request::send_object_list_at_position>& req) {
        // First check if the request has been successfully answered
        if (req.failed) {
            if (req.unhandled) {
                error("server does not know how to build the list right now");
            } else {
                error("server cannot build the list");
                if (req.failure.rsn == request::send_object_list_at_position::failure::reason::invalid_position) {
                    reason("the provided position is invalid");
                }
            }
        } else {
            // We have the data we requested, do whatever we want with it...
            // Here we just print the names of the ships
            for (const ship& s : req.answer.list) {
                print(s.name);
            }

            // ... and forward the list to the renderer
            this->renderer().draw(req.answer.list);
        }
    }
);
```

Here we may have the same problem as when receiving messages, that the `this` pointer may not point to valid memory when the answer is received. Again, this is solved by returning a connection object that can be stored inside a `scoped_connection_pool` (see the previous section).

The server must then register the corresponding handling code using `netcom_base::watch_request`:

```c++
pool << net.watch_request(
    // Function to call when receiving the request
    [this](netcom_base::request_t<request::send_object_list_at_position>&& req) {
        if (req.arg.x < this->x_min || req.arg.x >= this->x_max ||
            req.arg.y < this->y_min || req.arg.y >= this->y_max) {
            // The position is outside of the allowed region
            // Send a failed request packet:
            req.fail(request::send_object_list_at_position::failure::reason::invalid_position);
        } else {
            // Else compute the ship list
            std::vector<ship> lst = this->list_object_at_position(req.arg.x, req.arg.y);
            // And send it
            req.answer(lst);
        }
    }
);
```

In this case, the request did not declare any necessary credentials, so any client can issue it and, if the server can, it will answer. Now we will consider another example of a request with required credentials: a client requesting the server to shutdown, and this is only allowed for administrators.

Credentials are specified as simple strings. In this case, the _administrator_ credential is called `"admin"`. The only thing to do is to use the `NETCOM_REQUIRES` macro to list the required credentials:

```c++
namespace request {
    namespace server {
        NETCOM_PACKET(shutdown) {
            // List credentials here
            NETCOM_REQUIRES("admin");

            struct answer {};
            struct failure {};
        };
    }
}
```

On the client side, the code is very similar to a request with no credentials. The only thing is that it is also possible to check if any credential was missing:

```c++
pool << net.send_request(server_actor_id, request::server::shutdown{},
    [&](const netcom_base::request_answer_t<request::server::shutdown>& ans) {
        if (ans.failed) {
            // If the request failed, it may be because of missing credentials
            if (!ans.missing_credentials.empty()) {
                error("not enough credentials to shutdown the server");
                note("missing:");
                for (auto& c : msg.missing_credentials) {
                    note(" - ", c);
                }
            } else {
                error("server could not shutdown");
            }
        } else {
            note("server will shutdown!");
        }
    }
);
```

The code to check the credentials is hidden in the implementation of `netcom_base`, so on the server side there is nothing special to do:

```c++
pool << net.watch_request([this](netcom_base::request_t<request::server::shutdown>&& req) {
    this->shutdown = true;
    req.answer();
});
```

As for messages, it is possible to specify different watch policies when calling `netcom_base::watch_request()`.

# The implementation

In this chapter we will see how the interface described above is implemented inside the `netcom_base` class. We will follow the path of sent and received packets, and see which helper classes and functions are involved in each case.

## The `netcom_base` class

As stated in the previous chapter, the actual implementation of the network communication is delegated to implementation classes that derive from `netcom_base`. Therefore, the `netcom_base` class itself only contains generic structures.

The most important one is the pair of output and input packet queues, `netcom_base::output_` and `netcom_base::input_`.

The output queue is filled by the `netcom_base` class itself when sending messages, requests or request answers using the `netcom_base::send()` function, and it is consumed by the implementation class to send the packets over the network.

On the other hand, the input queue is consumed by the `netcom_base` class inside the `netcom_base::process_packets()` function, and is filled by the implementation whenever a packet is received from the network.

Because both queues are declared as `ctl::lock_free_queue<>`, they can be manipulated in a thread safe way. Therefore, all the "true" network communication (i.e. physical sending and receiving of packets) can be done in a separate thread within the implementation class, in order not to block the whole program if the internet is slow.

## Packets

Let us go back to the example of the previous chapter, of a client sending a message to be displayed in the chat. The packet structure was:

```c++
namespace message {
    NETCOM_PACKET(send_chat_message) {
        std::uint8_t channel; // the chat channel to send this message to
        std::string text;     // the text to display in the chat
    };
}
```

Before going further, let us see what the `NETCOM_PACKET` macro actually does:

```c++
namespace packet_impl {
    struct base_ {};

    template<packet_id_t ID>
    struct base : base_ {
        static const packet_id_t packet_id__ = ID;
        static const char* packet_name__;
    };

    template<packet_id_t ID>
    const packet_id_t base<ID>::packet_id__;

    template<typename T>
    using is_packet = std::is_base_of<base_, T>;
}

#define NETCOM_PACKET(name) struct name : packet_impl::base<#name ## _crc32>
```

It declares a new structure whose name is provided in argument, and specifies a base class of type `packet_impl::base<>`. The template argument is the CRC32 integer associated to the name of the packet (`"send_chat_message"_crc32 == 3013527476`). This is a good way to get a unique identifier for this packet type, and it is thus chosen as the corresponding packet ID. Note that the CRC32 algorithm can generate collision, i.e. different packet names that yield the same CRC32 integer. This is rare, but it can happen.

So all the macro does is to:

1. generate a unique ID for this packet type,
2. assign a base class to the packet type that will define the `base::packet_id__` static member, allowing to get an integer identifier for this packet type,
3. define the static member `base::packet_name__` that is filled automatically (using the `refgen` tool, see below) and only used for debugging purposes,
4. give a common public base to all packets, for the sole purpose of the `packet_impl::is_packet<>` trait. This is used to check that packets are properly declared using the `NETCOM_PACKET` macro.

The point of this library is to get rid of most redundancies, by generating code automatically as much as possible. In this case, we do not ask the library user to define the packet ID by himself. But this is not the only thing we want. In particular, we want to serialize and deserialize these packets. The code to do so is simple, and was already illustrated in the previous chapter when serializing a `ship` structure:

```c++
packet_t::base& operator << (packet_t::base& p, const message::send_chat_message& m) {
    return p << m.channel << m.text;
}
packet_t::base& operator >> (packet_t::base& p, message::send_chat_message& m) {
    return p >> m.channel >> m.text;
}
```

But it is tedious to write. When C++ will get native reflection, it will be possible to generate such functions automatically using template metaprogramming. For now, we must rely on an external tool that will parse the C++ code and generate the necessary functions in a second pass. This tool is called `refgen`, and it is automatically invoked by the Makefile. More details about this tool is given in the corresponding manual.

The code to send the packet was:

```c++
net.send_message(
    // Recipient
    netcom_base::server_actor_id,
    // Message data
    make_packet<message::send_chat_message>(3, "hello world!!!")
);
```

First, we make use of the `make_packet` helper function that simplifies the creation of packets. This is yet another feature provided by the `refgen` tool. In an ideal world, we would be able to write `message::send_chat_message{3, "hello world!!!"}`. But this syntax is only available for _aggregate_ types, and with the current definition of the C++ standard, our packets are not aggregates because they have a base class. The fact that, in our case, the base class has no non static data member and no virtual function does not matter. This is inconvenient, and was [reported](https://groups.google.com/a/isocpp.org/forum/#!topic/std-proposals/77IY0cAlYR8) in the C++ standardization mailing list. It may be fixed in the C++17 standard if the corresponding proposal gets accepted.

Back to plain old C++11, the `make_packet` function is a shortcut:

```c++
template<typename T, typename ... Args>
T make_packet(Args&& ... args) {
    return packet_impl::packet_builder<T>()(std::forward<Args>(args)...);
}
```

The `packet_impl::packet_builder<>` class is generated automatically by the `refgen` tool, and its body is the following:

```c++
namespace packet_impl {
    template<> struct packet_builder<message::send_chat_message> {
        template<typename T0, typename T1>
        message::send_chat_message operator () (T0&& t0, T1&& t1) {
            message::send_chat_message p;
            p.channel = std::forward<T0>(t0);
            p.text = std::forward<T1>(t1);
            return p;
        }
    };
}
```

So in other word, this is simply a shortcut for:

```c++
message::send_chat_message msg;
msg.channel = 3;
msg.text = "hello world!!!";

net.send_message(
    // Recipient
    netcom_base::server_actor_id,
    // Message data
    std::move(msg)
);
```

Finally, we saw in the previous section that it was possible to define credential requirements:

```c++
namespace request {
    namespace server {
        NETCOM_PACKET(shutdown) {
            // List credentials here
            NETCOM_REQUIRES("admin");

            struct answer {};
            struct failure {};
        };
    }
}
```

The `NETCOM_REQUIRES` macro is defined as follows:

```c++
using constant_credential_t = const char*;

template<typename... Args>
constexpr std::array<constant_credential_t, sizeof...(Args)> make_credential_array(Args&&... args) {
     return std::array<constant_credential_t, sizeof...(Args)>{{ std::forward<Args>(args)... }};
}

#define NETCOM_REQUIRES(...) \
    static constexpr auto credentials = make_credential_array(__VA_ARGS__)
```

In other words, it simply stores the provided list of strings inside a `static constexpr std::array<const char*,N>`. This list is then read at compile time by the `netcom_base::watch_request` function to create the code to check the sender's credentials.

Now we are ready to dig inside the implementation of `netcom_base`.

## Messages

Messages are sent using the `netcom_base::send_message` function. The code here is rather straightforward:

```c++
template<typename M>
void send_message(actor_id_t aid, M&& msg = M()) {
    // Create the corresponding packet
    out_packet_t p = create_message(std::forward<M>(msg));
    // Set the recipient
    p.to = aid;
    // Push the packet to the output queue
    send(std::move(p));
}
```

The packet is actually created inside the `netcom_base::create_message_` function:

```c++
template<typename M>
out_packet_t create_message(M&& msg) {
    return create_message_<typename std::decay<M>::type>(std::forward<M>(msg));
}

template<typename MessageType, typename ... Args>
out_packet_t create_message_(Args&& ... args) {
    // Create a new packet
    out_packet_t p;
    // Specify this is a message
    p << netcom_impl::packet_type::message;
    // Specify the packet ID of this message
    p << MessageType::packet_id__;
    // Serialize the packet data
    packet_write(p, std::forward<Args>(args)...);
    return p;
}
```

And that's about it. On the other side, the packet is (hopefully) received and pushed to the input queue. It is then processed by the `netcom_base::process_packet()` function:

```c++
void netcom_base::process_packets() {
    processing_ = true;
    auto sc = ctl::make_scoped([this]() { processing_ = false; });

    // Process newly arrived packets
    in_packet_t p;
    while (input_.pop(p)) {
        // Read packet type
        netcom_impl::packet_type t;
        p >> t;

        // Forward processing to the corresponding function
        switch (t) {
        case netcom_impl::packet_type::message :
            process_message_(std::move(p));
            break;
        case netcom_impl::packet_type::request :
            process_request_(std::move(p));
            break;
        case netcom_impl::packet_type::answer :
        case netcom_impl::packet_type::failure :
        case netcom_impl::packet_type::missing_credentials :
        case netcom_impl::packet_type::unhandled :
            process_answer_(t, std::move(p));
            break;
        }
    }

    if (call_terminate_) {
        terminate_();
    }
}
```

The packet type is read, and since it is a message, it is forwarded to the `netcom_base::process_message_()` function:

```c++
void netcom_base::process_message_(in_packet_t&& p) {
    // Read the message packet ID
    packet_id_t id;
    p >> id;

    if (debug_packets) {
        std::cout << "<" << p.from << ": " << get_packet_name(id) << std::endl;
    }

    // Try to find registered handling functions
    auto iter = message_signals_.find(id);
    if (iter == message_signals_.end() || (*iter)->empty()) {
        if (id != message::unhandled_message::packet_id__ &&
            id != message::unhandled_request::packet_id__ &&
            id != message::unhandled_request_answer::packet_id__) {
            // If none, notify the receiver that a packet has not been handled
            out_packet_t tp = create_message(make_packet<message::unhandled_message>(id));
            process_message_(std::move(tp.to_input()));
        }
    } else {
        // If some are found, forward handling of the packet to the signal
        (*iter)->dispatch(std::move(p));
    }
}
```

This function extracts the packet ID, and looks in `message_signals_` for any registered handling code. If none, an `unhandled_message` message is produced, else the packet is forwarded to the corresponding `netcom_impl::message_signal_t`:

```c++
namespace netcom_impl {
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
        signals::message<P> signal;

        message_signal_impl() : message_signal_t(P::packet_id__) {}

        void dispatch(in_packet_t&& p) override {
            message_t<P> m(std::move(p));
            signal.dispatch(m);
        }

        bool empty() const override {
            return signal.empty();
        }

        void clear() override {
            signal.clear();
        }
    };
}
```

`netcom_impl::message_signal_t` is just a type erased interface. It is implemented by `netcom_impl::message_signal_impl<P>` for each different packet type `P`. When the packet is sent to the `message_signal_t::dispatch(in_packet&&)` function, the implementation reads the packet back into the C++ packet structure. This is done by `netcom_impl::message_t<P>`:

```c++
namespace netcom_impl {
    template<typename MessageType>
    struct message_t {
        static_assert(packet_impl::is_packet<MessageType>::value,
            "template argument of message_t must be a packet");

        using packet_t = MessageType;

    private :
        template<typename P>
        friend struct message_signal_impl;

        explicit message_t(in_packet_t&& p) : packet(std::move(p)) {
            // Extract the packet back into the C++ structure
            packet >> arg;
        }

    public :
        in_packet_t packet;
        packet_t arg;
    };
}
```

This type is exposed to the public interface, as we saw in the previous chapter, therefore it has a corresponding alias inside the `netcom_base` class: `netcom_base::message_t<P>`. Then, the `message_t<P>` is sent to the actual signal (see signal/slot manual). Its type is `netcom_impl::signals::message<P>`, which is just an alias for `signal_t<void(const message_t<P>&)>`.

The last step is to actually register a handling function. We saw this was done using the `netcom_base::watch_message()` function:

```c++
pool << net.watch_message([this](const netcom_base::message_t<message::send_chat_message>& msg) {
    // Find the corresponding channel
    channel& c = this->get_chat_channel(msg.arg.channel);
    // Get the username of the sender
    std::string name = this->get_client_username(msg.packet.from);
    // Print the text
    c.print(name+"> "+msg.arg.text);
});
```

The only argument is the handling function, here given as a lambda function (it can also be provided as any other kind of functor or function pointer). The only argument of the lambda function is a `message_t<P>`, with `P` the packet type this function is supposed to handle. It is therefore possible for the `watch_message` function to guess automatically which packet type to register this function to:

```c++
template<template<typename> class WP = watch_policy::none, typename FR>
signal_connection_base& watch_message(FR&& receive_func) {
    // Check the function signature
    static_assert(ctl::argument_count<FR>::value == 1,
        "message reception handler can only take one argument");
    using ArgType = typename std::decay<ctl::functor_argument<FR>>::type;
    static_assert(netcom_impl::is_message<ArgType>::value ||
                  packet_impl::is_packet<ArgType>::value,
        "message reception handler argument must either be a packet or a message_t");

    // Proceed further
    return watch_message_<WP>(
        std::forward<FR>(receive_func), netcom_impl::is_message<ArgType>{}
    );
}
```

The first template argument, as we saw, allows choosing the _watch policy_, and defaults to `watch_policy::none`. Inside the body of the function, we start by various checks to make sure that the provided function has a suitable signature. Then we check if the argument of this function is a `message_t<P>` or a plain packet `P`, and go for different versions of `netcom_base::watch_message_()`:

```c++
// The argument is a plain packet
template<template<typename> class WP, typename FR>
signal_connection_base& watch_message_(FR&& receive_func, std::false_type) {
    using MessageType = typename std::decay<ctl::functor_argument<FR>>::type;
    using ArgType = message_t<MessageType>;

    // Find the signal corresponding to this packet type
    message_signal_impl<MessageType>& netsig = get_message_signal_<MessageType>();

    // Register this new function
    return netsig.signal.template connect<WP>([receive_func](const ArgType& msg) {
        // Create a glue function that only forwards the extracted packet to the
        // provided handler function
        receive_func(msg.arg);
    });
}

// The argument is a message_t<P>
template<template<typename> class WP, typename FR>
signal_connection_base& watch_message_(FR&& receive_func, std::true_type) {
    using ArgType = typename std::decay<ctl::functor_argument<FR>>::type;
    using MessageType = typename ArgType::packet_t;

    // Find the signal corresponding to this packet type
    message_signal_impl<MessageType>& netsig = get_message_signal_<MessageType>();

    // Register this new function
    return netsig.signal.template connect<WP>(std::forward<FR>(receive_func));
}
```

In our case, the chosen overload is the second one. We first look (or create) the signal corresponding to the reception of such packet type, and then bind the provided function directly to the signal, and that's it. If we had provided a lambda function whose argument was simply the packet type, we would have gone for the first overload. This overload is doing exactly the same thing, except that a _glue_ function is created, taking the extracted C++ packet structure from the `message_t<P>` and forwarding it to the provided function.

The code of `get_message_signal_<P>()` is rather straightforward:

```c++
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
```

It looks for the signal in the `message_signals_` container, and creates it if it does not exists.

That's all for how messages are handled. All the magic is then done by the signal/slot library.

## Requests
