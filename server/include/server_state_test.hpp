#ifndef SERVER_STATE_TEST_HPP
#define SERVER_STATE_TEST_HPP

#include "server_netcom.hpp"
#include "server_state.hpp"
#include <config_shared_state.hpp>

namespace server {
namespace state {
    class test : public base_impl<state_id::test> {
        scoped_connection_pool pool_;
        config::shared_state shcfg_;

    public :
        explicit test(server::instance& serv);
    };
}
}

namespace request {
namespace server {
    NETCOM_PACKET(test_change_config) {
        struct answer {};
        struct failure {};
    };
}
}

#ifndef NO_AUTOGEN
#include "autogen/packets/server_state_test.hpp"
#endif

#endif
