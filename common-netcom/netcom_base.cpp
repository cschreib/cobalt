#include "netcom_base.hpp"
#include "endian.hpp"

netcom_base::netcom_base() {
    std::size_t n = std::numeric_limits<request_id_t>::max();
    available_request_ids_.reserve(n);
    for (std::size_t i = 1; i <= n; ++i) {
        available_request_ids_.insert(n-i);
    }

    n = std::numeric_limits<netcom_impl::watch_id_t>::max();
    available_mwatch_ids_.reserve(n);
    for (std::size_t i = 1; i <= n; ++i) {
        available_mwatch_ids_.insert(n-i);
    }
}

void netcom_base::send_(out_packet_t&& p) {
    if (p.to == invalid_actor_id) throw invalid_actor();
    if (p.to == self_actor_id) {
        input_.push(std::move(p.to_input()));
    } else {
        output_.push(std::move(p));
    }
}

bool netcom_base::request_cancelled_(request_id_t id) const {
    auto iter = requests_.find(id);
    return iter == requests_.end() || (*iter)->canceled;
}

void netcom_base::request_notify_pool_(request_id_t id, request_pool_t* pool) {
    auto iter = requests_.find(id);
    if (iter == requests_.end()) return;
    (*iter)->pool = pool;
}

void netcom_base::cancel_request_(request_id_t id) {
    auto iter = requests_.find(id);
    if (iter != requests_.end()) return;
    cancel_request_(**iter);
}

void netcom_base::cancel_request_(netcom_impl::request_t& req) {
    free_request_id_(req.id);
    req.canceled = true;
}

void netcom_base::free_request_id_(request_id_t id) {
    available_request_ids_.insert(id);
}

request_id_t netcom_base::make_request_id_() {
    if (available_request_ids_.empty()) throw too_many_requests();
    request_id_t id = available_request_ids_.back();
    available_request_ids_.pop_back();
    return id;
}

bool netcom_base::message_watch_cancelled_(packet_id_t id, netcom_impl::watch_id_t wid) {
    auto iter1 = message_watches_.find(id);
    if (iter1 == message_watches_.end()) return true;
    auto iter2 = iter1->group.find(wid);
    return iter2 == iter1->group.end() || (*iter2)->canceled;
}

void netcom_base::cancel_message_watch_(packet_id_t id, netcom_impl::watch_id_t wid) {
    auto iter1 = message_watches_.find(id);
    if (iter1 == message_watches_.end()) return;
    auto iter2 = iter1->group.find(wid);
    if (iter2 == iter1->group.end()) return;
    cancel_message_watch_(**iter2);
}

void netcom_base::cancel_message_watch_(netcom_impl::message_watch_t& watch) {
    free_message_watch_id_(watch.id);
    watch.canceled = true;
    watch.accept = false;
}

void netcom_base::hold_message_watch_(packet_id_t id, netcom_impl::watch_id_t wid) {
    auto iter1 = message_watches_.find(id);
    if (iter1 == message_watches_.end()) return;
    auto iter2 = iter1->group.find(wid);
    if (iter2 == iter1->group.end()) return;
    (*iter2)->hold();
}

void netcom_base::release_message_watch_(packet_id_t id, netcom_impl::watch_id_t wid) {
    auto iter1 = message_watches_.find(id);
    if (iter1 == message_watches_.end()) return;
    auto iter2 = iter1->group.find(wid);
    if (iter2 == iter1->group.end()) return;
    (*iter2)->release(*this);
}

void netcom_base::free_message_watch_id_(netcom_impl::watch_id_t id) {
    available_mwatch_ids_.insert(id);
}

netcom_impl::watch_id_t netcom_base::make_message_watch_id_() {
    if (available_mwatch_ids_.empty()) throw too_many_watches();
    netcom_impl::watch_id_t id = available_mwatch_ids_.back();
    available_mwatch_ids_.pop_back();
    return id;
}

bool netcom_base::request_watch_cancelled_(packet_id_t id) {
    auto iter = request_watches_.find(id);
    return iter == request_watches_.end() || (*iter)->canceled;
}

void netcom_base::cancel_request_watch_(packet_id_t id) {
    auto iter = request_watches_.find(id);
    if (iter == request_watches_.end()) return;
    cancel_request_watch_(**iter);
}

void netcom_base::cancel_request_watch_(netcom_impl::request_watch_t& watch) {
    watch.canceled = true;
    watch.accept = false;
}

void netcom_base::hold_request_watch_(packet_id_t id) {
    auto iter = request_watches_.find(id);
    if (iter == request_watches_.end()) return;
    (*iter)->hold();
}

void netcom_base::release_request_watch_(packet_id_t id) {
    auto iter = request_watches_.find(id);
    if (iter == request_watches_.end()) return;
    (*iter)->release(*this);
}

void netcom_base::register_watch_pool_(watch_pool_t* pool) {
    watch_pools_.insert(pool);
}

void netcom_base::unregister_watch_pool_(watch_pool_t* pool) {
    watch_pools_.erase(pool);
}

void netcom_base::clear_all_() {
    input_.clear();
    output_.clear();

    for (auto& r : requests_) {
        cancel_request_(*r);
    }

    requests_.clear();

    for (auto& group : message_watches_)
    for (auto& w : group.group) {
        cancel_message_watch_(*w);
    }

    for (watch_pool_t* p : watch_pools_) {
        p->cancel_all();
    }

    message_watches_.clear();

    for (auto& w : request_watches_) {
        cancel_request_watch_(*w);
    }

    request_watches_.clear();
}

bool netcom_base::process_message_(in_packet_t&& p) {
    packet_id_t id;
    p >> id;
    auto iter = message_watches_.find(id);
    if (iter == message_watches_.end()) return false;

    bool received = false;
    for (auto& tw : iter->group) {
        auto& watch = *tw;
        if (!watch.accept) continue;
        if (watch.held()) {
            received = true;
            watch.hold(in_packet_t(p));
            if (watch.once) {
                watch.accept = false;
            }
        } else {
            received = true;
            watch.receive(in_packet_t(p));
            if (watch.once) {
                cancel_message_watch_(watch);
            }
        }
    }

    return received;
}

bool netcom_base::process_request_(in_packet_t&& p) {
    packet_id_t id;
    p >> id;
    auto iter = request_watches_.find(id);
    if (iter != request_watches_.end() && (*iter)->accept) {
        auto& watch = **iter;
        if (watch.held()) {
            watch.hold(std::move(p));
            if (watch.once) {
                watch.accept = false;
            }
        } else {
            watch.receive_and_answer(*this, std::move(p));
            if (watch.once) {
                cancel_request_watch_(watch);
            }
        }
        return true;
    } else {
        // No one is here to answer this request, error from the client or from the server?
        // Send 'unhandled request' packet.
        send_unhandled_(std::move(p));
        return false;
    }
}

bool netcom_base::process_answer_(in_packet_t&& p) {
    request_id_t id;
    p >> id;

    auto iter = requests_.find(id);
    if (iter == requests_.end() || (*iter)->canceled) return false;

    (*iter)->receive(std::move(p));
    cancel_request_(**iter);
    return true;
}

bool netcom_base::process_failure_(in_packet_t&& p) {
    request_id_t id;
    p >> id;

    auto iter = requests_.find(id);
    if (iter == requests_.end() || (*iter)->canceled) return false;

    (*iter)->fail(std::move(p));
    cancel_request_(**iter);
    return true;
}

bool netcom_base::process_unhandled_(in_packet_t&& p) {
    request_id_t id;
    p >> id;

    auto iter = requests_.find(id);
    if (iter == requests_.end() || (*iter)->canceled) return false;

    (*iter)->unhandled();
    cancel_request_(**iter);
    return true;
}

void netcom_base::process_packets() {
    // Clean canceled requests and watches
    erase_if(requests_, [](std::unique_ptr<netcom_impl::request_t>& r) {
        return r->canceled;
    });
    erase_if(request_watches_, [](std::unique_ptr<netcom_impl::request_watch_t>& w) {
        return w->canceled;
    });

    for (auto& g : message_watches_) {
        erase_if(g.group, [](std::unique_ptr<netcom_impl::message_watch_t>& w) {
            return w->canceled;
        });
    }

    erase_if(message_watches_, [](netcom_impl::message_watch_group_t& g) {
        return g.group.empty();
    });

    // Process newly arrived packets
    in_packet_t p;
    while (input_.pop(p)) {
        netcom_impl::packet_type t;
        p >> t;
        in_packet_t op = p;
        switch (t) {
        case netcom_impl::packet_type::message :
            if (!process_message_(std::move(p))) {
                packet_id_t id;
                op >> id;
                out_packet_t tp = create_message_<message::unhandled_message>(id);
                process_message_(std::move(tp.to_input()));
            }
            break;
        case netcom_impl::packet_type::request :
            if (!process_request_(std::move(p))) {
                packet_id_t id;
                op >> id;
                out_packet_t tp = create_message_<message::unhandled_request>(id);
                process_message_(std::move(tp.to_input()));
            }
            break;
        case netcom_impl::packet_type::answer :
            if (!process_answer_(std::move(p))) {
                request_id_t id;
                p >> id;
                out_packet_t tp = create_message_<message::unhandled_answer>(id);
                process_message_(std::move(tp.to_input()));
            }
            break;
        case netcom_impl::packet_type::failure :
            if (!process_failure_(std::move(p))) {
                request_id_t id;
                p >> id;
                out_packet_t tp = create_message_<message::unhandled_failure>(id);
                process_message_(std::move(tp.to_input()));
            }
            break;
        case netcom_impl::packet_type::unhandled_request :
            process_unhandled_(std::move(p));
            break;
        }
    }
}

#include <print.hpp>

namespace netcom_impl {
    request_t::~request_t() {
        if (pool) {
            pool->remove_(id);
        }
    }

    void request_keeper_t::cancel() {
        if (!empty_) {
            net_->cancel_request_(rid_);
            empty_ = true;
        }
    }

    bool request_keeper_t::canceled() const {
        if (empty_) return true;
        empty_ = net_->request_cancelled_(rid_);
        return empty_;
    }

    void request_keeper_t::notify_pool_(request_pool_t* pool) {
        net_->request_notify_pool_(rid_, pool);
    }

    void message_watch_t::release(netcom_base& net) {
        if (!held_) return;

        if (!canceled) {
            for (auto& p : held_packets_) {
                receive(std::move(p));
            }
        }

        if (!held_packets_.empty() && once) {
            net.cancel_message_watch_(*this);
        }

        held_packets_.clear();
        held_ = false;
    }

    void request_watch_t::release(netcom_base& net) {
        if (!held_) return;

        for (auto& p : held_packets_) {
            receive_and_answer(net, std::move(p));
        }

        if (!held_packets_.empty() && once) {
            net.cancel_request_watch_(*this);
        }

        held_packets_.clear();
        held_ = false;
    }

    watch_pool_t::watch_pool_t(netcom_base& net) : net_(net), held_(false) {
        net_.register_watch_pool_(this);
    }

    watch_pool_t::~watch_pool_t() {
        net_.unregister_watch_pool_(this);
    }
}
