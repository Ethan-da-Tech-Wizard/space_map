# SpaceMap Architecture Book: Chapter 3

This chapter provides a complete line-by-line, character-by-character architectural explanation of the memory-optimized Tree Node (`src/tree_node.hpp` and `src/tree_node.cpp`).

---

# CHAPTER 3: Memory-Optimized Tree Nodes & Thread Safety

Filesystem structures are naturally recursive (directories contain subdirectories, which contain files). To represent this hierarchy in memory without running out of RAM, SpaceMap uses a custom-built, thread-safe Tree Node structure. It utilizes **prefix path compression** (storing only relative file/folder names) and **atomic stats propagation** (rolling up file counts and sizes to parent directories concurrently and lock-free).

---

## 1. src/tree_node.hpp (Header File Breakdown)

The header file defines the layout of the `TreeNode` structure in memory and its member functions.

```cpp
1: #pragma once
```
* **Explanation**: Preprocessor header guard. It tells the compiler to process this header only once per compilation unit, preventing multiple definition errors.

```cpp
2: #include <string>
3: #include <vector>
4: #include <memory>
5: #include <cstdint>
6: #include <shared_mutex>
7: #include <string_view>
8: #include <atomic>
```
* **Explanation**: Imports standard library headers:
  * `<string>`: Exposes standard dynamic strings (`std::string`).
  * `<vector>`: Exposes dynamic arrays (`std::vector`).
  * `<memory>`: Exposes smart pointers (`std::unique_ptr`).
  * `<cstdint>`: Exposes standardized integer widths like `uint64_t`.
  * `<shared_mutex>`: Exposes `std::shared_mutex` for read/write lock synchronization.
  * `<string_view>`: Exposes `std::string_view` (C++17) to refer to string data without copying.
  * `<atomic>`: Exposes lock-free atomic math operations (`std::atomic`).

```cpp
10: struct TreeNode {
```
* **Explanation**: Declares a `struct` named `TreeNode`. In C++, a `struct` is identical to a `class`, except all members are `public` by default.

```cpp
11:     std::string name;
```
* **Explanation**:
  * Stores the relative name of the file or folder (e.g. `icon.png`, not `/home/user/build/icon.png`).
  * **Prefix Path Compression**: By only storing the relative filename, we save hundreds of megabytes of RAM. Rebuilding the absolute path is deferred to when the user requests it (walking up parent pointers).

```cpp
12:     std::atomic<uint64_t> size{0};
```
* **Explanation**:
  * Atomic variable tracking the total logical size in bytes.
  * `std::atomic<uint64_t>`: Wraps a standard 64-bit integer, ensuring all increments and reads translate directly to CPU-native atomic instructions, preventing data races when multiple threads update the same folder size simultaneously.
  * `{0}`: Braced initialization sets the starting size value to 0.

```cpp
13:     std::atomic<uint64_t> allocated_size{0};
```
* **Explanation**: Atomic variable tracking the actual physical space occupied on disk. Files are allocated in sectors (usually 512 or 4096-byte blocks). This represents the real disk footprint.

```cpp
14:     std::atomic<uint64_t> file_count{0};
15:     std::atomic<uint64_t> dir_count{0};
```
* **Explanation**: Atomic variables tracking total nested files and subdirectories.

```cpp
16:     bool is_dir = false;
```
* **Explanation**: Boolean flag. If `true`, the node is a directory; if `false`, it is a regular file.

```cpp
17:     TreeNode* parent = nullptr;
```
* **Explanation**:
  * Raw pointer referencing the parent node in the hierarchy.
  * **Circular Reference Prevention**: We use a raw pointer (`TreeNode*`) instead of a smart pointer (`std::shared_ptr`) here. If children held a `shared_ptr` to their parent, and parents held a `shared_ptr` to their children, it would create a reference loop. The reference counts would never drop to zero, causing a permanent memory leak. A raw pointer broke the loop with zero overhead.

```cpp
18:     std::vector<std::unique_ptr<TreeNode>> children;
```
* **Explanation**:
  * Dynamic array storing child nodes.
  * `std::unique_ptr`: Explicitly defines **exclusive ownership**. The parent node exclusively owns its children. When the parent is deleted from memory, `std::unique_ptr` automatically deletes all child nodes, freeing memory recursively without leaks.

```cpp
19:     mutable std::shared_mutex mutex; // Protects children vector
```
* **Explanation**:
  * Mutex lock guarding the `children` array structure.
  * `mutable`: Allows us to lock and unlock the mutex even inside `const` (read-only) member functions (like `find_child`).
  * `std::shared_mutex`: Exposes a reader/writer lock. Multiple worker threads can acquire a read-lock (`std::shared_lock`) to search the children list concurrently. Only one thread can acquire an exclusive write-lock (`std::unique_lock`) to insert new children, preventing vector corruption while maximizing thread throughput.

```cpp
21:     TreeNode(std::string n, bool dir, TreeNode* p = nullptr);
```
* **Explanation**: Constructor declaration. Accepts name string `n`, directory flag `dir`, and parent pointer `p` (defaulting to `nullptr`).

```cpp
22:     ~TreeNode() = default;
```
* **Explanation**: Destructor. `= default` tells the compiler to auto-generate the standard destructor, which recursively deletes the `children` smart pointers.

```cpp
25:     TreeNode(const TreeNode&) = delete;
26:     TreeNode& operator=(const TreeNode&) = delete;
```
* **Explanation**: `= delete` explicitly blocks the compiler from creating copy constructors or copy assignment operators, preventing accidental copies of a node which would corrupt parent-child pointers.

```cpp
27:     TreeNode(TreeNode&&) = delete;
28:     TreeNode& operator=(TreeNode&&) = delete;
```
* **Explanation**: Blocks move constructors and move assignment operators, making the node location stationary in memory.

```cpp
31:     TreeNode* find_child(std::string_view child_name) const;
```
* **Explanation**: Const (read-only) lookup function. Searches `children` for a matching name and returns a raw pointer.

```cpp
34:     TreeNode* get_or_create_child(std::string_view child_name, bool child_is_dir);
```
* **Explanation**: Looks up a child node by name. If not found, creates and inserts it in sorted position under a write lock.

```cpp
37:     void propagate_stats(uint64_t size_delta, uint64_t allocated_delta, uint64_t files_delta, uint64_t dirs_delta);
```
* **Explanation**: Aggregates directory sizes and file counts recursively up the parent tree node links.

```cpp
40:     void sort_children_by_size(bool recursive = false);
```
* **Explanation**: Sorts child nodes descending by size.

```cpp
43:     static void test_tree_node();
```
* **Explanation**: Verification helper function running during startup.

---

## 2. src/tree_node.cpp (Source File Breakdown)

The source file implements tree traversal, sorted child insertion via double-checked locking, statistics rollup, and testing routines.

```cpp
1: #include "tree_node.hpp"
2: #include <algorithm>
3: #include <mutex>
```
* **Explanation**: Imports dependencies. `<algorithm>` exposes binary search algorithms (`std::lower_bound`). `<mutex>` exposes standard lock guards.

```cpp
5: TreeNode::TreeNode(std::string n, bool dir, TreeNode* p)
6:     : name(std::move(n)), is_dir(dir), parent(p) {
```
* **Explanation**:
  * Constructor definition.
  * `: name(std::move(n)), is_dir(dir), parent(p)`: Member initializer list.
  * `std::move(n)`: Transfers ownership of the string `n` buffer memory directly to the class member `name` without copying, reducing memory allocation cycles.

```cpp
7:     if (is_dir) {
8:         children.reserve(16);
9:     }
10: }
```
* **Explanation**: If this node is a directory, pre-allocates memory capacity for 16 child pointers in the `children` vector. This avoids repeated re-allocations and pointer copying during initial scanning.

```cpp
12: TreeNode* TreeNode::find_child(std::string_view child_name) const {
13:     std::shared_lock lock(mutex);
```
* **Explanation**:
  * `std::shared_lock lock(mutex)`: Acquires a shared read-lock. Multiple worker threads can query children in the same folder concurrently.

```cpp
14:     auto it = std::lower_bound(children.begin(), children.end(), child_name,
15:         [](const std::unique_ptr<TreeNode>& node, std::string_view name) {
16:             return node->name < name;
17:         });
```
* **Explanation**:
  * `std::lower_bound`: Standard library binary search ($O(\log N)$ complexity). It assumes the `children` vector is kept alphabetically sorted.
  * `[]( ... ) { ... }`: Inline lambda comparator. Compares a child node's name against the target string view.

```cpp
18:     if (it != children.end() && (*it)->name == child_name) {
19:         return it->get();
20:     }
21:     return nullptr;
22: }
```
* **Explanation**:
  * If the iterator `it` is valid and the child name matches:
    * `it->get()` extracts the raw address pointer from the owning `std::unique_ptr` wrapper and returns it.
  * Otherwise, returns a null pointer (`nullptr`).

```cpp
24: TreeNode* TreeNode::get_or_create_child(std::string_view child_name, bool child_is_dir) {
```
* **Explanation**: Thread-safe lookup-or-create method.

```cpp
25:     // 1. Double-checked locking pattern: first try with a read lock
26:     {
27:         std::shared_lock lock(mutex);
28:         auto it = std::lower_bound(children.begin(), children.end(), child_name,
29:             [](const std::unique_ptr<TreeNode>& node, std::string_view name) {
30:                 return node->name < name;
31:             });
32:         if (it != children.end() && (*it)->name == child_name) {
33:             return it->get();
34:         }
35:     }
```
* **Explanation**:
  * **Double-Checked Locking - Read Phase**:
    * Attempts to locate the node under a shared read-lock (`std::shared_lock`). If the directory has already been created, returns it immediately. This allows concurrent, lock-free lookups for existing nodes, keeping traversal extremely fast.
    * The closing brace `}` automatically releases the read lock (RAII pattern).

```cpp
38:     std::unique_lock lock(mutex);
```
* **Explanation**:
  * **Double-Checked Locking - Write Phase**:
    * If the node does not exist, acquires an exclusive write-lock (`std::unique_lock`). Only one thread can enter the writing phase at a time.

```cpp
41:     auto it = std::lower_bound(children.begin(), children.end(), child_name,
42:         [](const std::unique_ptr<TreeNode>& node, std::string_view name) {
43:             return node->name < name;
44:         });
45:     if (it != children.end() && (*it)->name == child_name) {
46:         return it->get();
47:     }
```
* **Explanation**:
  * **Second Check**: Re-searches the vector under the write-lock. In the split second between releasing the read-lock and acquiring the write-lock, another thread might have created the node. Re-verifying prevents duplicate creations.

```cpp
50:     auto new_node = std::make_unique<TreeNode>(std::string(child_name), child_is_dir, this);
51:     TreeNode* ptr = new_node.get();
52:     children.insert(it, std::move(new_node));
53:     return ptr;
54: }
```
* **Explanation**:
  * `std::make_unique`: Instantiates the new `TreeNode` on the heap.
  * `children.insert(it, std::move(new_node))`: Inserts the unique pointer at the alphabetically sorted iterator position `it`, keeping the vector sorted.
  * Returns the raw pointer to the created node.

```cpp
56: void TreeNode::propagate_stats(uint64_t size_delta, uint64_t allocated_delta, uint64_t files_delta, uint64_t dirs_delta) {
57:     TreeNode* current = this;
58:     while (current != nullptr) {
59:         current->size += size_delta;
60:         current->allocated_size += allocated_delta;
61:         current->file_count += files_delta;
62:         current->dir_count += dirs_delta;
63:         current = current->parent;
64:     }
```
* **Explanation**:
  * **Lock-Free Stats Propagation**: Propagates statistics up the parent chain to the root node.
  * `current->size += size_delta`: Uses atomic additions. These operate directly on registers, allowing worker threads to update directory sizes concurrently without mutex locks.
  * `current = current->parent`: Moves to the parent node. The loop terminates when it reaches the root (where parent is `nullptr`).

```cpp
67: void TreeNode::sort_children_by_size(bool recursive) {
68:     std::unique_lock lock(mutex);
69:     std::sort(children.begin(), children.end(),
70:         [](const std::unique_ptr<TreeNode>& a, const std::unique_ptr<TreeNode>& b) {
71:             if (a->size != b->size) {
72:                 return a->size > b->size; // Sort by size descending
73:             }
74:             return a->name < b->name; // Alphabetical fallback
75:         });
```
* **Explanation**:
  * Sorts child nodes descending by size.
  * `std::unique_lock lock(mutex)`: Acquires the write-lock before sorting.
  * `a->size > b->size`: Sorts by size descending.
  * `a->name < b->name`: Alphabetical fallback. If sizes are equal, sorts alphabetically. This satisfies the strict weak ordering requirement of C++ STL sort comparators, preventing sorting engine crashes.

```cpp
77:     if (recursive) {
78:         for (auto& child : children) {
79:             child->sort_children_by_size(true);
80:         }
81:     }
82: }
```
* **Explanation**: If `recursive` is `true`, calls sorting recursively on all subdirectory branches.

```cpp
88: void TreeNode::test_tree_node() {
```
* **Explanation**: Verification test harness. It tests node lookup, stats propagation, sorting logic, and multi-threaded write safety.

```cpp
89:     auto root = std::make_unique<TreeNode>("/", true);
90:     
91:     // Test child retrieval/creation
92:     TreeNode* home = root->get_or_create_child("home", true);
93:     assert(home != nullptr);
94:     assert(home->parent == root.get());
95:     assert(home->name == "home");
```
* **Explanation**: Creates a mock root and home directory, asserting parent relationships and names are mapped correctly.

```cpp
97:     TreeNode* user = home->get_or_create_child("user", true);
98:     assert(user != nullptr);
99:     assert(user->parent == home);
```
* **Explanation**: Creates nested user folder.

```cpp
101:     // Test stats propagation
102:     TreeNode* file1 = user->get_or_create_child("file1.txt", false);
103:     file1->propagate_stats(100, 100, 1, 0); // 100 bytes size, 100 bytes allocated, 1 file
```
* **Explanation**: Simulates finding a 100-byte file and propagates statistics.

```cpp
105:     assert(file1->size == 100);
106:     assert(file1->allocated_size == 100);
107:     assert(user->size == 100);
108:     assert(user->allocated_size == 100);
109:     assert(user->file_count == 1);
110:     assert(root->size == 100);
```
* **Explanation**: Asserts stats rolled up to all parent nodes.

```cpp
112:     // Test sorting by size
113:     TreeNode* file2 = user->get_or_create_child("file2.txt", false);
114:     file2->propagate_stats(500, 500, 1, 0); // file2 gets 500 bytes
```
* **Explanation**: Creates a larger file.

```cpp
116:     assert(user->size == 600);
117:     assert(user->allocated_size == 600);
118:     assert(user->file_count == 2);
```
* **Explanation**: Verifies total size updated to 600 bytes.

```cpp
120:     user->sort_children_by_size(false);
121:     assert(user->children[0]->name == "file2.txt");
122:     assert(user->children[1]->name == "file1.txt");
```
* **Explanation**: Runs sorting and asserts larger files are sorted to the front.

```cpp
124:     // Test multi-threaded insertion safety
125:     std::vector<std::thread> threads;
126:     for (int i = 0; i < 10; ++i) {
127:         threads.emplace_back([user, i]() {
128:             for (int j = 0; j < 100; ++j) {
129:                 std::string fname = "thread_" + std::to_string(i) + "_file_" + std::to_string(j) + ".txt";
130:                 TreeNode* f = user->get_or_create_child(fname, false);
131:                 f->propagate_stats(10, 10, 1, 0);
132:             }
133:         });
134:     }
```
* **Explanation**:
  * **Concurrency Stress Test**: Spawns 10 threads. Each thread inserts 100 files simultaneously in the same directory under active lock contention, simulating intense multi-threaded directory crawlers.

```cpp
135:     for (auto& t : threads) {
136:         t.join();
137:     }
```
* **Explanation**: Joins and terminates all threads.

```cpp
139:     assert(user->file_count == 2 + 1000);
140:     assert(user->size == 600 + 1000 * 10);
141:     assert(user->allocated_size == 600 + 1000 * 10);
142: 
143:     std::cout << "TreeNode tests passed successfully!" << std::endl;
144: }
```
* **Explanation**: Asserts all 1000 files were created safely without data races or corruption, and prints success to console.
