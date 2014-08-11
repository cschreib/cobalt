== Overview ==

For network communication, _cobalt_ uses the TCP protocol and a packet based system. The interface is client/server agnostic, i.e. it does not rely on the existence of a "server" and a "client", and more generically considers the interactions between different _actors_. However, for the sake of clarity and because _cobalt_ uses a client/server architecture, we will give examples with two actors named "client" and "server" in the following.

The low level interface, which is _not_ exposed, allows sending arbitrary data through the network using packets (with the SFML implementation [sf::Packet][http://sfml-dev.org/documentation/2.0/classsf_1_1Packet.php] for serialization). However, when a packet is sent from one actor to another, the recipient needs to know how to interpret the received data: is this a chat message, the username and password of a client, or something else?

To disentangle between all the possibilities, we append at the beginning of each packet an integer that uniquely identifies the packet type. This number is called the _packet ID_. For example, all packets containing a message to be displayed in the chat would have ID `0`, while all packets containing a client's credentials would have ID `1`, and so on and so forth. Therefore, when the server receives a packet from a client, it first reads the packet ID from the received packet, and it can then forward the handling of this packet to the corresponding function.

This is the simplest form of communication that is exposed through the _cobalt_ interface: one actor sending some information to another. Packets sent this way are called _messages_. When an actor sends a message to another, it does not care whether the other actor has properly received it or not, nor does it expect any answer: it just carries an information.

Although all other forms of communication can be decomposed as a sequence of such messages, it can be more convenient and more efficient to implement them natively using other protocols. In particular, the second most basic communication element is the _request_: one actor asks a question, or a favor, to another actor. For example, a client may ask the server to send him the list of connected players, or to add him to the list of administrators. But not every actor has to right to request anything: each request optionally comes with a list of required _credentials_, that senders have to acquire in order to issue them.

== The `netcom` class ==

== Messages ==

== Requests ==

<!-- When such a request is issued, the sender expects to receive some other packet in return. In the _cobalt_ interface, this returned packet can be of four different types:

1. _Unhandled request_: the receiver does not know how to answer this request, i.e. no handling code is currently registered.
2. _Insufficient credentials_: the sender does not have the right to issue this request (see below).
3. _Request failed_: the receiver could not issue this request because of some other reason (details are supposed to be provided).
4. _The actual answer_. -->
