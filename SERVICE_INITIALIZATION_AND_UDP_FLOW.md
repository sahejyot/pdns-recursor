# PowerDNS Recursor Service Initialization and UDP Query Flow

## Table of Contents
1. [Service Entry Point](#service-entry-point)
2. [Initialization Flow](#initialization-flow)
3. [Global and Thread-Local Variables](#global-and-thread-local-variables)
4. [Thread Initialization](#thread-initialization)
5. [UDP Query Entry Point](#udp-query-entry-point)
6. [Complete Flow Diagram](#complete-flow-diagram)

---

## Service Entry Point

### Main Function
**File:** `pdns-recursor/rec-main.cc:3195`

```cpp
int main(int argc, char** argv)
{
    // 1. Basic setup
    g_argc = argc;
    g_argv = argv;
    versionSetProduct(ProductRecursor);
    reportAllTypes();
    
    // 2. Parse command line arguments
    ::arg().setDefaults();
    ::arg().laxParse(argc, argv);
    
    // 3. Setup logging
    setupLogging(s_structured_logger_backend);
    
    // 4. Load configuration (YAML or old-style)
    // ... configuration loading ...
    
    // 5. Initialize caches (GLOBAL)
    g_recCache = std::make_unique<MemRecursorCache>(::arg().asNum("record-cache-shards"));
    g_negCache = std::make_unique<NegCache>(::arg().asNum("record-cache-shards") / 8);
    
    // 6. Start main service
    ret = serviceMain(startupLog);
    
    return ret;
}
```

---

## Initialization Flow

### Phase 1: serviceMain() - Core Initialization
**File:** `pdns-recursor/rec-main.cc:2196`

```cpp
static int serviceMain(Logr::log_t log)
{
    // 1. Logging setup
    g_log.setName(g_programname);
    g_log.disableSyslog(::arg().mustDo("disable-syslog"));
    
    // 2. Network initialization
    ret = initNet(log);           // Network subsystem setup
    
    // 3. DNSSEC initialization
    ret = initDNSSEC(log);        // DNSSEC configuration
    
    // 4. Lua configuration
    auto luaResult = luaconfig(false);
    
    // 5. Security/ACL setup
    parseACLs();                  // Access control lists
    initPublicSuffixList(...);
    initDontQuery(log);
    
    // 6. Synchronous resolver initialization
    ret = initSyncRes(log);       // SyncRes static members
    
    // 7. DNS64 initialization
    ret = initDNS64(log);
    
    // 8. Thread configuration
    RecThreadInfo::setNumDistributorThreads(::arg().asNum("distributor-threads"));
    RecThreadInfo::setNumUDPWorkerThreads(::arg().asNum("threads"));
    RecThreadInfo::setNumTCPWorkerThreads(::arg().asNum("tcp-threads"));
    
    // 9. Control channel
    ret = initControl(log, newuid, forks);
    
    // 10. Thread pipes for inter-thread communication
    RecThreadInfo::makeThreadPipes(log);
    
    // 11. Port initialization (UDP/TCP socket creation)
    ret = initPorts(log);         // Creates sockets, stores in deferredAdds
    
    // 12. Start all threads
    ret = RecThreadInfo::runThreads(log);
    
    return ret;
}
```

### Phase 2: initPorts() - Socket Creation
**File:** `pdns-recursor/rec-main.cc:2022`

```cpp
static int initPorts(Logr::log_t log)
{
    // Configure UDP source port ranges
    g_minUdpSourcePort = ::arg().asNum("udp-source-port-min");
    g_maxUdpSourcePort = ::arg().asNum("udp-source-port-max");
    // ... port configuration ...
    
    return 0;
}
```

**Note:** Actual socket creation happens in `makeUDPServerSockets()` which is called from `RecThreadInfo::runThreads()`:
- **File:** `pdns-recursor/pdns_recursor.cc:2702`
- Creates UDP sockets bound to listening addresses
- Stores socket FDs in `deferredAdds` (deferred registration)
- Sockets are NOT registered with multiplexer yet

---

## Global and Thread-Local Variables

### Global Variables (Initialized in main() or serviceMain())

**File:** `pdns-recursor/rec-main.cc:84-126`

```cpp
// Program identity
string g_programname = "pdns_recursor";
string g_pidfname;
int g_argc;
char** g_argv;

// Control and configuration
RecursorControlChannel g_rcc;
bool g_regressionTestMode;
bool g_yamlSettings;
string g_yamlSettingsSuffix;

// Caches (initialized in main())
std::unique_ptr<MemRecursorCache> g_recCache;
std::unique_ptr<NegCache> g_negCache;

// Security and ACLs
LockGuarded<std::shared_ptr<SyncRes::domainmap_t>> g_initialDomainMap;
LockGuarded<std::shared_ptr<NetmaskGroup>> g_initialAllowFrom;
LockGuarded<std::shared_ptr<NetmaskGroup>> g_initialAllowNotifyFrom;
LockGuarded<std::shared_ptr<notifyset_t>> g_initialAllowNotifyFor;
shared_ptr<NetmaskGroup> g_initialProxyProtocolACL;

// Network configuration
uint32_t g_disthashseed;
bool g_useIncomingECS;
unsigned int g_maxCacheEntries;
unsigned int g_networkTimeoutMsec;
uint16_t g_outgoingEDNSBufsize;
unsigned int g_maxMThreads;
uint16_t g_udpTruncationThreshold;
unsigned int g_maxUDPQueriesPerRound;

// Logging
std::shared_ptr<Logr::Logger> g_slog;
std::shared_ptr<Logr::Logger> g_slogtcpin;
std::shared_ptr<Logr::Logger> g_slogudpin;
bool g_quiet;
bool g_logCommonErrors;

// Time
struct timeval g_now;  // Updated by FDMultiplexer::run()
```

### Thread-Local Variables (Initialized in recursorThread())

**File:** `pdns-recursor/rec-main.cc:2876-2951`

```cpp
// Thread-local storage (each thread has its own copy)
thread_local std::unique_ptr<MT_t> g_multiTasker;           // MTasker (coroutine system)
thread_local std::unique_ptr<FDMultiplexer> t_fdm;          // File descriptor multiplexer
thread_local std::unique_ptr<UDPClientSocks> t_udpclientsocks; // UDP client sockets
thread_local NetmaskGroup t_allowFrom;                      // ACL for this thread
thread_local NetmaskGroup t_allowNotifyFrom;                // Notify ACL
thread_local notifyset_t t_allowNotifyFor;                  // Notify-for set
thread_local shared_ptr<NetmaskGroup> t_proxyProtocolACL;   // Proxy protocol ACL
thread_local shared_ptr<std::set<ComboAddress>> t_proxyProtocolExceptions;
thread_local std::unique_ptr<ProxyMapping> t_proxyMapping;  // Proxy mapping
thread_local std::shared_ptr<RecursorLua4> t_pdl;            // Lua scripting
thread_local RecThreadInfo::Counters t_Counters;            // Per-thread counters

// Statistics ring buffers (optional)
thread_local std::unique_ptr<addrringbuf_t> t_remotes;
thread_local std::unique_ptr<addrringbuf_t> t_servfailremotes;
thread_local std::unique_ptr<addrringbuf_t> t_bogusremotes;
thread_local std::unique_ptr<addrringbuf_t> t_largeanswerremotes;
thread_local std::unique_ptr<addrringbuf_t> t_timeouts;
thread_local std::unique_ptr<boost::circular_buffer<pair<DNSName, uint16_t>>> t_queryring;
```

---

## Thread Initialization

### recursorThread() - Per-Thread Setup
**File:** `pdns-recursor/rec-main.cc:2876`

```cpp
static void recursorThread()
{
    auto log = g_slog->withName("runtime");
    t_Counters.updateSnap(true);
    
    auto& threadInfo = RecThreadInfo::self();
    
    // 1. Initialize SyncRes (allocates thread storage)
    {
        SyncRes tmp(g_now);
        SyncRes::setDomainMap(*g_initialDomainMap.lock());
        t_allowFrom = *g_initialAllowFrom.lock();
        t_allowNotifyFrom = *g_initialAllowNotifyFrom.lock();
        t_allowNotifyFor = *g_initialAllowNotifyFor.lock();
        t_proxyProtocolACL = g_initialProxyProtocolACL;
        t_proxyProtocolExceptions = g_initialProxyProtocolExceptions;
        t_udpclientsocks = std::make_unique<UDPClientSocks>();
        
        if (g_proxyMapping) {
            t_proxyMapping = make_unique<ProxyMapping>(*g_proxyMapping);
        }
        
        // Prime root hints (only in handler thread)
        if (threadInfo.isHandler()) {
            if (!primeHints()) {
                threadInfo.setExitCode(EXIT_FAILURE);
                RecursorControlChannel::stop = true;
            }
        }
    }
    
    // 2. Load Lua script (if configured)
    if (threadInfo.isWorker() || threadInfo.isListener()) {
        if (!::arg()["lua-dns-script"].empty()) {
            t_pdl = std::make_shared<RecursorLua4>();
            t_pdl->loadFile(::arg()["lua-dns-script"]);
        }
    }
    
    // 3. Initialize statistics ring buffers (if configured)
    if (unsigned int ringsize = ::arg().asNum("stats-ringbuffer-entries") / RecThreadInfo::numUDPWorkers(); ringsize != 0) {
        t_remotes = std::make_unique<addrringbuf_t>();
        t_remotes->set_capacity(ringsize);
        // ... more ring buffers ...
    }
    
    // 4. Create MTasker (coroutine system)
    g_multiTasker = std::make_unique<MT_t>(::arg().asNum("stack-size"), ::arg().asNum("stack-cache-size"));
    threadInfo.setMT(g_multiTasker.get());
    
    // 5. Create FDMultiplexer (epoll/kqueue/libevent/etc.)
    t_fdm = unique_ptr<FDMultiplexer>(getMultiplexer(log));
    
    // 6. Register thread pipe for inter-thread communication
    t_fdm->addReadFD(threadInfo.getPipes().readToThread, handlePipeRequest);
    
    // 7. Register UDP sockets (if this thread is a listener)
    if (threadInfo.isListener()) {
        if (g_reusePort) {
            // Each thread has its own sockets (SO_REUSEPORT)
            for (const auto& deferred : threadInfo.getDeferredAdds()) {
                t_fdm->addReadFD(deferred.first, deferred.second);
                // deferred.first = socket FD
                // deferred.second = callback function (handleNewUDPQuestion)
            }
        }
        else {
            // All threads share the same sockets
            for (const auto& deferred : s_deferredUDPadds) {
                t_fdm->addReadFD(deferred.first, deferred.second);
            }
        }
    }
    
    // 8. Register control channel (handler thread only)
    if (threadInfo.isHandler()) {
        t_fdm->addReadFD(g_rcc.d_fd, handleRCC);
    }
    
    // 9. Start event loop
    recLoop();
}
```

---

## UDP Query Entry Point

### Event Loop - recLoop()
**File:** `pdns-recursor/rec-main.cc:2800`

```cpp
static void recLoop()
{
    auto& threadInfo = RecThreadInfo::self();
    
    while (!RecursorControlChannel::stop) {
        // 1. Schedule MTasker coroutines
        while (g_multiTasker->schedule(g_now)) {
            ; // Let coroutines run
        }
        
        // 2. Periodic maintenance tasks
        if (threadInfo.isHandler() && s_counter % 11 == 0) {
            g_multiTasker->makeThread(houseKeeping, nullptr);
        }
        
        // 3. Check for expired TCP connections
        if (s_counter % 55 == 0) {
            auto expired = t_fdm->getTimeouts(g_now);
            // ... handle timeouts ...
        }
        
        // 4. MAIN EVENT LOOP - Wait for I/O events
        auto timeoutUsec = g_multiTasker->nextWaiterDelayUsec(500000);
        t_fdm->run(&g_now, static_cast<int>(timeoutUsec / 1000));
        // ^^^ This is where UDP packets are detected!
        //     When a UDP socket becomes readable, it calls the registered callback
    }
}
```

**Key Point:** `t_fdm->run()` blocks until:
- A registered file descriptor becomes readable/writable
- A timeout expires
- When a UDP socket has data, it calls the callback registered with `addReadFD()`

---

### UDP Packet Reception - handleNewUDPQuestion()
**File:** `pdns-recursor/pdns_recursor.cc:2474`

This is the **ENTRY POINT** for UDP queries:

```cpp
static void handleNewUDPQuestion(int fileDesc, FDMultiplexer::funcparam_t& /* var */)
{
    // 1. Prepare receive buffer
    static thread_local std::string data;
    ComboAddress fromaddr;
    ComboAddress source;
    ComboAddress destination;
    struct msghdr msgh{};
    struct iovec iov{};
    cmsgbuf_aligned cbuf;
    
    // 2. Receive UDP packet with control messages (for source address)
    data.resize(maxIncomingQuerySize);
    fillMSGHdr(&msgh, &iov, &cbuf, sizeof(cbuf), data.data(), data.size(), &fromaddr);
    
    ssize_t len = recvmsg(fileDesc, &msgh, 0);  // <-- RECEIVE PACKET
    
    // 3. Validate packet
    if ((msgh.msg_flags & MSG_TRUNC) != 0) {
        t_Counters.at(rec::Counter::truncatedDrops)++;
        return;  // Drop truncated packets
    }
    
    data.resize(static_cast<size_t>(len));
    
    // 4. Extract destination address
    ComboAddress destaddr;
    HarvestDestinationAddress(&msgh, &destaddr);
    
    // 5. Handle proxy protocol (if enabled)
    if (expectProxyProtocol(fromaddr, destaddr)) {
        bool tcp = false;
        ssize_t used = parseProxyHeader(data, proxyProto, source, destination, tcp, proxyProtocolValues);
        data.erase(0, used);
    }
    
    // 6. Validate DNS header
    if (data.size() < sizeof(dnsheader)) {
        t_Counters.at(rec::Counter::ignoredCount)++;
        return;
    }
    
    const dnsheader_aligned headerdata(data.data());
    const dnsheader* dnsheader = headerdata.get();
    
    // 7. Validate query (not an answer, correct opcode, has questions)
    if (dnsheader->qr) {
        return;  // Ignore answers
    }
    if (dnsheader->opcode != static_cast<unsigned>(Opcode::Query) && 
        dnsheader->opcode != static_cast<unsigned>(Opcode::Notify)) {
        return;  // Unsupported opcode
    }
    if (dnsheader->qdcount == 0U) {
        return;  // Empty query
    }
    
    // 8. Check ACLs
    if (t_allowFrom && !t_allowFrom->match(&mappedSource)) {
        t_Counters.at(rec::Counter::unauthorizedUDP)++;
        return;  // Not allowed
    }
    
    // 9. Harvest timestamp (if available)
    struct timeval tval = {0, 0};
    HarvestTimestamp(&msgh, &tval);
    
    // 10. Process query (either directly or distribute to worker)
    if (RecThreadInfo::weDistributeQueries()) {
        // Distribute to worker thread via pipe
        distributeAsyncFunction(data, [localdata = std::move(data), fromaddr, destaddr, ...]() mutable {
            return doProcessUDPQuestion(localdata, fromaddr, destaddr, ...);
        });
    }
    else {
        // Process directly in this thread
        doProcessUDPQuestion(data, fromaddr, destaddr, source, destination, mappedSource, 
                           tval, fileDesc, proxyProtocolValues, eventTrace, otTrace);
    }
}
```

---

### Query Processing - doProcessUDPQuestion()
**File:** `pdns-recursor/pdns_recursor.cc:2190`

This function processes the actual DNS query:

```cpp
static string* doProcessUDPQuestion(const std::string& question, const ComboAddress& fromaddr, ...)
{
    // 1. Update time
    gettimeofday(&g_now, nullptr);
    
    // 2. Increment counters
    ++t_Counters.at(rec::Counter::qcounter);
    
    // 3. Parse DNS question
    const dnsheader_aligned headerdata(question.data());
    const dnsheader* dnsheader = headerdata.get();
    
    // 4. Check packet cache
    string response;
    if (g_packetCache && g_packetCache->getResponsePacket(remote, packet, ...)) {
        // Cache hit - return immediately
        return response;
    }
    
    // 5. Parse question name and type
    DNSName qname;
    QType qtype;
    // ... parsing ...
    
    // 6. Create MTasker coroutine for recursive resolution
    g_multiTasker->makeThread(startDoResolve, comboWriter);
    // This spawns a coroutine that will:
    //   - Create SyncRes instance
    //   - Call beginResolve() -> SyncRes::beginResolve()
    //   - Perform recursive DNS resolution
    //   - Send response back to client
    
    return nullptr;  // Response sent asynchronously
}
```

---

## Complete Flow Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│ 1. SERVICE STARTUP                                              │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
                    ┌─────────────────┐
                    │  main()          │
                    │  rec-main.cc:3195│
                    └─────────────────┘
                              │
                              ▼
                    ┌─────────────────┐
                    │  serviceMain()   │
                    │  rec-main.cc:2196│
                    └─────────────────┘
                              │
        ┌─────────────────────┼─────────────────────┐
        │                     │                     │
        ▼                     ▼                     ▼
┌──────────────┐      ┌──────────────┐      ┌──────────────┐
│ initNet()    │      │ initDNSSEC() │      │ initSyncRes()│
│ initPorts()  │      │ luaconfig()  │      │ parseACLs()  │
└──────────────┘      └──────────────┘      └──────────────┘
        │                     │                     │
        └─────────────────────┼─────────────────────┘
                              │
                              ▼
                    ┌─────────────────┐
                    │ runThreads()     │
                    │ rec-main.cc:245  │
                    └─────────────────┘
                              │
                              ▼
                    ┌─────────────────┐
                    │ recursorThread()│
                    │ rec-main.cc:2876│
                    └─────────────────┘
                              │
        ┌─────────────────────┼─────────────────────┐
        │                     │                     │
        ▼                     ▼                     ▼
┌──────────────┐      ┌──────────────┐      ┌──────────────┐
│ Initialize   │      │ Create       │      │ Register     │
│ thread-local │      │ FDMultiplexer│      │ UDP sockets  │
│ variables    │      │ (epoll/etc.) │      │ with t_fdm   │
└──────────────┘      └──────────────┘      └──────────────┘
                              │
                              ▼
                    ┌─────────────────┐
                    │ recLoop()        │
                    │ rec-main.cc:2800 │
                    └─────────────────┘
                              │
                              ▼
                    ┌─────────────────┐
                    │ t_fdm->run()    │
                    │ (Event loop)    │
                    └─────────────────┘
                              │
                    ┌─────────┴─────────┐
                    │                   │
                    ▼                   ▼
        ┌──────────────────┐  ┌──────────────────┐
        │ UDP socket       │  │ TCP socket       │
        │ readable         │  │ readable         │
        └──────────────────┘  └──────────────────┘
                    │                   │
                    ▼                   ▼
        ┌──────────────────┐  ┌──────────────────┐
        │ handleNewUDP     │  │ handleNewTCP     │
        │ Question()       │  │ Connection()     │
        │ pdns_recursor.cc │  │                  │
        │ :2474            │  │                  │
        └──────────────────┘  └──────────────────┘
                    │
                    ▼
        ┌──────────────────┐
        │ recvmsg()        │
        │ Receive packet   │
        └──────────────────┘
                    │
                    ▼
        ┌──────────────────┐
        │ Validate packet  │
        │ Check ACLs       │
        └──────────────────┘
                    │
                    ▼
        ┌──────────────────┐
        │ doProcessUDP     │
        │ Question()       │
        │ pdns_recursor.cc │
        │ :2190            │
        └──────────────────┘
                    │
        ┌───────────┴───────────┐
        │                       │
        ▼                       ▼
┌──────────────┐      ┌──────────────┐
│ Packet cache │      │ Create       │
│ check         │      │ MTasker      │
│ (cache hit?)  │      │ coroutine    │
└──────────────┘      └──────────────┘
        │                       │
        │                       ▼
        │              ┌──────────────┐
        │              │ startDoResolve│
        │              │ (coroutine)  │
        │              └──────────────┘
        │                       │
        │                       ▼
        │              ┌──────────────┐
        │              │ SyncRes::    │
        │              │ beginResolve │
        │              └──────────────┘
        │                       │
        └───────────────────────┘
                    │
                    ▼
        ┌──────────────────┐
        │ Send response    │
        │ to client        │
        └──────────────────┘
```

---

## Key Points Summary

1. **Entry Point:** `main()` → `serviceMain()` → `runThreads()` → `recursorThread()` → `recLoop()`

2. **Global Variables:** Initialized in `main()` or `serviceMain()`:
   - Caches: `g_recCache`, `g_negCache`
   - Configuration: `g_networkTimeoutMsec`, `g_maxMThreads`, etc.
   - ACLs: `g_initialAllowFrom`, `g_initialDomainMap`, etc.

3. **Thread-Local Variables:** Initialized in `recursorThread()`:
   - `g_multiTasker` - Coroutine system
   - `t_fdm` - File descriptor multiplexer
   - `t_udpclientsocks` - UDP client sockets
   - `t_allowFrom`, `t_pdl`, `t_Counters`, etc.

4. **UDP Socket Registration:**
   - Sockets created in `makeUDPServerSockets()` (called from `runThreads()`)
   - Stored in `deferredAdds` or `s_deferredUDPadds`
   - Registered with `t_fdm->addReadFD(socketFD, handleNewUDPQuestion)` in `recursorThread()`

5. **UDP Query Entry Point:**
   - `t_fdm->run()` detects readable UDP socket
   - Calls `handleNewUDPQuestion(fileDesc, ...)`
   - `handleNewUDPQuestion()` receives packet via `recvmsg()`
   - Calls `doProcessUDPQuestion()` to process query
   - `doProcessUDPQuestion()` creates MTasker coroutine for recursive resolution

6. **Event Loop:**
   - `recLoop()` runs continuously
   - `t_fdm->run()` blocks until I/O event or timeout
   - When UDP packet arrives, callback (`handleNewUDPQuestion`) is invoked
   - MTasker coroutines are scheduled between I/O events

---

## File Locations Reference

- **Main entry:** `pdns-recursor/rec-main.cc:3195`
- **Service initialization:** `pdns-recursor/rec-main.cc:2196`
- **Thread initialization:** `pdns-recursor/rec-main.cc:2876`
- **Event loop:** `pdns-recursor/rec-main.cc:2800`
- **UDP query entry:** `pdns-recursor/pdns_recursor.cc:2474`
- **Query processing:** `pdns-recursor/pdns_recursor.cc:2190`
- **Socket creation:** `pdns-recursor/pdns_recursor.cc:2702` (`makeUDPServerSockets`)

