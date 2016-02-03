#ifndef SERVER_STATE_GAME_HPP
#define SERVER_STATE_GAME_HPP

#include "server_state.hpp"
#include "server_serializable.hpp"
#include "server_universe.hpp"
#include "server_player_list.hpp"
#include "ptr_vector.hpp"
#include "space.hpp"
#include <thread>

namespace server {
namespace state {
    class game : public base_impl<state_id::game> {
        scoped_connection_pool pool_;

        std::thread thread_;
        std::atomic<bool> saving_;

        ctl::ptr_vector<server::serializable> save_chunks_;

        std::unique_ptr<server::player_list> plist_;

        server::universe universe_;

    public :
        explicit game(server::instance& serv);

        void set_player_list(std::unique_ptr<server::player_list> plist);

        void save_to_directory(const std::string& dir);
        void load_from_directory(const std::string& dir);
        bool is_saved_game_directory(const std::string& dir) const;
    };
}
}

namespace request {
namespace server {
    NETCOM_PACKET(game_save) {
        NETCOM_REQUIRES("game_configurer");

        std::string save;

        struct answer {};
        struct failure {
            enum class reason {
                already_saving
            } rsn;

            std::string details;
        };
    };

    NETCOM_PACKET(game_load) {
        NETCOM_REQUIRES("game_configurer");

        std::string save;

        struct answer {};
        struct failure {
            enum class reason {
                cannot_load_while_saving,
                no_such_saved_game,
                invalid_saved_game
            } rsn;

            std::string details;
        };
    };
}
}

namespace message {
namespace server {
    NETCOM_PACKET(game_save_progress) {
        enum class step {
            gathering_game_data,
            saving_to_disk,
            game_saved
        } stp;
    };

    NETCOM_PACKET(game_load_progress) {
        std::uint16_t num_steps;
        std::uint16_t current_step;
        std::string   current_step_name;
    };
}
}

#ifndef NO_AUTOGEN
#include "autogen/packets/server_state_game.hpp"
#endif

#endif
