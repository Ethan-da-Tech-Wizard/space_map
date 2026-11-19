// ==============================================================================
// scanner_mac.cpp - macOS implementation of Scanner::scan_directory.
//
// Uses POSIX opendir/readdir and fstatat() relative to the directory's file
// descriptor, which avoids building a full path (and a kernel path walk) for
// every file. Symlinks are never followed. Compiled only on Apple platforms.
// ==============================================================================
#include "scanner.hpp"
#ifdef __APPLE__
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

// Enumerate one directory. For each entry: skip ".", ".." and suspicious names,
// skip symlinks, then handle directories (cycle/device checks, queue them) and
// regular files (record size and allocated size).
void Scanner::scan_directory(const std::string& current_path, TreeNode* parent_node,
                             uint64_t& dir_files_size, uint64_t& dir_files_allocated,
                             uint64_t& dir_files_count, uint64_t& dir_subdirs_count) {
    DIR* dir = opendir(current_path.c_str());
    if (!dir) return;

    // Get directory file descriptor for fast relative path stat calls (avoids full path construction)
    int dfd = dirfd(dir);
    if (dfd < 0) {
        closedir(dir);
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (m_cancelled) [[unlikely]] break;

        // Use raw C-string length check to avoid std::string allocation for skipped entries
        const char* d_name = entry->d_name;
        if (d_name[0] == '.') {
            if (d_name[1] == '\0') continue;                    // "."
            if (d_name[1] == '.' && d_name[2] == '\0') continue; // ".."
        }

        size_t name_len = strlen(d_name);

        // Names longer than 255 bytes cannot be valid on these filesystems.
        // Security check: Ignore abnormally long filenames (max 255 bytes)
        if (name_len > 255) continue;

        // Control characters are skipped so odd names cannot corrupt display/paths.
        // Security check: Ignore names containing dangerous control/escape characters (ASCII < 32)
        bool has_invalid_char = false;
        for (size_t i = 0; i < name_len; ++i) {
            if (static_cast<unsigned char>(d_name[i]) < 32) {
                has_invalid_char = true;
                break;
            }
        }
        if (has_invalid_char) continue;

        // Symlinks could loop or double-count data, so they are not followed or counted.
        // Skip symlinks
        if (entry->d_type == DT_LNK) continue;

        if (entry->d_type == DT_DIR) {
            std::string full_path = current_path == "/" ? "/" + std::string(d_name, name_len)
                                                        : current_path + "/" + d_name;

            // Firmlinked/virtual locations that would duplicate data or are not real files.
            // Skip macOS pseudo-filesystem mount points
            if (full_path == "/System/Volumes" || full_path == "/private/var/vm") continue;

            // Stay on the root's device (do not descend into other mounted volumes) and
            // skip directories already seen (device+inode) to avoid cycles/double counting.
            // Use fstatat with relative name (fast: no kernel path traversal)
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

            TreeNode* child = parent_node->get_or_create_child(std::string_view(d_name, name_len), true);
            add_task(full_path, child);
            dir_subdirs_count++;
        } else if (entry->d_type == DT_REG) {
            // st_blocks is in 512-byte units on macOS, so blocks * 512 is the space the
            // file really occupies on disk (allocated), which can differ from st_size.
            // Use fstatat with relative name — avoids constructing full_path entirely for files
            struct stat st;
            uint64_t fsize = 0;
            uint64_t allocated_size = 0;
            if (fstatat(dfd, d_name, &st, AT_SYMLINK_NOFOLLOW) == 0) {
                fsize = st.st_size;
                allocated_size = st.st_blocks * 512;
            }

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
            // Same logic as above but the entry type must come from stat().
            // Fallback for filesystems that do not populate d_type (e.g. some network mounts)
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
