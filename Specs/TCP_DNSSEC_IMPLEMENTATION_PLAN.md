# TCP & DNSSEC Implementation Plan
## PowerDNS Recursor - Windows Support

**Document Version**: 1.0  
**Date**: Based on verified code analysis  
**Focus**: TCP Support (Sprint 7) & DNSSEC Validation (Sprints 8-9)  
**Sprint Duration**: 2-2.5 weeks per sprint (varies by complexity)  
**Team Size**: 1 developer (with AI assistance)  

---

## Overview

This document provides detailed implementation plans for:
- **Sprint 7**: TCP Support (11-17 days / 2.5-3 weeks)
- **Sprint 8**: DNSSEC Validation - Part 1 (10-13 days / 2-2.5 weeks)
- **Sprint 9**: DNSSEC Validation - Part 2 & Configuration (9-13 days / 1.8-2.5 weeks)

**Total Estimated Time**: 30-43 days (6-8.5 weeks)

---

## Sprint 7: TCP Support

**Duration**: 2.5-3 weeks (11-17 days)  
**Goal**: Enable TCP transport for DNS queries and responses  
**Status**: Integration point already exists in `rec-main.cc:1963`

### Sprint 7 Stories

#### Story 7.1: Copy and Adapt rec-tcp.cc
- **Points**: 8
- **Estimated Time**: 2-3 days
- **Priority**: CRITICAL
- **Description**: Copy TCP server implementation from upstream and adapt for Windows

**Tasks**:
1. Copy `pdns-recursor/rec-tcp.cc` → `pdns_recursor_windows/rec-tcp.cc`
2. Review Windows-specific sections:
   - `setCloseOnExec()` - May need Windows equivalent or skip
   - Socket options: Skip Linux-only (`TCP_DEFER_ACCEPT`, `SO_REUSEPORT`)
   - Verify `TCP_FASTOPEN` availability on Windows
3. Add `#ifdef _WIN32` guards where needed
4. Verify all includes compile on Windows
5. Document any skipped features

**Key Functions to Copy** (from `pdns-recursor/rec-tcp.cc`):
- `makeTCPServerSockets()` - Line 1097 (non-static, exported)
- `handleNewTCPQuestion()` - Line 696 (non-static, exported)
- `handleRunningTCPQuestion()` - Line 519 (static, internal)
- `doProcessTCPQuestion()` - Line 292 (static, internal)
- `finishTCPReply()` - Line 160 (non-static, exported)
- `sendErrorOverTCP()` - Line 126 (static, internal)

**Integration Points**:
- `rec-main.cc::initDistribution()` already calls `makeTCPServerSockets()` at line 1963
- Multiplexer registration at line 1200 in `rec-tcp.cc`
- Uses same `deferredAdds` mechanism as UDP

**Acceptance Criteria**:
- ✅ `rec-tcp.cc` copied and compiles
- ✅ Windows-specific sections identified
- ✅ Linux-only code commented/guarded
- ✅ No compilation errors
- ✅ Integration point verified

**Time Breakdown**:
- Copy file: 0.5-1 hours
- Review and identify Windows issues: 4-8 hours
- Add Windows guards: 2-4 hours
- Fix compilation errors: 4-10 hours
- Documentation: 1-3 hours
- **Total**: 2-3 days

---

#### Story 7.2: Fix Windows Socket Compatibility
- **Points**: 5
- **Estimated Time**: 1 day
- **Priority**: HIGH
- **Description**: Fix Windows-specific socket operations in TCP code

**Tasks**:
1. Review `makeTCPServerSockets()` (line 1097):
   - Socket creation (line 1116, 1120) - Should work with Winsock2
   - `setNonBlocking()` - Verify Windows version works
   - `setTCPNoDelay()` - Verify Windows version works
   - `setCloseOnExec()` - Skip or implement Windows equivalent
2. Review `handleNewTCPQuestion()` (line 696):
   - `accept()` - Works with Winsock2
   - `setNonBlocking()` - Verify works
   - `setTCPNoDelay()` - Verify works
3. Skip Linux-only socket options:
   - `TCP_DEFER_ACCEPT` - Not available on Windows
   - `SO_REUSEPORT` - Not available on Windows (use `SO_REUSEADDR`)
   - `TCP_FASTOPEN` - Check Windows 10+ support
4. Test socket creation and binding

**Acceptance Criteria**:
- ✅ TCP sockets create successfully
- ✅ Sockets bind to ports
- ✅ Non-blocking mode works
- ✅ TCP_NODELAY works
- ✅ No Linux-only options used

**Time Breakdown**:
- Review socket operations: 2-4 hours
- Fix Windows compatibility: 3-6 hours
- Testing: 2-4 hours
- Debug issues: 2-4 hours
- **Total**: 1-2 days

---

#### Story 7.3: Verify Header Declarations
- **Points**: 3
- **Estimated Time**: 1 day
- **Priority**: HIGH
- **Description**: Ensure all TCP function declarations exist in headers

**Tasks**:
1. Verify `rec-main.hh` has required declarations:
   - `makeTCPServerSockets()` - Line 649 (upstream) / 671 (Windows)
   - `handleNewTCPQuestion()` - Line 650 (upstream) / 672 (Windows)
   - `finishTCPReply()` - Line 646 (upstream)
   - `sendResponseOverTCP()` - Line 300 (upstream) / 322 (Windows) - Template function
2. Verify `Utility::writev()` declaration:
   - `utility.hh` - Line 121 (upstream) / 156 (Windows)
3. Add missing declarations if needed
4. Verify compilation with headers

**Acceptance Criteria**:
- ✅ All function declarations exist
- ✅ Function signatures match implementations
- ✅ Template functions properly declared
- ✅ No linker errors

**Time Breakdown**:
- Check declarations: 1-3 hours
- Add missing declarations: 2-4 hours
- Verify compilation: 1-2 hours
- Fix any issues: 1-2 hours
- **Total**: 1 day

---

#### Story 7.4: Fix Build Errors
- **Points**: 5
- **Estimated Time**: 1-2 days
- **Priority**: HIGH
- **Description**: Fix all compilation and linking errors

**Tasks**:
1. Compile `rec-tcp.cc` with rest of codebase
2. Fix include path issues
3. Fix missing dependency errors
4. Fix type mismatches
5. Fix Windows-specific compilation issues
6. Document all fixes

**Acceptance Criteria**:
- ✅ Code compiles without errors
- ✅ No linker errors
- ✅ All dependencies resolved
- ✅ Warnings addressed (or documented)

**Time Breakdown**:
- Initial compilation: 1-3 hours
- Fix errors iteratively: 6-12 hours
- Testing compilation: 1-2 hours
- Debug complex issues: 2-4 hours
- **Total**: 1-2 days

---

#### Story 7.5: Basic TCP Socket Testing
- **Points**: 3
- **Estimated Time**: 1-2 days
- **Priority**: MEDIUM
- **Description**: Test TCP socket creation and basic connection acceptance

**Tasks**:
1. Test `makeTCPServerSockets()`:
   - Create TCP listening socket
   - Bind to port
   - Verify socket is listening
2. Test `handleNewTCPQuestion()`:
   - Accept incoming connection
   - Verify non-blocking mode
   - Verify TCP_NODELAY set
   - Create TCPConnection object
3. Test connection registration with multiplexer
4. Test connection cleanup

**Acceptance Criteria**:
- ✅ TCP socket creates and binds
- ✅ Connections are accepted
- ✅ Sockets registered with multiplexer
- ✅ No memory leaks
- ✅ Connections close properly

**Time Breakdown**:
- Write test code: 2-4 hours
- Test socket creation: 2-4 hours
- Test connection acceptance: 3-5 hours
- Debug issues: 3-6 hours
- **Total**: 1-2 days

---

#### Story 7.6: TCP Query Processing Testing
- **Points**: 5
- **Estimated Time**: 1-2 days
- **Priority**: MEDIUM
- **Description**: Test full TCP query/response cycle

**Tasks**:
1. Test `handleRunningTCPQuestion()`:
   - Read 2-byte length prefix
   - Read DNS query packet
   - Verify parsing works
2. Test `doProcessTCPQuestion()`:
   - Process DNS query
   - Verify cache check works
   - Verify `startDoResolve()` is called
3. Test `finishTCPReply()`:
   - Send response with length prefix
   - Verify connection lifecycle
4. Test error handling:
   - Invalid queries
   - Connection timeouts
   - Partial reads

**Acceptance Criteria**:
- ✅ TCP queries are parsed correctly
- ✅ Responses are sent with length prefix
- ✅ Cache integration works
- ✅ Error handling works
- ✅ No crashes on invalid input

**Time Breakdown**:
- Write test queries: 2-4 hours
- Test query processing: 4-6 hours
- Test response sending: 3-5 hours
- Test error cases: 2-4 hours
- Debug issues: 3-8 hours
- **Total**: 2-3 days

---

#### Story 7.7: Integration Testing
- **Points**: 5
- **Estimated Time**: 2-3 days
- **Priority**: HIGH
- **Description**: End-to-end TCP testing in main flow

**Tasks**:
1. Test TCP in main recursor flow:
   - Verify `initDistribution()` creates TCP sockets
   - Verify TCP worker threads work
   - Test with real DNS client (dig +tcp)
2. Test large responses (>512 bytes):
   - Verify TCP handles large packets
   - Test truncation fallback (UDP → TCP)
3. Test multiple queries per connection:
   - Verify connection reuse
   - Test connection timeout
4. Test concurrent connections:
   - Multiple clients simultaneously
   - Connection limits
5. Test TCP fallback for truncated UDP:
   - Verify `syncres.cc:5991-5994` logic works
   - Test automatic TCP retry

**Acceptance Criteria**:
- ✅ TCP works in main flow
- ✅ Large responses work
- ✅ Multiple queries per connection work
- ✅ Concurrent connections handled
- ✅ UDP → TCP fallback works
- ✅ No memory leaks
- ✅ Performance acceptable

**Time Breakdown**:
- Main flow integration: 4-8 hours
- Large response testing: 3-5 hours
- Multiple queries testing: 3-5 hours
- Concurrent connection testing: 4-6 hours
- TCP fallback testing: 3-5 hours
- Debug and fix issues: 6-12 hours
- **Total**: 3-4 days

---

### Sprint 7 Summary

| Story | Points | Time Estimate | Priority |
|-------|--------|---------------|----------|
| 7.1: Copy rec-tcp.cc | 8 | 2-3 days | CRITICAL |
| 7.2: Windows Socket Compatibility | 5 | 1-2 days | HIGH |
| 7.3: Verify Headers | 3 | 1 day | HIGH |
| 7.4: Fix Build Errors | 5 | 1-2 days | HIGH |
| 7.5: Basic Socket Testing | 3 | 1-2 days | MEDIUM |
| 7.6: Query Processing Testing | 5 | 2-3 days | MEDIUM |
| 7.7: Integration Testing | 5 | 3-4 days | HIGH |
| **Total** | **34** | **11-17 days** | |

**Sprint 7 Total**: 11-17 days (2.5-3 weeks)  
**Note**: Can be completed in 2.5-3 weeks with focused effort

---

## Sprint 8: DNSSEC Validation - Part 1

**Duration**: 2-2.5 weeks (10-13 days)  
**Goal**: Basic DNSSEC validation with minimal Linux dependencies  
**Status**: Most code is portable (C++ + OpenSSL), only 2 small Linux dependencies found

### Sprint 8 Stories

#### Story 8.1: Copy DNSSEC Files
- **Points**: 5
- **Estimated Time**: 1-2 days
- **Priority**: CRITICAL
- **Description**: Copy DNSSEC validation files from upstream

**Tasks**:
1. Copy missing DNSSEC files:
   - `pdns-recursor/opensslsigners.cc` → `pdns_recursor_windows/opensslsigners.cc`
   - `pdns-recursor/opensslsigners.hh` → `pdns_recursor_windows/opensslsigners.hh`
   - `pdns-recursor/sodiumsigners.cc` → `pdns_recursor_windows/sodiumsigners.cc` (if libsodium enabled)
   - `pdns-recursor/nsecrecords.cc` → `pdns_recursor_windows/nsecrecords.cc`
2. Verify existing files are up to date:
   - `validate-recursor.cc/hh` - Already exists
   - `validate.cc/hh` - Already exists
   - `dnssecinfra.cc/hh` - Already exists
3. Add files to CMakeLists.txt
4. Attempt initial compilation

**Acceptance Criteria**:
- ✅ All DNSSEC files copied
- ✅ Files added to build system
- ✅ Initial compilation attempted
- ✅ Errors documented

**Time Breakdown**:
- Copy files: 1-3 hours
- Update CMakeLists.txt: 1-2 hours
- Initial compilation: 2-4 hours
- Document errors: 1-3 hours
- Fix initial issues: 2-4 hours
- **Total**: 1-2 days

---

#### Story 8.2: Fix OpenSSL Threading (if needed)
- **Points**: 3
- **Estimated Time**: 1-2 day
- **Priority**: MEDIUM (only if OpenSSL < 1.1.0)
- **Description**: Fix OpenSSL threading callback for Windows

**Issue Found**:
- `opensslsigners.cc:71-73` uses `pthread_self()` for OpenSSL < 1.1.0
- OpenSSL 1.1.0+ is thread-safe (no callback needed)

**Tasks**:
1. Check OpenSSL version:
   - If OpenSSL >= 1.1.0: Skip this story (thread-safe by default)
   - If OpenSSL < 1.1.0: Continue
2. If needed, fix `openssl_pthreads_id_callback()`:
   - Option A: Use `pthreads-win32` library
   - Option B: Use `GetCurrentThreadId()` with wrapper
3. Test OpenSSL initialization
4. Verify threading works correctly

**Acceptance Criteria**:
- ✅ OpenSSL version checked
- ✅ Threading callback works (if needed)
- ✅ OpenSSL initializes correctly
- ✅ No threading issues

**Time Breakdown**:
- Check OpenSSL version: 0.5-1 hour
- If needed, implement fix: 3-6 hours
- Testing: 2-4 hours
- Debug issues: 1-3 hours
- **Total**: 1-2 days (only if OpenSSL < 1.1.0)

**Note**: Modern OpenSSL (1.1.0+) doesn't need this, so this story may be skipped.

---

#### Story 8.3: Fix ZoneParserTNG File I/O
- **Points**: 3
- **Estimated Time**: 1-2 day
- **Priority**: HIGH
- **Description**: Fix POSIX file I/O in trust anchor loading

**Issue Found**:
- `zoneparser-tng.cc:63,71,73,82` uses POSIX `open()`, `fstat()`, `close()`, `fdopen()`
- Windows has equivalents: `_open()`, `_fstat()`, `_close()`, `_fdopen()`

**Tasks**:
1. Review `zoneparser-tng.cc::stackFile()`:
   - Line 63: `open()` → Use `_open()` or C++ `std::ifstream`
   - Line 71: `fstat()` → Use `_fstat()` or `std::filesystem`
   - Line 73: `close()` → Use `_close()` or let `std::ifstream` handle
   - Line 82: `fdopen()` → Use `_fdopen()` or `std::ifstream`
2. Add `#ifdef _WIN32` blocks:
   - Use Windows equivalents OR
   - Refactor to use C++ `std::ifstream` (more portable)
3. Test trust anchor file loading
4. Verify error handling works

**Acceptance Criteria**:
- ✅ Trust anchor files load on Windows
- ✅ File I/O works correctly
- ✅ Error handling works
- ✅ No POSIX dependencies

**Time Breakdown**:
- Review file I/O code: 2-3 hours
- Implement Windows version: 4-6 hours
- Test file loading: 2-4 hours
- Debug issues: 3-5 hours
- **Total**: 1-2 days

---

#### Story 8.4: Remove Stubs and Enable DNSSEC
- **Points**: 3
- **Estimated Time**: 1-2 days
- **Priority**: CRITICAL
- **Description**: Enable DNSSEC compilation and remove stub implementations

**Tasks**:
1. Set `HAVE_DNSSEC=1` in CMakeLists.txt or config
2. Remove or disable `dnssec_stubs.cc`:
   - File contains stubs for `HAVE_DNSSEC=0`
   - Real implementations exist in `validate-recursor.cc`, `validate.cc`
3. Verify `initDNSSEC()` is called:
   - `rec-main.cc:2221` - Already exists
4. Test DNSSEC initialization:
   - Verify `g_dnssecmode` is set
   - Verify trust anchor loading works
5. Fix any compilation errors from enabling DNSSEC

**Acceptance Criteria**:
- ✅ `HAVE_DNSSEC=1` set
- ✅ Stubs removed/disabled
- ✅ DNSSEC initializes correctly
- ✅ No compilation errors
- ✅ Trust anchors can be loaded

**Time Breakdown**:
- Enable DNSSEC flag: 0.5-1 hour
- Remove stubs: 1-2 hours
- Fix compilation errors: 4-8 hours
- Test initialization: 2-4 hours
- Debug issues: 2-4 hours
- **Total**: 1-2 days

---

#### Story 8.5: Fix Compilation Errors
- **Points**: 5
- **Estimated Time**: 1.5-2 days
- **Priority**: HIGH
- **Description**: Fix all DNSSEC-related compilation errors

**Tasks**:
1. Compile with DNSSEC enabled
2. Fix missing includes
3. Fix type mismatches
4. Fix missing function implementations
5. Fix Windows-specific issues
6. Document all fixes

**Expected Issues**:
- Missing OpenSSL headers
- Missing libsodium (if enabled)
- Type mismatches
- Missing utility functions

**Acceptance Criteria**:
- ✅ Code compiles without errors
- ✅ No linker errors
- ✅ All dependencies resolved
- ✅ Warnings addressed

**Time Breakdown**:
- Initial compilation: 1-3 hours
- Fix errors iteratively: 6-10 hours
- Test compilation: 1-2 hours
- Documentation: 1-2 hours
- Debug complex issues: 2-3 hours
- **Total**: 1.5-2 days

---

#### Story 8.6: Trust Anchor Configuration
- **Points**: 3
- **Estimated Time**: 1 day
- **Priority**: MEDIUM
- **Description**: Load and verify root trust anchor

**Tasks**:
1. Test `updateTrustAnchorsFromFile()`:
   - `validate-recursor.cc:45` - Already implemented
   - Test loading root trust anchor file
2. Verify trust anchor format:
   - DS records
   - DNSKEY records
3. Test trust anchor loading in `initDNSSEC()`:
   - `rec-main.cc:3650` - Already calls `updateTrustAnchorsFromFile()`
4. Verify trust anchors are stored correctly

**Acceptance Criteria**:
- ✅ Root trust anchor loads
- ✅ Trust anchor format validated
- ✅ Can validate against trust anchor
- ✅ Trust anchors persist in memory

**Time Breakdown**:
- Test file loading: 2-3 hours
- Verify format: 1-2 hours
- Test integration: 2-3 hours
- Debug issues: 1-2 hours
- **Total**: 1 day

---

#### Story 8.7: Basic Signature Validation
- **Points**: 8
- **Estimated Time**: 2-3 days
- **Priority**: HIGH
- **Description**: Implement basic RRSIG validation and chain of trust

**Tasks**:
1. Test RRSIG validation:
   - `validate.cc` - Core validation logic
   - `validateWithKeySet()` - Signature verification
   - Test with DNSSEC-signed domains
2. Test DS record validation:
   - Verify DS → DNSKEY chain
   - Test DS digest algorithms
3. Test chain of trust:
   - Root → TLD → Domain
   - Verify each step
4. Test AD flag setting:
   - Set AD=1 when validation succeeds
   - Verify flag in response

**Key Functions** (already implemented):
- `SyncRes::validateRecordsWithSigs()` - `syncres.cc:3956`
- `SyncRes::validateDNSKeys()` - `syncres.cc:3845`
- `SyncRes::getDNSKeys()` - `syncres.cc:3916`

**Acceptance Criteria**:
- ✅ RRSIG validation works
- ✅ DS validation works
- ✅ Chain of trust verified
- ✅ AD flag set correctly
- ✅ Valid signatures accepted
- ✅ Invalid signatures rejected

**Time Breakdown**:
- Test RRSIG validation: 4-6 hours
- Test DS validation: 3-5 hours
- Test chain of trust: 4-6 hours
- Test AD flag: 2-4 hours
- Debug issues: 4-8 hours
- **Total**: 2-3 days

---

#### Story 8.8: DNSSEC Testing
- **Points**: 3
- **Estimated Time**: 1-2 days
- **Priority**: MEDIUM
- **Description**: Test DNSSEC with various scenarios

**Tasks**:
1. Test with DNSSEC-signed domains:
   - `dnssec.works` (test domain)
   - `isc.org` (real DNSSEC domain)
   - Verify validation succeeds
2. Test with invalid signatures:
   - Modified signatures
   - Expired signatures
   - Verify validation fails
3. Test with missing DS records:
   - Domains without DS
   - Verify Insecure state
4. Test error handling:
   - SERVFAIL on validation failure
   - Logging of validation failures

**Acceptance Criteria**:
- ✅ Valid signatures accepted
- ✅ Invalid signatures rejected
- ✅ Missing DS handled correctly
- ✅ Errors logged appropriately
- ✅ No crashes on errors

**Time Breakdown**:
- Test valid signatures: 3-5 hours
- Test invalid signatures: 3-5 hours
- Test missing DS: 2-3 hours
- Test error handling: 2-4 hours
- Debug issues: 2-4 hours
- **Total**: 1-2 days

---

### Sprint 8 Summary

| Story | Points | Time Estimate | Priority |
|-------|--------|---------------|----------|
| 8.1: Copy DNSSEC Files | 5 | 1-2 days | CRITICAL |
| 8.2: Fix OpenSSL Threading | 3 | 1-2 days* | MEDIUM |
| 8.3: Fix ZoneParserTNG I/O | 3 | 1-2 days | HIGH |
| 8.4: Enable DNSSEC | 3 | 1-2 days | CRITICAL |
| 8.5: Fix Compilation Errors | 5 | 1.5-2 days | HIGH |
| 8.6: Trust Anchor Config | 3 | 1 day | MEDIUM |
| 8.7: Basic Signature Validation | 8 | 2-3 days | HIGH |
| 8.8: DNSSEC Testing | 3 | 1-2 days | MEDIUM |
| **Total** | **33** | **10-13 days** | |

*Only if OpenSSL < 1.1.0 (modern OpenSSL doesn't need this)

**Sprint 8 Total**: 10-13 days (2-2.5 weeks)  
**Note**: Can be completed in 2-2.5 weeks with focused effort

---

## Sprint 9: DNSSEC Validation - Part 2 & Configuration

**Duration**: 2-2.5 weeks (8-12 days)  
**Goal**: Complete DNSSEC features and add configuration support

### Sprint 9 Stories

#### Story 9.1: Advanced DNSSEC Features
- **Points**: 8
- **Estimated Time**: 2-3 days
- **Priority**: MEDIUM
- **Description**: Implement NSEC/NSEC3 validation and advanced features

**Tasks**:
1. Test NSEC validation:
   - `validate.cc` - NSEC validation logic exists
   - Test negative responses with NSEC
   - Verify NSEC proofs work
2. Test NSEC3 validation:
   - NSEC3 hash validation
   - NSEC3 iteration limits
   - Verify NSEC3 proofs work
3. Test all signature algorithms:
   - RSA (RSASHA1, RSASHA256, RSASHA512)
   - ECDSA (ECDSA256, ECDSA384)
   - Ed25519, Ed448 (if supported)
4. Implement negative trust anchors:
   - `rec-main.cc:3655` - Already has negative anchor support
   - Test negative anchor loading
5. Add DNSSEC statistics:
   - Validation counters
   - Algorithm usage stats
   - Bogus response counts

**Acceptance Criteria**:
- ✅ NSEC validation works
- ✅ NSEC3 validation works
- ✅ All algorithms supported
- ✅ Negative trust anchors work
- ✅ Statistics tracked

**Time Breakdown**:
- Test NSEC: 4-6 hours
- Test NSEC3: 4-6 hours
- Test algorithms: 3-5 hours
- Negative anchors: 3-5 hours
- Statistics: 3-5 hours
- Debug issues: 6-10 hours
- **Total**: 3-4 days

---

#### Story 9.2: Configuration File Parsing
- **Points**: 5
- **Estimated Time**: 1-2 days
- **Priority**: MEDIUM
- **Description**: Add YAML configuration file support

**Tasks**:
1. Copy config file parsing code:
   - `rec-lua-conf.cc` - Already exists (Lua/YAML config)
   - Verify YAML parsing works on Windows
2. Add Windows default paths:
   - Config file location (e.g., `%PROGRAMDATA%\PowerDNS\recursor.conf`)
   - Trust anchor file location
3. Test config loading:
   - Parse DNSSEC settings
   - Parse trust anchor settings
   - Parse negative trust anchor settings
4. Test config validation:
   - Invalid settings rejected
   - Defaults applied correctly

**Acceptance Criteria**:
- ✅ Config file parsed
- ✅ Windows paths work
- ✅ Settings applied correctly
- ✅ Errors reported clearly

**Time Breakdown**:
- Review config code: 2-4 hours
- Add Windows paths: 3-5 hours
- Test config loading: 3-5 hours
- Test validation: 2-4 hours
- Debug issues: 3-6 hours
- **Total**: 2-3 days

---

#### Story 9.3: Command-Line Options
- **Points**: 3
- **Estimated Time**: 1-2 days
- **Priority**: LOW
- **Description**: Add DNSSEC-related command-line options

**Tasks**:
1. Verify `::arg()` system works:
   - `arguments.cc/hh` - Already exists
   - Test argument parsing on Windows
2. Add DNSSEC CLI options:
   - `--dnssec=off|process|validate|log-fail`
   - `--trustanchorfile=<file>`
   - `--dnssec-log-bogus`
   - `--signature-inception-skew`
3. Test option parsing:
   - Verify options are read
   - Verify defaults work
   - Test option validation
4. Test option override:
   - CLI overrides config file
   - Help text correct

**Acceptance Criteria**:
- ✅ CLI options work
- ✅ Override config file
- ✅ Help text correct
- ✅ Options validated

**Time Breakdown**:
- Review arg() system: 2-3 hours
- Add DNSSEC options: 3-5 hours
- Test parsing: 2-4 hours
- Test override: 1-2 hours
- Debug issues: 1-3 hours
- **Total**: 1-2 days

---

#### Story 9.4: Integration Testing
- **Points**: 5
- **Estimated Time**: 2-3 days
- **Priority**: HIGH
- **Description**: End-to-end DNSSEC testing

**Tasks**:
1. Test DNSSEC in main flow:
   - Verify DNSSEC mode is set
   - Test with real DNSSEC domains
   - Verify validation happens
2. Test all DNSSEC modes:
   - `off` - No DNSSEC processing
   - `process` - Process but don't validate
   - `validate` - Validate and SERVFAIL on failure
   - `log-fail` - Validate but only log failures
3. Test performance:
   - Validation overhead
   - Cache impact
   - Query latency
4. Test edge cases:
   - Expired signatures
   - Future signatures
   - Algorithm rollover
   - Chain breaks
5. Regression testing:
   - Verify UDP still works
   - Verify TCP still works
   - Verify caching still works

**Acceptance Criteria**:
- ✅ DNSSEC works in main flow
- ✅ All modes work correctly
- ✅ Performance acceptable
- ✅ Edge cases handled
- ✅ No regressions

**Time Breakdown**:
- Main flow testing: 4-6 hours
- Mode testing: 3-5 hours
- Performance testing: 3-5 hours
- Edge case testing: 4-6 hours
- Regression testing: 3-5 hours
- Debug issues: 6-12 hours
- **Total**: 3-4 days

---

### Sprint 9 Summary

| Story | Points | Time Estimate | Priority |
|-------|--------|---------------|----------|
| 9.1: Advanced DNSSEC | 8 | 3-4 days | MEDIUM |
| 9.2: Config File Parsing | 5 | 2-3 days | MEDIUM |
| 9.3: CLI Options | 3 | 1-2 days | LOW |
| 9.4: Integration Testing | 5 | 3-4 days | HIGH |
| **Total** | **21** | **9-13 days** | |

**Sprint 9 Total**: 9-13 days (1.8-2.5 weeks)  
**Note**: Can be completed in 2-2.5 weeks with focused effort

---

## Overall Summary

### Time Estimates by Sprint

| Sprint | Focus | Total Time | Weeks |
|--------|-------|------------|-------|
| **Sprint 7** | TCP Support | 11-17 days | 2.5-3 weeks |
| **Sprint 8** | DNSSEC Part 1 | 10-13 days | 2-2.5 weeks |
| **Sprint 9** | DNSSEC Part 2 | 9-13 days | 1.8-2.5 weeks |
| **Total** | **TCP + DNSSEC** | **30-43 days** | **6-8.5 weeks** |

### Risk Factors

**Low Risk**:
- ✅ TCP integration point already exists
- ✅ DNSSEC code is mostly portable
- ✅ Most functions already implemented

**Medium Risk**:
- ⚠️ Windows socket compatibility issues
- ⚠️ OpenSSL version differences
- ⚠️ File I/O portability

**Mitigation**:
- Reference Unbound Windows implementation
- Test with multiple OpenSSL versions
- Use C++ standard library where possible

### Dependencies

**External Dependencies**:
- OpenSSL (cross-platform, already used)
- libevent (already integrated for Windows)
- libsodium (optional, for Ed25519/Ed448)

**Internal Dependencies**:
- UDP flow must be working (Sprint 3-5)
- Caching must be working (Sprint 6)
- MTasker must be working (Sprint 2)

### Success Criteria

**Sprint 7 Complete When**:
- ✅ TCP queries work end-to-end
- ✅ Large responses work
- ✅ UDP → TCP fallback works
- ✅ No memory leaks

**Sprint 8 Complete When**:
- ✅ Basic DNSSEC validation works
- ✅ Trust anchors load
- ✅ AD flag set correctly
- ✅ Invalid signatures rejected

**Sprint 9 Complete When**:
- ✅ All DNSSEC features work
- ✅ Configuration works
- ✅ Performance acceptable
- ✅ No regressions

---

## Notes

1. **Time estimates are conservative** - Actual time may be less with focused effort
2. **AI assistance impact** - AI can help with code analysis, understanding integration points, and providing implementation guidance, which may speed up analysis and planning phases. However, core development work (coding, testing, debugging, integration) still requires the developer's time, so estimates remain valid.
3. **Stories can be done in parallel** - Some stories are independent
4. **Testing can overlap** - Integration testing can start while features are being developed
5. **OpenSSL version matters** - Modern OpenSSL (1.1.0+) simplifies threading
6. **File I/O is simple** - C++ standard library provides portable alternatives

---

**Document Status**: Ready for Sprint Planning  
**Based on**: Verified code analysis from TCP and DNSSEC implementation plans

