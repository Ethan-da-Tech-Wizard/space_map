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
9: #include <unordered_set>
10: #include <sys/types.h>
11: #include <sys/stat.h>
12: #ifndef _WIN32
13: #include <sys/statvfs.h>
14: #endif
15: #include "tree_node.hpp"
```
* **Explanation**: Imports external dependencies:
  * `<QObject>`: Exposes the `QObject` base class, enabling Qt metadata features like signals, slots, and object trees.
  * `<string>`: Exposes `std::string` for dynamic path string storage.
  * `<vector>`: Exposes `std::vector` for task queuing and worker thread tracking.
  * `<thread>`: Exposes `std::thread` to spawn concurrent OS-level threads.
  * `<mutex>`: Exposes mutual exclusion variables to prevent data races.
  * `<condition_variable>`: Exposes `std::condition_variable` to block/wake threads efficiently.
  * `<atomic>`: Exposes CPU-native lock-free mathematical variables.
  * `<unordered_set>`: Exposes `std::unordered_set` (hash-table search structure) for tracking visited inodes in $O(1)$ time, replacing the older red-black tree `std::set` implementation to prevent pointer-chasing latency.
  * `<sys/types.h>` and `<sys/stat.h>`: Exposes system types like `dev_t` (device IDs) and `ino_t` (inode IDs) to identify filesystem loops.
  * `#ifndef _WIN32`: Excludes `<sys/statvfs.h>` on Windows platforms since Windows does not implement POSIX disk statistics (which are instead resolved via the Win32 API).
  * `"tree_node.hpp"`: Includes our thread-safe `TreeNode` structure so the scanner can construct the file tree hierarchy.

```cpp
17: class Scanner : public QObject {
18:     Q_OBJECT
```
* **Explanation**:
  * `class Scanner : public QObject`: Defines the `Scanner` class inheriting from Qt's `QObject`.
  * `Q_OBJECT`: The crucial Qt preprocessor macro. When Qt's build tool (moc - Meta-Object Compiler) parses this file, this macro triggers the generation of C++ code that implements signals, slots, dynamic properties, and run-time type information (RTTI) for the class.

```cpp
19: public:
20:     explicit Scanner(QObject* parent = nullptr);
```
* **Explanation**: The class constructor.
  * `explicit`: Prevents the compiler from implicitly converting a parent pointer to a `Scanner` instance (avoiding type conversion bugs).
  * `QObject* parent = nullptr`: Accepts an optional parent object pointer. In Qt, if a parent object is deleted, it automatically deletes all of its child objects, preventing memory leaks.

```cpp
21:     ~Scanner() override;
```
* **Explanation**: Destructor. Declared with `override` to ensure the compiler checks that it correctly overrides the base `QObject` virtual destructor. Inside, it cancels any running threads to prevent zombie processes.

```cpp
23:     // Starts the multi-threaded scanning process for root_path.
24:     void start(const std::string& root_path);
```
* **Explanation**: Public method called by the user interface to initiate scanning. It allocates the root tree node and launches the worker threads.

```cpp
26:     // Pauses scanning. Threads block safely until resumed.
27:     void pause();
```
* **Explanation**: Sets the paused atomic flag, instructing worker threads to halt I/O traversal and sleep.

```cpp
29:     // Resumes scanning.
30:     void resume();
```
* **Explanation**: Clears the paused flag and signals the threads to wake up.

```cpp
32:     // Cancels scanning. Threads terminate as quickly as possible.
33:     void cancel();
```
* **Explanation**: Sets the cancelled flag, wakes up all sleeping threads, and blocks (joins) until all threads have exited cleanly.

```cpp
35:     // Resets the scanner state, freeing the current tree.
36:     void reset();
```
* **Explanation**: Resets internal atomic counters, releases the memory-mapped file tree, and clears task lists.

```cpp
38:     // Accessors
39:     bool running() const { return m_running; }
40:     bool paused() const { return m_paused; }
41:     bool cancelled() const { return m_cancelled; }
```
* **Explanation**: Getter methods returning the current states. They are marked `const` because they do not modify any class variables.

```cpp
42:     uint64_t files_scanned() const { return m_files_scanned; }
43:     uint64_t dirs_scanned() const { return m_dirs_scanned; }
44:     uint64_t bytes_scanned() const { return m_bytes_scanned; }
```
* **Explanation**: Return the current counts of files, directories, and total byte sizes read so far.

```cpp
45:     double progress_percentage() const;
```
* **Explanation**: Method to calculate and return the progress ratio (from 0.0 to 99.0 while running, 100.0 upon completion) compared to used disk space on the partition.

```cpp
46:     uint64_t free_bytes() const { return m_free_bytes; }
```
* **Explanation**: Returns the available free bytes on the partition.

```cpp
48:     TreeNode* root_node() { return m_root_node.get(); }
```
* **Explanation**: Returns the raw pointer to the root of the file tree. `get()` extracts the raw address from the owning `std::unique_ptr` without transferring ownership.

```cpp
50:     void adjust_stats(int64_t files_delta, int64_t dirs_delta, int64_t size_delta) {
51:         m_files_scanned += files_delta;
52:         m_dirs_scanned += dirs_delta;
53:         m_bytes_scanned += size_delta;
54:     }
```
* **Explanation**: Thread-safe delta arithmetic helper. When a user deletes a file/folder, the UI calls this to subtract the deleted counts from the global scanner statistics, keeping the HUD dashboard synchronized.

```cpp
58:     static void test_scanner();
```
* **Explanation**: Verification function that tests the scanner against a temporary directory structure during project testing.

```cpp
60: signals:
61:     // Emitted periodically during the scan to update the GUI
62:     void progressUpdated(uint64_t files, uint64_t dirs, uint64_t bytes);
63:     // Emitted when scanning is complete (or cancelled)
64:     void finished();
```
* **Explanation**:
  * `signals:`: Qt keyword specifying that the following functions are signals. Signals have no function bodies; they are implemented by the Meta-Object Compiler.
  * `progressUpdated(...)`: Emitted periodically to push scanning numbers to the main thread GUI without blocking the scanning threads.
  * `finished()`: Emitted when the task queue is exhausted or the scan is cancelled, notifying the UI to stop timers and enable controls.

```cpp
66: private:
67:     void worker_loop();
```
* **Explanation**: Private method run by each thread in the thread pool. It fetches tasks from the queue and scans subdirectories in a loop.

```cpp
68:     bool get_next_task(std::string& path, TreeNode*& parent_node);
```
* **Explanation**: A thread-safe, blocking method to pop a directory scan task from the queue. Returns `false` if the scan has finished or cancelled.

```cpp
69:     void add_task(const std::string& path, TreeNode* parent_node);
```
* **Explanation**: Pushes a new subdirectory scan task onto the queue and notifies a waiting thread.

```cpp
70:     void scan_directory(const std::string& current_path, TreeNode* parent_node,
71:                          uint64_t& dir_files_size, uint64_t& dir_files_allocated,
72:                          uint64_t& dir_files_count, uint64_t& dir_subdirs_count);
```
* **Explanation**: Core low-level directory traversal function. Its implementation is platform-specific (Linux, macOS, and Windows have their own implementations in `src/platform/`).

```cpp
73:     void worker_finished();
```
* **Explanation**: Thread lifecycle hook.

```cpp
75:     struct DirId {
76:         dev_t dev;
77:         ino_t ino;
78:         bool operator==(const DirId& o) const {
79:             return dev == o.dev && ino == o.ino;
80:         }
81:     };
```
* **Explanation**:
  * `struct DirId`: Uniquely identifies a directory across the system.
    * `dev_t dev`: Device ID of the disk storage partition containing the folder.
    * `ino_t ino`: Inode ID (unique file/folder index number) within that partition.
  * `bool operator==(const DirId& o) const`: Equality comparator needed by `std::unordered_set` to resolve hash collisions.

```cpp
85:     struct DirIdHash {
86:         size_t operator()(const DirId& id) const noexcept {
87:             size_t h1 = std::hash<dev_t>{}(id.dev);
88:             size_t h2 = std::hash<ino_t>{}(id.ino);
89:             return h1 ^ (h2 * 0x9e3779b97f4a7c15ULL + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
90:         }
91:     };
```
* **Explanation**:
  * **Hash Functor**: Custom hashing functor that combines the 64-bit device ID (`dev`) and the 64-bit inode ID (`ino`) using a fast bit-mixing algorithm (inspired by `boost::hash_combine`). This distributes hash values uniformly, ensuring near-constant time $O(1)$ operations in the unordered set lookup.

```cpp
95:     std::unique_ptr<TreeNode> m_root_node;
```
* **Explanation**: The smart pointer that owns the memory of the root `TreeNode` of the scanned tree. When this unique pointer is reset or reassigned, it cleans up all subnodes automatically.

```cpp
98:     struct Task {
99:         std::string path;
100:         TreeNode* parent_node;
101:     };
```
* **Explanation**: Defines a unit of scanning work.
  * `path`: Absolute filesystem path to scan (e.g. `/home/user/Downloads`).
  * `parent_node`: Raw pointer to the `TreeNode` inside the tree where discovered files/folders should be attached.

```cpp
102:     std::vector<Task> m_task_queue;
```
* **Explanation**: Dynamic array acting as the scan task queue. Worker threads pop tasks from the back (`m_task_queue.pop_back()`) to process them.

```cpp
103:     std::mutex m_queue_mutex;
```
* **Explanation**: Mutual exclusion lock protecting the `m_task_queue` and active worker counts. Since multiple threads push and pop from the queue, this lock prevents corruption of the internal vector structure.

```cpp
104:     std::condition_variable m_queue_cv;
```
* **Explanation**: Condition variable used to coordinate threads. When the queue is empty, worker threads call `m_queue_cv.wait()` to sleep, putting the CPU to sleep instead of busy-waiting. When a new task is pushed, `m_queue_cv.notify_one()` wakes up one worker.

```cpp
105:     std::condition_variable m_pause_cv;
```
* **Explanation**: Condition variable used to pause worker threads. When the user clicks "Pause", workers block on this condition variable until the user clicks "Resume".

```cpp
108:     std::vector<std::thread> m_workers;
```
* **Explanation**: Array holding the handle to each thread spawned by the scanner. Used to join threads during cancellation or completion.

```cpp
109:     std::atomic<bool> m_running{false};
110:     std::atomic<bool> m_paused{false};
111:     std::atomic<bool> m_cancelled{false};
```
* **Explanation**: State control flags. They are wrapped in `std::atomic` to ensure that checks and modifications are thread-safe and visible across threads without standard mutex locking overhead.

```cpp
112:     std::atomic<int> m_active_workers{0};
```
* **Explanation**: Tracks the number of worker threads currently processing a directory. When this count drops to 0 and the task queue is empty, the entire scanning job is complete.

```cpp
115:     std::unordered_set<DirId, DirIdHash> m_visited_dirs;
116:     std::mutex m_visited_mutex;
```
* **Explanation**:
  * `std::unordered_set<DirId, DirIdHash> m_visited_dirs`: Hash-based set containing the IDs of all directories traversed during the scan. Used to detect and block recursive symlink cycles, bind mounts, and recursive directory loops. Optimized to $O(1)$ complexity to avoid red-black tree search overhead.
  * `std::mutex m_visited_mutex`: Exclusive lock protecting the visited directory set.

```cpp
119:     std::atomic<uint64_t> m_files_scanned{0};
120:     std::atomic<uint64_t> m_dirs_scanned{0};
121:     std::atomic<uint64_t> m_bytes_scanned{0};
122:     std::atomic<uint64_t> m_total_target_bytes{0};
123:     std::atomic<uint64_t> m_free_bytes{0};
```
* **Explanation**: Atomic counters tracking total progress. `m_total_target_bytes` holds the partition size used bytes, which serves as the reference point for progress calculations.

---

## 2. src/scanner.cpp (Source File Breakdown)

The source file implements the coordinator, queue management, thread synchronization, progress calculation, and unit verification.

```cpp
20: Scanner::Scanner(QObject* parent) : QObject(parent) {}
```
* **Explanation**: Class constructor. Delegates the parent pointer initialization to the base `QObject`.

```cpp
22: Scanner::~Scanner() {
23:     cancel();
24: }
```
* **Explanation**: Destructor. It calls `cancel()` to ensure that if the GUI is closed while a scan is active, all worker threads are safely terminated and joined before memory destruction begins, avoiding segfaults.

```cpp
26: void Scanner::reset() {
27:     cancel();
28:     m_root_node.reset();
```
* **Explanation**:
  * `cancel()`: Terminates active scanning.
  * `m_root_node.reset()`: Resets the root unique pointer, deleting the entire hierarchy and freeing all memory.

```cpp
29:     m_files_scanned = 0;
30:     m_dirs_scanned = 0;
31:     m_bytes_scanned = 0;
32:     m_total_target_bytes = 0;
33:     m_free_bytes = 0;
```
* **Explanation**: Resets atomic metrics back to zero.

```cpp
34:     {
35:         std::lock_guard<std::mutex> lock(m_visited_mutex);
36:         m_visited_dirs.clear();
37:     }
```
* **Explanation**:
  * `std::lock_guard<std::mutex> lock(...)`: Acquires the mutex for the scope. RAII pattern releases it when the closing brace `}` is reached.
  * `m_visited_dirs.clear()`: Empties the visited directories set.

```cpp
38:     {
39:         std::lock_guard<std::mutex> lock(m_queue_mutex);
40:         m_task_queue.clear();
41:     }
42: }
```
* **Explanation**: Empties the task queue under its corresponding queue lock.

```cpp
44: void Scanner::start(const std::string& root_path) {
45:     reset();
```
* **Explanation**: Starts a scan. First calls `reset()` to ensure a clean slate and abort any pre-existing scanning tasks.

```cpp
47:     m_running = true;
48:     m_paused = false;
49:     m_cancelled = false;
```
* **Explanation**: Sets the scanner's main state variables.

```cpp
51:     m_root_node = std::make_unique<TreeNode>(root_path, true);
```
* **Explanation**: Allocates a new root `TreeNode` on the heap for our scan tree, using the starting root path as its name.

```cpp
54:     std::error_code ec;
55:     std::string abs_path = std::filesystem::absolute(root_path, ec).string();
56:     if (ec) {
57:         abs_path = root_path;
58:     }
```
* **Explanation**:
  * Converts the input path to an absolute path using C++17 `std::filesystem::absolute`.
  * `std::error_code ec`: Captures any path errors without throwing exceptions. If resolving fails, it falls back to the original root path.

```cpp
60:     // Measure the total used bytes of the filesystem partition to calculate accurate progress
61: #ifdef _WIN32
62:     // Convert path to wide string
63:     std::wstring wpath(abs_path.begin(), abs_path.end());
64:     ULARGE_INTEGER freeBytesAvailable, totalBytes, totalFreeBytes;
65:     if (GetDiskFreeSpaceExW(wpath.c_str(), &freeBytesAvailable, &totalBytes, &totalFreeBytes)) {
66:         m_total_target_bytes = totalBytes.QuadPart - totalFreeBytes.QuadPart;
67:         m_free_bytes = freeBytesAvailable.QuadPart;
68:     }
69: #else
70:     struct statvfs vfs;
71:     if (statvfs(abs_path.c_str(), &vfs) == 0) {
72:         m_total_target_bytes = (vfs.f_blocks - vfs.f_bfree) * vfs.f_frsize;
73:         m_free_bytes = vfs.f_bavail * vfs.f_frsize;
74:     }
75: #endif
```
* **Explanation**:
  * Cross-platform disk info calculation.
  * On Windows, it converts the UTF-8 string to a wide character string (`std::wstring`) and invokes `GetDiskFreeSpaceExW`.
  * On Linux/macOS, it calls the POSIX `statvfs(...)` system call.
  * `m_total_target_bytes` stores the total allocated bytes on the partition. This serves as the progress baseline.

```cpp
77:     struct stat st;
78:     if (stat(abs_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
79:         {
80:             std::lock_guard<std::mutex> lock(m_visited_mutex);
81:             m_visited_dirs.insert({st.st_dev, st.st_ino});
82:         }
83:         add_task(abs_path, m_root_node.get());
```
* **Explanation**:
  * `stat(abs_path.c_str(), &st)`: Gets standard file information for the absolute root path.
  * `S_ISDIR(st.st_mode)`: Verifies if the root path is indeed a directory.
  * `m_visited_dirs.insert({st.st_dev, st.st_ino})`: Inserts the root directory's device and inode number to initialize cycle detection.
  * `add_task(...)`: Adds the initial scanning task (the root path) to the task queue.

```cpp
84:     } else {
85:         m_running = false;
86:         emit finished();
87:         return;
88:     }
```
* **Explanation**: If the root path does not exist or is not a directory, sets `m_running = false`, emits the `finished()` signal immediately, and exits.

```cpp
90:     // Pre-allocate task queue to avoid repeated heap reallocations during scan burst
91:     m_task_queue.reserve(4096);
```
* **Explanation**:
  * **Memory Pre-Allocation**: Reserves space for 4,096 tasks upfront in the `m_task_queue` vector. During the start of a scan, the task queue receives a massive burst of directory scan items. Pre-allocating capacity avoids successive memory reallocations, memory copying, and heap fragmentation.

```cpp
93:     // Use all available logical cores. Scanning is I/O-bound and SSDs handle
94:     // parallel random reads efficiently, so hyperthreaded cores still help.
95:     unsigned int num_threads = std::thread::hardware_concurrency();
96:     if (num_threads == 0) num_threads = 4;
```
* **Explanation**:
  * **Parallelism Strategy**: Since modern drives are high-speed SSDs, they handle concurrent random read requests efficiently. Artificially limiting the thread count to physical cores (as is sometimes done for HDDs to prevent mechanical head thrashing) is counterproductive. Using all logical hyperthreaded processors maximizes throughput.
  * `std::thread::hardware_concurrency()`: Queries the system for the total number of hardware cores or logical processors. If it returns 0, we fallback to spawning 4 threads.

```cpp
98:     m_active_workers = num_threads;
99:     m_workers.reserve(num_threads);
100:     for (unsigned int i = 0; i < num_threads; ++i) {
101:         m_workers.emplace_back(&Scanner::worker_loop, this);
102:     }
```
* **Explanation**:
  * Sets the active worker atomic counter to the thread count.
  * `m_workers.reserve(...)`: Prevents reallocation of the thread list vector.
  * `emplace_back(...)`: Spawns logical workers in-place inside the vector, mapping the `Scanner::worker_loop` execution to each thread.

```cpp
105: void Scanner::pause() {
106:     m_paused = true;
107: }
```
* **Explanation**: Sets `m_paused` to `true`. Worker threads check this flag before executing tasks.

```cpp
109: void Scanner::resume() {
110:     m_paused = false;
111:     m_pause_cv.notify_all();
112: }
```
* **Explanation**:
  * Sets `m_paused` to `false`.
  * `m_pause_cv.notify_all()`: Wakes up all worker threads waiting on the pause condition variable, resuming traversal.

```cpp
114: void Scanner::cancel() {
115:     bool was_running = m_running;
116:     m_cancelled = true;
117:     m_paused = false;
```
* **Explanation**: Sets the cancelled flag, clears the paused flag, and signals termination.

```cpp
118:     m_pause_cv.notify_all();
119:     m_queue_cv.notify_all();
```
* **Explanation**: Wakes up all sleeping worker threads, whether they are waiting for tasks or are paused. Waking them allows them to check the `m_cancelled` flag and exit cleanly.

```cpp
121:     for (auto& worker : m_workers) {
122:         if (worker.joinable()) {
123:             worker.join();
124:         }
125:     }
126:     m_workers.clear();
```
* **Explanation**:
  * Iterates through the worker list.
  * `worker.joinable()`: Checks if the thread handle refers to an active OS thread.
  * `worker.join()`: Blocks the calling thread (the GUI/main thread) until the worker thread finishes executing and exits. This prevents thread leaks and crashes during exit.
  * `m_workers.clear()`: Empties the thread handles vector.

```cpp
127:     m_running = false;
128: 
129:     if (was_running) {
130:         emit finished();
131:     }
132: }
```
* **Explanation**: Emits the finished signal if the scanner was actually running.

```cpp
134: void Scanner::add_task(const std::string& path, TreeNode* parent_node) {
135:     // Security check: limit directory traversal depth to prevent stack/OOM loop exploits
136:     constexpr size_t kMaxDepth = 100;
137:     size_t depth = 0;
138:     for (char c : path) {
139:         if (c == '/' || c == '\\') {
140:             depth++;
141:         }
142:     }
143:     if (depth > kMaxDepth) return;
```
* **Explanation**:
  * **Path Depth Guard**: Counts directory separator slashes (`/` or `\\`). If the path depth exceeds 100 levels, we ignore it. This prevents stack overflow crashes (from deeply recursive folder structures) and protects against maliciously structured folders.

```cpp
145:     std::lock_guard<std::mutex> lock(m_queue_mutex);
146:     m_task_queue.push_back({path, parent_node});
147:     m_queue_cv.notify_one();
148: }
```
* **Explanation**:
  * `std::lock_guard`: Locks the queue mutex.
  * `m_task_queue.push_back`: Adds the task unit.
  * `m_queue_cv.notify_one()`: Signals a single sleeping worker thread that a new task is available in the queue.

```cpp
150: bool Scanner::get_next_task(std::string& path, TreeNode*& parent_node) {
151:     std::unique_lock<std::mutex> lock(m_queue_mutex);
```
* **Explanation**:
  * `std::unique_lock`: Locks the queue mutex. Unlike `lock_guard`, a `unique_lock` can be unlocked and locked manually, which is required by condition variables.

```cpp
154:     m_active_workers--;
```
* **Explanation**: Decrements the atomic active worker count because the thread is about to wait for a task.

```cpp
157:     if (m_active_workers == 0 && m_task_queue.empty()) {
158:         m_running = false;
159:         m_queue_cv.notify_all();
160:         emit finished();
161:         return false;
162:     }
```
* **Explanation**:
  * **Scan Completion Check**: If this thread is the last one active (`m_active_workers == 0`) and there are no tasks left in the queue, the entire scan is finished.
  * `m_running = false`: Disables the scanner.
  * `m_queue_cv.notify_all()`: Wakes up all other sleeping threads so they can notice the scanner has stopped and exit their loop.
  * `emit finished()`: Emits completion signal to the GUI.
  * Returns `false` to terminate the thread's loop.

```cpp
164:     while (m_task_queue.empty() && m_running && !m_cancelled) {
165:         m_queue_cv.wait(lock);
166:     }
```
* **Explanation**:
  * If the queue is empty, the thread calls `m_queue_cv.wait(lock)`. This atomically releases the lock and puts the thread to sleep.
  * Waking up is controlled by `m_queue_cv` notifications. Waking inside a `while` loop prevents **spurious wakeups** (where a thread wakes up without an actual signal).

```cpp
169:     m_active_workers++;
```
* **Explanation**: Increments the active worker count because the thread has woken up and is processing a task.

```cpp
171:     if (m_cancelled || !m_running || m_task_queue.empty()) {
172:         m_active_workers--; // Clean up thread exit state
173:         return false;
174:     }
```
* **Explanation**: If woken up because the scan was cancelled or stopped, decrements the active worker count and returns `false` to signal loop termination.

```cpp
176:     auto task = m_task_queue.back();
177:     m_task_queue.pop_back();
178:     path = std::move(task.path);
179:     parent_node = task.parent_node;
180:     return true;
181: }
```
* **Explanation**:
  * Pops the task from the back of the vector.
  * `std::move(task.path)`: Transfers the string buffer memory to the output parameter `path` without copying, saving memory allocation overhead.
  * Returns `true` to signal a task is ready to be processed.

```cpp
183: void Scanner::worker_loop() {
184: #ifdef _WIN32
185:     SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
186: #else
187:     setpriority(PRIO_PROCESS, 0, 10);
188: #endif
```
* **Explanation**: 
  * **Low-Priority Thread Scheduling**: Lower the thread's scheduling priority right at startup to prevent system stuttering:
    * `SetThreadPriority(..., THREAD_PRIORITY_BELOW_NORMAL)` on Windows.
    * `setpriority(PRIO_PROCESS, 0, 10)` on POSIX/Linux (increasing the thread "niceness" value).
    * This ensures the operating system prioritizes user foreground tasks (like gaming or audio playback) over the disk scanning threads.

```cpp
190:     std::string current_path;
191:     TreeNode* parent_node = nullptr;
192: 
193:     auto last_update = std::chrono::steady_clock::now();
194:     uint64_t files_since_update = 0;
```
* **Explanation**: Initialize loop metrics. `last_update` tracks the time of the last progress signal to the GUI, preventing performance degradation from excessive signal emissions.

```cpp
196:     while (m_running && !m_cancelled) {
197:         // Handle Pause
198:         if (m_paused) {
199:             std::unique_lock<std::mutex> pause_lock(m_queue_mutex);
200:             while (m_paused && !m_cancelled) {
201:                 m_pause_cv.wait(pause_lock);
202:             }
203:         }
```
* **Explanation**: If paused, the thread locks the queue mutex and blocks on `m_pause_cv` until the pause state is cleared.

```cpp
205:         if (m_cancelled) [[unlikely]] break;
```
* **Explanation**:
  * `[[unlikely]]`: C++20 attribute that hints to the compiler's branch predictor that cancellation is rare. This optimizes compiler branch structure for the common path (scanning).

```cpp
207:         if (!get_next_task(current_path, parent_node)) {
208:             break;
209:         }
```
* **Explanation**: Acquires the next task. If it returns false, the thread breaks the loop and terminates.

```cpp
211:         uint64_t dir_files_size = 0;
212:         uint64_t dir_files_allocated = 0;
213:         uint64_t dir_files_count = 0;
214:         uint64_t dir_subdirs_count = 0;
```
* **Explanation**: Allocates thread-local variables to aggregate directory statistics. Summing stats locally instead of updating atomic variables for each file reduces cache invalidation overhead between CPU cores.

```cpp
216:         try {
217:             scan_directory(current_path, parent_node, dir_files_size, dir_files_allocated, dir_files_count, dir_subdirs_count);
```
* **Explanation**: Calls the platform-specific filesystem traversal code.

```cpp
219:             if (dir_files_size > 0 || dir_files_allocated > 0 || dir_files_count > 0 || dir_subdirs_count > 0) {
220:                 parent_node->propagate_stats(dir_files_size, dir_files_allocated, dir_files_count, dir_subdirs_count);
```
* **Explanation**: Propagates the accumulated stats up the tree to the root node.

```cpp
222:                 m_files_scanned += dir_files_count;
223:                 m_dirs_scanned += dir_subdirs_count;
224:                 m_bytes_scanned += dir_files_size;
225:                 
226:                 files_since_update += dir_files_count + dir_subdirs_count;
227:             }
```
* **Explanation**: Updates the global atomic counters and thread-local GUI update trackers.

```cpp
228:         } catch (...) {
229:             // Fail-safe to keep thread running
230:         }
```
* **Explanation**: Catch-all block. Ensures any unexpected filesystem access exceptions do not crash the worker thread, allowing the scan to continue.

```cpp
233:         constexpr auto kUpdateInterval = std::chrono::milliseconds(100);
234:         constexpr uint64_t kUpdateBatchThreshold = 2000;
235:         auto now = std::chrono::steady_clock::now();
236:         if (now - last_update > kUpdateInterval || files_since_update > kUpdateBatchThreshold) {
237:             emit progressUpdated(m_files_scanned.load(), m_dirs_scanned.load(), m_bytes_scanned.load());
238:             files_since_update = 0;
239:             last_update = now;
240:         }
241:     }
```
* **Explanation**:
  * Throttled UI Updates: If more than 100ms has elapsed since the last update, or the thread has processed over 2000 files, it emits the `progressUpdated(...)` signal to update the UI progress bar.

```cpp
244:     emit progressUpdated(m_files_scanned.load(), m_dirs_scanned.load(), m_bytes_scanned.load());
245: }
```
* **Explanation**: Emits one final progress signal upon loop exit to ensure the GUI displays the final counts.

```cpp
250: double Scanner::progress_percentage() const {
251:     if (!m_running) return 100.0;
252:     uint64_t target = m_total_target_bytes.load();
253:     if (target == 0) return 0.0;
254:     double pct = (static_cast<double>(m_bytes_scanned.load()) / target) * 99.0;
255:     if (pct > 99.0) pct = 99.0;
256:     return pct;
257: }
```
* **Explanation**:
  * Calculates scan progress as a percentage of the partition's total used bytes.
  * The result is capped at 99.0% while running. This prevents the progress bar from reaching 100% before all threads have finished and joined.

```cpp
259: void Scanner::test_scanner() {
260:     std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "spacemap_test_scan_temp";
261:     std::filesystem::create_directories(test_dir / "sub1" / "sub2");
```
* **Explanation**: Verification test suite. It creates a mock directory hierarchy in the OS temporary directory, constructs nested subfolders using `std::filesystem::create_directories`.

```cpp
263:     // Write dummy files
264:     {
265:         std::ofstream f1(test_dir / "file1.txt");
266:         f1 << "12345"; // 5 bytes
267:     }
268:     {
269:         std::ofstream f2(test_dir / "sub1" / "file2.txt");
270:         f2 << "1234567890"; // 10 bytes
271:     }
272:     {
273:         std::ofstream f3(test_dir / "sub1" / "sub2" / "file3.txt");
274:         f3 << "1"; // 1 byte
275:     }
```
* **Explanation**: Writes 3 dummy files containing 5, 10, and 1 byte(s) respectively.

```cpp
277:     Scanner scanner;
278:     scanner.start(test_dir.string());
```
* **Explanation**: Instantiates a test scanner and scans the temporary directory.

```cpp
281:     while (scanner.running()) {
282:         std::this_thread::sleep_for(std::chrono::milliseconds(5));
283:     }
```
* **Explanation**: Blocks the test thread until the scanner threads finish processing the mock directory.

```cpp
286:     // Verify statistics
287:     assert(scanner.files_scanned() == 3);
288:     assert(scanner.dirs_scanned() == 2);
289:     assert(scanner.bytes_scanned() == 16);
```
* **Explanation**: Asserts that the scanner counted 3 files, 2 directories, and 16 bytes.

```cpp
290:     TreeNode* root = scanner.root_node();
291:     assert(root != nullptr);
292:     assert(root->size == 16);
293:     assert(root->file_count == 3);
294:     assert(root->dir_count == 2);
```
* **Explanation**: Verifies the root node contains correct aggregated values.

```cpp
297:     std::filesystem::remove_all(test_dir);
```
* **Explanation**: Deletes the temporary test directory and its files.

```cpp
299:     std::cout << "Scanner tests passed successfully!" << "\n";
300: }
```
* **Explanation**: Prints confirmation to stdout. Uses `"\n"` instead of `std::endl` to avoid forced flushes.
