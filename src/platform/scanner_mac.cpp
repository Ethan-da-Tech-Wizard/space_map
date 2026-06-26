#include "scanner.hpp"
#ifdef __APPLE__
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>

void Scanner::scan_directory(const std::string& current_path, TreeNode* parent_node,
                             uint64_t& dir_files_size, uint64_t& dir_files_allocated,
                             uint64_t& dir_files_count, uint64_t& dir_subdirs_count) {
    DIR* dir = opendir(current_path.c_str());
    if (!dir) return;

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

        std::string full_path = current_path == "/" ? "/" + name : current_path + "/" + name;

        if (entry->d_type == DT_DIR) {
            struct stat st;
            if (stat(full_path.c_str(), &st) == 0) {
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
            struct stat st;
            uint64_t fsize = 0;
            uint64_t allocated_size = 0;
            if (stat(full_path.c_str(), &st) == 0) {
                fsize = st.st_size;
                allocated_size = st.st_blocks * 512;
            }

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
            struct stat st;
            if (stat(full_path.c_str(), &st) == 0) {
                if (S_ISDIR(st.st_mode)) {
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
