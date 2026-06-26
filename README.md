# SpaceMap - C++ High-Speed Disk Analyzer

An ultra-low-latency, memory-optimized native desktop application that scans, analyzes, and manages disk space usage in real-time. Built in native C++20 and Qt6 with multi-threading, hardware-accelerated layouts, and safe file deletion tools.

---

## 🚀 How to Build and Run (Cross-Platform)

### 🐧 Linux (Arch / Ubuntu / Fedora)
#### 1. Install Dependencies
* **Ubuntu/Debian**: `sudo apt install build-essential cmake qt6-base-dev`
* **Arch Linux**: `sudo pacman -S base-devel cmake qt6-base`
* **Fedora**: `sudo dnf groupinstall "Development Tools" && sudo dnf install cmake qt6-qtbase-devel`

#### 2. Compile
```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

#### 3. Run
```bash
./build/SpaceMap
```
*(Note: Scans requiring access to restricted system folders will prompt for Polkit elevation using standard `pkexec` automatically.)*

---

### 🍏 macOS (Intel & Apple Silicon)
#### 1. Install Dependencies
Install [Homebrew](https://brew.sh/) if not already installed, then run:
```bash
brew install cmake qt
```

#### 2. Compile
```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.ncpu)
```

#### 3. Run
```bash
./build/SpaceMap
```

---

### 🪟 Windows (NTFS / Win32)
#### 1. Install Dependencies
1. Install [Visual Studio Community](https://visualstudio.microsoft.com/) with **Desktop development with C++** selected.
2. Install [CMake](https://cmake.org/download/).
3. Install Qt6 widgets using the [Qt Online Installer](https://www.qt.io/download-open-source).

#### 2. Compile (developer Command Prompt for VS)
Open **Developer Command Prompt for VS**, navigate to the project directory and run:
```cmd
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

#### 3. Run
```cmd
build\Release\SpaceMap.exe
```

---

## 📂 Project Architecture

* **[CMakeLists.txt](file:///home/ethan/space_map/CMakeLists.txt)** - Compiles compiler optimization options and platform setups.
* **[src/main.cpp](file:///home/ethan/space_map/src/main.cpp)** - App initializer, Polkit elevation checker, and test verification launcher.
* **[src/main_window.hpp](file:///home/ethan/space_map/src/main_window.hpp) / [main_window.cpp](file:///home/ethan/space_map/src/main_window.cpp)** - Layout styling, warning button configurations, custom context menus, and trash tool triggers.
* **[src/file_tree_model.hpp](file:///home/ethan/space_map/src/file_tree_model.hpp) / [file_tree_model.cpp](file:///home/ethan/space_map/src/file_tree_model.cpp)** - Qt model interface mapping tree node listings and dynamic column sizing algorithms.
* **[src/tree_node.hpp](file:///home/ethan/space_map/src/tree_node.hpp) / [tree_node.cpp](file:///home/ethan/space_map/src/tree_node.cpp)** - Thread-safe prefix path compressed node trees storing statistics rollups.
* **[src/scanner.hpp](file:///home/ethan/space_map/src/scanner.hpp) / [scanner.cpp](file:///home/ethan/space_map/src/scanner.cpp)** - Parallel task-based directory walk coordinator and worker loops.
* **[src/platform/](file:///home/ethan/space_map/src/platform/)** - Platform-specific walkers for Linux (`statx` relative calls), macOS (`dirent`), and Windows (`FindFirstFileW` wide-char handles).

---

## 📖 Deep Code Explainer
For line-by-line descriptions explaining the C++ syntax, memory safety, multithreading logic, and Qt6 configurations, navigate to the **[super_indepth_code_explainer/](file:///home/ethan/space_map/super_indepth_code_explainer/)** directory.
