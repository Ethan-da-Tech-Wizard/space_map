// ==============================================================================
// scanner_mac.cpp - macOS implementation of Scanner::scan_directory.
//
// Uses POSIX opendir/readdir and fstatat() relative to the directory's file
// descriptor, which avoids building a full path (and a kernel path walk) for
// every file. Symlinks are never followed. Compiled only on Apple platforms.
//
// How it fits: Scanner::scan_directory is declared in scanner.hpp and called by
// the worker threads in scanner.cpp, one directory at a time. This version adds
// the directory's entries to the TreeNode tree, queues sub-directories as new
// tasks and returns the totals of the plain files through the reference
// parameters (the caller propagates them up the tree).
//
// Known limits: hard links are counted once per link; unlike the Linux version
// the DT_UNKNOWN fallback below does not apply the device or pseudo-filesystem
// checks (macOS filesystems normally report d_type, so it is rarely used).
// ==============================================================================
#include "scanner.hpp"
#ifdef __APPLE__
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

/**
 * Lists one directory of the scan (macOS version).
 *
 * For each entry: skip ".", ".." and suspicious names, skip symlinks, then
 * handle directories (cycle/device checks, queue them) and regular files
 * (record size and allocated size).
 *
 * @param current_path     Absolute path of the directory to list.
 * @param parent_node      Tree node that receives the entries found.
 * @param dir_files_size   [out] Sum of the logical sizes of files directly inside.
 * @param dir_files_allocated [out] Sum of their allocated (on-disk) sizes.
 * @param dir_files_count  [out] Number of files directly inside.
 * @param dir_subdirs_count [out] Number of sub-directories queued.
 * Unopenable directories (permissions such as TCC, races) are skipped silently.
 */
void Scanner::scan_directory(const std::string& current_path, TreeNode* parent_node,
                             uint64_t& dir_files_size, uint64_t& dir_files_allocated,
                             uint64_t& dir_files_count, uint64_t& dir_subdirs_count) {
    DIR* dir = opendir(current_path.c_str());
    if (!dir) return;

    // Entries are stat'ed relative to this descriptor, which avoids building a
    // full path and a kernel path walk for every file.
    // Get directory file descriptor for fast relative path stat calls (avoids full path construction)
    int dfd = dirfd(dir);
    if (dfd < 0) {
        closedir(dir);
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // Check cancellation per entry so Cancel responds quickly in huge directories.
        if (m_cancelled) [[unlikely]] break;

        // "." and ".." are recognised by hand, before strlen, so skipped entries cost nothing.
        const char* d_name = entry->d_name;
        if (d_name[0] == '.') {
            if (d_name[1] == '\0') continue;                    // "."
            if (d_name[1] == '.' && d_name[2] == '\0') continue; // ".."
        }

        size_t name_len = strlen(d_name);

        // Defensive limits: no valid name is longer than 255 bytes, and control
        // characters could corrupt the UI or the paths built from the name.
        if (name_len > 255) continue;

        bool has_invalid_char = false;
        for (size_t i = 0; i < name_len; ++i) {
            if (static_cast<unsigned char>(d_name[i]) < 32) {
                has_invalid_char = true;
                break;
            }
        }
        if (has_invalid_char) continue;

        // Symlinks could loop or double-count data, so they are neither followed nor counted.
        if (entry->d_type == DT_LNK) continue;

        if (entry->d_type == DT_DIR) {
            std::string full_path = current_path == "/" ? "/" + std::string(d_name, name_len)
                                                        : current_path + "/" + d_name;

            // Firmlinked/virtual locations (the data volume is already reachable through
            // "/") would double count; /private/var/vm holds swap and sleep images.
            if (full_path == "/System/Volumes" || full_path == "/private/var/vm") continue;

            // Stay on the root's device (do not descend into other mounted volumes) and
            // skip directories already seen (device + inode) to avoid cycles and double
            // counting. fstatat with the relative name avoids a kernel path traversal.
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

            // Create the node now; its contents are filled in when a worker lists it.
            TreeNode* child = parent_node->get_or_create_child(std::string_view(d_name, name_len), true);
            add_task(full_path, child);
            dir_subdirs_count++;
        } else if (entry->d_type == DT_REG) {
            // st_blocks is in 512-byte units on macOS, so blocks * 512 is the space the
            // file really occupies on disk (allocated), which can differ from st_size
            // (sparse or compressed files).
            struct stat st;
            uint64_t fsize = 0;
            uint64_t allocated_size = 0;
            if (fstatat(dfd, d_name, &st, AT_SYMLINK_NOFOLLOW) == 0) {
                fsize = st.st_size;
                allocated_size = st.st_blocks * 512;
            }

            // File nodes are leaves and carry their own numbers; directory totals come
            // from the caller's propagate_stats().
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
        } else if (entry->d_type == DT_UNKNOWN) {
            // Fallback for filesystems that do not fill d_type (e.g. some network mounts):
            // the entry kind must come from stat(). Same recording logic as above.
            struct stat st;
            std::string full_path = current_path == "/" ? "/" + std::string(d_name, name_len)
                                                        : current_path + "/" + d_name;
            if (fstatat(dfd, d_name, &st, AT_SYMLINK_NOFOLLOW) == 0) {
                if (S_ISDIR(st.st_mode)) {
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
