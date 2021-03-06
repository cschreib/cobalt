#ifndef CLIENT_NETCOM_HPP
#define CLIENT_NETCOM_HPP

#include <netcom_base.hpp>
#include <shared_collection.hpp>
#include <thread>
#include <atomic>

namespace config {
    class state;
}

namespace client {
    class netcom : public netcom_base {
    public :
        explicit netcom(config::state& conf, logger& out);

        /// Call wait_for_shutdown().
        ~netcom();

        /// Return the public actor ID of this client.
        /** Return 'invalid_actor_id' if there is no active connection.
        **/
        actor_id_t self_id() const;

        /// Return true as long as the loop is running.
        bool is_running() const;

        /// Return true as long as there is an active connection with the server.
        bool is_connected() const;

        /// Try to connect to a server.
        /** Use is_connected() in order to see if/when the connection is successful.
            It usually will not happen immediately though. During the connection process,
            this class will emit a handful of messages that the client can watch for in
            order to know how things are going.

            First step:
             - connection_established: the server exists and is reachable, but it has
                not yet accepted the connection.
             - connection_failed: the server could not be reached or would not answer.

            Second step:
             - connection_granted: the server accepts the connection, end of story.
             - connection_denied: the server does not want to accept the connection.
             - connection_failed: the server could not be reached or would not answer.

            Note that, after the connection is successful, there is still a chance for the
            server to go offline, for the internet to go down, and plenty of other problems.
            When the server cannot be reached anymore, a connection_failed packet is sent,
            and the connection is closed.
        **/
        void run(const std::string& addr, std::uint16_t port);

        /// Disconnects from server and frees all resources.
        void shutdown();

        /// Same as shutdown(), but explicitely waits for the loop to end.
        /** Called in the destructor.
        **/
        void wait_for_shutdown();

        template<typename T>
        shared_collection<T> make_shared_collection(std::string name) {
            return sc_factory_.make_shared_collection<T>(name);
        }

        template<typename T>
        shared_collection_observer<T> make_shared_collection_observer(shared_collection_id_t id) {
            return sc_factory_.make_shared_collection_observer<T>(id);
        }

    private :
        void do_terminate_() override;
        void loop_();

        scoped_connection_pool pool_;

        std::string   address_;
        std::uint16_t port_;

        std::atomic<actor_id_t> self_id_;
        std::atomic<bool>       running_;
        std::atomic<bool>       connected_;
        std::atomic<bool>       terminate_thread_;
        std::thread             listener_thread_;

        shared_collection_factory sc_factory_;
    };
}

#endif
