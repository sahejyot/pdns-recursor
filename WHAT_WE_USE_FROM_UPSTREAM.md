# What We're Actually Using From Upstream

## Overview

Despite having custom code and stubs, we **ARE using a significant amount of upstream code**. The core DNS resolution engine and data structures come from upstream.

---

## ✅ **Core Components We Use From Upstream**

### **1. SyncRes - The Core DNS Resolver** ⭐ **MOST IMPORTANT**

**Location:** `syncres.hh` / `syncres.cc` (upstream)

**What it does:**
- Performs iterative DNS resolution
- Queries root servers, TLD servers, authoritative servers
- Handles caching, negative caching
- DNSSEC validation
- CNAME following
- All the complex DNS resolution logic

**How we use it:**
```cpp
// main_test.cc:307-470
SyncRes resolver(now);
resolver.beginResolve(DNSName(j->fqdn), QType(j->qtype), QClass(j->qclass), ret);
// resolver does ALL the DNS resolution work!
```

**This is the HEART of the DNS resolver** - we're using it directly from upstream!

---

### **2. MTasker - Coroutine/Async Task System**

**Location:** `mtasker.hh` (upstream)

**What it does:**
- Provides coroutine-based async I/O
- Allows blocking operations (like DNS queries) to yield and resume
- Manages task scheduling

**How we use it:**
```cpp
// main_test.cc:41-42
typedef MTasker<std::shared_ptr<PacketID>, PacketBuffer, PacketIDCompare> MT_t;
extern thread_local std::unique_ptr<MT_t> g_multiTasker;

// Use it for async DNS queries
g_multiTasker->makeThread(resolveTaskFunc, job);
g_multiTasker->schedule(g_now);
```

**This enables async DNS resolution** - we're using upstream's coroutine system!

---

### **3. Cache Classes - MemRecursorCache & NegCache**

**Location:** `recursor_cache.hh` / `negcache.hh` (upstream)

**What they do:**
- Store DNS records (positive cache)
- Store negative responses (NXDOMAIN, etc.)
- TTL management
- Cache replacement policies

**How we use it:**
```cpp
// main_test.cc:36-37, 560-567
extern std::unique_ptr<MemRecursorCache> g_recCache;
extern std::unique_ptr<NegCache> g_negCache;

g_recCache->replace(now, DNSName(templ), QType::A, {arr}, ...);
g_recCache->get(now, DNSName(j->fqdn), QType(j->qtype), ret, ...);
```

**SyncRes uses these caches** - we're using upstream's caching system!

---

### **4. DNS Data Types - DNSName, QType, QClass, DNSRecord**

**Location:** `dnsname.hh`, `qtype.hh`, `dnsrecords.hh` (upstream)

**What they do:**
- Represent DNS names, types, classes, records
- Handle DNS name encoding/decoding
- Type-safe DNS operations

**How we use it:**
```cpp
// main_test.cc:72-73, 307-470
DNSName(templ)
QType::A, QType::NS
DNSRecord, DNSResourceRecord
NSRecordContent, ARecordContent
```

**All DNS data structures** come from upstream!

---

### **5. DNSPacketWriter - Packet Construction**

**Location:** `dnswriter.hh` / `dnswriter.cc` (upstream, with Windows fixes)

**What it does:**
- Constructs DNS response packets
- Handles DNS name compression
- Manages packet sections (ANSWER, AUTHORITY, ADDITIONAL)

**How we use it:**
```cpp
// main_test.cc:350-470
DNSPacketWriter w(resp, DNSName(j->fqdn), QType(j->qtype), QClass(j->qclass));
w.startRecord(DNSName(j->fqdn), QType(j->qtype), ttl, QClass::IN, DNSResourceRecord::ANSWER);
w.commit();
```

**We use upstream's packet writer** (with Windows padding fixes)!

---

### **6. MOADNSParser - Packet Parsing**

**Location:** `dnsparser.hh` / `dnsparser.cc` (upstream, with Windows fixes)

**What it does:**
- Parses incoming DNS packets
- Extracts question, answers, authority, additional sections
- Handles DNS name decompression

**How we use it:**
- Currently bypassed (we use `parseWireQuestion()`)
- But it's available and fixed for Windows
- Can be used with `DNSComboWriter`

---

### **7. FDMultiplexer - I/O Multiplexing Interface**

**Location:** `mplexer.hh` (upstream interface)

**What it does:**
- Abstract interface for I/O multiplexing (epoll, kqueue, libevent)
- Handles socket event registration and callbacks

**How we use it:**
```cpp
// main_test.cc:43, 605
extern thread_local std::unique_ptr<FDMultiplexer> t_fdm;
t_fdm->addReadFD(g_udp_socket, handleDNSQuery, param);
t_fdm->run(&g_now, timeoutMsec);
```

**We implement `LibeventFDMultiplexer`** (Windows version), but use upstream's interface!

---

### **8. lwres.cc - Upstream Query Functions**

**Location:** `lwres.cc` (upstream, with Windows fixes)

**What it does:**
- `asyncresolve()` - Performs async DNS queries to upstream servers
- Uses MTasker for async I/O
- Handles query construction and response parsing

**How we use it:**
```cpp
// SyncRes calls this internally via asendto/arecvfrom
// lwres.cc:asyncresolve() is called by SyncRes during resolution
```

**SyncRes uses this** for upstream queries - we're using upstream's query functions!

---

### **9. Utility Functions**

**Location:** `utility.hh`, `misc.hh`, `iputils.hh` (upstream)

**What they do:**
- `gettimeofday()` - Time functions
- `stringerror()` - Error string conversion
- `ComboAddress` - Address handling
- Various utility functions

**How we use it:**
```cpp
// main_test.cc:27, 268, 571
#include "utility.hh"
Utility::gettimeofday(&now, nullptr);
```

---

### **10. DNS Record Content Types**

**Location:** `dnsrecords.hh` / `dnsrecords.cc` (upstream)

**What they do:**
- `NSRecordContent` - NS record representation
- `ARecordContent` - A record representation
- `AAAARecordContent` - AAAA record representation
- All DNS record type implementations

**How we use it:**
```cpp
// main_test.cc:72-84
NSRecordContent nsrr;
ARecordContent arr;
nsrr.setContent(std::make_shared<NSRecordContent>(DNSName(templ)));
arr.setContent(std::make_shared<ARecordContent>(ComboAddress(addr)));
```

---

## ❌ **What We Created Ourselves (Stubs/Manual)**

### **1. Query Handler - handleDNSQuery()**

**Upstream:** `handleNewUDPQuestion()` → `doProcessUDPQuestion()` → `startDoResolve()`

**Our POC:** `handleDNSQuery()` → `resolveTaskFunc()`

**Why:** Simplified for POC, bypasses `MOADNSParser` issues (now fixed)

---

### **2. Resolution Task - resolveTaskFunc()**

**Upstream:** `startDoResolve()` in `pdns_recursor.cc:975`

**Our POC:** `resolveTaskFunc()` in `main_test.cc:260`

**Why:** Simplified, uses `ResolveJob` instead of `DNSComboWriter`

---

### **3. Job Structure - ResolveJob**

**Upstream:** `DNSComboWriter` (30+ fields)

**Our POC:** `ResolveJob` (7 fields)

**Why:** Simplified for POC, avoids dependencies

---

### **4. Packet Parser - parseWireQuestion()**

**Upstream:** `MOADNSParser` (full packet parser)

**Our POC:** `parseWireQuestion()` (minimal wire parser)

**Why:** Bypassed `MOADNSParser` due to Windows padding issues (now fixed)

---

### **5. Main Function - main()**

**Upstream:** `main()` → `serviceMain()` → `recursorThread()` → `recLoop()`

**Our POC:** `main()` with custom event loop

**Why:** Simplified for POC, single-threaded

---

### **6. Socket Creation - Manual**

**Upstream:** `makeUDPServerSockets()` → `deferredAdds` mechanism

**Our POC:** Manual `socket()`, `bind()`, `setsockopt()`

**Why:** Simplified for POC, hardcoded port

---

### **7. Event Loop - Custom**

**Upstream:** `recLoop()` in `rec-main.cc:2800`

**Our POC:** Custom `while(true)` loop in `main()`

**Why:** Simplified for POC, single-threaded

---

### **8. Stub Functions**

**Location:** `lwres_stubs.cc`, `dnssec_stubs.cc`, `globals_stub.cc`

**What they do:**
- Provide minimal implementations of functions SyncRes expects
- Some are empty (no-op)
- Some provide basic functionality

**Examples:**
- `lwres_stubs.cc` - Stubs for resolver functions
- `globals_stub.cc` - Global variable stubs
- `dnssec_stubs.cc` - DNSSEC stubs (if not using DNSSEC)

---

## 📊 **Summary: What's Upstream vs Ours**

| Component | Source | Purpose |
|-----------|--------|---------|
| **SyncRes** | ✅ Upstream | Core DNS resolver (THE MOST IMPORTANT) |
| **MTasker** | ✅ Upstream | Coroutine/async system |
| **MemRecursorCache** | ✅ Upstream | Positive cache |
| **NegCache** | ✅ Upstream | Negative cache |
| **DNSName, QType, QClass** | ✅ Upstream | DNS data types |
| **DNSRecord, DNSResourceRecord** | ✅ Upstream | DNS record types |
| **DNSPacketWriter** | ✅ Upstream (Windows fixes) | Packet construction |
| **MOADNSParser** | ✅ Upstream (Windows fixes) | Packet parsing |
| **FDMultiplexer** | ✅ Upstream (interface) | I/O multiplexing interface |
| **LibeventFDMultiplexer** | ⚠️ Our implementation | Windows multiplexer (implements upstream interface) |
| **lwres.cc asyncresolve()** | ✅ Upstream (Windows fixes) | Upstream query functions |
| **handleDNSQuery()** | ❌ Our POC | Query handler (simplified) |
| **resolveTaskFunc()** | ❌ Our POC | Resolution task (simplified) |
| **ResolveJob** | ❌ Our POC | Job structure (simplified) |
| **parseWireQuestion()** | ❌ Our POC | Wire parser (simplified) |
| **main()** | ❌ Our POC | Main function (simplified) |
| **Socket creation** | ❌ Our POC | Manual socket code |
| **Event loop** | ❌ Our POC | Custom event loop |
| **Stub functions** | ⚠️ Our stubs | Minimal implementations |

---

## 🎯 **Key Insight**

**We ARE using the core upstream code:**

1. ✅ **SyncRes** - Does ALL the DNS resolution work
2. ✅ **MTasker** - Provides async I/O capability
3. ✅ **Caches** - Store DNS records
4. ✅ **Data types** - All DNS structures
5. ✅ **Packet writer** - Constructs responses
6. ✅ **Upstream queries** - `lwres.cc` for querying other servers

**What we created:**
- ❌ **Wrappers** - Simplified handlers and tasks
- ❌ **Infrastructure** - Socket creation, event loop
- ⚠️ **Stubs** - Minimal implementations of required functions

---

## 💡 **Why It Works**

**The magic is that SyncRes does the heavy lifting:**

```
Our Code (handleDNSQuery) 
  └─> Our Code (resolveTaskFunc)
       └─> ✅ UPSTREAM (SyncRes::beginResolve)
            └─> ✅ UPSTREAM (lwres.cc::asyncresolve) - queries upstream servers
            └─> ✅ UPSTREAM (Cache lookups)
            └─> ✅ UPSTREAM (DNS resolution logic)
            └─> Returns DNS records
       └─> Our Code (DNSPacketWriter - upstream class, our usage)
            └─> Our Code (sendto response)
```

**We're essentially:**
- Using upstream's **core DNS engine** (SyncRes)
- Using upstream's **async I/O system** (MTasker)
- Using upstream's **data structures** (DNSName, DNSRecord, etc.)
- Using upstream's **packet construction** (DNSPacketWriter)
- Using upstream's **upstream queries** (lwres.cc)

**But providing:**
- Our own **query reception** (handleDNSQuery)
- Our own **task wrapper** (resolveTaskFunc)
- Our own **infrastructure** (socket, event loop)
- Our own **simplified structures** (ResolveJob)

---

## 🔄 **For Integration**

**To integrate with upstream, we need to:**
1. Replace `handleDNSQuery()` → `handleNewUDPQuestion()`
2. Replace `resolveTaskFunc()` → `startDoResolve()`
3. Replace `ResolveJob` → `DNSComboWriter`
4. Replace manual socket → `makeUDPServerSockets()`
5. Replace custom event loop → `recLoop()`
6. Remove stubs → Use upstream implementations

**But keep:**
- ✅ All the core upstream code (SyncRes, MTasker, caches, etc.)
- ✅ Windows fixes (padding, byte order)
- ✅ LibeventFDMultiplexer (our Windows implementation)

---

## Summary

**We ARE using a LOT from upstream:**
- ✅ **SyncRes** - The core DNS resolver (most important!)
- ✅ **MTasker** - Async I/O system
- ✅ **Caches** - MemRecursorCache, NegCache
- ✅ **Data types** - DNSName, QType, DNSRecord, etc.
- ✅ **Packet writer** - DNSPacketWriter
- ✅ **Upstream queries** - lwres.cc

**What we created:**
- ❌ **Wrappers** - Simplified handlers
- ❌ **Infrastructure** - Socket, event loop
- ⚠️ **Stubs** - Minimal implementations

**The core DNS resolution engine is 100% upstream!** We just wrapped it with simplified infrastructure.

