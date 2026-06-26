cmake_minimum_required(VERSION 3.16)
project(SpaceMap LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force Release build type with -O3 optimizations by default
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose build type" FORCE)
endif()

# Enable Link-Time Optimization (LTO / IPO) if supported
include(CheckIPOSupported)
check_ipo_supported(RESULT lto_supported OUTPUT lto_output)
if(lto_supported)
    set_property(DIRECTORY PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# Enable security hardening flags for GCC and Clang compilers
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(
        -D_GLIBCXX_ASSERTIONS       # Bounds checking on std::vector/string/array
        -fstack-protector-strong    # Canary protection against stack buffer overflows
        -fPIE                       # Position-independent code for ASLR
    )
    add_link_options(
        -pie                        # Position-independent executable for ASLR
        -Wl,-z,relro                # Relocation Read-Only (RELRO)
        -Wl,-z,now                  # Immediate binding (Full RELRO)
    )
elseif(MSVC)
    add_compile_options(
        /GS                         # Buffer Security Check
        /sdl                        # Enable Additional Security Checks
        /guard:cf                   # Enable Control Flow Guard
    )
    add_link_options(
        /DYNAMICBASE                # Use Address Space Layout Randomization (ASLR)
        /NXCOMPAT                   # Data Execution Prevention (DEP)
        /HIGHENTROPYVA              # 64-bit ASLR
    )
endif()


# Enable Qt6 automatic compilation features
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

# Include source directories
include_directories(src)

# Define source files explicitly to avoid build system synchronization issues
set(SOURCES
    src/main.cpp
    src/main_window.cpp
    src/main_window.hpp
    src/tree_node.cpp
    src/tree_node.hpp
    src/scanner.cpp
    src/scanner.hpp
    src/file_tree_model.cpp
    src/file_tree_model.hpp
)

if(WIN32)
    list(APPEND SOURCES src/platform/scanner_win.cpp)
elseif(APPLE)
    list(APPEND SOURCES src/platform/scanner_mac.cpp)
elseif(UNIX AND NOT APPLE)
    list(APPEND SOURCES src/platform/scanner_linux.cpp)
endif()

add_executable(SpaceMap ${SOURCES})

target_link_libraries(SpaceMap PRIVATE Qt6::Widgets)
