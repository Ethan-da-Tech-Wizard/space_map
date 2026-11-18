// ==============================================================================
// Program: space_map - scanner.hpp
// Programmer: Ethan-da-Tech-Wizard (written with Claude, an AI assistant)
// Last Modified: 2026-11-18
// Purpose: Declares Scanner, the multi-threaded filesystem crawler that fills a TreeNode tree and reports progress through Qt signals.
// ==============================================================================
#pragma once
#include <QObject>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <set>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <sys/statvfs.h>
#endif
#include "tree_node.hpp"

/// Walks a directory tree with a pool of worker threads and builds a TreeNode tree of sizes.
/// Progress and completion are reported through Qt signals.
class Scanner : public QObject {
    Q_OBJECT
public:
    /// Creates an idle scanner.
    explicit Scanner(QObject* parent = nullptr);
    /// Cancels any running scan and joins the worker threads.
    ~Scanner() override;

    // Starts the multi-threaded scanning process for root_path.
    void start(const std::string& root_path);

    // Pauses scanning. Threads block safely until resumed.
    void pause();

    // Resumes scanning.
    void resume();

    // Cancels scanning. Threads terminate as quickly as possible.
    void cancel();

    // Resets the scanner state, freeing the current tree.
    void reset();

    // Accessors: cheap, thread-safe reads of the scan state and counters
    // Accessors
    bool running() const { return m_running; }
    bool paused() const { return m_paused; }
    bool cancelled() const { return m_cancelled; }
    uint64_t files_scanned() const { return m_files_scanned; }
    uint64_t dirs_scanned() const { return m_dirs_scanned; }
    uint64_t bytes_scanned() const { return m_bytes_scanned; }
    /// Estimated progress, 0-100, based on bytes seen versus bytes used on the filesystem.
    double progress_percentage() const;
    uint64_t free_bytes() const { return m_free_bytes; }
    uint64_t total_capacity_bytes() const { return m_total_capacity_bytes; }
    dev_t root_dev() const { return m_root_dev; }
    
    /// The root of the scanned tree (owned by the scanner); null before a scan.
    TreeNode* root_node() { return m_root_node.get(); }

    /// Adds signed deltas to the totals, used after a file or folder is deleted.
    void adjust_stats(int64_t files_delta, int64_t dirs_delta, int64_t size_delta) {
        m_files_scanned += files_delta;
        m_dirs_scanned += dirs_delta;
        m_bytes_scanned += size_delta;
    }


    // Static test verification helper
    /// Built-in self-test that scans a small temporary folder and checks the totals.
    static void test_scanner();

signals:
    // Emitted periodically during the scan to update the GUI
    void progressUpdated(uint64_t files, uint64_t dirs, uint64_t bytes);
    // Emitted when scanning is complete (or cancelled)
    void finished();

private:
    /// Body of each worker thread: take a directory task, scan it, report progress.
    void worker_loop();
    /// Blocks until a directory task is available; returns false when the scan is over or cancelled.
    bool get_next_task(std::string& path, TreeNode*& parent_node);
    /// Queues a directory to be scanned (skipped when it is nested too deeply).
    void add_task(const std::string& path, TreeNode* parent_node);
    /// Reads one directory (implemented per platform); adds file nodes, queues subdirectories,
    /// and returns the file/folder counts and byte totals through the reference parameters.
    void scan_directory(const std::string& current_path, TreeNode* parent_node,
                         uint64_t& dir_files_size, uint64_t& dir_files_allocated,
                         uint64_t& dir_files_count, uint64_t& dir_subdirs_count);
    /// Declared but not defined or called anywhere in src/.
    void worker_finished();

    /// Identifies a directory by device and inode so a directory is never visited twice (cycle protection).
    struct DirId {
        dev_t dev;
        ino_t ino;
        bool operator<(const DirId& o) const {
            if (dev != o.dev) return dev < o.dev;
            return ino < o.ino;
        }
    };

    // Root node of the scanned directory tree
    // Root node of the scanned directory tree
    std::unique_ptr<TreeNode> m_root_node;

    // Queue management
    /// A pending unit of work: one directory and the tree node that represents it.
    struct Task {
        std::string path;
        TreeNode* parent_node;
    };
    std::vector<Task> m_task_queue;
    std::mutex m_queue_mutex;
    std::condition_variable m_queue_cv;
    std::condition_variable m_pause_cv;

    // Workers
    std::vector<std::thread> m_workers;
    // Scan state flags shared between the GUI thread and the workers
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_paused{false};
    std::atomic<bool> m_cancelled{false};
    // Number of workers currently busy; when it hits zero with an empty queue the scan is done
    std::atomic<int> m_active_workers{0};

    // Target filesystem device ID
    dev_t m_root_dev{0};

    // Cycle detection
    std::set<DirId> m_visited_dirs;
    std::mutex m_visited_mutex;

    // Progress stats
    std::atomic<uint64_t> m_files_scanned{0};
    std::atomic<uint64_t> m_dirs_scanned{0};
    std::atomic<uint64_t> m_bytes_scanned{0};
    std::atomic<uint64_t> m_total_target_bytes{0};
    std::atomic<uint64_t> m_total_capacity_bytes{0};
    std::atomic<uint64_t> m_free_bytes{0};
};
