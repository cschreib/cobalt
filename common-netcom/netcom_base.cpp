#include "netcom_base.hpp"
#include <scoped.hpp>
#include <iostream>

namespace netcom_exception {
    base::base(const std::string& s) : std::runtime_error(s) {}
    base::base(const char* s) : std::runtime_error(s) {}

    base::~base() noexcept {}
}

netcom_base::netcom_base() : out_(cout) {}
netcom_base::netcom_base(logger& out) : out_(out) {}

void netcom_base::send(out_packet_t p) {
    if (p.to == invalid_actor_id) throw netcom_exception::invalid_actor();
    output_.push(std::move(p));
}

void netcom_base::stop_request_(request_id_t id) {
    if (clearing_) return;

    auto iter = answer_signals_.find(id);
    if (iter != answer_signals_.end()) {
        answer_signals_.erase(iter);
    }

    request_id_provider_.free_id(id);
}

void netcom_base::terminate_() {
    if (processing_) {
        call_terminate_ = true;
    } else {
        terminate_();
    }
}

void netcom_base::do_terminate_() {
    auto sd = ctl::scoped_toggle(clearing_);

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

void netcom_base::process_message_(in_packet_t&& p) {
    packet_id_t id;
    p >> id;

    if (debug_packets) {
        out_.print("<", p.from, ": ", get_packet_name(id), " (id=", id, ")");
    }

    auto iter = message_signals_.find(id);
    if (iter == message_signals_.end() || (*iter)->empty()) {
        if (id != message::unhandled_message::packet_id__ &&
            id != message::unhandled_request::packet_id__ &&
            id != message::unhandled_request_answer::packet_id__) {
            if (!is_packet_id(id)) {
                throw netcom_exception::invalid_packet_id(id);
            }

            if (debug_packets) {
                out_.print(" -> unhandled");
            }

            out_packet_t tp = create_message(make_packet<message::unhandled_message>(id));
            in_packet_t& itp = tp.to_input();
            netcom_impl::packet_type t; itp >> t; // Trash packet type
            process_message_(std::move(itp));
        }
    } else {
        (*iter)->dispatch(std::move(p));
    }
}

void netcom_base::process_request_(in_packet_t&& p) {
    packet_id_t id;
    p >> id;

    if (debug_packets) {
        request_id_t rid; p.view() >> rid;
        out_.print("<", p.from, ": ", get_packet_name(id),  " (", rid, ")");
    }

    auto iter = request_signals_.find(id);
    if (iter == request_signals_.end() || (*iter)->empty()) {
        if (!is_packet_id(id)) {
            throw netcom_exception::invalid_packet_id(id);
        }

        if (debug_packets) {
            out_.print(" -> unhandled");
        }

        // No one is here to answer this request. Could be an error of either sides.
        // Send 'unhandled request' packet to the requester.
        request_id_t rid;
        p >> rid;
        send_unhandled_(p.from, rid);

        out_packet_t tp = create_message(make_packet<message::unhandled_request>(id));
        in_packet_t& itp = tp.to_input();
        netcom_impl::packet_type t; itp >> t; // Trash packet type
        process_message_(std::move(itp));
    } else {
        (*iter)->dispatch(*this, std::move(p));
    }
}

void netcom_base::process_answer_(netcom_impl::packet_type t, in_packet_t&& p) {
    request_id_t rid;
    p >> rid;

    auto iter = answer_signals_.find(rid);
    if (iter == answer_signals_.end()) {
        if (debug_packets) {
            out_.print("<", p.from, ": answer to request ", rid, " (unhandled)");
        }

        out_packet_t tp = create_message(make_packet<message::unhandled_request_answer>(rid));
        in_packet_t& itp = tp.to_input();
        netcom_impl::packet_type t; itp >> t; // Trash packet type
        process_message_(std::move(itp));
    } else {
        if (debug_packets) {
            out_.print("<", p.from, ": answer to ", get_packet_name((*iter)->id), " (", rid, ")");
        }

        (*iter)->dispatch(t, std::move(p));
    }
}

void netcom_base::process_packets() {
    auto sc = ctl::scoped_toggle(processing_);

    // Process newly arrived packets
    in_packet_t p;
    while (input_.pop(p)) {
        netcom_impl::packet_type t;
        p >> t;

        // TODO: implement optional packet compression?

        switch (t) {
        case netcom_impl::packet_type::message :
            process_message_(std::move(p));
            break;
        case netcom_impl::packet_type::request :
            process_request_(std::move(p));
            break;
        case netcom_impl::packet_type::answer :
        case netcom_impl::packet_type::failure :
        case netcom_impl::packet_type::missing_credentials :
        case netcom_impl::packet_type::unhandled :
            process_answer_(t, std::move(p));
            break;
        }
    }

    if (call_terminate_) {
        do_terminate_();
    }
}
