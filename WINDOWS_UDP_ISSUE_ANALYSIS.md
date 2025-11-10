# Windows UDP Socket Event Detection Issue - Root Cause Analysis

## Key Finding: Upstream Doesn't Have This Problem

**Upstream PowerDNS Recursor:**
- Uses **epoll** on Linux (level-triggered by default)
- **Does NOT run on Windows** - no Windows support in upstream codebase
- epoll is naturally level-triggered - it keeps reporting ready sockets until data is read
- No edge-triggered issues because epoll doesn't use EPOLLET flag

**Our Windows POC:**
- Uses **libevent's win32 backend** (only backend available)
- win32 backend uses **WSAEventSelect** internally
- **WSAEventSelect is edge-triggered** (despite Microsoft docs claiming level-triggered)
- Unbound documentation confirms: "WSAEventSelect page says it does level notify, but this is not true"

## The Problem

1. **WSAEventSelect Edge-Triggered Behavior:**
   - Fires once when data arrives
   - Won't fire again until you get WSAEWOULDBLOCK
   - For connected UDP sockets, may not fire at all for subsequent queries

2. **Connected UDP Sockets:**
   - We use `connect()` on UDP sockets (like upstream does)
   - WSAEventSelect doesn't reliably detect FD_READ for connected UDP sockets
   - First query works, subsequent queries fail

3. **Libevent Limitation:**
   - Only "win32" backend available in MSYS2/MinGW build
   - No "select" backend compiled in
   - Can't force select backend because it doesn't exist

## Evidence from Code

**Upstream epoll (level-triggered):**
```cpp
// pdns-recursor/epollmplexer.cc line 121
eevent.events = convertEventKind(kind);  // Just EPOLLIN/EPOLLOUT, no EPOLLET!
// No EPOLLET flag = level-triggered by default
```

**Our libevent win32 (edge-triggered):**
```cpp
// libevent's win32 backend uses WSAEventSelect internally
// WSAEventSelect is edge-triggered, not level-triggered
```

## Solution: Workaround Required

Since we can't change libevent's backend, we need to:
1. **Manually poll waiting sockets** using select() when libevent misses events
2. **Or implement "sticky events"** like Unbound does for TCP (but for UDP)
3. **Or hybrid approach** - use libevent for main loop, manual select() for UDP waiting sockets

The workaround needs to detect when data is available on waiting UDP sockets that libevent didn't detect.








