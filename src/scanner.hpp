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

class Scanner : public QObject {
    Q_OBJECT
public:
    explicit Scanner(QObject* parent = nullptr);
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

    // Accessors
    bool running() const { return m_running; }
    bool paused() const { return m_paused; }
    bool cancelled() const { return m_cancelled; }
    uint64_t files_scanned() const { return m_files_scanned; }
    uint64_t dirs_scanned() const { return m_dirs_scanned; }
    uint64_t bytes_scanned() const { return m_bytes_scanned; }
    double progress_percentage() const;
    uint64_t free_bytes() const { return m_free_bytes; }
    
    TreeNode* root_node() { return m_root_node.get(); }

    void adjust_stats(int64_t files_delta, int64_t dirs_delta, int64_t size_delta) {
        m_files_scanned += files_delta;
        m_dirs_scanned += dirs_delta;
        m_bytes_scanned += size_delta;
    }


    // Static test verification helper
    static void test_scanner();

signals:
    // Emitted periodically during the scan to update the GUI
    void progressUpdated(uint64_t files, uint64_t dirs, uint64_t bytes);
    // Emitted when scanning is complete (or cancelled)
    void finished();

private:
    void worker_loop();
    bool get_next_task(std::string& path, TreeNode*& parent_node);
    void add_task(const std::string& path, TreeNode* parent_node);
    void scan_directory(const std::string& current_path, TreeNode* parent_node,
                         uint64_t& dir_files_size, uint64_t& dir_files_allocated,
                         uint64_t& dir_files_count, uint64_t& dir_subdirs_count);
    void worker_finished();

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
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_paused{false};
    std::atomic<bool> m_cancelled{false};
    std::atomic<int> m_active_workers{0};

    // Cycle detection
    std::set<DirId> m_visited_dirs;
    std::mutex m_visited_mutex;

    // Progress stats
    std::atomic<uint64_t> m_files_scanned{0};
    std::atomic<uint64_t> m_dirs_scanned{0};
    std::atomic<uint64_t> m_bytes_scanned{0};
    std::atomic<uint64_t> m_total_target_bytes{0};
    std::atomic<uint64_t> m_free_bytes{0};
};
