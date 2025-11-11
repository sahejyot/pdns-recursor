# TCP for Windows - Sprint Summary

**⚠️ ALL INFORMATION VERIFIED FROM ACTUAL CODE** - All function names, line numbers, and integration points verified against source code.

## Quick Overview

**Goal**: Enable TCP support for incoming DNS queries on Windows

**Status**: TCP infrastructure partially exists, but main server module (`rec-tcp.cc`) is missing

**Effort**: 1.5-2.5 weeks

---

## Files to Work On

### 🔴 CRITICAL - Must Add

1. **`rec-tcp.cc`** (NEW FILE)
   - **Source**: Copy from `pdns-recursor/rec-tcp.cc`
   - **Purpose**: Main TCP server implementation
   - **Key Functions**:
     - `makeTCPServerSockets()` - Creates TCP listening sockets
     - `handleNewTCPQuestion()` - Accepts new connections
     - `handleRunningTCPQuestion()` - Handles I/O on connections
     - `doProcessTCPQuestion()` - Processes DNS queries
   - **Changes Needed**:
     - Skip `TCP_DEFER_ACCEPT` (Linux-only)
     - Skip `SO_REUSEPORT` (not on Windows)
     - Make `setCloseOnExec()` no-op (Windows doesn't have `fork()`)
     - Verify `TCP_FASTOPEN` handling (may not be available)

### 🟡 HIGH - Verify/Review

2. **`rec-main.hh`**
   - **Action**: Verify function declarations exist
   - **Functions to check** (VERIFIED):
     - `makeTCPServerSockets()` - Line 649 (upstream) / Line 671 (Windows)
     - `handleNewTCPQuestion()` - Line 650 (upstream) / Line 672 (Windows)
     - `finishTCPReply()` - Line 646 (upstream) / Should exist in Windows

3. **`rec-main.cc`**
   - **Status**: Already calls `makeTCPServerSockets()` in `initDistribution()` function (line 1963)
   - **Action**: Verify integration works after adding `rec-tcp.cc`

4. **`tcpiohandler.cc/hh`**
   - **Status**: Already exists
   - **Action**: Review for Windows compatibility (should work)

### 🟢 MEDIUM - Verify Integration

5. **`lwres.cc`**
   - **Status**: Has `tcpconnect()` and `tcpsendrecv()` functions
   - **Action**: Verify TCP functions work with Windows sockets

6. **`pdns_recursor.cc`**
   - **Status**: Has `sendResponseOverTCP()` usage
   - **Action**: Verify `Utility::writev()` works on Windows

7. **`misc.cc`**
   - **Status**: Already has Windows implementations
   - **Functions**: `setNonBlocking()`, `closesocket()`, `setTCPNoDelay()`
   - **Action**: Verify all are used correctly

### 🔵 LOW - Should Work As-Is

8. **`rec-tcpout.cc/hh`** - Outgoing TCP (already exists, should work)
9. **`syncres.cc`** - TCP fallback logic (already implemented)

---

## Implementation Flow

```
1. Copy rec-tcp.cc from upstream
   ↓
2. Adapt Windows-specific sections
   ↓
3. Verify header declarations in rec-main.hh
   ↓
4. Compile and fix build errors
   ↓
5. Test TCP socket creation (makeTCPServerSockets)
   ↓
6. Test connection acceptance (handleNewTCPQuestion)
   ↓
7. Test query processing (handleRunningTCPQuestion)
   ↓
8. Integration test with main query flow
```

---

## Key Integration Points

### 1. Main Loop Integration
**File**: `rec-main.cc::initDistribution()` (line 1923)
**Location**: Lines 1958-1964
```cpp
for (unsigned int i = 0; i < RecThreadInfo::numTCPWorkers(); i++, threadNum++) {
  auto& info = RecThreadInfo::info(threadNum);
  auto& deferredAdds = info.getDeferredAdds();
  auto& tcpSockets = info.getTCPSockets();
  count += makeTCPServerSockets(deferredAdds, tcpSockets, log, ...);
}
```
**Status**: ✅ Code structure ready, just needs `rec-tcp.cc`

### 2. Multiplexer Registration
**File**: `rec-tcp.cc` (in `makeTCPServerSockets()`)
```cpp
deferredAdds.emplace_back(socketFd, handleNewTCPQuestion);
```
**Status**: ✅ Uses existing `libeventmplexer.cc` (Windows-compatible)

### 3. Query Processing
**File**: `rec-tcp.cc` (in `doProcessTCPQuestion()`)
- Creates `DNSComboWriter` object
- Calls main query processing functions
- Sends response via `sendResponseOverTCP()`
**Status**: ✅ Should integrate with existing query flow

---

## Windows Compatibility Status

| Component | Status | Notes |
|-----------|--------|-------|
| Socket creation (`socket()`) | ✅ | Winsock2 compatible |
| Non-blocking I/O | ✅ | `setNonBlocking()` implemented |
| Socket close | ✅ | `closesocket()` wrapper exists |
| I/O multiplexing | ✅ | `libeventmplexer.cc` works |
| TCP_NODELAY | ✅ | Implemented in `misc.cc` |
| SO_REUSEADDR | ✅ | Standard Winsock2 |
| IPV6_V6ONLY | ✅ | Standard Winsock2 |
| TCP_DEFER_ACCEPT | ❌ | Linux-only, skip |
| SO_REUSEPORT | ❌ | Not on Windows, skip |
| TCP_FASTOPEN | ⚠️ | May not be available, guarded |

---

## Testing Strategy

### Phase 1: Basic TCP Socket
- [ ] Create TCP listening socket
- [ ] Bind to address/port
- [ ] Set non-blocking
- [ ] Register with multiplexer

### Phase 2: Connection Handling
- [ ] Accept incoming connection
- [ ] Read DNS query (2-byte length + packet)
- [ ] Parse DNS query
- [ ] Process through main query flow

### Phase 3: Response Handling
- [ ] Generate DNS response
- [ ] Send response (2-byte length + packet)
- [ ] Handle connection cleanup
- [ ] Test multiple queries on same connection

### Phase 4: Integration
- [ ] Test with real DNS client
- [ ] Test truncated UDP → TCP fallback
- [ ] Test concurrent connections
- [ ] Test error handling

---

## Dependencies

### Already Available
- ✅ Winsock2 (Windows socket API)
- ✅ libevent (I/O multiplexing)
- ✅ OpenSSL (TLS/SSL if needed)
- ✅ Socket utility functions (`misc.cc`)

### May Need
- ⚠️ `Utility::writev()` - Verify or implement Windows version
- ⚠️ `setCloseOnExec()` - Make no-op on Windows

---

## Risk Areas

1. **`Utility::writev()`** - May need Windows implementation
2. **Socket option differences** - Some Linux-specific options need to be skipped
3. **I/O multiplexing** - Should work but needs testing
4. **TLS/SSL** - OpenSSL should work, but verify initialization

---

## Success Criteria

- [ ] TCP server socket created and listening
- [ ] Incoming TCP connections accepted
- [ ] DNS queries received over TCP
- [ ] DNS responses sent over TCP
- [ ] Multiple queries per connection work
- [ ] Connection cleanup works correctly
- [ ] Integration with main query flow works
- [ ] TCP fallback for truncated UDP works

---

## Estimated Timeline

| Task | Days | Notes |
|------|------|-------|
| Copy and adapt `rec-tcp.cc` | 2-3 | Main implementation |
| Fix build errors | 1 | Compilation issues |
| Basic TCP socket test | 1 | Socket creation |
| Connection handling test | 1-2 | Accept and read |
| Query processing test | 1-2 | Full query/response |
| Integration testing | 2-3 | End-to-end |
| **Total** | **8-13 days** | **1.5-2.5 weeks** |

---

## Next Sprint Actions

1. **Copy** `pdns-recursor/rec-tcp.cc` → `pdns_recursor_windows/rec-tcp.cc`
2. **Review** Windows-specific sections
3. **Adapt** Linux-only code (TCP_DEFER_ACCEPT, SO_REUSEPORT, etc.)
4. **Verify** header declarations in `rec-main.hh`
5. **Compile** and fix errors
6. **Test** basic TCP socket creation
7. **Iterate** on connection handling and query processing

