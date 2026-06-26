# SpaceMap Architecture Book: Chapter 5

This chapter provides a complete line-by-line, character-by-character architectural explanation of the platform-specific directory crawlers (`src/platform/scanner_linux.cpp`, `src/platform/scanner_win.cpp`, and `src/platform/scanner_mac.cpp`).

---

# CHAPTER 5: Platform-Specific Filesystem Walkers

Because directory structure layouts and size metadata queries differ radically between operating systems, SpaceMap utilizes native system APIs directly rather than generic abstractions. This chapter details how SpaceMap communicates with Linux, Windows, and macOS kernels at the raw system level.

---

## 1. src/platform/scanner_linux.cpp (Linux Platform Walker)

The Linux implementation uses low-level POSIX file descriptors, relative path lookups, and the modern `statx` system call to query metadata, optimized to eliminate per-entry heap string allocations.

```cpp
1: #include "scanner.hpp"
```
* **Explanation**: Includes the header declaration of the `Scanner` class.

```cpp
2: #ifdef __linux__
```
* **Explanation**: Preprocessor guard. The compiler only processes this file when compiling on Linux.

```cpp
3: #include <dirent.h>
4: #include <sys/types.h>
5: #include <sys/stat.h>
6: #include <fcntl.h>
7: #include <unistd.h>
8: #include <sys/syscall.h>
9: #include <linux/stat.h>
10: #include <cstring>
```
* **Explanation**: Imports standard POSIX and Linux-specific headers:
  * `<dirent.h>`: Exposes POSIX directory stream constructs (`DIR`, `dirent`, `opendir`, `readdir`).
  * `<sys/types.h>` and `<sys/stat.h>`: Declares file modes, attributes, and file system info.
  * `<fcntl.h>`: Provides file control flags like `AT_FDCWD` or `AT_SYMLINK_NOFOLLOW`.
  * `<unistd.h>`: Declares standard symbolic constants and system call wrappers (like `syscall`).
  * `<sys/syscall.h>`: Maps kernel system call numbers (like `SYS_statx`).
  * `<linux/stat.h>`: Kernel header defining kernel-level file stat structures.
  * `<cstring>`: Standard C string manipulation functions (like `strcmp` and `strlen`).

```cpp
45: void Scanner::scan_directory(const std::string& current_path, TreeNode* parent_node,
46:                              uint64_t& dir_files_size, uint64_t& dir_files_allocated,
47:                              uint64_t& dir_files_count, uint64_t& dir_subdirs_count) {
```
* **Explanation**: Class member function signature. Gathers thread-local outputs via references (`&`).

```cpp
48:     DIR* dir = opendir(current_path.c_str());
49:     if (!dir) return;
```
* **Explanation**: Opens the directory stream. If failure occurs, returns early.

```cpp
51:     int dfd = dirfd(dir); // Get directory file descriptor for fast relative path calls
52:     if (dfd < 0) {
53:         closedir(dir);
54:         return;
55:     }
```
* **Explanation**:
  * `dirfd(dir)`: Extracts the underlying system file descriptor (an integer referencing the open folder handle in the kernel).
  * Relative System Calls: Having a directory file descriptor (`dfd`) allows us to execute file calls relative to this descriptor. Instead of assembling long paths, we pass the file descriptor and the relative filename. This prevents string allocations, saves memory bandwidth, and blocks TOCTOU (Time-of-Check to Time-of-Use) file-swapping attacks.

```cpp
57:     struct dirent* entry;
58:     while ((entry = readdir(dir)) != nullptr) {
59:         if (m_cancelled) [[unlikely]] break;
```
* **Explanation**: Reads filesystem entries in a loop.

```cpp
61:         // Use raw C-string length check to avoid std::string allocation for skipped entries
62:         const char* d_name = entry->d_name;
63:         if (d_name[0] == '.') {
64:             if (d_name[1] == '\0') continue;                    // "."
65:             if (d_name[1] == '.' && d_name[2] == '\0') continue; // ".."
66:         }
```
* **Explanation**:
  * **Allocation-Free Filter**: Extract a raw pointer to `entry->d_name` and check for the dot folders (`.` and `..`) by inspecting characters directly. This avoids allocating a temporary `std::string` buffer on the heap for every single skipped entry.

```cpp
68:         size_t name_len = strlen(d_name);
```
* **Explanation**: Gets the string length of the filename once to reuse in string views and path creations.

```cpp
70:         // Security check: Ignore abnormally long filenames (max 255 bytes)
71:         if (name_len > 255) continue;
```
* **Explanation**: Rejects filenames exceeding the standard 255-byte limit.

```cpp
73:         // Security check: Ignore names containing dangerous control/escape characters (ASCII < 32)
74:         bool has_invalid_char = false;
75:         for (size_t i = 0; i < name_len; ++i) {
76:             if (static_cast<unsigned char>(d_name[i]) < 32) {
77:                 has_invalid_char = true;
78:                 break;
79:             }
80:         }
81:         if (has_invalid_char) continue;
```
* **Explanation**: Loops through raw characters to filter out invalid control characters, skipping files with malicious names without allocating any memory.

```cpp
83:         // Skip symlinks
84:         if (entry->d_type == DT_LNK) continue;
```
* **Explanation**: Skips symbolic links to avoid cycle traversal.

```cpp
86:         if (entry->d_type == DT_DIR) {
87:             std::string full_path = current_path == "/" ? "/" + std::string(d_name, name_len)
88:                                                         : current_path + "/" + std::string(d_name, name_len);
```
* **Explanation**: If the entry is a directory, construct the full path string. We only allocate path memory when we definitely need to enqueue a new subdirectory scan task.

```cpp
89:             // Ignore virtual/pseudo-filesystems
90:             if (full_path == "/proc" || full_path == "/sys" || full_path == "/dev" || full_path == "/run") {
91:                 continue;
92:             }
```
* **Explanation**: Filters out virtual directories.

```cpp
94:             // Check for mount/bind cycles using inode dev IDs via fast fstatat
95:             struct stat st;
96:             if (fstatat(dfd, d_name, &st, AT_SYMLINK_NOFOLLOW) == 0) {
97:                 DirId id{st.st_dev, st.st_ino};
98:                 {
99:                     std::lock_guard<std::mutex> lock(m_visited_mutex);
100:                     if (m_visited_dirs.contains(id)) {
101:                         continue;
102:                     }
103:                     m_visited_dirs.insert(id);
104:                 }
105:             }
```
* **Explanation**: Performs a fast `fstatat` using the directory descriptor. It locks the cycle-tracking mutex and uses C++20 `m_visited_dirs.contains()` for an $O(1)$ lookup in the unordered set to see if the directory has already been visited.

```cpp
107:             TreeNode* child = parent_node->get_or_create_child(std::string_view(d_name, name_len), true);
108:             add_task(full_path, child);
109:             dir_subdirs_count++;
```
* **Explanation**:
  * Pass `std::string_view` directly into `get_or_create_child`. By passing a pointer-and-length slice of the `readdir` buffer rather than a `std::string`, we avoid a heap string copy allocation for nodes that already exist in the tree.
  * Adds the new sub-path to the task queue.

```cpp
110:         } else if (entry->d_type == DT_REG) {
111:             uint64_t fsize = 0;
112:             uint64_t allocated_size = 0;
```
* **Explanation**: If it is a regular file, set up sizes.

```cpp
114: #if defined(SYS_statx)
115:             struct statx stx;
116:             // Pass dfd and relative entry name to statx (avoids full path allocation)
117:             if (syscall(SYS_statx, dfd, d_name, AT_SYMLINK_NOFOLLOW, STATX_SIZE | STATX_BLOCKS, &stx) == 0) {
118:                 fsize = stx.stx_size;
119:                 allocated_size = stx.stx_blocks * 512;
120:             } else {
121:                 struct stat st;
122:                 if (fstatat(dfd, d_name, &st, AT_SYMLINK_NOFOLLOW) == 0) {
123:                     fsize = st.st_size;
124:                     allocated_size = st.st_blocks * 512;
125:                 }
126:             }
127: #else
128:             struct stat st;
129:             if (fstatat(dfd, d_name, &st, AT_SYMLINK_NOFOLLOW) == 0) {
130:                 fsize = st.st_size;
131:                 allocated_size = st.st_blocks * 512;
132:             }
133: #endif
```
* **Explanation**: Queries sizes using the optimized `syscall` interface for `statx` when supported, falling back to POSIX `fstatat` relative calls if needed. Avoids full path strings entirely.

```cpp
135:             TreeNode* child = parent_node->get_or_create_child(std::string_view(d_name, name_len), false);
136:             {
137:                 std::unique_lock<std::shared_mutex> child_lock(child->mutex);
138:                 child->size = fsize;
139:                 child->allocated_size = allocated_size;
140:                 child->file_count = 1;
141:             }
```
* **Explanation**: Creates or fetches the child file node using `std::string_view` (zero-copy allocation if the node already exists), locking it to assign sizes.

```cpp
143:             dir_files_size += fsize;
144:             dir_files_allocated += allocated_size;
145:             dir_files_count++;
```
* **Explanation**: Aggregates thread-local directory statistics.

```cpp
146:         } else if (entry->d_type == DT_UNKNOWN) {
```
* **Explanation**: Fallback parsing using `fstatat` for filesystems that do not provide `d_type` in the directory entry (e.g., some network mounts).

---

## 2. src/platform/scanner_win.cpp (Windows Platform Walker)

The Windows kernel does not expose standard POSIX structures. It uses wide characters (UTF-16) and Win32 directory handles (`FindFirstFileW`/`FindNextFileW`).

```cpp
1: #include "scanner.hpp"
2: #ifdef _WIN32
3: #include <windows.h>
4: #include <string>
```
* **Explanation**: Guarded compile block. Imports `<windows.h>` on Windows targets.

```cpp
7: static std::wstring utf8_to_utf16(const std::string& utf8) {
8:     if (utf8.empty()) return L"";
9:     int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
10:     std::wstring utf16(size, L'\0');
11:     MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &utf16[0], size);
12:     if (!utf16.empty() && utf16.back() == L'\0') utf16.pop_back();
13:     return utf16;
14: }
```
* **Explanation**:
  * UTF-8 to UTF-16 Converter: Windows uses 16-bit wide characters (`wchar_t`) for file paths. Since our C++ tree uses UTF-8 strings (`std::string`), we must convert them to UTF-16 wide strings (`std::wstring`).
  * `MultiByteToWideChar` maps strings to Windows UTF-16 character buffers.

```cpp
17: static std::string utf16_to_utf8(const std::wstring& utf16) {
18:     if (utf16.empty()) return "";
19:     int size = WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), -1, nullptr, 0, nullptr, nullptr);
20:     std::string utf8(size, '\0');
21:     WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), -1, &utf8[0], size, nullptr, nullptr);
22:     if (!utf8.empty() && utf8.back() == '\0') utf8.pop_back();
23:     return utf8;
24: }
```
* **Explanation**: Converts UTF-16 wide strings (`std::wstring`) back to standard UTF-8 strings (`std::string`).

```cpp
29:     std::string search_path = current_path;
30:     if (search_path.back() != '\\' && search_path.back() != '/') {
31:         search_path += "\\*";
32:     } else {
33:         search_path += "*";
34:     }
```
* **Explanation**: Appends wildcard paths for Win32 wildcard directory discovery.

```cpp
36:     std::wstring wsearch_path = utf8_to_utf16(search_path);
37:     WIN32_FIND_DATAW find_data;
38:     HANDLE find_handle = FindFirstFileW(wsearch_path.c_str(), &find_data);
```
* **Explanation**: Invokes wide-character search handles on Windows storage.

```cpp
68:         // Skip reparse points (symlinks/junctions) to prevent infinite loops
69:         if (find_data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
70:             continue;
71:         }
```
* **Explanation**:
  * **Reparse Point Guard**: Skips directory junctions, symlinks, and volume mount points, preventing loops.

```cpp
81:             // Get compressed/allocated size on disk
82:             std::wstring wfull_path = utf8_to_utf16(full_path);
83:             DWORD high_size = 0;
84:             DWORD low_size = GetCompressedFileSizeW(wfull_path.c_str(), &high_size);
85:             if (low_size != INVALID_FILE_SIZE || GetLastError() == NO_ERROR) {
86:                 allocated_size = (static_cast<uint64_t>(high_size) << 32) | low_size;
87:             }
```
* **Explanation**:
  * `GetCompressedFileSizeW(...)`: Calls the Win32 API to find the physical disk footprint of the file, handling Windows NTFS file compression and sparse files correctly.

---

## 3. src/platform/scanner_mac.cpp (macOS Platform Walker)

macOS is UNIX-based. It uses POSIX streams similarly to Linux but does not support Linux-specific system calls like `statx`. It is optimized to avoid per-entry heap string allocations.

```cpp
1: #include "scanner.hpp"
2: #ifdef __APPLE__
```
* **Explanation**: Guarded compile block. Compiles only on macOS (Apple) platforms.

```cpp
3: #include <dirent.h>
4: #include <sys/types.h>
5: #include <sys/stat.h>
6: #include <unistd.h>
7: #include <fcntl.h>
8: #include <cstring>
```
* **Explanation**: Includes standard POSIX and BSD directory headers.

```cpp
10: void Scanner::scan_directory(const std::string& current_path, TreeNode* parent_node,
11:                              uint64_t& dir_files_size, uint64_t& dir_files_allocated,
12:                              uint64_t& dir_files_count, uint64_t& dir_subdirs_count) {
```
* **Explanation**: Gathers local outputs using references.

```cpp
13:     DIR* dir = opendir(current_path.c_str());
14:     if (!dir) return;
```
* **Explanation**: Opens directory stream.

```cpp
16:     // Get directory file descriptor for fast relative path stat calls (avoids full path construction)
17:     int dfd = dirfd(dir);
18:     if (dfd < 0) {
19:         closedir(dir);
20:         return;
21:     }
```
* **Explanation**: Extracts the directory file descriptor (`dfd`) to support relative `fstatat` calls, bypassing costly full-path string constructions on macOS.

```cpp
23:     struct dirent* entry;
24:     while ((entry = readdir(dir)) != nullptr) {
25:         if (m_cancelled) [[unlikely]] break;
```
* **Explanation**: Traverse entries.

```cpp
27:         // Use raw C-string length check to avoid std::string allocation for skipped entries
28:         const char* d_name = entry->d_name;
29:         if (d_name[0] == '.') {
30:             if (d_name[1] == '\0') continue;                    // "."
31:             if (d_name[1] == '.' && d_name[2] == '\0') continue; // ".."
32:         }
```
* **Explanation**: Skips standard current (`.`) and parent (`..`) folders without allocating any heap memory.

```cpp
34:         size_t name_len = strlen(d_name);
```
* **Explanation**: Computes string length for reuse.

```cpp
36:         // Security check: Ignore abnormally long filenames (max 255 bytes)
37:         if (name_len > 255) continue;
```
* **Explanation**: Rejects overly long filenames.

```cpp
39:         // Security check: Ignore names containing dangerous control/escape characters (ASCII < 32)
40:         bool has_invalid_char = false;
41:         for (size_t i = 0; i < name_len; ++i) {
42:             if (static_cast<unsigned char>(d_name[i]) < 32) {
43:                 has_invalid_char = true;
44:                 break;
45:             }
46:         }
47:         if (has_invalid_char) continue;
```
* **Explanation**: Validates character codes. Skips files containing invalid control characters.

```cpp
49:         // Skip symlinks
50:         if (entry->d_type == DT_LNK) continue;
```
* **Explanation**: Skips symbolic links.

```cpp
52:         if (entry->d_type == DT_DIR) {
53:             std::string full_path = current_path == "/" ? "/" + std::string(d_name, name_len)
54:                                                         : current_path + "/" + std::string(d_name, name_len);
```
* **Explanation**: Constructs folder paths. String allocations occur only for subdirectories that need task scheduling.

```cpp
56:             // Skip macOS pseudo-filesystem mount points
57:             if (full_path == "/System/Volumes" || full_path == "/private/var/vm") continue;
```
* **Explanation**: Skips Apple virtual folders and swap directories.

```cpp
59:             // Use fstatat with relative name (fast: no kernel path traversal)
60:             struct stat st;
61:             if (fstatat(dfd, d_name, &st, AT_SYMLINK_NOFOLLOW) == 0) {
62:                 DirId id{st.st_dev, st.st_ino};
63:                 {
64:                     std::lock_guard<std::mutex> lock(m_visited_mutex);
65:                     if (m_visited_dirs.contains(id)) {
66:                         continue;
67:                     }
68:                     m_visited_dirs.insert(id);
69:                 }
70:             }
```
* **Explanation**: Performs a fast relative `fstatat` system call. Checks device and inode numbers against `m_visited_dirs` using C++20 `contains()` in $O(1)$ time to prevent cyclic directory loops.

```cpp
72:             TreeNode* child = parent_node->get_or_create_child(std::string_view(d_name, name_len), true);
73:             add_task(full_path, child);
74:             dir_subdirs_count++;
```
* **Explanation**: Registers the subdirectory in the tree using zero-copy `std::string_view` lookup, enqueues the traversal task, and increments counters.

```cpp
75:         } else if (entry->d_type == DT_REG) {
76:             // Use fstatat with relative name — avoids constructing full_path entirely for files
77:             struct stat st;
78:             uint64_t fsize = 0;
79:             uint64_t allocated_size = 0;
80:             if (fstatat(dfd, d_name, &st, AT_SYMLINK_NOFOLLOW) == 0) {
81:                 fsize = st.st_size;
82:                 allocated_size = st.st_blocks * 512;
83:             }
```
* **Explanation**: Performs a fast relative `fstatat` system call to retrieve sizes. `st.st_blocks * 512` accurately gets the physical block allocation size.

```cpp
85:             TreeNode* child = parent_node->get_or_create_child(std::string_view(d_name, name_len), false);
86:             {
87:                 std::unique_lock<std::shared_mutex> child_lock(child->mutex);
88:                 child->size = fsize;
89:                 child->allocated_size = allocated_size;
90:                 child->file_count = 1;
91:             }
```
* **Explanation**: Creates or retrieves the child file node with zero string copies, updating its statistics.

```cpp
93:             dir_files_size += fsize;
94:             dir_files_allocated += allocated_size;
95:             dir_files_count++;
```
* **Explanation**: Adds sizes to thread-local counters.

```cpp
96:         } else if (entry->d_type == DT_UNKNOWN) {
```
* **Explanation**: Fallback parser using `fstatat` for filesystems that do not provide `d_type` in the directory entry (e.g., some network mounts).
