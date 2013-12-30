#include "config.hpp"
#include <fstream>

namespace config {
    state::state() : dirty_(false) {}

    void state::parse(const std::string& file) {
        std::ifstream f(file.c_str());
        if (!f.is_open()) return;

        while (!f.eof()) {
            std::string line;
            std::getline(f, line);

            std::size_t start_name = line.find_first_not_of(" \t");
            if (start_name == std::string::npos) continue;
            std::size_t end_name = line.find_first_of(" \t(", start_name);
            if (end_name == std::string::npos) continue;
            std::size_t start_value = line.find_first_of("(", end_name);
            if (start_value == std::string::npos) continue;
            ++start_value;
            std::size_t end_value = line.find_last_of(")");
            if (end_value == std::string::npos) continue;

            config_node& node = tree_.reach(line.substr(start_name, end_name-start_name));
            if (!node.is_empty) {
                std::string new_value = line.substr(start_value, end_value-start_value);
                if (node.value != new_value) {
                    node.signal.dispatch(node.value);
                }
            } else {
                node.value = line.substr(start_value, end_value-start_value);
                node.is_empty = false;
            }
        }
    }

    void state::save(const std::string& file) const {
        if (dirty_) {
            std::ofstream f(file.c_str());
            save_node_(f, tree_.root(), "");
            dirty_ = false;
        }
    }

    void state::clear() {
        tree_.clear();
    }

    void state::save_node_(std::ofstream& f, const ctl::string_tree<config_node>::branch& node,
        const std::string& name) const {

        for (auto& n : node.childs) {
            if (n->is_branch)
                save_node_(f, (const ctl::string_tree<config_node>::branch&)*n, name+n->name+'.');
            else
                f << name << n->name;
                f << '(' << ((ctl::string_tree<config_node>::leaf&)*n).data.value << ')' << '\n';
        }
    }
}
