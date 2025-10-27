# PowerDNS Recursor - Windows Port

This directory contains the Windows port of PowerDNS Recursor using native Windows APIs and MSVC compiler.

## Project Status

This is a **Proof of Concept (POC)** implementation to demonstrate PowerDNS Recursor running natively on Windows.

### Current Phase: Sprint 1 - Minimal Core POC

**Completed:**
- ✅ Development environment setup (Visual Studio 2022, CMake, vcpkg)
- ✅ Dependencies installed (Boost, OpenSSL, libevent)

**In Progress:**
- 🔄 Project structure and build system

**Next Steps:**
- Copy core DNS parsing files
- First compilation attempt
- Implement Windows-specific I/O multiplexer using libevent

## Build Requirements

### Prerequisites

1. **Windows 10/11** (64-bit)
2. **Visual Studio 2022** (Community Edition or higher)
   - Desktop development with C++ workload
   - Windows 10/11 SDK
3. **CMake** 3.20 or higher
4. **vcpkg** package manager
5. **Git** for Windows

### Dependencies (via vcpkg)

The following packages must be installed:

```bash
cd C:\vcpkg
.\vcpkg install boost-context:x64-windows boost-system:x64-windows openssl:x64-windows libevent:x64-windows
.\vcpkg integrate install
```

**Installed Versions:**
- Boost 1.89.0 (context, system modules)
- OpenSSL 3.6.0
- libevent 2.1.12

## Building

### Configure with CMake

```bash
cd C:\pdns-recursor-5.3.0.2.relrec53x.g594400838\pdns_recursor_windows

# Create build directory
mkdir build
cd build

# Configure (vcpkg toolchain is auto-integrated)
cmake .. -DCMAKE_BUILD_TYPE=Release -A x64

# Or specify vcpkg explicitly:
cmake .. -DCMAKE_BUILD_TYPE=Release -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### Build

```bash
# Build from build directory
cmake --build . --config Release

# Or use Visual Studio
# Open pdns-recursor-windows.sln in Visual Studio and build
```

### Build Configurations

- **Debug**: Full debugging symbols, no optimizations
- **Release**: Optimized for performance
- **RelWithDebInfo**: Optimizations + debug info (recommended for testing)

## Project Structure

```
pdns_recursor_windows/
├── CMakeLists.txt              # Main build configuration
├── README.md                   # Project overview
├── README-WINDOWS.md           # This file
├── windows-compat.h            # Windows compatibility layer
├── .vscode/                    # VS Code configuration
│   └── settings.json
├── Specs/                      # All documentation & specs
│   ├── BUILD-INSTRUCTIONS.md
│   ├── POC-PROGRESS.md
│   ├── IMPLEMENTATION-PLAN-SPRINTS.md
│   ├── TECHNICAL-ARCHITECTURE.md
│   ├── PRD-PowerDNS-Cross-Platform.md
│   ├── DOCUMENTATION-INDEX.md
│   ├── implementation-plan.md
│   ├── unbound-windows-analysis.md
│   └── (more spec files...)
└── (source files)
```

## Key Implementation Strategies

### 1. Cross-Platform Approach
- Use conditional compilation (`#ifdef _WIN32`) for platform-specific code
- Maintain same file structure as original Linux version
- Copy files as-is from original, then adapt incrementally

### 2. Windows-Specific Adaptations

**I/O Multiplexing:**
- Use `libevent` for cross-platform event handling
- Replaces Linux `epoll` with Windows-compatible backend (IOCP/select)

**Threading:**
- Use `std::thread` and Boost.Context for coroutines
- Windows native threading as needed

**Networking:**
- Winsock2 instead of POSIX sockets
- Conditional compilation for socket options

**Process Management:**
- Windows Services API instead of Unix fork/daemon
- Service control for background operation

### 3. Build System
- CMake for cross-platform build configuration
- Native MSVC compiler (not MSYS2/MinGW)
- vcpkg for dependency management

## Development Workflow

### Current Sprint Tasks

See [Specs/IMPLEMENTATION-PLAN-SPRINTS.md](Specs/IMPLEMENTATION-PLAN-SPRINTS.md) for detailed sprint breakdown.

**Sprint 1 (Minimal Core POC):**
1. Environment setup ✅
2. Project structure 🔄
3. Copy core DNS files
4. First compilation

### Testing

```bash
# Run tests (when implemented)
cd build
ctest --output-on-failure
```

## Known Limitations (POC Phase)

- Limited feature set (core DNS resolution only)
- No service installation yet
- Configuration file support minimal
- DNSSEC support to be implemented

## References

### Documentation
See **[Specs/DOCUMENTATION-INDEX.md](Specs/DOCUMENTATION-INDEX.md)** for complete documentation index.

**Key Documents:**
- [Specs/POC-PROGRESS.md](Specs/POC-PROGRESS.md) - Current progress
- [Specs/BUILD-INSTRUCTIONS.md](Specs/BUILD-INSTRUCTIONS.md) - Detailed build guide
- [Specs/IMPLEMENTATION-PLAN-SPRINTS.md](Specs/IMPLEMENTATION-PLAN-SPRINTS.md) - Sprint planning
- [Specs/TECHNICAL-ARCHITECTURE.md](Specs/TECHNICAL-ARCHITECTURE.md) - Architecture

### External Links
- Original Project: https://github.com/PowerDNS/pdns
- PowerDNS Documentation: https://doc.powerdns.com/recursor/
- vcpkg: https://vcpkg.io/
- libevent: https://libevent.org/

## License

Same as PowerDNS Recursor (GPL v2)

## Contributing

This is a POC implementation. For questions or contributions, refer to the main PowerDNS project.

