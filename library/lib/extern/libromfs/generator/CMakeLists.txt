cmake_minimum_required(VERSION 3.16)
project(libromfs-generator)
set(CMAKE_CXX_STANDARD 20)

# Add sources
add_executable(libromfs-generator source/main.cpp)

target_compile_options(libromfs-generator PRIVATE -O3 -Wall -static-libgcc -static-libstdc++)

# Windows 平台链接 CRT 库
if(WIN32)
    target_link_libraries(libromfs-generator PRIVATE 
        msvcrt
        kernel32
        user32
    )
    # 移除静态 CRT 导致的冲突
    string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    string(REPLACE "/MD" "/MT" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
endif()

target_include_directories(${PROJECT_NAME} PRIVATE include)

if (USE_BOOST_FILESYSTEM)
    find_package(Boost 1.44 REQUIRED COMPONENTS filesystem)
    if(Boost_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE Boost::filesystem)
        target_compile_options(${PROJECT_NAME} PRIVATE -DUSE_BOOST_FILESYSTEM)
    endif()
endif ()