# Setting Up MSYS2 in Cursor for PowerDNS Recursor Development

## Overview

This guide helps you set up MSYS2 directly in Cursor so you can develop PowerDNS Recursor for Windows using MinGW toolchain.

## What is MSYS2?

MSYS2 provides:
- **MinGW compiler** (GCC) for building Windows binaries
- **Bash shell** and Unix tools (make, git, etc.)
- **Package manager (pacman)** for dependencies
- **Native Windows binaries** (no WSL needed!)

## Step 1: Download and Install MSYS2

### Download
1. Go to: https://www.msys2.org/
2. Download the installer: `msys2-x86_64-YYYYMMDD.exe`
3. Run the installer and install to: `C:\msys64` (default)

### Install
```powershell
# After running the installer
# Click "Next", "Next", "Install", "Finish"
# It will open an MSYS2 terminal
```

## Step 2: Update MSYS2

The installer will open a terminal automatically. Run:

```bash
# In the MSYS2 UCRT64 terminal (or MinGW64)
pacman -Syu
# Close terminal when it says "Terminate all MSYS processes"
# Reopen MSYS2 UCRT64 terminal from Start Menu
# Run again:
pacman -Syu
```

**Important**: Always use **MSYS2 UCRT64** terminal, NOT the MSYS2 MSYS terminal!

## Step 3: Install Development Tools

Open **MSYS2 UCRT64** terminal (from Start Menu) and run:

```bash
# Install MinGW toolchain
pacman -S mingw-w64-ucrt-x86_64-gcc
pacman -S mingw-w64-ucrt-x86_64-gcc-fortran
pacman -S mingw-w64-ucrt-x86_64-cmake
pacman -S mingw-w64-ucrt-x86_64-make
pacman -S mingw-w64-ucrt-x86_64-pkg-config

# Install git
pacman -S git

# Install editors
pacman -S mingw-w64-ucrt-x86_64-nano
pacman -S mingw-w64-ucrt-x86_64-vim
```

## Step 4: Install Build Dependencies for PowerDNS

```bash
# Boost library
pacman -S mingw-w64-ucrt-x86_64-boost

# OpenSSL for DNSSEC
pacman -S mingw-w64-ucrt-x86_64-openssl

# libevent for I/O multiplexing
pacman -S mingw-w64-ucrt-x86_64-libevent

# curl (for downloading source files)
pacman -S mingw-w64-ucrt-x86_64-curl

# Other useful tools
pacman -S mingw-w64-ucrt-x86_64-wget
pacman -S mingw-w64-ucrt-x86_64-unzip
```

## Step 5: Configure MSYS2 for Cursor

### Option A: Use Cursor's Integrated Terminal with Bash

1. **Open Cursor Settings**:
   - Press `Ctrl+,` (or File → Preferences → Settings)
   - Search for: `terminal.integrated.shell`
   - Or search: `terminal.integrated.profiles`

2. **Add MSYS2 UCRT64 to terminal profiles**:
   - In settings, go to "Terminal → Integrated → Profiles: Windows"
   - Click "Add New" or edit settings.json directly
   
3. **Edit Cursor's settings.json**:
   
   ```json
   {
     "terminal.integrated.profiles.windows": {
       "MSYS2 UCRT64": {
         "path": "C:\\msys64\\ucrt64.exe",
         "icon": "terminal-bash"
       }
     },
     "terminal.integrated.defaultProfile.windows": "MSYS2 UCRT64"
   }
   ```

4. **Save and restart Cursor**

### Option B: Launch MSYS2 Terminal Manually

You can always launch MSYS2 UCRT64 from Start Menu → MSYS2 UCRT64.

## Step 6: Verify Installation

Open MSYS2 UCRT64 terminal (either in Cursor or from Start Menu) and run:

```bash
# Check versions
gcc --version
# Should show: gcc (ucrt-*) 

cmake --version
# Should show: cmake version 3.x

pkg-config --version

# Check dependencies
pkg-config --list-all | grep -i boost
pkg-config --list-all | grep -i openssl
pkg-config --list-all | grep -i libevent
```

Expected output:
```
gcc (ucrt *.*.*) 13.x.x
cmake version 3.x.x
```

## Step 7: Test with PowerDNS Recursor Build

Navigate to your project in MSYS2 terminal:

```bash
# In MSYS2 UCRT64 terminal
cd /c/pdns-recursor-5.3.0.2.relrec53x.g594400838/pdns_recursor_windows

# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug

# Build (this will take a few minutes)
make -j$(nproc)

# Run tests
# ./tests/pdns_recursor_tests
```

## Step 8: Configure Cursor for C++ Development

### Install Cursor Extensions:
1. Press `Ctrl+Shift+X` to open Extensions
2. Install these extensions:
   - **C/C++** (by Microsoft)
   - **CMake Tools** (by Microsoft)
   - **GitLens** (optional, for Git integration)

### Create .vscode/settings.json for IntelliSense:

Create `pdns_recursor_windows/.vscode/settings.json`:

```json
{
  "C_Cpp.default.compilerPath": "C:/msys64/ucrt64/bin/gcc.exe",
  "C_Cpp.default.cStandard": "c17",
  "C_Cpp.default.cppStandard": "c++20",
  "C_Cpp.default.intelliSenseMode": "gcc-x64",
  "C_Cpp.default.includePath": [
    "${workspaceFolder}/**",
    "C:/msys64/ucrt64/include",
    "C:/msys64/ucrt64/include/c++/*",
    "C:/msys64/ucrt64/include/boost",
    "C:/msys64/ucrt64/include/openssl"
  ],
  "files.associations": {
    "*.hh": "cpp",
    "*.cc": "cpp",
    "*.h": "c"
  }
}
```

## Step 9: Paths and Environments

MSYS2 paths work differently in bash vs Windows:

### In MSYS2 Bash (UCRT64 terminal):
```bash
# Current directory
cd /c/pdns-recursor-5.3.0.2.relrec53x.g594400838/pdns_recursor_windows
pwd
# Output: /c/pdns-recursor-5.3.0.2.relrec53x.g594400838/pdns_recursor_windows
```

### In PowerShell/CMD:
```powershell
# Current directory
cd C:\pdns-recursor-5.3.0.2.relrec53x.g594400838\pdns_recursor_windows
```

### Converting paths:
- PowerShell → MSYS2: `C:\` becomes `/c/`
- MSYS2 → PowerShell: `/c/` becomes `C:\`

## Step 10: Development Workflow in Cursor

### Daily Workflow:

1. **Open Cursor**
2. **Open folder**: `C:\pdns-recursor-5.3.0.2.relrec53x.g594400838\pdns_recursor_windows`
3. **Open integrated terminal**: Press `Ctrl+`` (backtick)
4. **Choose MSYS2 UCRT64 profile** (if configured)
5. **Build**:
   ```bash
   cd build
   make -j$(nproc)
   ```
6. **Debug** (in another terminal):
   ```bash
   gdb ./pdns_recursor.exe
   ```

### Build Commands Quick Reference:

```bash
# Full rebuild
rm -rf build && mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)

# Incremental build
cd build
make -j$(nproc)

# Run executable
./pdns_recursor.exe --version

# Clean
make clean

# Run tests
make test
```

## Troubleshooting

### Problem: "gcc not found"
```bash
# Solution: Make sure you're in UCRT64 terminal
# Check with:
echo $MSYSTEM
# Should output: UCRT64
```

### Problem: "CMake Error: CMAKE_C_COMPILER not set"
```bash
# Solution: Export environment
export PATH="/ucrt64/bin:$PATH"
cmake .. -G "MinGW Makefiles"
```

### Problem: "cannot find -lboost_context"
```bash
# Solution: Reinstall Boost
pacman -S mingw-w64-ucrt-x86_64-boost --overwrite='*'
```

### Problem: IntelliSense not working
- Restart Cursor after changing settings.json
- Check that compiler path in settings.json is correct
- Run "C/C++: Reset IntelliSense Database" from command palette

### Problem: Terminal not opening in Cursor
- Make sure you selected "MSYS2 UCRT64" profile in settings
- Try manually opening: `C:\msys64\ucrt64.exe`

## Additional Resources

- MSYS2 Documentation: https://www.msys2.org/docs/
- MSYS2 Wiki: https://github.com/msys2/msys2/wiki
- PowerDNS Build Instructions: `BUILD-INSTRUCTIONS.md`
- PowerDNS Architecture: `TECHNICAL-ARCHITECTURE.md`

## Next Steps

After setup is complete:
1. ✅ Read `IMPLEMENTATION-PLAN-SPRINTS.md` for development roadmap
2. ✅ Start with Sprint 1: Setup & Compilation
3. ✅ Follow `BUILD-INSTRUCTIONS.md` for detailed build process

---

**Happy Coding! 🚀**

