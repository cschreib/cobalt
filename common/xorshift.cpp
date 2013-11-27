#include "xorshift.hpp"
#include <limits>

static const xorshift::state_type s_default_seed = {
    123456789, 362436069, 521288629, 88675123
};

xorshift::xorshift(void)
    : state_(s_default_seed)
{}

xorshift::xorshift(const state_type &seed)
    : state_(seed)
{}

// note: addition by Kalith (05/2013)
xorshift::xorshift(const std::string &seed) {
    this->seed(seed);
}

xorshift::xorshift(result_type r)
    : state_({s_default_seed.x, s_default_seed.y, s_default_seed.z, r})
{}

void xorshift::seed(const state_type &seed) {
    state(seed);
}

void xorshift::seed(void) {
    state(s_default_seed);
}

void xorshift::seed(result_type r) {
    auto seed = s_default_seed;
    seed.w = r;
    state(seed);
}

// note: addition by Kalith (05/2013)
void xorshift::seed(const std::string& state) {
    std::string tmp = state; tmp.resize(16);
    const std::uint32_t* data = reinterpret_cast<const std::uint32_t*>(tmp.c_str());
    state_.x = data[0];
    state_.y = data[1];
    state_.z = data[2];
    state_.w = data[3];
}

void xorshift::discard(unsigned long long z) {
    while (z--)
        (*this)();
}

const xorshift::state_type &xorshift::state(void) const {
    return state_;
}

void xorshift::state(const state_type &state) {
    state_ = state;
}

xorshift::result_type xorshift::min(void) {
    return std::numeric_limits<result_type>::min();
}

xorshift::result_type xorshift::max(void) {
    return std::numeric_limits<result_type>::max();
}

xorshift::result_type xorshift::operator()(void) {
    result_type t = state_.x ^ (state_.x << 15);
    state_.x = state_.y; state_.y = state_.z; state_.z = state_.w;
    return state_.w = state_.w ^ (state_.w >> 21) ^ (t ^ (t >> 4));
}

static bool operator==(
    const xorshift::state_type &lhs, const xorshift::state_type &rhs)
{
    return lhs.x == rhs.x
        && lhs.y == rhs.y
        && lhs.z == rhs.z
        && lhs.w == rhs.w;
}

static bool operator!=(
    const xorshift::state_type &lhs, const xorshift::state_type &rhs)
{
    return lhs.x != rhs.x
        || lhs.y != rhs.y
        || lhs.z != rhs.z
        || lhs.w != rhs.w;
}

bool operator==(const xorshift &lhs, const xorshift &rhs) {
    return lhs.state() == rhs.state();
}

bool operator!=(const xorshift &lhs, const xorshift &rhs) {
    return lhs.state() != rhs.state();
}
