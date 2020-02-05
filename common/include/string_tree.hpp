#ifndef STRING_TREE_HPP
#define STRING_TREE_HPP

#include "sorted_vector.hpp"
#include "member_comparator.hpp"
#include <vector>
#include <memory>
#include <algorithm>
#include <string>

namespace ctl {
    /// Data structure that indexes its elements by their position in a string tree.
    /** Every element is identified by its key, which is a simple
        string. It is similar to some standard containers like
        std::map, but differs in a few cases.
        In particular, the tree structure is explicit and contained
        in the key itself : "foo.bar" will be indexed as the leaf
        "bar" of the branch "foo". If such a key exists in this
        structure, then :
        1) There can be no key named "foo". "foo" is a branch,
           making a leaf out of it would destroy "foo.bar".
        2) There can be no key named "foo.bar.zee". "foo.bar" is a
           leaf, making a branch out of it would destroy the data
           if contains.
        When one tries to do one of the above, an exception is
        raised, and it is up to the caller to catch it and act
        accordingly.

        Implementation details :
        A leaf is implemented as a simple aggregate of its key
        and the data is holds.
        A branch is implemented as a sorted std::vector of nodes,
        nodes that can be either leaves or branches.
    **/
    template<class T>
    struct string_tree {
        /// Exception raised when an error occurs while trying to modify the tree.
        /** It derives from std::exception : use what() to have the error message.
        */
        struct wrong_structure_exception : public std::exception {
            explicit wrong_structure_exception(const std::string& message) : message(message) {}
            const char* what() const noexcept override { return message.c_str(); }
            std::string message;
        };

        struct expecting_branch_exception : public wrong_structure_exception {
            explicit expecting_branch_exception(const std::string& where) :
                wrong_structure_exception("expecting '"+where+"' to be a branch") {}
        };

        struct expecting_leaf_exception : public wrong_structure_exception {
            explicit expecting_leaf_exception(const std::string& where) :
                wrong_structure_exception("expecting '"+where+"' to be a leaf") {}
        };

        /// Abstract node in the tree.
        struct node {
            node(const std::string& name, bool is_branch) : name(name), is_branch(is_branch) {}
            virtual ~node() {}

            std::string name;
            bool is_branch;
        };

        /// Branch of the tree : contains other nodes.
        struct branch : public node {
            branch(const std::string& name) : node(name, true) {}
            sorted_vector<std::unique_ptr<node>, mem_var_comp(&node::name)> children;
        };

        /// Leaf of the tree : contains data.
        struct leaf : public node {
            leaf(const std::string& name) : node(name, false) {}
            T data;
        };

        /// Default constructor.
        /** Builds an "empty" tree, with a single branch and no leaf.
        */
        string_tree() : root_("") {}

        /// Try to reach a leaf of the tree by its key.
        /** If the provided key is incompatible with the current
        *   tree structure, i.e. it incorrectly assumes that a node
        *   is a branch or a leaf, then no node is returned.
        *   It also will not modify the tree, so it will not create
        *   any node in order to reach it.
        *   \param name The name of the leaf to reach
        */
        const T* try_reach(const std::string& name) const {
            const leaf* l = try_reach_<leaf>(root_, name, 0);
            if (l) {
                return &l->data;
            } else {
                return nullptr;
            }
        }

        /// Reach or create a leaf of the tree by its key.
        /** If the provided key is incompatible with the current
        *   tree structure, i.e. it incorrectly assumes that a node
        *   is a branch or a leaf, then an exception is raised.
        *   If the leaf or any of its parent do not exist, they
        *   will be created.
        *   \param name The name of the leaf to reach
        */
        T& reach(const std::string& name) {
            return reach_<leaf>(root_, name, 0).data;
        }

        /// Try to reach a branch of the tree by its key.
        /** If the provided key is incompatible with the current
        *   tree structure, i.e. it incorrectly assumes that a node
        *   is a branch or a leaf, then no node is returned.
        *   It also will not modify the tree, so it will not create
        *   any node in order to reach it.
        *   \param name The name of the branch to reach
        */
        const branch* try_reach_branch(const std::string& name) const {
            return try_reach_<branch>(root_, name, 0);
        }

        /// Reach or create a branch of the tree by its key.
        /** If the provided key is incompatible with the current
        *   tree structure, i.e. it incorrectly assumes that a node
        *   is a branch or a leaf, then an exception is raised.
        *   If the branch or any of its parent do not exist, they
        *   will be created.
        *   \param name The name of the branch to reach
        */
        branch& reach_branch(const std::string& name) {
            return reach_<branch>(root_, name, 0);
        }

        /// Removes all the elements from the tree.
        /** Only leaves the root branch.
        */
        void clear() {
            root_.children.clear();
        }

        /// Reference to the root branch.
        /** Shoud only be used to travel through the tree.
        */
        branch& root() {
            return root_;
        }

        /// Const reference to the root branch.
        /** Shoud only be used to travel through the tree.
        */
        const branch& root() const {
            return root_;
        }

    private :
        branch root_;

        template<typename U>
        const U* try_reach_(const branch& b, const std::string& name, std::size_t start) const {
            std::size_t pos = name.find_first_of('.', start);
            std::string node_name;
            if (pos != std::string::npos) {
                node_name = name.substr(start, pos-start);
            } else {
                node_name = name.substr(start);
            }

            auto iter = b.children.find(node_name);
            if (iter != b.children.end()) {
                auto* node = iter->get();
                if (pos == std::string::npos) {
                    if ((std::is_same<U,branch>::value && node->is_branch) ||
                        (std::is_same<U,leaf>::value && !node->is_branch)) {
                        return static_cast<const U*>(node);
                    } else {
                        return nullptr;
                    }
                } else {
                    if (node->is_branch) {
                        return try_reach_<U>(static_cast<const branch&>(*node), name, pos+1);
                    } else {
                        return nullptr;
                    }
                }
            } else {
                return nullptr;
            }
        }

        template<typename U>
        U& reach_(branch& b, const std::string& name, std::size_t start) const {
            std::size_t pos = name.find_first_of('.', start);
            std::string node_name;
            if (pos != std::string::npos) {
                node_name = name.substr(start, pos-start);
            } else {
                node_name = name.substr(start);
            }

            auto iter = b.children.find(node_name);
            if (iter != b.children.end()) {
                auto& node = **iter;
                if (pos == std::string::npos) {
                    if (std::is_same<U,branch>::value && !node.is_branch) {
                        throw expecting_branch_exception(name.substr(0, pos));
                    }
                    if (std::is_same<U,leaf>::value && node.is_branch) {
                        throw expecting_leaf_exception(name.substr(0, pos));
                    }

                    return static_cast<U&>(node);
                } else {
                    if (node.is_branch) {
                        return reach_<U>(static_cast<branch&>(node), name, pos+1);
                    } else {
                        throw expecting_branch_exception(name.substr(0, pos));
                    }
                }
            } else {
                if (pos == std::string::npos) {
                    auto iter = b.children.insert(std::unique_ptr<node>(new U(node_name)));
                    return static_cast<U&>(**iter);
                } else {
                    auto iter = b.children.insert(std::unique_ptr<node>(new branch(node_name)));
                    return reach_<U>(static_cast<branch&>(**iter), name, pos+1);
                }
            }
        }
    };
}

#endif
