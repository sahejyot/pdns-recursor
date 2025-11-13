# UDP Flow Comparison: Upstream vs Windows POC Implementation

## Overview
This document compares the upstream PowerDNS Recursor UDP flow with our Windows POC implementation, identifying stub functions and differences.

---

## 1. UPSTREAM UDP FLOW

### 1.1 Main Event Loop (Upstream)
**Location:** `pdns-recursor/rec-main.cc:2876` - `recursorThread()`

```
while (!RecursorControlChannel::stop) {
    // 1. Process MTasker tasks (wake up waiting coroutines)
    auto timeoutUsec = g_multiTasker->nextWaiterDelayUsec(500000);
    
    // 2. Run FD multiplexer (epoll/kqueue/libevent) - handles ALL I/O
    t_fdm->run(&g_now, static_cast<int>(timeoutUsec / 1000));
    
    // That's it! Multiplexer handles both incoming queries and outgoing responses
}
```

**Key Points:**
- Single multiplexer (`t_fdm`) handles ALL I/O events
- Uses epoll (Linux) or kqueue (BSD) - level-triggered
- No manual polling needed
- Incoming queries and outgoing responses both handled by multiplexer callbacks

---

### 1.2 Incoming UDP Query Flow (Upstream)

```
┌─────────────────────────────────────────────────────────────┐
│ 1. FDMultiplexer::run() detects UDP socket readable         │
│    (epoll/kqueue/libevent callback)                         │
└─────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────┐
│ 2. handleNewUDPQuestion(fileDesc, param)                   │
│    📁 pdns_recursor.cc:2474                                  │
│                                                              │
│    • recvmsg(fileDesc, &msgh, 0) - Receive UDP packet       │
│    • Extract source/destination from control messages       │
│    • Handle proxy protocol (if enabled)                     │
│    • Validate packet (size, format, opcode)                 │
│    • Parse DNS header using dnsheader_aligned               │
│    • Call doProcessUDPQuestion()                            │
└─────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────┐
│ 3. doProcessUDPQuestion(question, fromaddr, ...)           │
│    📁 pdns_recursor.cc:2190                                  │
│                                                              │
│    • Check packet cache (g_packetCache->getResponsePacket) │
│    • If cache miss:                                         │
│      └─> Create MTask (coroutine) for resolution            │
│          └─> SyncRes::beginResolve()                        │
└─────────────────────────────────────────────────────────────┘
```

**Upstream Functions:**
- `handleNewUDPQuestion()` - Entry point for incoming queries
- `doProcessUDPQuestion()` - Main query processor
- Uses `dnsheader_aligned` for header parsing (handles alignment)

---

### 1.3 Outgoing UDP Query Flow (Upstream)

```
┌─────────────────────────────────────────────────────────────┐
│ 1. SyncRes::doResolve() needs to query upstream server      │
│    └─> Calls asendto()                                      │
└─────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────┐
│ 2. asendto(data, len, toAddress, qid, ...)                  │
│    📁 pdns_recursor.cc:281                                   │
│                                                              │
│    • Create PacketID (domain, qtype, remote, qid)           │
│    • Check for existing waiters (query chaining)             │
│    • t_udpclientsocks->getSocket() - Get/create UDP socket  │
│    • t_fdm->addReadFD(fd, handleUDPServerResponse, pident) │
│    • send(fd, data, len) - Send query                      │
│    • Return Success (socket registered with multiplexer)     │
└─────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────┐
│ 3. MTask yields: g_multiTasker->waitEvent(pident, &data)    │
│    • Coroutine suspends, waiting for response                │
│    • Other queries can be processed                          │
└─────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────┐
│ 4. Response arrives → FDMultiplexer detects socket readable │
│    └─> handleUDPServerResponse(fileDesc, param)            │
│        📁 pdns_recursor.cc:262                                │
│                                                              │
│        • recvfrom(fd, packet, ...) - Receive response       │
│        • Parse DNS header (dnsheader_aligned)                │
│        • Match PacketID (id, domain, remote)                 │
│        • g_multiTasker->sendEvent(pident, &packet)          │
│        • t_fdm->removeReadFD(fd) - Unregister socket        │
│        • t_udpclientsocks->returnSocket(fd) - Return socket │
└─────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────┐
│ 5. MTask resumes: arecvfrom() receives packet                │
│    • Validate response (spoofing checks, ECS matching)       │
│    • Return to SyncRes::doResolve()                          │
└─────────────────────────────────────────────────────────────┘
```

**Upstream Functions:**
- `asendto()` - Send UDP query to upstream server
- `arecvfrom()` - Receive UDP response (called after waitEvent)
- `handleUDPServerResponse()` - Callback for incoming responses
- `UDPClientSocks::getSocket()` - Get/create UDP socket
- `UDPClientSocks::returnSocket()` - Return socket to pool

---

## 2. WINDOWS POC UDP FLOW

### 2.0 I/O Architecture Clarification

**IMPORTANT:** We ARE using **WSAEventSelect** as the PRIMARY I/O mechanism, NOT manual `select()`.

**Architecture:**
```
┌─────────────────────────────────────────────────────────────┐
│ PRIMARY I/O MECHANISM (WSAEventSelect)                       │
│                                                              │
│ g_multiplexer (LibeventFDMultiplexer)                       │
│   └─> libevent win32 backend                                │
│       └─> WSAEventSelect() → handleDNSQuery() callback      │
│                                                              │
│ t_fdm (LibeventFDMultiplexer)                               │
│   └─> libevent win32 backend                                │
│       └─> WSAEventSelect() → handleUDPServerResponse()      │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│ FALLBACK WORKAROUNDS (Manual select())                      │
│                                                              │
│ Manual select() checks in event loop                        │
│   └─> Compensate for WSAEventSelect edge-triggered behavior │
│   └─> Catch events that WSAEventSelect might miss            │
└─────────────────────────────────────────────────────────────┘
```

**Why Both?**
- **WSAEventSelect** (via libevent) is the primary mechanism - it's efficient and handles most events
- **Manual select()** is a fallback workaround because WSAEventSelect is edge-triggered and may miss events on connected UDP sockets
- The workarounds are safety nets, not the primary mechanism

### 2.1 Main Event Loop (Windows POC)
**Location:** `pdns_recursor_windows/main_test.cc:619` - `resolveTaskFunc()`

```
while (true) {
    // 1. Process MTasker tasks (wake up waiting coroutines)
    while (g_multiTasker && g_multiTasker->schedule(g_now)) {
        Utility::gettimeofday(&g_now, nullptr);
    }
    
    // 2. WINDOWS WORKAROUND (FALLBACK): Manual select() check for incoming queries
    //    PRIMARY: g_multiplexer uses WSAEventSelect (via libevent win32 backend)
    //    FALLBACK: Manual select() catches events WSAEventSelect might miss
    #ifdef _WIN32
    if (g_udp_socket >= 0) {
        select() check → handleDNSQuery() if data available
    }
    
    // 3. WINDOWS WORKAROUND (FALLBACK): Manual select() check for outgoing responses
    //    PRIMARY: t_fdm uses WSAEventSelect (via libevent win32 backend)
    //    FALLBACK: Manual select() catches events WSAEventSelect might miss
    if (g_multiTasker && t_fdm) {
        select() check on all waiting sockets → recvfrom() if data available
    }
    #endif
    
    // 4. PRIMARY: Run FD multiplexer (libevent win32 backend → WSAEventSelect)
    //    This is the main I/O mechanism - processes incoming queries via callbacks
    int events = g_multiplexer->run(&g_now, timeoutMsec);
    
    // 5. WINDOWS WORKAROUND (FALLBACK): Check again after multiplexer
    #ifdef _WIN32
    if (g_udp_socket >= 0) {
        select() check → handleDNSQuery() if data available
    }
    #endif
    
    // 6. PRIMARY: Run t_fdm for outgoing UDP socket events (WSAEventSelect)
    //    This processes responses from upstream servers via handleUDPServerResponse()
    if (t_fdm) {
        t_fdm->run(&g_now, timeoutMsec);
    }
}
```

**Key Differences:**
- **Two multiplexers:** `g_multiplexer` (incoming) and `t_fdm` (outgoing)
- **Primary I/O mechanism:** Both use `LibeventFDMultiplexer` → libevent win32 backend → **WSAEventSelect**
- **Fallback workarounds:** Manual `select()` checks compensate for WSAEventSelect's edge-triggered behavior that may miss events
- **Note:** We ARE using WSAEventSelect (via libevent), NOT manual `select()` as primary mechanism

---

### 2.2 Incoming UDP Query Flow (Windows POC)

```
┌─────────────────────────────────────────────────────────────┐
│ 1. Manual select() check OR libevent callback               │
│    (Windows workaround + normal multiplexer)                │
└─────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────┐
│ 2. handleDNSQuery(fd, param)                                │
│    📁 main_test.cc:164                                       │
│                                                              │
│    • recvfrom(fd, buffer, ...) - Receive UDP packet         │
│    • parseWireQuestion() - Parse DNS question                │
│    • Create ResolveJob (qname, qtype, qid, rdflag)          │
│    • g_multiTasker->makeThread(resolveTaskFunc, job)        │
│    • Return quickly (non-blocking)                           │
└─────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────┐
│ 3. resolveTaskFunc(job) - MTasker task                      │
│    📁 main_test.cc:258                                       │
│                                                              │
│    • Create SyncRes resolver                                │
│    • Call SyncRes::beginResolve()                            │
│    • Build response using DNSPacketWriter                    │
│    • sendto() response to client                            │
└─────────────────────────────────────────────────────────────┘
```

**Our Functions:**
- `handleDNSQuery()` - Entry point for incoming queries (replaces `handleNewUDPQuestion`)
- `resolveTaskFunc()` - MTasker task for resolution (replaces `doProcessUDPQuestion` MTask creation)
- `parseWireQuestion()` - Simple wire format parser (bypasses MOADNSParser issues)

**Differences from Upstream:**
- No `doProcessUDPQuestion()` - we use `resolveTaskFunc()` directly
- No packet cache check (simplified for POC)
- No proxy protocol support
- No Lua hooks
- Simpler packet parsing

---

### 2.3 Outgoing UDP Query Flow (Windows POC)

```
┌─────────────────────────────────────────────────────────────┐
│ 1. SyncRes::doResolve() needs to query upstream server      │
│    └─> Calls asendto()                                      │
└─────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────┐
│ 2. asendto(data, len, toAddress, qid, ...)                  │
│    📁 pdns_recursor_poc_parts.cc:281 (upstream code)        │
│                                                              │
│    • Create PacketID (domain, qtype, remote, qid)           │
│    • Check for existing waiters (query chaining)            │
│    • t_udpclientsocks->getSocket() - Get/create UDP socket  │
│    • t_fdm->addReadFD(fd, handleUDPServerResponse, pident) │
│    • send(fd, data, len) - Send query                      │
│    • WINDOWS FIX: Direct wire writes for ID/flags after     │
│      commit() to avoid struct padding issues                │
│    • Return Success                                         │
└─────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────┐
│ 3. MTask yields: g_multiTasker->waitEvent(pident, &data)   │
│    • Coroutine suspends, waiting for response                │
└─────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────┐
│ 4. Response arrives → libevent callback OR manual select()  │
│    └─> handleUDPServerResponse(fileDesc, param)            │
│        📁 pdns_recursor_poc_parts.cc:230                     │
│                                                              │
│        • recvfrom(fd, packet, ...) - Receive response      │
│        • WINDOWS FIX: Parse header with padding handling    │
│          (dnsheader_aligned on Linux, manual copy on Win)  │
│        • Match PacketID (id, domain, remote)                 │
│        • g_multiTasker->sendEvent(pident, &packet)          │
│        • t_fdm->removeReadFD(fd) - Unregister socket        │
│        • t_udpclientsocks->returnSocket(fd) - Return socket│
└─────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────┐
│ 5. MTask resumes: arecvfrom() receives packet               │
│    • Validate response (spoofing checks, ECS matching)      │
│    • Return to SyncRes::doResolve()                          │
└─────────────────────────────────────────────────────────────┘
```

**Our Functions:**
- `asendto()` - **UPSTREAM CODE** (copied from `pdns_recursor.cc`)
- `arecvfrom()` - **UPSTREAM CODE** (copied from `pdns_recursor.cc`)
- `handleUDPServerResponse()` - **UPSTREAM CODE** with Windows padding fixes
- `UDPClientSocks::getSocket()` - **UPSTREAM CODE** (copied)
- `UDPClientSocks::returnSocket()` - **UPSTREAM CODE** with Windows WSAEventSelect fix

**Windows-Specific Fixes:**
- Direct wire writes for ID/flags in `lwres.cc` (bypasses struct padding)
- Manual header parsing in `handleUDPServerResponse()` (handles 14-byte struct)
- WSAEventSelect cleanup in `returnSocket()` (prevents stale state)

---

## 3. STUB FUNCTIONS CREATED

### 3.1 `lwres_stubs.cc`
**Purpose:** Provide minimal implementations when full `pdns_recursor.cc` is not available.

**Stub Functions:**
1. **`asendtcp()`** - Returns `LWResult::Result::PermanentError` (TCP not enabled for POC)
2. **`arecvtcp()`** - Returns `LWResult::Result::PermanentError` (TCP not enabled for POC)
3. **`asendto()`** - Stub (only if `ENABLE_WINDOWS_POC_PARTS` not defined)
4. **`arecvfrom()`** - Stub (only if `ENABLE_WINDOWS_POC_PARTS` not defined)
5. **`mthreadSleep()`** - Uses `std::this_thread::sleep_for()`
6. **`nsspeeds_t::putPB()`** - Returns 0 (stub)
7. **`nsspeeds_t::getPB()`** - Returns 0 (stub)
8. **`primeHints()`** - Returns false (stub)
9. **`RecResponseStats::RecResponseStats()`** - Constructor stub
10. **`RecResponseStats::operator+=()`** - Addition operator stub
11. **`broadcastAccFunction()`** - Template function stub

**Note:** When `ENABLE_WINDOWS_POC_PARTS` is defined, real implementations from `pdns_recursor_poc_parts.cc` are used instead of stubs.

---

### 3.2 `globals_stub.cc`
**Purpose:** Provide global variable definitions and initialization stubs.

**Stub Variables/Functions:**
1. **Global caches** - `g_packetCache`, `g_negCache`, etc. (declared as extern in `main_test.cc`)
2. **Thread-local variables** - Various thread-local storage stubs
3. **Configuration stubs** - Minimal config values for POC

---

### 3.3 `dnssec_stubs.cc`
**Purpose:** Provide DNSSEC validation function stubs.

**Stub Functions:**
1. **DNSSEC validation functions** - Minimal implementations for POC
2. **Wildcard expansion checks** - Stub implementations

---

## 4. KEY DIFFERENCES SUMMARY

| Aspect | Upstream | Windows POC |
|--------|----------|-------------|
| **Incoming Query Handler** | `handleNewUDPQuestion()` | `handleDNSQuery()` |
| **Query Processor** | `doProcessUDPQuestion()` | `resolveTaskFunc()` |
| **Multiplexer** | Single `t_fdm` for all I/O | Two: `g_multiplexer` (incoming) + `t_fdm` (outgoing)<br>Both use `LibeventFDMultiplexer` → libevent win32 backend → **WSAEventSelect** |
| **I/O Detection** | epoll/kqueue (level-triggered) | **PRIMARY:** WSAEventSelect (via libevent win32 backend)<br>**FALLBACK:** Manual `select()` workarounds |
| **Packet Cache** | Full implementation | Stub (not used) |
| **Proxy Protocol** | Supported | Not supported |
| **Lua Hooks** | Full support | Not supported |
| **DNS Header Parsing** | `dnsheader_aligned` (12 bytes) | Windows: Manual copy (14-byte struct) |
| **DNS Header Writing** | Direct struct access | Windows: Direct wire writes (bypasses padding) |
| **Socket Management** | Standard upstream code | Upstream code + WSAEventSelect cleanup |
| **Event Loop** | Simple: `t_fdm->run()` | Complex: Multiple multiplexers + workarounds |

---

## 5. WHY STUBS WERE NEEDED

1. **`lwres_stubs.cc`:**
   - Upstream `pdns_recursor.cc` is large and has many dependencies
   - We only need UDP query/response functions for POC
   - Stubs allow compilation without full upstream code
   - When `ENABLE_WINDOWS_POC_PARTS` is defined, real implementations are used

2. **`globals_stub.cc`:**
   - Many global variables are defined in `pdns_recursor.cc`
   - We need minimal definitions to satisfy linker
   - Some are truly stubbed (not used), others are properly initialized

3. **`dnssec_stubs.cc`:**
   - DNSSEC validation is complex and not needed for basic POC
   - Stubs allow compilation without full DNSSEC implementation

---

## 6. FILES INVOLVED

### Upstream Files (Copied/Adapted):
- `pdns_recursor_poc_parts.cc` - Contains `asendto()`, `arecvfrom()`, `handleUDPServerResponse()`, `UDPClientSocks`
- `lwres.cc` - Contains `asyncresolve()` (calls `asendto()`)

### Our Files:
- `main_test.cc` - Main event loop, `handleDNSQuery()`, `resolveTaskFunc()`
- `lwres_stubs.cc` - Stub implementations
- `globals_stub.cc` - Global variable stubs
- `dnssec_stubs.cc` - DNSSEC stubs

### Modified Files:
- `dnswriter.cc` - Windows padding fixes for DNS header writing
- `pdns_recursor_poc_parts.cc` - Windows padding fixes for header reading

---

## 7. FUTURE INTEGRATION PATH

To fully integrate with upstream:
1. Replace `handleDNSQuery()` with `handleNewUDPQuestion()`
2. Replace `resolveTaskFunc()` with proper `doProcessUDPQuestion()` MTask creation
3. Remove Windows workarounds (manual `select()` checks)
4. Use single multiplexer (`t_fdm`) for all I/O
5. Add packet cache support
6. Add proxy protocol support
7. Add Lua hooks support
8. Remove stub files (use real upstream implementations)

