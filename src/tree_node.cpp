// ==============================================================================
// Program: space_map - tree_node.cpp
// Programmer: Ethan-da-Tech-Wizard (written with Claude, an AI assistant)
// Last Modified: 2026-11-18
// Purpose: Implements TreeNode: sorted child lookup and insertion under a reader-writer lock, upward statistics propagation, size sorting, and a built-in self-test.
// ==============================================================================
#include "tree_node.hpp"
#include <algorithm>
#include <mutex>

/// Constructs a node; directories pre-reserve room for children to limit early reallocations.
TreeNode::TreeNode(std::string n, bool dir, TreeNode* p)
    : name(std::move(n)), is_dir(dir), parent(p) {
    if (is_dir) {
        children.reserve(16);
    }
}

/// Looks up a child by name with a binary search under a shared (read) lock.
/// Returns null when no child has that name.
TreeNode* TreeNode::find_child(std::string_view child_name) const {
    std::shared_lock lock(mutex);
    // Binary search for the first child whose name is not less than the target
    auto it = std::lower_bound(children.begin(), children.end(), child_name,
        [](const std::unique_ptr<TreeNode>& node, std::string_view name) {
            return node->name < name;
        });
    if (it != children.end() && (*it)->name == child_name) {
        return it->get();
    }
    return nullptr;
}

/// Returns the named child, inserting it in sorted position when absent.
/// Uses double-checked locking so the common 'already exists' case only needs a read lock.
TreeNode* TreeNode::get_or_create_child(std::string_view child_name, bool child_is_dir) {
    // 1. Double-checked locking pattern: first try with a read lock
    {
        std::shared_lock lock(mutex);
        auto it = std::lower_bound(children.begin(), children.end(), child_name,
            [](const std::unique_ptr<TreeNode>& node, std::string_view name) {
                return node->name < name;
            });
        if (it != children.end() && (*it)->name == child_name) {
            return it->get();
        }
    }

    // 2. Not found, acquire write lock
    std::unique_lock lock(mutex);
    
    // Re-verify that another thread didn't insert it while we were waiting
    auto it = std::lower_bound(children.begin(), children.end(), child_name,
        [](const std::unique_ptr<TreeNode>& node, std::string_view name) {
            return node->name < name;
        });
    if (it != children.end() && (*it)->name == child_name) {
        return it->get();
    }

    // 3. Insert in sorted position
    auto new_node = std::make_unique<TreeNode>(std::string(child_name), child_is_dir, this);
    TreeNode* ptr = new_node.get();
    children.insert(it, std::move(new_node));
    return ptr;
}

/// Adds the given deltas to this node and every ancestor up to the root.
/// Atomic adds mean concurrent scanner threads can call this without extra locking.
void TreeNode::propagate_stats(uint64_t size_delta, uint64_t allocated_delta, uint64_t files_delta, uint64_t dirs_delta) {
    // Walk upward from this node so every ancestor total stays correct
    TreeNode* current = this;
    while (current != nullptr) {
        current->size += size_delta;
        current->allocated_size += allocated_delta;
        current->file_count += files_delta;
        current->dir_count += dirs_delta;
        current = current->parent;
    }
}

/// Orders children by size, largest first (ties by name), optionally for the whole subtree.
/// Takes the write lock for this node while it reorders.
void TreeNode::sort_children_by_size(bool recursive) {
    std::unique_lock lock(mutex);
    std::sort(children.begin(), children.end(),
        [](const std::unique_ptr<TreeNode>& a, const std::unique_ptr<TreeNode>& b) {
            if (a->size != b->size) {
                return a->size > b->size; // Sort by size descending
            }
            return a->name < b->name; // Alphabetical fallback
        });
    
    if (recursive) {
        for (auto& child : children) {
            child->sort_children_by_size(true);
        }
    }
}

// ---- Self-test support ----
#include <cassert>
#include <iostream>
#include <thread>

/// Self-test: checks child creation, stats propagation, sorting, and concurrent inserts.
/// Uses assert, so failures only show in builds where assertions are enabled.
void TreeNode::test_tree_node() {
    auto root = std::make_unique<TreeNode>("/", true);
    
    // Test child retrieval/creation
    TreeNode* home = root->get_or_create_child("home", true);
    assert(home != nullptr);
    assert(home->parent == root.get());
    assert(home->name == "home");

    TreeNode* user = home->get_or_create_child("user", true);
    assert(user != nullptr);
    assert(user->parent == home);

    // Test stats propagation
    TreeNode* file1 = user->get_or_create_child("file1.txt", false);
    file1->propagate_stats(100, 100, 1, 0); // 100 bytes size, 100 bytes allocated, 1 file

    assert(file1->size == 100);
    assert(file1->allocated_size == 100);
    assert(user->size == 100);
    assert(user->allocated_size == 100);
    assert(user->file_count == 1);
    assert(root->size == 100);

    // Test sorting by size
    TreeNode* file2 = user->get_or_create_child("file2.txt", false);
    file2->propagate_stats(500, 500, 1, 0); // file2 gets 500 bytes

    assert(user->size == 600);
    assert(user->allocated_size == 600);
    assert(user->file_count == 2);

    user->sort_children_by_size(false);
    assert(user->children[0]->name == "file2.txt");
    assert(user->children[1]->name == "file1.txt");

    // Ten threads each add 100 files to the same parent to exercise the locking
    // Test multi-threaded insertion safety
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([user, i]() {
            for (int j = 0; j < 100; ++j) {
                std::string fname = "thread_" + std::to_string(i) + "_file_" + std::to_string(j) + ".txt";
                TreeNode* f = user->get_or_create_child(fname, false);
                f->propagate_stats(10, 10, 1, 0);
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }

    assert(user->file_count == 2 + 1000);
    assert(user->size == 600 + 1000 * 10);
    assert(user->allocated_size == 600 + 1000 * 10);

    std::cout << "TreeNode tests passed successfully!" << "\n";
}
