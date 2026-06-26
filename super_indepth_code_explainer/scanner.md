# SpaceMap Architecture Book: Chapter 4

This chapter provides a complete line-by-line, character-by-character architectural explanation of the Multi-Threaded Scanning Engine (`src/scanner.hpp` and `src/scanner.cpp`).

---

# CHAPTER 4: Multi-Threaded Scanning Engine & Task Synchronization

Scanning a modern solid-state drive containing millions of files requires massive parallelism. SpaceMap implements a high-performance thread-pool architecture using standard C++ threads (`std::thread`), mutexes (`std::mutex`), condition variables (`std::condition_variable`), and atomic variables (`std::atomic`). The coordinator class, `Scanner`, manages this workforce and synchronizes progress back to the Qt graphical user interface.

---

## 1. src/scanner.hpp (Header File Breakdown)

The header file defines the layout of the `Scanner` class in memory, its signal/slot mechanisms, and private structures.

```cpp
1: #pragma once
```
* **Explanation**: Preprocessor header guard. It instructs the compiler to process this header file only once per translation unit, preventing duplicate declaration errors.

```cpp
2: #include <QObject>
3: #include <string>
4: #include <vector>
5: #include <thread>
6: #include <mutex>
7: #include <condition_variable>
8: #include <atomic>
9: #include <set>
10: #include <sys/types.h>
11: #include <sys/stat.h>
12: #include <sys/statvfs.h>
13: #include "tree_node.hpp"
```
* **Explanation**: Imports external dependencies:
  * `<QObject>`: Exposes the `QObject` base class, enabling Qt metadata features like signals, slots, and object trees.
  * `<string>`: Exposes `std::string` for dynamic path string storage.
  * `<vector>`: Exposes `std::vector` for task queuing and worker thread tracking.
  * `<thread>`: Exposes `std::thread` to spawn concurrent OS-level threads.
  * `<mutex>`: Exposes mutual exclusion variables to prevent data races.
  * `<condition_variable>`: Exposes `std::condition_variable` to block/wake threads efficiently.
  * `<atomic>`: Exposes CPU-native lock-free mathematical variables.
  * `<set>`: Exposes `std::set` (red-black tree search structure) for tracking visited inodes.
  * `<sys/types.h>` and `<sys/stat.h>`: Exposes system types like `dev_t` (device IDs) and `ino_t` (inode IDs) to identify filesystem loops.
  * `<sys/statvfs.h>`: Exposes `statvfs` to query partition disk space statistics.
  * `"tree_node.hpp"`: Includes our thread-safe `TreeNode` structure so the scanner can construct the file tree hierarchy.

```cpp
15: class Scanner : public QObject {
16:     Q_OBJECT
```
* **Explanation**:
  * `class Scanner : public QObject`: Defines the `Scanner` class inheriting from Qt's `QObject`.
  * `Q_OBJECT`: The crucial Qt preprocessor macro. When Qt's build tool (moc - Meta-Object Compiler) parses this file, this macro triggers the generation of C++ code that implements signals, slots, dynamic properties, and run-time type information (RTTI) for the class.

```cpp
17: public:
18:     explicit Scanner(QObject* parent = nullptr);
```
* **Explanation**: The class constructor.
  * `explicit`: Prevents the compiler from implicitly converting a parent pointer to a `Scanner` instance (avoiding type conversion bugs).
  * `QObject* parent = nullptr`: Accepts an optional parent object pointer. In Qt, if a parent object is deleted, it automatically deletes all of its child objects, preventing memory leaks.

```cpp
19:     ~Scanner() override;
```
* **Explanation**: Destructor. Declared with `override` to ensure the compiler checks that it correctly overrides the base `QObject` virtual destructor. Inside, it cancels any running threads to prevent zombie processes.

```cpp
21:     // Starts the multi-threaded scanning process for root_path.
22:     void start(const std::string& root_path);
```
* **Explanation**: Public method called by the user interface to initiate scanning. It allocates the root tree node and launches the worker threads.

```cpp
24:     // Pauses scanning. Threads block safely until resumed.
25:     void pause();
```
* **Explanation**: Sets the paused atomic flag, instructing worker threads to halt I/O traversal and sleep.

```cpp
27:     // Resumes scanning.
28:     void resume();
```
* **Explanation**: Clears the paused flag and signals the threads to wake up.

```cpp
30:     // Cancels scanning. Threads terminate as quickly as possible.
31:     void cancel();
```
* **Explanation**: Sets the cancelled flag, wakes up all sleeping threads, and blocks (joins) until all threads have exited cleanly.

```cpp
33:     // Resets the scanner state, freeing the current tree.
34:     void reset();
```
* **Explanation**: Resets internal atomic counters, releases the memory-mapped file tree, and clears task lists.

```cpp
36:     // Accessors
37:     bool running() const { return m_running; }
38:     bool paused() const { return m_paused; }
39:     bool cancelled() const { return m_cancelled; }
```
* **Explanation**: Inline getter methods returning the current states. They are marked `const` because they do not modify any class variables.

```cpp
40:     uint64_t files_scanned() const { return m_files_scanned; }
41:     uint64_t dirs_scanned() const { return m_dirs_scanned; }
42:     uint64_t bytes_scanned() const { return m_bytes_scanned; }
```
* **Explanation**: Return the current counts of files, directories, and total byte sizes read so far.

```cpp
43:     double progress_percentage() const;
```
* **Explanation**: Method to calculate and return the progress ratio (from 0.0 to 100.0) compared to used disk space on the partition.

```cpp
44:     uint64_t free_bytes() const { return m_free_bytes; }
```
* **Explanation**: Returns the available free bytes on the partition.

```cpp
46:     TreeNode* root_node() { return m_root_node.get(); }
```
* **Explanation**: Returns the raw pointer to the root of the file tree. `get()` extracts the raw address from the owning `std::unique_ptr` without transferring ownership.

```cpp
48:     void adjust_stats(int64_t files_delta, int64_t dirs_delta, int64_t size_delta) {
49:         m_files_scanned += files_delta;
50:         m_dirs_scanned += dirs_delta;
51:         m_bytes_scanned += size_delta;
52:     }
```
* **Explanation**: Thread-safe delta arithmetic helper. When a user deletes a file/folder, the UI calls this to subtract the deleted counts from the global scanner statistics, keeping the HUD dashboard synchronized.

```cpp
56:     static void test_scanner();
```
* **Explanation**: Verification function that tests the scanner against a temporary directory structure during project testing.

```cpp
58: signals:
59:     // Emitted periodically during the scan to update the GUI
60:     void progressUpdated(uint64_t files, uint64_t dirs, uint64_t bytes);
61:     // Emitted when scanning is complete (or cancelled)
62:     void finished();
```
* **Explanation**:
  * `signals:`: Qt keyword specifying that the following functions are signals. Signals have no function bodies; they are implemented by the Meta-Object Compiler.
  * `progressUpdated(...)`: Emitted periodically to push scanning numbers to the main thread GUI without blocking the scanning threads.
  * `finished()`: Emitted when the task queue is exhausted or the scan is cancelled, notifying the UI to stop timers and enable controls.

```cpp
64: private:
65:     void worker_loop();
```
* **Explanation**: Private method run by each thread in the thread pool. It fetches tasks from the queue and scans subdirectories in a loop.

```cpp
66:     bool get_next_task(std::string& path, TreeNode*& parent_node);
```
* **Explanation**: A thread-safe, blocking method to pop a directory scan task from the queue. Returns `false` if the scan has finished or cancelled.

```cpp
67:     void add_task(const std::string& path, TreeNode* parent_node);
```
* **Explanation**: Pushes a new subdirectory scan task onto the queue and notifies a waiting thread.

```cpp
68:     void scan_directory(const std::string& current_path, TreeNode* parent_node,
69:                          uint64_t& dir_files_size, uint64_t& dir_files_allocated,
70:                          uint64_t& dir_files_count, uint64_t& dir_subdirs_count);
```
* **Explanation**: Core low-level directory traversal function. Its implementation is platform-specific (Linux, macOS, and Windows have their own implementations in `src/platform/`).

```cpp
71:     void worker_finished();
```
* **Explanation**: Currently unused hook, but can be used to track individual worker terminations.

```cpp
73:     struct DirId {
74:         dev_t dev;
75:         ino_t ino;
76:         bool operator<(const DirId& o) const {
77:             if (dev != o.dev) return dev < o.dev;
78:             return ino < o.ino;
79:         }
80:     };
```
* **Explanation**:
  * `struct DirId`: Uniquely identifies a directory across the system.
    * `dev_t dev`: Device ID of the disk storage partition containing the folder.
    * `ino_t ino`: Inode ID (unique file/folder index number) within that partition.
  * `bool operator<(const DirId& o) const`: The less-than comparator operator overload. Since we store visited folders in a `std::set` (which maintains ordering), we must define how to compare two `DirId` structs. It sorts first by device, and then by inode.

```cpp
83:     std::unique_ptr<TreeNode> m_root_node;
```
* **Explanation**: The smart pointer that owns the memory of the root `TreeNode` of the scanned tree. When this unique pointer is reset or reassigned, it cleans up all subnodes automatically.

```cpp
86:     struct Task {
87:         std::string path;
88:         TreeNode* parent_node;
89:     };
```
* **Explanation**: Defines a unit of scanning work.
  * `path`: Absolute filesystem path to scan (e.g. `/home/user/Downloads`).
  * `parent_node`: Raw pointer to the `TreeNode` inside the tree where discovered files/folders should be attached.

```cpp
90:     std::vector<Task> m_task_queue;
```
* **Explanation**: Dynamic array acting as the scan task queue. Worker threads pop tasks from the back (`m_task_queue.pop_back()`) to process them.

```cpp
91:     std::mutex m_queue_mutex;
```
* **Explanation**: Mutual exclusion lock protecting the `m_task_queue` and active worker counts. Since multiple threads push and pop from the queue, this lock prevents corruption of the internal vector structure.

```cpp
92:     std::condition_variable m_queue_cv;
```
* **Explanation**: Condition variable used to coordinate threads. When the queue is empty, worker threads call `m_queue_cv.wait()` to sleep, putting the CPU to sleep instead of busy-waiting. When a new task is pushed, `m_queue_cv.notify_one()` wakes up one worker.

```cpp
93:     std::condition_variable m_pause_cv;
```
* **Explanation**: Condition variable used to pause worker threads. When the user clicks "Pause", workers block on this condition variable until the user clicks "Resume".

```cpp
96:     std::vector<std::thread> m_workers;
```
* **Explanation**: Array holding the handle to each thread spawned by the scanner. Used to join threads during cancellation or completion.

```cpp
97:     std::atomic<bool> m_running{false};
98:     std::atomic<bool> m_paused{false};
99:     std::atomic<bool> m_cancelled{false};
```
* **Explanation**: State control flags. They are wrapped in `std::atomic` to ensure that checks and modifications are thread-safe and visible across threads without standard mutex locking overhead.

```cpp
100:     std::atomic<int> m_active_workers{0};
```
* **Explanation**: Tracks the number of worker threads currently processing a directory. When this count drops to 0 and the task queue is empty, the entire scanning job is complete.

```cpp
103:     std::set<DirId> m_visited_dirs;
104:     std::mutex m_visited_mutex;
```
* **Explanation**:
  * `std::set<DirId> m_visited_dirs`: Set containing the IDs of all directories traversed during the scan. Used to detect and block recursive symlink cycles, bind mounts, and recursive directory loops.
  * `std::mutex m_visited_mutex`: Exclusive lock protecting the visited directory set.

```cpp
107:     std::atomic<uint64_t> m_files_scanned{0};
108:     std::atomic<uint64_t> m_dirs_scanned{0};
109:     std::atomic<uint64_t> m_bytes_scanned{0};
110:     std::atomic<uint64_t> m_total_target_bytes{0};
111:     std::atomic<uint64_t> m_free_bytes{0};
```
* **Explanation**: Atomic counters tracking total progress. `m_total_target_bytes` holds the partition size used bytes, which serves as the reference point for progress calculations.

---

## 2. src/scanner.cpp (Source File Breakdown)

The source file implements the coordinator, queue management, thread synchronization, progress calculation, and unit verification.

```cpp
10: Scanner::Scanner(QObject* parent) : QObject(parent) {}
```
* **Explanation**: Class constructor. Delegates the parent pointer initialization to the base `QObject`.

```cpp
12: Scanner::~Scanner() {
13:     cancel();
14: }
```
* **Explanation**: Destructor. It calls `cancel()` to ensure that if the GUI is closed while a scan is active, all worker threads are safely terminated and joined before memory destruction begins, avoiding segfaults.

```cpp
16: void Scanner::reset() {
17:     cancel();
18:     m_root_node.reset();
```
* **Explanation**:
  * `cancel()`: Terminates active scanning.
  * `m_root_node.reset()`: Resets the root unique pointer, deleting the entire hierarchy and freeing all memory.

```cpp
19:     m_files_scanned = 0;
20:     m_dirs_scanned = 0;
21:     m_bytes_scanned = 0;
22:     m_total_target_bytes = 0;
23:     m_free_bytes = 0;
```
* **Explanation**: Resets atomic metrics back to zero.

```cpp
24:     {
25:         std::lock_guard<std::mutex> lock(m_visited_mutex);
26:         m_visited_dirs.clear();
27:     }
```
* **Explanation**:
  * `std::lock_guard<std::mutex> lock(...)`: Acquires the mutex for the scope. RAII pattern releases it when the closing brace `}` is reached.
  * `m_visited_dirs.clear()`: Empties the visited directories set.

```cpp
28:     {
29:         std::lock_guard<std::mutex> lock(m_queue_mutex);
30:         m_task_queue.clear();
31:     }
32: }
```
* **Explanation**: Empties the task queue under its corresponding queue lock.

```cpp
34: void Scanner::start(const std::string& root_path) {
35:     reset();
```
* **Explanation**: Starts a scan. First calls `reset()` to ensure a clean slate and abort any pre-existing scanning tasks.

```cpp
37:     m_running = true;
38:     m_paused = false;
39:     m_cancelled = false;
```
* **Explanation**: Sets the scanner's main state variables.

```cpp
41:     m_root_node = std::make_unique<TreeNode>(root_path, true);
```
* **Explanation**: Allocates a new root `TreeNode` on the heap for our scan tree, using the starting root path as its name.

```cpp
44:     std::error_code ec;
45:     std::string abs_path = std::filesystem::absolute(root_path, ec).string();
46:     if (ec) {
47:         abs_path = root_path;
48:     }
```
* **Explanation**:
  * Converts the input path to an absolute path using C++17 `std::filesystem::absolute`.
  * `std::error_code ec`: Captures any path errors without throwing exceptions. If resolving fails, it falls back to the original root path.

```cpp
50:     // Measure the total used bytes of the filesystem partition to calculate accurate progress
51:     struct statvfs vfs;
52:     if (statvfs(abs_path.c_str(), &vfs) == 0) {
53:         m_total_target_bytes = (vfs.f_blocks - vfs.f_bfree) * vfs.f_frsize;
54:         m_free_bytes = vfs.f_bavail * vfs.f_frsize;
55:     }
```
* **Explanation**:
  * `struct statvfs vfs`: Allocates a structure to hold filesystem stats.
  * `statvfs(...)`: POSIX call to query filesystem details.
  * `m_total_target_bytes = (vfs.f_blocks - vfs.f_bfree) * vfs.f_frsize`: Computes total used bytes on the drive partition ($(\text{total blocks} - \text{free blocks}) \times \text{fragment size}$). This is our scanning baseline.
  * `m_free_bytes = vfs.f_bavail * vfs.f_frsize`: Computes available partition bytes.

```cpp
57:     struct stat st;
58:     if (stat(abs_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
59:         {
60:             std::lock_guard<std::mutex> lock(m_visited_mutex);
61:             m_visited_dirs.insert({st.st_dev, st.st_ino});
62:         }
63:         add_task(abs_path, m_root_node.get());
```
* **Explanation**:
  * `stat(abs_path.c_str(), &st)`: Gets standard file information for the absolute root path.
  * `S_ISDIR(st.st_mode)`: Verifies if the root path is indeed a directory.
  * `m_visited_dirs.insert({st.st_dev, st.st_ino})`: Inserts the root directory's device and inode number to initialize cycle detection.
  * `add_task(...)`: Adds the initial scanning task (the root path) to the task queue.

```cpp
64:     } else {
65:         m_running = false;
66:         emit finished();
67:         return;
68:     }
```
* **Explanation**: If the root path does not exist or is not a directory, sets `m_running = false`, emits the `finished()` signal immediately, and exits.

```cpp
70:     unsigned int num_threads = std::thread::hardware_concurrency();
71:     if (num_threads == 0) num_threads = 4;
```
* **Explanation**:
  * `std::thread::hardware_concurrency()`: Queries the CPU's available hardware thread count (cores/logical processors).
  * If the system cannot determine the count (returns 0), we default to 4 threads.

```cpp
73:     m_active_workers = num_threads;
74:     for (unsigned int i = 0; i < num_threads; ++i) {
75:         m_workers.emplace_back(&Scanner::worker_loop, this);
76:     }
77: }
```
* **Explanation**:
  * Sets the active worker atomic counter to the thread count.
  * `m_workers.emplace_back(...)`: Spawns logical workers. The function `Scanner::worker_loop` is executed on each thread, passing `this` (the scanner instance pointer) as the class context.

```cpp
79: void Scanner::pause() {
80:     m_paused = true;
81: }
```
* **Explanation**: Sets `m_paused` to `true`. Worker threads check this flag before executing tasks.

```cpp
83: void Scanner::resume() {
84:     m_paused = false;
85:     m_pause_cv.notify_all();
86: }
```
* **Explanation**:
  * Sets `m_paused` to `false`.
  * `m_pause_cv.notify_all()`: Wakes up all worker threads waiting on the pause condition variable, resuming traversal.

```cpp
88: void Scanner::cancel() {
89:     bool was_running = m_running;
90:     m_cancelled = true;
91:     m_paused = false;
```
* **Explanation**: Sets the cancelled flag, clears the paused flag, and signals termination.

```cpp
92:     m_pause_cv.notify_all();
93:     m_queue_cv.notify_all();
```
* **Explanation**: Wakes up all sleeping worker threads, whether they are waiting for tasks or are paused. Waking them allows them to check the `m_cancelled` flag and exit cleanly.

```cpp
95:     for (auto& worker : m_workers) {
96:         if (worker.joinable()) {
97:             worker.join();
98:         }
99:     }
100:     m_workers.clear();
```
* **Explanation**:
  * Iterates through the worker list.
  * `worker.joinable()`: Checks if the thread handle refers to an active OS thread.
  * `worker.join()`: Blocks the calling thread (the GUI/main thread) until the worker thread finishes executing and exits. This prevents thread leaks and crashes during exit.
  * `m_workers.clear()`: Empties the thread handles vector.

```cpp
101:     m_running = false;
102: 
103:     if (was_running) {
104:         emit finished();
105:     }
106: }
```
* **Explanation**: Emits the finished signal if the scanner was actually running.

```cpp
108: void Scanner::add_task(const std::string& path, TreeNode* parent_node) {
109:     // Security check: limit directory traversal depth to 100 to prevent stack/OOM loop exploits
110:     size_t depth = 0;
111:     for (char c : path) {
112:         if (c == '/' || c == '\\') {
113:             depth++;
114:         }
115:     }
116:     if (depth > 100) return;
```
* **Explanation**:
  * **Path Depth Guard**: Counts directory separator slashes (`/` or `\\`). If the path depth exceeds 100 levels, we ignore it. This prevents stack overflow crashes (from deeply recursive folder structures) and protects against maliciously structured folders.

```cpp
118:     std::lock_guard<std::mutex> lock(m_queue_mutex);
119:     m_task_queue.push_back({path, parent_node});
120:     m_queue_cv.notify_one();
121: }
```
* **Explanation**:
  * `std::lock_guard`: Locks the queue mutex.
  * `m_task_queue.push_back`: Adds the task unit.
  * `m_queue_cv.notify_one()`: Signals a single sleeping worker thread that a new task is available in the queue.

```cpp
123: bool Scanner::get_next_task(std::string& path, TreeNode*& parent_node) {
124:     std::unique_lock<std::mutex> lock(m_queue_mutex);
```
* **Explanation**:
  * `std::unique_lock`: Locks the queue mutex. Unlike `lock_guard`, a `unique_lock` can be unlocked and locked manually, which is required by condition variables.

```cpp
126:     // Decrement active workers to indicate this thread is now waiting
127:     m_active_workers--;
```
* **Explanation**: Decrements the atomic active worker count because the thread is about to wait for a task.

```cpp
130:     if (m_active_workers == 0 && m_task_queue.empty()) {
131:         m_running = false;
132:         m_queue_cv.notify_all();
133:         emit finished();
134:         return false;
135:     }
```
* **Explanation**:
  * **Scan Completion Check**: If this thread is the last one active (`m_active_workers == 0`) and there are no tasks left in the queue, the entire scan is finished.
  * `m_running = false`: Disables the scanner.
  * `m_queue_cv.notify_all()`: Wakes up all other sleeping threads so they can notice the scanner has stopped and exit their loop.
  * `emit finished()`: Emits completion signal to the GUI.
  * Returns `false` to terminate the thread's loop.

```cpp
137:     while (m_task_queue.empty() && m_running && !m_cancelled) {
138:         m_queue_cv.wait(lock);
139:     }
```
* **Explanation**:
  * If the queue is empty, the thread calls `m_queue_cv.wait(lock)`. This atomically releases the lock and puts the thread to sleep.
  * Waking up is controlled by `m_queue_cv` notifications. Waking inside a `while` loop prevents **spurious wakeups** (where a thread wakes up without an actual signal).

```cpp
141:     // Thread wakes up to execute a task, increment active workers
142:     m_active_workers++;
```
* **Explanation**: Increments the active worker count because the thread has woken up and is processing a task.

```cpp
144:     if (m_cancelled || !m_running || m_task_queue.empty()) {
145:         m_active_workers--; // Clean up thread exit state
146:         return false;
147:     }
```
* **Explanation**: If woken up because the scan was cancelled or stopped, decrements the active worker count and returns `false` to signal loop termination.

```cpp
149:     auto task = m_task_queue.back();
150:     m_task_queue.pop_back();
151:     path = std::move(task.path);
152:     parent_node = task.parent_node;
153:     return true;
154: }
```
* **Explanation**:
  * Pops the task from the back of the vector.
  * `std::move(task.path)`: Transfers the string buffer memory to the output parameter `path` without copying, saving memory allocation overhead.
  * Returns `true` to signal a task is ready to be processed.

```cpp
159: void Scanner::worker_loop() {
160: #ifdef _WIN32
161:     SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
162: #else
163:     setpriority(PRIO_PROCESS, 0, 10);
164: #endif
165: 
166:     std::string current_path;
167:     TreeNode* parent_node = nullptr;
168: 
169:     auto last_update = std::chrono::steady_clock::now();
170:     uint64_t files_since_update = 0;
```
* **Explanation**: 
  * **Low-Priority Thread Scheduling**: Lower the thread's scheduling priority right at startup to prevent system stuttering:
    * `SetThreadPriority(..., THREAD_PRIORITY_BELOW_NORMAL)` on Windows.
    * `setpriority(PRIO_PROCESS, 0, 10)` on POSIX/Linux (increasing the thread "niceness" value).
    * This ensures the operating system prioritizes user foreground tasks (like gaming or audio playback) over the disk scanning threads.
  * Initialize loop metrics. `last_update` tracks the time of the last progress signal to the GUI, preventing performance degradation from excessive signal emissions.

```cpp
163:     while (m_running && !m_cancelled) {
164:         // Handle Pause
165:         if (m_paused) {
166:             std::unique_lock<std::mutex> pause_lock(m_queue_mutex);
167:             while (m_paused && !m_cancelled) {
168:                 m_pause_cv.wait(pause_lock);
169:             }
170:         }
```
* **Explanation**:
  * If paused, the thread locks the queue mutex and blocks on `m_pause_cv` until the pause state is cleared.

```cpp
172:         if (m_cancelled) [[unlikely]] break;
```
* **Explanation**:
  * `[[unlikely]]`: C++20 attribute that hints to the compiler's branch predictor that cancellation is rare. This optimizes compiler branch structure for the common path (scanning).

```cpp
174:         if (!get_next_task(current_path, parent_node)) {
175:             break;
176:         }
```
* **Explanation**: Acquires the next task. If it returns false, the thread breaks the loop and terminates.

```cpp
178:         uint64_t dir_files_size = 0;
179:         uint64_t dir_files_allocated = 0;
180:         uint64_t dir_files_count = 0;
181:         uint64_t dir_subdirs_count = 0;
```
* **Explanation**: Allocates thread-local variables to aggregate directory statistics. Summing stats locally instead of updating atomic variables for each file reduces cache invalidation overhead between CPU cores.

```cpp
183:         try {
184:             scan_directory(current_path, parent_node, dir_files_size, dir_files_allocated, dir_files_count, dir_subdirs_count);
```
* **Explanation**: Calls the platform-specific filesystem traversal code.

```cpp
186:             if (dir_files_size > 0 || dir_files_allocated > 0 || dir_files_count > 0 || dir_subdirs_count > 0) {
187:                 parent_node->propagate_stats(dir_files_size, dir_files_allocated, dir_files_count, dir_subdirs_count);
```
* **Explanation**: Propagates the accumulated stats up the tree to the root node.

```cpp
189:                 m_files_scanned += dir_files_count;
190:                 m_dirs_scanned += dir_subdirs_count;
191:                 m_bytes_scanned += dir_files_size;
192:                 
193:                 files_since_update += dir_files_count + dir_subdirs_count;
194:             }
```
* **Explanation**: Updates the global atomic counters and thread-local GUI update trackers.

```cpp
195:         } catch (...) {
196:             // Fail-safe to keep thread running
197:         }
```
* **Explanation**: Catch-all block. Ensures any unexpected filesystem access exceptions do not crash the worker thread, allowing the scan to continue.

```cpp
200:         auto now = std::chrono::steady_clock::now();
201:         if (now - last_update > std::chrono::milliseconds(100) || files_since_update > 2000) {
202:             emit progressUpdated(m_files_scanned.load(), m_dirs_scanned.load(), m_bytes_scanned.load());
203:             files_since_update = 0;
204:             last_update = now;
205:         }
206:     }
```
* **Explanation**:
  * Throttled UI Updates: If more than 100ms has elapsed since the last update, or the thread has processed over 2000 files, it emits the `progressUpdated(...)` signal to update the UI progress bar.

```cpp
208:     // Final clean update
209:     emit progressUpdated(m_files_scanned.load(), m_dirs_scanned.load(), m_bytes_scanned.load());
210: }
```
* **Explanation**: Emits one final progress signal upon loop exit to ensure the GUI displays the final counts.

```cpp
215: double Scanner::progress_percentage() const {
216:     if (!m_running) return 100.0;
217:     uint64_t target = m_total_target_bytes.load();
218:     if (target == 0) return 0.0;
219:     double pct = (static_cast<double>(m_bytes_scanned.load()) / target) * 99.0;
220:     if (pct > 99.0) pct = 99.0;
221:     return pct;
222: }
```
* **Explanation**:
  * Calculates scan progress as a percentage of the partition's total used bytes.
  * The result is capped at 99.0% while running. This prevents the progress bar from reaching 100% before all threads have finished and joined.

```cpp
224: void Scanner::test_scanner() {
```
* **Explanation**: Verification test suite. It creates a mock directory hierarchy on disk, populates it with dummy files, scans the hierarchy, and asserts that the scanner results match the expected values.

```cpp
225:     std::string test_dir = "./test_scan_temp";
226:     std::filesystem::create_directories(test_dir + "/sub1/sub2");
```
* **Explanation**: Allocates a temp directory path and constructs nested subfolders using `std::filesystem::create_directories`.

```cpp
228:     // Write dummy files
229:     {
230:         std::ofstream f1(test_dir + "/file1.txt");
231:         f1 << "12345"; // 5 bytes
232:     }
233:     {
234:         std::ofstream f2(test_dir + "/sub1/file2.txt");
235:         f2 << "1234567890"; // 10 bytes
236:     }
237:     {
238:         std::ofstream f3(test_dir + "/sub1/sub2/file3.txt");
239:         f3 << "1"; // 1 byte
240:     }
```
* **Explanation**: Writes 3 dummy files containing 5, 10, and 1 byte(s) respectively.

```cpp
242:     Scanner scanner;
243:     scanner.start(test_dir);
```
* **Explanation**: Instantiates a test scanner and scans the temporary directory.

```cpp
246:     while (scanner.running()) {
247:         std::this_thread::sleep_for(std::chrono::milliseconds(5));
248:     }
```
* **Explanation**: Blocks the test thread until the scanner threads finish processing the mock directory.

```cpp
250:     // Verify statistics
251:     assert(scanner.files_scanned() == 3);
252:     assert(scanner.dirs_scanned() == 2);
253:     assert(scanner.bytes_scanned() == 16);
```
* **Explanation**: Asserts that the scanner counted 3 files, 2 directories, and 16 bytes.

```cpp
255:     TreeNode* root = scanner.root_node();
256:     assert(root != nullptr);
257:     assert(root->size == 16);
258:     assert(root->file_count == 3);
259:     assert(root->dir_count == 2);
```
* **Explanation**: Verifies the root node contains correct aggregated values.

```cpp
262:     std::filesystem::remove_all(test_dir);
```
* **Explanation**: Deletes the temporary test directory and its files.

```cpp
264:     std::cout << "Scanner tests passed successfully!" << std::endl;
265: }
```
* **Explanation**: Prints confirmation to stdout.
