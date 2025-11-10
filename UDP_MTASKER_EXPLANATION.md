# Why UDP Functions (`asendto`/`arecvfrom`) Need Proper Implementation

## The Problem

The current stub implementations in `lwres_stubs.cc` return `LWResult::Result::PermanentError` because they depend on three **thread-local** global variables that are **not initialized**:

1. **`g_multiTasker`** - `thread_local std::unique_ptr<MT_t>` (MTasker instance)
2. **`t_fdm`** - `thread_local std::unique_ptr<FDMultiplexer>` (I/O multiplexer)
3. **`t_udpclientsocks`** - `thread_local std::unique_ptr<UDPClientSocks>` (UDP socket manager)

## What Each Component Does

### 1. `g_multiTasker` (MTasker)
- **Purpose**: Manages cooperative multitasking for async I/O
- **Key Operations**:
  - `waitEvent()` - Blocks until a response arrives or timeout
  - `sendEvent()` - Notifies a waiting task that data is ready
  - `getWaiters()` - Returns list of tasks waiting for I/O
- **Usage in `asendto`**: 
  - Checks for existing queries that can be "chained" (line 296)
  - Used to coordinate async responses
- **Usage in `arecvfrom`**: 
  - Blocks waiting for UDP response (line 365)
  - Returns when response arrives or timeout

### 2. `t_fdm` (FDMultiplexer)
- **Purpose**: Registers/unregisters file descriptors for I/O events
- **Key Operations**:
  - `addReadFD(fd, callback, param)` - Register FD for reading
  - `removeReadFD(fd)` - Unregister FD
- **Usage in `asendto`**: 
  - Registers the UDP socket for reading after sending query (line 330)
  - Sets up callback `handleUDPServerResponse` to process incoming responses
- **We already have this!** - `libeventmplexer.cc` implements `FDMultiplexer`

### 3. `t_udpclientsocks` (UDPClientSocks)
- **Purpose**: Manages outbound UDP client sockets
- **Key Operations**:
  - `getSocket(toAddress, fileDesc)` - Gets a connected UDP socket to target
  - `returnSocket(fileDesc)` - Returns socket to pool
- **Usage in `asendto`**: 
  - Gets a socket to send query to upstream server (line 322)
  - Creates connected UDP socket to the target nameserver
- **This is a simple socket pool** - can be implemented

## Why Current Stubs Don't Work

Looking at `asendto` implementation (lines 281-342):

```cpp
LWResult::Result asendto(...) {
  // 1. Check for chaining (needs g_multiTasker->getWaiters())
  auto chain = g_multiTasker->getWaiters().equal_range(pident, ...);
  
  // 2. Get UDP socket (needs t_udpclientsocks)
  auto ret = t_udpclientsocks->getSocket(toAddress, fileDesc);
  
  // 3. Register FD for reading (needs t_fdm)
  t_fdm->addReadFD(*fileDesc, handleUDPServerResponse, pident);
  
  // 4. Send query
  send(*fileDesc, data, len, 0);
  
  return LWResult::Result::Success;
}
```

Our stub just returns `PermanentError`, so **no DNS queries can be resolved**.

## What `arecvfrom` Does

Looking at `arecvfrom` implementation (lines 346-400):

```cpp
LWResult::Result arecvfrom(...) {
  // Creates PacketID and waits for response
  auto pident = std::make_shared<PacketID>();
  // ... set pident fields ...
  
  // Blocks until response arrives or timeout
  int ret = g_multiTasker->waitEvent(pident, &packet, authWaitTimeMSec(g_multiTasker), &now);
  
  // Processes response
  if (ret > 0) {
    // Got response - validate and return
  } else if (ret == 0) {
    // Timeout
    return LWResult::Result::Timeout;
  } else {
    // Error
    return LWResult::Result::PermanentError;
  }
}
```

This is **cooperative blocking** - the MTasker yields to other tasks while waiting.

## How Upstream Initializes These

In `pdns_recursor.cc`, these are **thread-local** variables:

```cpp
thread_local std::unique_ptr<MT_t> g_multiTasker;
thread_local std::unique_ptr<FDMultiplexer> t_fdm;
thread_local std::unique_ptr<UDPClientSocks> t_udpclientsocks;
```

They are initialized in **worker thread startup code** (not shown in the excerpts, but typically in thread initialization functions). Each worker thread gets its own instances.

## What We Need to Do

### Option 1: Minimal Implementation (Recommended for POC)

1. **Initialize in `main_test.cc`**:
   ```cpp
   // After creating FDMultiplexer
   g_multiTasker = std::make_unique<MT_t>();
   t_fdm = std::make_unique<LibeventFDMultiplexer>(); // or reuse existing
   t_udpclientsocks = std::make_unique<UDPClientSocks>();
   ```

2. **Implement `UDPClientSocks` class** (simple socket pool):
   ```cpp
   class UDPClientSocks {
   public:
     LWResult::Result getSocket(const ComboAddress& toaddr, int* fileDesc);
     void returnSocket(int fileDesc);
   private:
     int makeClientSocket(int family);
     uint32_t d_numsocks = 0;
   };
   ```

3. **Implement `asendto` and `arecvfrom`** in `lwres_stubs.cc` using the real logic from upstream

### Option 2: Full Integration (For Production)

Copy the full implementation from `pdns_recursor.cc` lines 281-400, along with:
- `handleUDPServerResponse` callback function
- `UDPClientSocks` class implementation
- All helper functions

## Current State

- ✅ **`t_fdm`**: We have `libeventmplexer.cc` - just need to initialize it
- ❌ **`g_multiTasker`**: Not initialized - need to create MTasker instance
- ❌ **`t_udpclientsocks`**: Not implemented - need to create UDPClientSocks class

## Impact

**Without proper implementation:**
- All DNS queries will fail (stubs return `PermanentError`)
- `SyncRes::asyncresolveWrapper` will fail
- No recursive DNS resolution will work

**With proper implementation:**
- DNS queries can be sent to upstream servers
- Responses can be received asynchronously
- Full recursive resolution will work (for UDP queries)

## Next Steps

1. Add initialization of these globals in `main_test.cc`
2. Copy `UDPClientSocks` class from upstream
3. Replace stub `asendto`/`arecvfrom` with real implementations
4. Implement `handleUDPServerResponse` callback

This is **required** for any DNS resolution to work beyond the simple echo response in `main_test.cc`.











