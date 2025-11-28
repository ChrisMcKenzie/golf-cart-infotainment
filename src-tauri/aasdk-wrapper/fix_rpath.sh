#!/bin/bash
# Script to fix rpath in libaasdk.so to find libaasdk_proto.so
# Run this after building AASDK

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIB_DIR="${SCRIPT_DIR}/build/lib"
LIB_FILE="${LIB_DIR}/libaasdk.so"

if [ ! -f "$LIB_FILE" ]; then
    echo "Error: $LIB_FILE not found"
    exit 1
fi

# Check if patchelf is available
if command -v patchelf &> /dev/null; then
    echo "Fixing rpath in libaasdk.so..."
    patchelf --set-rpath "$LIB_DIR" "$LIB_FILE"
    echo "Done! libaasdk.so should now find libaasdk_proto.so"
else
    echo "Warning: patchelf not found. Install it with: sudo apt install patchelf"
    echo "For now, you can use the run-dev.sh script to set LD_LIBRARY_PATH"
fi

