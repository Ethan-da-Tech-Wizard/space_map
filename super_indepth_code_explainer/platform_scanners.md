# SpaceMap Architecture Book: Chapter 5

This chapter provides a complete line-by-line, character-by-character architectural explanation of the platform-specific directory crawlers (`src/platform/scanner_linux.cpp`, `src/platform/scanner_win.cpp`, and `src/platform/scanner_mac.cpp`).

---

# CHAPTER 5: Platform-Specific Filesystem Walkers

Because directory structure layouts and size metadata queries differ radically between operating systems, SpaceMap utilizes native system APIs directly rather than generic abstractions. This chapter details how SpaceMap communicates with Linux, Windows, and macOS kernels at the raw system level.

---

## 1. src/platform/scanner_linux.cpp (Linux Platform Walker)

The Linux implementation uses low-level POSIX file descriptors, relative path lookups, and the modern `statx` system call to query metadata.

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
* **Explanation**:
  * `<dirent.h>`: Exposes POSIX directory stream constructs (`DIR`, `dirent`, `opendir`, `readdir`).
  * `<sys/types.h>` and `<sys/stat.h>`: Declares file modes, attributes, and file system info.
  * `<fcntl.h>`: Provides file control flags like `AT_FDCWD` or `AT_SYMLINK_NOFOLLOW`.
  * `<unistd.h>`: Declares standard symbolic constants and system call wrappers (like `syscall`).
  * `<sys/syscall.h>`: Maps kernel system call numbers (like `SYS_statx`).
  * `<linux/stat.h>`: Kernel header defining kernel-level file stat structures.
  * `<cstring>`: Standard C string manipulation functions (like `strcmp`).

```cpp
12: #ifndef STATX_BASIC_STATS
13: struct statx_timestamp {
14:     __int64_t tv_sec;
15:     __uint32_t tv_nsec;
16:     __int32_t __reserved;
17: };
```
* **Explanation**: If compile target glibc headers are older than the `statx` system call introduction, manually declares the `statx_timestamp` structure. Contains standard 64-bit seconds and 32-bit nanoseconds.

```cpp
18: struct statx {
19:     __uint32_t stx_mask;
20:     __uint32_t stx_blksize;
21:     __uint64_t stx_attributes;
22:     __uint32_t stx_nlink;
23:     __uint32_t stx_uid;
24:     __uint32_t stx_gid;
25:     __uint16_t stx_mode;
26:     __uint16_t __spare0[1];
27:     __uint64_t stx_ino;
28:     __uint64_t stx_size;
29:     __uint64_t stx_blocks;
30:     __uint64_t stx_attributes_mask;
31:     struct statx_timestamp stx_atime;
32:     struct statx_timestamp stx_btime;
33:     struct statx_timestamp stx_ctime;
34:     struct statx_timestamp stx_mtime;
35:     __uint32_t stx_rdev_major;
36:     __uint32_t stx_rdev_minor;
37:     __uint32_t stx_dev_major;
38:     __uint32_t stx_dev_minor;
39:     __uint64_t __spare2[14];
40: };
```
* **Explanation**: Manually maps the full kernel layout of the `statx` structure to ensure the compiler offsets line up with the Linux kernel response buffer on all systems.

```cpp
41: #define STATX_SIZE 0x00000200U
42: #define STATX_BLOCKS 0x00000400U
43: #endif
```
* **Explanation**: Defines bit-mask constants indicating size and block counts.

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
* **Explanation**:
  * `opendir(...)`: Linux POSIX function that opens a directory stream.
  * If the function returns a null pointer (`!dir`), it indicates permission denied (e.g. `/root` folder) or file not found. The scanner returns silently.

```cpp
51:     int dfd = dirfd(dir); // Get directory file descriptor for fast relative path calls
52:     if (dfd < 0) {
53:         closedir(dir);
54:         return;
55:     }
```
* **Explanation**:
  * `dirfd(dir)`: Extracts the underlying system file descriptor (an integer referring to the open folder handle in the kernel).
  * Relative System Calls: Having a directory file descriptor (`dfd`) allows us to execute file calls relative to this descriptor. Instead of assembling long paths (like `/usr/share/doc/.../index.html`), we just pass the file descriptor and the relative filename (`index.html`). This prevents string allocations, saves memory bandwidth, and blocks **TOCTOU** (Time-of-Check to Time-of-Use) file-swapping attacks.
  * `closedir(dir)`: If the file descriptor is invalid, closes the directory structure and aborts.

```cpp
57:     struct dirent* entry;
58:     while ((entry = readdir(dir)) != nullptr) {
59:         if (m_cancelled) [[unlikely]] break;
```
* **Explanation**:
  * `readdir(dir)`: Sequentially reads the next file or folder within the directory stream.
  * Returns `nullptr` when the directory traversal reaches the end.
  * If `m_cancelled` is true, immediately aborts the loop. The `[[unlikely]]` hint optimizes assembly layout for execution speed.

```cpp
61:         std::string name = entry->d_name;
62:         if (name == "." || name == "..") continue;
```
* **Explanation**: Skips standard current directory (`.`) and parent directory (`..`) references to prevent infinite recursion loops.

```cpp
64:         // Security check: Ignore abnormally long filenames (max 255 bytes)
65:         if (name.length() > 255) continue;
```
* **Explanation**: The standard maximum filename length on modern Linux filesystems (ext4, XFS, Btrfs) is 255 bytes. Rejects longer filenames as malicious or corrupt entries.

```cpp
67:         // Security check: Ignore names containing dangerous control/escape characters (ASCII < 32)
68:         bool has_invalid_char = false;
69:         for (char c : name) {
70:             if (static_cast<unsigned char>(c) < 32) {
71:                 has_invalid_char = true;
72:                 break;
73:             }
74:         }
75:         if (has_invalid_char) continue;
```
* **Explanation**: Filename Sanitization: Scans the filename characters. If any character is below ASCII value 32 (control characters like newline `\n`, carriage return `\r`, backspace `\b`, or null `\0`), the file is skipped. This prevents console manipulation attacks, Qt UI rendering corruption, and exploits using hidden filenames.

```cpp
77:         // Skip symlinks
78:         if (entry->d_type == DT_LNK) continue;
```
* **Explanation**: Skips symbolic links (`DT_LNK`) to prevent escaping target directories or entering recursion cycles.

```cpp
80:         if (entry->d_type == DT_DIR) {
81:             std::string full_path = current_path == "/" ? "/" + name : current_path + "/" + name;
```
* **Explanation**: If directory type, maps absolute path string. Handles root `/` concatenation specifically to avoid double slashes.

```cpp
82:             // Ignore virtual/pseudo-filesystems
83:             if (full_path == "/proc" || full_path == "/sys" || full_path == "/dev" || full_path == "/run") {
84:                 continue;
85:             }
```
* **Explanation**: Blocks traversing dynamic kernel directories which contain virtual nodes that do not consume space on physical disks.

```cpp
87:             // Check for mount/bind cycles using inode dev IDs via fast fstatat
88:             struct stat st;
89:             if (fstatat(dfd, entry->d_name, &st, AT_SYMLINK_NOFOLLOW) == 0) {
90:                 DirId id{st.st_dev, st.st_ino};
91:                 {
92:                     std::lock_guard<std::mutex> lock(m_visited_mutex);
93:                     if (m_visited_dirs.count(id)) {
94:                         continue;
95:                     }
96:                     m_visited_dirs.insert(id);
97:                 }
98:             }
```
* **Explanation**:
  * `fstatat(...)`: Fast stat relative to the directory file descriptor `dfd`.
  * `st.st_dev` / `st.st_ino`: Fetches device ID and inode index.
  * Checks globally under `m_visited_mutex`. If already visited, skips the folder to block bind-mount cycles and circular recursion loops.

```cpp
100:             TreeNode* child = parent_node->get_or_create_child(name, true);
101:             add_task(full_path, child);
102:             dir_subdirs_count++;
```
* **Explanation**: Creates directory tree child, enqueues path to task vector, and increments counter.

```cpp
103:         } else if (entry->d_type == DT_REG) {
104:             uint64_t fsize = 0;
105:             uint64_t allocated_size = 0;
```
* **Explanation**: Registers regular files.

```cpp
107: #if defined(SYS_statx)
108:             struct statx stx;
109:             // Pass dfd and relative entry name to statx (avoids full path allocation)
110:             if (syscall(SYS_statx, dfd, entry->d_name, AT_SYMLINK_NOFOLLOW, STATX_SIZE | STATX_BLOCKS, &stx) == 0) {
111:                 fsize = stx.stx_size;
112:                 allocated_size = stx.stx_blocks * 512;
```
* **Explanation**:
  * If the system call number `SYS_statx` is defined:
    * `syscall(...)`: Triggers a direct Linux kernel call. Passes directory file descriptor `dfd` and relative filename.
    * `STATX_SIZE | STATX_BLOCKS`: Requests only size and block allocation from the filesystem driver (bypassing timestamps/permissions query overhead).
    * `stx.stx_blocks * 512`: Converts 512-byte blocks count to physical disk allocation size (handles sparse files).

```cpp
113:             } else {
114:                 struct stat st;
115:                 if (fstatat(dfd, entry->d_name, &st, AT_SYMLINK_NOFOLLOW) == 0) {
116:                     fsize = st.st_size;
117:                     allocated_size = st.st_blocks * 512;
118:                 }
119:             }
```
* **Explanation**: Fallback. If `syscall` returns failure (meaning older kernel), uses standard `fstatat`.

```cpp
120: #else
121:             struct stat st;
122:             if (fstatat(dfd, entry->d_name, &st, AT_SYMLINK_NOFOLLOW) == 0) {
123:                 fsize = st.st_size;
124:                 allocated_size = st.st_blocks * 512;
125:             }
126: #endif
```
* **Explanation**: Preprocessor branch if compile system headers lack `SYS_statx` entirely.

```cpp
128:             TreeNode* child = parent_node->get_or_create_child(name, false);
129:             {
130:                 std::unique_lock<std::shared_mutex> child_lock(child->mutex);
131:                 child->size = fsize;
132:                 child->allocated_size = allocated_size;
133:                 child->file_count = 1;
134:             }
```
* **Explanation**: Inserts/retrieves file node under exclusive child lock write control, writing statistics.

```cpp
136:             dir_files_size += fsize;
137:             dir_files_allocated += allocated_size;
138:             dir_files_count++;
```
* **Explanation**: Aggregates metadata to thread-local counters.

```cpp
139:         } else if (entry->d_type == DT_UNKNOWN) {
```
* **Explanation**: Handles filesystems (like legacy NFS/FAT formats) that do not expose file types within the initial `readdir` directory entry structure.

```cpp
141:             struct stat st;
142:             if (fstatat(dfd, entry->d_name, &st, AT_SYMLINK_NOFOLLOW) == 0) {
143:                 if (S_ISDIR(st.st_mode)) {
```
* **Explanation**: Resolves stat details and checks modes.

```cpp
144:                     std::string full_path = current_path == "/" ? "/" + name : current_path + "/" + name;
145:                     if (full_path == "/proc" || full_path == "/sys" || full_path == "/dev" || full_path == "/run") {
146:                         continue;
147:                     }
148:                     DirId id{st.st_dev, st.st_ino};
149:                     {
150:                         std::lock_guard<std::mutex> lock(m_visited_mutex);
151:                         if (m_visited_dirs.count(id)) {
152:                             continue;
153:                         }
154:                         m_visited_dirs.insert(id);
155:                     }
156:                     TreeNode* child = parent_node->get_or_create_child(name, true);
157:                     add_task(full_path, child);
158:                     dir_subdirs_count++;
```
* **Explanation**: Performs the same directory validation, cycle check, and task enqueue operations if verified as a directory.

```cpp
159:                 } else if (S_ISREG(st.st_mode)) {
160:                     TreeNode* child = parent_node->get_or_create_child(name, false);
161:                     {
162:                         std::unique_lock<std::shared_mutex> child_lock(child->mutex);
163:                         child->size = st.st_size;
164:                         child->allocated_size = st.st_blocks * 512;
165:                         child->file_count = 1;
166:                     }
167:                     dir_files_size += st.st_size;
168:                     dir_files_allocated += st.st_blocks * 512;
169:                     dir_files_count++;
170:                 }
171:             }
172:         }
173:     }
174:     closedir(dir);
175: }
```
* **Explanation**:
  * Performs the same file statistics assignment if verified as a regular file.
  * `closedir(dir)`: Closes the directory stream. This releases the directory file descriptor back to the Linux kernel, preventing resource leakage.

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
  * `MultiByteToWideChar`: Calls it twice:
    1. The first call (passing `nullptr` as buffer and `0` as size) queries the exact wide-character length required to store the converted string.
    2. The second call performs the actual conversion into the pre-allocated `std::wstring` buffer.
  * `CP_UTF8`: Instructs Windows that the input source is UTF-8.
  * `pop_back()`: Clears the trailing null terminator character if present.

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
* **Explanation**: Converts UTF-16 wide strings (`std::wstring`) back to standard UTF-8 strings (`std::string`) using a similar double-call pattern with `WideCharToMultiByte`.

```cpp
26: void Scanner::scan_directory(const std::string& current_path, TreeNode* parent_node,
27:                              uint64_t& dir_files_size, uint64_t& dir_files_allocated,
28:                              uint64_t& dir_files_count, uint64_t& dir_subdirs_count) {
```
* **Explanation**: Windows scan entry point.

```cpp
29:     std::string search_path = current_path;
30:     if (search_path.back() != '\\' && search_path.back() != '/') {
31:         search_path += "\\*";
32:     } else {
33:         search_path += "*";
34:     }
```
* **Explanation**: Windows requires directory queries to end with a wildcard character (`*` or `\*`). This tells the Win32 file search API to match all files and folders inside the directory.

```cpp
36:     std::wstring wsearch_path = utf8_to_utf16(search_path);
37:     WIN32_FIND_DATAW find_data;
38:     HANDLE find_handle = FindFirstFileW(wsearch_path.c_str(), &find_data);
```
* **Explanation**:
  * Converts path to UTF-16.
  * `WIN32_FIND_DATAW`: Structure receiving metadata (attributes, sizes, names).
  * `FindFirstFileW`: Opens the search handle and gets the first child entry.

```cpp
40:     if (find_handle == INVALID_HANDLE_VALUE) return;
```
* **Explanation**: Aborts if search handle is invalid (permission error / folder empty).

```cpp
42:     do {
43:         if (m_cancelled) [[unlikely]] break;
```
* **Explanation**: Loop iteration.

```cpp
45:         std::wstring wname = find_data.cFileName;
46:         if (wname == L"." || wname == L"..") continue;
```
* **Explanation**: Skips self and parent directory pointers.

```cpp
48:         std::string name = utf16_to_utf8(wname);
```
* **Explanation**: Converts wide name back to UTF-8.

```cpp
49:         // Security check: Ignore abnormally long filenames (max 255 bytes)
50:         if (name.length() > 255) continue;
51: 
52:         // Security check: Ignore names containing dangerous control/escape characters (ASCII < 32)
53:         bool has_invalid_char = false;
54:         for (char c : name) {
55:             if (static_cast<unsigned char>(c) < 32) {
56:                 has_invalid_char = true;
57:                 break;
58:             }
59:         }
60:         if (has_invalid_char) continue;
```
* **Explanation**: Filename length check and control character sanitization.

```cpp
61:         std::string full_path = current_path;
62:         if (full_path.back() != '\\' && full_path.back() != '/') {
63:             full_path += "\\" + name;
64:         } else {
65:             full_path += name;
66:         }
```
* **Explanation**: Constructs sub-path string using Windows backslash separators.

```cpp
68:         // Skip reparse points (symlinks/junctions) to prevent infinite loops
69:         if (find_data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
70:             continue;
71:         }
```
* **Explanation**:
  * **Reparse Point Guard**: Skips directory junctions, symlinks, and volume mount points, preventing directory loops.

```cpp
73:         if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
74:             TreeNode* child = parent_node->get_or_create_child(name, true);
75:             add_task(full_path, child);
76:             dir_subdirs_count++;
```
* **Explanation**: Handles folder structure registration and creates subtasks.

```cpp
77:         } else {
78:             uint64_t fsize = (static_cast<uint64_t>(find_data.nFileSizeHigh) << 32) | find_data.nFileSizeLow;
79:             uint64_t allocated_size = fsize;
```
* **Explanation**: Combines the high and low 32-bit integers of the file size into a single 64-bit value using shift and bitwise operations.

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
  * `GetCompressedFileSizeW(...)`: Calls the Win32 API to find the physical disk footprint of the file. Handles NTFS file compression and sparse files correctly.

```cpp
89:             TreeNode* child = parent_node->get_or_create_child(name, false);
90:             {
91:                 std::unique_lock<std::shared_mutex> child_lock(child->mutex);
92:                 child->size = fsize;
93:                 child->allocated_size = allocated_size;
94:                 child->file_count = 1;
95:             }
```
* **Explanation**: Registers node variables under exclusive lock.

```cpp
97:             dir_files_size += fsize;
98:             dir_files_allocated += allocated_size;
99:             dir_files_count++;
100:         }
101:     } while (FindNextFileW(find_handle, &find_data));
```
* **Explanation**:
  * Aggregates stats.
  * `FindNextFileW`: Reads the next entry in the loop until finished.

```cpp
103:     FindClose(find_handle);
104: }
```
* **Explanation**: Closes the search handle to prevent resource leaks.

---

## 3. src/platform/scanner_mac.cpp (macOS Platform Walker)

macOS is UNIX-based. It uses POSIX streams similarly to Linux but does not support Linux-specific system calls like `statx`.

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
7: #include <cstring>
```
* **Explanation**: Includes standard POSIX / BSD headers.

```cpp
9: void Scanner::scan_directory(const std::string& current_path, TreeNode* parent_node,
10:                              uint64_t& dir_files_size, uint64_t& dir_files_allocated,
11:                              uint64_t& dir_files_count, uint64_t& dir_subdirs_count) {
12:     DIR* dir = opendir(current_path.c_str());
13:     if (!dir) return;
```
* **Explanation**: Opens directory stream.

```cpp
15:     struct dirent* entry;
16:     while ((entry = readdir(dir)) != nullptr) {
17:         if (m_cancelled) [[unlikely]] break;
```
* **Explanation**: Traverses files.

```cpp
19:         std::string name = entry->d_name;
20:         if (name == "." || name == "..") continue;
```
* **Explanation**: Skips self and parent.

```cpp
22:         // Security check: Ignore abnormally long filenames (max 255 bytes)
23:         if (name.length() > 255) continue;
24: 
25:         // Security check: Ignore names containing dangerous control/escape characters (ASCII < 32)
26:         bool has_invalid_char = false;
27:         for (char c : name) {
28:             if (static_cast<unsigned char>(c) < 32) {
29:                 has_invalid_char = true;
30:                 break;
31:             }
32:         }
33:         if (has_invalid_char) continue;
```
* **Explanation**: Sanitization check.

```cpp
35:         // Skip symlinks
36:         if (entry->d_type == DT_LNK) continue;
```
* **Explanation**: Skips symbolic links.

```cpp
38:         std::string full_path = current_path == "/" ? "/" + name : current_path + "/" + name;
```
* **Explanation**: Constructs absolute path.

```cpp
40:         if (entry->d_type == DT_DIR) {
41:             struct stat st;
42:             if (stat(full_path.c_str(), &st) == 0) {
43:                 DirId id{st.st_dev, st.st_ino};
44:                 {
45:                     std::lock_guard<std::mutex> lock(m_visited_mutex);
46:                     if (m_visited_dirs.count(id)) {
47:                         continue;
48:                     }
49:                     m_visited_dirs.insert(id);
50:                 }
51:             }
```
* **Explanation**:
  * Resolves file statistics via POSIX `stat(...)`.
  * Checks device/inode combinations to prevent infinite loops.

```cpp
53:             TreeNode* child = parent_node->get_or_create_child(name, true);
54:             add_task(full_path, child);
55:             dir_subdirs_count++;
```
* **Explanation**: Creates node, enqueues path, and increments folder counters.

```cpp
56:         } else if (entry->d_type == DT_REG) {
57:             struct stat st;
58:             uint64_t fsize = 0;
59:             uint64_t allocated_size = 0;
60:             if (stat(full_path.c_str(), &st) == 0) {
61:                 fsize = st.st_size;
62:                 allocated_size = st.st_blocks * 512;
63:             }
```
* **Explanation**:
  * Reads file stats.
  * `st.st_blocks * 512`: Resolves disk allocations on APFS filesystem blocks.

```cpp
65:             TreeNode* child = parent_node->get_or_create_child(name, false);
66:             {
67:                 std::unique_lock<std::shared_mutex> child_lock(child->mutex);
68:                 child->size = fsize;
69:                 child->allocated_size = allocated_size;
70:                 child->file_count = 1;
71:             }
```
* **Explanation**: Maps metrics to tree child.

```cpp
73:             dir_files_size += fsize;
74:             dir_files_allocated += allocated_size;
75:             dir_files_count++;
76:         }
77:     }
78:     closedir(dir);
79: }
```
* **Explanation**:
  * Aggregates stats.
  * Closes the directory stream.
