#ifndef SCOPED_CONNECTION_POOL_HPP
#define SCOPED_CONNECTION_POOL_HPP

#include "signal.hpp"

/// Helper class to manage a pool of signal/slot connections.
struct scoped_connection_pool {
    using connection_type = signal_connection_base;

private :
    using base = std::vector<connection_type*>;

public :
    scoped_connection_pool() = default;

    scoped_connection_pool(const scoped_connection_pool&) = delete;
    scoped_connection_pool& operator= (const scoped_connection_pool&) = delete;

    scoped_connection_pool(scoped_connection_pool&& scp) : pool_(std::move(scp.pool_)) {
        for (auto* c : pool_) {
            set_on_stop_(*c);
        }
    }

    scoped_connection_pool& operator= (scoped_connection_pool&& scp) {
        stop_all();

        pool_ = std::move(scp.pool_);
        for (auto* c : pool_) {
            set_on_stop_(*c);
        }

        return *this;
    }

    /// Destructor. Calls stop_all().
    ~scoped_connection_pool() {
        stop_all();
    }

    /// Shortcut for add().
    scoped_connection_pool& operator << (connection_type& c) {
        add(c);
        return *this;
    }

    /// Store a new connection in this pool.
    void add(connection_type& c) {
        set_on_stop_(c);
        if (blocked_) {
            c.block();
        } else {
            c.unblock();
        }

        pool_.push_back(&c);
    }

    /// Merge another pool into this one.
    /** The other pool is left empty in the process.
    **/
    void merge(scoped_connection_pool&& p) {
        for (auto* c : p.pool_) {
            set_on_stop_(*c);
            if (blocked_ != p.blocked_) {
                if (blocked_) {
                    c->block();
                } else {
                    c->unblock();
                }
            }
        }

        pool_.insert(pool_.begin(), p.pool_.begin(), p.pool_.end());
        p.pool_.clear();
    }

    /// Stop all connections in this pool.
    void stop_all() {
        for (auto* c : pool_) {
            c->on_stop = nullptr;
            c->stop();
        }

        pool_.clear();
    }

    /// Block all current and future connections.
    void block_all() {
        if (blocked_) return;
        blocked_ = true;
        for (auto* c : pool_) {
            c->block();
        }
    }

    /// Check if this pool is currently blocked.
    bool blocked() const {
        return blocked_;
    }

    /// Unblock all the connections in the pool.
    void unblock_all() {
        if (!blocked_) return;
        blocked_ = false;
        for (auto* c : pool_) {
            c->unblock();
        }
    }

private :
    void stop_(connection_type& c) {
        for (auto iter = pool_.begin(); iter != pool_.end(); ++iter) {
            if (*iter == &c) {
                pool_.erase(iter);
                break;
            }
        }
    }

    void set_on_stop_(connection_type& c) {
        c.on_stop = [this](signal_connection_base& tc) {
            stop_(tc);
        };
    }

    base pool_;
    bool blocked_ = false;
};

#endif
