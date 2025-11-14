# Windows Compatibility Changes for `makeUDPServerSockets()`

## Overview

This document records all Windows compatibility changes made to the `makeUDPServerSockets()` function in `pdns_recursor.cc`. The function is currently in an `#if 0` block and will be enabled when integrating into the main service flow.

**Function Location:** `pdns_recursor.cc:3997-4103`

---

## Change 1: Socket Creation Error Handling

### Problem
**Line 4016:** After `socket()` call fails, the code uses `stringerror()` without an error code parameter. On Windows, socket errors are retrieved via `WSAGetLastError()`, not `errno`.

### Change
```cpp
// BEFORE:
if (socketFd < 0) {
  throw PDNSException("Making a UDP server socket for resolver: " + stringerror());
}

// AFTER:
if (socketFd < 0) {
#ifdef _WIN32
  int err = WSAGetLastError();
#else
  int err = errno;
#endif
  throw PDNSException("Making a UDP server socket for resolver: " + stringerror(err));
}
```

### Impact
- **Windows:** Now correctly retrieves socket error using `WSAGetLastError()`
- **Linux/Unix:** Continues to use `errno` as before
- **Result:** Proper error messages on both platforms

---

## Change 2: IP_PKTINFO Socket Option (IPv4)

### Problem
**Line 4025:** `GEN_IP_PKTINFO` (or `IP_PKTINFO`) socket option is Linux-specific and not available on Windows. Windows uses a different mechanism for receiving packet information, or it's not available at all.

### Change
```cpp
// BEFORE:
if (address.sin4.sin_family == AF_INET) {
  if (setsockopt(socketFd, IPPROTO_IP, GEN_IP_PKTINFO, &one, sizeof(one)) == 0) {
    g_fromtosockets.insert(socketFd);
  }
}

// AFTER:
if (address.sin4.sin_family == AF_INET) {
#ifdef _WIN32
  // Windows: IP_PKTINFO is not available or works differently
  // Skip setting GEN_IP_PKTINFO on Windows - destination address will be determined via other means
  // (e.g., getsockname() or rplookup() fallback in handleNewUDPQuestion)
#else
  if (setsockopt(socketFd, IPPROTO_IP, GEN_IP_PKTINFO, &one, sizeof(one)) == 0) {
    g_fromtosockets.insert(socketFd);
  }
#endif
}
```

### Impact
- **Windows:** Skips setting `IP_PKTINFO` (not available)
- **Linux/Unix:** Continues to set `IP_PKTINFO` as before
- **Result:** Destination address determination falls back to `getsockname()` or `rplookup()` on Windows, which is already handled in `handleNewUDPQuestion()`

---

## Change 3: IPV6_RECVPKTINFO Socket Option (IPv6)

### Problem
**Line 4031:** `IPV6_RECVPKTINFO` may not be available on Windows, or works differently. The code is already wrapped in `#ifdef IPV6_RECVPKTINFO`, but we need to explicitly skip it on Windows.

### Change
```cpp
// BEFORE:
#ifdef IPV6_RECVPKTINFO
  if (address.sin4.sin_family == AF_INET6) {
    if (setsockopt(socketFd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &one, sizeof(one)) == 0) {
      g_fromtosockets.insert(socketFd);
    }
  }
#endif

// AFTER:
#ifdef IPV6_RECVPKTINFO
  if (address.sin4.sin_family == AF_INET6) {
#ifdef _WIN32
    // Windows: IPV6_RECVPKTINFO may not be available
    // Skip setting IPV6_RECVPKTINFO on Windows - destination address will be determined via other means
#else
    if (setsockopt(socketFd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &one, sizeof(one)) == 0) {
      g_fromtosockets.insert(socketFd);
    }
#endif
  }
#endif
```

### Impact
- **Windows:** Skips setting `IPV6_RECVPKTINFO` (may not be available)
- **Linux/Unix:** Continues to set `IPV6_RECVPKTINFO` as before
- **Result:** Same fallback mechanism as IPv4 for destination address determination

---

## Change 4: IPv6_V6ONLY Error Handling

### Problem
**Line 4037:** After `setsockopt()` for `IPV6_V6ONLY` fails, the code uses `errno` to get the error. On Windows, socket errors should be retrieved via `WSAGetLastError()`.

### Change
```cpp
// BEFORE:
if (address.sin6.sin6_family == AF_INET6 && setsockopt(socketFd, IPPROTO_IPV6, IPV6_V6ONLY, &one, sizeof(one)) < 0) {
  int err = errno;
  SLOG(g_log << Logger::Warning << "Failed to set IPv6 socket to IPv6 only, continuing anyhow: " << stringerror(err) << endl,
       log->error(Logr::Warning, err, "Failed to set IPv6 socket to IPv6 only, continuing anyhow"));
}

// AFTER:
if (address.sin6.sin6_family == AF_INET6 && setsockopt(socketFd, IPPROTO_IPV6, IPV6_V6ONLY, &one, sizeof(one)) < 0) {
#ifdef _WIN32
  int err = WSAGetLastError();
#else
  int err = errno;
#endif
  SLOG(g_log << Logger::Warning << "Failed to set IPv6 socket to IPv6 only, continuing anyhow: " << stringerror(err) << endl,
       log->error(Logr::Warning, err, "Failed to set IPv6 socket to IPv6 only, continuing anyhow"));
}
```

### Impact
- **Windows:** Now correctly retrieves socket error using `WSAGetLastError()`
- **Linux/Unix:** Continues to use `errno` as before
- **Result:** Proper error messages on both platforms

---

## Change 5: bind() Error Handling

### Problem
**Line 4084:** After `bind()` call fails, the code uses `errno` to get the error. On Windows, socket errors should be retrieved via `WSAGetLastError()`.

### Change
```cpp
// BEFORE:
if (::bind(socketFd, reinterpret_cast<struct sockaddr*>(&address), socklen) < 0) {
  int err = errno;
  if (!configIsDefault || address != ComboAddress{"::1", defaultLocalPort}) {
    throw PDNSException("Resolver binding to server socket on " + address.toStringWithPort() + ": " + stringerror(err));
  }
  log->info(Logr::Warning, "Cannot listen on this address, skipping", "proto", Logging::Loggable("UDP"), "address", Logging::Loggable(address), "error", Logging::Loggable(stringerror(err)));
  continue;
}

// AFTER:
if (::bind(socketFd, reinterpret_cast<struct sockaddr*>(&address), socklen) < 0) {
#ifdef _WIN32
  int err = WSAGetLastError();
#else
  int err = errno;
#endif
  if (!configIsDefault || address != ComboAddress{"::1", defaultLocalPort}) {
    throw PDNSException("Resolver binding to server socket on " + address.toStringWithPort() + ": " + stringerror(err));
  }
  log->info(Logr::Warning, "Cannot listen on this address, skipping", "proto", Logging::Loggable("UDP"), "address", Logging::Loggable(address), "error", Logging::Loggable(stringerror(err)));
  continue;
}
```

### Impact
- **Windows:** Now correctly retrieves socket error using `WSAGetLastError()`
- **Linux/Unix:** Continues to use `errno` as before
- **Result:** Proper error messages and exception handling on both platforms

---

## Summary of Changes

| Change | Line | Issue | Solution |
|--------|------|-------|----------|
| **1. Socket creation error** | 4016 | Uses `stringerror()` without error code | Use `WSAGetLastError()` on Windows, `errno` on Linux |
| **2. IP_PKTINFO (IPv4)** | 4025 | Not available on Windows | Skip on Windows, use fallback mechanisms |
| **3. IPV6_RECVPKTINFO (IPv6)** | 4031 | May not be available on Windows | Skip on Windows, use fallback mechanisms |
| **4. IPv6_V6ONLY error** | 4037 | Uses `errno` for socket error | Use `WSAGetLastError()` on Windows |
| **5. bind() error** | 4084 | Uses `errno` for socket error | Use `WSAGetLastError()` on Windows |

---

## Functions That Don't Need Changes

These functions are already Windows-compatible or have proper error handling:

1. **`setSocketTimestamps()`** (Line 4019)
   - Already has try-catch error handling
   - Returns `false` on failure, which is handled gracefully

2. **`setCloseOnExec()`** (Line 4046)
   - Already implemented as a no-op on Windows
   - No changes needed

3. **`setSocketReceiveBuffer()`** (Line 4049)
   - Already implemented for Windows
   - Wrapped in try-catch for error handling

4. **`SO_REUSEPORT`** (Lines 4056-4071)
   - Already wrapped in `#if defined(SO_REUSEPORT_LB)` / `#if defined(SO_REUSEPORT)`
   - Not defined on Windows, so code won't execute
   - No changes needed

5. **`setSocketIgnorePMTU()`** (Line 4075)
   - Already wrapped in `#if defined(IP_MTU_DISCOVER)` checks
   - Wrapped in try-catch for error handling
   - No changes needed

6. **`setNonBlocking()`** (Line 4092)
   - Already implemented for Windows
   - No changes needed

---

## Testing Notes

When this function is enabled (removed from `#if 0`), test:

1. **Socket creation:** Verify proper error messages on Windows if socket creation fails
2. **bind() errors:** Verify proper error messages if binding fails (e.g., port already in use)
3. **Destination address:** Verify that destination address determination works correctly on Windows without `IP_PKTINFO` (should fall back to `getsockname()` or `rplookup()`)
4. **IPv6 support:** Verify IPv6 sockets work correctly on Windows

---

## Related Files

- **`pdns_recursor.cc`:** Main file with `makeUDPServerSockets()` function
- **`iputils.cc`:** Contains helper functions like `setSocketIgnorePMTU()`, `setNonBlocking()`, etc.
- **`windows-compat.h`:** Windows compatibility header with `WSAGetLastError()` definitions
- **`handleNewUDPQuestion()`:** Uses fallback mechanisms for destination address when `IP_PKTINFO` is not available

---

## Status

✅ **All Windows compatibility changes have been applied**
- Function is ready for integration (currently in `#if 0` block)
- All error handling uses platform-appropriate methods
- Socket options that aren't available on Windows are skipped gracefully
- Fallback mechanisms are in place for destination address determination

