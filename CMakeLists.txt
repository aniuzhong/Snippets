cmake_minimum_required(VERSION 3.14...3.22)

project(Snippets LANGUAGES C CXX)

enable_testing()

include(cmake/tools.cmake)
include(cmake/CPM.cmake)

CPMAddPackage("gh:doctest/doctest@2.4.11")
CPMAddPackage("gh:TheLartians/Format.cmake@1.8.1")

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)

add_executable(counters counters.cpp)
set_target_properties(counters PROPERTIES CXX_STANDARD 20)

add_executable(locking_queue locking_queue.cpp)
set_target_properties(locking_queue PROPERTIES CXX_STANDARD 20)