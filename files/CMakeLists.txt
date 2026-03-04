cmake_minimum_required(VERSION 3.16)
project(NetworkRelationshipAnalyzer
        VERSION 2.0.0
        DESCRIPTION "Graph Intelligence & Anomaly Detection Engine"
        LANGUAGES CXX)

# ── C++ standard ──────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Build type ────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ── Compiler flags ────────────────────────────────────────────
set(COMMON_WARNINGS
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
    -Wno-sign-compare)

set(RELEASE_FLAGS  -O3 -DNDEBUG -march=native)
set(DEBUG_FLAGS    -O0 -g3 -fsanitize=address,undefined)

# ── Source files ──────────────────────────────────────────────
set(LIB_SOURCES
    src/logger.cpp
    src/config.cpp
    src/csv_parser.cpp
    src/graph.cpp
    src/algorithms.cpp
    src/reporter.cpp)

# ── Static library ────────────────────────────────────────────
add_library(nra_lib STATIC ${LIB_SOURCES})
target_include_directories(nra_lib PUBLIC include)
target_compile_options(nra_lib PRIVATE ${COMMON_WARNINGS})
target_compile_options(nra_lib PRIVATE
    $<$<CONFIG:Release>:${RELEASE_FLAGS}>
    $<$<CONFIG:Debug>:${DEBUG_FLAGS}>)

# ── Main executable ───────────────────────────────────────────
add_executable(nra src/main.cpp)
target_link_libraries(nra PRIVATE nra_lib)
target_include_directories(nra PRIVATE include)
target_compile_options(nra PRIVATE ${COMMON_WARNINGS})
target_compile_options(nra PRIVATE
    $<$<CONFIG:Release>:${RELEASE_FLAGS}>
    $<$<CONFIG:Debug>:${DEBUG_FLAGS}>)

# Link threads
find_package(Threads REQUIRED)
target_link_libraries(nra_lib PUBLIC Threads::Threads)

# ── Tests ─────────────────────────────────────────────────────
enable_testing()
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/test_main.cpp")
    add_executable(nra_tests tests/test_main.cpp)
    target_link_libraries(nra_tests PRIVATE nra_lib)
    target_include_directories(nra_tests PRIVATE include)
    add_test(NAME unit_tests COMMAND nra_tests)
endif()

# ── Install ───────────────────────────────────────────────────
install(TARGETS nra DESTINATION bin)

# ── Summary ───────────────────────────────────────────────────
message(STATUS "=== NRA Build Configuration ===")
message(STATUS "  Build type : ${CMAKE_BUILD_TYPE}")
message(STATUS "  CXX        : ${CMAKE_CXX_COMPILER}")
message(STATUS "  Install to : ${CMAKE_INSTALL_PREFIX}/bin")
