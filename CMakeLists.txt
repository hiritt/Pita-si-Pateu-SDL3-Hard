cmake_minimum_required(VERSION 3.24)
project(SDL3_HardTask LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Reuse an installed SDL3 if one exists (vcpkg also works).
# Otherwise, download a specific official stable release automatically.
find_package(SDL3 QUIET CONFIG)
if(NOT TARGET SDL3::SDL3)
    include(FetchContent)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
        URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.16/SDL3-3.4.16.tar.gz
        URL_HASH SHA256=7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(SDL3)
endif()

add_executable(SDL3_HardTask main.cpp)
target_link_libraries(SDL3_HardTask PRIVATE SDL3::SDL3)

# Place SDL3.dll next to the .exe for Windows Debug/Release runs.
if(WIN32 AND TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET SDL3_HardTask POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL3::SDL3-shared>"
            "$<TARGET_FILE_DIR:SDL3_HardTask>"
        VERBATIM)
endif()
