# Implementation Plan & Sprint Breakdown
## PowerDNS Recursor - Windows Support

**Document Version**: 1.0  
**Date**: October 27, 2025  
**Author**: Program Management / Scrum Master  
**Sprint Duration**: 2 weeks  
**Team Size**: 1-2 developers  
**Total Duration**: 26 weeks (13 sprints)

---

## 1. Overall Phases

### Phase Overview
```
Phase 1: POC (4-5 weeks)           → Sprints 1-3
Phase 2: Core Features (12 weeks)  → Sprints 4-9
Phase 3: Advanced (6 weeks)        → Sprints 10-12
Phase 4: Release (2 weeks)         → Sprint 13
```

---

## 2. PHASE 1: Proof of Concept (Sprints 1-3)

### Sprint 1: Foundation & Basic Compilation

**Duration**: 2 weeks  
**Goal**: Get minimal DNS code compiling on Windows

#### Sprint 1 Stories

**Story 1.1: Development Environment Setup**
- **Points**: 3
- **Priority**: CRITICAL
- **Description**: Set up Windows build environment with dual compiler support
- **Build Path Options**:
  
  **Option A: MSVC + Visual Studio (RECOMMENDED for PowerShell/cmd)**
  - Native Windows compilation
  - Use from PowerShell/cmd directly (no MSYS2 terminal)
  - Better debugging with Visual Studio IDE
  - Professional Windows development experience
  - Better performance (optimized Windows codegen)
  
  **Option B: MinGW + MSYS2 (Alternative)**
  - GCC-based compilation
  - Requires MSYS2 terminal for build
  - Closer to Linux toolchain (easier porting)
  - Good for cross-platform testing
  - Proven approach (Unbound uses this)

- **Tasks**:
  1. Choose build path (both produce native .exe!)
  2a. **IF MSVC**:
     - Install Visual Studio 2022 Community (free)
     - Install CMake 3.15+
     - Install vcpkg for dependencies
     - Install: Boost, OpenSSL, libevent via vcpkg
  2b. **IF MinGW**:
     - Install MSYS2
     - Install MinGW toolchain
     - Install dependencies via pacman
     - Install: Boost, OpenSSL, libevent
  3. Verify Unbound still works (reference)
  4. Document chosen setup process
  5. Create hello-world test program

- **Acceptance Criteria**:
  - Chosen toolchain installed and working
  - Can compile hello-world C++ program
  - CMake detects compiler correctly
  - Dependencies available
  - Unbound reference works (if needed)
  - Documentation updated with chosen path

**Story 1.2: Create Project Structure**
- **Points**: 2
- **Description**: Set up build structure (NO directory changes - same as original!)
- **Tasks**:
  - Create initial CMakeLists.txt (at root level)
  - Copy pdns-recursor/.gitignore (if exists)
  - Create Windows-specific README-WINDOWS.md
  - Verify directory is ready for file copying
- **Acceptance Criteria**:
  - CMakeLists.txt skeleton exists
  - README-WINDOWS.md created
  - Ready to copy files from original
  
**Note**: We maintain the SAME flat file structure as original PowerDNS!

**Story 1.3: Copy Minimal Core Files**
- **Points**: 5
- **Description**: Copy core DNS files to pdns_recursor_windows/ (same flat structure)
- **Tasks**:
  - Copy dnsname.cc/hh from ../pdns-recursor/
  - Copy dnsparser.cc/hh from ../pdns-recursor/
  - Copy dnswriter.cc/hh from ../pdns-recursor/
  - Copy qtype.cc/hh from ../pdns-recursor/
  - Copy misc.hh from ../pdns-recursor/
  - Copy any dependencies these files need
- **Acceptance Criteria**:
  - Files copied to pdns_recursor_windows/ root
  - Same filenames as original (no renaming!)
  - Git tracks changes
  
**Note**: Copy to root level, NOT to subdirectories!

**Story 1.4: First Compilation Attempt**
- **Points**: 8
- **Tasks**:
  - Add files to CMakeLists.txt
  - Attempt build
  - Fix compilation errors (iterative)
  - Document errors and solutions
- **Acceptance Criteria**:
  - Core library compiles
  - No compilation errors
  - Can link into test program

**Sprint 1 Total**: 18 points

---

### Sprint 2: I/O Multiplexer

**Duration**: 2 weeks  
**Goal**: Implement Windows I/O event handling using libevent

#### Sprint 2 Stories

**Story 2.1: Install and Verify libevent**
- **Points**: 2
- **Tasks**:
  - Install libevent via MSYS2 or vcpkg
  - Create simple libevent test program
  - Verify it works on Windows
- **Acceptance Criteria**:
  - libevent installed
  - Test program compiles and runs

**Story 2.2: Copy Multiplexer Interface**
- **Points**: 3
- **Tasks**:
  - Copy mplexer.hh from original
  - Add Windows socket compatibility (#ifdef)
  - Update CMakeLists.txt
- **Acceptance Criteria**:
  - mplexer.hh compiles on Windows
  - Interface is unchanged

**Story 2.3: Implement LibeventMultiplexer**
- **Points**: 13
- **Description**: Create new Windows I/O multiplexer (similar to epollmplexer.cc)
- **Tasks**:
  - Create libeventmplexer.cc at root (same level as epollmplexer.cc)
  - Implement all FDMultiplexer methods using libevent
  - Handle Winsock initialization (WSAStartup/WSACleanup)
  - Add #ifdef _WIN32 guards
  - Study Unbound's winsock_event.c for reference
- **Acceptance Criteria**:
  - libeventmplexer.cc at root level
  - All methods implemented
  - Compiles without errors
  - Follows FDMultiplexer interface from mplexer.hh
  
**Note**: File at root like epollmplexer.cc, NOT in subdirectory!

**Story 2.4: Unit Test Multiplexer**
- **Points**: 5
- **Tasks**:
  - Create test program
  - Test addReadFD/removeReadFD
  - Test event triggering
  - Test timeout handling
- **Acceptance Criteria**:
  - Can add/remove file descriptors
  - Events trigger callbacks
  - No crashes

**Sprint 2 Total**: 23 points

---

### Sprint 3: Basic UDP Query/Response

**Duration**: 2 weeks  
**Goal**: Accept UDP query, return dummy response

#### Sprint 3 Stories

**Story 3.1: Socket Compatibility Layer**
- **Points**: 5
- **Tasks**:
  - Create socket_compat.hh
  - Add Winsock2 includes and typedefs
  - Add WSAStartup() wrapper
  - Test socket creation
- **Acceptance Criteria**:
  - Can create UDP socket on Windows
  - Socket operations work
  - No memory leaks

**Story 3.2: Create Minimal Main Program**
- **Points**: 8
- **Tasks**:
  - Create main.cc
  - Initialize Winsock
  - Create UDP socket on port 5353
  - Bind socket
  - Add to multiplexer
- **Acceptance Criteria**:
  - Program starts
  - Binds to port 5353
  - Listens for events

**Story 3.3: Receive UDP Query**
- **Points**: 5
- **Tasks**:
  - Implement callback for read events
  - Use recvfrom() to get query
  - Log received data
- **Acceptance Criteria**:
  - Receives UDP packets
  - Logs query data
  - No crashes on invalid input

**Story 3.4: Send Dummy Response**
- **Points**: 5
- **Tasks**:
  - Parse basic DNS query
  - Create hardcoded response packet
  - Use sendto() to reply
  - Test with dig or PowerShell script
- **Acceptance Criteria**:
  - Sends response packet
  - Client receives response
  - Can query from test script

**Sprint 3 Total**: 23 points

**Phase 1 Total**: 64 points (3 sprints)  
**Phase 1 Deliverable**: ✅ POC that accepts queries and returns dummy responses

---

## 3. PHASE 2: Core Features (Sprints 4-9)

### Sprint 4: Core DNS Resolution - Part 1

**Duration**: 2 weeks  
**Goal**: Integrate real DNS resolution logic

#### Sprint 4 Stories

**Story 4.1: Copy Core Resolution Files**
- **Points**: 5
- **Tasks**:
  - Copy syncres.cc/hh
  - Copy rec-lua-conf.cc/hh (if needed)
  - Copy supporting headers
  - Attempt compilation
- **Acceptance Criteria**:
  - Files copied
  - Compilation attempted
  - Errors documented

**Story 4.2: Fix Compilation Errors (Iterative)**
- **Points**: 13
- **Tasks**:
  - Fix each compilation error
  - Add #ifdef _WIN32 where needed
  - Comment out problematic sections temporarily
  - Document all changes
- **Acceptance Criteria**:
  - syncres.cc compiles
  - All dependencies resolved
  - Changes documented

**Story 4.3: Integrate SyncRes with Main**
- **Points**: 5
- **Tasks**:
  - Create SyncRes instance in query handler
  - Parse incoming query
  - Call SyncRes::resolve()
  - Handle exceptions
- **Acceptance Criteria**:
  - SyncRes is instantiated
  - Resolve method is called
  - Errors are handled

**Sprint 4 Total**: 23 points

---

### Sprint 5: Core DNS Resolution - Part 2

**Duration**: 2 weeks  
**Goal**: Complete resolution chain (root → TLD → auth)

#### Sprint 5 Stories

**Story 5.1: Root Server Queries**
- **Points**: 8
- **Tasks**:
  - Verify root hints work
  - Test query to root servers
  - Handle root responses
  - Log resolution path
- **Acceptance Criteria**:
  - Queries root servers successfully
  - Parses NS records
  - Follows referrals

**Story 5.2: TLD and Authoritative Queries**
- **Points**: 8
- **Tasks**:
  - Query TLD servers (.com, .net, etc.)
  - Query authoritative servers
  - Handle glue records
  - Complete resolution chain
- **Acceptance Criteria**:
  - Full resolution works (root → TLD → auth)
  - Returns final A record
  - Logs show complete path

**Story 5.3: Error Handling**
- **Points**: 5
- **Tasks**:
  - Handle NXDOMAIN
  - Handle SERVFAIL
  - Handle timeouts
  - Return appropriate error codes
- **Acceptance Criteria**:
  - Errors handled gracefully
  - Correct DNS error codes returned
  - No crashes on errors

**Story 5.4: End-to-End Testing**
- **Points**: 3
- **Tasks**:
  - Test google.com resolution
  - Test github.com (CNAME chain)
  - Test nonexistent.example.com (NXDOMAIN)
  - Document test results
- **Acceptance Criteria**:
  - All test queries work
  - Responses are correct
  - Performance is reasonable

**Sprint 5 Total**: 24 points

---

### Sprint 6: Caching

**Duration**: 2 weeks  
**Goal**: Implement DNS response caching

#### Sprint 6 Stories

**Story 6.1: Copy Cache Implementation**
- **Points**: 5
- **Tasks**:
  - Copy recursor_cache.cc/hh
  - Copy negcache.cc/hh
  - Fix compilation errors
  - Add to CMakeLists.txt
- **Acceptance Criteria**:
  - Cache files compile
  - Dependencies resolved

**Story 6.2: Integrate Cache with Resolution**
- **Points**: 8
- **Tasks**:
  - Check cache before resolution
  - Store responses in cache
  - Honor TTL values
  - Test cache hits
- **Acceptance Criteria**:
  - Cache stores responses
  - Cache serves cached responses
  - TTL is respected

**Story 6.3: Negative Caching**
- **Points**: 5
- **Tasks**:
  - Cache NXDOMAIN responses
  - Cache NODATA responses
  - Implement negative TTL
  - Test negative cache
- **Acceptance Criteria**:
  - NXDOMAIN is cached
  - Serves from negative cache
  - Negative TTL works

**Story 6.4: Cache Statistics**
- **Points**: 5
- **Tasks**:
  - Count cache hits/misses
  - Log cache size
  - Implement cache clear
  - Log statistics
- **Acceptance Criteria**:
  - Hit/miss counters work
  - Can clear cache
  - Statistics are accurate

**Sprint 6 Total**: 23 points

---

### Sprint 7: TCP Support

**Duration**: 2 weeks  
**Goal**: Add TCP transport for DNS

#### Sprint 7 Stories

**Story 7.1: TCP Socket Handling**
- **Points**: 8
- **Tasks**:
  - Create TCP listening socket
  - Accept TCP connections
  - Add TCP sockets to multiplexer
  - Handle connection lifecycle
- **Acceptance Criteria**:
  - Accepts TCP connections
  - Multiple connections handled
  - Connections close properly

**Story 7.2: TCP Query Parsing**
- **Points**: 5
- **Tasks**:
  - Read DNS message length prefix (2 bytes)
  - Read full DNS message
  - Parse TCP query
  - Handle partial reads
- **Acceptance Criteria**:
  - Parses TCP queries correctly
  - Handles fragmentation
  - No buffer overflows

**Story 7.3: TCP Response Sending**
- **Points**: 5
- **Tasks**:
  - Add length prefix to response
  - Send response over TCP
  - Handle partial writes
  - Close connection after response
- **Acceptance Criteria**:
  - Sends complete response
  - Length prefix correct
  - Connection handled properly

**Story 7.4: TCP Testing**
- **Points**: 5
- **Tasks**:
  - Test with dig +tcp
  - Test large responses (>512 bytes)
  - Test multiple queries per connection
  - Test connection timeout
- **Acceptance Criteria**:
  - TCP queries work
  - Large responses work
  - No memory leaks

**Sprint 7 Total**: 23 points

---

### Sprint 8: DNSSEC Validation - Part 1

**Duration**: 2 weeks  
**Goal**: Basic DNSSEC validation

#### Sprint 8 Stories

**Story 8.1: Copy DNSSEC Files**
- **Points**: 8
- **Tasks**:
  - Copy validate-recursor.cc/hh
  - Copy dnssecinfra.cc/hh
  - Copy opensslsigners.cc/hh
  - Fix compilation errors
- **Acceptance Criteria**:
  - DNSSEC files compile
  - OpenSSL integration works

**Story 8.2: Trust Anchor Configuration**
- **Points**: 5
- **Tasks**:
  - Load root trust anchor
  - Verify trust anchor format
  - Test trust anchor loading
- **Acceptance Criteria**:
  - Root TA loads correctly
  - Can validate against TA

**Story 8.3: Basic Signature Validation**
- **Points**: 8
- **Tasks**:
  - Implement RRSIG validation
  - Validate DS records
  - Check chain of trust
  - Set AD flag when validated
- **Acceptance Criteria**:
  - RRSIG validation works
  - Chain of trust verified
  - AD flag set correctly

**Story 8.4: DNSSEC Testing**
- **Points**: 3
- **Tasks**:
  - Test with DNSSEC-signed domains
  - Test with invalid signatures
  - Test with missing DS records
- **Acceptance Criteria**:
  - Valid signatures accepted
  - Invalid signatures rejected
  - Errors handled

**Sprint 8 Total**: 24 points

---

### Sprint 9: DNSSEC Validation - Part 2 & Configuration

**Duration**: 2 weeks  
**Goal**: Complete DNSSEC, add configuration support

#### Sprint 9 Stories

**Story 9.1: Advanced DNSSEC**
- **Points**: 8
- **Tasks**:
  - Handle all signature algorithms
  - NSEC/NSEC3 validation
  - Negative trust anchors
  - DNSSEC statistics
- **Acceptance Criteria**:
  - All algorithms supported
  - NSEC validation works
  - Statistics tracked

**Story 9.2: Configuration File Parsing**
- **Points**: 8
- **Tasks**:
  - Copy config_file.cc/hh
  - Add Windows default paths
  - Parse YAML configuration
  - Test config loading
- **Acceptance Criteria**:
  - Config file parsed
  - Settings applied
  - Errors reported

**Story 9.3: Command-Line Options**
- **Points**: 5
- **Tasks**:
  - Add CLI argument parsing
  - Override config with CLI
  - Add --help option
  - Test all options
- **Acceptance Criteria**:
  - CLI options work
  - Override config file
  - Help text correct

**Story 9.4: Sprint 9 Testing**
- **Points**: 3
- **Tasks**:
  - Integration testing
  - Performance testing
  - Regression testing
- **Acceptance Criteria**:
  - All features work together
  - Performance acceptable
  - No regressions

**Sprint 9 Total**: 24 points

**Phase 2 Total**: 141 points (6 sprints)  
**Phase 2 Deliverable**: ✅ Full-featured DNS recursor with DNSSEC

---

## 4. PHASE 3: Advanced Features (Sprints 10-12)

### Sprint 10: Windows Service Integration

**Duration**: 2 weeks  
**Goal**: Run as Windows Service

#### Sprint 10 Stories

**Story 10.1: Windows Service Skeleton**
- **Points**: 8
- **Tasks**:
  - Create win_service.cc/hh
  - Implement ServiceMain()
  - Register service control handler
  - Handle SERVICE_CONTROL_STOP
- **Acceptance Criteria**:
  - Service registers
  - Starts and stops
  - Responds to control commands

**Story 10.2: Service Installation**
- **Points**: 5
- **Tasks**:
  - Add --install-service option
  - Call CreateService() API
  - Set service parameters
  - Test installation
- **Acceptance Criteria**:
  - Service installs successfully
  - Appears in Services.msc
  - Can be started manually

**Story 10.3: Service Configuration**
- **Points**: 5
- **Tasks**:
  - Read config from registry or file
  - Set service description
  - Configure auto-start
  - Set recovery options
- **Acceptance Criteria**:
  - Config read correctly
  - Service auto-starts
  - Recovery works

**Story 10.4: Logging Integration**
- **Points**: 5
- **Tasks**:
  - Write to Windows Event Log
  - Define event sources
  - Add log levels
  - Test logging
- **Acceptance Criteria**:
  - Logs appear in Event Viewer
  - All log levels work
  - No log spam

**Sprint 10 Total**: 23 points

---

### Sprint 11: Monitoring & Advanced Features

**Duration**: 2 weeks  
**Goal**: Add monitoring, RPZ, forward zones

#### Sprint 11 Stories

**Story 11.1: Statistics API**
- **Points**: 8
- **Tasks**:
  - Copy webserver.cc (if applicable)
  - Expose statistics via REST API
  - Add Windows-specific stats
  - Test API endpoints
- **Acceptance Criteria**:
  - REST API works
  - Statistics accurate
  - Prometheus format (optional)

**Story 11.2: RPZ Support**
- **Points**: 8
- **Tasks**:
  - Copy RPZ files
  - Load RPZ zones
  - Apply RPZ policies
  - Test RPZ filtering
- **Acceptance Criteria**:
  - RPZ zones load
  - Filtering works
  - Performance acceptable

**Story 11.3: Forward Zones**
- **Points**: 5
- **Tasks**:
  - Implement forward-zones config
  - Forward specific zones
  - Handle forward failures
  - Test forwarding
- **Acceptance Criteria**:
  - Zones forward correctly
  - Fallback works
  - Config validated

**Story 11.4: rec_control for Windows**
- **Points**: 5
- **Tasks**:
  - Implement TCP-based control
  - Add control commands
  - Test dump-cache, wipe-cache, etc.
- **Acceptance Criteria**:
  - rec_control connects
  - Commands work
  - Authentication (optional)

**Sprint 11 Total**: 26 points

---

### Sprint 12: Polish & Optimization

**Duration**: 2 weeks  
**Goal**: Performance optimization, bug fixes

#### Sprint 12 Stories

**Story 12.1: Performance Profiling**
- **Points**: 8
- **Tasks**:
  - Profile with Windows Performance Analyzer
  - Identify bottlenecks
  - Optimize hot paths
  - Benchmark improvements
- **Acceptance Criteria**:
  - Profiling complete
  - Bottlenecks identified
  - Performance improved

**Story 12.2: Memory Optimization**
- **Points**: 5
- **Tasks**:
  - Check for memory leaks
  - Optimize cache memory usage
  - Test under high load
- **Acceptance Criteria**:
  - No memory leaks
  - Memory usage acceptable
  - Stable under load

**Story 12.3: Bug Fixes**
- **Points**: 8
- **Tasks**:
  - Fix known bugs from backlog
  - Test edge cases
  - Handle error conditions
  - Update error messages
- **Acceptance Criteria**:
  - All critical bugs fixed
  - Edge cases handled
  - Error messages helpful

**Story 12.4: Documentation**
- **Points**: 5
- **Tasks**:
  - Write Windows user guide
  - Document configuration options
  - Create troubleshooting guide
  - Add examples
- **Acceptance Criteria**:
  - User guide complete
  - All options documented
  - Examples work

**Sprint 12 Total**: 26 points

**Phase 3 Total**: 75 points (3 sprints)  
**Phase 3 Deliverable**: ✅ Production-ready Windows build

---

## 5. PHASE 4: Release (Sprint 13)

### Sprint 13: Release Preparation

**Duration**: 2 weeks  
**Goal**: Package and release Windows version

#### Sprint 13 Stories

**Story 13.1: Windows Installer (MSI)**
- **Points**: 13
- **Tasks**:
  - Create WiX project
  - Package binaries and DLLs
  - Create Start Menu shortcuts
  - Add uninstaller
  - Test install/uninstall
- **Acceptance Criteria**:
  - MSI installer builds
  - Silent install works
  - Uninstaller removes everything
  - No registry orphans

**Story 13.2: Release Documentation**
- **Points**: 5
- **Tasks**:
  - Write release notes
  - Document new features
  - List known issues
  - Create changelog
- **Acceptance Criteria**:
  - Release notes complete
  - Changes documented
  - Known issues listed

**Story 13.3: Final Testing**
- **Points**: 8
- **Tasks**:
  - Full regression test suite
  - Performance benchmarks
  - Security scan
  - Fresh install test on clean Windows
- **Acceptance Criteria**:
  - All tests pass
  - Performance meets targets
  - No security issues
  - Clean install works

**Sprint 13 Total**: 26 points

**Phase 4 Total**: 26 points (1 sprint)  
**Phase 4 Deliverable**: ✅ Released v1.0 with Windows support

---

## 6. Sprint Capacity Planning

### Velocity Assumptions
- **Story Points per Sprint**: ~23-26 points
- **Team Size**: 1 developer (adjust if 2 developers)
- **Sprint Duration**: 2 weeks
- **Working Hours per Sprint**: ~60 hours (with buffer)

### Sprint Summary

| Sprint | Phase | Points | Focus | Deliverable |
|--------|-------|--------|-------|-------------|
| 1 | POC | 18 | Setup & compile | Core files compile |
| 2 | POC | 23 | I/O multiplexer | Multiplexer works |
| 3 | POC | 23 | UDP query/response | Dummy responses |
| 4 | Core | 23 | DNS resolution | SyncRes integrated |
| 5 | Core | 24 | Resolution chain | Full resolution |
| 6 | Core | 23 | Caching | Cache works |
| 7 | Core | 23 | TCP support | TCP queries work |
| 8 | Core | 24 | DNSSEC part 1 | Basic validation |
| 9 | Core | 24 | DNSSEC part 2 + config | Complete DNSSEC |
| 10 | Advanced | 23 | Windows Service | Service works |
| 11 | Advanced | 26 | Monitoring + RPZ | Features complete |
| 12 | Advanced | 26 | Polish | Production ready |
| 13 | Release | 26 | Packaging | v1.0 released |

**Total Points**: 306  
**Total Sprints**: 13  
**Total Duration**: 26 weeks

---

## 7. Risk Management

### Sprint Risks

**Risk**: Underestimated complexity
- **Impact**: Sprint overrun
- **Mitigation**: Buffer tasks in each sprint, can be deprioritized

**Risk**: Unforeseen Windows API issues
- **Impact**: Blocked progress
- **Mitigation**: Reference Unbound implementation, ask for help

**Risk**: Performance not meeting targets
- **Impact**: Unhappy users
- **Mitigation**: Sprint 12 dedicated to optimization

---

## 8. Definition of Done

**Story is done when**:
- ✅ Code is written and committed
- ✅ Code compiles without warnings
- ✅ Unit tests pass (if applicable)
- ✅ Manually tested on Windows
- ✅ Code reviewed (self-review or peer)
- ✅ Documentation updated
- ✅ No known bugs in the feature

**Sprint is done when**:
- ✅ All stories completed
- ✅ Sprint goal achieved
- ✅ Regression tests pass
- ✅ Demo prepared (if applicable)
- ✅ Backlog groomed for next sprint

---

## 9. Next Document

**Detailed Scrum Stories** (individual story cards with tasks) → Coming next

This implementation plan provides the roadmap. The next document will break down each story into detailed tasks ready for sprint planning.

---

**Document Status**: Ready for Story Breakdown  
**Next**: Detailed Scrum Stories Document

