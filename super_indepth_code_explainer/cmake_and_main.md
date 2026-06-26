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
69: # Modern macOS SDKs (Xcode 15+) removed AGL entirely, so any reference to it
70: # causes a fatal linker error. Qt 6.8 was built with AGL support on older SDKs
71: # and bakes the reference into its imported target properties.
72: if(APPLE)
73:     # Define an explicit list of Qt6 and system targets to patch, as they may be global imported targets
74:     # and not listed in the directory-local IMPORTED_TARGETS property.
75:     set(_qt_targets
76:         Qt6::Gui
77:         Qt6::GuiPrivate
78:         Qt6::Widgets
79:         Qt6::WidgetsPrivate
80:         Qt6::Core
81:         Qt6::CorePrivate
82:         Qt6::Platform
83:         Qt6::QMacStylePlugin
84:         WrapOpenGL::WrapOpenGL
85:         OpenGL::GL
86:     )
87:     
88:     # Also discover any directory-scope imported targets
89:     get_property(_dir_targets DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY IMPORTED_TARGETS)
90:     if(_dir_targets)
91:         list(APPEND _qt_targets ${_dir_targets})
92:     endif()
93:     list(REMOVE_DUPLICATES _qt_targets)
94: 
95:     foreach(_target ${_qt_targets})
96:         if(TARGET ${_target})
97:             # Strip from every property that can carry linker flags
98:             foreach(_prop
99:                 INTERFACE_LINK_LIBRARIES
100:                 IMPORTED_LINK_INTERFACE_LIBRARIES
101:                 IMPORTED_LINK_INTERFACE_LIBRARIES_RELEASE
102:                 IMPORTED_LINK_INTERFACE_LIBRARIES_RELWITHDEBINFO
103:                 IMPORTED_LINK_INTERFACE_LIBRARIES_MINSIZEREL
104:                 IMPORTED_LINK_INTERFACE_LIBRARIES_DEBUG
105:                 IMPORTED_LINK_INTERFACE_LIBRARIES_NOCONFIG
106:             )
107:                 get_target_property(_libs ${_target} ${_prop})
108:                 if(_libs)
109:                     string(REPLACE "-framework AGL" "" _clean "${_libs}")
110:                     set_target_properties(${_target} PROPERTIES ${_prop} "${_clean}")
111:                 endif()
112:             endforeach()
113:         endif()
114:     endforeach()
115: endif()
```
* **Explanation**:
  * Since Qt 6.8.0 was precompiled on macOS environments referencing the legacy AGL framework, those references are baked into the imported target configuration properties of the Qt6 package.
  * This script queries all CMake targets associated with Qt6, iterates over their linker dependency properties, and strips out `-framework AGL`. This is critical to prevent the linker from searching for a non-existent system framework on newer macOS systems.

```cmake
117: # Include source directories
118: include_directories(src)
```
* **Explanation**: 
  * Instructs the compiler to add the `src/` directory to its header search paths, allowing includes like `#include "tree_node.hpp"` to resolve relative to `src/` without using relative dots (e.g. `#include "../tree_node.hpp"`).

```cmake
121: set(SOURCES
122:     src/main.cpp
123:     src/main_window.cpp
124:     src/main_window.hpp
125:     src/tree_node.cpp
126:     src/tree_node.hpp
127:     src/scanner.cpp
128:     src/scanner.hpp
129:     src/file_tree_model.cpp
130:     src/file_tree_model.hpp
131: )
```
* **Explanation**: Declares a variable list `SOURCES` containing the files needed to build the core application.

```cmake
133: if(WIN32)
134:     list(APPEND SOURCES src/platform/scanner_win.cpp)
135: elseif(APPLE)
136:     list(APPEND SOURCES src/platform/scanner_mac.cpp)
137: elseif(UNIX AND NOT APPLE)
138:     list(APPEND SOURCES src/platform/scanner_linux.cpp)
139: endif()
```
* **Explanation**: 
  * Platform-specific crawler appending:
    * `WIN32`: Matches Windows targets. Appends Windows-native directory traversal source code.
    * `APPLE`: Matches macOS targets. Appends macOS traversal code.
    * `UNIX AND NOT APPLE`: Matches Linux/BSD targets. Appends optimized POSIX/Linux directory traversal.

```cmake
141: if(APPLE)
142:     add_executable(SpaceMap MACOSX_BUNDLE ${SOURCES})
143: else()
144:     add_executable(SpaceMap ${SOURCES})
145: endif()
```
* **Explanation**: 
  * `add_executable`: Tells CMake to compile and link an executable binary.
  * `MACOSX_BUNDLE`: Instructs CMake to compile SpaceMap as a standard macOS Application Bundle (`SpaceMap.app`), which is necessary for launching GUI applications natively and packaging them inside DMG archives.

```cmake
147: target_link_libraries(SpaceMap PRIVATE Qt6::Widgets)
```
* **Explanation**: 
  * Links libraries to target targets.
  * `SpaceMap`: Target executable.
  * `PRIVATE`: Specifies private linking. Components that link against SpaceMap won't inherit Qt widgets.
  * `Qt6::Widgets`: The library objects to link.

```cmake
149: # Final safety net: tell the macOS linker to treat AGL as an optional weak framework.
150: # If AGL exists, it links weakly; if it doesn't exist, the linker silently ignores it.
151: if(APPLE)
152:     # Create a dummy AGL framework to satisfy legacy Qt6 dependencies on modern macOS SDKs.
153:     # This acts as the ultimate fallback safety net in case any library or target transitively links AGL.
154:     set(DUMMY_FRAMEWORK_DIR "${CMAKE_BINARY_DIR}/dummy_frameworks/AGL.framework")
155:     file(MAKE_DIRECTORY "${DUMMY_FRAMEWORK_DIR}")
156:     
157:     # Compile a dummy universal binary stub for AGL (arm64 + x86_64)
158:     execute_process(
159:         COMMAND clang -shared -arch x86_64 -arch arm64 -o "${DUMMY_FRAMEWORK_DIR}/AGL" -x c /dev/null
160:         RESULT_VARIABLE DUMMY_BUILD_RESULT
161:         ERROR_QUIET
162:     )
163:     if(NOT DUMMY_BUILD_RESULT EQUAL 0)
164:         # Fallback to single architecture if universal compilation is not supported by host compiler
165:         execute_process(
166:             COMMAND clang -shared -o "${DUMMY_FRAMEWORK_DIR}/AGL" -x c /dev/null
167:             ERROR_QUIET
168:         )
169:     endif()
170:     
171:     # Instruct the linker to search the dummy frameworks directory and treat AGL as weak
172:     target_link_options(SpaceMap PRIVATE 
173:         "LINKER:-F,${CMAKE_BINARY_DIR}/dummy_frameworks"
174:         "LINKER:-weak_framework,AGL"
175:     )
176: endif()
```
* **Explanation**:
  * **Double Safety Net**: In case some internal Qt dependency (such as dynamic plugins loaded during build processes) still references the AGL framework during compile time:
  * `set(DUMMY_FRAMEWORK_DIR ...)`: Creates a temporary directory structure mimicking a native macOS framework bundle (`AGL.framework`).
  * `execute_process(COMMAND clang -shared -arch x86_64 -arch arm64 ...)`: Invokes the Clang compiler to build a dummy shared stub library targeting both Intel and Apple Silicon processor architectures. By compiling from `/dev/null`, the library is completely empty but carries the correct file header signatures.
  * `target_link_options(...)`: Passes linker instructions:
    * `-F [dir]`: Directs the linker to search our dummy framework directory during compilation.
    * `-weak_framework AGL`: Injects a weak reference. This tells the linker that AGL is optional. The application will compile successfully, and at runtime, macOS will skip loading the missing framework, preventing crashes.

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
* **Explanation**:
  * `#include`: Directs the compiler preprocessor to replace this line with the contents of the target header file.
  * `"main_window.hpp"`, `"tree_node.hpp"`, `"scanner.hpp"`: Includes our application's class definitions.
  * `<QApplication>`: Core Qt runtime initializer class.
  * `<iostream>`: Exposes standard input/output streams (`std::cout`, `std::cerr`).
  * `<chrono>`: Exposes time tracking utilities (`std::chrono::high_resolution_clock`).
  * `<thread>`: Exposes thread utilities for benchmark sleep operations.
  * `<vector>`: Exposes dynamic array storage.
  * `<filesystem>`: Exposes cross-platform path manipulation helper classes.

```cpp
11: #ifndef _WIN32
12: #include <sys/resource.h>
13: #include <unistd.h>
14: #endif
```
* **Explanation**:
  * Guarded Includes: Includes POSIX headers only on non-Windows target compilations.
    * `<sys/resource.h>`: Exposes `getrusage` to read peak memory metrics (RSS) under Linux/macOS.
    * `<unistd.h>`: Exposes POSIX API calls `geteuid()` (get effective user ID) and `execvp()` (replace process image).

```cpp
16: int main(int argc, char* argv[]) {
```
* **Explanation**:
  * `int`: Returns integer exit codes back to the parent operating system shell upon termination.
  * `main`: Standardized C++ execution entry point.
  * `int argc`: Argument Count. The number of command line parameters passed.
  * `char* argv[]`: Argument Vector. An array of null-terminated character strings representing the arguments. `argv[0]` contains the path to the running executable itself.

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
* **Explanation**:
  * Optional Elevation Hook: SpaceMap runs in standard unprivileged user mode by default.
  * Loops starting at index `1` (skipping the executable path at `0`).
  * `std::string arg = argv[i]`: Converts the raw C-style string pointer to a C++ string object for safe comparison.
  * If the user explicitly passes `--elevate`, sets the `elevate` flag to true and exits the loop.

```cpp
28: #ifndef _WIN32
29:     if (elevate && geteuid() != 0) {
```
* **Explanation**:
  * Guarded Unix Privilege Check:
  * `#ifndef _WIN32`: This block is completely stripped out when compiling on Windows (Windows has no concept of UID 0 or `pkexec`).
  * `geteuid() != 0`: Calls POSIX privilege query. Under Linux/macOS, UID `0` represents the root administrator. If we are not root but root was requested (`elevate`), we enter the elevation block.

```cpp
30:         std::vector<std::string> args = { "pkexec", "env" };
```
* **Explanation**:
  * Initializes a list of string parameters to pass to `execvp`.
    * `pkexec`: Polkit elevation helper. It triggers a system graphical dialog box requesting the user's password.
    * `env`: Utility command that allows launching a process while passing custom environmental variables.

```cpp
32:         // Pass X11/Wayland display environment variables so the root process can connect to the user's GUI session
33:         char* display = getenv("DISPLAY");
34:         if (display) args.push_back(std::string("DISPLAY=") + display);
```
* **Explanation**:
  * `getenv("DISPLAY")`: Reads X11 server display environments.
  * If found, appends `DISPLAY=[value]` to the elevation arguments. Without this, the elevated root process won't know which display screen to open the window on, causing a startup crash.

```cpp
36:         char* xauth = getenv("XAUTHORITY");
37:         if (xauth) {
38:             args.push_back(std::string("XAUTHORITY=") + xauth);
39:         } else {
40:             char* home = getenv("HOME");
41:             if (home) {
42:                 args.push_back(std::string("XAUTHORITY=") + home + "/.Xauthority");
43:             }
44:         }
```
* **Explanation**:
  * Resolves cookie authorization key directories for screen authentication under X11 server connections. Defaults to the user's home folder `.Xauthority` if the environment is not set.

```cpp
46:         char* wayland_display = getenv("WAYLAND_DISPLAY");
47:         if (wayland_display) args.push_back(std::string("WAYLAND_DISPLAY=") + wayland_display);
48: 
49:         char* xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
50:         if (xdg_runtime_dir) args.push_back(std::string("XDG_RUNTIME_DIR=") + xdg_runtime_dir);
```
* **Explanation**:
  * Resolves Wayland compositor environments and runtime communication folders.

```cpp
52:         // Resolve absolute path to the binary to avoid relative path lookup failures
53:         std::string abs_exe_path = std::filesystem::absolute(argv[0]).string();
54:         args.push_back(abs_exe_path);
```
* **Explanation**:
  * Resolves the absolute file path of the current binary and adds it to the command list. This ensures `pkexec` knows exactly which binary to launch, preventing lookup failures.

```cpp
56:         for (int i = 1; i < argc; ++i) {
57:             args.push_back(argv[i]);
58:         }
```
* **Explanation**:
  * Appends any extra command-line flags (such as the directory to scan) to the execution list.

```cpp
60:         std::vector<char*> c_args;
61:         for (auto& a : args) {
62:             c_args.push_back(const_cast<char*>(a.c_str()));
63:         }
64:         c_args.push_back(nullptr);
```
* **Explanation**:
  * Translates our C++ string list to a C-style array of pointers (`char* argv[]`) required by the system execution call:
    * `a.c_str()` gets the raw character pointer.
    * `const_cast<char*>` strips the const safety constraint.
    * `c_args.push_back(nullptr)` appends a null pointer. POSIX standard execution lists *must* be null-terminated so the kernel knows where the list ends in memory.

```cpp
66:         execvp("pkexec", c_args.data());
67:         std::cerr << "Warning: Failed to elevate privileges using pkexec. Proceeding as standard user." << "\n";
68:     }
69: #endif
```
* **Explanation**:
  * `execvp(...)`: Replaces the current process in memory with `pkexec` using the constructed parameters. If successful, the code execution swaps to the new elevated process immediately. If it fails, execution resumes on the next line, printing a warning to `std::cerr`, and proceeds as standard user.

```cpp
71:     // Run TreeNode assertions first
72:     TreeNode::test_tree_node();
73:     Scanner::test_scanner();
```
* **Explanation**: Runs the static test suites for tree nodes and scanners to assert correctness before booting the GUI.

```cpp
75:     if (argc > 1 && std::string(argv[1]) == "--test-only") {
76:         return 0;
77:     }
```
* **Explanation**: Exit Hook: If started with the `--test-only` flag, exits the program with code `0` immediately after running tests.

```cpp
79:     if (argc > 2 && std::string(argv[1]) == "--benchmark") {
80:         std::string scan_path = argv[2];
81:         std::cout << "Starting benchmark scan on: " << scan_path << "..." << "\n";
```
* **Explanation**: Headless Benchmark Block: If benchmark parameters are passed, skips the GUI.

```cpp
83:         auto start_time = std::chrono::high_resolution_clock::now();
```
* **Explanation**: Begins execution time tracking.

```cpp
85:         Scanner scanner;
86:         scanner.start(scan_path);
```
* **Explanation**: Instantiates a headless scanner and runs it against the path.

```cpp
88:         while (scanner.running()) {
89:             std::this_thread::sleep_for(std::chrono::milliseconds(5));
90:         }
```
* **Explanation**: Blocks the main thread, sleeping in 5ms intervals until scanner threads finish.

```cpp
92:         auto end_time = std::chrono::high_resolution_clock::now();
93:         auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
```
* **Explanation**: Calculates total elapsed time in milliseconds.

```cpp
95:         uint64_t files = scanner.files_scanned();
96:         uint64_t dirs = scanner.dirs_scanned();
97:         uint64_t bytes = scanner.bytes_scanned();
```
* **Explanation**: Retrieves final traversal metrics.

```cpp
99:         double seconds = duration / 1000.0;
100:         double files_per_sec = seconds > 0.0 ? files / seconds : files;
101:         double mb_per_sec = seconds > 0.0 ? (bytes / (1024.0 * 1024.0)) / seconds : 0.0;
```
* **Explanation**: Computes throughput metrics.

```cpp
103:         std::cout << "\n--- Scan Benchmark Results ---" << "\n";
104:         std::cout << "Duration: " << seconds << " seconds" << "\n";
105:         std::cout << "Files Scanned: " << files << "\n";
106:         std::cout << "Folders Scanned: " << dirs << "\n";
107:         std::cout << "Total Size: " << FileTreeModel::formatSize(bytes).toStdString() << "\n";
108:         std::cout << "Throughput: " << files_per_sec << " files/sec" << "\n";
109:         std::cout << "I/O Speed: " << mb_per_sec << " MB/sec" << "\n";
```
* **Explanation**: Prints performance statistics to the console.

```cpp
111: #ifndef _WIN32
112:         struct rusage usage;
113:         if (getrusage(RUSAGE_SELF, &usage) == 0) {
114:             std::cout << "Max RSS: " << (usage.ru_maxrss / 1024.0) << " MB" << "\n";
115:         }
116: #endif
117:         return 0;
118:     }
```
* **Explanation**:
  * Guarded Memory Inspection: Queries `getrusage` on non-Windows platforms.
  * `usage.ru_maxrss`: Reads the peak Resident Set Size (RSS), representing the peak physical memory allocation consumed by the process.
  * Prints memory statistics and exits the benchmark.

```cpp
120:     QApplication app(argc, argv);
```
* **Explanation**: Instantiates the core Qt application controller on the stack.

```cpp
121:     MainWindow window;
122:     window.resize(1024, 768);
123:     window.show();
```
* **Explanation**: Instantiates the main window, resizes it to 1024x768 pixels, and displays it.

```cpp
124:     return app.exec();
125: }
```
* **Explanation**: Starts the Qt GUI event loop. The thread blocks here, waiting and responding to user window events. Returns the exit code of `app.exec()` back to the parent operating system shell when closed.
