# Callback Chain for Outgoing UDP Sockets

## Registration (in `asendto()`)

```cpp
// Line 403 in pdns_recursor_poc_parts.cc
t_fdm->addReadFD(*fileDesc, handleUDPServerResponse, pident);
```

This registers:
- **Callback function**: `handleUDPServerResponse`
- **Parameter**: `pident` (PacketID with query info)

## Callback Chain When Data Arrives

### 1. **WSAEventSelect detects data** (Windows API level)
   - Windows detects data on the socket
   - Signals the event object that libevent is waiting on

### 2. **libevent calls `eventCallback()`** (libeventmplexer.cc line 194)
   ```cpp
   void LibeventFDMultiplexer::eventCallback(evutil_socket_t fd, short what, void* arg)
   ```
   - This is libevent's internal callback
   - `what` will have `EV_READ` flag set

### 3. **`eventCallback()` looks up registered callback** (line 206-210)
   ```cpp
   if (what & EV_READ) {
     const auto& iter = mplex->d_readCallbacks.find(fdInt);
     if (iter != mplex->d_readCallbacks.end()) {
       iter->d_callback(iter->d_fd, iter->d_parameter);  // ← THIS SHOULD CALL handleUDPServerResponse
     }
   }
   ```

### 4. **Should call `handleUDPServerResponse()`** (pdns_recursor_poc_parts.cc line 171)
   ```cpp
   static void handleUDPServerResponse(int fileDesc, FDMultiplexer::funcparam_t& var)
   ```
   - `fileDesc` = the socket fd
   - `var` = the `pident` (PacketID) we registered

## What Should Happen

When a response arrives on the socket:
1. ✅ WSAEventSelect detects it (Windows level)
2. ✅ libevent's `eventCallback()` is called
3. ✅ `eventCallback()` looks up callback in `d_readCallbacks`
4. ✅ **Should call `handleUDPServerResponse(fileDesc, pident)`**
5. ✅ `handleUDPServerResponse()` reads the response and notifies MTasker

## The Problem

For the second query, step 3-4 might be failing:
- Either `d_readCallbacks` doesn't have the entry for the new socket
- Or `eventCallback()` is never called by libevent
- Or the callback is found but the function isn't being invoked

## Debugging

Check logs for:
- `"[DEBUG] libevent: eventCallback fired for fd=..."` - confirms libevent detected event
- `"[DEBUG] libevent: calling read callback for fd=..."` - confirms callback lookup succeeded
- `"[DEBUG] handleUDPServerResponse: CALLED fd=..."` - confirms our function was called

If you see the first two but not the third, there's a bug in the callback invocation.
If you don't see any of them, libevent isn't detecting the event.








