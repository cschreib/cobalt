#include "server_state_configure.hpp"
#include <filesystem.hpp>
#include <time.hpp>

namespace server {
namespace state {
    configure::configure(netcom& net, logger& out) : net_(net), out_(out),
        config_(net, "server_state_configure") {

        build_generator_list_();

        pool_ << net_.watch_request(
            [this](server::netcom::request_t<request::server::configure_list_generators>&& req) {
            req.answer(available_generators_);
        });

        pool_ << net_.watch_request(
            [this](server::netcom::request_t<request::server::configure_get_current_generator>&& req) {
            req.answer(generator_);
        });

        pool_ << net_.watch_request(
            [this](server::netcom::request_t<request::server::configure_set_current_generator>&& req) {
            if (set_generator_(req.arg.gen)) {
                req.answer();
            } else {
                using failure_t = request::server::configure_set_current_generator::failure;
                req.fail(failure_t::reason::no_such_generator);
            }
        });

        pool_ << net_.watch_request(
            [this](server::netcom::request_t<request::server::configure_generate>&& req) {
            if (!generator_.empty()) {
                generate();
                req.answer();
            } else {
                req.fail(request::server::configure_generate::failure::reason::no_generator_set);
            }
        });
    }

    void configure::build_generator_list_() {
        available_generators_.clear();

        auto dirs = file::list_directories("generators/");
        for (auto&& dir : dirs) {
            if (file::library_exists("generators/"+dir+"/"+dir)) {
                // TODO: check that the needed functions are available
                available_generators_.insert(generator_info{dir});
            }
        }

        if (available_generators_.empty()) {
            out_.warning("no universe generator available, using default (empty universe)");
            available_generators_.insert(generator_info{"default"});
        }
    }

    bool configure::set_generator_(const std::string& id) {
        auto iter = available_generators_.find(id);
        if (iter == available_generators_.end()) return false;

        generator_ = id;
        config_.clear();
        config_.parse_from_file("generators/"+generator_+"/"+generator_+"_default.conf");

        return true;
    }

    void configure::generate() {
        // Serialize config
        std::ostringstream ss;
        config_.save(ss);

        if (save_dir_.empty()) {
            save_dir_ = "saves/"+today_str()+time_of_day_str()+"/";
            file::mkdir(save_dir_);
        }

        net_.send_message<message::server::configure_generating>(server::netcom::all_actor_id);

        // TODO: Get 'generate' function inside library
        // Start a new thread in which 'generate' is called on ss.str().c_str()
        // and saving in 'save_dir_'.

        net_.send_message<message::server::configure_generated>(server::netcom::all_actor_id);
    }
}
}
