# SpaceMap Architecture Book: Chapters 1 & 2

This document provides a complete line-by-line, character-by-character architectural explanation of the build system (`CMakeLists.txt`) and the main application entry point (`src/main.cpp`).

---

# CHAPTER 1: The Build System & Security Configurations (CMakeLists.txt)

This chapter explains the complete CMake configuration. CMake is a cross-platform build configuration system. It does not compile C++ code itself; instead, it parses this configuration and outputs native build files (like `Makefiles` on Linux, Xcode projects on macOS, or `.sln` solution files on Windows).

```cmake
1: cmake_minimum_required(VERSION 3.16)
```
* **Explanation**: 
  * `cmake_minimum_required`: A built-in CMake function that enforces compatibility.
  * `(VERSION 3.16)`: Specifies that the user must have CMake version 3.16 or newer installed. Version 3.16 is chosen because it introduced built-in support for Qt6 auto-configuration tools (`AUTOUIC`, `AUTOMOC`). If an older version is used, CMake will halt with a clear error message.

```cmake
3: if(APPLE)
4:     # Target macOS 11.0 (Big Sur) and later to ensure compatibility with Ventura 13 and older versions
5:     set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment version" FORCE)
6:     # Build a universal binary supporting both Intel (x86_64) and Apple Silicon (arm64)
7:     set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures for Mac OS X" FORCE)
8: endif()
```
* **Explanation**:
  * `if(APPLE)`: Checks if the target platform is macOS/iOS.
  * `set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" ...)`: Enforces backward compatibility. It instructs the compiler and SDK to target macOS 11.0 (Big Sur) or later, ensuring that compiling the app on newer runner OS images (e.g. Sonoma 14) results in a binary that runs flawlessly on older macOS versions such as Ventura 13.
  * `set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" ...)`: Enables **Universal Binary** packaging. It instructs the Clang compiler to build slice targets for both standard Apple Silicon (`arm64`) and legacy Intel (`x86_64`) processor architectures.

```cmake
10: project(SpaceMap LANGUAGES CXX)
```
* **Explanation**: 
  * `project(...)`: Declares the name of the project.
  * `SpaceMap`: Sets the project name (stored in the variable `${PROJECT_NAME}`).
  * `LANGUAGES CXX`: Restricts the compiler search. `CXX` is the standard acronym for C++ in build tools. This instructs CMake to search the host system specifically for a working C++ compiler (like `g++`, `clang++`, or `MSVC`) and verify that it compiles basic code.

```cmake
12: set(CMAKE_CXX_STANDARD 20)
```
* **Explanation**: 
  * `set`: Assigns a value to a variable.
  * `CMAKE_CXX_STANDARD`: A special built-in variable controlling compiler flags.
  * `20`: Targets **C++20**. This tells the compiler to activate C++20 features (like atomic operations on 64-bit integers, shared mutexes, and standard attributes). The compiler will add `-std=c++20` (GCC/Clang) or `/std:c++20` (MSVC) flags.

```cmake
13: set(CMAKE_CXX_STANDARD_REQUIRED ON)
```
* **Explanation**: 
  * `CMAKE_CXX_STANDARD_REQUIRED`: Controls standard fallback behavior.
  * `ON`: If the compiler does not support C++20, compilation stops immediately instead of falling back to C++17 or C++14, preventing confusing syntax errors.

```cmake
15: if(APPLE)
16:     # Force removal of AGL framework if it was picked up by legacy dependencies in modern macOS SDKs
17:     string(REPLACE "-framework AGL" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
18:     string(REPLACE "-framework AGL" "" CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")
19: endif()
```
* **Explanation**:
  * Strips AGL references from default CMake linker flags. The Apple Graphics Library (AGL) framework is deprecated and completely absent from modern macOS SDKs (Xcode 15+). Stripping it from the global linker variables prevents the linker from failing if CMake generates default targets that include it.

```cmake
21: # Force Release build type with -O3 optimizations by default
22: if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
```
* **Explanation**: 
  * `if(...)`: Conditional execution block.
  * `NOT CMAKE_BUILD_TYPE`: Evaluates to true if the build type (Debug/Release) is unspecified.
  * `AND NOT CMAKE_CONFIGURATION_TYPES`: Multi-configuration generators (like Visual Studio or Xcode) support multiple configurations (Debug, Release, MinSizeRel) simultaneously and ignore `CMAKE_BUILD_TYPE`. If this is also empty (meaning we are generating a single-configuration system like Makefiles), we enter the block.

```cmake
23:     set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose build type" FORCE)
24: endif()
```
* **Explanation**: 
  * `Release`: Configures the compiler to optimize the binary for maximum execution speed, strip debugging assertions, and enable aggressive optimizations (e.g. `-O3`).
  * `CACHE STRING "Choose build type"`: Saves this build type setting inside `CMakeCache.txt` so it persists between builds.
  * `FORCE`: Overwrites any pre-existing build type in the cache to enforce Release mode by default.

```cmake
26: # Enable Link-Time Optimization (LTO / IPO) if supported
27: include(CheckIPOSupported)
```
* **Explanation**: 
  * `include(...)`: Loads and runs a CMake module.
  * `CheckIPOSupported`: A built-in CMake module that verifies if the active compiler supports Interprocedural Optimization (IPO), commonly called Link-Time Optimization (LTO).

```cmake
28: check_ipo_supported(RESULT lto_supported OUTPUT lto_output)
```
* **Explanation**: 
  * `check_ipo_supported(...)`: Performs test compilations using LTO flags.
  * `RESULT lto_supported`: Stores the result (`TRUE` or `FALSE`) in the `lto_supported` variable.
  * `OUTPUT lto_output`: Writes any warning messages or errors to the `lto_output` variable for debugging.

```cmake
29: if(lto_supported)
30:     set_property(DIRECTORY PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
31: endif()
```
* **Explanation**: 
  * `set_property`: Sets a configuration property.
  * `DIRECTORY`: Applies to all compilations in the current directory and subdirectories.
  * `PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE`: Enables Link-Time Optimization. Normally, C++ files are compiled individually into object files, and the linker simply joins them. With LTO enabled, the compiler performs global optimization across all source files during the linking stage. This allows it to perform inline expansions of code across different files (e.g. inlining `TreeNode` methods inside `Scanner` loops), bypassing traditional compilation barriers for maximum speed.

```cmake
33: # Enable security hardening flags for GCC and Clang compilers
34: if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
```
* **Explanation**: 
  * `CMAKE_CXX_COMPILER_ID`: Built-in variable containing the compiler's signature name.
  * `STREQUAL "GNU"`: Checks if the compiler is GCC.
  * `MATCHES "Clang"`: Checks if the compiler is Clang (on Linux, macOS, or MinGW).
  * `OR`: Logical OR. If either compiler is detected, we enter the GCC/Clang hardening block.

```cmake
35:     add_compile_options(
36:         -D_GLIBCXX_ASSERTIONS       # Bounds checking on std::vector/string/array
37:         -fstack-protector-strong    # Canary protection against stack buffer overflows
38:         -fPIE                       # Position-independent code for ASLR
39:     )
```
* **Explanation**: 
  * `add_compile_options`: Appends compiler command-line flags.
  * `-D_GLIBCXX_ASSERTIONS`: Preprocessor definition. Instructs standard library headers to run active bounds checks on `std::vector`, `std::string`, and `std::array` bracket `[]` accesses. If a boundary violation occurs, the program immediately aborts, preventing memory corruption exploits.
  * `-fstack-protector-strong`: Injects a protective canary value onto the stack frame right before local variables. If an attacker attempts a stack buffer overflow, the canary is destroyed. The function checks the canary before returning; if it is changed, it aborts the process immediately (`SIGABRT`), preventing execution hijacking.
  * `-fPIE`: Compiles the binary objects as Position Independent Code, which is a prerequisite for enabling Address Space Layout Randomization (ASLR).

```cmake
40:     if(NOT APPLE)
41:         add_link_options(
42:             -pie                        # Position-independent executable for ASLR
43:             -Wl,-z,relro                # Relocation Read-Only (RELRO)
44:             -Wl,-z,now                  # Immediate binding (Full RELRO)
45:         )
46:     endif()
```
* **Explanation**: 
  * Hardens the executable on non-Apple platform compilations (since Apple platforms use different dynamic linkers that do not recognize standard GNU linker flags):
  * `-pie`: Linker flag. Directs the OS loader to run the binary as a Position Independent Executable, scrambling program memory addresses in RAM on every launch to make exploits highly unreliable.
  * `-Wl,-z,relro`: Linker parameter (`-Wl` passes it to the linker). Activates Read-Only Relocations (RELRO). The binary's dynamic linker table is write-protected once initialized.
  * `-Wl,-z,now`: Forces immediate symbol resolution at startup (Full RELRO). This completely write-protects the Global Offset Table (GOT), preventing attackers from modifying GOT pointers to hijack program execution flow.

```cmake
47: elseif(MSVC)
```
* **Explanation**: Checks if the active compiler is MSVC (Microsoft Visual C++ compiler on Windows).

```cmake
48:     add_compile_options(
49:         /GS                         # Buffer Security Check
50:         /sdl                        # Enable Additional Security Checks
51:         /guard:cf                   # Enable Control Flow Guard
52:     )
```
* **Explanation**:
  * `/GS`: Injects stack overrun canary checks (equivalent to GCC's `-fstack-protector`).
  * `/sdl`: Activates Security Development Lifecycle checks. Turns compiler warnings regarding security-relevant bugs (like uninitialized variables or unsafe API usage) into errors.
  * `/guard:cf`: Enables Control Flow Guard (CFG). Performs compiler-generated checks on indirect call targets to prevent code execution hijacking.

```cmake
53:     add_link_options(
54:         /DYNAMICBASE                # Use Address Space Layout Randomization (ASLR)
55:         /NXCOMPAT                   # Data Execution Prevention (DEP)
56:         /HIGHENTROPYVA              # 64-bit ASLR
57:     )
58: endif()
```
* **Explanation**:
  * `/DYNAMICBASE`: Tells Windows to randomize the address space layout (ASLR).
  * `/NXCOMPAT`: Marks the stack and heap memory pages as non-executable (Data Execution Prevention / DEP), blocking execution of shellcode injected into data buffers.
  * `/HIGHENTROPYVA`: Forces 64-bit Address Space Layout Randomization, vastly increasing the randomization entropy on 64-bit Windows platforms.

```cmake
61: # Enable Qt6 automatic compilation features
62: set(CMAKE_AUTOMOC ON)
63: set(CMAKE_AUTOUIC ON)
64: set(CMAKE_AUTORCC ON)
```
* **Explanation**: 
  * Enables Qt automation pipelines.
    * `CMAKE_AUTOMOC`: Automatically compiles files requiring the Qt Meta-Object Compiler (MOC) (classes containing `Q_OBJECT` macros).
    * `CMAKE_AUTOUIC`: Automatically compiles Qt Designer `.ui` layout XMLs.
    * `CMAKE_AUTORCC`: Automatically compiles Qt `.qrc` resource files.

```cmake
66: find_package(Qt6 REQUIRED COMPONENTS Widgets)
```
* **Explanation**: 
  * `find_package`: Locates external SDKs/packages.
  * `Qt6`: Looks for Qt version 6.
  * `REQUIRED`: Halts compilation if Qt6 is not found on the host system.
  * `COMPONENTS Widgets`: Specifically requests the desktop widgets module of Qt.

```cmake
68: # Patch ALL Qt6 imported targets to strip the deprecated AGL framework on macOS.
...
115: endif()
```
* **Explanation**: Links properties cleanup code to prevent legacy AGL linker dependency issues on macOS compilations.

```cmake
118: include_directories(src)
```
* **Explanation**: Registers the source directory for absolute path resolution.

```cmake
121: set(SOURCES
...
131: )
```
* **Explanation**: Specifies list of files for compiler parsing.

```cmake
133: if(WIN32)
134:     list(APPEND SOURCES src/platform/scanner_win.cpp)
135: elseif(APPLE)
136:     list(APPEND SOURCES src/platform/scanner_mac.cpp)
137: elseif(UNIX AND NOT APPLE)
138:     list(APPEND SOURCES src/platform/scanner_linux.cpp)
139: endif()
```
* **Explanation**: Selects cross-platform scanning drivers dynamically depending on compiler output targets.

```cmake
141: if(APPLE)
142:     add_executable(SpaceMap MACOSX_BUNDLE ${SOURCES})
143: else()
144:     add_executable(SpaceMap ${SOURCES})
145: endif()
```
* **Explanation**: Commands compilation executable generation, packaging bundles on Mac systems.

```cmake
147: target_link_libraries(SpaceMap PRIVATE Qt6::Widgets)
```
* **Explanation**: Links compile output to external dependency libraries.

```cmake
151: if(APPLE)
...
176: endif()
```
* **Explanation**: Creates and maps stub OpenGL frameworks to satisfy linker flags on macOS.

---

# CHAPTER 2: The Application Entrypoint & Elevation (src/main.cpp)

This chapter walks through the entrypoint of the application. It handles startup arguments, system user ID elevation checks, runs internal unit tests, and launches the graphical interface window.

```cpp
1: #include "main_window.hpp"
2: #include "tree_node.hpp"
3: #include "scanner.hpp"
4: #include <QApplication>
5: #include <iostream>
6: #include <chrono>
7: #include <thread>
8: #include <vector>
9: #include <filesystem>
```
* **Explanation**: Imports headers for SpaceMap components and standardized library types.

```cpp
11: #ifndef _WIN32
12: #include <sys/resource.h>
13: #include <unistd.h>
14: #endif
```
* **Explanation**: Conditional includes for UNIX/Linux system APIs.

```cpp
16: int main(int argc, char* argv[]) {
```
* **Explanation**: Executable execution entry point.

```cpp
17:     // Check if running as administrator (root). By default, run as standard user.
18:     // Only elevate with pkexec if --elevate is explicitly passed.
19:     bool elevate = false;
20:     for (int i = 1; i < argc; ++i) {
21:         std::string arg = argv[i];
22:         if (arg == "--elevate") {
23:             elevate = true;
24:             break;
25:         }
26:     }
```
* **Explanation**: Inspects inputs to see if dynamic root elevation flag (`--elevate`) has been specified.

```cpp
28: #ifndef _WIN32
29:     if (elevate && geteuid() != 0) {
...
68:     }
69: #endif
```
* **Explanation**: Executes PKExec elevation procedures on Unix if root permissions are missing, transferring Wayland/X11 graphic handles along to the target process.

```cpp
71:     // Run TreeNode assertions first
72:     try {
73:         TreeNode::test_tree_node();
74:         Scanner::test_scanner();
75:     } catch (const std::exception& e) {
76:         std::cerr << "Warning: Self-tests failed to initialize: " << e.what() << "\n";
77:     } catch (...) {
78:         std::cerr << "Warning: Unknown exception during self-tests." << "\n";
79:     }
```
* **Explanation**:
  * Runs static assertion test checks for TreeNode operations and Scanners.
  * Wraps execution in `try-catch` blocks to capture and log failures cleanly rather than crashing the executable outright.

```cpp
81:     if (argc > 1 && std::string(argv[1]) == "--test-only") {
82:         return 0;
83:     }
```
* **Explanation**: Checks for test-only flags and exits cleanly immediately if specified.

```cpp
85:     if (argc > 2 && std::string(argv[1]) == "--benchmark") {
...
124:     }
```
* **Explanation**: Processes CLI headless scanning benchmarks, measuring thread duration and peak resident memory sizes (RSS).

```cpp
126:     QApplication app(argc, argv);
127:     MainWindow window;
128:     window.resize(1024, 768);
129:     window.show();
130:     return app.exec();
131: }
```
* **Explanation**: Launches Qt graphics loops, builds main frame configurations, sets the default dimensions, displays it on user monitors, and launches GUI listening threads.
