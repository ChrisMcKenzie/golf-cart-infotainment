#!/bin/bash
# Build script for AASDK library
# This script builds AASDK and the C wrapper

set -e

# Get the script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Use relative paths from script directory, or absolute paths if provided
if [ -n "$1" ]; then
    AASDK_DIR="$1"
else
    AASDK_DIR="${SCRIPT_DIR}/aasdk"
fi

if [ -n "$2" ]; then
    BUILD_DIR="$2"
else
    BUILD_DIR="${SCRIPT_DIR}/build"
fi

# Convert to absolute paths if they're relative
if [[ "$AASDK_DIR" != /* ]]; then
    AASDK_DIR="${SCRIPT_DIR}/${AASDK_DIR}"
fi
if [[ "$BUILD_DIR" != /* ]]; then
    BUILD_DIR="${SCRIPT_DIR}/${BUILD_DIR}"
fi

# Resolve to absolute paths
AASDK_DIR="$(cd "$(dirname "$AASDK_DIR")" && pwd)/$(basename "$AASDK_DIR")"
BUILD_DIR="$(cd "$(dirname "$BUILD_DIR")" && pwd)/$(basename "$BUILD_DIR")"

echo "Building AASDK from: $AASDK_DIR"
echo "Build directory: $BUILD_DIR"

# Check if AASDK directory exists
if [ ! -d "$AASDK_DIR" ]; then
    echo "AASDK directory not found: $AASDK_DIR"
    echo "Please clone AASDK first:"
    echo "  git clone https://github.com/f1xpl/aasdk.git $AASDK_DIR"
    exit 1
fi

# Check if CMakeLists.txt exists in AASDK directory
if [ ! -f "$AASDK_DIR/CMakeLists.txt" ]; then
    echo "Error: CMakeLists.txt not found in $AASDK_DIR"
    echo "The AASDK directory might not be properly cloned or is incorrect."
    exit 1
fi

# Clean any CMake cache files from source directory (prevents in-source builds)
echo "Cleaning any CMake cache from source directory..."
find "$AASDK_DIR" -name "CMakeCache.txt" -delete 2>/dev/null || true
find "$AASDK_DIR" -type d -name "CMakeFiles" -exec rm -rf {} + 2>/dev/null || true
find "$AASDK_DIR" -name "Makefile" -not -path "*/aasdk_proto/*" -delete 2>/dev/null || true

# Create build directory (clean it if it exists)
if [ -d "$BUILD_DIR" ]; then
    echo "Cleaning existing build directory..."
    rm -rf "$BUILD_DIR"
fi
mkdir -p "$BUILD_DIR"

echo "Running CMake..."
# Get the absolute path to the lib directory where libraries will be built
LIB_DIR="${BUILD_DIR}/lib"

# Build AASDK - explicitly specify source and build directories
# Set CMAKE_BUILD_RPATH so libraries can find their dependencies at runtime
if ! cmake -S "$AASDK_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DCMAKE_BUILD_RPATH="${LIB_DIR}" \
    -DCMAKE_INSTALL_RPATH="${LIB_DIR}" \
    -DCMAKE_BUILD_RPATH_USE_ORIGIN=ON; then
    echo "ERROR: CMake configuration failed!"
    echo "Please check the error messages above."
    exit 1
fi

cd "$BUILD_DIR"

# Verify Makefile was created
if [ ! -f "Makefile" ]; then
    echo "ERROR: CMake did not generate Makefile!"
    echo "Build directory contents:"
    ls -la
    exit 1
fi

echo "Running make..."
if ! make -j$(nproc) 2>&1 | tee make.log; then
    echo ""
    echo "=========================================="
    echo "ERROR: Build failed!"
    echo "=========================================="
    echo "Last 50 lines of build output:"
    tail -50 make.log
    echo ""
    echo "Full build log saved to: $BUILD_DIR/make.log"
    exit 1
fi

echo "AASDK built successfully!"

