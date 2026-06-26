#include "scanner.hpp"
#ifdef _WIN32
#include <windows.h>
#include <string>

// Helper to convert UTF-8 string to UTF-16 wstring
static std::wstring utf8_to_utf16(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring utf16(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &utf16[0], size);
    if (!utf16.empty() && utf16.back() == L'\0') utf16.pop_back();
    return utf16;
}

// Helper to convert UTF-16 wstring to UTF-8 string
static std::string utf16_to_utf8(const std::wstring& utf16) {
    if (utf16.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), -1, &utf8[0], size, nullptr, nullptr);
    if (!utf8.empty() && utf8.back() == '\0') utf8.pop_back();
    return utf8;
}

void Scanner::scan_directory(const std::string& current_path, TreeNode* parent_node,
                             uint64_t& dir_files_size, uint64_t& dir_files_allocated,
                             uint64_t& dir_files_count, uint64_t& dir_subdirs_count) {
    std::string search_path = current_path;
    if (search_path.back() != '\\' && search_path.back() != '/') {
        search_path += "\\*";
    } else {
        search_path += "*";
    }

    std::wstring wsearch_path = utf8_to_utf16(search_path);
    WIN32_FIND_DATAW find_data;
    HANDLE find_handle = FindFirstFileW(wsearch_path.c_str(), &find_data);

    if (find_handle == INVALID_HANDLE_VALUE) return;

    do {
        if (m_cancelled) [[unlikely]] break;

        std::wstring wname = find_data.cFileName;
        if (wname == L"." || wname == L"..") continue;

        std::string name = utf16_to_utf8(wname);
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
        std::string full_path = current_path;
        if (full_path.back() != '\\' && full_path.back() != '/') {
            full_path += "\\" + name;
        } else {
            full_path += name;
        }

        // Skip reparse points (symlinks/junctions) to prevent infinite loops
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
            continue;
        }

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            TreeNode* child = parent_node->get_or_create_child(name, true);
            add_task(full_path, child);
            dir_subdirs_count++;
        } else {
            uint64_t fsize = (static_cast<uint64_t>(find_data.nFileSizeHigh) << 32) | find_data.nFileSizeLow;
            uint64_t allocated_size = fsize;

            // Get compressed/allocated size on disk
            std::wstring wfull_path = utf8_to_utf16(full_path);
            DWORD high_size = 0;
            DWORD low_size = GetCompressedFileSizeW(wfull_path.c_str(), &high_size);
            if (low_size != INVALID_FILE_SIZE || GetLastError() == NO_ERROR) {
                allocated_size = (static_cast<uint64_t>(high_size) << 32) | low_size;
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
        }
    } while (FindNextFileW(find_handle, &find_data));

    FindClose(find_handle);
}
#endif
