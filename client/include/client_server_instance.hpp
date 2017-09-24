#ifndef CLIENT_SERVER_INSTANCE_HPP
#define CLIENT_SERVER_INSTANCE_HPP

#include <atomic>
#include <thread>
#include <log.hpp>
#include "client_netcom.hpp"
#include "client_server_state.hpp"
#include <server_instance.hpp>

namespace config {
    class state;
}

namespace client {
    class server_instance {
        std::string   server_ip_   = "127.0.0.1";
        std::uint16_t server_port_ = 4444;
        std::string   admin_password_;
        bool          is_admin_ = false;

        std::atomic<bool> shutdown_;

        template<typename T, typename ... Args>
        T& set_state_(Args&& ... args) {
            return set_state_(std::make_unique<T>(*this, std::forward<Args>(args)...));
        }

        template<typename T>
        T& set_state_(std::unique_ptr<T> st) {
            T& ret = *st;
            current_state_ = std::move(st);
            on_state_changed.dispatch(st.id());
            return ret;
        }

        void set_state_(server::state_id sid);

    protected:
        config::state& conf_;
        logger& out_;

        netcom net_;
        scoped_connection_pool pool_;

        std::unique_ptr<server_state::base> current_state_;

    public :
        explicit server_instance(config::state& conf, logger& log);

        logger& get_log();
        netcom& get_netcom();
        config::state& get_conf();

        bool is_running() const;
        void run();
        void shutdown();
        bool is_admin() const;

        signal_t<void()> on_iter;

        // Signals for connection with server
        signal_t<void(std::string, std::uint16_t)> on_connecting;
        signal_t<void()> on_connected;
        signal_t<void(message::server::connection_failed::reason)> on_connection_failed;
        signal_t<void()> on_disconnecting;
        signal_t<void()> on_disconnected;
        signal_t<void()> on_unexpected_disconnected;

        // Signals for server state
        signal_t<void(server::state_id)> on_state_changed;

        // Signals for administative rights
        signal_t<void(request::server::admin_rights::failure::reason)> on_admin_rights_denied;
        signal_t<void()> on_admin_rights_granted;
    };
}

#ifndef NO_AUTOGEN
#include "autogen/packets/client_server_instance.hpp"
#endif

#endif
