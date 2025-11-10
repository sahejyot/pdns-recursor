# Why We Don't Use DNSComboWriter in Our POC

## Overview

**DNSComboWriter** is upstream's structure for passing DNS query context to the resolution function. In our POC, we created a simplified **ResolveJob** structure instead.

---

## What is DNSComboWriter?

**Location:** `pdns-recursor/rec-main.hh:54`

**Purpose:** Container for all DNS query information and metadata

**Contains:**
```cpp
struct DNSComboWriter {
    // Query data
    MOADNSParser d_mdp;              // Parsed DNS query
    std::string d_query;             // Raw query packet
    struct timeval d_now;             // Timestamp
    
    // Address information
    ComboAddress d_remote;            // Client address (where query came from)
    ComboAddress d_source;            // Source address (after proxy protocol)
    ComboAddress d_local;             // Local address (where we received it)
    ComboAddress d_destination;      // Destination address (after proxy protocol)
    ComboAddress d_mappedSource;      // Mapped source (after table-based mapping)
    
    // Socket
    int d_socket;                     // Socket file descriptor
    
    // Query metadata
    uint16_t d_qhash;                 // Query hash
    uint32_t d_tag;                   // Tag for filtering
    bool d_tcp;                       // TCP or UDP?
    bool d_variable;                  // Variable answer size?
    bool d_followCNAMERecords;        // Follow CNAME records?
    bool d_logResponse;               // Log response?
    
    // ECS (EDNS Client Subnet)
    bool d_ecsFound;                  // ECS found in query?
    bool d_ecsParsed;                 // ECS parsed?
    EDNSSubnetOpts d_ednssubnet;      // ECS data
    
    // Policy and filtering
    std::unordered_set<std::string> d_policyTags;
    std::unordered_set<std::string> d_gettagPolicyTags;
    std::vector<DNSRecord> d_records; // Pre-set records (from policy)
    boost::optional<int> d_rcode;     // Pre-set RCODE (from policy)
    
    // Lua context
    shared_ptr<RecursorLua4> d_luaContext;
    LuaContext::LuaObject d_data;
    
    // TTL cap
    uint32_t d_ttlCap;
    
    // Extended error codes
    boost::optional<uint16_t> d_extendedErrorCode;
    std::string d_extendedErrorExtra;
    
    // Proxy protocol
    std::vector<ProxyProtocolValue> d_proxyProtocolValues;
    
    // UUID for protobuf logging
    std::string d_uuid;
    
    // Device information
    std::string d_requestorId;
    std::string d_deviceId;
    std::string d_deviceName;
    
    // Routing
    std::string d_routingTag;
    
    // Event tracing
    RecEventTrace d_eventTrace;
    pdns::trace::InitialSpanInfo d_otTrace;
    
    // Kernel timestamp
    struct timeval d_kernelTimestamp;
    
    // Response padding
    bool d_responsePaddingDisabled;
    
    // Meta data
    std::map<std::string, std::string> d_meta;
};
```

**Usage in Upstream:**
```cpp
// In handleNewUDPQuestion() or doProcessUDPQuestion()
auto comboWriter = std::make_unique<DNSComboWriter>(question, g_now, ...);
comboWriter->setRemote(fromaddr);
comboWriter->setSource(source);
// ... set many fields ...
g_multiTasker->makeThread(startDoResolve, comboWriter.get());

// In startDoResolve()
auto comboWriter = std::unique_ptr<DNSComboWriter>(static_cast<DNSComboWriter*>(arg));
SyncRes resolver(comboWriter->d_now);
// Use comboWriter->d_mdp.d_qname, comboWriter->d_mdp.d_qtype, etc.
```

---

## What We Use Instead: ResolveJob

**Location:** `main_test.cc:149-157`

**Purpose:** Simplified container for minimal query information

**Contains:**
```cpp
struct ResolveJob {
    evutil_socket_t sock;      // Socket for sending response
    sockaddr_in to;            // Client address (where to send response)
    std::string fqdn;          // Query name
    uint16_t qtype;            // Query type
    uint16_t qclass;           // Query class
    uint16_t qid;              // Query ID
    uint8_t rd;                // Recursion desired flag
};
```

**Usage in Our POC:**
```cpp
// In handleDNSQuery()
auto* job = new ResolveJob{ sendSock, replyAddr, fqdn, qtypeWire, qclassWire, qid, rdflag };
g_multiTasker->makeThread(resolveTaskFunc, job);

// In resolveTaskFunc()
std::unique_ptr<ResolveJob> j(static_cast<ResolveJob*>(pv));
// Use j->fqdn, j->qtype, j->qid, etc.
```

---

## Why We Didn't Use DNSComboWriter

### **1. Complexity**

**DNSComboWriter has 30+ fields** covering:
- Proxy protocol support
- ECS (EDNS Client Subnet)
- Policy filtering (RPZ)
- Lua scripting
- Event tracing
- Protobuf logging
- Device tracking
- Response padding
- Extended error codes

**ResolveJob has 7 fields** covering:
- Basic query information
- Response destination

**For a POC:** We only need basic functionality, not all upstream features.

---

### **2. Dependencies**

**DNSComboWriter requires:**
- `MOADNSParser` - Full DNS packet parser
- `ComboAddress` - Address handling (IPv4/IPv6)
- `RecursorLua4` - Lua scripting engine
- `EDNSSubnetOpts` - ECS support
- `ProxyProtocolValue` - Proxy protocol support
- `RecEventTrace` - Event tracing
- `pdns::trace::InitialSpanInfo` - OpenTelemetry tracing
- Many other upstream structures

**ResolveJob requires:**
- Basic C++ types (`std::string`, `uint16_t`, etc.)
- Standard socket types (`sockaddr_in`)

**For a POC:** We want minimal dependencies to test core functionality.

---

### **3. Parsing Approach**

**Upstream:**
- Uses `MOADNSParser` to parse full DNS packet
- Stores parsed query in `comboWriter->d_mdp`
- Accesses fields via `comboWriter->d_mdp.d_qname`, `comboWriter->d_mdp.d_qtype`, etc.

**Our POC:**
- Uses simple wire-format parser (`parseWireQuestion()`)
- Extracts only needed fields (qname, qtype, qclass, qid, rd)
- Stores in simple `ResolveJob` structure

**Reason:** We had issues with `MOADNSParser` on Windows (struct padding), so we bypassed it.

---

### **4. Feature Scope**

**Upstream features we don't need for POC:**
- ❌ Proxy protocol
- ❌ ECS (EDNS Client Subnet)
- ❌ Policy filtering (RPZ)
- ❌ Lua scripting
- ❌ Event tracing
- ❌ Protobuf logging
- ❌ Device tracking
- ❌ Response padding
- ❌ Extended error codes
- ❌ IPv6 (initially)

**What we need:**
- ✅ Basic query (qname, qtype, qclass)
- ✅ Query ID (for response matching)
- ✅ Recursion desired flag
- ✅ Response destination (socket + address)

---

### **5. Windows Compatibility**

**DNSComboWriter issues:**
- Uses `MOADNSParser` which had struct padding issues on Windows
- Requires many upstream structures that may have Windows compatibility issues
- More complex = more potential Windows-specific bugs

**ResolveJob benefits:**
- Simple structure = no padding issues
- Uses basic types = no compatibility issues
- Easy to debug

---

## Comparison

| Aspect | DNSComboWriter (Upstream) | ResolveJob (Our POC) |
|--------|---------------------------|----------------------|
| **Fields** | 30+ fields | 7 fields |
| **Dependencies** | Many (MOADNSParser, ComboAddress, Lua, etc.) | Minimal (basic C++ types) |
| **Parsing** | MOADNSParser (full packet) | Simple wire parser (minimal) |
| **Features** | Full-featured (proxy, ECS, policy, Lua, etc.) | Basic (query + response) |
| **Windows Issues** | Potential (struct padding, dependencies) | None (simple structure) |
| **Complexity** | High | Low |
| **Purpose** | Production (all features) | POC (core functionality) |

---

## When to Use DNSComboWriter

**For full integration into upstream:**
- ✅ Use `DNSComboWriter` when integrating into upstream codebase
- ✅ Required for all upstream features (proxy, ECS, policy, etc.)
- ✅ Matches upstream architecture exactly

**For POC/testing:**
- ✅ Use `ResolveJob` for minimal testing
- ✅ Easier to debug
- ✅ Fewer dependencies
- ✅ Faster to implement

---

## Migration Path

**To integrate with upstream:**

1. **Replace `ResolveJob` with `DNSComboWriter`:**
   ```cpp
   // OLD
   struct ResolveJob { ... };
   auto* job = new ResolveJob{ ... };
   
   // NEW
   auto comboWriter = std::make_unique<DNSComboWriter>(question, g_now, ...);
   comboWriter->setRemote(fromaddr);
   // ... set other fields ...
   ```

2. **Use `MOADNSParser` instead of wire parser:**
   ```cpp
   // OLD
   parseWireQuestion(buffer, size, fqdn, qtype, qclass);
   
   // NEW
   MOADNSParser mdp(true, question);
   // Access via mdp.d_qname, mdp.d_qtype, etc.
   ```

3. **Update `resolveTaskFunc` to match `startDoResolve`:**
   ```cpp
   // OLD
   static void resolveTaskFunc(void* pv) {
       std::unique_ptr<ResolveJob> j(...);
       // Use j->fqdn, j->qtype, etc.
   }
   
   // NEW
   static void startDoResolve(void* arg) {
       auto comboWriter = std::unique_ptr<DNSComboWriter>(...);
       // Use comboWriter->d_mdp.d_qname, comboWriter->d_mdp.d_qtype, etc.
   }
   ```

---

## Summary

**We didn't "remove" DNSComboWriter** - we never used it in the first place.

**Why:**
1. **Too complex** for POC (30+ fields vs 7 fields)
2. **Too many dependencies** (MOADNSParser, ComboAddress, Lua, etc.)
3. **Windows compatibility issues** (MOADNSParser struct padding)
4. **Unnecessary features** (proxy, ECS, policy, Lua, etc.)
5. **Simpler is better** for testing core functionality

**For integration:** We'll need to switch to `DNSComboWriter` to match upstream architecture and support all features.

