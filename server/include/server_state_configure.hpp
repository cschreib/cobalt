#ifndef SERVER_STATE_CONFIGURE_HPP
#define SERVER_STATE_CONFIGURE_HPP

#include "server_netcom.hpp"
#include "server_state.hpp"
#include "config_shared_state.hpp"

namespace server {
namespace state {
    class configure : public server::state::base {
    public :
        struct generator_info {
            std::string id;
            // localized_string name, description;
            // a picture...
        };

        using generator_list_t =
            ctl::sorted_vector<generator_info, mem_var_comp(&generator_info::id)>;

    private :
        netcom& net_;
        logger& out_;

        scoped_connection_pool pool_;

        generator_list_t available_generators_;

        std::string generator_;
        std::string save_dir_;

        config::shared_state config_;

        void build_generator_list_();
        bool set_generator_(const std::string& id);

    public :
        configure(netcom& net, logger& out);

        void generate();
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
                no_generator_set
            } rsn;
        };
    };
}
}

namespace message {
namespace server {
    NETCOM_PACKET(configure_generating) {};

    NETCOM_PACKET(configure_generated) {
        // Send statistics about generated data
    };
}
}

#ifndef NO_AUTOGEN
#include "autogen/packets/server_state_configure.hpp"
#endif

#endif
