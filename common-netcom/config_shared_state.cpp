#include "config_shared_state.hpp"

namespace config {
    void shared_state::clear() {
        typed_state::clear();
        shared_.clear();
    }
}
