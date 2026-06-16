option(BKUI_SHARED "Build shared library" OFF)
option(BKUI_USE_PHYSFS "Use PhysicsFS for the virtual file system" ON)

set(BKUI_THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/platform")

file(
    GLOB_RECURSE
    BKUI_SRC
    CONFIGURE_DEPENDS
    src/core/*.cpp
    src/resource/*.cpp
    src/rhi/*.cpp
)

if(BKUI_PLATFORM_SWITCH)
    list(APPEND BKUI_SRC src/platform/SwitchPlatform.cpp)
    list(APPEND BKUI_SRC src/backend/deko3d/Deko3DDevice.cpp)
else()
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory("${BKUI_THIRD_PARTY_DIR}/SDL3" "${CMAKE_BINARY_DIR}/third_party/SDL3" EXCLUDE_FROM_ALL)

    list(APPEND BKUI_SRC src/platform/SDLPlatform.cpp)
    list(APPEND BKUI_SRC src/backend/opengl/OpenGLDevice.cpp)
endif()

if(BKUI_USE_PHYSFS AND NOT BKUI_PLATFORM_SWITCH)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    set(PHYSFS_BUILD_STATIC ON CACHE BOOL "" FORCE)
    set(PHYSFS_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(PHYSFS_BUILD_TEST OFF CACHE BOOL "" FORCE)
    set(PHYSFS_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(PHYSFS_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
    add_subdirectory("${BKUI_THIRD_PARTY_DIR}/physfs" "${CMAKE_BINARY_DIR}/third_party/physfs" EXCLUDE_FROM_ALL)
endif()

if(BKUI_SHARED)
    add_library(bkui SHARED ${BKUI_SRC})
else()
    add_library(bkui STATIC ${BKUI_SRC})
endif()

add_library(BeikUI::bkui ALIAS bkui)

target_include_directories(
    bkui
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_features(bkui PUBLIC cxx_std_20)

target_compile_definitions(
    bkui
    PUBLIC
        $<$<BOOL:${BKUI_PLATFORM_SWITCH}>:BKUI_PLATFORM_SWITCH=1>
)

if(BKUI_PLATFORM_SWITCH)
    target_include_directories(bkui PRIVATE "$ENV{DEVKITPRO}/libnx/include")
    find_library(
        BKUI_NX_LIBRARY
        nx
        PATHS "$ENV{DEVKITPRO}/libnx/lib"
        NO_DEFAULT_PATH
    )
    find_library(
        BKUI_DEKO3D_LIBRARY
        deko3d
        PATHS "$ENV{DEVKITPRO}/libnx/lib" "$ENV{DEVKITPRO}/portlibs/switch/lib"
        NO_DEFAULT_PATH
    )
    if(BKUI_NX_LIBRARY)
        target_link_libraries(bkui PUBLIC "${BKUI_NX_LIBRARY}")
    endif()
    if(BKUI_DEKO3D_LIBRARY)
        target_link_libraries(bkui PUBLIC "${BKUI_DEKO3D_LIBRARY}" m)
    endif()
else()
    target_link_libraries(bkui PUBLIC SDL3::SDL3-static)

    if(WIN32)
        target_link_libraries(bkui PRIVATE opengl32)
    elseif(APPLE)
        find_library(BKUI_OPENGL_FRAMEWORK OpenGL REQUIRED)
        target_link_libraries(bkui PRIVATE "${BKUI_OPENGL_FRAMEWORK}")
    else()
        find_package(OpenGL REQUIRED)
        target_link_libraries(bkui PRIVATE OpenGL::GL)
    endif()

    if(BKUI_USE_PHYSFS AND TARGET PhysFS::PhysFS-static)
        target_link_libraries(bkui PRIVATE PhysFS::PhysFS-static)
        target_compile_definitions(bkui PRIVATE BKUI_USE_PHYSFS=1)
    endif()
endif()

set_target_properties(
    bkui
    PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${BKUI_LIBRARY_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${BKUI_LIBRARY_OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY "${BKUI_RUNTIME_OUTPUT_DIR}"
)
