#include "server_serializable.hpp"

namespace server {
    serializable::serializable(std::string name) : name_(std::move(name)) {}

    const std::string& serializable::name() const {
        return name_;
    }
}
