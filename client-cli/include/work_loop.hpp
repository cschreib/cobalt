#ifndef WORK_LOOP_HPP
#define WORK_LOOP_HPP

#include <config.hpp>
#include <lock_free_queue.hpp>
#include <atomic>
#include <thread>
#include <client_player_list.hpp>
#include <lua.h>

class work_loop {
    client::netcom net_;
    scoped_connection_pool pool_;

    std::string prompt_ = "> ";
    std::string admin_password_;

    std::string   server_ip_   = "127.0.0.1";
    std::uint16_t server_port_ = 4444;

    client::player_list plist_;

    std::atomic<bool> shutdown_;
    std::thread worker_;

    ctl::lock_free_queue<std::string> commands_;
    lua_State* lua_ = nullptr;

    void open_lua_();
    void server_connect_();
    void run_();
    void execute_(const std::string& cmd);

public :
    explicit work_loop(config::state& conf);
    ~work_loop();

    void start();
    bool is_stopped() const;
    void stop();

    void execute(std::string cmd);
};

#endif
