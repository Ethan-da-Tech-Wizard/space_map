#include "scanner.hpp"
#ifdef __APPLE__
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

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

        // Skip symlinks
        if (entry->d_type == DT_LNK) continue;

        if (entry->d_type == DT_DIR) {
            std::string full_path = current_path == "/" ? "/" + std::string(d_name, name_len)
                                                        : current_path + "/" + std::string(d_name, name_len);

            // Skip macOS pseudo-filesystem mount points
            if (full_path == "/System/Volumes" || full_path == "/private/var/vm") continue;

            // Use fstatat with relative name (fast: no kernel path traversal)
            struct stat st;
            if (fstatat(dfd, d_name, &st, AT_SYMLINK_NOFOLLOW) == 0) {
                DirId id{st.st_dev, st.st_ino};
                {
                    std::lock_guard<std::mutex> lock(m_visited_mutex);
                    if (m_visited_dirs.count(id)) {
                        continue;
                    }
                    m_visited_dirs.insert(id);
                }
            }

            TreeNode* child = parent_node->get_or_create_child(std::string_view(d_name, name_len), true);
            add_task(full_path, child);
            dir_subdirs_count++;
        } else if (entry->d_type == DT_REG) {
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
            // Fallback for filesystems that do not populate d_type (e.g. some network mounts)
            struct stat st;
            std::string full_path = current_path == "/" ? "/" + std::string(d_name, name_len)
                                                        : current_path + "/" + std::string(d_name, name_len);
            if (fstatat(dfd, d_name, &st, AT_SYMLINK_NOFOLLOW) == 0) {
                if (S_ISDIR(st.st_mode)) {
                    DirId id{st.st_dev, st.st_ino};
                    {
                        std::lock_guard<std::mutex> lock(m_visited_mutex);
                        if (m_visited_dirs.count(id)) {
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
