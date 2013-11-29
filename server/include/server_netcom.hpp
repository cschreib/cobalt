#ifndef SERVER_NETCOM_HPP
#define SERVER_NETCOM_HPP

#include <netcom_base.hpp>
#include <SFML/Network.hpp>

namespace server {
    class netcom : public netcom_base {
    public :
        netcom();
        ~netcom();

        /// Define the maximum number of simultaneously connected clients.
        /** If the limit is decreased and there are more connected clients than what is now allowed,
            no client will be disconnected. This will only prevent new clients to connect, until the
            new client limit is reached.
        **/
        void set_max_client(std::size_t max_client);

        /// Will return true as long as the server is listening to incoming connections.
        bool is_connected() const;

        /// Start the server, listening to the given port.
        void run(std::uint16_t port);

        /// Shut down the server cleanly (called by the destructor).
        /** Calling this function on a server that is not running has no effect.
        **/
        void terminate();

    private :
        struct client_t {
            std::unique_ptr<sf::TcpSocket> socket;
            actor_id_t                     id;
        };

        using client_list_t = sorted_vector<client_t, mem_var_comp(&client_t::id)>;

        void loop_();
        bool make_id_(actor_id_t& id);
        void free_id_(actor_id_t id);
        void remove_client_(actor_id_t cid);
        void remove_client_(client_list_t::iterator ic);

        std::uint16_t      listen_port_;
        std::size_t        max_client_;
        sf::TcpListener    listener_;
        sf::SocketSelector selector_;

        client_list_t             clients_;
        sorted_vector<actor_id_t, std::greater<request_id_t>> available_ids_;

        std::atomic<bool> is_connected_;
        std::atomic<bool> terminate_thread_;
        sf::Thread listener_thread_;
    };
}

namespace message {
namespace server {
    namespace internal {
        ID_STRUCT(cannot_listen_port) {
            using types = type_list<std::uint16_t>;
        };

        ID_STRUCT(start_listening_port) {
            using types = type_list<std::uint16_t>;
        };

        ID_STRUCT(unknown_client) {
            using types = type_list<std::uint16_t>;
        };

        ID_STRUCT(client_connected) {
            using types = type_list<std::uint16_t, std::string>;
        };

        ID_STRUCT(client_disconnected) {
            enum class reason : std::uint8_t {
                connection_lost
            };

            using types = type_list<std::uint16_t, reason>;
        };
    }

    ID_STRUCT(connection_established) {
        using types = type_list<>;
    };

    ID_STRUCT(connection_failed) {
        enum class reason : std::uint8_t {
            cannot_authenticate,
            disconnected,
            timed_out
        };

        using types = type_list<reason>;
    };

    ID_STRUCT(connection_denied) {
        enum class reason : std::uint8_t {
            too_many_clients,
            unexpected_packet
        };

        using types = type_list<reason>;
    };

    ID_STRUCT(connection_granted) {
        using types = type_list<>;
    };
}
}

#endif
