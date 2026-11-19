// ==============================================================================
// Program: scanner.cpp
// Programmer: Ethan-da-Tech-Wizard
// Last Modified: November 18, 2026
// Purpose: Parallel multi-threaded filesystem crawler and disk space analyzer
//
// How it fits: MainWindow creates one Scanner, connects its progressUpdated /
// finished signals and calls start/pause/resume/cancel. Scanner builds the
// TreeNode tree (tree_node.hpp) that FileTreeModel then displays. The per-OS
// directory listing is `Scanner::scan_directory`, defined in
// src/platform/scanner_{linux,mac,win}.cpp; this file holds everything that is
// OS independent: the worker pool, queue, pause/cancel and progress maths.
//
// Concurrency model: N workers (one per hardware thread) share a LIFO task
// stack of directories. A directory is "done" when its worker returns from
// scan_directory; sub-directories it found were already pushed as new tasks.
// The scan ends when every worker is idle and the stack is empty
// (get_next_task); only then is finished() emitted.
//
// Known limits: directory depth is capped at 100 path separators; the crawl
// never leaves the starting filesystem; symlink loops are prevented by
// remembering (device, inode) pairs of visited directories.
// ==============================================================================
#include "scanner.hpp"
#include <filesystem>
#include <chrono>
#include <mutex>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>

// Fallback for platforms (notably MSVC) whose <sys/stat.h> lacks the S_ISDIR macro.
#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/resource.h>
#endif

/** Constructs an idle scanner; call start() to begin. */
Scanner::Scanner(QObject* parent) : QObject(parent) {}

/** Cancels and joins any running workers so no thread outlives the object. */
Scanner::~Scanner() {
    cancel();
}

/**
 * Cancels any running scan, then clears the tree, counters and visited set so
 * the object is ready for a fresh start(). Invalidates pointers obtained from
 * root_node().
 */
// Cancel any scan in progress, then clear the tree, counters and bookkeeping so
// the object can be reused for a new scan.
void Scanner::reset() {
    cancel();
    m_root_node.reset();
    m_files_scanned = 0;
    m_dirs_scanned = 0;
    m_bytes_scanned = 0;
    m_total_target_bytes = 0;
    m_total_capacity_bytes = 0;
    m_free_bytes = 0;
    m_root_dev = 0;
    {
        std::lock_guard<std::mutex> lock(m_visited_mutex);
        m_visited_dirs.clear();
    }
    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        m_task_queue.clear();
    }
}

/**
 * Starts scanning @p root_path with a pool of worker threads.
 *
 * Steps: reset previous state; create the root node; measure the volume
 * (used bytes for the progress denominator, capacity and free space for the
 * HUD); record the root's device/inode; queue the root directory; launch
 * workers. If the path is not a directory, finished() is emitted at once
 * and nothing is started.
 * @param root_path Directory to scan (relative paths are made absolute).
 * Returns immediately; the scan runs in the background and reports via the
 * progressUpdated and finished signals.
 */
// Starts a scan: reset, create the root node, measure the volume (for progress
// and the free-space display), seed the queue with the root directory and
// launch the worker threads. If the root is not a directory, finish at once.
void Scanner::start(const std::string& root_path) {
    reset();

    m_running = true;
    m_paused = false;
    m_cancelled = false;

    m_root_node = std::make_unique<TreeNode>(root_path, true);

    // absolute() can fail for odd paths; then the original string is the best we have.
    std::error_code ec;
    std::string abs_path = std::filesystem::absolute(root_path, ec).string();
    if (ec) {
        abs_path = root_path;
    }

    // Progress denominator: used space of the whole volume (blocks - free blocks),
    // because the real size of the tree is unknown until the scan ends. A scan of
    // a sub-folder therefore never reaches the 99 % cap.
    // Used space of the whole volume (blocks - free blocks) is the denominator for
    // progress_percentage(), since the true size of the tree is unknown until the
    // scan ends.
// Windows: GetDiskFreeSpaceExW. The path is widened byte-by-byte, which is
// only correct for ASCII paths.
#ifdef _WIN32
    // Convert path to wide string
    std::wstring wpath(abs_path.begin(), abs_path.end());
    ULARGE_INTEGER freeBytesAvailable, totalBytes, totalFreeBytes;
    if (GetDiskFreeSpaceExW(wpath.c_str(), &freeBytesAvailable, &totalBytes, &totalFreeBytes)) {
        m_total_target_bytes = totalBytes.QuadPart - totalFreeBytes.QuadPart;
        m_total_capacity_bytes = totalBytes.QuadPart;
        m_free_bytes = freeBytesAvailable.QuadPart;
    }
// POSIX: statvfs. f_frsize (fragment size) is the unit of the block counts;
// f_bavail (not f_bfree) is what an ordinary user can actually use.
#else
    struct statvfs vfs;
    if (statvfs(abs_path.c_str(), &vfs) == 0) {
        m_total_target_bytes = (vfs.f_blocks - vfs.f_bfree) * vfs.f_frsize;
        m_total_capacity_bytes = static_cast<uint64_t>(vfs.f_blocks) * vfs.f_frsize;
        m_free_bytes = vfs.f_bavail * vfs.f_frsize;
    }
#endif

    // Record the root's device id so the crawl stays on one filesystem (no
    // descending into other mounts), and seed the loop-detection set.
    // Record the root's device id (so the scan stays on one filesystem) and mark the
    // root as visited for loop detection.
    struct stat st;
    if (stat(abs_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        m_root_dev = st.st_dev;
        {
            std::lock_guard<std::mutex> lock(m_visited_mutex);
            m_visited_dirs.insert({st.st_dev, st.st_ino});
        }
        add_task(abs_path, m_root_node.get());
    // Not a directory (or unreadable): nothing to scan, finish immediately.
    } else {
        m_running = false;
        emit finished();
        return;
    }

    // Reserve after queueing the root: purely a performance hint for the scan burst.
    m_task_queue.reserve(4096);

    // Scanning is I/O bound and SSDs serve parallel reads well, so every logical
    // core is used. hardware_concurrency() may return 0; assume 4 then.
    unsigned int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4;

    // All workers start "active" (busy); get_next_task decrements as each goes idle.
    m_active_workers = num_threads;
    m_workers.reserve(num_threads);
    for (unsigned int i = 0; i < num_threads; ++i) {
        m_workers.emplace_back(&Scanner::worker_loop, this);
    }
}

/** Requests pause: workers block at the next directory boundary. */
void Scanner::pause() {
    m_paused = true;
}

/** Clears the pause flag and wakes workers waiting on m_pause_cv. */
void Scanner::resume() {
    m_paused = false;
    m_pause_cv.notify_all();
}

/**
 * Stops the scan: raises the cancel flag, wakes every waiting thread, joins all
 * workers and emits finished() ONLY if a scan was actually running. Also used
 * by reset() and the destructor, so it must be safe to call repeatedly.
 */
// Wake every waiting thread (pause and queue waits), join them all, and emit
// finished() only if a scan was actually running. Also called from the
// destructor and reset(), so it must be safe to call repeatedly.
void Scanner::cancel() {
    // Remember whether a scan was in progress before the flags change, so a
    // second cancel() on an idle scanner does not emit a spurious finished().
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

/**
 * Queues one directory for scanning.
 * @param path        Absolute path of the directory.
 * @param parent_node Tree node that will receive its contents.
 * Directories deeper than 100 path components are silently skipped.
 */
void Scanner::add_task(const std::string& path, TreeNode* parent_node) {
    // Depth guard: counting separators is a cheap proxy for nesting depth and
    // stops pathological or looping trees (deep bind mounts) from consuming
    // unbounded memory.
    constexpr size_t kMaxDepth = 100;
    size_t depth = 0;
    for (char c : path) {
        if (c == '/' || c == '\\') {
            depth++;
        }
    }
    if (depth > kMaxDepth) return;

    // Push then wake one sleeping worker; the lock keeps push_back and notify consistent.
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    m_task_queue.push_back({path, parent_node});
    m_queue_cv.notify_one();
}

/**
 * Hands the calling worker its next directory, blocking until one exists.
 *
 * A worker arriving here is idle, so it first decrements the active count. If
 * it was the last active worker and the queue is empty the scan is complete:
 * running is cleared, everyone is woken and finished() is emitted. Otherwise
 * it sleeps until work arrives or the scan is cancelled.
 * @param path        [out] Directory to scan.
 * @param parent_node [out] Tree node to fill.
 * @return false when the worker should exit (scan over or cancelled).
 */
// Worker rendezvous. A worker that comes here is by definition idle, so it
// decrements the active count first. If it was the last one and nothing is
// queued, the scan is complete: stop everyone and emit finished(). Otherwise it
// sleeps until work arrives or the scan is cancelled/stopped.
bool Scanner::get_next_task(std::string& path, TreeNode*& parent_node) {
    std::unique_lock<std::mutex> lock(m_queue_mutex);
    
    // Idle from now on.
    // Decrement active workers to indicate this thread is now waiting
    m_active_workers--;

    // Termination detection: nobody is working and nothing is queued, so no new
    // work can ever appear.
    // If all threads are waiting and queue is empty, the scan is finished
    if (m_active_workers == 0 && m_task_queue.empty()) {
        m_running = false;
        m_queue_cv.notify_all();
        emit finished();
        return false;
    }

    // Sleep until a task is pushed, the scan stops or is cancelled (re-checked in a loop
    // because condition variables can wake spuriously).
    while (m_task_queue.empty() && m_running && !m_cancelled) {
        m_queue_cv.wait(lock);
    }

    // Busy again.
    // Thread wakes up to execute a task, increment active workers
    m_active_workers++;

    // Woken because of cancellation or end of scan: undo the increment and exit.
    if (m_cancelled || !m_running || m_task_queue.empty()) {
        m_active_workers--; // Clean up thread exit state
        return false;
    }

    // LIFO: taking the newest directory keeps the scan depth-first, which keeps
    // the queue small and cache-friendly.
    auto task = m_task_queue.back();
    m_task_queue.pop_back();
    path = std::move(task.path);
    parent_node = task.parent_node;
    return true;
}

/**
 * Body of every worker thread: fetch a directory, list it, add its totals to
 * the tree and the global counters, throttle progress signals, repeat.
 */
// Lower the thread priority so the scan does not make the machine sluggish
// (below normal on Windows, nice 10 elsewhere).
void Scanner::worker_loop() {
// Lower priority so a full-speed scan does not make the rest of the machine sluggish.
#ifdef _WIN32
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#else
    setpriority(PRIO_PROCESS, 0, 10);
#endif

    std::string current_path;
    TreeNode* parent_node = nullptr;

    // Throttling state for progressUpdated signals.
    auto last_update = std::chrono::steady_clock::now();
    uint64_t files_since_update = 0;

    while (m_running && !m_cancelled) {
        // Pause gate: wait on the condition variable until resume() or cancel().
        if (m_paused) {
            std::unique_lock<std::mutex> pause_lock(m_queue_mutex);
            while (m_paused && !m_cancelled) {
                m_pause_cv.wait(pause_lock);
            }
        }

        // [[unlikely]]: cancellation is rare compared with the loop's normal path.
        if (m_cancelled) [[unlikely]] break;

        if (!get_next_task(current_path, parent_node)) {
            break;
        }

        // scan_directory fills these with the totals of the plain FILES directly in
        // the directory (sub-directories are queued as separate tasks).
        uint64_t dir_files_size = 0;
        uint64_t dir_files_allocated = 0;
        uint64_t dir_files_count = 0;
        uint64_t dir_subdirs_count = 0;

        try {
            scan_directory(current_path, parent_node, dir_files_size, dir_files_allocated, dir_files_count, dir_subdirs_count);

            // Add this directory's direct contents to its node and every ancestor, then
            // to the global counters that drive progress and the status bar. Skipped when
            // the directory was empty, to avoid pointless atomic traffic.
            if (dir_files_size > 0 || dir_files_allocated > 0 || dir_files_count > 0 || dir_subdirs_count > 0) {
                parent_node->propagate_stats(dir_files_size, dir_files_allocated, dir_files_count, dir_subdirs_count);
                
                m_files_scanned += dir_files_count;
                m_dirs_scanned += dir_subdirs_count;
                m_bytes_scanned += dir_files_size;
                
                files_since_update += dir_files_count + dir_subdirs_count;
            }
        // Never let an exception (e.g. out of memory, odd filesystem state) kill the
        // worker; that directory is simply skipped and the scan continues.
        } catch (...) {
        }

        // Progress throttle: emit at most every 100 ms, or sooner after 2000 entries,
        // so the GUI thread is not flooded with signals yet stays responsive.
        constexpr auto kUpdateInterval = std::chrono::milliseconds(100);
        constexpr uint64_t kUpdateBatchThreshold = 2000;
        auto now = std::chrono::steady_clock::now();
        if (now - last_update > kUpdateInterval || files_since_update > kUpdateBatchThreshold) {
            emit progressUpdated(m_files_scanned.load(), m_dirs_scanned.load(), m_bytes_scanned.load());
            files_since_update = 0;
            last_update = now;
        }
    }

    // One last update so the display shows the final numbers.
    emit progressUpdated(m_files_scanned.load(), m_dirs_scanned.load(), m_bytes_scanned.load());
}

#include <fstream>
#include <cassert>

/**
 * Scanned bytes as a percentage (0-99) of the volume's used bytes; 100 when no
 * scan is running. The 99 % cap means the bar never claims completion before
 * the scan really ends. For a sub-folder scan the denominator is the whole
 * volume, so the bar ends well below 99 %.
 */
double Scanner::progress_percentage() const {
    if (!m_running) return 100.0;
    uint64_t target = m_total_target_bytes.load();
    // Unknown denominator (statvfs failed): report 0 rather than dividing by zero.
    if (target == 0) return 0.0;
    double pct = (static_cast<double>(m_bytes_scanned.load()) / target) * 99.0;
    if (pct > 99.0) pct = 99.0;
    return pct;
}

/**
 * Self-test run by `spacemap --test-only` (asserts are active only in non-NDEBUG builds).
 * Creates <tmp>/spacemap_test_scan_temp/{file1.txt (5 B), sub1/file2.txt (10 B),
 * sub1/sub2/file3.txt (1 B)}, scans it and checks 3 files, 2 dirs and 16 bytes
 * (root totals too), then deletes the folder.
 */
void Scanner::test_scanner() {
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "spacemap_test_scan_temp";
    std::filesystem::create_directories(test_dir / "sub1" / "sub2");

    // Fixture: three files in a two-level directory tree with known sizes.
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

    // Busy-wait until the background scan has finished.
    while (scanner.running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Counters set by the scan must equal what was written above.
    assert(scanner.files_scanned() == 3);
    assert(scanner.dirs_scanned() == 2);
    assert(scanner.bytes_scanned() == 16);

    // The root node's aggregates must match the global counters.
    TreeNode* root = scanner.root_node();
    assert(root != nullptr);
    assert(root->size == 16);
    assert(root->file_count == 3);
    assert(root->dir_count == 2);

    // Remove the fixture; runs after the asserts, so a failure in a debug build aborts before cleanup.
    std::filesystem::remove_all(test_dir);

    std::cout << "Scanner tests passed successfully!" << "\n";
}
