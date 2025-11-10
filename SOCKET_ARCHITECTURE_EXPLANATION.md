# Socket Architecture - Two Separate Event Loops

## Two Different Multiplexers

### 1. `g_multiplexer` - Main Event Loop (Incoming Queries)
- **Purpose**: Listens for incoming DNS queries from clients
- **Socket**: `g_udp_socket` (bound to port 5533)
- **Backend**: libevent win32 (via `LibeventFDMultiplexer`)
- **Status**: ✅ **WORKS FINE** - This is NOT the problem
- **Function**: `handleDNSQuery()` processes incoming client queries

### 2. `t_fdm` - Outgoing UDP Sockets (SyncRes Queries)
- **Purpose**: Manages outgoing UDP sockets for querying upstream DNS servers
- **Sockets**: Created dynamically by `UDPClientSocks::getSocket()` for each upstream query
- **Backend**: libevent win32 (via `LibeventFDMultiplexer`)
- **Status**: ❌ **THE PROBLEM** - WSAEventSelect is edge-triggered for connected UDP sockets
- **Function**: `handleUDPServerResponse()` processes responses from upstream servers

## The Problem: Edge-Triggered Behavior on Outgoing Sockets

### Flow:
1. `SyncRes::doResolve()` needs to query upstream server (e.g., root server)
2. `asendto()` is called:
   - `UDPClientSocks::getSocket()` creates a UDP socket
   - Socket is `connect()`ed to upstream server (e.g., 198.41.0.4:53)
   - Query is sent via `send()`
   - Socket is registered with `t_fdm->addReadFD(fd, handleUDPServerResponse, pident)`
3. `arecvfrom()` waits for response:
   - `g_multiTasker->waitEvent(pident, ...)` blocks waiting
   - Main event loop calls `t_fdm->run()` to process I/O
   - **PROBLEM**: WSAEventSelect (libevent win32 backend) is edge-triggered
   - Response arrives but WSAEventSelect doesn't fire `handleUDPServerResponse`
   - `waitEvent()` times out

### Why It's Edge-Triggered:
- libevent's win32 backend uses `WSAEventSelect()` internally
- `WSAEventSelect()` is edge-triggered (despite Microsoft docs claiming level-triggered)
- For connected UDP sockets, it may not detect subsequent responses
- First query might work, but second query fails

## Evidence:

```cpp
// Main event loop (WORKS FINE):
g_multiplexer->addReadFD(g_udp_socket, handleDNSQuery, param);  // ✅ Works

// Outgoing sockets (THE PROBLEM):
t_fdm->addReadFD(fileDesc, handleUDPServerResponse, pident);  // ❌ Edge-triggered issue
```

## Solution:

Manually poll waiting UDP sockets using `select()` when libevent misses events:
- Check `g_multiTasker->getWaiters()` for waiting sockets
- Use `select()` to detect readiness
- Manually trigger `handleUDPServerResponse` when data is available








