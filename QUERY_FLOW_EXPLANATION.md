# Query Flow - Understanding the Problem

## First Query (Works ✅)

1. **`asendto()` is called** for query 1 (e.g., root NS query)
   - `getSocket()` creates **NEW socket** (e.g., `fd=544`)
   - Socket is `connect()`ed to upstream server (e.g., `198.41.0.4:53`)
   - Query is sent via `send()`
   - `addReadFD(fd=544, handleUDPServerResponse, pident)` registers with libevent

2. **`arecvfrom()` waits** for response
   - `waitEvent(pident, ...)` blocks in MTasker
   - Main event loop calls `t_fdm->run()` to process I/O

3. **Response arrives**
   - libevent detects data on `fd=544`
   - `handleUDPServerResponse(fd=544, ...)` callback is called
   - Response is processed, `sendEvent()` notifies waiting task
   - `returnSocket(fd=544)` is called:
     - `removeReadFD(fd=544)` unregisters from libevent
     - `closesocket(fd=544)` closes the socket
     - Socket `fd=544` is now closed

## Second Query (Fails ❌)

1. **`asendto()` is called** for query 2 (e.g., google.com TLD query)
   - `getSocket()` creates **NEW socket** (e.g., `fd=516` or possibly reused `fd=544`)
   - Socket is `connect()`ed to upstream server (e.g., `192.48.79.30:53`)
   - Query is sent via `send()`
   - `addReadFD(fd=516, handleUDPServerResponse, pident)` registers with libevent

2. **`arecvfrom()` waits** for response
   - `waitEvent(pident, ...)` blocks in MTasker
   - Main event loop calls `t_fdm->run()` to process I/O

3. **Response arrives BUT...**
   - ❌ libevent callback `handleUDPServerResponse()` is **NOT called**
   - `waitEvent()` times out
   - Query fails

## Key Observations

1. **Each query uses a NEW socket** - `getSocket()` always creates a new socket via `makeClientSocket()`
2. **Socket is closed after use** - `returnSocket()` closes the socket
3. **Windows may reuse fd numbers** - If `fd=544` is closed, Windows might reuse that fd number for the next socket
4. **First query works, second fails** - Suggests libevent's win32 backend state might be getting confused

## Possible Causes

1. **Libevent win32 backend state confusion**:
   - When socket is closed and a new one is created with the same fd number
   - WSAEventSelect might not properly re-register for the new socket
   - Our fix in `addFD()` tries to handle this by removing stale events

2. **WSAEventSelect not detecting events for new sockets**:
   - Even though Microsoft says it's level-triggered
   - There might be an issue with how libevent handles WSAEventSelect for newly created sockets

3. **Timing issue**:
   - Event might be registered after data arrives
   - But this seems unlikely since we register before sending

4. **Socket state issue**:
   - Connected UDP socket state might not be properly set up
   - WSAEventSelect might require additional setup for connected UDP sockets








