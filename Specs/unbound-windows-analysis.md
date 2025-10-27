# Unbound Windows Implementation - Complete Analysis

## 🎯 **Purpose**
Deep analysis of Unbound's Windows port to extract patterns for PowerDNS Recursor POC.

---

## 📁 **Key Files Analyzed**

### **1. util/winsock_event.c/.h** - Windows I/O Multiplexer (815 lines)
**Purpose**: Custom event system that mimics libevent API using native Windows APIs

### **2. winrc/win_svc.c/.h** - Windows Service (662 lines)
**Purpose**: Integration with Windows Service Control Manager

### **3. util/netevent.c** - Network event abstraction
**Purpose**: Cross-platform network event handling

---

## 🔍 **Critical Discovery: Unbound's I/O Approach**

### **What Unbound Uses**
```c
// From winsock_event.h line 40-44:
/**
 * This file contains interface functions with the WinSock2 API on Windows.
 * It uses the winsock WSAWaitForMultipleEvents interface on a number of
 * sockets.
 *
 * Note that windows can only wait for max 64 events at one time.
 */
```

**NOT using**:
- ❌ IOCP (too complex)
- ❌ WSAPoll (requires libevent)
- ❌ select() (too limited)

**Using**:
- ✅ **WSAWaitForMultipleEvents** - Native Windows API, no dependencies!

---

## 💻 **Unbound's Event System Architecture**

### **Structure Definition**

```c
// winsock_event.h lines 132-165

struct event_base {
    // Timeout management
    rbtree_type* times;              // Sorted by timeout
    
    // Socket/event management  
    struct event** items;            // Array of events
    int max;                         // Number in use
    int cap;                         // Capacity (WSK_MAX_ITEMS = 64)
    
    // Signal handling
    struct event** signals;          // Signal events (MAX_SIG = 32)
    
    // TCP special handling
    int tcp_stickies;                // TCP events that stay "ready"
    int tcp_reinvigorated;           // Re-check these
    
    // Windows event handles
    WSAEVENT waitfor[WSK_MAX_ITEMS]; // The actual Windows events
    
    // Control
    int need_to_exit;
    time_t* time_secs;
    struct timeval* time_tv;
};

struct event {
    // Public (libevent-compatible)
    rbnode_type node;
    int added;
    struct event_base *ev_base;
    int ev_fd;                       // Socket fd
    short ev_events;                 // EV_READ, EV_WRITE, etc.
    struct timeval ev_timeout;
    void (*ev_callback)(int, short, void *);
    void *ev_arg;
    
    // Windows-specific private
    int idx;                         // Index in items array
    WSAEVENT hEvent;                 // ⭐ Windows event handle
    int is_tcp;                      // TCP needs special handling
    short old_events;                // Remember previous events
    int stick_events;                // TCP "sticky" events flag
    int is_signal;                   // User-created signal event
    int just_checked;                // For fairness
};
```

---

## 🔑 **Key Pattern #1: Edge to Level Trigger Conversion**

**The Problem** (from winsock_event.h lines 51-64):
```c
/**
 * When a socket becomes readable, then it will not be flagged as 
 * readable again until you have gotten WOULDBLOCK from a recv routine.
 * That means the event handler must store the readability (edge notify)
 * and process the incoming data until it blocks. 
 *
 * The WSAEventSelect page says that it does do level notify, as long
 * as you call a recv/write/accept at least once when it is signalled.
 * This last bit is not true, even though documented in server2008 api docs
 * from microsoft, it does not happen at all. Instead you have to test for
 * WSAEWOULDBLOCK on a tcp stream, and only then retest the socket.
 * And before that remember the previous result as still valid.
 */
```

**Unbound's Solution**: TCP "sticky events"

```c
// When socket is ready
if (socket_ready && is_tcp) {
    event->stick_events = 1;      // Remember it's ready
    event->old_events = EV_READ;  // Remember what was ready
}

// Keep reporting ready until...
if (event->stick_events) {
    // Still report as ready
    callback(fd, event->old_events, arg);
}

// Application signals WOULDBLOCK
void winsock_tcp_wouldblock(struct event* ev, int eventbit) {
    ev->stick_events = 0;  // Clear sticky flag
    // Now will wait for new event
}
```

**For PowerDNS**: Must implement similar logic for TCP connections!

---

## 🔑 **Key Pattern #2: The Main Event Loop**

```c
// Simplified from winsock_event.c

int event_base_dispatch(struct event_base* base) {
    while(!base->need_to_exit) {
        // 1. Calculate timeout
        struct timeval* wait_tv = find_next_timeout(base);
        DWORD timeout_ms = tv_to_milliseconds(wait_tv);
        
        // 2. Build array of WSAEVENTs
        WSAEVENT waitfor[WSK_MAX_ITEMS];
        int num_events = 0;
        for (int i=0; i < base->max; i++) {
            if (base->items[i]->hEvent) {
                waitfor[num_events++] = base->items[i]->hEvent;
            }
        }
        
        // 3. Wait for any event ⭐ THE MAGIC HAPPENS HERE
        DWORD result = WSAWaitForMultipleEvents(
            num_events,      // How many events
            waitfor,         // Array of WSAEVENT handles
            FALSE,           // Wait for ANY (not all)
            timeout_ms,      // Timeout
            FALSE            // Not alertable
        );
        
        // 4. Handle result
        if (result == WSA_WAIT_TIMEOUT) {
            // Process timeouts
            handle_timeouts(base);
        }
        else if (result >= WSA_WAIT_EVENT_0 && 
                 result < WSA_WAIT_EVENT_0 + num_events) {
            // Which event fired?
            int idx = result - WSA_WAIT_EVENT_0;
            struct event* ev = base->items[idx];
            
            // Check what happened on this socket
            WSANETWORKEVENTS network_events;
            WSAEnumNetworkEvents(ev->ev_fd, ev->hEvent, &network_events);
            
            // Translate to callback events
            short what = 0;
            if (network_events.lNetworkEvents & FD_READ)
                what |= EV_READ;
            if (network_events.lNetworkEvents & FD_WRITE)
                what |= EV_WRITE;
            if (network_events.lNetworkEvents & FD_ACCEPT)
                what |= EV_READ;  // Treat accept as read
            
            // Call user callback
            ev->ev_callback(ev->ev_fd, what, ev->ev_arg);
        }
        
        // 5. Process TCP sticky events
        process_tcp_stickies(base);
    }
    return 0;
}
```

---

## 🔑 **Key Pattern #3: Adding Socket to Event System**

```c
// When PowerDNS wants to monitor a socket

int event_add(struct event* ev, struct timeval* tv) {
    struct event_base* base = ev->ev_base;
    
    // 1. Create Windows event for this socket
    ev->hEvent = WSACreateEvent();
    if (ev->hEvent == WSA_INVALID_EVENT)
        return -1;
    
    // 2. Associate socket with event
    long network_events = 0;
    if (ev->ev_events & EV_READ)
        network_events |= FD_READ | FD_ACCEPT | FD_CLOSE;
    if (ev->ev_events & EV_WRITE)
        network_events |= FD_WRITE | FD_CONNECT;
    
    if (WSAEventSelect(ev->ev_fd, ev->hEvent, network_events) != 0) {
        WSACloseEvent(ev->hEvent);
        return -1;
    }
    
    // 3. Add to event base
    base->items[base->max] = ev;
    ev->idx = base->max;
    base->max++;
    
    ev->added = 1;
    return 0;
}
```

---

## 🔑 **Key Pattern #4: Windows Service Integration**

```c
// From win_svc.c - How Unbound becomes a Windows Service

// 1. Service Entry Point
void WINAPI ServiceMain(DWORD argc, LPTSTR* argv) {
    // Register control handler
    service_status_handle = RegisterServiceCtrlHandler(
        SERVICE_NAME,
        hdlr  // Control handler function
    );
    
    // Report we're starting
    report_status(SERVICE_START_PENDING, NO_ERROR, 3000);
    
    // Create stop event
    service_stop_event = WSACreateEvent();
    
    // Start DNS server (calls main daemon code)
    start_daemon(config);
    
    // Report we're running
    report_status(SERVICE_RUNNING, NO_ERROR, 0);
    
    // Wait for stop signal
    // (Server runs in background threads)
}

// 2. Control Handler (receives stop/start commands)
static void hdlr(DWORD ctrl) {
    if (ctrl == SERVICE_CONTROL_STOP) {
        report_status(SERVICE_STOP_PENDING, NO_ERROR, 0);
        
        // Signal stop event
        WSASetEvent(service_stop_event);
        
        // Event loop will see this and exit gracefully
    }
}

// 3. In Event Loop - Check for stop
if (WSAWaitForMultipleEvents(...) sees service_stop_event) {
    // Clean shutdown
    cleanup_and_exit();
}
```

**For PowerDNS**: Similar pattern, integrate with existing shutdown logic.

---

## 🔑 **Key Pattern #5: Handling 64-Socket Limit**

**The Problem**: `WSAWaitForMultipleEvents` can only wait on 64 events at once.

**Unbound's Solution** (from winsock_event.h line 127):
```c
#define WSK_MAX_ITEMS 64
```

**Options**:
1. **Accept the limit** (fine for most DNS workloads)
2. **Use multiple threads** - each thread handles up to 64 sockets
3. **Upgrade to IOCP later** if needed

**Reality**: Most DNS servers don't need >64 concurrent connections per thread when using multiple threads.

---

## 📊 **API Mapping: libevent → Unbound's winsock_event**

| libevent API | Unbound Windows | Notes |
|--------------|-----------------|-------|
| `event_base_new()` | `event_init()` | Create event base |
| `event_new()` | `event_set()` | Create event |
| `event_add()` | `event_add()` | Start monitoring |
| `event_del()` | `event_del()` | Stop monitoring |
| `event_base_dispatch()` | `event_base_dispatch()` | Run event loop |
| `event_base_loopexit()` | `event_base_loopexit()` | Exit loop |

**Result**: Drop-in replacement API, minimal code changes!

---

## 🎯 **Lessons for PowerDNS POC**

### **1. Don't reinvent the wheel - Copy Unbound's approach!**

Unbound's `winsock_event.c` is **815 lines** of battle-tested code. PowerDNS can:
- ✅ Study and adapt this exact pattern
- ✅ Create similar `WSAEventsMultiplexer` class
- ✅ ~90% of code is reusable concept

### **2. Handle TCP sticky events**

PowerDNS has TCP support. Must implement:
```cpp
// In PowerDNS socket code
if (recv() returns WSAEWOULDBLOCK) {
    multiplexer->tcp_wouldblock(socket, EV_READ);
}
```

### **3. Use multiple threads, not multiple processes**

Windows doesn't have `fork()`:
```cpp
// PowerDNS already does this!
for (int i=0; i < num_threads; i++) {
    threads[i] = std::thread(worker_main, i);
}
// Each thread has its own event_base (up to 64 sockets each)
```

### **4. Windows Service is straightforward**

~200 lines of boilerplate code:
- Register service
- Handle SERVICE_CONTROL_STOP
- Report status
- Integrate with existing shutdown

---

## 📝 **POC Implementation Checklist**

### **Phase 1: Minimal I/O** (Week 1)
- [ ] Port `winsock_event.c` concepts to C++
- [ ] Create `WSAEventsMultiplexer` class
- [ ] Implement `addReadFD()`, `removeFD()`, `run()`
- [ ] Test with single UDP socket

### **Phase 2: UDP Queries** (Week 2)
- [ ] Integrate with PowerDNS's UDP query handler
- [ ] Test with `dig` queries
- [ ] Verify response handling

### **Phase 3: TCP Support** (Week 3)
- [ ] Implement TCP sticky events
- [ ] Add `tcp_wouldblock()` calls
- [ ] Test TCP queries

### **Phase 4: Threading** (Week 4)
- [ ] Remove `fork()` calls
- [ ] Use existing `std::thread` infrastructure
- [ ] Test multi-threaded

### **Phase 5: Windows Service** (Week 5)
- [ ] Port `win_svc.c` pattern
- [ ] Service install/uninstall
- [ ] Test as service

---

## 🚀 **Next Document to Create**

**`poc-implementation-guide.md`**
- Complete C++ code for `WSAEventsMultiplexer`
- Step-by-step integration with PowerDNS
- Build instructions
- Testing procedures

**Estimated time to create**: 1-2 days
**Estimated time to implement POC**: 4-5 weeks

---

## ✅ **Conclusion**

**Unbound proves Windows DNS servers work great without IOCP!**

Key takeaways:
1. **`WSAWaitForMultipleEvents` is sufficient** for DNS workloads
2. **Edge-to-level trigger** conversion is critical for TCP
3. **64-socket limit** is acceptable with threading
4. **Windows Service** integration is standard boilerplate
5. **~1000 lines of Windows-specific code** total

**PowerDNS can follow this exact pattern for POC!** 🎯


