#ifndef SERVER_NETCOM_HPP
#define SERVER_NETCOM_HPP

#include <netcom_base.hpp>
#include <SFML/Network.hpp>
#include <connection_handler.hpp>
#include <unique_id_provider.hpp>

namespace config {
    class state;
}

namespace server {
    class netcom : public netcom_base {
    public :
        explicit netcom(config::state& conf);

        /// Call terminate().
        ~netcom();

        /// Define the maximum number of simultaneously connected clients.
        /** If the limit is decreased and there are more connected clients than what is now allowed,
            no client will be disconnected. This will only prevent new clients to connect, until the
            new client limit is reached.
        **/
        void set_max_client(std::size_t max_client);

        /// Will return true as long as the server is listening to incoming connections.
        bool is_connected() const;

        /// Start the server, listening to the last or default port.
        void run();

        /// Start the server, listening to the given port.
        void run(std::uint16_t port);

        /// Return the IP address of a given actor.
        std::string get_actor_ip(actor_id_t cid) const;

    private :
        struct client_t {
            std::unique_ptr<sf::TcpSocket> socket;
            actor_id_t                     id;
        };

        using client_list_t = ctl::sorted_vector<client_t, mem_var_comp(&client_t::id)>;

        void terminate_() override;
        void loop_();
        void set_max_client_(std::size_t max_client);
        void remove_client_(actor_id_t cid);
        void remove_client_(client_list_t::iterator ic);

        config::state& conf_;
        scoped_connection_pool pool_;

        std::uint16_t      listen_port_;
        std::size_t        max_client_;
        sf::TcpListener    listener_;
        sf::SocketSelector selector_;

        client_list_t                       clients_;
        ctl::unique_id_provider<actor_id_t> client_id_provider_;

        std::atomic<bool> is_connected_;
        std::atomic<bool> terminate_thread_;
        sf::Thread listener_thread_;
    };
}

namespace message {
namespace server {
    namespace internal {
        NETCOM_PACKET(cannot_listen_port) {
            std::uint16_t port;
        };

        NETCOM_PACKET(start_listening_port) {
            std::uint16_t port;
        };

        NETCOM_PACKET(unknown_client) {
            actor_id_t id;
        };

        NETCOM_PACKET(client_connected) {
            actor_id_t id;
            std::string ip;
        };

        NETCOM_PACKET(client_disconnected) {
            actor_id_t id;

            enum class reason : std::uint8_t {
                connection_lost
            } rsn;
        };
    }

    NETCOM_PACKET(connection_established) {};

    NETCOM_PACKET(connection_failed) {
        enum class reason : std::uint8_t {
            cannot_authenticate,
            disconnected,
            timed_out
        } rsn;
    };

    NETCOM_PACKET(connection_denied) {
        enum class reason : std::uint8_t {
            too_many_clients,
            unexpected_packet
        } rsn;
    };

    NETCOM_PACKET(connection_granted) {
        actor_id_t id;
    };
}
}

#ifndef NO_AUTOGEN
#include "autogen/packets/server_netcom.hpp"
#endif

#endif
