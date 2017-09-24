#ifndef WORK_LOOP_HPP
#define WORK_LOOP_HPP

#include <lock_free_queue.hpp>
#include <client_server_instance.hpp>
#include <sol.hpp>

class work_loop {
    config::state& conf_;
    logger& out_;
    scoped_connection_pool pool_;

    std::atomic<bool> running_;
    std::atomic<bool> stop_;
    std::thread worker_;
    client::server_instance* server_ = nullptr;
    bool reconnect_now_ = false;

    std::string prompt_ = "> ";
    bool auto_reconnect_ = true;
    float auto_reconnect_delay_ = 2.0f;

    ctl::lock_free_queue<std::string> commands_;
    sol::state lua_;

    void open_lua_();
    void connect_();
    void run_();
    void execute_(const std::string& cmd);

    void setup_idle_();
    void setup_configure_();
    void setup_game_();

public :
    explicit work_loop(config::state& conf, logger& log);
    ~work_loop();

    std::atomic<bool> quit;
    std::atomic<bool> restart;

    void execute(std::string cmd);

    bool is_running() const;
    void run();
    void shutdown();
    void wait_for_shutdown();
};

#endif
