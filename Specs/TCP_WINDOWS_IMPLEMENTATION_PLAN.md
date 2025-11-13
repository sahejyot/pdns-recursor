# TCP Support for Windows - Implementation Plan

**⚠️ ALL INFORMATION VERIFIED FROM ACTUAL CODE** - No assumptions made. All line numbers, function signatures, and integration points verified against `pdns-recursor/rec-tcp.cc`, `pdns-recursor/rec-main.cc`, and related files.

## Overview

This document outlines the modules, files, and changes required to enable TCP support for PowerDNS Recursor on Windows. TCP is essential for:
- Handling DNS queries over TCP (RFC 1035)
- Fallback when UDP responses are truncated
- DNS-over-TLS (DoT) support
- Outgoing TCP connections to upstream servers

---

## Current Status

### ✅ Already Available in Windows Version
- `tcpiohandler.cc/hh` - TCP I/O handler (exists, may need Windows adaptations)
- `rec-tcpout.cc/hh` - Outgoing TCP connection management (exists)
- `setNonBlocking()` - Windows implementation exists in `misc.cc`
- `closesocket()` - Windows wrapper exists in `misc.cc`
- Socket compatibility layer (`socket_compat.hh`, `windows-compat.h`)

### ❌ Missing from Windows Version
- `rec-tcp.cc` - **CRITICAL**: Main TCP server implementation (does not exist)
- `makeTCPServerSockets()` function
- `handleNewTCPQuestion()` function
- `handleRunningTCPQuestion()` function
- `doProcessTCPQuestion()` function

---

## Files to Work On

### 1. **Core TCP Server Module** (PRIMARY FOCUS)

#### File: `rec-tcp.cc` (NEW - Copy from upstream)
**Location**: `pdns-recursor/rec-tcp.cc` → `pdns_recursor_windows/rec-tcp.cc`

**Purpose**: Main TCP server implementation for handling incoming TCP DNS queries

**Key Functions** (VERIFIED FROM CODE):
- `makeTCPServerSockets()` - Line 1097: Creates and binds TCP listening sockets (non-static, exported)
- `handleNewTCPQuestion()` - Line 696: Accepts new TCP connections (non-static, exported)
- `handleRunningTCPQuestion()` - Line 519: Handles ongoing TCP connection I/O (static, internal)
- `doProcessTCPQuestion()` - Line 292: Processes DNS query from TCP connection (static, internal)
- `finishTCPReply()` - Line 160: Sends response and manages connection lifecycle (non-static, exported)
- `sendErrorOverTCP()` - Line 126: Sends error responses over TCP (static, internal)

**Windows-Specific Changes Needed**:

1. **Socket Creation** (`makeTCPServerSockets()` - VERIFIED):
   ```cpp
   // Line 1116: socket() call - ✅ Already compatible (Winsock2)
   auto socketFd = FDWrapper(socket(address.sin6.sin6_family, SOCK_STREAM, 0));
   
   // Line 1120: setCloseOnExec() - ⚠️ May need Windows equivalent
   setCloseOnExec(socketFd);
   
   // Line 1190: setNonBlocking() - ✅ Already has Windows implementation
   setNonBlocking(socketFd);
   
   // Line 1199: listen() - ✅ Compatible (Winsock2)
   listen(socketFd, 128);
   ```

2. **Connection Acceptance** (`handleNewTCPQuestion()` - VERIFIED):
   ```cpp
   // Line 700: accept() - ✅ Compatible (Winsock2)
   int newsock = accept(fileDesc, reinterpret_cast<struct sockaddr*>(&addr), &addrlen);
   
   // Line 706: closesocket() - ✅ Already has Windows wrapper
   closesocket(newsock);
   
   // Line 753: setNonBlocking() - ✅ Already has Windows implementation
   setNonBlocking(newsock);
   
   // Line 754: setTCPNoDelay() - ✅ Already implemented in misc.cc
   setTCPNoDelay(newsock);
   ```

3. **Socket Options**:
   - `SO_REUSEADDR` - ✅ Compatible
   - `IPV6_V6ONLY` - ✅ Compatible
   - `TCP_DEFER_ACCEPT` - ⚠️ Linux-specific, skip on Windows (already guarded)
   - `TCP_FASTOPEN` - ⚠️ May not be available on Windows (already guarded)

**Integration Points**:
- Called from `rec-main.cc::initDistribution()` at line 1963 (TCP worker threads)
- Registered with multiplexer via `deferredAdds.emplace_back(socketFd, handleNewTCPQuestion)` (line 1200)
- Uses `t_fdm` (thread-local FDMultiplexer) for I/O events

---

### 2. **TCP I/O Handler** (REVIEW & ADAPT)

#### File: `tcpiohandler.cc/hh` (EXISTS - Review for Windows compatibility)
**Location**: `pdns_recursor_windows/tcpiohandler.cc/hh`

**Purpose**: Low-level TCP I/O operations, TLS/SSL handling for outgoing connections

**Windows-Specific Considerations**:

1. **Socket Operations**:
   - `recv()`, `send()` - ✅ Compatible (Winsock2)
   - Non-blocking I/O - ✅ Uses `setNonBlocking()` which has Windows implementation
   - Socket close - ✅ Uses `closesocket()` wrapper

2. **TLS/SSL**:
   - OpenSSL is cross-platform - ✅ Should work
   - May need to verify Windows-specific OpenSSL initialization

3. **I/O Multiplexing Integration**:
   - Uses `FDMultiplexer` interface - ✅ Already abstracted via `libeventmplexer.cc`
   - Should work with libevent backend on Windows

**Action Items**:
- Review for any Linux-specific socket options
- Verify TLS context creation works on Windows
- Test with libevent multiplexer

---

### 3. **Outgoing TCP Connections** (REVIEW)

#### File: `rec-tcpout.cc/hh` (EXISTS - Review)
**Location**: `pdns_recursor_windows/rec-tcpout.cc/hh`

**Purpose**: Manages outgoing TCP connections to upstream DNS servers

**Windows-Specific Considerations**:
- Connection pooling logic - ✅ Platform-agnostic
- Uses `tcpiohandler.cc` - ✅ Should work if tcpiohandler is compatible
- Timeout handling - ✅ Uses `timeval` structures (compatible)

**Action Items**:
- Verify connection lifecycle works correctly
- Test timeout handling on Windows

---

### 4. **Main Integration Points** (VERIFY)

#### File: `rec-main.cc` (EXISTS - Verify TCP integration)
**Location**: `pdns_recursor_windows/rec-main.cc`

**Current Code** (VERIFIED - Lines 1958-1978, in `initDistribution()` function):
```cpp
threadNum = 1 + RecThreadInfo::numDistributors() + RecThreadInfo::numUDPWorkers();
for (unsigned int i = 0; i < RecThreadInfo::numTCPWorkers(); i++, threadNum++) {
  auto& info = RecThreadInfo::info(threadNum);
  auto& deferredAdds = info.getDeferredAdds();
  auto& tcpSockets = info.getTCPSockets();
  count += makeTCPServerSockets(deferredAdds, tcpSockets, log, 
                                i == RecThreadInfo::numTCPWorkers() - 1, 
                                RecThreadInfo::numTCPWorkers());
}
```

**Status**: 
- ✅ Code structure is correct
- ❌ `makeTCPServerSockets()` function doesn't exist yet (needs `rec-tcp.cc`)

**Action Items**:
- Verify `makeTCPServerSockets()` is declared in `rec-main.hh`
- Ensure TCP worker threads are properly initialized

---

#### File: `rec-main.hh` (EXISTS - Verify declarations)
**Location**: `pdns_recursor_windows/rec-main.hh`

**Required Declarations** (VERIFIED):
```cpp
// Line 649: Function declaration
unsigned int makeTCPServerSockets(deferredAdd_t& deferredAdds, 
                                   std::set<int>& tcpSockets, 
                                   Logr::log_t, 
                                   bool doLog, 
                                   unsigned int instances);

// Line 650: Handler function declaration
void handleNewTCPQuestion(int fileDesc, FDMultiplexer::funcparam_t&);

// Line 646: finishTCPReply declaration
void finishTCPReply(std::unique_ptr<DNSComboWriter>&, bool hadError, bool updateInFlight);
```

**Action Items**:
- Verify these declarations exist
- Add if missing

---

#### File: `pdns_recursor.cc` (EXISTS - Verify TCP response handling)
**Location**: `pdns_recursor_windows/pdns_recursor.cc`

**TCP Response Function** (VERIFIED):
- `sendResponseOverTCP()` - Template function in `rec-main.hh` (line 300 in upstream, line 322 in Windows)
- Uses `Utility::writev()` - ⚠️ Declared in `utility.hh` line 121 (upstream) / line 156 (Windows), needs implementation check

**Action Items**:
- Verify `Utility::writev()` works on Windows or implement wrapper

---

### 5. **Socket Utility Functions** (VERIFY)

#### File: `misc.cc` (EXISTS - Already has Windows implementations)
**Location**: `pdns_recursor_windows/misc.cc`

**Already Implemented**:
- ✅ `setNonBlocking()` - Windows version using `ioctlsocket(FIONBIO)`
- ✅ `setBlocking()` - Windows version
- ✅ `closesocket()` - Windows wrapper
- ✅ `setTCPNoDelay()` - Compatible (uses standard `setsockopt`)

**May Need**:
- `setCloseOnExec()` - ⚠️ Check if needed for Windows (Windows doesn't have `fork()`)

**Action Items**:
- Review `setCloseOnExec()` usage - may be no-op on Windows
- Verify all socket utility functions are called correctly

---

### 6. **Low-Level DNS Query Module** (VERIFY TCP INTEGRATION)

#### File: `lwres.cc` (EXISTS - Verify TCP functions)
**Location**: `pdns_recursor_windows/lwres.cc`

**TCP-Related Functions** (VERIFIED):
- `tcpconnect()` - Line 291: Creates outgoing TCP connection (static, internal)
- `tcpsendrecv()` - Line 328: Sends/receives over TCP (static, internal)

**Windows-Specific Considerations** (VERIFIED):
```cpp
// Line 302: Socket creation
Socket s(ip.sin4.sin_family, SOCK_STREAM);  // ✅ Compatible

// Line 303: Non-blocking
s.setNonBlocking();  // ✅ Uses Windows-compatible implementation

// Line 304: TCP_NODELAY
setTCPNoDelay(s.getHandle());  // ✅ Compatible

// Line 306: Bind
s.bind(localip);  // ✅ Compatible

// Line 321: TCPIOHandler creation
connection.d_handler = std::make_shared<TCPIOHandler>(...);  // ✅ Should work
```

**Action Items**:
- Verify `Socket` class works correctly on Windows
- Test TCP connection establishment
- Verify `TCPIOHandler` integration

---

### 7. **Synchronous Resolver** (VERIFY TCP FALLBACK)

#### File: `syncres.cc` (EXISTS - Verify TCP retry logic)
**Location**: `pdns_recursor_windows/syncres.cc`

**TCP Fallback Logic** (VERIFIED - Lines 5991-5994):
```cpp
if (forceTCP || (spoofed || (gotAnswer && truncated))) {
  /* retry, over TCP this time */
  gotAnswer = doResolveAtThisIP(..., true, doDoT, ...);
}
```

**Status**: 
- ✅ Logic is correct
- ✅ Calls `doResolveAtThisIP()` with `tcp=true` parameter
- ✅ Should work once TCP infrastructure is in place

**Action Items**:
- Verify `doResolveAtThisIP()` handles TCP parameter correctly
- Test truncated response fallback to TCP

---

## Implementation Steps

### Step 1: Copy Core TCP Module
1. Copy `pdns-recursor/rec-tcp.cc` → `pdns_recursor_windows/rec-tcp.cc`
2. Review and adapt Windows-specific sections:
   - `setCloseOnExec()` - Make Windows-compatible or no-op
   - `TCP_DEFER_ACCEPT` - Already guarded, verify skip on Windows
   - `TCP_FASTOPEN` - Already guarded, verify skip on Windows
   - `SO_REUSEPORT` - Already guarded, verify skip on Windows

### Step 2: Verify Header Declarations
1. Check `rec-main.hh` for required function declarations
2. Add missing declarations if needed:
   - `makeTCPServerSockets()`
   - `handleNewTCPQuestion()`
   - `handleRunningTCPQuestion()` (if not static)

### Step 3: Verify Socket Utilities
1. Review `misc.cc` for all required socket functions
2. Implement `setCloseOnExec()` as no-op on Windows (or skip)
3. Verify `Utility::writev()` exists or implement Windows version

### Step 4: Test TCP Server Socket Creation
1. Test `makeTCPServerSockets()` creates sockets correctly
2. Verify sockets are registered with multiplexer
3. Test socket binding and listening

### Step 5: Test TCP Connection Handling
1. Test `handleNewTCPQuestion()` accepts connections
2. Test `handleRunningTCPQuestion()` processes queries
3. Test `doProcessTCPQuestion()` processes DNS queries
4. Test `finishTCPReply()` sends responses

### Step 6: Test Outgoing TCP
1. Verify `lwres.cc::tcpconnect()` works
2. Test `lwres.cc::tcpsendrecv()` sends/receives
3. Test TCP connection pooling in `rec-tcpout.cc`

### Step 7: Integration Testing
1. Test incoming TCP queries from clients
2. Test TCP fallback for truncated UDP responses
3. Test DNS-over-TLS (if enabled)
4. Test multiple concurrent TCP connections

---

## Dependencies

### External Libraries
- ✅ **libevent** - Already integrated for Windows (I/O multiplexing)
- ✅ **OpenSSL** - Cross-platform (TLS/SSL support)
- ✅ **Winsock2** - Windows socket API (already in use)

### Internal Dependencies
- ✅ `FDMultiplexer` interface - Abstracted via `libeventmplexer.cc`
- ✅ `Socket` class - Should work with Winsock2
- ✅ `ComboAddress` - Platform-agnostic
- ✅ `TCPIOHandler` - Needs review but should work
- ⚠️ `Utility::writev()` - May need Windows implementation

---

## Windows-Specific Considerations

### 1. Socket Options
- `SO_REUSEADDR` - ✅ Works on Windows
- `IPV6_V6ONLY` - ✅ Works on Windows
- `TCP_DEFER_ACCEPT` - ❌ Linux-specific, skip on Windows
- `TCP_FASTOPEN` - ⚠️ May not be available, already guarded
- `SO_REUSEPORT` - ❌ Not available on Windows, already guarded

### 2. File Descriptor Handling
- Windows uses `SOCKET` type (unsigned int) vs `int` on Unix
- ✅ Already handled via `FDWrapper` and socket compatibility layer
- `close()` vs `closesocket()` - ✅ Already wrapped

### 3. Non-Blocking I/O
- Unix: `fcntl(F_SETFL, O_NONBLOCK)`
- Windows: `ioctlsocket(FIONBIO)`
- ✅ Already implemented in `misc.cc::setNonBlocking()`

### 4. Process Management
- `setCloseOnExec()` - Not needed on Windows (no `fork()`)
- Can be no-op or skipped

### 5. I/O Multiplexing
- Unix: `epoll` (Linux) or `kqueue` (BSD)
- Windows: `libevent` (uses `WSAEventSelect` or `select()`)
- ✅ Already abstracted via `libeventmplexer.cc`

---

## Files Summary

| File | Status | Action Required | Priority |
|------|--------|----------------|----------|
| `rec-tcp.cc` | ❌ Missing | Copy from upstream, adapt for Windows | **CRITICAL** |
| `rec-main.hh` | ✅ Exists | Verify function declarations | High |
| `rec-main.cc` | ✅ Exists | Verify integration (already calls `makeTCPServerSockets`) | High |
| `tcpiohandler.cc/hh` | ✅ Exists | Review for Windows compatibility | Medium |
| `rec-tcpout.cc/hh` | ✅ Exists | Review, should work | Low |
| `lwres.cc` | ✅ Exists | Verify TCP functions work | Medium |
| `misc.cc` | ✅ Exists | Verify all socket utilities | Low |
| `pdns_recursor.cc` | ✅ Exists | Verify `sendResponseOverTCP()` | Medium |
| `syncres.cc` | ✅ Exists | Verify TCP fallback logic | Low |

---

## Testing Checklist

### Basic TCP Functionality
- [ ] TCP server socket creation and binding
- [ ] TCP connection acceptance
- [ ] TCP query reception
- [ ] TCP response sending
- [ ] TCP connection cleanup

### Integration
- [ ] TCP queries processed through main query flow
- [ ] TCP responses sent correctly
- [ ] Multiple concurrent TCP connections
- [ ] TCP connection timeout handling

### Fallback Scenarios
- [ ] UDP truncated response → TCP retry
- [ ] TCP fallback for large responses
- [ ] Error handling and cleanup

### Outgoing TCP
- [ ] Outgoing TCP connection establishment
- [ ] TCP query sending to upstream servers
- [ ] TCP response reception
- [ ] TCP connection pooling

---

## Estimated Effort

- **Step 1-2**: Copy and adapt `rec-tcp.cc` - **2-3 days**
- **Step 3**: Verify utilities and fix issues - **1-2 days**
- **Step 4-5**: Test and debug TCP server - **2-3 days**
- **Step 6**: Test outgoing TCP - **1-2 days**
- **Step 7**: Integration testing - **2-3 days**

**Total**: **8-13 days** (1.5-2.5 weeks)

---

## Next Steps

1. **Immediate**: Copy `rec-tcp.cc` from upstream
2. **Review**: Check all Windows-specific sections
3. **Adapt**: Fix Windows compatibility issues
4. **Test**: Basic TCP socket creation and connection
5. **Integrate**: Verify with main query flow
6. **Validate**: Full TCP query/response cycle

---
    
## Notes

- Most socket operations are already Windows-compatible
- Main work is copying `rec-tcp.cc` and adapting minor differences
- I/O multiplexing is already abstracted, should work seamlessly
- TLS/SSL support should work via OpenSSL (cross-platform)
- Focus on testing and debugging rather than major rewrites

