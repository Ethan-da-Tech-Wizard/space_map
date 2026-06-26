cmake_minimum_required(VERSION 3.16)

if(APPLE)
    # Target macOS 11.0 (Big Sur) and later to ensure compatibility with Ventura 13 and older versions
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment version" FORCE)
    # Build a universal binary supporting both Intel (x86_64) and Apple Silicon (arm64)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures for Mac OS X" FORCE)
endif()

project(SpaceMap LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    # Force removal of AGL framework if it was picked up by legacy dependencies in modern macOS SDKs
    string(REPLACE "-framework AGL" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    string(REPLACE "-framework AGL" "" CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")
endif()

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
    if(NOT APPLE)
        add_link_options(
            -pie                        # Position-independent executable for ASLR
            -Wl,-z,relro                # Relocation Read-Only (RELRO)
            -Wl,-z,now                  # Immediate binding (Full RELRO)
        )
    endif()
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

# Patch ALL Qt6 imported targets to strip the deprecated AGL framework on macOS.
# Modern macOS SDKs (Xcode 15+) removed AGL entirely, so any reference to it
# causes a fatal linker error. Qt 6.8 was built with AGL support on older SDKs
# and bakes the reference into its imported target properties.
if(APPLE)
    # Define an explicit list of Qt6 and system targets to patch, as they may be global imported targets
    # and not listed in the directory-local IMPORTED_TARGETS property.
    set(_qt_targets
        Qt6::Gui
        Qt6::GuiPrivate
        Qt6::Widgets
        Qt6::WidgetsPrivate
        Qt6::Core
        Qt6::CorePrivate
        Qt6::Platform
        Qt6::QMacStylePlugin
        WrapOpenGL::WrapOpenGL
        OpenGL::GL
    )
    
    # Also discover any directory-scope imported targets
    get_property(_dir_targets DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY IMPORTED_TARGETS)
    if(_dir_targets)
        list(APPEND _qt_targets ${_dir_targets})
    endif()
    list(REMOVE_DUPLICATES _qt_targets)

    foreach(_target ${_qt_targets})
        if(TARGET ${_target})
            # Strip from every property that can carry linker flags
            foreach(_prop
                INTERFACE_LINK_LIBRARIES
                IMPORTED_LINK_INTERFACE_LIBRARIES
                IMPORTED_LINK_INTERFACE_LIBRARIES_RELEASE
                IMPORTED_LINK_INTERFACE_LIBRARIES_RELWITHDEBINFO
                IMPORTED_LINK_INTERFACE_LIBRARIES_MINSIZEREL
                IMPORTED_LINK_INTERFACE_LIBRARIES_DEBUG
                IMPORTED_LINK_INTERFACE_LIBRARIES_NOCONFIG
            )
                get_target_property(_libs ${_target} ${_prop})
                if(_libs)
                    string(REPLACE "-framework AGL" "" _clean "${_libs}")
                    set_target_properties(${_target} PROPERTIES ${_prop} "${_clean}")
                endif()
            endforeach()
        endif()
    endforeach()
endif()

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

if(APPLE)
    add_executable(SpaceMap MACOSX_BUNDLE ${SOURCES})
else()
    add_executable(SpaceMap ${SOURCES})
endif()

target_link_libraries(SpaceMap PRIVATE Qt6::Widgets)

# Final safety net: tell the macOS linker to treat AGL as an optional weak framework.
# If AGL exists, it links weakly; if it doesn't exist, the linker silently ignores it.
if(APPLE)
    # Create a dummy AGL framework to satisfy legacy Qt6 dependencies on modern macOS SDKs.
    # This acts as the ultimate fallback safety net in case any library or target transitively links AGL.
    set(DUMMY_FRAMEWORK_DIR "${CMAKE_BINARY_DIR}/dummy_frameworks/AGL.framework")
    file(MAKE_DIRECTORY "${DUMMY_FRAMEWORK_DIR}")
    
    # Compile a dummy universal binary stub for AGL (arm64 + x86_64)
    execute_process(
        COMMAND clang -shared -arch x86_64 -arch arm64 -o "${DUMMY_FRAMEWORK_DIR}/AGL" -x c /dev/null
        RESULT_VARIABLE DUMMY_BUILD_RESULT
        ERROR_QUIET
    )
    if(NOT DUMMY_BUILD_RESULT EQUAL 0)
        # Fallback to single architecture if universal compilation is not supported by host compiler
        execute_process(
            COMMAND clang -shared -o "${DUMMY_FRAMEWORK_DIR}/AGL" -x c /dev/null
            ERROR_QUIET
        )
    endif()
    
    # Instruct the linker to search the dummy frameworks directory and treat AGL as weak
    target_link_options(SpaceMap PRIVATE 
        "LINKER:-F,${CMAKE_BINARY_DIR}/dummy_frameworks"
        "LINKER:-weak_framework,AGL"
    )
endif()
