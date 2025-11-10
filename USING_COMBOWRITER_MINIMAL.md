# Using DNSComboWriter with Minimal Features

## Overview

Yes, we **can** use `DNSComboWriter` and just ignore the fields we don't need! The unused fields will have default values and won't cause issues.

---

## Current Issue: MOADNSParser

**The main blocker was `MOADNSParser`** which is required by `DNSComboWriter` constructor:

```cpp
DNSComboWriter(const std::string& query, const struct timeval& now, ...) :
    d_mdp(true, query),  // <-- This creates MOADNSParser
    ...
```

**Problem:** `MOADNSParser` constructor calls `init()` which does:
```cpp
memcpy(&d_header, packet.data(), sizeof(dnsheader));  // sizeof = 14 on Windows!
```

**But we already fixed this!** Looking at `pdns_recursor_windows/dnsparser.cc:254-276`, we have a Windows-specific fix that handles the padding correctly.

---

## Solution: Use DNSComboWriter with Minimal Setup

### **Step 1: Verify MOADNSParser Works**

**Check:** Our `MOADNSParser::init()` already handles Windows padding:
```cpp
// pdns_recursor_windows/dnsparser.cc:254
void MOADNSParser::init(bool query, const std::string_view& packet) {
    constexpr size_t DNS_HEADER_WIRE_SIZE = 12;
    if (packet.size() < DNS_HEADER_WIRE_SIZE)
        throw MOADNSException("Packet shorter than minimal header");
    
    // Windows fix: Copy header correctly (12 bytes wire → 14 bytes struct)
    // ... (handles padding)
}
```

**If this works**, we can use `MOADNSParser` and thus `DNSComboWriter`!

---

### **Step 2: Create DNSComboWriter with Minimal Data**

**In `handleDNSQuery()`:**

```cpp
void handleDNSQuery(int fd, boost::any& param) {
    // ... receive packet ...
    
    std::string queryPacket(buffer, bytes_received);
    timeval now{};
    Utility::gettimeofday(&now, nullptr);
    
    // Create DNSComboWriter with minimal setup
    // Pass nullptr for Lua context (we don't need it)
    auto comboWriter = std::make_unique<DNSComboWriter>(
        queryPacket, 
        now, 
        nullptr  // No Lua context needed
    );
    
    // Set only the fields we need
    comboWriter->setSocket(fd);
    
    // Convert sockaddr_in to ComboAddress
    ComboAddress remote;
    remote.sin4.sin_family = AF_INET;
    remote.sin4.sin_addr = client_addr.sin_addr;
    remote.sin4.sin_port = client_addr.sin_port;
    comboWriter->setRemote(remote);
    comboWriter->setSource(remote);  // Same as remote for now
    comboWriter->setLocal(remote);    // Same for now
    
    // Set TCP flag (false for UDP)
    comboWriter->d_tcp = false;
    
    // All other fields have default values and can be ignored:
    // - d_policyTags: empty by default
    // - d_luaContext: nullptr (we passed nullptr)
    // - d_ednssubnet: empty by default
    // - d_extendedErrorCode: boost::none by default
    // - etc.
    
    // Pass to MTasker
    g_multiTasker->makeThread(resolveTaskFunc, comboWriter.get());
}
```

---

### **Step 3: Update resolveTaskFunc to Use DNSComboWriter**

**Replace `ResolveJob` with `DNSComboWriter`:**

```cpp
// OLD
static void resolveTaskFunc(void* pv) {
    std::unique_ptr<ResolveJob> j(static_cast<ResolveJob*>(pv));
    // Use j->fqdn, j->qtype, etc.
}

// NEW
static void resolveTaskFunc(void* pv) {
    auto comboWriter = std::unique_ptr<DNSComboWriter>(
        static_cast<DNSComboWriter*>(pv)
    );
    
    // Use comboWriter->d_mdp.d_qname instead of j->fqdn
    // Use comboWriter->d_mdp.d_qtype instead of j->qtype
    // Use comboWriter->d_mdp.d_qclass instead of j->qclass
    // Use comboWriter->d_mdp.d_header.id instead of j->qid
    // Use comboWriter->d_mdp.d_header.rd instead of j->rd
    
    // Get socket and address
    int sock = comboWriter->d_socket;
    ComboAddress remote = comboWriter->d_remote;
    
    // Create SyncRes
    SyncRes resolver(comboWriter->d_now);
    
    // ... rest of resolution logic ...
    
    // Send response
    sendto(sock, response.data(), response.size(), 0,
           (struct sockaddr*)&remote, sizeof(remote));
}
```

---

## Benefits of Using DNSComboWriter

### **1. Matches Upstream Architecture**
- ✅ Uses same structure as upstream
- ✅ Easier to integrate later
- ✅ Compatible with upstream code

### **2. Access to Parsed Data**
- ✅ `comboWriter->d_mdp.d_qname` - Already parsed DNSName
- ✅ `comboWriter->d_mdp.d_qtype` - Already parsed QType
- ✅ `comboWriter->d_mdp.d_header` - Full DNS header
- ✅ No need for manual wire parsing

### **3. Future-Proof**
- ✅ Easy to add features later (just set the fields)
- ✅ Compatible with upstream `startDoResolve()` function
- ✅ Can gradually enable features as needed

### **4. Cleaner Code**
- ✅ No need for `ResolveJob` structure
- ✅ Uses upstream's parsing (MOADNSParser)
- ✅ Standardized approach

---

## What We Need to Check

### **1. MOADNSParser Works on Windows**

**Test:** Try creating `MOADNSParser` with a query packet:

```cpp
// Test code
std::string testPacket = "\x12\x34\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00"
                         "\x03www\x07example\x03com\x00\x00\x01\x00\x01";
try {
    MOADNSParser mdp(true, testPacket);
    std::cout << "MOADNSParser works! qname=" << mdp.d_qname << std::endl;
} catch (const std::exception& e) {
    std::cerr << "MOADNSParser failed: " << e.what() << std::endl;
}
```

**If this works**, we can use `DNSComboWriter`!

---

### **2. Required Dependencies**

**Check if we have:**
- ✅ `ComboAddress` - For address handling
- ✅ `MOADNSParser` - For packet parsing (already fixed)
- ✅ `DNSComboWriter` - The struct itself
- ⚠️ `RecursorLua4` - Can pass `nullptr` if not needed

---

## Implementation Plan

### **Phase 1: Test MOADNSParser**
1. Create test code to verify `MOADNSParser` works
2. If it works, proceed to Phase 2
3. If it fails, fix the issue first

### **Phase 2: Replace ResolveJob with DNSComboWriter**
1. Update `handleDNSQuery()` to create `DNSComboWriter`
2. Update `resolveTaskFunc()` to use `DNSComboWriter`
3. Remove `ResolveJob` structure
4. Test basic query resolution

### **Phase 3: Clean Up**
1. Remove `parseWireQuestion()` (use `MOADNSParser` instead)
2. Remove `ResolveJob` structure
3. Update comments

---

## Code Changes Required

### **1. Include Headers**

```cpp
#include "rec-main.hh"  // For DNSComboWriter
#include "iputils.hh"   // For ComboAddress
```

### **2. Update handleDNSQuery()**

```cpp
void handleDNSQuery(int fd, boost::any& param) {
    // ... receive packet ...
    
    std::string queryPacket(buffer, bytes_received);
    timeval now{};
    Utility::gettimeofday(&now, nullptr);
    
    // Create DNSComboWriter
    auto comboWriter = std::make_unique<DNSComboWriter>(
        queryPacket, now, nullptr  // No Lua
    );
    
    // Set required fields
    comboWriter->setSocket(fd);
    ComboAddress remote;
    // ... convert client_addr to ComboAddress ...
    comboWriter->setRemote(remote);
    comboWriter->setSource(remote);
    comboWriter->d_tcp = false;
    
    // Pass to MTasker
    g_multiTasker->makeThread(resolveTaskFunc, comboWriter.get());
    comboWriter.release();  // MTasker takes ownership
}
```

### **3. Update resolveTaskFunc()**

```cpp
static void resolveTaskFunc(void* pv) {
    auto comboWriter = std::unique_ptr<DNSComboWriter>(
        static_cast<DNSComboWriter*>(pv)
    );
    
    // Use parsed data from MOADNSParser
    DNSName qname = comboWriter->d_mdp.d_qname;
    QType qtype = QType(comboWriter->d_mdp.d_qtype);
    QClass qclass = QClass(comboWriter->d_mdp.d_qclass);
    uint16_t qid = comboWriter->d_mdp.d_header.id;
    bool rd = comboWriter->d_mdp.d_header.rd;
    
    // Create resolver
    SyncRes resolver(comboWriter->d_now);
    
    // ... resolution logic ...
    
    // Send response
    sendto(comboWriter->d_socket, ...);
}
```

---

## Summary

**Yes, we can use `DNSComboWriter` with minimal features!**

**Requirements:**
1. ✅ Verify `MOADNSParser` works (should work with our padding fix)
2. ✅ Create `DNSComboWriter` with `nullptr` for Lua
3. ✅ Set only required fields (socket, remote, source, tcp)
4. ✅ Ignore unused fields (they have safe defaults)

**Benefits:**
- ✅ Matches upstream architecture
- ✅ Uses upstream parsing (MOADNSParser)
- ✅ Future-proof (easy to add features)
- ✅ Cleaner code

**Next Step:** Test if `MOADNSParser` works with our padding fix, then replace `ResolveJob` with `DNSComboWriter`.

