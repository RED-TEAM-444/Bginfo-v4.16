#!/usr/bin/env bash
# ============================================================
#  build.sh  –  Build NRA with CMake or direct g++
# ============================================================
set -e
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
BIN="$PROJECT_DIR/nra"

echo "=== Network Relationship Analyzer – Build Script ==="

USE_CMAKE=0
command -v cmake >/dev/null 2>&1 && USE_CMAKE=1

if [ "$USE_CMAKE" -eq 1 ]; then
    echo "Using CMake..."
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON 2>&1
    cmake --build . --config Release -j"$(nproc 2>/dev/null || echo 4)"
    cp "$BUILD_DIR/nra" "$PROJECT_DIR/nra"
    cp "$BUILD_DIR/nra_tests" "$PROJECT_DIR/nra_tests" 2>/dev/null || true
else
    echo "CMake not found – falling back to direct g++ build..."
    SRCS="src/logger.cpp src/config.cpp src/csv_parser.cpp \
          src/graph.cpp src/algorithms.cpp src/reporter.cpp src/main.cpp"
    g++ -std=c++17 -O2 -Iinclude $SRCS -o nra -lpthread
    echo "  -> nra binary built."

    echo "  Building tests..."
    TSRCS="src/logger.cpp src/config.cpp src/csv_parser.cpp \
           src/graph.cpp src/algorithms.cpp src/reporter.cpp \
           tests/test_main.cpp"
    g++ -std=c++17 -O2 -Iinclude $TSRCS -o nra_tests -lpthread
    echo "  -> nra_tests binary built."
fi

echo ""
echo "Run tests:    ./nra_tests"
echo "Interactive:  ./nra"
echo "Batch mode:   ./nra --input data/network_sample.csv --batch --weighted"
echo "With config:  ./nra --config config.ini"
