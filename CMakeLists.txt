cmake_minimum_required(VERSION 3.14...3.22)

project(Snippets LANGUAGES C CXX)

enable_testing()

include(cmake/tools.cmake)
include(cmake/CPM.cmake)

CPMAddPackage("gh:doctest/doctest@2.4.11")

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)

# Packages managed by vcpkg
#   In .vscode/settings.json, add "CMAKE_TOOLCHAIN_FILE": "path/to/vcpkg/scripts/buildsystems/vcpkg.cmake"
find_package(FFMPEG REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_mixer CONFIG REQUIRED)

add_executable(counters counters.cpp)
set_target_properties(counters PROPERTIES CXX_STANDARD 20)

add_executable(locking_queue locking_queue.cpp)
set_target_properties(locking_queue PROPERTIES CXX_STANDARD 20)

add_executable(minimal_allocator minimal_allocator.cpp)
set_target_properties(minimal_allocator PROPERTIES CXX_STANDARD 20)

add_executable(scl_timer scl_timer.cpp)
set_target_properties(scl_timer PROPERTIES CXX_STANDARD 20)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  target_compile_options(scl_timer PUBLIC -Wall -Wpedantic -Wextra -Werror)
elseif(MSVC)
  target_compile_options(scl_timer PUBLIC /W4 /WX)
endif()

add_executable(atomic_shared_ptr atomic_shared_ptr.cpp)
set_target_properties(atomic_shared_ptr PROPERTIES CXX_STANDARD 20)
