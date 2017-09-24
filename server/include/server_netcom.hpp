#ifndef SERVER_NETCOM_HPP
#define SERVER_NETCOM_HPP

#include <netcom_base.hpp>
#include <SFML/Network.hpp>
#include <scoped_connection_pool.hpp>
#include <unique_id_provider.hpp>
#include <shared_collection.hpp>
#include <thread>

namespace config {
    class state;
}

namespace server {
    class netcom : public netcom_base {
    public :
        /// Constructor.
        /** Create a netcom in "stopped" state. You need to call run() to actually launch the
            connection.
        **/
        explicit netcom(config::state& conf, logger& out);

        /// Call wait_for_shutdown().
        ~netcom();

        /// Define the maximum number of simultaneously connected clients.
        /** If the limit is decreased and there are more connected clients than what is now allowed,
            no client will be disconnected. This will only prevent new clients to connect, until the
            new client limit is reached.
        **/
        void set_max_client(std::size_t max_client);

        /// Will return true as long as the netcom loop is running.
        /** This function will return true if run() has been called. It will only come back
            to false after shutdown() has been called and the netcom loop is effectively
            stopped, i.e. all connected clients are properly disconnected. This may take some
            time, and will not always happen right after shutdown() is closed.
        **/
        bool is_running() const;

        /// Will return true as long as the server is listening to incoming connections.
        bool is_connected() const;

        /// Start the server, listening to the last or default port.
        /** Try to activate the connection, and then set the netcom to the "running" state.
        **/
        void run();

        /// Start the server, listening to the given port.
        /** Try to activate the connection, and then set the netcom to the "running" state.
        **/
        void run(std::uint16_t port);

        /// Gracefully stop the server.
        /** This function switches the netcom in "shutdown" state. A signal is sent to all
            clients asking them to terminate the connection. All other packets are no longer sent
            nor received. When all clients are disconnected, or after time out, the server stops
            listening to the chosen port, the loop is stopped, and the netcom is in "stopped"
            state.
        **/
        void shutdown();

        /// Same as shutdown(), but explicitely waits for the loop to end.
        /** Called in the destructor.
        **/
        void wait_for_shutdown();

        /// Return the IP address of a given actor.
        std::string get_actor_ip(actor_id_t cid) const;

        /// Grant credentials to a given client
        void grant_credentials(actor_id_t cid, const credential_list_t& creds);

        /// Remove credentials to a given client
        void remove_credentials(actor_id_t cid, const credential_list_t& creds);

        template<typename T>
        shared_collection<T> make_shared_collection(const std::string& name) {
            return sc_factory_.make_shared_collection<T>(name);
        }

        template<typename T>
        shared_collection_observer<T> make_shared_collection_observer(shared_collection_id_t id) {
            return sc_factory_.make_shared_collection_observer<T>(id);
        }

    private :
        struct connected_client_t {
            connected_client_t(std::unique_ptr<sf::TcpSocket> s, actor_id_t i);

            std::unique_ptr<sf::TcpSocket> socket;
            actor_id_t                     id;
        };

        using connected_client_list_t = ctl::sorted_vector<connected_client_t, mem_var_comp(&connected_client_t::id)>;

        struct client_t {
            client_t(actor_id_t i, std::string ip);

            actor_id_t        id;
            std::string       ip;
            credential_list_t cred;
        };

        using client_list_t = ctl::sorted_vector<client_t, mem_var_comp(&client_t::id)>;

        void do_terminate_() override;
        void loop_();
        void set_max_client_(std::size_t max_client);
        void remove_client_(actor_id_t cid);
        void remove_client_(connected_client_list_t::iterator ic);

        struct credential_link_t {
            credential_t cred;
            ctl::sorted_vector<credential_t> links;
        };

        using credential_links_t = ctl::sorted_vector<credential_link_t,
            mem_var_comp(&credential_link_t::cred)>;

        credential_links_t credential_links_;

        void read_credential_links_(const std::string& file_name);
        bool credential_implies_(const credential_t& c1, const credential_t& c2) const;
        credential_list_t get_missing_credentials_(actor_id_t cid,
            const constant_credential_list_t& lst) const override;

        config::state& conf_;
        scoped_connection_pool pool_;

        bool              running_;
        std::atomic<bool> connected_;
        double            connection_time_out_;

        std::uint16_t      listen_port_;
        sf::TcpListener    listener_;
        sf::SocketSelector selector_;

        std::size_t                         max_client_;
        connected_client_list_t             connected_clients_;
        ctl::unique_id_provider<actor_id_t> client_id_provider_;

        client_list_t clients_;

        std::atomic<bool> shutdown_;
        double            shutdown_time_out_;
        double            shutdown_countdown_ = 0.0;
        std::thread       listener_thread_;

        shared_collection_factory sc_factory_;
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

        NETCOM_PACKET(begin_terminate) {};
        NETCOM_PACKET(do_terminate) {};
    }

    NETCOM_PACKET(connection_established) {};

    NETCOM_PACKET(connection_failed) {
        enum class reason : std::uint8_t {
            cannot_authenticate,
            disconnected,
            unreachable,
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

    NETCOM_PACKET(will_shutdown) {
        double countdown;
    };
}
}

namespace request {
namespace server {
    NETCOM_PACKET(ping) {
        struct answer {};
        struct failure {};
    };
}
}

#ifndef NO_AUTOGEN
#include "autogen/packets/server_netcom.hpp"
#endif

#endif
