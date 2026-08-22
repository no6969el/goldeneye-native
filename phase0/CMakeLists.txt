# Phase 0 bind test — deliberately standalone. It does not link ge_vr, and it
# builds without OpenXR, without a ROM and without the rest of this repo.
cmake_minimum_required(VERSION 3.16)
project(ge_bind_test C)

set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(ge_bind_test STATIC ge_bind_test.c)
target_include_directories(ge_bind_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
if(NOT MSVC)
    target_compile_options(ge_bind_test PRIVATE -Wall -Wextra -pedantic)
    target_link_libraries(ge_bind_test PUBLIC m)
endif()

add_executable(ge_bind_test_selftest ge_bind_test_selftest.c)
target_link_libraries(ge_bind_test_selftest PRIVATE ge_bind_test)

enable_testing()
add_test(NAME ge_bind_test_selftest COMMAND ge_bind_test_selftest)
