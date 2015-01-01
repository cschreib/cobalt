#include "server_state_configure.hpp"
#include <filesystem.hpp>
#include <time.hpp>

namespace server {
namespace state {
    configure::configure(netcom& net, logger& out) : net_(net), out_(out),
        config_(net, "server_state_configure") {

        update_generator_list();

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
            if (set_generator(req.arg.gen)) {
                req.answer();
            } else {
                using failure_t = request::server::configure_set_current_generator::failure;
                req.fail(failure_t::reason::no_such_generator);
            }
        });

        pool_ << net_.watch_request(
            [this](server::netcom::request_t<request::server::configure_generate>&& req) {
            try {
                generate();
                req.answer();
            } catch (request::server::configure_generate::failure::reason rsn) {
                req.fail(rsn);
            } catch (...) {
                throw;
            }
        });
    }

    void configure::update_generator_list() {
        available_generators_.clear();

        auto libs = file::list_files("generators/*."+shared_library::file_extension);
        for (auto&& lib_file : libs) {
            shared_library lib("generators/"+lib_file);
            if (lib.open() && lib.load_symbol("generate_universe") != nullptr) {
                available_generators_.insert(generator_info{lib_file});
            }
        }

        if (available_generators_.empty()) {
            out_.warning("no universe generator available, using default (empty universe)");
            available_generators_.insert(generator_info{"default"});
        }

        generator_ = available_generators_.front().id;
    }

    bool configure::set_generator(const std::string& id) {
        auto iter = available_generators_.find(id);
        if (iter == available_generators_.end()) return false;

        generator_ = id;
        config_.clear();
        config_.parse_from_file("generators/"+generator_+"_default.conf");

        return true;
    }

    void configure::generate() {
        using failure = request::server::configure_generate::failure::reason;

        if (generating_) {
            throw failure::already_generating;
        }

        if (generator_.empty()) {
            throw failure::no_generator_set;
        }

        // Serialize config
        std::ostringstream ss;
        config_.save(ss);

        if (save_dir_.empty()) {
            save_dir_ = "saves/"+generator_+"-"+today_str()+time_of_day_str()+"/";
            file::mkdir(save_dir_);
        }

        shared_library lib("generators/"+generator_);
        if (!lib.open()) {
            throw failure::invalid_generator;
        }

        auto* fun = lib.load_function<void(const char*)>("generate_universe");
        if (!fun) {
            throw failure::invalid_generator;
        }

        net_.send_message<message::server::configure_generating>(server::netcom::all_actor_id);
        generating_ = true;

        // TODO: run that in a thread
        (*fun)(ss.str().c_str());
        generating_ = false;

        net_.send_message<message::server::configure_generated>(server::netcom::all_actor_id);
    }
}
}
