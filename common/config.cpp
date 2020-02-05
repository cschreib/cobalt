#include "config.hpp"
#include <fstream>
#include <string.hpp>

namespace config {
    const std::string meta_header = "__meta";

    state::state() : dirty_(false) {}

    void state::parse_from_file(const std::string& file) {
        std::ifstream f(file.c_str());
        if (!f.is_open()) return;

        parse(f);
    }

    void state::parse(std::istream& flux) {
        while (!flux.eof()) {
            std::string line;
            std::getline(flux, line);

            std::size_t start_name = line.find_first_not_of(" \t");
            if (start_name == std::string::npos) continue;
            std::size_t end_name = line.find_first_of(" \t(", start_name);
            if (end_name == std::string::npos) continue;
            std::size_t start_value = line.find_first_of("(", end_name);
            if (start_value == std::string::npos) continue;
            ++start_value;
            std::size_t end_value = line.find_last_of(")");
            if (end_value == std::string::npos) continue;

            std::string name = line.substr(start_name, end_name-start_name);
            config_node& node = tree_.reach(name);
            if (!node.is_empty) {
                std::string new_value = line.substr(start_value, end_value-start_value);
                if (node.value != new_value) {
                    on_value_changed.dispatch(name, node.value);
                    node.signal.dispatch(node.value);
                }
            } else {
                node.value = line.substr(start_value, end_value-start_value);
                node.is_empty = false;
            }
        }
    }

    void state::save_to_file(const std::string& file) const {
        if (dirty_) {
            std::ofstream f(file.c_str());
            save(f);
            dirty_ = false;
        }
    }

    void state::save(std::ostream& flux) const {
        save_node_(flux, tree_.root(), "");
    }

    void state::clear() {
        tree_.clear();
    }

    void state::save_node_(std::ostream& f, const ctl::string_tree<config_node>::branch& node,
        const std::string& name) const {

        for (auto& n : node.children) {
            if (n->is_branch) {
                auto& b = static_cast<const ctl::string_tree<config_node>::branch&>(*n);
                save_node_(f, b, name + n->name + '.');
            } else {
                f << name << n->name;
                auto& l = static_cast<const ctl::string_tree<config_node>::leaf&>(*n);
                f << '(' << l.data.value << ')' << '\n';
            }
        }
    }

    bool typed_state::is_meta(const std::string& name) const {
        return string::start_with(name, meta_header+".");
    }
}
