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
#define STATX_SIZE 0x00000200U
#define STATX_BLOCKS 0x00000400U
#endif

void Scanner::scan_directory(const std::string& current_path, TreeNode* parent_node,
                             uint64_t& dir_files_size, uint64_t& dir_files_allocated,
                             uint64_t& dir_files_count, uint64_t& dir_subdirs_count) {
    DIR* dir = opendir(current_path.c_str());
    if (!dir) return;

    int dfd = dirfd(dir); // Get directory file descriptor for fast relative path calls
    if (dfd < 0) {
        closedir(dir);
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (m_cancelled) [[unlikely]] break;

        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;

        // Security check: Ignore abnormally long filenames (max 255 bytes)
        if (name.length() > 255) continue;

        // Security check: Ignore names containing dangerous control/escape characters (ASCII < 32)
        bool has_invalid_char = false;
        for (char c : name) {
            if (static_cast<unsigned char>(c) < 32) {
                has_invalid_char = true;
                break;
            }
        }
        if (has_invalid_char) continue;

        // Skip symlinks
        if (entry->d_type == DT_LNK) continue;

        if (entry->d_type == DT_DIR) {
            std::string full_path = current_path == "/" ? "/" + name : current_path + "/" + name;
            // Ignore virtual/pseudo-filesystems
            if (full_path == "/proc" || full_path == "/sys" || full_path == "/dev" || full_path == "/run") {
                continue;
            }

            // Check for mount/bind cycles using inode dev IDs via fast fstatat
            struct stat st;
            if (fstatat(dfd, entry->d_name, &st, AT_SYMLINK_NOFOLLOW) == 0) {
                DirId id{st.st_dev, st.st_ino};
                {
                    std::lock_guard<std::mutex> lock(m_visited_mutex);
                    if (m_visited_dirs.count(id)) {
                        continue;
                    }
                    m_visited_dirs.insert(id);
                }
            }

            TreeNode* child = parent_node->get_or_create_child(name, true);
            add_task(full_path, child);
            dir_subdirs_count++;
        } else if (entry->d_type == DT_REG) {
            uint64_t fsize = 0;
            uint64_t allocated_size = 0;

#if defined(SYS_statx)
            struct statx stx;
            // Pass dfd and relative entry name to statx (avoids full path allocation)
            if (syscall(SYS_statx, dfd, entry->d_name, AT_SYMLINK_NOFOLLOW, STATX_SIZE | STATX_BLOCKS, &stx) == 0) {
                fsize = stx.stx_size;
                allocated_size = stx.stx_blocks * 512;
            } else {
                struct stat st;
                if (fstatat(dfd, entry->d_name, &st, AT_SYMLINK_NOFOLLOW) == 0) {
                    fsize = st.st_size;
                    allocated_size = st.st_blocks * 512;
                }
            }
#else
            struct stat st;
            if (fstatat(dfd, entry->d_name, &st, AT_SYMLINK_NOFOLLOW) == 0) {
                fsize = st.st_size;
                allocated_size = st.st_blocks * 512;
            }
#endif

            TreeNode* child = parent_node->get_or_create_child(name, false);
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
            // Fallback for filesystems that do not return d_type
            struct stat st;
            if (fstatat(dfd, entry->d_name, &st, AT_SYMLINK_NOFOLLOW) == 0) {
                if (S_ISDIR(st.st_mode)) {
                    std::string full_path = current_path == "/" ? "/" + name : current_path + "/" + name;
                    if (full_path == "/proc" || full_path == "/sys" || full_path == "/dev" || full_path == "/run") {
                        continue;
                    }
                    DirId id{st.st_dev, st.st_ino};
                    {
                        std::lock_guard<std::mutex> lock(m_visited_mutex);
                        if (m_visited_dirs.count(id)) {
                            continue;
                        }
                        m_visited_dirs.insert(id);
                    }
                    TreeNode* child = parent_node->get_or_create_child(name, true);
                    add_task(full_path, child);
                    dir_subdirs_count++;
                } else if (S_ISREG(st.st_mode)) {
                    TreeNode* child = parent_node->get_or_create_child(name, false);
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
