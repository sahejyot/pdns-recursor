# MSYS2 Quick Start for Cursor

## ✅ MSYS2 Already Installed!

**Status**: ✅ MSYS2 is installed at `C:\msys64` with GCC 15.2.0

### Step 1: Open MSYS2 Terminal
1. Press Windows key
2. Search for "MSYS2 MinGW64" (NOT MSYS2 MSYS!)
3. Open it

### Step 2: Install Dependencies
Run in MSYS2 MinGW64 terminal:

```bash
# Update system (close terminal when prompted, reopen, run again)
pacman -Syu

# Install compiler and tools
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-make

# Install build dependencies
pacman -S mingw-w64-x86_64-boost mingw-w64-x86_64-openssl mingw-w64-x86_64-libevent

# Install git
pacman -S git
```

### Step 3: Configure Cursor
✅ Configuration already created in `.vscode/settings.json`!

Restart Cursor and it will:
- ✅ Use MSYS2 MinGW64 as default terminal
- ✅ Configure IntelliSense for C++ 
- ✅ Set up build tasks

### Step 4: Open Terminal in Cursor
1. Press `Ctrl+`` (backtick)
2. Terminal will use MSYS2 by default

## 🚀 Your First Build

```bash
# In Cursor terminal (MSYS2)
cd /c/pdns-recursor-5.3.0.2.relrec53x.g594400838/pdns_recursor_windows

# Create build directory
mkdir -p build && cd build

# Configure
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug

# Build (this will take a few minutes)
make -j$(nproc)

# Test
./pdns_recursor.exe --version
```

## 📝 Path Conversion

| PowerShell | MSYS2 Bash |
|------------|------------|
| `C:\projects\file` | `/c/projects/file` |
| `cd C:\` | `cd /c/` |
| `ls C:\` | `ls /c/` |

## 🛠️ Common Commands

### Build
```bash
# Full build
make -j$(nproc)

# Clean
make clean

# Rebuild
make clean && make -j$(nproc)
```

### Debug
```bash
# Run debugger
gdb ./pdns_recursor.exe

# Set breakpoint
(gdb) break syncres.cc:100
(gdb) run --version
```

### Install Packages
```bash
# Search package
pacman -Ss boost

# Install
pacman -S mingw-w64-ucrt-x86_64-package-name

# Update
pacman -Syu
```

## 🎯 Terminal Profiles

Cursor will auto-detect MSYS2. You can also switch terminals:

1. Click dropdown arrow next to `+` in terminal
2. Select: "MSYS2 UCRT64" or "PowerShell"

## 🔍 Verify Setup

```bash
# Check compiler
gcc --version

# Check cmake
cmake --version

# Check dependencies
pkg-config --list-all | grep boost
pkg-config --list-all | grep openssl
```

Expected output:
- GCC 13.x
- CMake 3.x
- Boost, OpenSSL found

## 📚 More Info

- Full setup guide: `SETUP-MSYS2-CURSOR.md`
- Build instructions: `BUILD-INSTRUCTIONS.md`
- Implementation plan: `IMPLEMENTATION-PLAN-SPRINTS.md`

## ⚠️ Important Notes

1. **Always use UCRT64 terminal** (not MSYS2 MSYS)
2. **MSYS2 paths are different**: `/c/` = `C:\`
3. **Close all MSYS2 terminals** before running `pacman -Syu`
4. **Use `nproc` for CPU count** in make commands

---

**That's it! You're ready to build PowerDNS Recursor! 🚀**

