#include "config_shared_state.hpp"

namespace config {
    void shared_state::clear() {
        state::clear();
        shared_.clear();
    }
}
