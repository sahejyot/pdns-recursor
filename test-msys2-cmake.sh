#!/bin/bash
# Test MSYS2 + CMake build

set -e

echo "=== Testing MSYS2 + CMake Build ==="

# Navigate to project directory  
cd "/c/pdns-recursor-5.3.0.2.relrec53x.g594400838/pdns_recursor_windows"

# Create build directory
echo ""
echo "Creating MSYS2 build directory..."
rm -rf build_msys2
mkdir -p build_msys2
cd build_msys2

# Check for required tools
echo ""
echo "Checking tools:"
gcc --version | head -1 || echo "GCC not found!"
g++ --version | head -1 || echo "G++ not found!"
cmake --version | head -1 || echo "CMake not found!"

# Configure with CMake using MinGW Makefiles
echo ""
echo "Configuring with CMake (MinGW Makefiles)..."
cmake .. \
  -G "MinGW Makefiles" \
  -DCMAKE_C_COMPILER=gcc \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_BUILD_TYPE=Debug

echo ""
echo "CMake configuration completed!"

# Try to build
echo ""
echo "Building with mingw32-make..."
mingw32-make -j$(nproc) 2>&1 | head -50

echo ""
echo "Build completed!"


