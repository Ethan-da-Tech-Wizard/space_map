// ==============================================================================
// scanner_linux.cpp - Linux implementation of Scanner::scan_directory.
//
// Like the macOS version (opendir/readdir plus fstatat relative to the
// directory fd) but it additionally skips virtual filesystems such as /proc and
// /sys, and prefers the statx() system call when available to fetch only the
// size and block count it needs. Compiled only on Linux.
//
// How it fits: Scanner::scan_directory is declared in scanner.hpp and called by
// the worker threads in scanner.cpp for one directory at a time. This version
// adds the entries of that directory to the TreeNode tree, queues
// sub-directories as new tasks and returns the totals of the plain files via the
// reference parameters (the caller propagates them up the tree).
//
// Safety rules (same on every platform): symlinks are never followed or
// counted; names that are over-long (> 255 bytes) or contain control
// characters are ignored; the scan stays on the starting device; every
// directory is visited once (device + inode set).
//
// Known limits: apparent size (stx_size) and allocated size (stx_blocks * 512)
// are recorded per file; hard links are counted once per link.
// ==============================================================================
#include "scanner.hpp"
#ifdef __linux__
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/stat.h>
#include <cstring>

// Fallback ABI definitions: older glibc/kernel headers (before Linux 4.11) do
// not declare statx. The layout below must match the kernel struct exactly
// because the buffer is filled by a raw syscall.
// Minimal local definitions of struct statx and its flags for toolchains whose
// headers predate statx (kernel 4.11). Field order and sizes mirror the kernel ABI.
#ifndef STATX_BASIC_STATS
struct statx_timestamp {
    __int64_t tv_sec;
    __uint32_t tv_nsec;
    __int32_t __reserved;
};
struct statx {
    __uint32_t stx_mask;
    __uint32_t stx_blksize;
    __uint64_t stx_attributes;
    __uint32_t stx_nlink;
    __uint32_t stx_uid;
    __uint32_t stx_gid;
    __uint16_t stx_mode;
    __uint16_t __spare0[1];
    __uint64_t stx_ino;
    __uint64_t stx_size;
    __uint64_t stx_blocks;
    __uint64_t stx_attributes_mask;
    struct statx_timestamp stx_atime;
    struct statx_timestamp stx_btime;
    struct statx_timestamp stx_ctime;
    struct statx_timestamp stx_mtime;
    __uint32_t stx_rdev_major;
    __uint32_t stx_rdev_minor;
    __uint32_t stx_dev_major;
    __uint32_t stx_dev_minor;
    __uint64_t __spare2[14];
};
// Only the two request flags this file uses.
#define STATX_SIZE 0x00000200U
#define STATX_BLOCKS 0x00000400U
#endif

/**
 * Lists one directory of the scan (Linux version).
 *
 * @param current_path     Absolute path of the directory to list.
 * @param parent_node      Tree node that receives the entries found.
 * @param dir_files_size   [out] Sum of the logical sizes of the files directly inside.
 * @param dir_files_allocated [out] Sum of their on-disk (allocated) sizes.
 * @param dir_files_count  [out] Number of files directly inside.
 * @param dir_subdirs_count [out] Number of sub-directories queued.
 * Directories that cannot be opened (permissions, races) are skipped silently.
 * Runs concurrently on several threads, each with a different directory.
 */
void Scanner::scan_directory(const std::string& current_path, TreeNode* parent_node,
                             uint64_t& dir_files_size, uint64_t& dir_files_allocated,
                             uint64_t& dir_files_count, uint64_t& dir_subdirs_count) {
    DIR* dir = opendir(current_path.c_str());
    if (!dir) return;

    // Entries are stat'ed relative to this descriptor (fstatat/statx), which saves
    // the kernel from re-resolving the whole path for every file.
    int dfd = dirfd(dir); // Get directory file descriptor for fast relative path calls
    if (dfd < 0) {
        closedir(dir);
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // Check for cancellation per entry so Cancel reacts quickly even in huge directories.
        if (m_cancelled) [[unlikely]] break;

        // "." and ".." are tested by hand before strlen so skipped entries cost nothing.
        // Use raw C-string length check to avoid std::string allocation for skipped entries
        const char* d_name = entry->d_name;
        if (d_name[0] == '.') {
            if (d_name[1] == '\0') continue;                    // "."
            if (d_name[1] == '.' && d_name[2] == '\0') continue; // ".."
        }

        size_t name_len = strlen(d_name);

        // Defensive limits: no real filename exceeds 255 bytes, and control characters
        // could corrupt the UI or the paths we build.
        // Security check: Ignore abnormally long filenames (max 255 bytes)
        if (name_len > 255) continue;

        // Security check: Ignore names containing dangerous control/escape characters (ASCII < 32)
        bool has_invalid_char = false;
        for (size_t i = 0; i < name_len; ++i) {
            if (static_cast<unsigned char>(d_name[i]) < 32) {
                has_invalid_char = true;
                break;
            }
        }
        if (has_invalid_char) continue;

        // Symlinks could create loops or count data twice, so they are ignored entirely.
        // Skip symlinks
        if (entry->d_type == DT_LNK) continue;

        // d_type from readdir avoids a stat call for most entries (DT_UNKNOWN is handled below).
        if (entry->d_type == DT_DIR) {
            std::string full_path = current_path == "/" ? "/" + std::string(d_name, name_len)
                                                        : current_path + "/" + d_name;
            // Pseudo filesystems report huge or meaningless sizes and are not real disk
            // use; /mnt and /media hold other volumes that would double count.
            // Pseudo filesystems report huge or meaningless sizes and are not real disk use.
            // Ignore virtual/pseudo-filesystems and root mount points when scanning /
            if (full_path == "/proc" || full_path == "/sys" || full_path == "/dev" || full_path == "/run" ||
                full_path == "/mnt" || full_path == "/media" || full_path == "/sys/fs/cgroup") {
                continue;
            }

            // Stay on the root's device (st_dev) so mounts are not crossed, and skip
            // directories already seen (device + inode) to survive bind mounts and loops.
            // If stat fails the directory is still queued; the worker will just skip it.
            // Check for mount/bind cycles and enforce single-filesystem boundary (st_dev matching)
            struct stat st;
            if (fstatat(dfd, d_name, &st, AT_SYMLINK_NOFOLLOW) == 0) {
                if (m_root_dev != 0 && st.st_dev != m_root_dev) {
                    continue; // Skip directories located on a different filesystem/device
                }
                DirId id{st.st_dev, st.st_ino};
                {
                    std::lock_guard<std::mutex> lock(m_visited_mutex);
                    if (m_visited_dirs.contains(id)) {
                        continue;
                    }
                    m_visited_dirs.insert(id);
                }
            }

            // Create the node now (cheap) so the tree shape exists as soon as the task
            // is queued; its contents arrive when a worker lists it.
            TreeNode* child = parent_node->get_or_create_child(std::string_view(d_name, name_len), true);
            add_task(full_path, child);
            dir_subdirs_count++;
        // Regular file: record both sizes. Several threads never touch the same file.
        } else if (entry->d_type == DT_REG) {
            uint64_t fsize = 0;
            uint64_t allocated_size = 0;

// statx lets us ask for just STATX_SIZE | STATX_BLOCKS (cheaper than a full
// stat); if the call fails we fall back to fstatat. stx_blocks counts 512-byte units.
// statx lets us ask for just STATX_SIZE | STATX_BLOCKS; if the call fails we
// fall back to fstatat. stx_blocks counts 512-byte units.
#if defined(SYS_statx)
            struct statx stx;
            // Pass dfd and the relative name (no full path to build or resolve).
            // Pass dfd and relative entry name to statx (avoids full path allocation)
            if (syscall(SYS_statx, dfd, d_name, AT_SYMLINK_NOFOLLOW, STATX_SIZE | STATX_BLOCKS, &stx) == 0) {
                fsize = stx.stx_size;
                allocated_size = stx.stx_blocks * 512;
            } else {
                struct stat st;
                if (fstatat(dfd, d_name, &st, AT_SYMLINK_NOFOLLOW) == 0) {
                    fsize = st.st_size;
                    allocated_size = st.st_blocks * 512;
                }
            }
#else
            struct stat st;
            if (fstatat(dfd, d_name, &st, AT_SYMLINK_NOFOLLOW) == 0) {
                fsize = st.st_size;
                allocated_size = st.st_blocks * 512;
            }
#endif

            // File nodes are leaves, so they carry their own size/count directly; the
            // directory totals are added by the caller through propagate_stats().
            TreeNode* child = parent_node->get_or_create_child(std::string_view(d_name, name_len), false);
            {
                std::unique_lock<std::shared_mutex> child_lock(child->mutex);
                child->size = fsize;
                child->allocated_size = allocated_size;
                child->file_count = 1;
            }

            dir_files_size += fsize;
            dir_files_allocated += allocated_size;
            dir_files_count++;
        // Fallback for filesystems that do not return d_type (some network or FUSE
        // mounts): the entry kind must come from stat. Same rules as the branches above.
        } else if (entry->d_type == DT_UNKNOWN) {
            // Fallback for filesystems that do not return d_type
            struct stat st;
            if (fstatat(dfd, d_name, &st, AT_SYMLINK_NOFOLLOW) == 0) {
                if (S_ISDIR(st.st_mode)) {
                    std::string full_path = current_path == "/" ? "/" + std::string(d_name, name_len)
                                                                : current_path + "/" + d_name;
                    if (full_path == "/proc" || full_path == "/sys" || full_path == "/dev" || full_path == "/run" ||
                        full_path == "/mnt" || full_path == "/media" || full_path == "/sys/fs/cgroup") {
                        continue;
                    }
                    if (m_root_dev != 0 && st.st_dev != m_root_dev) {
                        continue; // Skip directories located on a different filesystem/device
                    }
                    DirId id{st.st_dev, st.st_ino};
                    {
                        std::lock_guard<std::mutex> lock(m_visited_mutex);
                        if (m_visited_dirs.contains(id)) {
                            continue;
                        }
                        m_visited_dirs.insert(id);
                    }
                    TreeNode* child = parent_node->get_or_create_child(std::string_view(d_name, name_len), true);
                    add_task(full_path, child);
                    dir_subdirs_count++;
                } else if (S_ISREG(st.st_mode)) {
                    TreeNode* child = parent_node->get_or_create_child(std::string_view(d_name, name_len), false);
                    {
                        std::unique_lock<std::shared_mutex> child_lock(child->mutex);
                        child->size = st.st_size;
                        child->allocated_size = st.st_blocks * 512;
                        child->file_count = 1;
                    }
                    dir_files_size += st.st_size;
                    dir_files_allocated += st.st_blocks * 512;
                    dir_files_count++;
                }
            }
        }
    }
    closedir(dir);
}
#endif
