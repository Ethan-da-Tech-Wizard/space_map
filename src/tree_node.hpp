// ==============================================================================
// tree_node.hpp - TreeNode, the in-memory model of the scanned file tree.
//
// Every file and directory is a TreeNode. A node's size/count fields are
// aggregates for its whole subtree: when a file is found, its numbers are
// added to every ancestor (see propagate_stats). Children are kept sorted by
// name so lookups are a binary search; sort_children_by_size() reorders them
// for display.
//
// Thread safety: counters are atomics; the children vector is protected by a
// per-node shared_mutex (many readers or one writer).
// ==============================================================================
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
    // size = logical bytes (what ls reports); allocated_size = bytes actually
    // reserved on disk (blocks), which can be larger or smaller (sparse/compressed).
    std::atomic<uint64_t> size{0};
    std::atomic<uint64_t> allocated_size{0};
    // Totals for the subtree rooted at this node.
    std::atomic<uint64_t> file_count{0};
    std::atomic<uint64_t> dir_count{0};
    bool is_dir = false;
    // Raw back-pointer (non-owning); the parent owns the child via unique_ptr.
    TreeNode* parent = nullptr;
    // Sorted by name while scanning (required by find_child's binary search); may
    // be re-sorted by size for display, after which name lookup must not be used.
    std::vector<std::unique_ptr<TreeNode>> children;
    mutable std::shared_mutex mutex; // Protects children vector

    TreeNode(std::string n, bool dir, TreeNode* p = nullptr);
    ~TreeNode() = default;

    // Prevent copying/moving to keep parent-child pointers valid
    // Nodes point at each other, so copying or moving one would leave dangling
    // parent pointers; they are neither copyable nor movable.
    TreeNode(const TreeNode&) = delete;
    TreeNode& operator=(const TreeNode&) = delete;
    TreeNode(TreeNode&&) = delete;
    TreeNode& operator=(TreeNode&&) = delete;

    // Find a child node by name under read lock. Returns nullptr if not found.
    TreeNode* find_child(std::string_view child_name) const;

    // Get a child node by name, creating it under write lock if it doesn't exist.
    TreeNode* get_or_create_child(std::string_view child_name, bool child_is_dir);

    // Propagates size, allocated size, file count, and directory count updates up to the root.
    // Walks up the parent chain adding the deltas; each addition is atomic, so
    // concurrent workers can update shared ancestors without a lock.
    void propagate_stats(uint64_t size_delta, uint64_t allocated_delta, uint64_t files_delta, uint64_t dirs_delta);

    // Sorts children by size descending for display. Recursively sorts if requested.
    void sort_children_by_size(bool recursive = false);

    // Static test verification helper
    static void test_tree_node();
};
