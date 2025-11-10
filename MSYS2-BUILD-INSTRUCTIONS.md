# MSYS2 + CMake Build Instructions

## Quick Start

### 1. Open MSYS2 MinGW64 Terminal
- Press Windows key, search "MSYS2 MinGW64"
- Open the terminal (black window with green text)

### 2. Install Dependencies

```bash
# Update MSYS2 (if prompted to close terminal, do so and restart)
pacman -Syu

# Install Clang compiler
pacman -S mingw-w64-x86_64-clang

# Install CMake build system
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-make

# Install dependencies
pacman -S mingw-w64-x86_64-boost mingw-w64-x86_64-openssl mingw-w64-x86_64-libevent

# Verify installation
pacman -Q | grep -E "clang|cmake|boost|openssl|libevent"
```

### 3. Navigate to Project

```bash
cd /c/pdns-recursor-5.3.0.2.relrec53x.g594400838/pdns_recursor_windows
```

### 4. Configure with CMake

```bash
# Create build directory
mkdir -p build && cd build

# Configure with Clang
cmake .. \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_BUILD_TYPE=Debug \
  -G "MinGW Makefiles"

# Should output:
# -- Using Clang compiler (GCC-compatible)
# -- Found Boost...
# -- Found OpenSSL...
# -- Found libevent...
```

### 5. Build

```bash
# Build (use all CPU cores)
cmake --build . -j$(nproc)

# Or with make directly
make -j$(nproc)

# Should compile successfully!
```

### 6. Test

```bash
# Run the executable
./bin/pdns_recursor.exe --version
# or
./pdns_recursor.exe --version
```

---

## Why MSYS2 + Clang?

### ✅ Advantages
- **No vcpkg** - Use package manager (pacman)
- **GCC-compatible** - Bitfields work as expected
- **Clang benefits** - Better error messages
- **Familiar** - Unix-like environment
- **Proven** - Same as Unbound DNS approach

### ⚠️ Differences from MSVC
- **Terminal** - Use MSYS2 terminal (not PowerShell)
- **Paths** - Use `/c/` instead of `C:\`
- **Compilation** - Uses GCC/Clang (not MSVC)
- **DLLs** - MinGW runtime (msvcr140.dll not needed)

---

## Troubleshooting

### Problem: "cmake not found"
```bash
pacman -S mingw-w64-x86_64-cmake
```

### Problem: "clang not found"
```bash
pacman -S mingw-w64-x86_64-clang
```

### Problem: "boost not found"
```bash
pacman -S mingw-w64-x86_64-boost
```

### Problem: Compilation errors with bitfields
Fixed automatically with Clang! (GCC-compatible)

### Problem: "Permission denied"
MSYS2 terminal doesn't need Administrator for building

---

## Development Workflow

### Edit Code
Use Cursor/VS Code with MSYS2 terminal open

### Build
```bash
# In MSYS2 terminal
cd /c/pdns-recursor-5.3.0.2.relrec53x.g594400838/pdns_recursor_windows/build
make -j$(nproc)
```

### Run
```bash
./bin/pdns_recursor.exe --local-address=127.0.0.1:5353
```

### Debug
```bash
# Install debugger
pacman -S mingw-w64-x86_64-gdb

# Run with debugger
gdb ./bin/pdns_recursor.exe
(gdb) run --version
(gdb) break dnsname.cc:100
```

---

## Comparison: MSVC vs MSYS2

| Feature | MSVC + vcpkg | MSYS2 + Clang |
|---------|--------------|---------------|
| **Terminal** | PowerShell | MSYS2 terminal |
| **Package Manager** | vcpkg | pacman |
| **Compilation Time** | Fast | Fast |
| **Bitfield Handling** | ⚠️ Different | ✅ GCC-compatible |
| **Error Messages** | Good | ✅ Excellent (Clang) |
| **ABI Issues** | None | None |
| **Unix Code Porting** | More changes needed | ✅ Fewer changes |

**Recommendation**: If you're comfortable with Unix/Linux, MSYS2 + Clang is easier!


