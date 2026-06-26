# SpaceMap - C++ High-Speed Disk Analyzer

An ultra-low-latency, memory-optimized native desktop application that scans, analyzes, and manages disk space usage in real-time. Built in native C++20 and Qt6 with multi-threading, hardware-accelerated layouts, and safe file deletion tools.

---

## 💾 Standalone Downloads (Zero Dependencies)

If you do not want to build from source, you can download a pre-packaged standalone version for your operating system directly from the **[GitHub Releases](https://github.com/Ethan-da-Tech-Wizard/space_map/releases)** page:

* **🪟 Windows**: Download `SpaceMap-Windows-x64.zip`. Extract the folder and double-click `SpaceMap.exe` to run.
* **🍏 macOS**: Download `SpaceMap-macOS.dmg`. Open the DMG file and drag `SpaceMap.app` to your `Applications` folder, then launch it. (See the Gatekeeper section below if you get a developer verification warning).
* **🐧 Linux**: Download `SpaceMap-Linux.AppImage`. Make the file executable (right-click -> Properties -> Allow executing, or run `chmod +x SpaceMap-Linux.AppImage` in the terminal), then run.

---

### 🍏 macOS Gatekeeper Security Warning Bypass

Because the macOS binary is compiled on GitHub Actions without an expensive Apple Developer signing certificate, macOS will show a warning saying **"SpaceMap cannot be opened because the developer cannot be verified"**.

You can bypass this easily using one of these two methods:

#### Method 1: The Finder Right-Click Method (Recommended)
1. Open your **Applications** folder in Finder.
2. **Control-click** (or right-click) the `SpaceMap` app icon.
3. Select **Open** from the context menu.
4. Click **Open** in the warning dialog. The app will now launch, and macOS will remember this preference so you can double-click to open it normally in the future.

#### Method 2: The Terminal Command Method
If the first method doesn't work, open **Terminal** and run the following command to remove the quarantine flag:
```bash
xattr -cr /Applications/SpaceMap.app
```

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
