# Overview

## Forewords

For network communication, _cobalt_ uses the TCP protocol and a packet based system. The interface is client/server agnostic, i.e. it does not rely on the existence of a "server" and a "client", and more generically considers the interactions between different _actors_, each identified by its _actor ID_. However, for the sake of clarity and because _cobalt_ uses a client/server architecture, we will most of the time give examples with two actors named "client" and "server" in what follows.

## The interface

### The `netcom_base` class

To communicate over the network, all actors use the `netcom_base` class. This is an abstract class that only provides the communication interface: the actual connection to the network and sending of data is handled by a implementation class derived from `netcom_base`, and which may be specific to the actor. For example, the implementation is different between the server and the clients: the server accepts multiple incoming connections from different clients, while clients are simply connected to the server and will therefore only communicate with it directly. In the examples that follow, we will consider that each actor has its own instance of `netcom_base`, and we will refer to it as the `net` object.

### Messages

The low level interface `netcom_base::send(out_packet&&)` allows sending arbitrary data through the network using packets (with the SFML implementation [sf::Packet](http://sfml-dev.org/documentation/2.0/classsf_1_1Packet.php) for serialization). An output packet (of type `netcom_base::out_packet`) contains the ID of the actor that the packet is to be sent to, along with the actual packet data. Here is an example:

```c++
// Create a new packet to be sent to the server
netcom_base::out_packet out(netcom_base::server_actor_id);
// The data to send
int i = 12; std::string s = "hello";
// Serialize the data inside the packet
out << i << s;
// Send the packet
net.send(std::move(out));
```

You may refer to the [SFML documentation](http://sfml-dev.org/documentation/2.0/classsf_1_1Packet.php) for a more thorough documentation of the SFML packet class. However, when a packet is sent this way from one actor to another, the recipient needs to know how to interpret the received data: is this a chat message, the username and password of a client, or something else?

To disentangle between all the possibilities, we need to append at the beginning of each packet an integer that uniquely identifies the packet type. This number is called the _packet ID_. For example, all packets containing a message to be displayed in the chat would have ID `0`, while all packets containing a client's credentials would have ID `1`, and so on and so forth. Therefore, when the server receives a packet from a client, it first reads the packet ID from the received packet, and it can then forward the handling of this packet to the corresponding function.

Let's consider a simple example. A client wants to send a packet to the server to display a simple text message in the chat. We will call this packet `send_chat_message`. In practice, one first needs to define the packet type. It is mandatory to put it inside the `message` namespace (it is allowed to used nested namespaces as well, e.g. `message::chat` if you wish). This is done with the macro `NETCOM_PACKET`:

```c++
namespace message {
    NETCOM_PACKET(send_chat_message) {
        int channel;      // the chat channel to send this message to
        std::string text; // the text to display in the chat
    };
}
```

This packet definition needs to be the same for both the client and the server, and so it should ideally be placed inside a header that is included on both sides. Then, to create and send the packet, the client uses the `netcom_base::send_message(actor_id_t, MessageType&&)` function:

```c++
net.send_message(netcom_base::server_actor_id,
    make_packet<message::send_chat_message>(3, "hello world!!!")
);
```

Here we are sending the text "hello world!!!" on the 3rd chat channel. On the other side, the server has to react when it receives such a packet. To do so, we register a handling function using the `netcom_base::watch_message(F&&)`:

```c++
net.watch_message([this](const message::send_chat_message& msg) {
    // Find the corresponding channel
    channel& c = this->get_chat_channel(msg.channel);
    // Print the text
    c.print(msg.text);
});
```

The above code is fine, but may introduce some bugs if, for some reason, the object pointed by `this` is destroyed. In this case, and if the `netcom_base` class receives a new packet, it will try to invoke the above function, and the program will crash because `this` does not exist anymore. To prevent this, the `watch_message` function returns a _connection object_ (see the signal/slot manual), that can be stored inside a `scoped_connection_pool`:

```c++
class channel_manager {
    scoped_connection_pool pool;

    channel& get_chat_channel(int id);

public :
    channel_manager(netcom_base& net) {
        pool << net.watch_message([this](const message::send_chat_message& msg) {
            // Find the corresponding channel
            channel& c = this->get_chat_channel(msg.channel);
            // Print the text
            c.print(msg.text);
        });
    }
};
```

On some occasions, it may be useful to know which actor the message came from. In our example, we may want to prepend the username of the client to the chat message. This is done by modifying the argument of the handling lambda function and wrapping the packet_type inside a `netcom_base::message_t<T>` that gives you access to the underlying `netcom_base::in_packet_t`:

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

Not only does this allow you to get the actor ID of the sender, but it allows you to perform additional read operations on the received packet. This is useful if you happen to need a packet that has no fixed structure, and that has to be read dynamically. Because it is less efficient, this should be avoided as much as possible, but there are some cases where it might be the best solution.

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

Messages are the simplest form of communication that is exposed through the _cobalt_ interface: one actor sending some information to another. Packets sent this way are called _messages_. When an actor sends a message to another, it does not care whether the other actor has properly received it or not, nor does it expect any answer: the packet just carries an information, and it's totally up to the receiver to decide what to do with it.

### Requests

Although all other forms of communication can be decomposed as a sequence of such messages, it can be more convenient and more efficient to implement them natively using other protocols. In particular, the second most basic communication element is the _request_: one actor asks a question, or a favor, to another actor. For example, a client may ask the server to send him the list of connected players, or to add him to the list of administrators. But not every actor has to right to request anything: each request optionally comes with a list of required _credentials_, that senders have to possess in order to issue them.

When a request is issued, the sender expects to receive some other packet in return, that we call here the _return packet_. In the _cobalt_ interface, this packet can be of four different types:

1. _Unhandled request_: the receiver does not know how to answer this request, i.e. no handling code is currently registered. For example, this may happen if a client asks the server to move a ship after the game is over.
2. _Insufficient credentials_: the sender does not have the right to issue this request. For example, a client asks the server to shutdown, but does not have administrator rights.
3. _Failed request_: the receiver could not issue this request because of some other reason. For example, a client asks the server to move a ship to an invalid position.
4. _The actual answer_.

However, when the return packet is received, the sender may have already issued several other requests, so there must be a way to unambiguously match the return packet to the corresponding handling code. To do so, the sender assigns a unique _request ID_ to his original request, that the receiver will also attach to the return packet. This ID is unique from the point of view of the sender only.

We will now detail how these protocols are implemented by looking directly into the code. Most of it is located inside `common-netcom/include/netcom_base.hpp`.