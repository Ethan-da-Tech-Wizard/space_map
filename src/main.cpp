#include "main_window.hpp"
#include "tree_node.hpp"
#include "scanner.hpp"
#include <QApplication>
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <filesystem>

#ifndef _WIN32
#include <sys/resource.h>
#include <unistd.h>
#endif

int main(int argc, char* argv[]) {
    // Check if running as administrator (root). By default, run as standard user.
    // Only elevate with pkexec if --elevate is explicitly passed.
    bool elevate = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--elevate") {
            elevate = true;
            break;
        }
    }

#ifndef _WIN32
    if (elevate && geteuid() != 0) {
        std::vector<std::string> args = { "pkexec", "env" };

        // Pass X11/Wayland display environment variables so the root process can connect to the user's GUI session
        char* display = getenv("DISPLAY");
        if (display) args.push_back(std::string("DISPLAY=") + display);

        char* xauth = getenv("XAUTHORITY");
        if (xauth) {
            args.push_back(std::string("XAUTHORITY=") + xauth);
        } else {
            char* home = getenv("HOME");
            if (home) {
                args.push_back(std::string("XAUTHORITY=") + home + "/.Xauthority");
            }
        }

        char* wayland_display = getenv("WAYLAND_DISPLAY");
        if (wayland_display) args.push_back(std::string("WAYLAND_DISPLAY=") + wayland_display);

        char* xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
        if (xdg_runtime_dir) args.push_back(std::string("XDG_RUNTIME_DIR=") + xdg_runtime_dir);

        // Resolve absolute path to the binary to avoid relative path lookup failures
        std::string abs_exe_path = std::filesystem::absolute(argv[0]).string();
        args.push_back(abs_exe_path);

        for (int i = 1; i < argc; ++i) {
            args.push_back(argv[i]);
        }

        std::vector<char*> c_args;
        for (auto& a : args) {
            c_args.push_back(const_cast<char*>(a.c_str()));
        }
        c_args.push_back(nullptr);

        execvp("pkexec", c_args.data());
        std::cerr << "Warning: Failed to elevate privileges using pkexec. Proceeding as standard user." << std::endl;
    }
#endif

    // Run TreeNode assertions first
    try {
        TreeNode::test_tree_node();
        Scanner::test_scanner();
    } catch (const std::exception& e) {
        std::cerr << "Warning: Self-tests failed to initialize: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Warning: Unknown exception during self-tests." << std::endl;
    }

    if (argc > 1 && std::string(argv[1]) == "--test-only") {
        return 0;
    }

    if (argc > 2 && std::string(argv[1]) == "--benchmark") {
        std::string scan_path = argv[2];
        std::cout << "Starting benchmark scan on: " << scan_path << "..." << std::endl;

        auto start_time = std::chrono::high_resolution_clock::now();

        Scanner scanner;
        scanner.start(scan_path);

        while (scanner.running()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        uint64_t files = scanner.files_scanned();
        uint64_t dirs = scanner.dirs_scanned();
        uint64_t bytes = scanner.bytes_scanned();

        double seconds = duration / 1000.0;
        double files_per_sec = seconds > 0.0 ? files / seconds : files;
        double mb_per_sec = seconds > 0.0 ? (bytes / (1024.0 * 1024.0)) / seconds : 0.0;

        std::cout << "\n--- Scan Benchmark Results ---" << std::endl;
        std::cout << "Duration: " << seconds << " seconds" << std::endl;
        std::cout << "Files Scanned: " << files << std::endl;
        std::cout << "Folders Scanned: " << dirs << std::endl;
        std::cout << "Total Size: " << FileTreeModel::formatSize(bytes).toStdString() << std::endl;
        std::cout << "Throughput: " << files_per_sec << " files/sec" << std::endl;
        std::cout << "I/O Speed: " << mb_per_sec << " MB/sec" << std::endl;

#ifndef _WIN32
        struct rusage usage;
        if (getrusage(RUSAGE_SELF, &usage) == 0) {
            std::cout << "Max RSS: " << (usage.ru_maxrss / 1024.0) << " MB" << std::endl;
        }
#endif
        return 0;
    }

    QApplication app(argc, argv);
    MainWindow window;
    window.resize(1024, 768);
    window.show();
    return app.exec();
}
