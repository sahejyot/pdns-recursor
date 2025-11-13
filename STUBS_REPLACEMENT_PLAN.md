# Stubs Replacement Plan: Use Upstream Code Instead

## Overview
This document lists all stub files and custom implementations we created, and how to replace them with upstream code from `pdns-recursor/pdns_recursor.cc`.

---

## 📋 **List of Stubs We Created**

### **1. `pdns_recursor_poc_parts.cc` - Custom UDP Functions**

**Status:** ⚠️ **PARTIALLY UPSTREAM** (some functions match upstream, some are custom)

**Functions in this file:**
1. **`UDPClientSocks::getSocket()`** (lines 82-109)
   - **Upstream:** `pdns-recursor/pdns_recursor.cc:102-128`
   - **Status:** ✅ Matches upstream (with Windows fixes)
   - **Action:** Keep, but ensure Windows fixes are in upstream version

2. **`UDPClientSocks::returnSocket()`** (lines 111-144)
   - **Upstream:** `pdns-recursor/pdns_recursor.cc:131-149`
   - **Status:** ✅ Matches upstream
   - **Action:** Keep, but ensure Windows fixes are in upstream version

3. **`UDPClientSocks::makeClientSocket()`** (lines 146-181)
   - **Upstream:** `pdns-recursor/pdns_recursor.cc:151-180` (likely)
   - **Status:** ✅ Matches upstream
   - **Action:** Keep, but ensure Windows fixes are in upstream version

4. **`handleUDPServerResponse()`** (lines 183-499)
   - **Upstream:** `pdns-recursor/pdns_recursor.cc:2979-3097`
   - **Status:** ⚠️ **CUSTOM** - Has Windows fixes for padding/byte order
   - **Action:** **REPLACE** with upstream version + add Windows fixes

5. **`asendto()`** (lines 501-625)
   - **Upstream:** `pdns-recursor/pdns_recursor.cc:281-342`
   - **Status:** ⚠️ **CUSTOM** - Has Windows fixes
   - **Action:** **REPLACE** with upstream version + add Windows fixes

6. **`arecvfrom()`** (lines 627-794)
   - **Upstream:** `pdns-recursor/pdns_recursor.cc:346-425`
   - **Status:** ⚠️ **CUSTOM** - Simplified version
   - **Action:** **REPLACE** with upstream version

7. **`initializeMTaskerInfrastructure()`** (lines 52-63)
   - **Upstream:** ❌ **NOT IN UPSTREAM** - This is custom initialization
   - **Status:** ⚠️ **CUSTOM** - But needed for our setup
   - **Action:** **KEEP** or move to main initialization

8. **`authWaitTimeMSec()`** (lines 74-79)
   - **Upstream:** `pdns-recursor/pdns_recursor.cc:268-276`
   - **Status:** ⚠️ **SIMPLIFIED** - Our version is minimal
   - **Action:** **REPLACE** with upstream version

---

### **2. `lwres_stubs.cc` - Stub Functions**

**Status:** ❌ **STUBS** (not real implementations)

**Functions:**
1. **`asendtcp()`** (lines 42-47)
   - **Upstream:** `pdns-recursor/pdns_recursor.cc` (TCP functions)
   - **Status:** ❌ **STUB** - Returns `PermanentError`
   - **Action:** **REMOVE** if TCP is disabled, or use upstream if TCP enabled

2. **`arecvtcp()`** (lines 49-53)
   - **Upstream:** `pdns-recursor/pdns_recursor.cc` (TCP functions)
   - **Status:** ❌ **STUB** - Returns `PermanentError`
   - **Action:** **REMOVE** if TCP is disabled, or use upstream if TCP enabled

3. **`asendto()`** (lines 57-62) - **GUARDED WITH `#if !defined(ENABLE_WINDOWS_POC_PARTS)`**
   - **Status:** ❌ **STUB** - Only used if `ENABLE_WINDOWS_POC_PARTS` is not defined
   - **Action:** **REMOVE** - We use real implementation from `pdns_recursor_poc_parts.cc`

4. **`arecvfrom()`** (lines 64-70) - **GUARDED WITH `#if !defined(ENABLE_WINDOWS_POC_PARTS)`**
   - **Status:** ❌ **STUB** - Only used if `ENABLE_WINDOWS_POC_PARTS` is not defined
   - **Action:** **REMOVE** - We use real implementation from `pdns_recursor_poc_parts.cc`

5. **`mthreadSleep()`** (lines 73-77)
   - **Upstream:** `pdns-recursor/pdns_recursor.cc` (likely)
   - **Status:** ⚠️ **MINIMAL** - Simple sleep implementation
   - **Action:** **REPLACE** with upstream version if available

6. **`nsspeeds_t::putPB()` / `nsspeeds_t::getPB()`** (lines 80-88)
   - **Upstream:** `pdns-recursor/rec-nsspeeds.cc` (likely)
   - **Status:** ❌ **STUBS** - Return 0
   - **Action:** **REPLACE** with upstream version or disable feature

7. **`primeHints()`** (lines 91-94)
   - **Upstream:** `pdns-recursor/reczones.cc:39` + `reczones-helpers.cc:124` (`putDefaultHintsIntoCache`)
   - **Status:** ❌ **STUB** - Returns false (PROBLEM: `syncres.cc:2528` calls this when root NS records are lost!)
   - **Action:** **REPLACE** with upstream version - **CRITICAL** - This is actively used by `syncres.cc`
   - **Note:** We also have custom `primeRootHints()` in `main_test.cc:49` for startup, but `primeHints()` is called dynamically

8. **`RecResponseStats` constructor/operator** (lines 96-125)
   - **Upstream:** `pdns-recursor/rec-responsestats.cc`
   - **Status:** ⚠️ **MINIMAL** - Basic implementation
   - **Action:** **REPLACE** with upstream version

9. **`broadcastAccFunction()`** (lines 131-139)
   - **Upstream:** `pdns-recursor/pdns_recursor.cc` or `rec-main.cc`
   - **Status:** ❌ **STUB** - Returns empty T{}
   - **Action:** **REPLACE** with upstream version or disable feature

---

### **3. `globals_stub.cc` - Global Variable Definitions**

**Status:** ⚠️ **PARTIALLY UPSTREAM** (some match, some are custom)

**Variables:**
1. **`g_recCache`** (line 15)
   - **Upstream:** `pdns-recursor/pdns_recursor.cc:52`
   - **Status:** ✅ Matches upstream
   - **Action:** **REPLACE** with upstream definition

2. **`g_negCache`** (line 16)
   - **Upstream:** `pdns-recursor/pdns_recursor.cc:53`
   - **Status:** ✅ Matches upstream
   - **Action:** **REPLACE** with upstream definition

3. **`g_lowercaseOutgoing`** (line 19)
   - **Upstream:** `pdns-recursor/pdns_recursor.cc` (likely)
   - **Status:** ⚠️ **CUSTOM** - May not match upstream
   - **Action:** **CHECK** upstream and replace

4. **`g_networkTimeoutMsec`** (line 20)
   - **Upstream:** `pdns-recursor/pdns_recursor.cc:91`
   - **Status:** ✅ Matches upstream
   - **Action:** **REPLACE** with upstream definition

5. **`g_outgoingEDNSBufsize`** (line 21)
   - **Upstream:** `pdns-recursor/pdns_recursor.cc` (likely)
   - **Status:** ⚠️ **CUSTOM** - May not match upstream
   - **Action:** **CHECK** upstream and replace

6. **`g_maxMThreads`** (line 22)
   - **Upstream:** `pdns-recursor/pdns_recursor.cc` (likely)
   - **Status:** ⚠️ **CUSTOM** - May not match upstream
   - **Action:** **CHECK** upstream and replace

7. **`g_logCommonErrors`** (line 23)
   - **Upstream:** `pdns-recursor/pdns_recursor.cc:80`
   - **Status:** ✅ Matches upstream
   - **Action:** **REPLACE** with upstream definition

8. **DNSSEC-related globals** (lines 26-30)
   - **Upstream:** `pdns-recursor/pdns_recursor.cc` (likely)
   - **Status:** ⚠️ **CUSTOM** - May not match upstream
   - **Action:** **CHECK** upstream and replace

9. **Logger instances** (lines 34-35)
   - **Upstream:** `pdns-recursor/pdns_recursor.cc` or `rec-main.cc`
   - **Status:** ⚠️ **CUSTOM** - May not match upstream
   - **Action:** **CHECK** upstream and replace

10. **`arg()` function** (lines 38-42)
    - **Upstream:** `pdns-recursor/arguments.cc`
    - **Status:** ⚠️ **CUSTOM** - Static instance
    - **Action:** **REPLACE** with upstream version

---

### **4. `dnssec_stubs.cc` - DNSSEC Validation Stubs**

**Status:** ❌ **STUBS** (when DNSSEC is disabled)

**Functions (all guarded with `#if defined(HAVE_DNSSEC) && !HAVE_DNSSEC`):**
1. **`vStateToString()`** (lines 9-13)
2. **`operator<<(vState)`** (lines 15-19)
3. **`operator<<(dState)`** (lines 21-25)
4. **`getSigner()`** (lines 27-30)
5. **`isRRSIGNotExpired()`** (lines 32-35)
6. **`getDenial()`** (lines 37-40)
7. **`updateDNSSECValidationState()`** (lines 42-44)
8. **`increaseXDNSSECStateCounter()`** (lines 46-49)
9. **`increaseDNSSECStateCounter()`** (lines 51-54)
10. **`isSupportedDS()`** (lines 56-59)
11. **`pdns::dedupRecords()`** (lines 61-64)
12. **`validateWithKeySet()`** (lines 68-77)
13. **`isWildcardExpanded()`** (lines 79-82)
14. **`isWildcardExpandedOntoItself()`** (lines 84-87)

**Upstream:** These should be in `pdns-recursor/validate-recursor.cc` or `validate.cc`

**Action:** 
- **KEEP** if DNSSEC is disabled (`HAVE_DNSSEC=0`)
- **REPLACE** with upstream versions if DNSSEC is enabled
- These are legitimate stubs when DNSSEC is disabled

---

## 🎯 **Replacement Strategy**

### **Phase 1: Enable `pdns_recursor.cc` and Replace Functions**

1. **Copy `pdns-recursor/pdns_recursor.cc` to `pdns_recursor_windows/`**
2. **Remove `#if 0` wrapper** (enable the file)
3. **Add Windows fixes** to upstream functions:
   - `handleUDPServerResponse()` - Add Windows padding/byte order fixes
   - `asendto()` - Add Windows fixes if needed
   - `arecvfrom()` - Use upstream version
4. **Comment out optional features** (TCP, Lua, Protobuf, etc.)

### **Phase 2: Replace Stub Files**

1. **Remove `lwres_stubs.cc`** - Functions will come from `pdns_recursor.cc`
2. **Replace `globals_stub.cc`** - Use globals from `pdns_recursor.cc`
3. **Keep `dnssec_stubs.cc`** - Only if DNSSEC is disabled
4. **Remove `pdns_recursor_poc_parts.cc`** - Functions will come from `pdns_recursor.cc`

### **Phase 3: Update Includes and Build**

1. **Update `CMakeLists.txt`** to include `pdns_recursor.cc` instead of stub files
2. **Remove stub files from build**
3. **Fix any missing dependencies**

---

## 📝 **Summary Table**

| File | Functions | Status | Action |
|------|-----------|--------|--------|
| **pdns_recursor_poc_parts.cc** | `UDPClientSocks::getSocket/returnSocket/makeClientSocket` | ✅ Matches upstream | Keep, add Windows fixes to upstream |
| **pdns_recursor_poc_parts.cc** | `handleUDPServerResponse()` | ⚠️ Custom with Windows fixes | **REPLACE** with upstream + Windows fixes |
| **pdns_recursor_poc_parts.cc** | `asendto()` | ⚠️ Custom with Windows fixes | **REPLACE** with upstream + Windows fixes |
| **pdns_recursor_poc_parts.cc** | `arecvfrom()` | ⚠️ Simplified | **REPLACE** with upstream |
| **pdns_recursor_poc_parts.cc** | `initializeMTaskerInfrastructure()` | ⚠️ Custom | **KEEP** or move to main |
| **pdns_recursor_poc_parts.cc** | `authWaitTimeMSec()` | ⚠️ Simplified | **REPLACE** with upstream |
| **lwres_stubs.cc** | `asendtcp/arecvtcp` | ❌ Stubs | **REMOVE** if TCP disabled |
| **lwres_stubs.cc** | `asendto/arecvfrom` (stubs) | ❌ Stubs | **REMOVE** (we use real ones) |
| **lwres_stubs.cc** | `primeHints()` | ❌ Stub (returns false) | **CRITICAL: REPLACE** - Used by `syncres.cc:2528` |
| **lwres_stubs.cc** | `mthreadSleep/nsspeeds/etc.` | ❌ Stubs | **REPLACE** with upstream or disable |
| **globals_stub.cc** | All globals | ⚠️ Partial match | **REPLACE** with upstream definitions |
| **dnssec_stubs.cc** | DNSSEC functions | ❌ Stubs (when disabled) | **KEEP** if DNSSEC disabled, else replace |

---

## 🚀 **Next Steps**

1. **Enable `pdns_recursor.cc`** (remove `#if 0`)
2. **Add Windows fixes** to `handleUDPServerResponse()` and `asendto()` in upstream file
3. **Replace globals** from `globals_stub.cc` with upstream definitions
4. **Remove stub files** from build
5. **Test and verify** everything works

---

## ⚠️ **Important Notes**

- **Windows fixes must be preserved** when replacing with upstream code
- **Use `#ifdef _WIN32` blocks** to isolate Windows-specific code
- **Test thoroughly** after each replacement
- **Keep commented-out code** for reference during transition

