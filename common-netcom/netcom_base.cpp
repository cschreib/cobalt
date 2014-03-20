#include "netcom_base.hpp"
#include "endian.hpp"
#include <scoped.hpp>
#include <iostream>

namespace netcom_exception {
    base::base(const std::string& s) : std::runtime_error(s) {}
    base::base(const char* s) : std::runtime_error(s) {}

    base::~base() noexcept {}
}

netcom_base::netcom_base() {}

void netcom_base::send(out_packet_t p) {
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

    request_id_provider_.free_id(id);
}

void netcom_base::terminate() {
    if (processing_) {
        call_terminate_ = true;
    } else {
        terminate_();
    }
}

void netcom_base::terminate_() {
    clearing_ = true;
    auto sd = ctl::make_scoped([this]() { clearing_ = false; });

    input_.clear();
    output_.clear();
    request_id_provider_.clear();

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

    if (debug_packets) {
        std::cout << "<" << p.from << ": " << get_packet_name(id) << std::endl;
    }

    auto iter = message_signals_.find(id);
    if (iter == message_signals_.end() || (*iter)->empty()) return false;

    (*iter)->dispatch(std::move(p));
    return true;
}

bool netcom_base::process_request_(in_packet_t&& p) {
    packet_id_t id;
    p >> id;

    if (debug_packets) {
        request_id_t rid; p.view() >> rid;
        std::cout << "<" << p.from << ": " << get_packet_name(id)
            << " (" << rid << ")" << std::endl;
    }

    auto iter = request_signals_.find(id);
    if (iter == request_signals_.end() || (*iter)->empty()) {
        // No one is here to answer this request. Could be an error of either sides.
        // Send 'unhandled request' packet to the requester.
        request_id_t rid;
        p >> rid;
        send_unhandled_(p.from, rid);
        return false;
    }

    (*iter)->dispatch(*this, std::move(p));
    return true;
}

bool netcom_base::process_answer_(netcom_impl::packet_type t, in_packet_t&& p) {
    request_id_t rid;
    p >> rid;

    auto iter = answer_signals_.find(rid);
    if (iter == answer_signals_.end()) {
        if (debug_packets) {
            std::cout << "<" << p.from << ": answer to request " << rid
                << " (unhandled)" << std::endl;
        }
        return false;
    }

    if (debug_packets) {
        std::cout << "<" << p.from << ": answer to " << get_packet_name((*iter)->id)
            << " (" << rid << ")" << std::endl;
    }

    (*iter)->dispatch(t, std::move(p));
    return true;
}

void netcom_base::process_packets() {
    processing_ = true;
    auto sc = ctl::make_scoped([this]() { processing_ = false; });

    // Process newly arrived packets
    in_packet_t p;
    while (input_.pop(p)) {
        netcom_impl::packet_type t;
        p >> t;

        // TODO: optimize this copy
        in_packet_t op = p;

        switch (t) {
        case netcom_impl::packet_type::message :
            if (!process_message_(std::move(p))) {
                packet_id_t id;
                op >> id;
                out_packet_t tp = create_message(make_packet<message::unhandled_message>(id));
                process_message_(std::move(tp.to_input()));
            }
            break;
        case netcom_impl::packet_type::request :
            if (!process_request_(std::move(p))) {
                packet_id_t id;
                op >> id;
                out_packet_t tp = create_message(make_packet<message::unhandled_request>(id));
                process_message_(std::move(tp.to_input()));
            }
            break;
        case netcom_impl::packet_type::answer :
        case netcom_impl::packet_type::failure :
        case netcom_impl::packet_type::unhandled :
            if (!process_answer_(t, std::move(p))) {
                request_id_t id;
                p >> id;
                out_packet_t tp = create_message(
                    make_packet<message::unhandled_request_answer>(id)
                );
                process_message_(std::move(tp.to_input()));
            }
            break;
        }
    }

    if (call_terminate_) {
        terminate_();
    }
}
