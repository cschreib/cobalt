#ifndef CONNECTION_HANDLER_HPP
#define CONNECTION_HANDLER_HPP

#include "signal.hpp"

struct scoped_connection_t {
    scoped_connection_t() = default;
    scoped_connection_t(signal_connection_t& c) : connection_(&c) {
        set_on_stop_();
    }

    scoped_connection_t(const scoped_connection_t&) = delete;
    scoped_connection_t& operator= (const scoped_connection_t&) = delete;


    scoped_connection_t(scoped_connection_t&& sc) : connection_(sc.connection_) {
        set_on_stop_();
        sc.connection_ = nullptr;
    }

    scoped_connection_t& operator= (scoped_connection_t&& sc) {
        if (connection_) {
            connection_->on_stop = nullptr;
            connection_->stop();
        }

        connection_ = sc.connection_;
        set_on_stop_();
        sc.connection_ = nullptr;

        return *this;
    }

    ~scoped_connection_t() {
        stop();
    }

    void stop() {
        if (connection_) {
            connection_->stop();
        }
    }

    void release() {
        connection_->on_stop = nullptr;
        connection_ = nullptr;
    }

private :
    void set_on_stop_() {
        connection_->on_stop = [this](signal_connection_t&) {
            connection_ = nullptr;
        };
    }

    signal_connection_t* connection_ = nullptr;
};

struct scoped_connection_pool_t {
    scoped_connection_pool_t() = default;

    scoped_connection_pool_t(const scoped_connection_pool_t&) = delete;
    scoped_connection_pool_t& operator= (const scoped_connection_pool_t&) = delete;

    scoped_connection_pool_t(scoped_connection_pool_t&& scp) : pool_(std::move(scp.pool_)) {
        for (auto* c : pool_) {
            set_on_stop_(*c);
        }
    }

    scoped_connection_pool_t& operator= (scoped_connection_pool_t&& scp) {
        stop();

        pool_ = std::move(scp.pool_);
        for (auto* c : pool_) {
            set_on_stop_(*c);
        }

        return *this;
    }

    ~scoped_connection_pool_t() {
        stop();
    }

    scoped_connection_pool_t& operator << (signal_connection_t& c) {
        add(c);
        return *this;
    }

    void add(signal_connection_t& c) {
        set_on_stop_(c);
        pool_.push_back(&c);
    }

    void stop() {
        for (auto* c : pool_) {
            c->on_stop = nullptr;
            c->stop();
        }

        pool_.clear();
    }

    void release() {
        for (auto* c : pool_) {
            c->on_stop = nullptr;
        }

        pool_.clear();
    }

private :
    void stop_(signal_connection_t& c) {
        for (auto iter = pool_.begin(); iter != pool_.end(); ++iter) {
            if (*iter == &c) {
                pool_.erase(iter);
                break;
            }
        }
    }

    void set_on_stop_(signal_connection_t& c) {
        c.on_stop = [this](signal_connection_t& tc) {
            stop_(tc);
        };
    }

    std::vector<signal_connection_t*> pool_;
};

#endif
