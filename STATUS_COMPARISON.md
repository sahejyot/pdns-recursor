# Status: What We Have vs What's Needed

## Overview
Comparison of what we've already implemented in our POC vs what's required for using upstream `pdns_recursor.cc`.

---

## ✅ ALREADY DONE

### **1. Infrastructure Initialization** ✅

**Status:** ✅ **COMPLETE**

**What We Have:**
- `initializeMTaskerInfrastructure()` in `pdns_recursor_poc_parts.cc:52-63`
- Initializes `g_multiTasker`, `t_fdm`, `t_udpclientsocks`
- Called in `main_test.cc:615`

**Code:**
```cpp
// pdns_recursor_poc_parts.cc
void initializeMTaskerInfrastructure() {
  if (!g_multiTasker) {
    g_multiTasker = std::make_unique<MT_t>();
  }
  if (!t_fdm) {
    t_fdm = std::unique_ptr<FDMultiplexer>(FDMultiplexer::getMultiplexerSilent());
  }
  if (!t_udpclientsocks) {
    t_udpclientsocks = std::make_unique<UDPClientSocks>();
  }
}
```

---

### **2. Configuration Variables** ✅

**Status:** ✅ **COMPLETE**

**What We Have:**
- `g_networkTimeoutMsec = 1500` in `globals_stub.cc:20`
- `g_maxMThreads = 2048` in `globals_stub.cc:22`
- `g_udpTruncationThreshold` - used in code (defaults applied)
- `g_logCommonErrors` - used in code

**Location:** `pdns_recursor_windows/globals_stub.cc`

---

### **3. Event Loop** ✅

**Status:** ✅ **COMPLETE**

**What We Have:**
- Event loop in `main_test.cc:648-1015`
- Matches upstream pattern: `g_multiTasker->schedule()` + `t_fdm->run()`
- Updates `g_now` with `gettimeofday()`

**Code:**
```cpp
// main_test.cc:660-823
while (g_multiTasker && g_multiTasker->schedule(g_now)) {
    // Process tasks
}
int timeoutMsec = g_multiTasker->getRemainingTime(g_now);
t_fdm->run(&g_now, timeoutMsec);
```

---

### **4. Socket Creation & Registration** ✅

**Status:** ✅ **COMPLETE**

**What We Have:**
- UDP socket creation in `main_test.cc:552-640`
- Socket registered with `t_fdm->addReadFD()`
- Non-blocking socket setup
- Windows-specific socket options

**Code:**
```cpp
// main_test.cc:552-640
g_udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
bind(g_udp_socket, ...);
t_fdm->addReadFD(g_udp_socket, handleDNSQuery, param);
```

---

### **5. Upstream Response Handling** ✅

**Status:** ✅ **COMPLETE** (with Windows fixes)

**What We Have:**
- `handleUDPServerResponse()` in `pdns_recursor_poc_parts.cc:200-450`
- Windows fixes applied (struct padding, byte order)
- Matches upstream function signature
- Used by `asendto()` via `t_fdm->addReadFD()`

**Location:** `pdns_recursor_poc_parts.cc:200-450`

---

### **6. Outgoing Query Functions** ✅

**Status:** ✅ **COMPLETE** (with Windows fixes)

**What We Have:**
- `asendto()` in `pdns_recursor_poc_parts.cc:140-180`
- `arecvfrom()` - handled via MTasker `waitEvent()`
- `UDPClientSocks::getSocket()` in `pdns_recursor_poc_parts.cc:82-130`
- `UDPClientSocks::returnSocket()` in `pdns_recursor_poc_parts.cc:131-139`
- Windows fixes in `lwres.cc` for query construction

**Location:** `pdns_recursor_poc_parts.cc`

---

### **7. Response Construction** ✅

**Status:** ✅ **COMPLETE** (with Windows fixes)

**What We Have:**
- `DNSPacketWriter` usage in `main_test.cc:358-449`
- Windows fixes for header field writes (direct wire writes)
- `startRecord()` + `commit()` pattern matches upstream
- Record adding loop matches upstream

**Location:** `main_test.cc:358-449`

---

### **8. Windows Fixes** ✅

**Status:** ✅ **COMPLETE**

**What We Have:**
- ✅ `dnsparser.cc` - MOADNSParser Windows fixes
- ✅ `dnswriter.cc` - DNSPacketWriter constructor Windows fixes
- ✅ `lwres.cc` - Outgoing query construction Windows fixes
- ✅ `main_test.cc` - Response construction Windows fixes
- ✅ `pdns_recursor_poc_parts.cc` - Response parsing Windows fixes

**All fixes documented in:** `WINDOWS_FIXES_SUMMARY.md`

---

## ❌ NOT DONE / NEEDS TO BE DONE

### **1. Use Upstream Functions Instead of Custom** ❌

**Status:** ❌ **NEEDS REPLACEMENT**

**What We Have (Custom):**
- `resolveTaskFunc()` in `main_test.cc:288-527` - Custom resolution function
- `handleDNSQuery()` in `main_test.cc:163-280` - Custom query handler

**What We Need (Upstream):**
- `startDoResolve()` from `pdns_recursor.cc:973` - Upstream resolution function
- `handleNewUDPQuestion()` from `pdns_recursor.cc:2474` - Upstream query handler
- `doProcessUDPQuestion()` from `pdns_recursor.cc:2190` - Upstream query processor

**Action Required:**
1. Replace `handleDNSQuery()` with `handleNewUDPQuestion()`
2. Replace `resolveTaskFunc()` with `startDoResolve()`
3. Use `doProcessUDPQuestion()` to process queries
4. Update function signatures and parameters

---

### **2. Use DNSComboWriter Instead of ResolveJob** ❌

**Status:** ❌ **NEEDS REPLACEMENT**

**What We Have:**
- `ResolveJob` struct in `main_test.cc:148-157` - Custom simple structure
- 7 fields: `sock`, `to`, `fqdn`, `qtype`, `qclass`, `qid`, `rd`

**What We Need:**
- `DNSComboWriter` from `rec-main.hh:54` - Upstream structure
- 30+ fields including `d_mdp` (MOADNSParser), addresses, metadata

**Action Required:**
1. Include `rec-main.hh`
2. Replace `ResolveJob` with `DNSComboWriter`
3. Update query creation to use `DNSComboWriter` constructor
4. Update resolution function to use `comboWriter->d_mdp.d_qname`, etc.

---

### **3. Use MOADNSParser Directly** ⚠️

**Status:** ⚠️ **PARTIALLY DONE**

**What We Have:**
- MOADNSParser tested in `main_test.cc:229-244` with fallback
- Windows fixes applied in `dnsparser.cc`
- Still using wire parser as fallback

**What We Need:**
- Use MOADNSParser as primary parser (no fallback)
- Access via `comboWriter->d_mdp` (already parsed in DNSComboWriter)

**Action Required:**
1. Remove wire parser fallback
2. Use `comboWriter->d_mdp.d_qname` directly
3. Trust MOADNSParser (it's fixed for Windows)

---

### **4. Include Original pdns_recursor.cc** ❌

**Status:** ❌ **NOT DONE**

**What We Have:**
- `pdns_recursor_windows/pdns_recursor.cc` - Currently disabled with `#if 0`
- Custom functions in `main_test.cc` and `pdns_recursor_poc_parts.cc`

**What We Need:**
- Copy `pdns-recursor/pdns_recursor.cc` to `pdns_recursor_windows/`
- Enable it (remove `#if 0`)
- Add Windows fixes to upstream functions
- Include it in build

**Action Required:**
1. Copy upstream file
2. Add Windows fixes with `#ifdef _WIN32` blocks
3. Comment out optional features (TCP, Lua, etc.)
4. Update CMakeLists.txt to include it

---

### **5. Apply Windows Fixes to Upstream Functions** ❌

**Status:** ❌ **NOT DONE**

**What We Need:**
- Windows fix in `startDoResolve()` - Response construction (lines ~1070-1076)
- Windows fix in `handleUDPServerResponse()` - Response parsing (lines ~3017-3022)
- Windows fix in `doProcessUDPQuestion()` - If needed

**Action Required:**
1. Add `#ifdef _WIN32` blocks in `startDoResolve()` for header field writes
2. Add `#ifdef _WIN32` blocks in `handleUDPServerResponse()` for header parsing
3. Test that fixes work correctly

---

### **6. Comment Out Optional Features** ❌

**Status:** ❌ **NOT DONE**

**What We Need:**
- Comment out TCP support
- Comment out Lua scripting
- Comment out Protobuf logging
- Comment out FrameStream logging
- Comment out Proxy protocol
- Comment out ECS
- Comment out Policy filtering
- Comment out Event tracing
- Comment out NOD/UDR
- Comment out DNS64
- Comment out CNAME following (optional)
- Comment out Query distribution (for single-threaded)

**Action Required:**
1. Wrap optional features in `#if 0` blocks
2. Keep only essential UDP flow
3. Document what's commented out

---

### **7. Update Includes and Dependencies** ❌

**Status:** ❌ **NOT DONE**

**What We Need:**
- Include `rec-main.hh` in `main_test.cc`
- Include `pdns_recursor.cc` (or link it)
- Ensure all upstream dependencies are available
- Remove custom includes that are replaced

**Action Required:**
1. Add `#include "rec-main.hh"` to `main_test.cc`
2. Ensure `pdns_recursor.cc` is compiled and linked
3. Remove unused includes
4. Fix any missing dependencies

---

### **8. Update Main Function** ❌

**Status:** ❌ **NOT DONE**

**What We Have:**
- Custom `main()` in `main_test.cc:532-1064`
- Manual initialization
- Custom event loop

**What We Need:**
- Either use upstream's main flow OR
- Keep custom main but use upstream functions
- Ensure initialization matches upstream pattern

**Action Required:**
1. Decide: Use upstream main OR keep custom main
2. If keeping custom main, update to use upstream functions
3. Ensure initialization order matches upstream

---

## Summary Table

| Component | Status | Location | Action Needed |
|-----------|--------|----------|---------------|
| **Infrastructure Init** | ✅ Done | `pdns_recursor_poc_parts.cc:52` | None |
| **Config Variables** | ✅ Done | `globals_stub.cc` | None |
| **Event Loop** | ✅ Done | `main_test.cc:648` | None |
| **Socket Creation** | ✅ Done | `main_test.cc:552` | None |
| **handleUDPServerResponse** | ✅ Done | `pdns_recursor_poc_parts.cc:200` | Move to pdns_recursor.cc |
| **asendto/arecvfrom** | ✅ Done | `pdns_recursor_poc_parts.cc:140` | Move to pdns_recursor.cc |
| **UDPClientSocks** | ✅ Done | `pdns_recursor_poc_parts.cc:82` | Move to pdns_recursor.cc |
| **Windows Fixes** | ✅ Done | Multiple files | Apply to upstream functions |
| **startDoResolve** | ❌ Missing | - | Use from pdns_recursor.cc |
| **handleNewUDPQuestion** | ❌ Missing | - | Use from pdns_recursor.cc |
| **doProcessUDPQuestion** | ❌ Missing | - | Use from pdns_recursor.cc |
| **DNSComboWriter** | ❌ Missing | - | Replace ResolveJob |
| **MOADNSParser (primary)** | ⚠️ Partial | `main_test.cc:229` | Remove fallback |
| **Include pdns_recursor.cc** | ❌ Missing | - | Copy and enable |
| **Comment Optional Features** | ❌ Missing | - | Wrap in #if 0 |
| **Update Includes** | ❌ Missing | - | Add rec-main.hh |

---

## Next Steps (Priority Order)

### **Phase 1: Copy and Enable Upstream File**
1. Copy `pdns-recursor/pdns_recursor.cc` → `pdns_recursor_windows/pdns_recursor.cc`
2. Remove `#if 0` wrapper (enable the file)
3. Add Windows fixes to essential functions
4. Comment out optional features

### **Phase 2: Replace Custom Functions**
1. Replace `handleDNSQuery()` with `handleNewUDPQuestion()`
2. Replace `resolveTaskFunc()` with `startDoResolve()`
3. Use `doProcessUDPQuestion()` for query processing

### **Phase 3: Use DNSComboWriter**
1. Include `rec-main.hh`
2. Replace `ResolveJob` with `DNSComboWriter`
3. Update query creation to use DNSComboWriter constructor
4. Update resolution to use `comboWriter->d_mdp.*`

### **Phase 4: Cleanup**
1. Remove custom functions from `main_test.cc`
2. Move `handleUDPServerResponse` from poc_parts to pdns_recursor.cc
3. Update includes
4. Test and verify

---

## Estimated Effort

- **Phase 1:** 2-3 hours (copy file, add fixes, comment features)
- **Phase 2:** 3-4 hours (replace functions, update signatures)
- **Phase 3:** 2-3 hours (replace ResolveJob, update code)
- **Phase 4:** 1-2 hours (cleanup, testing)

**Total:** ~8-12 hours of work

---

## Notes

- **We have most infrastructure ready** - initialization, event loop, socket management
- **Main work is replacing custom functions** with upstream equivalents
- **Windows fixes are already done** - just need to apply them to upstream functions
- **Optional features can be disabled incrementally** - start with essential UDP flow

