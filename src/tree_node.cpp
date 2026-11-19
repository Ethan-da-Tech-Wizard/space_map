// ==============================================================================
// Program: tree_node.cpp
// Programmer: Ethan-da-Tech-Wizard
// Last Modified: November 18, 2026
// Purpose: Thread-safe prefix path compressed node trees storing disk usage statistics
//
// How it fits: Scanner (scanner.cpp and src/platform/scanner_*.cpp) builds one
// TreeNode tree per scan from many worker threads; FileTreeModel exposes it to
// the QTreeView; MainWindow prunes it after deletes. The class declaration and
// the invariants (name-sorted children, atomic aggregates, per-node
// shared_mutex) are in tree_node.hpp.
//
// Locking rules implemented here: children are touched only under the node's
// own mutex (shared for reads, exclusive for inserts/sorts). Counters are
// atomics updated without any lock, which is safe because they only ever
// accumulate (in the scanner) and each ancestor is just a sum.
//
// Known limit: sort_children_by_size() destroys the name order that
// find_child()/get_or_create_child() depend on; it is only used after a scan
// has finished (see file_tree_model.cpp for the same caveat).
// ==============================================================================
#include "tree_node.hpp"
#include <algorithm>
#include <mutex>

/**
 * Creates a node.
 * @param n   File or directory name (not a path); may be the full root path for the root node.
 * @param dir true for directories; only directories get child storage reserved.
 * @param p   Owning parent, or nullptr for the root. Stored raw (non-owning).
 */
// Reserving 16 slots avoids the first few reallocations: most directories have
// few entries, and reallocating a vector of unique_ptr while other threads
// hold the lock would be wasted work.
TreeNode::TreeNode(std::string n, bool dir, TreeNode* p)
    : name(std::move(n)), is_dir(dir), parent(p) {
    if (is_dir) {
        children.reserve(16);
    }
}

/**
 * Looks up a direct child by exact name.
 * Binary search (std::lower_bound), valid only while `children` is sorted by
 * name, i.e. before any call to sort_children_by_size().
 * Takes a shared lock, so many threads can search the same directory at once.
 * @param child_name Name to find (case-sensitive byte comparison).
 * @return The child, or nullptr if absent. The pointer stays valid as long as
 *         the child is not removed from the tree.
 */
TreeNode* TreeNode::find_child(std::string_view child_name) const {
    std::shared_lock lock(mutex);
    auto it = std::lower_bound(children.begin(), children.end(), child_name,
        [](const std::unique_ptr<TreeNode>& node, std::string_view name) {
            return node->name < name;
        });
    if (it != children.end() && (*it)->name == child_name) {
        return it->get();
    }
    return nullptr;
}

/**
 * Returns the child called @p child_name, creating it if it does not exist.
 * Safe to call from many scanner threads at once: the common "already there"
 * case takes only a shared lock; creation upgrades to an exclusive lock.
 * The new child is inserted at its sorted position so find_child() keeps working.
 * @param child_name  Name of the entry.
 * @param child_is_dir Kind to give a NEW node (ignored if the child exists).
 * @return Never nullptr. The pointer remains valid for the life of the tree.
 */
TreeNode* TreeNode::get_or_create_child(std::string_view child_name, bool child_is_dir) {
    // Fast path: most lookups hit an existing child (a file seen again, a
    // directory already created by its parent's scan), and shared locks let all
    // workers do that in parallel.
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

    // A shared lock cannot be upgraded in place, so it was released above. Another
    // thread may have inserted the same name in that gap, hence the second
    // lookup under the exclusive lock (double-checked locking).
    std::unique_lock lock(mutex);
    
    auto it = std::lower_bound(children.begin(), children.end(), child_name,
        [](const std::unique_ptr<TreeNode>& node, std::string_view name) {
            return node->name < name;
        });
    if (it != children.end() && (*it)->name == child_name) {
        return it->get();
    }

    // `it` is the lower_bound position computed under the exclusive lock, so
    // inserting there keeps `children` sorted by name.
    auto new_node = std::make_unique<TreeNode>(std::string(child_name), child_is_dir, this);
    TreeNode* ptr = new_node.get();
    children.insert(it, std::move(new_node));
    return ptr;
}

/**
 * Adds a delta to this node and every ancestor up to the root.
 *
 * Called once per scanned directory with the totals for the files directly
 * inside it, so each ancestor's numbers always equal the sum of its subtree.
 * Deltas are unsigned: the scanner only adds. When MainWindow deletes a
 * node it subtracts from the ancestors with its own propagate_deletion_stats()
 * helper and adjusts the scanner's global counters via Scanner::adjust_stats;
 * it does not call this function.
 * @param size_delta      Logical bytes to add.
 * @param allocated_delta On-disk (block) bytes to add.
 * @param files_delta     Files to add.
 * @param dirs_delta      Sub-directories to add.
 * Thread safety: each += is atomic and needs no lock; concurrent callers
 * simply interleave their additions, and the sums stay exact.
 */
void TreeNode::propagate_stats(uint64_t size_delta, uint64_t allocated_delta, uint64_t files_delta, uint64_t dirs_delta) {
    // Walk parent pointers, not children, so no lock is needed: parents never
    // change after a node is created.
    TreeNode* current = this;
    while (current != nullptr) {
        current->size += size_delta;
        current->allocated_size += allocated_delta;
        current->file_count += files_delta;
        current->dir_count += dirs_delta;
        current = current->parent;
    }
}

/**
 * Reorders children largest-first for display.
 * Ties are broken by name so equal sizes always give the same order (stable output).
 * After this call the children are NO LONGER name-sorted: find_child() and
 * get_or_create_child() must not be used on this node again.
 * @param recursive When true, also sorts every descendant directory.
 * Takes the exclusive lock on each node while sorting that node's children.
 */
void TreeNode::sort_children_by_size(bool recursive) {
    std::unique_lock lock(mutex);
    // Exclusive: std::sort moves the unique_ptrs around, so no reader may iterate meanwhile.
    std::sort(children.begin(), children.end(),
        [](const std::unique_ptr<TreeNode>& a, const std::unique_ptr<TreeNode>& b) {
            if (a->size != b->size) {
                return a->size > b->size; // Sort by size descending
            }
            return a->name < b->name; // Alphabetical fallback
        });
    
    // Recursing while holding this node's lock is safe: locks are always taken
    // parent first, then child, never the reverse (no lock-order cycle).
    if (recursive) {
        for (auto& child : children) {
            child->sort_children_by_size(true);
        }
    }
}

// ---------------------------------------------------------------------------
// Self-test. TreeNode::test_tree_node() is called by main() when the program
// is started with --test-only. It builds a tiny tree, checks that stats
// propagate to every ancestor, that sorting by size works, and that 10 threads
// inserting 1000 files in total leave exact totals. Each check uses assert(),
// so a failure aborts the process in non-NDEBUG builds and is silently
// skipped in Release builds.
// ---------------------------------------------------------------------------
#include <cassert>
#include <iostream>
#include <thread>

/** Runs the TreeNode self-test described above; prints a line on success. */
void TreeNode::test_tree_node() {
    auto root = std::make_unique<TreeNode>("/", true);
    
    // Part 1: creating children links parent pointers and names correctly.
    // Test child retrieval/creation
    TreeNode* home = root->get_or_create_child("home", true);
    assert(home != nullptr);
    assert(home->parent == root.get());
    assert(home->name == "home");

    TreeNode* user = home->get_or_create_child("user", true);
    assert(user != nullptr);
    assert(user->parent == home);

    // Part 2: adding a file's numbers at the leaf updates every ancestor.
    // Test stats propagation
    TreeNode* file1 = user->get_or_create_child("file1.txt", false);
    file1->propagate_stats(100, 100, 1, 0); // 100 bytes size, 100 bytes allocated, 1 file

    assert(file1->size == 100);
    assert(file1->allocated_size == 100);
    assert(user->size == 100);
    assert(user->allocated_size == 100);
    assert(user->file_count == 1);
    assert(root->size == 100);

    // Part 3: after adding a larger file, size-descending order puts it first.
    // Test sorting by size
    TreeNode* file2 = user->get_or_create_child("file2.txt", false);
    file2->propagate_stats(500, 500, 1, 0); // file2 gets 500 bytes

    assert(user->size == 600);
    assert(user->allocated_size == 600);
    assert(user->file_count == 2);

    user->sort_children_by_size(false);
    assert(user->children[0]->name == "file2.txt");
    assert(user->children[1]->name == "file1.txt");

    // Part 4 (concurrency): ten threads each create 100 distinct files in the same
    // directory, so the same vector is inserted into 1000 times concurrently.
    // Totals must come out exact: 2 earlier files + 1000 new ones, 10 bytes each.
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

    // 2 + 1000 files and 600 + 1000 x 10 bytes in total (both size fields).
    assert(user->file_count == 2 + 1000);
    assert(user->size == 600 + 1000 * 10);
    assert(user->allocated_size == 600 + 1000 * 10);

    std::cout << "TreeNode tests passed successfully!" << "\n";
}
