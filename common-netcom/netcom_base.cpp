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

// void netcom_base::serialize_(type_list<>, out_packet_t& p) {}

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
    return iter == requests_.end() || (*iter)->cancelled;
}

void netcom_base::cancel_request_(request_id_t id) {
    auto iter = requests_.find(id);
    if (iter != requests_.end()) {
        cancel_request_(iter);
    }
}

void netcom_base::cancel_request_(request_container::iterator iter) {
    free_request_id_((*iter)->id);
    (*iter)->cancelled = true;
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

bool netcom_base::message_watch_cancelled_(message_packet_id_t id, netcom_impl::watch_id_t wid) {
    auto iter1 = message_watches_.find(id);
    if (iter1 == message_watches_.end()) return true;
    auto iter2 = iter1->group.find(wid);
    return iter2 == iter1->group.end() || (*iter2)->cancelled;
}

void netcom_base::cancel_message_watch_(message_packet_id_t id, netcom_impl::watch_id_t wid) {
    auto iter1 = message_watches_.find(id);
    if (iter1 == message_watches_.end()) return;
    auto iter2 = iter1->group.find(wid);
    if (iter2 == iter1->group.end()) return;
    free_message_watch_id_(wid);
    (*iter2)->cancelled = true;
}

void netcom_base::hold_message_watch_(message_packet_id_t id, netcom_impl::watch_id_t wid) {
    auto iter1 = message_watches_.find(id);
    if (iter1 == message_watches_.end()) return;
    auto iter2 = iter1->group.find(wid);
    if (iter2 == iter1->group.end()) return;
    (*iter2)->hold();
}

void netcom_base::release_message_watch_(message_packet_id_t id, netcom_impl::watch_id_t wid) {
    auto iter1 = message_watches_.find(id);
    if (iter1 == message_watches_.end()) return;
    auto iter2 = iter1->group.find(wid);
    if (iter2 == iter1->group.end()) return;
    (*iter2)->release();
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

bool netcom_base::request_watch_cancelled_(request_packet_id_t id) {
    auto iter = request_watches_.find(id);
    return iter == request_watches_.end() || (*iter)->cancelled;
}

void netcom_base::cancel_request_watch_(request_packet_id_t id) {
    auto iter = request_watches_.find(id);
    if (iter == request_watches_.end()) return;
    (*iter)->cancelled = true;
}

void netcom_base::hold_request_watch_(request_packet_id_t id) {
    auto iter = request_watches_.find(id);
    if (iter == request_watches_.end()) return;
    (*iter)->hold();
}

void netcom_base::release_request_watch_(request_packet_id_t id) {
    auto iter = request_watches_.find(id);
    if (iter == request_watches_.end()) return;
    (*iter)->release(*this);
}

bool netcom_base::process_message_(in_packet_t&& p) {
    message_packet_id_t id;
    p >> id;
    auto iter = message_watches_.find(id);
    if (iter == message_watches_.end()) return false;

    bool received = false;
    for (auto& watch : iter->group) {
        if (watch->cancelled) continue;
        if (watch->held()) {
            received = true;
            watch->hold(in_packet_t(p));
        } else {
            received = true;
            watch->receive(in_packet_t(p));
        }
    }
    
    return received;
}

bool netcom_base::process_request_(in_packet_t&& p) {
    request_packet_id_t id;
    p >> id;
    auto iter = request_watches_.find(id);
    if (iter != request_watches_.end() && !(*iter)->cancelled) {
        if ((*iter)->held()) {
            (*iter)->hold(std::move(p));
        } else {
            (*iter)->receive_and_answer(*this, std::move(p));
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
    if (iter == requests_.end() || (*iter)->cancelled) return false;

    (*iter)->receive(std::move(p));
    cancel_request_(iter);
    return true;
}

bool netcom_base::process_failure_(in_packet_t&& p) {
    request_id_t id;
    p >> id;
    
    auto iter = requests_.find(id);
    if (iter == requests_.end() || (*iter)->cancelled) return false;

    (*iter)->fail(std::move(p));
    cancel_request_(iter);
    return true;
}

bool netcom_base::process_unhandled_(in_packet_t&& p) {
    request_id_t id;
    p >> id;
    
    auto iter = requests_.find(id);
    if (iter == requests_.end() || (*iter)->cancelled) return false;

    (*iter)->unhandled();
    cancel_request_(iter);
    return true;
}

void netcom_base::process_packets() {
    // Clean cancelled requests and watches
    std::remove_if(requests_.begin(), requests_.end(),
        [](std::unique_ptr<netcom_impl::request_t>& r) { return r->cancelled; }
    );
    std::remove_if(request_watches_.begin(), request_watches_.end(),
        [](std::unique_ptr<netcom_impl::request_watch_t>& w) { return w->cancelled; }
    );
    for (auto& g : message_watches_) {
        std::remove_if(g.group.begin(), g.group.end(),
            [](std::unique_ptr<netcom_impl::message_watch_t>& w) { return w->cancelled; }
        );
    }
    std::remove_if(message_watches_.begin(), message_watches_.end(), 
        [](netcom_impl::message_watch_group_t& g) { return g.group.empty(); }
    );

    // Process newly arrived packets
    in_packet_t p;
    while (input_.pop(p)) {
        netcom_impl::packet_type t;
        p >> t;
        in_packet_t op = p;
        switch (t) {
        case netcom_impl::packet_type::message :
            if (!process_message_(std::move(p))) {
                message_packet_id_t id;
                op >> id;
                out_packet_t tp = create_message_<message::unhandled_message>(id);
                process_message_(std::move(tp.to_input()));
            }
            break;
        case netcom_impl::packet_type::request :
            if (!process_request_(std::move(p))) {
                request_packet_id_t id;
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

namespace netcom_impl {
    void request_keeper_t::cancel() {
        if (!empty_) {
            net_->cancel_request_(rid_);
            empty_ = true;
        }
    }

    bool request_keeper_t::cancelled() const {
        if (empty_) return true;
        empty_ = net_->request_cancelled_(rid_);
        return empty_;
    }

    void request_watch_t::release(netcom_base& net) {
        if (!cancelled) {
            for (auto& p : held_packets_) {
                receive_and_answer(net, std::move(p));
            }
        } else {
            for (auto& p : held_packets_) {
                net.send_unhandled_(std::move(p));
            }
        }

        held_packets_.clear();
        held_ = false;
    }
}
