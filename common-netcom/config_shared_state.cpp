#include "config_shared_state.hpp"
#include <string.hpp>

namespace config {
    const std::string meta_header = "__meta.";

    bool typed_state::is_meta(const std::string& name) const {
        return string::start_with(name, meta_header);
    }

    void shared_state::clear() {
        typed_state::clear();
        shared_.clear();
    }
}
