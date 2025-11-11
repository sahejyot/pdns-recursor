# Minimal Setup Requirements for Using pdns_recursor.cc

## Overview
This document outlines what's needed to use the original upstream `pdns_recursor.cc` with minimal setup, identifying essential functions vs. optional features that can be commented out.

---

## Essential Functions for Current UDP Flow

### **Core Resolution Functions (MUST ENABLE)**

1. **`startDoResolve()`** - Line 973
   - **Purpose:** Main DNS resolution function
   - **Called by:** `g_multiTasker->makeThread(startDoResolve, comboWriter)`
   - **Dependencies:** `SyncRes`, `DNSComboWriter`, `DNSPacketWriter`
   - **Status:** ✅ **ESSENTIAL** - Core resolution logic

2. **`handleNewUDPQuestion()`** - Line 2474
   - **Purpose:** Handles incoming UDP queries from clients
   - **Called by:** `t_fdm->addReadFD(socket, handleNewUDPQuestion)`
   - **Dependencies:** `recvmsg()`, `doProcessUDPQuestion()`
   - **Status:** ✅ **ESSENTIAL** - Entry point for client queries

3. **`doProcessUDPQuestion()`** - Line 2190
   - **Purpose:** Processes UDP query and creates DNSComboWriter
   - **Called by:** `handleNewUDPQuestion()`
   - **Dependencies:** `MOADNSParser`, `DNSComboWriter`, `startDoResolve()`
   - **Status:** ✅ **ESSENTIAL** - Query processing

4. **`handleUDPServerResponse()`** - Line 2979
   - **Purpose:** Handles responses from upstream DNS servers
   - **Called by:** `t_fdm->addReadFD(socket, handleUDPServerResponse)`
   - **Dependencies:** `recvfrom()`, `g_multiTasker->sendEvent()`
   - **Status:** ✅ **ESSENTIAL** - Upstream response handling
   - **Windows Fix Needed:** ✅ Yes (struct padding, byte order)

---

### **Supporting Functions (MUST ENABLE)**

5. **`asendto()`** - Line 281
   - **Purpose:** Sends UDP query to upstream server
   - **Called by:** `lwres.cc::asyncresolve()`
   - **Dependencies:** `UDPClientSocks::getSocket()`, `handleUDPServerResponse()`
   - **Status:** ✅ **ESSENTIAL** - Outgoing query sending

6. **`arecvfrom()`** - Line 340
   - **Purpose:** Receives UDP response from upstream server
   - **Called by:** `lwres.cc::asyncresolve()` (via MTasker)
   - **Dependencies:** `handleUDPServerResponse()`, `g_multiTasker->waitEvent()`
   - **Status:** ✅ **ESSENTIAL** - Response receiving

7. **`UDPClientSocks::getSocket()`** - Line 82
   - **Purpose:** Gets UDP socket for upstream queries
   - **Called by:** `asendto()`
   - **Dependencies:** `makeClientSocket()`, `connect()`
   - **Status:** ✅ **ESSENTIAL** - Socket management

8. **`UDPClientSocks::returnSocket()`** - Line 131
   - **Purpose:** Returns socket to pool
   - **Called by:** `handleUDPServerResponse()`, error handlers
   - **Status:** ✅ **ESSENTIAL** - Socket cleanup

9. **`UDPClientSocks::makeClientSocket()`** - Line 152
   - **Purpose:** Creates UDP client socket
   - **Called by:** `getSocket()`
   - **Status:** ✅ **ESSENTIAL** - Socket creation

10. **`addRecordToPacket()`** - Line 457
    - **Purpose:** Adds DNS record to response packet
    - **Called by:** `startDoResolve()` (in loop)
    - **Status:** ✅ **ESSENTIAL** - Response construction

11. **`authWaitTimeMSec()`** - Line 268
    - **Purpose:** Calculates timeout for upstream queries
    - **Called by:** `lwres.cc` (indirectly)
    - **Status:** ✅ **ESSENTIAL** - Timeout calculation

12. **`doResends()`** - Line 2918
    - **Purpose:** Handles query retries
    - **Called by:** `handleUDPServerResponse()` (on errors)
    - **Status:** ✅ **ESSENTIAL** - Error handling

---

## Optional Features (CAN BE COMMENTED OUT)

### **TCP Support (CAN DISABLE)**

- **Functions:**
  - `finishTCPReply()` - Line ~160 (in rec-tcp.cc, not in pdns_recursor.cc)
  - `handleTCPConnection()` - Not in pdns_recursor.cc
  - TCP-related code in `startDoResolve()` - Lines ~1150, ~1600+
  
- **How to Disable:**
  ```cpp
  #if 0  // DISABLE TCP FOR MINIMAL SETUP
  // TCP handling code
  #endif
  ```

- **Impact:** No TCP support (UDP only)
- **Status:** ⚠️ **OPTIONAL** - Can disable for minimal setup

---

### **Lua Scripting (CAN DISABLE)**

- **Functions:**
  - Lua-related code in `doProcessUDPQuestion()` - Lines ~2226-2300
  - Lua-related code in `startDoResolve()` - Lines ~1090-1120, ~1400-1500
  - `t_pdl` (thread-local Lua context) usage

- **How to Disable:**
  ```cpp
  #if 0  // DISABLE LUA FOR MINIMAL SETUP
  if (comboWriter->d_luaContext) {
    // Lua code
  }
  #endif
  ```

- **Impact:** No Lua scripting support
- **Status:** ⚠️ **OPTIONAL** - Can disable for minimal setup

---

### **Protobuf Logging (CAN DISABLE)**

- **Functions:**
  - `checkProtobufExport()` - Line ~2227
  - `logIncomingQuery()` - Line ~190 (declared, used in doProcessUDPQuestion)
  - `logIncomingResponse()` - Line ~219 (declared, used in startDoResolve)
  - `addPolicyTagsToPBMessageIfNeeded()` - Line 959

- **How to Disable:**
  ```cpp
  #if 0  // DISABLE PROTOBUF LOGGING FOR MINIMAL SETUP
  const auto pbExport = checkProtobufExport(luaconfsLocal);
  if (pbExport) {
    // Protobuf code
  }
  #endif
  ```

- **Impact:** No protobuf query/response logging
- **Status:** ⚠️ **OPTIONAL** - Can disable for minimal setup

---

### **FrameStream Logging (CAN DISABLE)**

- **Functions:**
  - `checkFrameStreamExport()` - Line ~2238
  - FrameStream-related code in `startDoResolve()`

- **How to Disable:**
  ```cpp
  #ifdef HAVE_FSTRM
  #if 0  // DISABLE FRAMESTREAM FOR MINIMAL SETUP
  checkFrameStreamExport(...);
  #endif
  #endif
  ```

- **Impact:** No FrameStream logging
- **Status:** ⚠️ **OPTIONAL** - Can disable for minimal setup

---

### **Proxy Protocol (CAN DISABLE)**

- **Functions:**
  - `expectProxyProtocol()` - Line 2171
  - Proxy protocol parsing in `handleNewUDPQuestion()` - Lines ~2490-2550
  - `ProxyProtocolValue` handling

- **How to Disable:**
  ```cpp
  #if 0  // DISABLE PROXY PROTOCOL FOR MINIMAL SETUP
  if (expectProxyProtocol(fromaddr, listenAddress)) {
    // Proxy protocol code
  }
  #endif
  ```

- **Impact:** No proxy protocol support
- **Status:** ⚠️ **OPTIONAL** - Can disable for minimal setup

---

### **ECS (EDNS Client Subnet) (CAN DISABLE)**

- **Functions:**
  - `checkIncomingECSSource()` - Line 344, 2953
  - ECS parsing in `doProcessUDPQuestion()` - Line ~2221
  - ECS handling in `startDoResolve()` - Line ~1020-1030

- **How to Disable:**
  ```cpp
  #if 0  // DISABLE ECS FOR MINIMAL SETUP
  if (option.first == EDNSOptionCode::ECS) {
    // ECS code
  }
  #endif
  ```

- **Impact:** No ECS support
- **Status:** ⚠️ **OPTIONAL** - Can disable for minimal setup

---

### **Policy Filtering / RPZ (CAN DISABLE)**

- **Functions:**
  - `handlePolicyHit()` - Line 535
  - `handleRPZCustom()` - Line 440
  - Policy-related code in `doProcessUDPQuestion()` - Lines ~2240-2300
  - Policy-related code in `startDoResolve()` - Lines ~1155-1200

- **How to Disable:**
  ```cpp
  #if 0  // DISABLE POLICY FILTERING FOR MINIMAL SETUP
  DNSFilterEngine::Policy appliedPolicy;
  // Policy code
  #endif
  ```

- **Impact:** No RPZ/policy filtering
- **Status:** ⚠️ **OPTIONAL** - Can disable for minimal setup

---

### **Event Tracing (CAN DISABLE)**

- **Functions:**
  - `RecEventTrace` usage throughout
  - `pdns::trace::InitialSpanInfo` usage
  - `dumpTrace()` - Line 894

- **How to Disable:**
  ```cpp
  #if 0  // DISABLE EVENT TRACING FOR MINIMAL SETUP
  RecEventTrace eventTrace;
  eventTrace.add(...);
  #endif
  ```

- **Impact:** No event tracing
- **Status:** ⚠️ **OPTIONAL** - Can disable for minimal setup

---

### **NOD (Newly Observed Domains) (CAN DISABLE)**

- **Functions:**
  - `nodCheckNewDomain()` - Line 632
  - `sendNODLookup()` - Line 652
  - `udrCheckUniqueDNSRecord()` - Line 673
  - NOD-related code in `startDoResolve()` - Lines ~990-1000, ~1555-1570

- **How to Disable:**
  ```cpp
  #ifdef NOD_ENABLED
  #if 0  // DISABLE NOD FOR MINIMAL SETUP
  if (g_nodEnabled) {
    // NOD code
  }
  #endif
  #endif
  ```

- **Impact:** No NOD/UDR support
- **Status:** ⚠️ **OPTIONAL** - Can disable for minimal setup

---

### **DNS64 (CAN DISABLE)**

- **Functions:**
  - `dns64Candidate()` - Line 702, 841
  - `getFakeAAAARecords()` - Line 737
  - `getFakePTRRecords()` - Line 803
  - DNS64 code in `startDoResolve()` - Lines ~1700-1800

- **How to Disable:**
  ```cpp
  #if 0  // DISABLE DNS64 FOR MINIMAL SETUP
  if (dns64Candidate(qtype, rcode, ret)) {
    // DNS64 code
  }
  #endif
  ```

- **Impact:** No DNS64 support
- **Status:** ⚠️ **OPTIONAL** - Can disable for minimal setup

---

### **CNAME Following (CAN DISABLE)**

- **Functions:**
  - `followCNAMERecords()` - Line 704
  - CNAME following code in `startDoResolve()` - Lines ~1600-1700

- **How to Disable:**
  ```cpp
  #if 0  // DISABLE CNAME FOLLOWING FOR MINIMAL SETUP
  if (comboWriter->d_followCNAMERecords) {
    // CNAME following code
  }
  #endif
  ```

- **Impact:** No automatic CNAME following
- **Status:** ⚠️ **OPTIONAL** - Can disable for minimal setup

---

### **Query Distribution / Worker Threads (CAN DISABLE)**

- **Functions:**
  - `trySendingQueryToWorker()` - Line 2811
  - `getWorkerLoad()` - Line 2841
  - `selectWorker()` - Line 2850
  - `distributeAsyncFunction()` - Line 2880

- **How to Disable:**
  ```cpp
  #if 0  // DISABLE QUERY DISTRIBUTION FOR MINIMAL SETUP
  if (trySendingQueryToWorker(...)) {
    // Distribution code
  }
  #endif
  ```

- **Impact:** No query distribution across workers
- **Status:** ⚠️ **OPTIONAL** - Can disable for single-threaded setup

---

### **Cache Wiping (CAN DISABLE)**

- **Functions:**
  - `pleaseWipeCaches()` - Line 2148
  - `requestWipeCaches()` - Line 2156

- **How to Disable:**
  ```cpp
  #if 0  // DISABLE CACHE WIPING FOR MINIMAL SETUP
  void requestWipeCaches(...) {
    // Cache wipe code
  }
  #endif
  ```

- **Impact:** No cache wiping functionality
- **Status:** ⚠️ **OPTIONAL** - Can disable for minimal setup

---

## Prerequisites / Initialization Required

### **1. Global Variables (MUST INITIALIZE)**

From `rec-main.hh`:
```cpp
extern thread_local std::unique_ptr<MT_t> g_multiTasker;
extern thread_local std::unique_ptr<FDMultiplexer> t_fdm;
extern thread_local std::unique_ptr<UDPClientSocks> t_udpclientsocks;
extern struct timeval g_now;
```

**Initialization:**
```cpp
// In main() or initialization function
g_multiTasker = std::make_unique<MT_t>();
t_fdm = std::unique_ptr<FDMultiplexer>(FDMultiplexer::getMultiplexerSilent());
t_udpclientsocks = std::make_unique<UDPClientSocks>();
gettimeofday(&g_now, nullptr);
```

---

### **2. Configuration Variables (MUST SET)**

From `rec-main.hh` and `pdns_recursor.cc`:
```cpp
extern unsigned int g_networkTimeoutMsec;      // Network timeout (default: 2000)
extern unsigned int g_maxMThreads;            // Max MTasker threads (default: 2048)
extern uint16_t g_udpTruncationThreshold;     // UDP truncation threshold (default: 1232)
extern unsigned int g_maxUDPQueriesPerRound;  // Max queries per UDP round (default: 1)
extern bool g_logCommonErrors;                // Log common errors (default: true)
extern bool g_quiet;                          // Quiet mode (default: false)
```

**Initialization:**
```cpp
g_networkTimeoutMsec = 2000;
g_maxMThreads = 2048;
g_udpTruncationThreshold = 1232;
g_maxUDPQueriesPerRound = 1;
g_logCommonErrors = true;
g_quiet = false;
```

---

### **3. Counters (MUST INITIALIZE)**

From `rec-tcounters.hh`:
```cpp
extern thread_local rec::t_Counters t_Counters;
```

**Initialization:**
```cpp
// Counters are automatically initialized when accessed
// But ensure rec::t_Counters is properly initialized
```

---

### **4. Socket Creation (MUST DO)**

**For Incoming Queries:**
```cpp
// Create UDP socket
int socketFd = socket(AF_INET, SOCK_DGRAM, 0);
// Bind to port
bind(socketFd, ...);
// Set non-blocking
// Register with multiplexer
t_fdm->addReadFD(socketFd, handleNewUDPQuestion, boost::any());
```

**For Outgoing Queries:**
- Handled automatically by `UDPClientSocks::getSocket()`
- No manual setup needed

---

### **5. Event Loop (MUST RUN)**

```cpp
while (running) {
    // Update time
    gettimeofday(&g_now, nullptr);
    
    // Process MTasker tasks
    g_multiTasker->schedule(g_now);
    
    // Process I/O events
    int timeoutMsec = g_multiTasker->getRemainingTime(g_now);
    t_fdm->run(&g_now, timeoutMsec);
}
```

---

## Windows-Specific Fixes Required

### **1. Response Construction (startDoResolve)**
- **Location:** Lines ~1070-1076
- **Fix:** Direct wire writes for header fields (ID, flags)
- **Status:** ✅ **REQUIRED**

### **2. Response Parsing (handleUDPServerResponse)**
- **Location:** Lines ~3017-3022
- **Fix:** Manual header parsing, `ntohs()` for ID
- **Status:** ✅ **REQUIRED**

### **3. Outgoing Query Construction (asendto)**
- **Location:** Already handled in `lwres.cc`
- **Fix:** Direct wire writes for ID/flags
- **Status:** ✅ **ALREADY FIXED**

---

## Minimal Setup Summary

### **Functions to ENABLE:**
1. ✅ `startDoResolve()` - Core resolution
2. ✅ `handleNewUDPQuestion()` - Incoming queries
3. ✅ `doProcessUDPQuestion()` - Query processing
4. ✅ `handleUDPServerResponse()` - Upstream responses
5. ✅ `asendto()` / `arecvfrom()` - Upstream I/O
6. ✅ `UDPClientSocks` methods - Socket management
7. ✅ `addRecordToPacket()` - Response construction
8. ✅ `authWaitTimeMSec()` - Timeout calculation
9. ✅ `doResends()` - Error handling

### **Features to DISABLE (Comment Out):**
1. ⚠️ TCP support
2. ⚠️ Lua scripting
3. ⚠️ Protobuf logging
4. ⚠️ FrameStream logging
5. ⚠️ Proxy protocol
6. ⚠️ ECS (EDNS Client Subnet)
7. ⚠️ Policy filtering / RPZ
8. ⚠️ Event tracing
9. ⚠️ NOD / UDR
10. ⚠️ DNS64
11. ⚠️ CNAME following (optional)
12. ⚠️ Query distribution (for single-threaded)

### **Prerequisites:**
1. ✅ Initialize `g_multiTasker`, `t_fdm`, `t_udpclientsocks`
2. ✅ Set configuration variables
3. ✅ Create and register UDP socket
4. ✅ Run event loop
5. ✅ Apply Windows fixes

---

## Example Minimal Setup Code

```cpp
// 1. Initialize globals
g_multiTasker = std::make_unique<MT_t>();
t_fdm = std::unique_ptr<FDMultiplexer>(FDMultiplexer::getMultiplexerSilent());
t_udpclientsocks = std::make_unique<UDPClientSocks>();

// 2. Set configuration
g_networkTimeoutMsec = 2000;
g_maxMThreads = 2048;
g_udpTruncationThreshold = 1232;
g_maxUDPQueriesPerRound = 1;

// 3. Create UDP socket
int socketFd = socket(AF_INET, SOCK_DGRAM, 0);
bind(socketFd, ...);
// Set non-blocking
t_fdm->addReadFD(socketFd, handleNewUDPQuestion, boost::any());

// 4. Run event loop
while (running) {
    gettimeofday(&g_now, nullptr);
    g_multiTasker->schedule(g_now);
    int timeout = g_multiTasker->getRemainingTime(g_now);
    t_fdm->run(&g_now, timeout);
}
```

---

## Notes

- **All optional features can be re-enabled later** by uncommenting code
- **Windows fixes must be applied** regardless of which features are enabled
- **Minimal setup focuses on UDP-only, basic DNS resolution**
- **For production, enable features as needed**

