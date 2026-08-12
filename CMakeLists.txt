cmake_minimum_required(VERSION 3.16)
project(arenac C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
  add_compile_options(/W4)
  if(MSVC_VERSION GREATER_EQUAL 1938)
    add_compile_options(/std:c11)
  else()
    message(FATAL_ERROR "arenac requires MSVC 2022 17.8.4 (1938) or newer: older MSVC has neither <threads.h> nor a pthread fallback")
  endif()
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(Threads REQUIRED)

add_library(arenac
  src/arena.c
  src/slab.c
)
target_include_directories(arenac PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(arenac PUBLIC Threads::Threads)

add_executable(test_arena tests/test_arena.c)
target_link_libraries(test_arena PRIVATE arenac)

add_executable(test_slab tests/test_slab.c)
target_link_libraries(test_slab PRIVATE arenac)

add_executable(bench bench/bench.c)
target_link_libraries(bench PRIVATE arenac)

enable_testing()
add_test(NAME arena COMMAND test_arena)
add_test(NAME slab COMMAND test_slab)