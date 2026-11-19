// ==============================================================================
// Program: main.cpp
// Programmer: Ethan-da-Tech-Wizard
// Last Modified: November 18, 2026
// Purpose: SpaceMap application entry point and Polkit elevation checker
//
// How it fits: this is the only file with main(). It decides between four
// run modes and delegates the real work:
//   spacemap                     start the Qt GUI (MainWindow)
//   spacemap --elevate           relaunch via pkexec (Linux) to read protected folders
//   spacemap --test-only         run the built-in self-tests (tree_node.cpp,
//                                scanner.cpp) and exit 0
//   spacemap --benchmark PATH    scan PATH headlessly and print throughput
//
// Design decisions: privilege elevation is opt-in (never automatic) so the
// default is least privilege; the GUI and the headless modes share the same
// Scanner class so a benchmark measures exactly what the GUI would do.
//
// Known limits: --elevate only works where pkexec exists (Linux); on other
// systems it is silently ignored.
// ==============================================================================
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

/**
 * Program entry point.
 * @param argc Argument count.
 * @param argv Arguments; see the run modes in the file header.
 * @return 0 on success (including --test-only and --benchmark); otherwise the
 *         Qt event loop's exit code from the GUI.
 *         A failed pkexec relaunch only prints a warning and continues unprivileged.
 */
int main(int argc, char* argv[]) {
    // --elevate is the only recognised flag at this stage because the relaunch
    // must happen before any Qt object is created (the root process has to start
    // from scratch). All other arguments are interpreted further below.
    bool elevate = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--elevate") {
            elevate = true;
            break;
        }
    }

// POSIX only: Windows has no geteuid()/pkexec, so the whole block is compiled out there.
#ifndef _WIN32
    // Relaunch only if not already root; otherwise the elevated child would
    // relaunch itself forever.
    if (elevate && geteuid() != 0) {
        // pkexec drops most of the environment, so the display-related variables are
        // forwarded explicitly (via `env`) for the root process to reach the user's X11
        // or Wayland session. execvp only returns on failure, in which case the program
        // continues as a normal user.
        // pkexec drops most of the environment, so the display-related variables are
        // forwarded explicitly (via `env`) for the root process to reach the user's X11
        // or Wayland session. execvp only returns on failure, in which case the program
        // continues as a normal user.
        // argv for execvp: `pkexec env VAR=... /abs/path/spacemap <original args>`.
        std::vector<std::string> args = { "pkexec", "env" };

        // X11 display name, e.g. ":0".
        char* display = getenv("DISPLAY");
        if (display) args.push_back(std::string("DISPLAY=") + display);

        // X authority cookie file; root cannot read the user's session without it.
        // Fall back to ~/.Xauthority, the conventional default location.
        char* xauth = getenv("XAUTHORITY");
        if (xauth) {
            args.push_back(std::string("XAUTHORITY=") + xauth);
        } else {
            char* home = getenv("HOME");
            if (home) {
                args.push_back(std::string("XAUTHORITY=") + home + "/.Xauthority");
            }
        }

        // Wayland socket name; XDG_RUNTIME_DIR (next) is the folder that socket lives in.
        char* wayland_display = getenv("WAYLAND_DISPLAY");
        if (wayland_display) args.push_back(std::string("WAYLAND_DISPLAY=") + wayland_display);

        char* xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
        if (xdg_runtime_dir) args.push_back(std::string("XDG_RUNTIME_DIR=") + xdg_runtime_dir);

        // pkexec starts the program with a different working directory, so a path
        // like ./spacemap would not resolve; pass an absolute path.
        std::string abs_exe_path = std::filesystem::absolute(argv[0]).string();
        args.push_back(abs_exe_path);

        // Forward the user's original arguments, including --elevate (harmless: the
        // child is root, so the relaunch condition is false).
        for (int i = 1; i < argc; ++i) {
            args.push_back(argv[i]);
        }

        // execvp wants a NULL-terminated array of char*; the strings are owned by `args`,
        // which stays alive until exec, hence the const_cast is safe (execvp never writes).
        std::vector<char*> c_args;
        for (auto& a : args) {
            c_args.push_back(const_cast<char*>(a.c_str()));
        }
        c_args.push_back(nullptr);

        // Replaces this process on success. Reaching the next line means it failed.
        execvp("pkexec", c_args.data());
        std::cerr << "Warning: Failed to elevate privileges using pkexec. Proceeding as standard user." << "\n";
    }
#endif

    // (The self-test block lives here deliberately: it runs before the benchmark
    // and GUI branches and needs no Qt event loop.)
    // --test-only runs the built-in self-tests and exits. They are NOT run on
    // normal GUI starts: they use assert() (an abort in non-NDEBUG builds), spawn
    // threads and write a scratch directory under the system temp folder.
    if (argc > 1 && std::string(argv[1]) == "--test-only") {
        // Each exception is only reported: a failing self-test must not turn into a crash dump.
        try {
            TreeNode::test_tree_node();
            Scanner::test_scanner();
        } catch (const std::exception& e) {
            std::cerr << "Warning: Self-tests failed to initialize: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "Warning: Unknown exception during self-tests." << "\n";
        }
        return 0;
    }

    // Headless benchmark. No QApplication is created: Scanner is a QObject but
    // only emits signals nobody listens to, so no event loop is required. The
    // loop below polls running() every 5 ms, which costs almost no CPU compared
    // with the scan itself.
    if (argc > 2 && std::string(argv[1]) == "--benchmark") {
        std::string scan_path = argv[2];
        std::cout << "Starting benchmark scan on: " << scan_path << "..." << "\n";

        // Wall-clock timing starts before start() so thread creation is included.
        auto start_time = std::chrono::high_resolution_clock::now();

        Scanner scanner;
        scanner.start(scan_path);

        while (scanner.running()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        // Counters are atomics read after the scan finished, so they are final.
        uint64_t files = scanner.files_scanned();
        uint64_t dirs = scanner.dirs_scanned();
        uint64_t bytes = scanner.bytes_scanned();

        // Throughput: files per second, and I/O speed as logical MiB per second.
        // Guard against a zero-length duration (tiny scans) to avoid dividing by zero.
        double seconds = duration / 1000.0;
        double files_per_sec = seconds > 0.0 ? files / seconds : files;
        double mb_per_sec = seconds > 0.0 ? (bytes / (1024.0 * 1024.0)) / seconds : 0.0;

        std::cout << "\n--- Scan Benchmark Results ---" << "\n";
        std::cout << "Duration: " << seconds << " seconds" << "\n";
        std::cout << "Files Scanned: " << files << "\n";
        std::cout << "Folders Scanned: " << dirs << "\n";
        std::cout << "Total Size: " << FileTreeModel::formatSize(bytes).toStdString() << "\n";
        std::cout << "Throughput: " << files_per_sec << " files/sec" << "\n";
        std::cout << "I/O Speed: " << mb_per_sec << " MB/sec" << "\n";

#ifndef _WIN32
        // getrusage is POSIX; ru_maxrss is peak resident memory. Linux reports it in
        // KiB (hence /1024 to get MiB) but macOS reports BYTES, so it needs /(1024*1024).
        struct rusage usage;
        if (getrusage(RUSAGE_SELF, &usage) == 0) {
#ifdef __APPLE__
            const double rss_divisor = 1024.0 * 1024.0;
#else
            const double rss_divisor = 1024.0;
#endif
            std::cout << "Max RSS: " << (usage.ru_maxrss / rss_divisor) << " MB" << "\n";
        }
#endif
        return 0;
    }

    // Normal start: standard Qt widget application, default window 1024x768.
    QApplication app(argc, argv);
    MainWindow window;
    window.resize(1024, 768);
    window.show();
    return app.exec();
}
