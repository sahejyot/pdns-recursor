# PowerDNS Recursor Windows Port - Implementation Plan
## Complete Migration Strategy with File-by-File Analysis

**Document Purpose**: Comprehensive implementation plan for migrating PowerDNS Recursor to Windows, organized by priority and complexity.

**Target**: Proof-of-concept Windows DNS Recursor with core functionality, then expand to full feature set.

---

## 📊 **Executive Summary**

| Phase | Features | Files | Effort | Status |
|-------|----------|-------|--------|--------|
| **Phase 1A** | Core DNS (Copy As-Is) | 45 files | 1 week | ✅ Ready |
| **Phase 1B** | Core DNS (Needs Tweaks) | 8 files | 3-4 weeks | 🟡 Needs Work |
| **Phase 2** | Non-Core Portable | 25 files | 1 week | ✅ Ready |
| **Phase 3** | Non-Core Tweaks | 12 files | 2-3 weeks | 🟡 Optional |
| **Phase 4** | Monitoring/Metrics | 8 files | 3-4 weeks | 🔴 Later |
| **TOTAL** | - | **98 files** | **10-15 weeks** | - |

---

## 🎯 **PHASE 1A: Core DNS - Copy As-Is (No Changes Needed)**

### **DNS Protocol & Parsing (100% Portable)**

| File | Purpose | Lines | Complexity | Notes |
|------|---------|-------|------------|-------|
| `dnsname.cc/hh` | DNS name handling | 800 | Easy | ✅ Pure C++, STL |
| `dnsparser.cc/hh` | DNS packet parsing | 900 | Easy | ✅ Pure C++, no OS deps |
| `dnswriter.cc/hh` | DNS packet writing | 600 | Easy | ✅ Pure C++, no OS deps |
| `dnsrecords.cc/hh` | DNS record types | 2500 | Easy | ✅ Pure C++, record definitions |
| `qtype.cc/hh` | Query type handling | 300 | Easy | ✅ Pure C++, enums |
| `dns.cc/hh` | DNS constants/structures | 500 | Easy | ✅ Pure C++, RFC structures |
| `dnspacket.hh` | DNS packet structures | 200 | Easy | ✅ Header-only, pure C++ |

**Subtotal: 7 files (~5,800 lines) - ✅ Copy directly**

---

### **Core Resolution Logic (100% Portable)**

| File | Purpose | Lines | Complexity | Notes |
|------|---------|-------|------------|-------|
| **`syncres.cc/hh`** | **Recursive resolution algorithm** | **6200** | **Hard** | **🌟 CRITICAL - Pure C++!** |
| `recursor_cache.cc/hh` | DNS caching (Boost containers) | 1800 | Medium | ✅ Boost.MultiIndex (portable) |
| `negcache.cc/hh` | Negative caching | 500 | Easy | ✅ Pure C++, STL containers |
| `aggressive_nsec.cc/hh` | NSEC aggressive use (DNSSEC) | 800 | Medium | ✅ Pure C++, DNSSEC logic |
| `recpacketcache.cc/hh` | Packet cache | 600 | Easy | ✅ Pure C++, caching |
| `packetcache.hh` | Packet cache interface | 100 | Easy | ✅ Header-only |

**Subtotal: 6 files (~10,000 lines) - ✅ Copy directly**

---

### **DNSSEC Validation (100% Portable)**

| File | Purpose | Lines | Complexity | Notes |
|------|---------|-------|------------|-------|
| `validate-recursor.cc/hh` | DNSSEC validation logic | 1500 | Hard | ✅ Pure C++, crypto (OpenSSL) |
| `validate.cc/hh` | DNSSEC crypto validation | 900 | Medium | ✅ OpenSSL (portable) |
| `dnssecinfra.cc/hh` | DNSSEC infrastructure | 2000 | Medium | ✅ Pure C++, DNSSEC records |
| `nsecrecords.cc` | NSEC record handling | 400 | Easy | ✅ Pure C++, DNSSEC |
| `dnsseckeeper.hh` | DNSSEC keeper interface | 200 | Easy | ✅ Header-only |
| `opensslsigners.cc/hh` | OpenSSL crypto signers | 800 | Medium | ✅ OpenSSL (cross-platform) |
| `sodiumsigners.cc` | libsodium signers | 200 | Easy | ✅ libsodium (portable) |
| `libssl.cc/hh` | SSL/TLS utilities | 500 | Easy | ✅ OpenSSL wrapper |

**Subtotal: 8 files (~6,500 lines) - ✅ Copy directly**

---

### **Coroutines/Threading (95% Portable via Boost.Context)**

| File | Purpose | Lines | Complexity | Notes |
|------|---------|-------|------------|-------|
| **`mtasker.hh`** | **Cooperative multitasking** | **700** | **Hard** | **✅ Boost.Context (Windows Fibers)** |
| **`mtasker_context.cc/hh`** | **Context switching** | **300** | **Hard** | **✅ Boost.Context (portable!)** |
| `taskqueue.cc/hh` | Task queue management | 400 | Easy | ✅ STL containers |
| `rec-taskqueue.cc/hh` | Recursor task queue | 500 | Easy | ✅ Pure C++, task dispatch |
| `channel.cc/hh` | Channel abstraction | 300 | Easy | ✅ C++, message passing |

**Subtotal: 5 files (~2,200 lines) - ✅ Copy directly (Boost handles Windows!)**

---

### **Data Structures & Utilities (100% Portable)**

| File | Purpose | Lines | Complexity | Notes |
|------|---------|-------|------------|-------|
| `circular_buffer.hh` | Ring buffer | 150 | Easy | ✅ Header-only, STL |
| `lock.hh` | Lock guards | 200 | Easy | ✅ C++11 mutexes (portable) |
| `sholder.hh` | Shared holder pattern | 100 | Easy | ✅ Header-only, smart ptrs |
| `histogram.hh` | Histogram utilities | 150 | Easy | ✅ Header-only, pure C++ |
| `stat_t.hh` | Statistics types | 100 | Easy | ✅ Header-only |
| `lazy_allocator.hh` | Lazy allocator | 100 | Easy | ✅ Header-only, STL |
| `noinitvector.hh` | Uninitialized vector | 50 | Easy | ✅ Header-only, STL |
| `namespaces.hh` | Namespace declarations | 50 | Easy | ✅ Header-only |
| `pdnsexception.hh` | Exception classes | 100 | Easy | ✅ Header-only, std::exception |
| `burtle.hh` | Hash functions | 200 | Easy | ✅ Header-only, pure C++ |
| `sha.hh` | SHA hashing | 100 | Easy | ✅ Header-only, crypto libs |

**Subtotal: 11 files (~1,300 lines) - ✅ Copy directly**

---

### **Configuration & Arguments (90% Portable)**

| File | Purpose | Lines | Complexity | Notes |
|------|---------|-------|------------|-------|
| `arguments.cc/hh` | Argument parsing | 800 | Easy | ✅ Pure C++, STL |
| `comment.hh` | Comment handling | 50 | Easy | ✅ Header-only |
| `credentials.cc/hh` | Credential management | 200 | Easy | ✅ Pure C++, passwords |

**Subtotal: 3 files (~1,050 lines) - ✅ Copy directly**

---

### **Encoding/Decoding (100% Portable)**

| File | Purpose | Lines | Complexity | Notes |
|------|---------|-------|------------|-------|
| `base32.cc/hh` | Base32 encoding | 200 | Easy | ✅ Pure C++, RFC 4648 |
| `base64.cc/hh` | Base64 encoding | 200 | Easy | ✅ Pure C++, RFC 4648 |
| `json.cc/hh` | JSON parsing/generation | 800 | Easy | ✅ Pure C++, JSON library |

**Subtotal: 3 files (~1,200 lines) - ✅ Copy directly**

---

### **Root Hints & Constants (100% Portable)**

| File | Purpose | Lines | Complexity | Notes |
|------|---------|-------|------------|-------|
| `root-addresses.hh` | Root server addresses | 100 | Easy | ✅ Const data, header-only |
| `root-dnssec.hh` | Root DNSSEC keys | 200 | Easy | ✅ Const data, header-only |
| `dns_random.hh` | Random number generation | 50 | Easy | ✅ Header-only, C++11 random |

**Subtotal: 3 files (~350 lines) - ✅ Copy directly**

---

## 📦 **Phase 1A Summary**

**Total Files**: 45 files  
**Total Lines**: ~29,400 lines  
**Effort**: **1 week** (just copying & verifying compilation)  
**Priority**: **CRITICAL** (Core DNS functionality)  
**Status**: ✅ **Ready to copy - NO changes needed!**

---

## 🔧 **PHASE 1B: Core DNS - Needs Tweaking (2-4 Weeks)**

### **I/O Multiplexing (CRITICAL - Must Rewrite)**

| File | Purpose | Lines | Complexity | Windows Replacement |
|------|---------|-------|------------|---------------------|
| **`epollmplexer.cc`** | **Linux epoll** | **200** | **Hard** | **❌ REPLACE with `iocpmplexer.cc`** |
| `kqueuemplexer.cc` | BSD kqueue | 250 | Hard | ❌ Skip (BSD-only) |
| `pollmplexer.cc` | poll() fallback | 200 | Medium | ⚠️ Works but slow on Windows |
| `devpollmplexer.cc` | Solaris devpoll | 180 | Hard | ❌ Skip (Solaris-only) |
| `portsmplexer.cc` | Solaris ports | 150 | Hard | ❌ Skip (Solaris-only) |
| `mplexer.hh` | Multiplexer interface | 150 | Easy | ✅ Keep (abstract interface) |

**Action**: Create **`iocpmplexer.cc`** (Windows IOCP implementation)  
**Effort**: **3-4 weeks** (most complex part of port)  
**Priority**: **CRITICAL**

---

### **Socket Operations (Needs Adaptation)**

| File | Purpose | Lines | Complexity | Changes Needed |
|------|---------|-------|------------|----------------|
| **`lwres.cc/hh`** | **Low-level DNS queries** | **700** | **Hard** | **⚠️ Replace `recvmsg()` with `WSARecvMsg()`** |
| `iputils.cc/hh` | IP utilities | 800 | Medium | ⚠️ Some socket options differ |
| `sstuff.hh` | Socket utilities | 400 | Medium | ⚠️ Winsock adaptations |
| `tcpiohandler.cc/hh` | TCP I/O handler | 600 | Medium | ⚠️ Adapt to IOCP model |
| `rec-tcp.cc` | TCP query handling | 800 | Hard | ⚠️ Adapt TCP state machine |
| `rec-tcpout.cc/hh` | Outgoing TCP connections | 600 | Medium | ⚠️ Adapt connection pooling |

**Effort**: **1-2 weeks** (socket API translation)  
**Priority**: **CRITICAL**

---

### **Main Loop & Initialization (Needs Rewrite)**

| File | Purpose | Lines | Complexity | Changes Needed |
|------|---------|-------|------------|----------------|
| **`rec-main.cc/hh`** | **Main event loop** | **3000** | **Hard** | **⚠️ Remove `fork()`, integrate IOCP, Windows Service** |
| **`pdns_recursor.cc`** | **Server entry point** | **2500** | **Hard** | **⚠️ Main query handlers, remove `/proc` checks** |

**Effort**: **2-3 weeks** (core integration work)  
**Priority**: **CRITICAL**

---

## 📦 **Phase 1B Summary**

**Total Files**: 8 files (+ 1 new file: `iocpmplexer.cc`)  
**Total Lines**: ~10,330 lines to adapt  
**Effort**: **3-4 weeks** (most complex work)  
**Priority**: **CRITICAL** (Can't function without this)  
**Status**: 🟡 **Requires significant rewrites**

---

## ✨ **PHASE 2: Non-Core But Portable (1 Week)**

These features add value but aren't required for basic DNS resolution. Copy them **after** Phase 1 works.

### **Security & Policy Features (100% Portable)**

| File | Purpose | Lines | Complexity | Notes |
|------|---------|-------|------------|-------|
| **`filterpo.cc/hh`** | **RPZ (DNS firewall)** | **1200** | **Medium** | **✅ Pure C++, Boost containers** |
| **`rpzloader.cc/hh`** | **RPZ zone loading** | **800** | **Medium** | **✅ Pure C++, file parsing** |
| `ednssubnet.cc/hh` | EDNS Client Subnet | 600 | Easy | ✅ Pure C++, packet manipulation |
| `ednscookies.cc/hh` | DNS Cookies | 400 | Easy | ✅ Pure C++, crypto |
| `ednsextendederror.cc/hh` | EDNS Extended Errors | 300 | Easy | ✅ Pure C++, error codes |
| `ednsoptions.cc/hh` | EDNS options | 500 | Easy | ✅ Pure C++, option parsing |
| `ednspadding.cc/hh` | EDNS padding | 200 | Easy | ✅ Pure C++, padding logic |
| `proxy-protocol.cc/hh` | Proxy Protocol support | 500 | Easy | ✅ Pure C++, header parsing |

**Subtotal**: 8 files (~4,500 lines) - ✅ **Copy directly**

---

### **Advanced DNS Features (100% Portable)**

| File | Purpose | Lines | Complexity | Notes |
|------|---------|-------|------------|-------|
| `svc-records.cc/hh` | SVCB/HTTPS records | 600 | Medium | ✅ Pure C++, new RFCs |
| `zonemd.cc/hh` | Zone Message Digest | 400 | Easy | ✅ Pure C++, ZONEMD |
| `shuffle.cc/hh` | Server shuffling | 200 | Easy | ✅ Pure C++, randomization |
| `sortlist.cc/hh` | Sort list handling | 300 | Easy | ✅ Pure C++, ordering |
| `sillyrecords.cc` | Unusual record types | 150 | Easy | ✅ Pure C++, edge cases |
| `resolve-context.hh` | Resolution context | 100 | Easy | ✅ Header-only, context state |

**Subtotal**: 6 files (~1,750 lines) - ✅ **Copy directly**

---

### **Scripting & Extensibility (95% Portable)**

| File | Purpose | Lines | Complexity | Notes |
|------|---------|-------|------------|-------|
| **`lua-recursor4.cc/hh`** | **Lua scripting engine** | **2000** | **Hard** | **✅ LuaJIT has Windows builds** |
| `lua-base4.cc/hh` | Lua base classes | 800 | Medium | ✅ Pure C++, Lua bindings |
| `lua-recursor4-ffi.hh` | Lua FFI interface | 200 | Easy | ✅ Header-only, FFI defs |
| `rec-lua-conf.cc/hh` | Lua configuration | 600 | Medium | ✅ Pure C++, config parsing |

**Subtotal**: 4 files (~3,600 lines) - ✅ **Copy directly** (LuaJIT already supports Windows)

---

### **Zone Management (100% Portable)**

| File | Purpose | Lines | Complexity | Notes |
|------|---------|-------|------------|-------|
| `rec-zonetocache.cc/hh` | Zone to cache loading | 500 | Medium | ✅ Pure C++, zone parsing |
| `axfr-retriever.cc/hh` | AXFR zone transfers | 700 | Medium | ✅ TCP-based, portable |
| `ixfr.cc/hh` | IXFR incremental transfers | 600 | Medium | ✅ Pure C++, diff logic |
| `reczones-helpers.cc/hh` | Zone helpers | 400 | Easy | ✅ Pure C++, zone utilities |
| `reczones.cc` | Zone management | 500 | Medium | ✅ Pure C++, zone state |
| `auth-catalogzone.hh` | Catalog zones | 100 | Easy | ✅ Header-only, JSON |
| `zoneparser-tng.cc/hh` | Zone file parser | 800 | Medium | ✅ Pure C++, RFC 1035 |

**Subtotal**: 7 files (~3,600 lines) - ✅ **Copy directly**

---

## 📦 **Phase 2 Summary**

**Total Files**: 25 files  
**Total Lines**: ~13,450 lines  
**Effort**: **1 week** (just integration & testing)  
**Priority**: **HIGH** (valuable features, easy wins)  
**Status**: ✅ **Ready to add after Phase 1 works**

---

## 🔨 **PHASE 3: Non-Core Needs Tweaking (2-3 Weeks)**

### **Logging & Debugging (Needs Minor Adaptation)**

| File | Purpose | Lines | Complexity | Changes Needed |
|------|---------|-------|------------|----------------|
| `logger.cc/hh` | Logging infrastructure | 600 | Easy | ⚠️ Adapt syslog → Windows Event Log |
| `logging.cc/hh` | Structured logging | 400 | Easy | ✅ Mostly portable (uses std::ostream) |
| `logr.hh` | Logging interface | 100 | Easy | ✅ Header-only |
| `rec-eventtrace.cc/hh` | Event tracing | 500 | Easy | ✅ Pure C++, timestamps |
| `protozero.cc/hh` | Protobuf serialization | 600 | Easy | ✅ Pure C++, protobuf |
| `protozero-trace.cc/hh` | Protobuf tracing | 300 | Easy | ✅ Pure C++, tracing |
| `protozero-helpers.hh` | Protobuf helpers | 100 | Easy | ✅ Header-only |
| `dnstap.cc/hh` | DNSTap logging | 500 | Easy | ✅ Pure C++, protocol buffers |
| `fstrm_logger.cc/hh` | FrameStream logger | 400 | Easy | ✅ Pure C++, frame stream |
| `remote_logger.cc/hh` | Remote logging | 400 | Easy | ✅ Pure C++, network logging |

**Effort**: **3-5 days** (adapt syslog calls)  
**Priority**: **MEDIUM**

---

### **Web Interface (Needs Minor Adaptation)**

| File | Purpose | Lines | Complexity | Changes Needed |
|------|---------|-------|------------|----------------|
| `webserver.cc/hh` | Web server | 800 | Medium | ⚠️ Minor socket adaptations |
| `ws-api.cc/hh` | REST API | 1200 | Medium | ✅ Mostly portable (HTTP/JSON) |
| `ws-recursor.cc/hh` | Recursor web interface | 900 | Medium | ✅ Mostly portable |
| `rec-web-stubs.hh` | Web stubs | 100 | Easy | ✅ Header-only |
| `htmlfiles.h` | Embedded HTML | 500 | Easy | ✅ Data only |

**Effort**: **3-5 days** (minor tweaks)  
**Priority**: **HIGH** (very useful for management)

---

### **Advanced Query Handling (Minor Tweaks)**

| File | Purpose | Lines | Complexity | Changes Needed |
|------|---------|-------|------------|----------------|
| `query-local-address.cc/hh` | Local address selection | 300 | Easy | ⚠️ Adapt to Windows network APIs |
| `rec-nsspeeds.cc/hh` | NS speed tracking | 500 | Easy | ✅ Pure C++, measurements |
| `rec-responsestats.cc/hh` | Response statistics | 400 | Easy | ✅ Pure C++, stats collection |
| `rec-xfr.cc/hh` | Zone transfer coordination | 600 | Medium | ✅ Pure C++, XFR logic |
| `rec-xfrtracker.cc` | XFR tracking | 300 | Easy | ✅ Pure C++, state tracking |

**Effort**: **3-5 days**  
**Priority**: **MEDIUM**

---

### **Security Features (Minor POSIX → Windows)**

| File | Purpose | Lines | Complexity | Changes Needed |
|------|---------|-------|------------|----------------|
| **`nod.cc/hh`** | **New domain tracking** | **260** | **Easy** | **⚠️ Replace `getpid()` → `GetCurrentProcessId()`, `mkstemp()` → `_mktemp_s()`** |
| `stable-bloom.hh` | Stable Bloom filter | 300 | Easy | ✅ Header-only, pure C++ |
| `secpoll.cc/hh` | Security polling | 400 | Easy | ✅ HTTP-based, portable |
| `secpoll-recursor.cc/hh` | Recursor security poll | 300 | Easy | ✅ Pure C++, HTTP client |

**Effort**: **1-2 days** (trivial POSIX replacements)  
**Priority**: **MEDIUM**

---

### **Utilities (Minor Adaptations)**

| File | Purpose | Lines | Complexity | Changes Needed |
|------|---------|-------|------------|----------------|
| `gettime.cc/hh` | Time utilities | 200 | Easy | ⚠️ Adapt `gettimeofday()` → `QueryPerformanceCounter()` |
| `uuid-utils.cc/hh` | UUID generation | 150 | Easy | ⚠️ Use Windows `UuidCreate()` or Boost.UUID |
| `pubsuffix.cc/hh` | Public suffix list | 500 | Easy | ✅ Pure C++, string matching |
| `pubsuffixloader.cc` | Suffix list loader | 200 | Easy | ✅ Pure C++, file loading |
| `threadname.cc/hh` | Thread naming | 100 | Easy | ⚠️ Use Windows `SetThreadDescription()` |
| `coverage.cc/hh` | Code coverage | 300 | Easy | ⚠️ Adapt to Windows profiling tools |
| `version.cc/hh` | Version information | 100 | Easy | ✅ Pure C++, version strings |

**Effort**: **2-3 days**  
**Priority**: **LOW**

---

## 📦 **Phase 3 Summary**

**Total Files**: 37 files  
**Total Lines**: ~11,850 lines  
**Effort**: **2-3 weeks** (minor adaptations)  
**Priority**: **MEDIUM-LOW** (nice-to-have features)  
**Status**: 🟡 **Add after core works**

---

## 🚫 **PHASE 4: Linux-Specific - Major Rewrites (3-4 Weeks)**

These require **significant rewrites** for Windows. **Skip for proof-of-concept**, add later if needed.

### **System Monitoring (Reads `/proc`)**

| File | Purpose | Lines | Complexity | Changes Needed |
|------|---------|-------|------------|----------------|
| **`misc.cc/hh`** | **System utilities** | **2000** | **Hard** | **❌ Reads `/proc/` → Windows Performance Counters** |
| `rec-carbon.cc` | Carbon metrics | 400 | Medium | ❌ Uses `/proc` stats |
| `rec-snmp.cc/hh` | SNMP agent | 600 | Medium | ❌ Uses `/proc` stats |
| `snmp-agent.cc/hh` | SNMP implementation | 500 | Medium | ❌ Uses `/proc` stats |
| `rec-tcounters.cc/hh` | Thread counters | 300 | Easy | ⚠️ Adapt to Windows thread APIs |
| `rec-metrics-gen.h` | Metrics definitions | 200 | Easy | ✅ Header-only, data |
| `rec-prometheus-gen.h` | Prometheus metrics | 200 | Easy | ⚠️ Data formatting OK, `/proc` reads not OK |

**Effort**: **2-3 weeks** (rewrite all `/proc` access)  
**Priority**: **LOW** (optional monitoring)

---

### **Process Management (Uses `fork()`)**

| File | Purpose | Lines | Complexity | Changes Needed |
|------|---------|-------|------------|----------------|
| **`capabilities.cc/hh`** | **Linux capabilities** | **200** | **Medium** | **❌ Skip entirely (Linux-specific)** |
| `unix_utility.cc` | Unix utilities | 150 | Easy | ❌ Skip (Unix-specific) |

**Effort**: **N/A** (skip these files)  
**Priority**: **SKIP** (not applicable to Windows)

---

### **Control Channel (Unix Sockets)**

| File | Purpose | Lines | Complexity | Changes Needed |
|------|---------|-------|------------|----------------|
| **`rec_channel.cc/hh`** | **Control channel** | **240** | **Medium** | **❌ Replace `AF_UNIX` with Named Pipes or TCP** |
| `rec_channel_rec.cc` | Channel receiver | 300 | Medium | ❌ Depends on rec_channel.cc |
| `rec_control.cc` | Control CLI tool | 450 | Medium | ❌ Depends on rec_channel.cc |

**Effort**: **3-5 days** (rewrite IPC)  
**Priority**: **LOW** (Web API is better alternative)

---

## 📦 **Phase 4 Summary**

**Total Files**: 13 files  
**Total Lines**: ~5,540 lines  
**Effort**: **3-4 weeks** (major rewrites)  
**Priority**: **LOW** (skip for POC, add later if needed)  
**Status**: 🔴 **Optional - can skip entirely**

---

## 📂 **Files to SKIP (Not Needed)**

### **Build System Files**
- `configure.ac`, `Makefile.am`, `Makefile.in` → Replace with CMake
- `meson.build`, `meson_options.txt` → Meson build (skip)
- `aclocal.m4`, `config.h.in` → Autotools (skip)
- All `test-*.cc` files → Port tests later

### **Linux Service Files**
- `pdns-recursor.service*` → Use Windows Service instead
- `RECURSOR-MIB.*` → SNMP (Phase 4)

### **Generated/Build Files**
- `*.o`, `*.lo`, `*.a`, `*.la` → Build artifacts
- `libtool`, `stamp-h1` → Build tools

---

## 🗂️ **File Categories Summary**

| Category | Files | Lines | Status |
|----------|-------|-------|--------|
| **Phase 1A: Core - Copy As-Is** | 45 | ~29,400 | ✅ Ready |
| **Phase 1B: Core - Needs Tweaks** | 8 | ~10,330 | 🟡 Critical |
| **Phase 2: Non-Core Portable** | 25 | ~13,450 | ✅ Ready |
| **Phase 3: Non-Core Tweaks** | 37 | ~11,850 | 🟡 Optional |
| **Phase 4: Major Rewrites** | 13 | ~5,540 | 🔴 Optional |
| **Total Usable Code** | **128** | **~70,570** | - |

---

## ⏱️ **Time Estimates with AI Assistance**

### **Phase 1: Core DNS (Proof of Concept)**

| Task | Manual | With AI | Notes |
|------|--------|---------|-------|
| **Copy Phase 1A files** | 2 weeks | **3 days** | AI validates portable code |
| **Create IOCP multiplexer** | 6 weeks | **3 weeks** | AI scaffolds IOCP patterns |
| **Adapt socket APIs** | 3 weeks | **1 week** | AI translates POSIX → Winsock |
| **Adapt main loop** | 4 weeks | **2 weeks** | AI integrates IOCP + MTasker |
| **Testing & debugging** | 4 weeks | **2 weeks** | AI helps diagnose issues |
| **TOTAL PHASE 1** | **19 weeks** | **7-8 weeks** | 🎯 **~2 months** |

### **Phase 2: Add Portable Features**

| Task | Manual | With AI | Notes |
|------|--------|---------|-------|
| **Copy & integrate** | 2 weeks | **1 week** | Just copying + build fixes |
| **Testing** | 1 week | **3 days** | Verify RPZ, Lua, Web API |
| **TOTAL PHASE 2** | **3 weeks** | **1.5 weeks** | Quick wins |

### **Phase 3: Add Features with Tweaks**

| Task | Manual | With AI | Notes |
|------|--------|---------|-------|
| **Logging adaptations** | 1 week | **3 days** | AI translates syslog → Event Log |
| **Web server tweaks** | 1 week | **3 days** | Minor socket fixes |
| **NOD/utilities** | 1 week | **2 days** | Trivial POSIX → Windows |
| **TOTAL PHASE 3** | **3 weeks** | **1 week** | Polish |

### **Phase 4: Monitoring (Optional)**

| Task | Manual | With AI | Notes |
|------|--------|---------|-------|
| **Rewrite `/proc` access** | 4 weeks | **2 weeks** | AI scaffolds Performance Counters |
| **TOTAL PHASE 4** | **4 weeks** | **2 weeks** | Optional |

---

## 📊 **Overall Timeline**

| Milestone | Manual Estimate | With AI | Status |
|-----------|----------------|---------|--------|
| **Phase 1: Core DNS POC** | 19 weeks | **7-8 weeks** | 🎯 **Critical** |
| **Phase 2: Add Portable** | 3 weeks | **1.5 weeks** | ⭐ **High Value** |
| **Phase 3: Add Tweaks** | 3 weeks | **1 week** | ✨ **Polish** |
| **Phase 4: Monitoring** | 4 weeks | **2 weeks** | 🔧 **Optional** |
| **TOTAL** | **29 weeks (~7 months)** | **11.5-13 weeks (~3 months)** | - |

**For just POC (Phase 1)**: **7-8 weeks with AI assistance** 🎉

---

## 🏗️ **Implementation Strategy**

### **Week 1-2: Setup & Infrastructure**

1. **Create Windows project structure**
   - Set up CMake build system
   - Configure Boost (including Boost.Context)
   - Link OpenSSL, LuaJIT (Windows builds)
   - Create stub IOCP multiplexer

2. **Copy Phase 1A files (45 files)**
   - DNS protocol & parsing (7 files)
   - Core resolution logic (6 files)
   - DNSSEC validation (8 files)
   - MTasker/threading (5 files)
   - Data structures (11 files)
   - Configuration (3 files)
   - Encoding (3 files)
   - Root hints (3 files)

3. **Verify compilation**
   - Fix include paths
   - Resolve link dependencies
   - Stub out missing functions

**Milestone**: Project compiles (doesn't run yet)

---

### **Week 3-5: IOCP Multiplexer**

1. **Design IOCP wrapper** matching `FDMultiplexer` interface
2. **Implement core IOCP operations**
   - `addReadFD()` / `removeReadFD()`
   - `addWriteFD()` / `removeWriteFD()`
   - `run()` main loop
3. **Handle socket lifecycle**
   - Completion key management
   - OVERLAPPED structure pool
   - Completion callbacks
4. **Test basic I/O**
   - UDP echo server
   - TCP echo server

**Milestone**: IOCP multiplexer works standalone

---

### **Week 6-7: Socket Layer Adaptation**

1. **Adapt `lwres.cc`**
   - Replace `recvmsg()` → `WSARecvMsg()`
   - Replace `sendmsg()` → `WSASendMsg()`
   - Adapt control message (CMSG) handling

2. **Adapt `iputils.cc`**
   - Translate socket options
   - Fix IP_PKTINFO → Windows equivalent
   - Handle SO_REUSEADDR differences

3. **Adapt TCP handlers**
   - `tcpiohandler.cc` - async I/O
   - `rec-tcp.cc` - query handling
   - `rec-tcpout.cc` - connection management

**Milestone**: Socket layer works with IOCP

---

### **Week 8-9: Main Loop Integration**

1. **Adapt `rec-main.cc`**
   - Remove `fork()` → Use threads or Windows Service
   - Remove `/proc` checks (stub out)
   - Integrate IOCP with MTasker
   - Handle signals → Windows events

2. **Adapt `pdns_recursor.cc`**
   - Wire up UDP query handler
   - Wire up TCP query handler
   - Initialize root hints
   - Start worker threads

**Milestone**: Server starts and accepts queries

---

### **Week 10-12: Testing & Debugging**

1. **Basic query testing**
   - Query root servers
   - Resolve A records
   - Follow CNAME chains
   - Test DNSSEC validation

2. **Cache testing**
   - Verify caching works
   - Test TTL expiration
   - Test negative caching

3. **Performance testing**
   - Benchmark query rate
   - Test concurrent queries
   - Profile with Windows tools

4. **Bug fixing**
   - Fix crashes
   - Fix race conditions
   - Fix memory leaks

**Milestone**: ✅ **Core DNS POC works!**

---

## 🎯 **Success Criteria for POC**

### **Functional Requirements**

- ✅ Accepts DNS queries on UDP port 53
- ✅ Resolves queries recursively from root
- ✅ Follows CNAME chains
- ✅ Validates DNSSEC when enabled
- ✅ Caches responses (positive and negative)
- ✅ Handles concurrent queries (1000+ QPS)
- ✅ Stable for >24 hours continuous operation

### **Performance Targets**

- ✅ **Query rate**: >5,000 QPS on modern hardware
- ✅ **Latency**: <10ms for cached queries, <100ms for uncached
- ✅ **Memory**: <500MB for typical workload
- ✅ **CPU**: <50% utilization at 1000 QPS

### **Quality Requirements**

- ✅ No memory leaks (Valgrind equivalent)
- ✅ No crashes under stress testing
- ✅ Proper error handling
- ✅ Clean shutdown on Ctrl+C

---

## 📝 **Next Steps**

### **Immediate Actions (Week 1)**

1. **Set up development environment**
   - Install Visual Studio 2022
   - Install CMake, vcpkg
   - Build Boost (with Boost.Context)
   - Build OpenSSL for Windows

2. **Create project structure**
   ```
   pdns_recursor_windows/
   ├── CMakeLists.txt
   ├── src/
   │   ├── core/           # Phase 1A files
   │   ├── io/             # IOCP multiplexer
   │   └── main/           # Main loop
   ├── include/
   ├── tests/
   └── docs/
   ```

3. **Copy Phase 1A files**
   - Start with DNS protocol layer
   - Build incrementally
   - Fix compilation errors

4. **Design IOCP interface**
   - Match existing `FDMultiplexer` API
   - Plan completion callback mechanism
   - Design socket state management

---

## 🎓 **Learning Resources**

### **Windows I/O Completion Ports**
- Microsoft Docs: I/O Completion Ports
- "Network Programming for Microsoft Windows" book
- Sample IOCP server implementations

### **Boost.Context**
- Boost.Context documentation
- Understanding fibers vs threads
- Windows Fiber API

### **PowerDNS Internals**
- Read `syncres.cc` - understand resolution algorithm
- Study `mtasker.hh` - understand coroutine system
- Review existing multiplexers (epoll, kqueue) for patterns

---

## 🚀 **Conclusion**

**Key Insights:**

1. **~70% of code is already portable** (45 files, ~29K lines)
2. **Main challenge is I/O multiplexing** (epoll → IOCP)
3. **MTasker (coroutines) works on Windows** via Boost.Context
4. **With AI assistance, POC achievable in 7-8 weeks**

**Recommended Approach:**

1. ✅ **Start with POC** (Phase 1) - Core DNS only
2. ⭐ **Add portable features** (Phase 2) - Quick wins (RPZ, Lua, Web API)
3. ✨ **Polish** (Phase 3) - Minor tweaks for full functionality
4. 🔧 **Optional monitoring** (Phase 4) - Add if needed

**This plan is realistic and achievable with systematic execution!** 🎉

---

*Document created: 2025-10-24*  
*PowerDNS Recursor Version: 5.3.0*  
*Estimated Timeline: 7-8 weeks for POC with AI assistance*



