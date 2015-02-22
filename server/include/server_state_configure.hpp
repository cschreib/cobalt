#ifndef SERVER_STATE_CONFIGURE_HPP
#define SERVER_STATE_CONFIGURE_HPP

#include "server_netcom.hpp"
#include "server_state.hpp"
#include "config_shared_state.hpp"
#include <thread>

namespace server {
namespace state {
    class game;

    class configure : public base_impl<state_id::configure> {
    public :
        struct generator_info {
            std::string id;
            // localized_string name, description;
            // a picture...
        };

        using generator_list_t =
            ctl::sorted_vector<generator_info, mem_var_comp(&generator_info::id)>;

    private :
        scoped_connection_pool pool_;
        scoped_connection_pool rw_pool_;

        std::thread thread_;

        generator_list_t available_generators_;
        bool generating_ = false;
        std::string generator_;
        std::string save_dir_;

        config::shared_state config_;

        ctl::sorted_vector<std::string> saved_games_;
        std::unique_ptr<state::game> loaded_game_;
        bool loading_ = false;

    public :
        explicit configure(server::instance& serv);
        ~configure();

        void update_generator_list();
        bool set_generator(const std::string& id);
        void generate();

        void update_saved_game_list();
        void load_saved_game(const std::string& dir, bool just_generated);

        void run_game();
    };

    template<typename O>
    O& operator << (O& out, const configure::generator_info& i) {
        return out << i.id;
    }

    template<typename I>
    I& operator >> (I& in, configure::generator_info& i) {
        return in >> i.id;
    }
}
}

namespace request {
namespace server {
    NETCOM_PACKET(configure_list_generators) {
        struct answer {
            ::server::state::configure::generator_list_t gens;
        };
        struct failure {};
    };

    NETCOM_PACKET(configure_get_current_generator) {
        struct answer {
            std::string gen;
        };
        struct failure {};
    };

    NETCOM_PACKET(configure_set_current_generator) {
        NETCOM_REQUIRES("game_configurer");

        std::string gen;

        struct answer {};
        struct failure {
            enum class reason {
                no_such_generator
            } rsn;
        };
    };

    NETCOM_PACKET(configure_generate) {
        NETCOM_REQUIRES("game_configurer");

        struct answer {};
        struct failure {
            enum class reason {
                no_generator_set,
                invalid_generator,
                already_generating,
                cannot_generate_while_loading
            } rsn;

            std::string details;
        };
    };

    NETCOM_PACKET(configure_load_game) {
        NETCOM_REQUIRES("game_configurer");

        std::string save;

        struct answer {};
        struct failure {
            enum class reason {
                no_such_saved_game,
                invalid_saved_game,
                already_loading,
                cannot_load_while_generating
            } rsn;

            std::string details;
        };
    };

    NETCOM_PACKET(configure_list_saved_games) {
        struct answer {
            ctl::sorted_vector<std::string> saves;
        };
        struct failure {};
    };

    NETCOM_PACKET(configure_is_game_loaded) {
        struct answer {
            bool loaded;
        };
        struct failure {};
    };

    NETCOM_PACKET(configure_run_game) {
        NETCOM_REQUIRES("game_configurer");

        struct answer {};
        struct failure {
            enum class reason {
                cannot_run_while_generating,
                cannot_run_while_loading,
                no_game_loaded
            } rsn;

            std::string details;
        };
    };
}
}

namespace message {
namespace server {
    NETCOM_PACKET(configure_generating) {};

    NETCOM_PACKET(configure_generated_internal) {
        // TODO: Send statistics about generated data?
        bool failed;
        std::string reason;
    };

    NETCOM_PACKET(configure_generated) {
        // TODO: Send statistics about generated data?
        bool failed;
        std::string reason;
    };

    NETCOM_PACKET(configure_loading) {};

    NETCOM_PACKET(configure_loaded_internal) {
        bool failed;
        std::string reason;
    };

    NETCOM_PACKET(configure_loaded) {
        bool failed;
        std::string reason;
    };
}
}

#ifndef NO_AUTOGEN
#include "autogen/packets/server_state_configure.hpp"
#endif

#endif
