#ifndef CLIENT_NETCOM_HPP
#define CLIENT_NETCOM_HPP

#include <netcom_base.hpp>
#include <SFML/Network.hpp>

namespace client {
    class netcom : public netcom_base {
    public :
        netcom();
        ~netcom();

        /// Will return true as long as there is an active connection with the server.
        bool is_connected() const;

        /// Try to connect to a server.
        /** Use is_connected() in order to see if/when the connection is successful.
            It usually will not happen immediately though. During the connection process,
            this class will emit a handful of messages that the client can watch for in
            order to know how things are going. First step:
             - connection_established: the server exists and is reachable, but it has
                not yet accepted the connection.
             - connection_failed: the server could not be reached or would not answer. The
                first and only object that is sent in this packet is actually a
                connection_failed_reason::reason.
            Second step:
             - connection_granted: the server accepts the connection, end of story.
             - connection_denied: the server does not want to accept the connection. There
                are several possible reasons for this. The first and only object that is
                sent in this packet is actually a connection_denied_reason::reason.
             - connection_failed: the server could not be reached or would not answer. The
                first and only object that is sent in this packet is actually a
                connection_failed_reason::reason.

            Note that, after the connection is successful, there is still a chance for the
            server to go offline, for the internet to go down, and plenty of other problems.
            When the server cannot be reached anymore, a connection_failed packet is sent,
            and the connection is closed.
        **/
        void run(const std::string& addr, std::uint16_t port);

        /// Stop the connection with the server (cleanly).
        /** Note that this function is automatically called in the destructor.
        **/
        void terminate();

    private :
        void loop_();

        std::string   address_;
        std::uint16_t port_;

        std::atomic<bool> is_connected_;
        std::atomic<bool> terminate_thread_;
        sf::Thread listener_thread_;
    };
}

#endif
