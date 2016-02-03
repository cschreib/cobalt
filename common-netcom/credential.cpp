#include "credential.hpp"
#include "packet.hpp"

credential_list_t::credential_list_t(std::initializer_list<credential_t> lst) :
    list_(lst) {}

void credential_list_t::grant(const credential_t& granted) {
    list_.insert(granted);
}

void credential_list_t::grant(const credential_list_t& granted) {
    for (auto& c : granted.list_) {
        list_.insert(c);
    }
}

void credential_list_t::remove(const credential_t& removed) {
    list_.erase(removed);
}

void credential_list_t::remove(const credential_list_t& removed) {
    for (auto& c : removed.list_) {
        list_.erase(c);
    }
}

void credential_list_t::clear() {
    list_.clear();
}

std::size_t credential_list_t::size() const {
    return list_.size();
}

bool credential_list_t::empty() const {
    return list_.empty();
}

credential_list_t::const_iterator credential_list_t::begin() const {
    return list_.begin();
}

credential_list_t::const_iterator credential_list_t::end() const {
    return list_.end();
}

packet_t::base& operator << (packet_t::base& t, const credential_list_t& l) {
    return t << l.list_;
}

packet_t::base& operator >> (packet_t::base& t, credential_list_t& l) {
    return t >> l.list_;
}
