# Building PowerDNS Recursor with Visual Studio (MSVC)

## 🎯 **Pure Windows Native Compilation**

This guide shows how to compile PowerDNS Recursor using **Microsoft Visual Studio** - no MSYS2, no MinGW, 100% Windows native tools.

---

## 📋 **Prerequisites**

### **1. Install Visual Studio 2022**
```
Download: https://visualstudio.microsoft.com/downloads/
Edition: Community (Free) or Professional
Workload: "Desktop development with C++"
```

### **2. Install vcpkg (Package Manager)**
```powershell
# PowerShell
cd C:\
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install
```

### **3. Install Dependencies via vcpkg**
```powershell
# In vcpkg directory
.\vcpkg install boost-context:x64-windows
.\vcpkg install boost-system:x64-windows
.\vcpkg install openssl:x64-windows
.\vcpkg install protobuf:x64-windows
```

---

## 🏗️ **Build Method 1: CMake + Visual Studio**

### **Step 1: Create CMakeLists.txt**

Already provided in `poc-implementation-guide.md`, but here's the MSVC-optimized version:

```cmake
cmake_minimum_required(VERSION 3.15)
project(pdns_recursor_windows CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC-specific settings
if(MSVC)
    add_definitions(-D_WIN32_WINNT=0x0601)
    add_definitions(-DNOMINMAX)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    
    # MSVC compiler flags
    add_compile_options(
        /W3           # Warning level 3
        /EHsc         # Exception handling
        /MP           # Multi-processor compilation
        /permissive-  # Standards conformance
    )
    
    # Release optimizations
    add_compile_options($<$<CONFIG:Release>:/O2 /Ob2 /Oi /Ot>)
endif()

# Find dependencies (vcpkg auto-detects)
find_package(Boost REQUIRED COMPONENTS context system)
find_package(OpenSSL REQUIRED)

# Source files
set(CORE_SOURCES
    # DNS Core (platform-independent)
    syncres.cc
    recursor_cache.cc
    negcache.cc
    dnsname.cc
    dnsparser.cc
    dnswriter.cc
    dnsrecords.cc
    qtype.cc
    
    # DNSSEC
    validate-recursor.cc
    dnssecinfra.cc
    
    # Caching
    aggressive_nsec.cc
    recpacketcache.cc
    
    # Threading
    mtasker_context.cc
    
    # Windows-specific
    wsaeventsmplexer.cc
    
    # Main
    rec-main.cc
    pdns_recursor.cc
)

add_executable(pdns_recursor ${CORE_SOURCES})

target_include_directories(pdns_recursor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(pdns_recursor PRIVATE
    Boost::context
    Boost::system
    OpenSSL::SSL
    OpenSSL::Crypto
    ws2_32      # Windows Sockets 2
    iphlpapi    # IP Helper API
    advapi32    # Advanced Windows API
)

# Output to bin directory
set_target_properties(pdns_recursor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
```

### **Step 2: Generate Visual Studio Solution**

```powershell
# PowerShell
cd C:\pdns-recursor-5.3.0.2.relrec53x.g594400838\pdns-recursor
mkdir build-vs
cd build-vs

# Generate VS solution
cmake .. -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake

# This creates: pdns_recursor_windows.sln
```

### **Step 3: Build**

**Option A: Command Line**
```powershell
cmake --build . --config Release
```

**Option B: Visual Studio IDE**
```
1. Open pdns_recursor_windows.sln in Visual Studio
2. Set build configuration to "Release"
3. Build → Build Solution (Ctrl+Shift+B)
4. Result: build-vs\bin\Release\pdns_recursor.exe
```

---

## 🏗️ **Build Method 2: Pure Visual Studio (Manual)**

### **Step 1: Create New Project**

```
File → New → Project
→ Empty Project (C++)
→ Name: pdns_recursor_windows
→ Location: C:\pdns-recursor\
```

### **Step 2: Add Source Files**

```
Solution Explorer → Source Files → Add → Existing Item
Select all .cc files from PowerDNS directory
```

### **Step 3: Configure Project Properties**

```
Right-click project → Properties

Configuration: All Configurations
Platform: x64

General:
  ├─ C++ Language Standard: ISO C++17
  └─ Windows SDK Version: (latest)

C/C++ → General:
  ├─ Additional Include Directories: 
  │    C:\vcpkg\installed\x64-windows\include
  │    $(ProjectDir)
  └─ SDL checks: No (/sdl-)

C/C++ → Preprocessor:
  Preprocessor Definitions:
    WIN32
    _WIN32
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS
    _WIN32_WINNT=0x0601

C/C++ → Code Generation:
  Runtime Library: Multi-threaded DLL (/MD)

Linker → General:
  Additional Library Directories:
    C:\vcpkg\installed\x64-windows\lib

Linker → Input:
  Additional Dependencies:
    ws2_32.lib
    iphlpapi.lib
    advapi32.lib
    libssl.lib
    libcrypto.lib
    boost_context-vc143-mt-x64.lib
    boost_system-vc143-mt-x64.lib
```

### **Step 4: Build**

```
Build → Build Solution
Result: x64\Release\pdns_recursor_windows.exe
```

---

## 🔧 **Handling Unix/Linux Code**

### **Common Issues & Fixes**

#### **Issue 1: POSIX Functions**
```cpp
// Unix code
#include <unistd.h>
close(fd);

// Windows equivalent
#ifdef _WIN32
    #include <io.h>
    #define close _close
#else
    #include <unistd.h>
#endif
```

#### **Issue 2: Socket Types**
```cpp
// Unix code
int socket_fd;

// Windows code
#ifdef _WIN32
    typedef SOCKET socket_fd_t;
#else
    typedef int socket_fd_t;
#endif
```

#### **Issue 3: Error Codes**
```cpp
// Unix code
if (errno == EWOULDBLOCK)

// Windows code
#ifdef _WIN32
    if (WSAGetLastError() == WSAEWOULDBLOCK)
#else
    if (errno == EWOULDBLOCK)
#endif
```

---

## 🧪 **Testing the Build**

### **Test 1: Verify Binary**
```powershell
# Check it's a real Windows PE executable
Get-Item .\bin\Release\pdns_recursor.exe | Select-Object *

# Should show:
# VersionInfo: Microsoft Visual C++
# ProductVersion: ...
```

### **Test 2: Check Dependencies**
```powershell
# Install Dependencies Walker or use:
dumpbin /DEPENDENTS .\bin\Release\pdns_recursor.exe

# Should show Windows DLLs:
# KERNEL32.dll
# ws2_32.dll
# VCRUNTIME140.dll
# etc.
```

### **Test 3: Run**
```powershell
.\bin\Release\pdns_recursor.exe --version
.\bin\Release\pdns_recursor.exe --local-address=127.0.0.1:5353
```

---

## 🚀 **Advantages of MSVC Build**

### **Performance**
```
MSVC optimizations for Windows:
- Profile-Guided Optimization (PGO)
- Link-Time Code Generation (LTCG)
- Windows-specific CPU instructions
```

### **Debugging**
```
Visual Studio debugger:
- Best Windows debugging experience
- Memory profiler
- Performance profiler
- Easy breakpoints
```

### **Integration**
```
Native Windows development:
- IntelliSense
- Code navigation
- Refactoring tools
- Unit test integration
```

---

## 📊 **MSVC vs MinGW Comparison**

| Feature | MSVC | MinGW |
|---------|------|-------|
| **Optimization** | ✅ Best for Windows | ✅ Good |
| **Binary Size** | ✅ Smaller | ⚠️ Larger |
| **Debugging** | ✅ Visual Studio | ⚠️ GDB |
| **Unix Code Porting** | ⚠️ May need changes | ✅ Easier |
| **C++ Standards** | ✅ Latest | ✅ Latest |
| **Build Speed** | ✅ Fast with /MP | ✅ Fast |
| **Free** | ✅ Community Edition | ✅ Yes |
| **Professional** | ✅ Industry standard | ⚠️ Less common |

---

## 🎯 **Recommended Workflow**

### **Development Phase**
```
1. Use Visual Studio Code + CMake
   - Fast iteration
   - Good for both MSVC and MinGW
   
2. Test with both compilers
   - MSVC for production
   - MinGW for Linux compatibility check
```

### **Production Phase**
```
1. Build with MSVC Release mode
   - Best optimization
   - Smallest binary
   
2. Enable all optimizations
   cmake --build . --config Release

3. Strip if needed
   (MSVC produces optimized binaries by default)
```

---

## 🐛 **Debugging Tips**

### **Visual Studio Debugger**
```
1. Set pdns_recursor as startup project
2. Properties → Debugging:
   - Command Arguments: --local-address=127.0.0.1:5353 --daemon=no
   - Working Directory: $(ProjectDir)
3. F5 to debug
4. Set breakpoints, inspect variables, step through
```

### **Performance Profiling**
```
Debug → Performance Profiler
- CPU Usage
- Memory Usage
- Thread Activity
```

---

## 📦 **Deployment**

### **Collect All Dependencies**
```powershell
# Copy DLLs
Copy-Item C:\vcpkg\installed\x64-windows\bin\*.dll .\bin\Release\

# Result: Self-contained folder
bin\Release\
  ├── pdns_recursor.exe
  ├── libssl-3-x64.dll
  ├── libcrypto-3-x64.dll
  └── boost_context-vc143-mt-x64.dll
```

### **Create Installer**
```
Use: Inno Setup, WiX, or NSIS
Include: .exe + DLLs + config files
Register as Windows Service
```

---

## ✅ **Summary**

**MSVC Build**:
- ✅ 100% Windows native
- ✅ Best performance
- ✅ Professional tooling
- ✅ No MSYS2 needed (even for building!)
- ✅ Industry standard on Windows

**Result**: `pdns_recursor.exe` that runs on any Windows machine!

---

## 🔗 **Next Steps**

1. ✅ Install Visual Studio 2022
2. ✅ Install vcpkg + dependencies
3. ✅ Use provided CMakeLists.txt
4. ✅ Build with MSVC
5. ✅ Test on Windows
6. ✅ Deploy to users

**No MSYS2, no MinGW, pure Windows development!** 🎯


