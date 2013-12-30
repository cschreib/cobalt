#ifndef CONNECTION_HANDLER_HPP
#define CONNECTION_HANDLER_HPP

#include "signal.hpp"

/// Helper class to manage lifetime of signal/slot connections.
/** Semantic equivalent of a std::unique_ptr.
    If given the responsability of a connection, this class will automatically close it when it goes
    out of scope. It is useful to bind a connection to the lifetime of the object that created it.
    In case one needs to create multiple connections from the same object, all bound to its
    lifetime, then it is more convenient to use a scoped_connection_pool_t.
**/
struct scoped_connection_t {
    /// Construct an empty object.
    /** No connection is managed.
    **/
    scoped_connection_t() = default;

    /// Manage a connection.
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

    /// Calls stop().
    ~scoped_connection_t() {
        stop();
    }

    /// Stop the connection, if any.
    /** Regardless of its previous state, the object is now empty.
    **/
    void stop() {
        if (connection_) {
            connection_->stop();
            connection_ = nullptr;
        }
    }

    /// Check if this object is empty.
    bool empty() const {
        return connection_ == nullptr;
    }

    /// Release the handled connection for it to be used elsewhere.
    /** The object is left empty in the process, and the connection is not stopped.
    **/
    signal_connection_t& release() {
        connection_->on_stop = nullptr;
        signal_connection_t* c = connection_;
        connection_ = nullptr;
        return *c;
    }

private :
    void set_on_stop_() {
        connection_->on_stop = [this](signal_connection_t&) {
            connection_ = nullptr;
        };
    }

    signal_connection_t* connection_ = nullptr;
};

/// Helper class to manage a pool of signal/slot connections.
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

    /// Calls stop().
    ~scoped_connection_pool_t() {
        stop();
    }

    /// Shortcut for add().
    scoped_connection_pool_t& operator << (signal_connection_t& c) {
        add(c);
        return *this;
    }

    /// Store a new connection in this pool.
    void add(signal_connection_t& c) {
        set_on_stop_(c);
        pool_.push_back(&c);
    }

    /// Stop all connections.
    void stop() {
        for (auto* c : pool_) {
            c->on_stop = nullptr;
            c->stop();
        }

        pool_.clear();
    }

    /// Merge another pool into this one.
    /** The other pool is left empty in the process.
    **/
    void merge(scoped_connection_pool_t&& p) {
        for (auto* c : p.pool_) {
            set_on_stop_(*c);
        }

        pool_.insert(pool_.begin(), p.pool_.begin(), p.pool_.end());
        p.pool_.clear();
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
