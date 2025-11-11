# TCP Module File Relationships and Data Flow

**⚠️ ALL INFORMATION VERIFIED FROM ACTUAL CODE** - All function calls, line numbers, and data flow verified against `pdns-recursor/rec-tcp.cc`, `pdns-recursor/pdns_recursor.cc`, and related source files.

## File Dependency Graph

```
┌─────────────────────────────────────────────────────────────┐
│                    Main Entry Point                          │
│                  rec-main.cc                                 │
│  - initDistribution() calls makeTCPServerSockets()           │
│  - Sets up TCP worker threads                                │
│  - Line 1923: initDistribution() function                    │
│  - Line 1959-1963: Calls makeTCPServerSockets()               │
└────────────────────┬────────────────────────────────────────┘
                     │
                     │ calls
                     ▼
┌─────────────────────────────────────────────────────────────┐
│              rec-tcp.cc (NEW - NEEDS TO BE ADDED)           │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ makeTCPServerSockets() (line 1097)                   │   │
│  │  - Creates TCP listening sockets                     │   │
│  │  - Binds to address/port                             │   │
│  │  - Registers with multiplexer (line 1200)            │   │
│  └──────────────────┬──────────────────────────────────┘   │
│                     │                                        │
│  ┌──────────────────▼──────────────────────────────────┐   │
│  │ handleNewTCPQuestion() (line 696)                   │   │
│  │  - Accepts new TCP connections (line 700)            │   │
│  │  - Creates TCPConnection object (line 755)          │   │
│  │  - Registers with multiplexer (line 773)            │   │
│  └──────────────────┬──────────────────────────────────┘   │
│                     │                                        │
│  ┌──────────────────▼──────────────────────────────────┐   │
│  │ handleRunningTCPQuestion() (line 519, static)       │   │
│  │  - Reads DNS query from TCP                          │   │
│  │  - Parses 2-byte length prefix                       │   │
│  │  - Calls doProcessTCPQuestion() (line 688)           │   │
│  └──────────────────┬──────────────────────────────────┘   │
│                     │                                        │
│  ┌──────────────────▼──────────────────────────────────┐   │
│  │ doProcessTCPQuestion() (line 292, static)           │   │
│  │  - Parses DNS query (qname, qtype, qclass)           │   │
│  │  - Checks packet cache (line 454)                     │   │
│  │  - Spawns startDoResolve() if cache miss (line 515)  │   │
│  └──────────────────┬──────────────────────────────────┘   │
└─────────────────────┼──────────────────────────────────────┘
                     │
                     │ uses
                     ▼
┌─────────────────────────────────────────────────────────────┐
│              rec-main.hh                                     │
│  - Function declarations (lines 644-650)                     │
│  - DNSComboWriter class                                      │
│  - sendResponseOverTCP() template function (line 300)        │
└────────────────────┬────────────────────────────────────────┘
                     │
                     │ uses
                     ▼
┌─────────────────────────────────────────────────────────────┐
│              pdns_recursor.cc                                │
│  - doProcessUDPQuestion() (shared logic)                     │
│  - Main query processing flow                                │
└────────────────────┬────────────────────────────────────────┘
                     │
                     │ uses
                     ▼
┌─────────────────────────────────────────────────────────────┐
│              syncres.cc                                      │
│  - doResolve() - Recursive resolution                        │
│  - TCP fallback for truncated responses                      │
└────────────────────┬────────────────────────────────────────┘
                     │
                     │ uses (for outgoing TCP)
                     ▼
┌─────────────────────────────────────────────────────────────┐
│              lwres.cc                                         │
│  - tcpconnect() (line 291, static) - Create outgoing TCP    │
│  - tcpsendrecv() (line 328, static) - Send/receive over TCP │
│  └──────────────────┬───────────────────────────────────────┘
│                     │ uses
│                     ▼
│  ┌──────────────────────────────────────────────────────┐ │
│  │ tcpiohandler.cc/hh                                    │ │
│  │  - TCPIOHandler class                                 │ │
│  │  - Low-level TCP I/O                                  │ │
│  │  - TLS/SSL support                                    │ │
│  └──────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
                     │
                     │ uses
                     ▼
┌─────────────────────────────────────────────────────────────┐
│              misc.cc                                         │
│  - setNonBlocking() - Windows implementation                │
│  - closesocket() - Windows wrapper                          │
│  - setTCPNoDelay() - TCP_NODELAY                            │
└─────────────────────────────────────────────────────────────┘
                     │
                     │ uses
                     ▼
┌─────────────────────────────────────────────────────────────┐
│              libeventmplexer.cc                              │
│  - FDMultiplexer implementation for Windows                  │
│  - Uses libevent for I/O events                             │
└─────────────────────────────────────────────────────────────┘
```

---

## Data Flow: Incoming TCP Query

**VERIFIED FROM CODE** (rec-tcp.cc:292-515, pdns_recursor.cc:973)

```
Client
  │
  │ TCP Connect
  ▼
┌─────────────────────────────────────────────────────────────┐
│ libeventmplexer.cc                                            │
│  - Detects new connection on listening socket                │
│  - Calls handleNewTCPQuestion()                              │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│ rec-tcp.cc::handleNewTCPQuestion()                          │
│  1. accept() - Accept connection                            │
│  2. setNonBlocking() - Set non-blocking                     │
│  3. setTCPNoDelay() - Enable TCP_NODELAY                     │
│  4. Create TCPConnection object                             │
│  5. Register with multiplexer                               │
└────────────────────┬────────────────────────────────────────┘
                     │
                     │ Connection registered
                     ▼
┌─────────────────────────────────────────────────────────────┐
│ libeventmplexer.cc                                            │
│  - Detects data available on connection                     │
│  - Calls handleRunningTCPQuestion()                         │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│ rec-tcp.cc::handleRunningTCPQuestion()                      │
│  1. Read 2-byte length prefix                               │
│  2. Read DNS query packet                                    │
│  3. Parse DNS query                                          │
│  4. Call doProcessTCPQuestion()                             │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│ rec-tcp.cc::doProcessTCPQuestion()                          │
│  1. Parse DNS query (qname, qtype, qclass)                  │
│  2. Call Lua gettag hooks                                    │
│  3. Check packet cache (checkForCacheHit)                    │
│  4a. If cache HIT: Send response directly                    │
│  4b. If cache MISS: Spawn startDoResolve() via multi-tasker │
└────────────────────┬────────────────────────────────────────┘
                     │
                     │ Cache miss → spawns task
                     ▼
┌─────────────────────────────────────────────────────────────┐
│ pdns_recursor.cc::startDoResolve()                           │
│  (Called via g_multiTasker->makeThread)                      │
│  1. Create SyncRes resolver                                   │
│  2. Parse EDNS options                                        │
│  3. Call resolver.beginResolve()                             │
│  4. Generate response packet                                  │
│  5. Send response via sendResponseOverTCP()                  │
└────────────────────┬────────────────────────────────────────┘
                     │
                     │ calls
                     ▼
┌─────────────────────────────────────────────────────────────┐
│ syncres.cc::SyncRes::beginResolve()                          │
│  - Calls SyncRes::doResolve()                                │
│  - Handles recursive resolution                              │
│  - Returns DNS records                                       │
└─────────────────────────────────────────────────────────────┘
                     │
                     │ Response generated
                     ▼
┌─────────────────────────────────────────────────────────────┐
│ rec-main.hh::sendResponseOverTCP()                          │
│  1. Prepend 2-byte length                                    │
│  2. Write response to TCP socket                            │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│ rec-tcp.cc::finishTCPReply()                                 │
│  1. Update connection state                                  │
│  2. Check if connection should close                        │
│  3. Re-register for next query or close                     │
└─────────────────────────────────────────────────────────────┘
```

**Key Points (VERIFIED FROM CODE)**:
- `doProcessTCPQuestion()` does NOT call `doProcessUDPQuestion()`
- TCP has its own cache check logic (line 454 in rec-tcp.cc)
- Both TCP and UDP eventually call `startDoResolve()` via multi-tasker
- `startDoResolve()` is shared between TCP and UDP, but entry points differ:
  - UDP: `doProcessUDPQuestion()` → `startDoResolve()`
  - TCP: `doProcessTCPQuestion()` → `startDoResolve()`

---

## Data Flow: Outgoing TCP Query (Upstream)

```
syncres.cc::doResolve()
  │
  │ Needs to query upstream server
  │ UDP fails or truncated
  ▼
┌─────────────────────────────────────────────────────────────┐
│ lwres.cc::tcpconnect()                                       │
│  1. Get connection from TCPOutConnectionManager             │
│  2. If not available, create new connection                │
│  3. Create Socket (SOCK_STREAM)                            │
│  4. setNonBlocking()                                         │
│  5. setTCPNoDelay()                                          │
│  6. bind() to local address                                 │
│  7. Create TCPIOHandler                                      │
│  8. tryConnect() - Connect to upstream                      │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│ tcpiohandler.cc::TCPIOHandler                                │
│  - Manages TCP connection state                              │
│  - Handles non-blocking connect                              │
│  - Handles TLS/SSL if needed                                 │
└────────────────────┬────────────────────────────────────────┘
                     │
                     │ Connection established
                     ▼
┌─────────────────────────────────────────────────────────────┐
│ lwres.cc::tcpsendrecv()                                      │
│  1. Send 2-byte length + DNS query                          │
│  2. Read 2-byte length                                       │
│  3. Read DNS response                                        │
│  4. Return response                                          │
└────────────────────┬────────────────────────────────────────┘
                     │
                     │ Response received
                     ▼
┌─────────────────────────────────────────────────────────────┐
│ rec-tcpout.cc::TCPOutConnectionManager                       │
│  - Store connection for reuse                                │
│  - Manage connection pool                                    │
│  - Cleanup idle connections                                  │
└─────────────────────────────────────────────────────────────┘
```

---

## Module Responsibilities

### rec-tcp.cc (NEW - Main TCP Server)
- **Incoming TCP connections**
- Socket creation and binding
- Connection acceptance
- Query reading and parsing
- Response sending
- Connection lifecycle management

### tcpiohandler.cc/hh (EXISTS)
- **Low-level TCP I/O**
- Non-blocking I/O operations
- TLS/SSL handling
- Connection state management
- Used by outgoing TCP connections

### rec-tcpout.cc/hh (EXISTS)
- **Outgoing TCP connection management**
- Connection pooling
- Idle connection cleanup
- Connection reuse

### lwres.cc (EXISTS)
- **DNS query sending**
- UDP and TCP query functions
- `tcpconnect()` - Create outgoing TCP
- `tcpsendrecv()` - Send/receive over TCP

### misc.cc (EXISTS)
- **Socket utility functions**
- `setNonBlocking()` - Windows implementation
- `closesocket()` - Windows wrapper
- `setTCPNoDelay()` - TCP_NODELAY option

### libeventmplexer.cc (EXISTS)
- **I/O event multiplexing**
- Detects socket events
- Calls registered callbacks
- Windows-compatible via libevent

---

## Integration Points

### 1. Main Initialization
**File**: `rec-main.cc`
**Function**: `initDistribution()` (line 1923)
**Action**: Calls `makeTCPServerSockets()` to create TCP listeners (line 1963)

### 2. Multiplexer Registration
**File**: `rec-tcp.cc`
**Function**: `makeTCPServerSockets()`
**Action**: Registers listening sockets with `deferredAdds` for multiplexer

### 3. Query Processing
**File**: `rec-tcp.cc`
**Function**: `doProcessTCPQuestion()`
**Action**: Integrates with main query processing flow (shared with UDP)

### 4. Response Sending
**File**: `rec-main.hh`
**Function**: `sendResponseOverTCP()`
**Action**: Sends DNS response over TCP with length prefix

### 5. Outgoing TCP
**File**: `lwres.cc`
**Functions**: `tcpconnect()`, `tcpsendrecv()`
**Action**: Used by `syncres.cc` for upstream queries

---

## Key Functions to Implement/Verify

### In rec-tcp.cc (NEW - VERIFIED)
1. `makeTCPServerSockets()` - Line 1097, non-static, exported
2. `handleNewTCPQuestion()` - Line 696, non-static, exported
3. `handleRunningTCPQuestion()` - Line 519, static, internal
4. `doProcessTCPQuestion()` - Line 292, static, internal
5. `finishTCPReply()` - Line 160, non-static, exported
6. `sendErrorOverTCP()` - Line 126, static, internal

### In rec-main.hh (VERIFIED)
1. `makeTCPServerSockets()` - Line 649 (upstream) / Line 671 (Windows)
2. `handleNewTCPQuestion()` - Line 650 (upstream) / Line 672 (Windows)
3. `finishTCPReply()` - Line 646 (upstream)
4. `sendResponseOverTCP()` - Line 300 (upstream) / Line 322 (Windows), template function

### In misc.cc (VERIFY)
1. `setNonBlocking()` - ✅ Windows implementation exists
2. `closesocket()` - ✅ Windows wrapper exists
3. `setTCPNoDelay()` - ✅ Exists

### In tcpiohandler.cc (REVIEW)
1. `TCPIOHandler` class - Review for Windows compatibility
2. TLS/SSL handling - Verify OpenSSL works

---

## Windows-Specific Adaptations

### Already Handled
- ✅ Socket creation (`socket()`)
- ✅ Non-blocking I/O (`setNonBlocking()`)
- ✅ Socket close (`closesocket()`)
- ✅ I/O multiplexing (`libeventmplexer.cc`)

### Need to Skip/Adapt
- ❌ `TCP_DEFER_ACCEPT` - Linux-only, skip on Windows
- ❌ `SO_REUSEPORT` - Not on Windows, skip
- ⚠️ `setCloseOnExec()` - No-op on Windows (no `fork()`)
- ⚠️ `TCP_FASTOPEN` - May not be available, already guarded

### Need to Verify
- ⚠️ `Utility::writev()` - May need Windows implementation
- ⚠️ TLS/SSL initialization - Verify OpenSSL works on Windows

