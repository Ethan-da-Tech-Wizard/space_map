#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <shared_mutex>
#include <string_view>
#include <atomic>

struct TreeNode {
    std::string name;
    std::atomic<uint64_t> size{0};
    std::atomic<uint64_t> allocated_size{0};
    std::atomic<uint64_t> file_count{0};
    std::atomic<uint64_t> dir_count{0};
    bool is_dir = false;
    TreeNode* parent = nullptr;
    std::vector<std::unique_ptr<TreeNode>> children;
    mutable std::shared_mutex mutex; // Protects children vector

    TreeNode(std::string n, bool dir, TreeNode* p = nullptr);
    ~TreeNode() = default;

    // Prevent copying/moving to keep parent-child pointers valid
    TreeNode(const TreeNode&) = delete;
    TreeNode& operator=(const TreeNode&) = delete;
    TreeNode(TreeNode&&) = delete;
    TreeNode& operator=(TreeNode&&) = delete;

    // Find a child node by name under read lock. Returns nullptr if not found.
    TreeNode* find_child(std::string_view child_name) const;

    // Get a child node by name, creating it under write lock if it doesn't exist.
    TreeNode* get_or_create_child(std::string_view child_name, bool child_is_dir);

    // Propagates size, allocated size, file count, and directory count updates up to the root.
    void propagate_stats(uint64_t size_delta, uint64_t allocated_delta, uint64_t files_delta, uint64_t dirs_delta);

    // Sorts children by size descending for display. Recursively sorts if requested.
    void sort_children_by_size(bool recursive = false);

    // Static test verification helper
    static void test_tree_node();
};
