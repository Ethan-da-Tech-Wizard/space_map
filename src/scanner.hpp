// ==============================================================================
// scanner.hpp - declaration of Scanner, the multi-threaded disk crawler.
//
// Scanner walks a directory tree with a pool of worker threads, builds a
// TreeNode tree holding per-directory sizes and counts, and reports progress
// to the GUI through Qt signals. The per-OS directory enumeration
// (scan_directory) is implemented separately in src/platform/scanner_*.cpp;
// everything else is in scanner.cpp.
//
// Threading model: start() launches one worker per hardware thread. Workers
// pull directories from a shared task stack (m_task_queue); scanning a
// directory enqueues its sub-directories. pause/resume/cancel are cooperative
// and checked between directories and between directory entries.
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

// QObject base is needed for the progressUpdated/finished signals. Signals are
// emitted from worker threads, so receivers should use queued connections
// (Qt's default for cross-thread connections).
class Scanner : public QObject {
    Q_OBJECT
public:
    explicit Scanner(QObject* parent = nullptr);
    // Destructor cancels and joins all workers so no thread outlives the object.
    ~Scanner() override;

    // Starts the multi-threaded scanning process for root_path.
    void start(const std::string& root_path);

    // Pauses scanning. Threads block safely until resumed.
    // Sets a flag only: workers block at the next directory boundary.
    void pause();

    // Resumes scanning.
    void resume();

    // Cancels scanning. Threads terminate as quickly as possible.
    // Stops workers, joins them and emits finished() if a scan was in progress.
    void cancel();

    // Resets the scanner state, freeing the current tree.
    void reset();

    // Accessors
    // Accessors are safe to call from any thread: the underlying counters are
    // std::atomic.
    bool running() const { return m_running; }
    bool paused() const { return m_paused; }
    bool cancelled() const { return m_cancelled; }
    uint64_t files_scanned() const { return m_files_scanned; }
    uint64_t dirs_scanned() const { return m_dirs_scanned; }
    uint64_t bytes_scanned() const { return m_bytes_scanned; }
    // Estimated progress, 0-100. See scanner.cpp for how it is derived.
    double progress_percentage() const;
    uint64_t free_bytes() const { return m_free_bytes; }
    uint64_t total_capacity_bytes() const { return m_total_capacity_bytes; }
    // Device id of the scan root; the crawl never crosses onto another device.
    dev_t root_dev() const { return m_root_dev; }
    
    // Ownership stays with the Scanner; the pointer is invalidated by reset()/start().
    TreeNode* root_node() { return m_root_node.get(); }

    // Lets the GUI keep the global counters consistent after it removes or edits
    // nodes (deltas may be negative).
    void adjust_stats(int64_t files_delta, int64_t dirs_delta, int64_t size_delta) {
        m_files_scanned += files_delta;
        m_dirs_scanned += dirs_delta;
        m_bytes_scanned += size_delta;
    }


    // Static test verification helper
    // Self-test that scans a small temporary directory and asserts the totals.
    static void test_scanner();

signals:
    // Emitted periodically during the scan to update the GUI
    void progressUpdated(uint64_t files, uint64_t dirs, uint64_t bytes);
    // Emitted when scanning is complete (or cancelled)
    void finished();

private:
    // Body of each worker thread.
    void worker_loop();
    // Blocks until a directory is available; returns false when the scan is over
    // (cancelled, or every worker is idle and the queue is empty).
    bool get_next_task(std::string& path, TreeNode*& parent_node);
    void add_task(const std::string& path, TreeNode* parent_node);
    // Platform-specific: lists one directory, creates TreeNodes for its entries,
    // queues sub-directories and returns the totals for the plain files it found
    // (sizes in logical and allocated bytes, file count, sub-directory count).
    void scan_directory(const std::string& current_path, TreeNode* parent_node,
                         uint64_t& dir_files_size, uint64_t& dir_files_allocated,
                         uint64_t& dir_files_count, uint64_t& dir_subdirs_count);
    void worker_finished();

    // Identity of a directory (device + inode) used for loop detection; ordered so
    // it can live in a std::set.
    struct DirId {
        dev_t dev;
        ino_t ino;
        bool operator<(const DirId& o) const {
            if (dev != o.dev) return dev < o.dev;
            return ino < o.ino;
        }
    };

    // Root node of the scanned directory tree
    std::unique_ptr<TreeNode> m_root_node;

    // Queue management
    // One unit of work: a directory path plus the tree node it fills in.
    struct Task {
        std::string path;
        TreeNode* parent_node;
    };
    // Used as a stack (LIFO), which tends to keep the scan depth-first and the
    // queue small. Guarded by m_queue_mutex.
    std::vector<Task> m_task_queue;
    std::mutex m_queue_mutex;
    std::condition_variable m_queue_cv;
    std::condition_variable m_pause_cv;

    // Workers
    std::vector<std::thread> m_workers;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_paused{false};
    std::atomic<bool> m_cancelled{false};
    // Number of workers currently scanning (not waiting for work); when it drops to
    // zero with an empty queue, the scan has finished.
    std::atomic<int> m_active_workers{0};

    // Target filesystem device ID
    dev_t m_root_dev{0};

    // Cycle detection
    // Directories already queued; prevents revisiting through bind mounts or loops.
    std::set<DirId> m_visited_dirs;
    std::mutex m_visited_mutex;

    // Progress stats
    // Progress counters. m_total_target_bytes is the used space of the partition
    // (denominator for progress); capacity/free are shown in the UI.
    std::atomic<uint64_t> m_files_scanned{0};
    std::atomic<uint64_t> m_dirs_scanned{0};
    std::atomic<uint64_t> m_bytes_scanned{0};
    std::atomic<uint64_t> m_total_target_bytes{0};
    std::atomic<uint64_t> m_total_capacity_bytes{0};
    std::atomic<uint64_t> m_free_bytes{0};
};
