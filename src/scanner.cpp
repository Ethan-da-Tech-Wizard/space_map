#include "scanner.hpp"
#include <filesystem>
#include <chrono>
#include <mutex>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/resource.h>
#endif

Scanner::Scanner(QObject* parent) : QObject(parent) {}

Scanner::~Scanner() {
    cancel();
}

void Scanner::reset() {
    cancel();
    m_root_node.reset();
    m_files_scanned = 0;
    m_dirs_scanned = 0;
    m_bytes_scanned = 0;
    m_total_target_bytes = 0;
    m_free_bytes = 0;
    {
        std::lock_guard<std::mutex> lock(m_visited_mutex);
        m_visited_dirs.clear();
    }
    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        m_task_queue.clear();
    }
}

void Scanner::start(const std::string& root_path) {
    reset();

    m_running = true;
    m_paused = false;
    m_cancelled = false;

    m_root_node = std::make_unique<TreeNode>(root_path, true);

    // Get absolute path
    std::error_code ec;
    std::string abs_path = std::filesystem::absolute(root_path, ec).string();
    if (ec) {
        abs_path = root_path;
    }

    // Measure the total used bytes of the filesystem partition to calculate accurate progress
#ifdef _WIN32
    // Convert path to wide string
    std::wstring wpath(abs_path.begin(), abs_path.end());
    ULARGE_INTEGER freeBytesAvailable, totalBytes, totalFreeBytes;
    if (GetDiskFreeSpaceExW(wpath.c_str(), &freeBytesAvailable, &totalBytes, &totalFreeBytes)) {
        m_total_target_bytes = totalBytes.QuadPart - totalFreeBytes.QuadPart;
        m_free_bytes = freeBytesAvailable.QuadPart;
    }
#else
    struct statvfs vfs;
    if (statvfs(abs_path.c_str(), &vfs) == 0) {
        m_total_target_bytes = (vfs.f_blocks - vfs.f_bfree) * vfs.f_frsize;
        m_free_bytes = vfs.f_bavail * vfs.f_frsize;
    }
#endif

    struct stat st;
    if (stat(abs_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        {
            std::lock_guard<std::mutex> lock(m_visited_mutex);
            m_visited_dirs.insert({st.st_dev, st.st_ino});
        }
        add_task(abs_path, m_root_node.get());
    } else {
        m_running = false;
        emit finished();
        return;
    }

    unsigned int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4;

    m_active_workers = num_threads;
    for (unsigned int i = 0; i < num_threads; ++i) {
        m_workers.emplace_back(&Scanner::worker_loop, this);
    }
}

void Scanner::pause() {
    m_paused = true;
}

void Scanner::resume() {
    m_paused = false;
    m_pause_cv.notify_all();
}

void Scanner::cancel() {
    bool was_running = m_running;
    m_cancelled = true;
    m_paused = false;
    m_pause_cv.notify_all();
    m_queue_cv.notify_all();

    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    m_workers.clear();
    m_running = false;

    if (was_running) {
        emit finished();
    }
}

void Scanner::add_task(const std::string& path, TreeNode* parent_node) {
    // Security check: limit directory traversal depth to 100 to prevent stack/OOM loop exploits
    size_t depth = 0;
    for (char c : path) {
        if (c == '/' || c == '\\') {
            depth++;
        }
    }
    if (depth > 100) return;

    std::lock_guard<std::mutex> lock(m_queue_mutex);
    m_task_queue.push_back({path, parent_node});
    m_queue_cv.notify_one();
}

bool Scanner::get_next_task(std::string& path, TreeNode*& parent_node) {
    std::unique_lock<std::mutex> lock(m_queue_mutex);
    
    // Decrement active workers to indicate this thread is now waiting
    m_active_workers--;

    // If all threads are waiting and queue is empty, the scan is finished
    if (m_active_workers == 0 && m_task_queue.empty()) {
        m_running = false;
        m_queue_cv.notify_all();
        emit finished();
        return false;
    }

    while (m_task_queue.empty() && m_running && !m_cancelled) {
        m_queue_cv.wait(lock);
    }

    // Thread wakes up to execute a task, increment active workers
    m_active_workers++;

    if (m_cancelled || !m_running || m_task_queue.empty()) {
        m_active_workers--; // Clean up thread exit state
        return false;
    }

    auto task = m_task_queue.back();
    m_task_queue.pop_back();
    path = std::move(task.path);
    parent_node = task.parent_node;
    return true;
}

void Scanner::worker_loop() {
#ifdef _WIN32
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#else
    setpriority(PRIO_PROCESS, 0, 10);
#endif

    std::string current_path;
    TreeNode* parent_node = nullptr;

    auto last_update = std::chrono::steady_clock::now();
    uint64_t files_since_update = 0;

    while (m_running && !m_cancelled) {
        // Handle Pause
        if (m_paused) {
            std::unique_lock<std::mutex> pause_lock(m_queue_mutex);
            while (m_paused && !m_cancelled) {
                m_pause_cv.wait(pause_lock);
            }
        }

        if (m_cancelled) [[unlikely]] break;

        if (!get_next_task(current_path, parent_node)) {
            break;
        }

        uint64_t dir_files_size = 0;
        uint64_t dir_files_allocated = 0;
        uint64_t dir_files_count = 0;
        uint64_t dir_subdirs_count = 0;

        try {
            scan_directory(current_path, parent_node, dir_files_size, dir_files_allocated, dir_files_count, dir_subdirs_count);

            if (dir_files_size > 0 || dir_files_allocated > 0 || dir_files_count > 0 || dir_subdirs_count > 0) {
                parent_node->propagate_stats(dir_files_size, dir_files_allocated, dir_files_count, dir_subdirs_count);
                
                m_files_scanned += dir_files_count;
                m_dirs_scanned += dir_subdirs_count;
                m_bytes_scanned += dir_files_size;
                
                files_since_update += dir_files_count + dir_subdirs_count;
            }
        } catch (...) {
            // Fail-safe to keep thread running
        }

        // Periodically emit progress updates to the main thread
        auto now = std::chrono::steady_clock::now();
        if (now - last_update > std::chrono::milliseconds(100) || files_since_update > 2000) {
            emit progressUpdated(m_files_scanned.load(), m_dirs_scanned.load(), m_bytes_scanned.load());
            files_since_update = 0;
            last_update = now;
        }
    }

    // Final clean update
    emit progressUpdated(m_files_scanned.load(), m_dirs_scanned.load(), m_bytes_scanned.load());
}

#include <fstream>
#include <cassert>

double Scanner::progress_percentage() const {
    if (!m_running) return 100.0;
    uint64_t target = m_total_target_bytes.load();
    if (target == 0) return 0.0;
    double pct = (static_cast<double>(m_bytes_scanned.load()) / target) * 99.0;
    if (pct > 99.0) pct = 99.0;
    return pct;
}

void Scanner::test_scanner() {
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "spacemap_test_scan_temp";
    std::filesystem::create_directories(test_dir / "sub1" / "sub2");

    // Write dummy files
    {
        std::ofstream f1(test_dir / "file1.txt");
        f1 << "12345"; // 5 bytes
    }
    {
        std::ofstream f2(test_dir / "sub1" / "file2.txt");
        f2 << "1234567890"; // 10 bytes
    }
    {
        std::ofstream f3(test_dir / "sub1" / "sub2" / "file3.txt");
        f3 << "1"; // 1 byte
    }

    Scanner scanner;
    scanner.start(test_dir.string());

    // Wait for the scan threads to complete
    while (scanner.running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Verify statistics
    assert(scanner.files_scanned() == 3);
    assert(scanner.dirs_scanned() == 2);
    assert(scanner.bytes_scanned() == 16);

    TreeNode* root = scanner.root_node();
    assert(root != nullptr);
    assert(root->size == 16);
    assert(root->file_count == 3);
    assert(root->dir_count == 2);

    // Clean up files
    std::filesystem::remove_all(test_dir);

    std::cout << "Scanner tests passed successfully!" << std::endl;
}
