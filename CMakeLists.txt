cmake_minimum_required(VERSION 3.22)

project(BeikUI LANGUAGES C CXX)

if(APPLE)
    enable_language(OBJC)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BKUI_PLATFORM_SWITCH "Build BeikUI for Nintendo Switch/libnx" OFF)
option(BKUI_PLATFORM_MACOS  "Build BeikUI for macOS"                  OFF)

if(APPLE AND NOT BKUI_PLATFORM_SWITCH)
    set(BKUI_PLATFORM_MACOS ON CACHE BOOL "" FORCE)
endif()

set(BKUI_RUNTIME_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin")
set(BKUI_LIBRARY_OUTPUT_DIR "${CMAKE_BINARY_DIR}/lib")

file(MAKE_DIRECTORY "${BKUI_RUNTIME_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${BKUI_LIBRARY_OUTPUT_DIR}")

add_subdirectory(bkui)
add_subdirectory(demo)
