#include "server_state_test.hpp"

namespace server {
namespace state {
    test::test(server::instance& serv) : base_impl(serv, "test"),
        shcfg_(net_, "test_shared") {

        shcfg_.parse_from_file("server.conf");
        shcfg_.on_value_changed.connect([&](const std::string& n, const std::string& v) {
            out_.note("value changed: ", n, " = ", v);
        });

        pool_ << net_.watch_request(
            [&](server::netcom::request_t<request::server::test_change_config>&& req) {
                shcfg_.set_value("admin.password", "gilo");

                req.answer();
            }
        );
    }
}
}
