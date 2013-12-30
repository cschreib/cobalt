#include "netcom_base.hpp"
#include "endian.hpp"
#include <scoped.hpp>

namespace netcom_exception {
    base::base(const std::string& s) : std::runtime_error(s) {}
    base::base(const char* s) : std::runtime_error(s) {}

    base::~base() noexcept {}
}

netcom_base::netcom_base() {
    std::size_t n = std::numeric_limits<request_id_t>::max();
    available_request_ids_.reserve(n);
    for (std::size_t i = 1; i <= n; ++i) {
        available_request_ids_.insert(n-i);
    }
}

void netcom_base::send_(out_packet_t&& p) {
    if (p.to == invalid_actor_id) throw netcom_exception::invalid_actor();
    if (p.to == self_actor_id) {
        // TODO: check thread safety of this
        input_.push(std::move(p.to_input()));
    } else {
        output_.push(std::move(p));
    }
}

void netcom_base::stop_request_(request_id_t id) {
    if (clearing_) return;

    auto iter = answer_signals_.find(id);
    if (iter != answer_signals_.end()) {
        answer_signals_.erase(iter);
    }

    free_request_id_(id);
}

void netcom_base::free_request_id_(request_id_t id) {
    available_request_ids_.insert(id);
}

request_id_t netcom_base::make_request_id_() {
    if (available_request_ids_.empty()) throw netcom_exception::too_many_requests();
    request_id_t id = available_request_ids_.back();
    available_request_ids_.pop_back();
    return id;
}

void netcom_base::clear_all_() {
    clearing_ = true;
    auto sd = ctl::make_scoped([this]() { clearing_ = false; });

    input_.clear();
    output_.clear();

    std::size_t n = std::numeric_limits<request_id_t>::max();
    available_request_ids_.clear();
    for (std::size_t i = 1; i <= n; ++i) {
        available_request_ids_.insert(n-i);
    }

    answer_signals_.clear();

    for (auto& s : request_signals_) {
        s->clear();
    }

    for (auto& s : message_signals_) {
        s->clear();
    }
}

bool netcom_base::process_message_(in_packet_t&& p) {
    packet_id_t id;
    p >> id;
    auto iter = message_signals_.find(id);
    if (iter == message_signals_.end() || (*iter)->empty()) return false;

    (*iter)->dispatch(std::move(p));
    return true;
}

bool netcom_base::process_request_(in_packet_t&& p) {
    packet_id_t id;
    p >> id;
    auto iter = request_signals_.find(id);
    if (iter == request_signals_.end() || (*iter)->empty()) {
        // No one is here to answer this request. Could be an error of either sides.
        // Send 'unhandled request' packet to the requester.
        send_unhandled_(std::move(p));
        return false;
    }

    (*iter)->dispatch(*this, std::move(p));
    return true;
}

bool netcom_base::process_answer_(in_packet_t&& p) {
    request_id_t id;
    p >> id;

    auto iter = answer_signals_.find(id);
    if (iter == answer_signals_.end()) return false;

    (*iter)->dispatch_answer(std::move(p));
    return true;
}

bool netcom_base::process_failure_(in_packet_t&& p) {
    request_id_t id;
    p >> id;

    auto iter = answer_signals_.find(id);
    if (iter == answer_signals_.end()) return false;

    (*iter)->dispatch_fail(std::move(p));
    return true;
}

bool netcom_base::process_unhandled_(in_packet_t&& p) {
    request_id_t id;
    p >> id;

    auto iter = answer_signals_.find(id);
    if (iter == answer_signals_.end()) return false;

    (*iter)->dispatch_unhandled();
    return true;
}

void netcom_base::process_packets() {
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
                out_packet_t tp = create_message_(make_packet<message::unhandled_message>(id));
                process_message_(std::move(tp.to_input()));
            }
            break;
        case netcom_impl::packet_type::request :
            if (!process_request_(std::move(p))) {
                packet_id_t id;
                op >> id;
                out_packet_t tp = create_message_(make_packet<message::unhandled_request>(id));
                process_message_(std::move(tp.to_input()));
            }
            break;
        case netcom_impl::packet_type::answer :
            if (!process_answer_(std::move(p))) {
                request_id_t id;
                p >> id;
                out_packet_t tp = create_message_(make_packet<message::unhandled_answer>(id));
                process_message_(std::move(tp.to_input()));
            }
            break;
        case netcom_impl::packet_type::failure :
            if (!process_failure_(std::move(p))) {
                request_id_t id;
                p >> id;
                out_packet_t tp = create_message_(make_packet<message::unhandled_failure>(id));
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
    void answer_connection::stop() {
        if (net) {
            net->stop_request_(rid);
        }

        signal_connection_t::stop();
    }
}
