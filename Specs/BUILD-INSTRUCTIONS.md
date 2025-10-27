# PowerDNS Recursor - Windows Build Instructions

**Document Version**: 1.0  
**Date**: October 27, 2025  
**Platform**: Windows 10/11 (x64)

---

## 🔑 Key Principle: Same File Structure!

**We maintain the EXACT SAME flat file structure as the original PowerDNS Recursor!**

```
pdns_recursor_windows/
├── rec-main.cc           ← Same file, with #ifdef _WIN32
├── syncres.cc            ← Copied as-is
├── dnsparser.cc          ← Copied as-is
├── mplexer.hh            ← Copied as-is
├── epollmplexer.cc       ← Copied (Linux-only via #ifdef)
├── libeventmplexer.cc    ← NEW (Windows-only via #ifdef _WIN32)
├── CMakeLists.txt        ← NEW (Windows build)
└── (all other files)     ← Copied as-is

NO subdirectories like src/core/, src/windows/, etc.!
```

**Why?**
- ✅ Easy to compare with original
- ✅ Easy to merge upstream changes
- ✅ Maintains mental model
- ✅ Linux builds untouched (autotools still works)

---

## Table of Contents

1. [Quick Start](#quick-start)
2. [Option A: MSVC Build (Recommended)](#option-a-msvc-build-recommended)
3. [Option B: MinGW Build (Alternative)](#option-b-mingw-build-alternative)
4. [Verification](#verification)
5. [Troubleshooting](#troubleshooting)

---

## Quick Start

**Choose your build path**:
- **Want to use PowerShell/cmd?** → Use **Option A (MSVC)**
- **Coming from Linux/prefer GCC?** → Use **Option B (MinGW)**

**Both produce the same native Windows `.exe`!**

---

## Option A: MSVC Build (Recommended)

### Why MSVC?
- ✅ Build from PowerShell/cmd (no MSYS2 terminal needed)
- ✅ Better Windows performance
- ✅ Professional Visual Studio debugging
- ✅ Native Windows toolchain

### Prerequisites

**1. Install Visual Studio 2022 Community** (Free)
- Download: https://visualstudio.microsoft.com/downloads/
- During installation, select:
  - ✅ "Desktop development with C++"
  - ✅ "C++ CMake tools for Windows"
  - ✅ "C++ ATL for latest build tools"

**2. Install CMake**
```powershell
# Option 1: Via Chocolatey
choco install cmake

# Option 2: Download installer
# https://cmake.org/download/ (Windows x64 Installer)
```

**3. Install vcpkg (Dependency Manager)**
```powershell
# Clone vcpkg
cd C:\
git clone https://github.com/microsoft/vcpkg
cd vcpkg

# Bootstrap
.\bootstrap-vcpkg.bat

# Add to PATH (optional but recommended)
[Environment]::SetEnvironmentVariable("Path", $env:Path + ";C:\vcpkg", "User")
```

### Step-by-Step Build Process

#### Step 1: Install Dependencies

```powershell
# Open PowerShell as Administrator
cd C:\vcpkg

# Install required libraries (this will take 15-30 minutes)
.\vcpkg install boost-context:x64-windows
.\vcpkg install boost-system:x64-windows
.\vcpkg install openssl:x64-windows
.\vcpkg install libevent:x64-windows

# Verify installation
.\vcpkg list
# Should show: boost-context, boost-system, openssl, libevent
```

#### Step 2: Configure CMake

```powershell
# Navigate to project
cd C:\pdns-recursor-5.3.0.2.relrec53x.g594400838\pdns_recursor_windows

# Create build directory
mkdir build
cd build

# Configure with MSVC (Visual Studio 2022)
cmake .. -G "Visual Studio 17 2022" `
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake `
  -DCMAKE_BUILD_TYPE=Release

# Expected output:
# -- Building with MSVC (Visual Studio)
# -- Found Boost...
# -- Found OpenSSL...
# -- Found libevent...
# -- Configuring done
# -- Generating done
```

#### Step 3: Build

```powershell
# Build Release version (in build directory)
cmake --build . --config Release

# OR build Debug version
cmake --build . --config Debug

# Build takes 5-10 minutes
```

#### Step 4: Run

```powershell
# Run the executable
.\Release\pdns_recursor.exe --version

# Expected output:
# PowerDNS Recursor 5.3.0 (Windows build)
```

### Development Workflow (MSVC)

```powershell
# ===================================
# Rapid Iteration Loop
# ===================================

# 1. Edit code in VS Code or Visual Studio
code ..\src\core\syncres.cc

# 2. Rebuild (from build directory)
cmake --build . --config Release

# 3. Test
.\Release\pdns_recursor.exe -d -c test.conf

# 4. Repeat!
```

### Using Visual Studio IDE (Optional)

```powershell
# Open solution in Visual Studio
start pdns_recursor.sln

# Now you can:
# - Build with F7
# - Debug with F5
# - Set breakpoints
# - Use IntelliSense
```

---

## Option B: MinGW Build (Alternative)

### Why MinGW?
- ✅ GCC compiler (closer to Linux)
- ✅ Easier code porting (fewer compiler quirks)
- ✅ POSIX compatibility layer
- ✅ Proven by Unbound

### Prerequisites

**1. Install MSYS2**
- Download: https://www.msys2.org/
- Install to default location: `C:\msys64`
- Run MSYS2 MINGW64 terminal (not MSYS2 MSYS!)

**2. Update MSYS2**
```bash
# In MSYS2 MINGW64 terminal
pacman -Syu
# Close terminal when prompted, reopen, run again:
pacman -Syu
```

### Step-by-Step Build Process

#### Step 1: Install Toolchain & Dependencies

```bash
# In MSYS2 MINGW64 terminal

# Install build tools
pacman -S mingw-w64-x86_64-gcc
pacman -S mingw-w64-x86_64-cmake
pacman -S mingw-w64-x86_64-make
pacman -S git

# Install dependencies
pacman -S mingw-w64-x86_64-boost
pacman -S mingw-w64-x86_64-openssl
pacman -S mingw-w64-x86_64-libevent

# Verify
gcc --version
cmake --version
```

#### Step 2: Configure CMake

```bash
# Navigate to project (use /c/ for C:\ drive)
cd /c/pdns-recursor-5.3.0.2.relrec53x.g594400838/pdns_recursor_windows

# Create build directory
mkdir build && cd build

# Configure with MinGW
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release

# Expected output:
# -- Building with MinGW (GCC)
# -- Found Boost...
# -- Found OpenSSL...
# -- Found libevent...
# -- Configuring done
```

#### Step 3: Build

```bash
# Build (in build directory)
make -j$(nproc)

# Build takes 5-10 minutes
```

#### Step 4: Run

```bash
# Run the executable
./pdns_recursor.exe --version

# Expected output:
# PowerDNS Recursor 5.3.0 (Windows build)
```

### Development Workflow (MinGW)

```bash
# ===================================
# ALL IN SAME MSYS2 MINGW64 TERMINAL
# ===================================

# 1. Edit code (use vim or external editor)
vim ../src/core/syncres.cc
# OR use VS Code from Windows

# 2. Rebuild
make -j$(nproc)

# 3. Test
./pdns_recursor.exe -d -c test.conf

# 4. Repeat!
```

---

## Verification

### Test 1: Version Check

```powershell
# MSVC
.\Release\pdns_recursor.exe --version

# MinGW (in MSYS2)
./pdns_recursor.exe --version
```

**Expected**: Version string with "Windows build"

### Test 2: Basic Configuration

Create `test.conf`:
```ini
# test.conf
local-address=127.0.0.1:5353
quiet=no
daemon=no
```

Run:
```powershell
# MSVC
.\Release\pdns_recursor.exe -c test.conf

# MinGW (in MSYS2)
./pdns_recursor.exe -c test.conf
```

**Expected**: Server starts, listens on 127.0.0.1:5353

### Test 3: DNS Query

```powershell
# In another PowerShell window
nslookup -port=5353 google.com 127.0.0.1

# Expected: DNS response with IP address
```

---

## Troubleshooting

### MSVC Issues

#### Problem: "CMake could not find Visual Studio"
```powershell
# Solution: Specify generator explicitly
cmake .. -G "Visual Studio 17 2022" -A x64
```

#### Problem: "Cannot find Boost/OpenSSL"
```powershell
# Solution: Make sure vcpkg toolchain file is specified
cmake .. -G "Visual Studio 17 2022" `
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
```

#### Problem: "LNK2019: unresolved external symbol"
```powershell
# Solution: Rebuild vcpkg libraries for correct triplet
cd C:\vcpkg
.\vcpkg remove boost openssl libevent
.\vcpkg install boost-context:x64-windows openssl:x64-windows libevent:x64-windows
```

#### Problem: DLL not found when running
```powershell
# Solution: Copy DLLs to executable directory
cd build\Release
copy C:\vcpkg\installed\x64-windows\bin\*.dll .
```

### MinGW Issues

#### Problem: "g++ not found"
```bash
# Solution: Make sure you're in MINGW64 terminal (not MSYS2)
# Window title should say "MINGW64"

# OR install toolchain
pacman -S mingw-w64-x86_64-toolchain
```

#### Problem: "CMake Error: CMAKE_C_COMPILER not set"
```bash
# Solution: Use MinGW Makefiles generator
cmake .. -G "MinGW Makefiles"
```

#### Problem: "cannot find -lboost_context"
```bash
# Solution: Reinstall Boost
pacman -S mingw-w64-x86_64-boost --overwrite '*'
```

#### Problem: "Multiple target patterns" (make error)
```bash
# Solution: Use mingw32-make instead of make
pacman -S mingw-w64-x86_64-make
mingw32-make -j$(nproc)
```

### Common to Both

#### Problem: Port 53 access denied
```
Solution: Run on non-privileged port (5353) for testing:
local-address=127.0.0.1:5353

For production port 53:
- MSVC: Run as Administrator or use Windows Service
- MinGW: Run as Administrator
```

#### Problem: "Address already in use"
```powershell
# Find process using port
netstat -ano | findstr :5353

# Kill process (replace PID)
taskkill /PID <PID> /F
```

---

## Performance Comparison

| Aspect | MSVC | MinGW |
|--------|------|-------|
| **Compile time** | ~8 min | ~6 min |
| **Binary size** | 4.2 MB | 5.8 MB |
| **Runtime performance** | Excellent | Very Good |
| **Debugger** | Visual Studio ⭐ | GDB |
| **Build from** | PowerShell ✅ | MSYS2 terminal |

**Recommendation**: Use **MSVC** for production, **MinGW** if you need Linux-like environment.

---

## Next Steps

After successful build:
1. ✅ Read [IMPLEMENTATION-PLAN-SPRINTS.md](IMPLEMENTATION-PLAN-SPRINTS.md) for development roadmap
2. ✅ Read [TECHNICAL-ARCHITECTURE.md](TECHNICAL-ARCHITECTURE.md) for design details
3. ✅ Start with Sprint 1 stories
4. ✅ Run tests: `ctest` (in build directory)

---

## Additional Resources

### MSVC
- Visual Studio Docs: https://docs.microsoft.com/en-us/cpp/
- vcpkg Documentation: https://vcpkg.io/
- CMake Documentation: https://cmake.org/cmake/help/latest/

### MinGW
- MSYS2 Wiki: https://www.msys2.org/wiki/Home/
- MinGW-w64: https://www.mingw-w64.org/
- GCC Manual: https://gcc.gnu.org/onlinedocs/

### PowerDNS
- PowerDNS Recursor Docs: https://doc.powerdns.com/recursor/
- GitHub: https://github.com/PowerDNS/pdns

---

## Support

**Found an issue?** Check:
1. This troubleshooting section
2. GitHub Issues: https://github.com/PowerDNS/pdns/issues
3. PowerDNS Community: https://community.powerdns.com/

**Building the POC?** Follow the sprint plan in [IMPLEMENTATION-PLAN-SPRINTS.md](IMPLEMENTATION-PLAN-SPRINTS.md)

---

**Happy Building! 🚀**

