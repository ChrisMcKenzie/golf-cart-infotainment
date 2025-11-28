#!/bin/bash
# Development launcher script that sets LD_LIBRARY_PATH for AASDK libraries

# Get the script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIB_DIR="${SCRIPT_DIR}/src-tauri/aasdk-wrapper/build/lib"

# Set LD_LIBRARY_PATH to include AASDK libraries
export LD_LIBRARY_PATH="${LIB_DIR}:${LD_LIBRARY_PATH}"

# Run the application
exec "${SCRIPT_DIR}/src-tauri/target/debug/golf-cart-infotainment" "$@"

