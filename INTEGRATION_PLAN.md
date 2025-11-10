# Integration Plan: Windows UDP Flow into Upstream Main Flow

## Overview
This document explains what needs to be achieved to integrate the Windows UDP flow from `main_test.cc` (POC/test module) into the upstream `rec-main.cc` flow, so it runs in the main recursor service instead of a test module.

---

## Current State

### **Our POC (`main_test.cc`):**
- **Standalone test module** with its own `main()` function
- Creates UDP socket manually
- Initializes caches manually
- Registers socket with `t_fdm` manually
- Runs custom event loop
- Uses `handleDNSQuery()` (simplified version of upstream's `handleNewUDPQuestion()`)

### **Upstream (`rec-main.cc`):**
- **Main service** with proper initialization flow
- UDP sockets created in `makeUDPServerSockets()` → `pdns_recursor.cc:2703`
- Sockets registered via `deferredAdds` mechanism
- Uses `handleNewUDPQuestion()` → `pdns_recursor.cc:2474`
- Runs `recLoop()` → `rec-main.cc:2800`
- Multi-threaded architecture with `RecThreadInfo`

---

## What Needs to Be Achieved

### **1. Remove Standalone `main()` Function**
**Current:** `main_test.cc` has its own `main()` that does everything
**Target:** Integrate into upstream's `serviceMain()` → `rec-main.cc:2196`

**Changes Required:**
- Remove `int main()` from `main_test.cc`
- Move initialization code into appropriate upstream functions
- Ensure Windows-specific initialization (WSAStartup) happens in upstream's init path

---

### **2. Integrate UDP Socket Creation**
**Current:** Manual socket creation in `main_test.cc:516-556`
```cpp
g_udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
bind(g_udp_socket, ...);
t_fdm->addReadFD(g_udp_socket, handleDNSQuery, param);
```

**Upstream:** Sockets created in `makeUDPServerSockets()` → `pdns_recursor.cc:2703`
```cpp
FDWrapper socketFd = socket(...);
bind(...);
deferredAdds.emplace_back(socketFd, handleNewUDPQuestion);
```

**Changes Required:**
- **Option A (Recommended):** Ensure `makeUDPServerSockets()` works on Windows
  - Add Windows-specific socket options (SO_REUSEADDR, non-blocking)
  - Ensure `handleNewUDPQuestion` works with Windows sockets
  - No changes to socket creation flow

- **Option B:** Add Windows-specific socket creation path
  - Create separate function `makeUDPServerSocketsWindows()`
  - Called from `serviceMain()` before `RecThreadInfo::runThreads()`
  - Still uses `deferredAdds` mechanism

---

### **3. Replace `handleDNSQuery()` with `handleNewUDPQuestion()`**
**Current:** Simplified `handleDNSQuery()` in `main_test.cc:163`
- Basic `recvfrom()` 
- Simple wire-format parsing
- Direct `resolveTaskFunc()` call

**Upstream:** Full-featured `handleNewUDPQuestion()` in `pdns_recursor.cc:2474`
- Uses `recvmsg()` (supports ancillary data, IPv6)
- Proxy protocol support
- Multiple queries per round (`g_maxUDPQueriesPerRound`)
- Calls `doProcessUDPQuestion()` → `pdns_recursor.cc:2190`

**Changes Required:**
- **Remove** `handleDNSQuery()` from `main_test.cc`
- **Ensure** `handleNewUDPQuestion()` works on Windows:
  - `recvmsg()` support (Windows has it, but may need adjustments)
  - IPv6 support (`msghdr` structure)
  - Non-blocking socket handling
- **Test** that Windows socket behavior matches Linux

---

### **4. Integrate Event Loop**
**Current:** Custom event loop in `main_test.cc:615-1015`
```cpp
while (true) {
    g_multiTasker->schedule(g_now);
    t_fdm->run(&g_now, timeoutMsec);
    // Windows workarounds (now disabled)
}
```

**Upstream:** `recLoop()` in `rec-main.cc:2800`
```cpp
while (!RecursorControlChannel::stop) {
    while (g_multiTasker->schedule(g_now)) { }
    // Housekeeping tasks
    t_fdm->run(&g_now, timeoutMsec);
}
```

**Changes Required:**
- **Remove** custom event loop from `main_test.cc`
- **Ensure** `recLoop()` works on Windows:
  - No changes needed - it's already platform-agnostic
  - Windows workarounds are disabled (not needed)
- **Verify** `RecursorControlChannel::stop` mechanism works on Windows

---

### **5. Integrate Initialization**
**Current:** Manual initialization in `main_test.cc:498-610`
- Winsock initialization
- Cache initialization
- Root hints priming
- MTasker infrastructure
- SyncRes defaults

**Upstream:** Initialization in `serviceMain()` → `rec-main.cc:2196`
- `initNet()` → network initialization
- `initSyncRes()` → SyncRes initialization
- `makeUDPServerSockets()` → UDP socket creation
- `RecThreadInfo::runThreads()` → thread initialization

**Changes Required:**
- **Move** Winsock initialization to `initNet()` or create `initNetWindows()`
- **Ensure** cache initialization happens in upstream's init path
- **Verify** root hints priming happens in `primeHints()` → `rec-main.cc:2899`
- **Remove** manual `initializeMTaskerInfrastructure()` - upstream handles this in `recursorThread()`

---

### **6. Integrate MTasker Infrastructure**
**Current:** Manual `initializeMTaskerInfrastructure()` in `main_test.cc`
```cpp
g_multiTasker = std::make_unique<MT_t>(...);
t_fdm = FDMultiplexer::getMultiplexerSilent();
t_udpclientsocks = std::make_unique<UDPClientSocks>();
```

**Upstream:** Initialized in `recursorThread()` → `rec-main.cc:2876`
```cpp
g_multiTasker = std::make_unique<MT_t>(...);
t_fdm = unique_ptr<FDMultiplexer>(getMultiplexer(log));
t_udpclientsocks = std::make_unique<UDPClientSocks>();
```

**Changes Required:**
- **Remove** `initializeMTaskerInfrastructure()` from POC
- **Ensure** `getMultiplexer()` returns `LibeventFDMultiplexer` on Windows
- **Verify** `t_fdm` initialization happens per-thread (thread_local)

---

### **7. Integrate Socket Registration**
**Current:** Manual registration in `main_test.cc:605`
```cpp
t_fdm->addReadFD(g_udp_socket, handleDNSQuery, param);
```

**Upstream:** Registration via `deferredAdds` in `recursorThread()` → `rec-main.cc:2982-2990`
```cpp
for (const auto& deferred : s_deferredUDPadds) {
    t_fdm->addReadFD(deferred.first, deferred.second);
}
```

**Changes Required:**
- **Remove** manual `addReadFD()` from `main_test.cc`
- **Ensure** `s_deferredUDPadds` contains Windows sockets
- **Verify** `handleNewUDPQuestion` is registered (not `handleDNSQuery`)

---

### **8. Remove Test-Specific Code**
**Current:** Test/debug code in `main_test.cc`
- `std::cout` debug statements
- Simplified error handling
- Hardcoded port 5533
- Minimal configuration

**Upstream:** Production code
- Proper logging via `g_log` / `g_slog`
- Configuration via `::arg()`
- Configurable ports
- Full error handling

**Changes Required:**
- **Replace** `std::cout` with `SLOG()` / `g_log`
- **Use** `::arg()` for configuration
- **Remove** hardcoded values
- **Add** proper error handling

---

### **9. Windows-Specific Considerations**

#### **9.1 Winsock Initialization**
- **Location:** `initNet()` or new `initNetWindows()`
- **When:** Before any socket operations
- **Cleanup:** `WSACleanup()` in shutdown path

#### **9.2 Socket Options**
- **SO_REUSEADDR:** Already handled in `makeUDPServerSockets()`
- **Non-blocking:** `setNonBlocking()` should work on Windows
- **IPv6:** Ensure `recvmsg()` works with IPv6 on Windows

#### **9.3 Multiplexer Selection**
- **Ensure** `getMultiplexer()` returns `LibeventFDMultiplexer` on Windows
- **Verify** `FDMultiplexer::getMultiplexerSilent()` works correctly

#### **9.4 Thread-Local Variables**
- **Verify** `t_fdm` (thread_local) works correctly on Windows
- **Ensure** each thread gets its own multiplexer instance

---

## Integration Steps (High-Level)

### **Phase 1: Preparation**
1. ✅ Ensure `LibeventFDMultiplexer` works (already done)
2. ✅ Ensure Windows DNS header padding fixes are in place (already done)
3. ✅ Test that `handleNewUDPQuestion()` can work with Windows sockets
4. ✅ Verify `recLoop()` works on Windows

### **Phase 2: Socket Integration**
1. Modify `makeUDPServerSockets()` to handle Windows-specific socket options
2. Ensure `deferredAdds` mechanism works on Windows
3. Test socket creation and binding on Windows

### **Phase 3: Handler Integration**
1. Replace `handleDNSQuery()` with `handleNewUDPQuestion()`
2. Ensure `recvmsg()` works on Windows (or add fallback)
3. Test query reception and parsing

### **Phase 4: Initialization Integration**
1. Add Winsock initialization to `initNet()` or create `initNetWindows()`
2. Remove manual initialization from `main_test.cc`
3. Ensure all initialization happens in upstream's flow

### **Phase 5: Event Loop Integration**
1. Remove custom event loop from `main_test.cc`
2. Ensure `recLoop()` is called from `recursorThread()`
3. Test that event loop works correctly

### **Phase 6: Cleanup**
1. Remove `main()` from `main_test.cc`
2. Remove test-specific code
3. Replace debug output with proper logging
4. Remove hardcoded values

---

## Key Files to Modify

### **Upstream Files (Need Windows Support):**
1. **`pdns-recursor/rec-main.cc`**
   - `serviceMain()` - Add Winsock init
   - `recursorThread()` - Already works, verify Windows
   - `recLoop()` - Already works, verify Windows

2. **`pdns-recursor/pdns_recursor.cc`**
   - `makeUDPServerSockets()` - Add Windows socket options
   - `handleNewUDPQuestion()` - Verify Windows compatibility
   - `doProcessUDPQuestion()` - Already works

3. **`pdns-recursor/resolver.cc`** (if needed)
   - Socket creation for upstream queries
   - Already uses `t_fdm`, should work

### **POC Files (Remove/Integrate):**
1. **`pdns_recursor_windows/main_test.cc`**
   - Remove `main()` function
   - Remove `handleDNSQuery()` (use upstream's)
   - Remove custom event loop
   - Remove manual initialization
   - Keep only Windows-specific fixes (if any)

---

## Testing Strategy

### **Unit Tests:**
- Test socket creation on Windows
- Test `recvmsg()` on Windows
- Test multiplexer on Windows

### **Integration Tests:**
- Test full query flow (incoming → resolution → response)
- Test multiple concurrent queries
- Test error handling

### **Performance Tests:**
- Compare performance with Linux
- Verify no regressions

---

## Success Criteria

1. ✅ **No standalone `main()` function** - integrated into upstream flow
2. ✅ **UDP sockets created via `makeUDPServerSockets()`** - not manually
3. ✅ **Uses `handleNewUDPQuestion()`** - not custom handler
4. ✅ **Uses `recLoop()`** - not custom event loop
5. ✅ **Initialization in upstream's flow** - not manual
6. ✅ **Works with multi-threading** - `RecThreadInfo` architecture
7. ✅ **Proper logging** - not debug output
8. ✅ **Configuration via `::arg()`** - not hardcoded

---

## Risks and Mitigations

### **Risk 1: `recvmsg()` on Windows**
- **Mitigation:** Test thoroughly, add fallback to `recvfrom()` if needed

### **Risk 2: IPv6 Support**
- **Mitigation:** Ensure `msghdr` structure works correctly on Windows

### **Risk 3: Thread-Local Variables**
- **Mitigation:** Verify `t_fdm` (thread_local) works on Windows

### **Risk 4: Multiplexer Compatibility**
- **Mitigation:** Already tested, `LibeventFDMultiplexer` works

---

## Summary

**Main Goal:** Move from standalone test module (`main_test.cc`) to integrated upstream flow (`rec-main.cc`)

**Key Changes:**
1. Remove `main()` → integrate into `serviceMain()`
2. Remove manual socket creation → use `makeUDPServerSockets()`
3. Remove `handleDNSQuery()` → use `handleNewUDPQuestion()`
4. Remove custom event loop → use `recLoop()`
5. Remove manual initialization → use upstream's init flow
6. Add Windows-specific initialization (Winsock) to upstream

**Result:** Windows support fully integrated into upstream codebase, following the same architecture as Linux/Unix.

