# Testing makeUDPServerSockets() in Our POC

## Overview

This document explains what changes are required to test `makeUDPServerSockets()` in our current POC code instead of manual socket creation.

---

## Current State

**Location:** `main_test.cc:516-606`

```cpp
// Manual socket creation
g_udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
setsockopt(g_udp_socket, SO_REUSEADDR, ...);
bind(g_udp_socket, ...);
t_fdm->addReadFD(g_udp_socket, handleDNSQuery, param);
```

---

## Target State

**Use upstream's `makeUDPServerSockets()`:**

```cpp
// Use upstream function
deferredAdd_t deferredAdds;
makeUDPServerSockets(deferredAdds, log, true, 1);
// Register deferred sockets
for (const auto& deferred : deferredAdds) {
    t_fdm->addReadFD(deferred.first, deferred.second);
}
```

---

## Required Changes

### **1. Include Required Headers**

**Add to `main_test.cc`:**

```cpp
#include "pdns_recursor.hh"  // For makeUDPServerSockets declaration
#include "rec-main.hh"       // For deferredAdd_t type
#include "logger.hh"          // For Logr::log_t
```

**Or if these aren't available, we need to:**
- Copy `makeUDPServerSockets()` function to our codebase
- Copy `deferredAdd_t` typedef
- Create a minimal `Logr::log_t` wrapper

---

### **2. Define deferredAdd_t Type**

**Location:** `rec-main.hh:291`

```cpp
typedef vector<pair<int, std::function<void(int, boost::any&)>>> deferredAdd_t;
```

**Add to `main_test.cc` (if header not available):**

```cpp
#include <vector>
#include <functional>
#include <boost/any.hpp>

typedef std::vector<std::pair<int, std::function<void(int, boost::any&)>>> deferredAdd_t;
```

---

### **3. Set Up Configuration System**

**`makeUDPServerSockets()` requires `::arg()` configuration:**

```cpp
::arg()["local-address"]  // Default: "127.0.0.1,::1"
::arg()["local-port"]      // Default: 53
```

**Add to `main_test.cc` before calling `makeUDPServerSockets()`:**

```cpp
// Set configuration (if ::arg() system not initialized)
::arg().set("local-address") = "0.0.0.0";  // Or "127.0.0.1"
::arg().set("local-port") = "5533";        // Our test port
```

**Or create minimal config if `::arg()` not available:**

```cpp
// Minimal config wrapper (if needed)
class MinimalArg {
    std::map<std::string, std::string> values;
public:
    std::string& operator[](const std::string& key) {
        if (values.find(key) == values.end()) {
            values[key] = "";
        }
        return values[key];
    }
    int asNum(const std::string& key) {
        return std::stoi(values[key]);
    }
    bool mustDo(const std::string& key) {
        return values[key] == "yes" || values[key] == "true";
    }
};
```

---

### **4. Create Logr::log_t Wrapper**

**`makeUDPServerSockets()` requires `Logr::log_t log` parameter.**

**Option A: Use nullptr (if function allows it):**
```cpp
Logr::log_t log = nullptr;  // May not work if function uses log
```

**Option B: Create minimal logger wrapper:**
```cpp
// Minimal logger wrapper
class MinimalLog {
public:
    void info(const std::string& msg) { std::cout << msg << std::endl; }
    void warning(const std::string& msg) { std::cerr << msg << std::endl; }
    void error(const std::string& msg) { std::cerr << msg << std::endl; }
};

// Create wrapper
MinimalLog minimalLog;
Logr::log_t log = &minimalLog;  // If compatible
```

**Option C: Copy and modify function to use std::cout:**
- Copy `makeUDPServerSockets()` to our codebase
- Replace `log->info()` with `std::cout`
- Replace `SLOG()` with `std::cout`

---

### **5. Handle handleNewUDPQuestion**

**`makeUDPServerSockets()` registers sockets with `handleNewUDPQuestion`:**

```cpp
deferredAdds.emplace_back(socketFd, handleNewUDPQuestion);
```

**We have two options:**

**Option A: Use upstream's `handleNewUDPQuestion()`**
- Requires full upstream integration
- May need additional dependencies

**Option B: Create wrapper that calls our `handleDNSQuery()`**
```cpp
// Create wrapper function
void handleNewUDPQuestionWrapper(int fd, boost::any& param) {
    handleDNSQuery(fd, param);
}

// Modify deferredAdds after makeUDPServerSockets()
for (auto& deferred : deferredAdds) {
    deferred.second = handleNewUDPQuestionWrapper;  // Replace callback
}
```

**Option C: Copy and modify `makeUDPServerSockets()`**
- Copy function to our codebase
- Change `handleNewUDPQuestion` to `handleDNSQuery`
- Modify as needed

---

### **6. Handle Dependencies**

**`makeUDPServerSockets()` uses these functions (need to verify availability):**

- ✅ `socket()` - Available
- ✅ `bind()` - Available
- ✅ `setsockopt()` - Available
- ⚠️ `setNonBlocking()` - Need to check if available
- ⚠️ `setCloseOnExec()` - Windows equivalent needed
- ⚠️ `setSocketTimestamps()` - May not work on Windows
- ⚠️ `setSocketReceiveBuffer()` - Need to check
- ⚠️ `setSocketIgnorePMTU()` - May not work on Windows
- ⚠️ `FDWrapper` - Need to check if available
- ⚠️ `ComboAddress` - Need to check if available
- ⚠️ `stringtok()` - Need to check if available
- ⚠️ `stringerror()` - Need Windows version
- ⚠️ `PDNSException` - Need to check if available
- ⚠️ `SLOG()` - Need to check if available
- ⚠️ `g_log` - Need to check if available
- ⚠️ `g_reusePort` - Need to check if available
- ⚠️ `g_fromtosockets` - Need to check if available
- ⚠️ `g_listenSocketsAddresses` - Need to check if available

---

### **7. Windows-Specific Considerations**

**Socket options that may fail on Windows (should be handled gracefully):**

1. **`GEN_IP_PKTINFO`** (Linux-specific)
   - Windows equivalent: `IP_PKTINFO`
   - Function already handles failure (`if (setsockopt(...) == 0)`)

2. **`IPV6_RECVPKTINFO`** (Linux/BSD)
   - Windows equivalent: `IPV6_PKTINFO`
   - Already wrapped in `#ifdef IPV6_RECVPKTINFO`

3. **`IP_MTU_DISCOVER`** (Linux-specific)
   - May not work on Windows
   - Already wrapped in try-catch

4. **`setCloseOnExec()`**
   - Windows doesn't have `FD_CLOEXEC`
   - Need to check if function handles Windows

5. **`setSocketTimestamps()`**
   - May not work on Windows
   - Already handles failure

---

## Recommended Approach

### **Option 1: Copy and Modify Function (Easiest)**

**Steps:**
1. Copy `makeUDPServerSockets()` from `pdns-recursor/pdns_recursor.cc:2703`
2. Create a modified version in `main_test.cc`:
   - Replace `handleNewUDPQuestion` with `handleDNSQuery`
   - Replace `log->info()` with `std::cout`
   - Replace `SLOG()` with `std::cout`
   - Add Windows-specific socket options (SO_REUSEADDR)
   - Handle Windows-specific errors
   - Simplify dependencies (remove unused features)

**Advantages:**
- ✅ No dependency on upstream headers
- ✅ Can customize for our needs
- ✅ Easy to debug
- ✅ Can add Windows-specific fixes

**Disadvantages:**
- ❌ Code duplication
- ❌ Need to maintain compatibility

---

### **Option 2: Use Upstream Function with Wrappers (More Complex)**

**Steps:**
1. Include upstream headers
2. Set up `::arg()` configuration system
3. Create `Logr::log_t` wrapper
4. Create `handleNewUDPQuestion` wrapper that calls `handleDNSQuery`
5. Handle all dependencies

**Advantages:**
- ✅ Uses upstream code directly
- ✅ No code duplication
- ✅ Automatically gets upstream improvements

**Disadvantages:**
- ❌ Requires many dependencies
- ❌ More complex setup
- ❌ May need to modify upstream code

---

## Minimal Test Implementation (Option 1)

### **Step 1: Copy Function to `main_test.cc`**

```cpp
// Simplified version of makeUDPServerSockets for testing
unsigned int makeUDPServerSocketsTest(deferredAdd_t& deferredAdds) {
    std::vector<std::string> localAddresses;
    localAddresses.push_back("0.0.0.0");  // Hardcoded for test
    
    const uint16_t defaultLocalPort = 5533;  // Our test port
    
    for (const auto& localAddress : localAddresses) {
        // Create socket
        int socketFd = socket(AF_INET, SOCK_DGRAM, 0);
        if (socketFd < 0) {
            std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
            continue;
        }
        
        // Set SO_REUSEADDR (Windows-compatible)
        int reuse = 1;
        if (setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, 
                      (const char*)&reuse, sizeof(reuse)) < 0) {
            std::cerr << "Warning: SO_REUSEADDR failed" << std::endl;
        }
        
        // Set non-blocking (Windows)
#ifdef _WIN32
        u_long mode = 1;
        ioctlsocket(socketFd, FIONBIO, &mode);
#else
        int flags = fcntl(socketFd, F_GETFL, 0);
        fcntl(socketFd, F_SETFL, flags | O_NONBLOCK);
#endif
        
        // Bind
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(defaultLocalPort);
        
        if (bind(socketFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            std::cerr << "Failed to bind: " << strerror(errno) << std::endl;
            closesocket(socketFd);
            continue;
        }
        
        // Add to deferredAdds with our handler
        deferredAdds.emplace_back(socketFd, handleDNSQuery);
    }
    
    return localAddresses.size();
}
```

### **Step 2: Replace Manual Socket Creation**

**In `main_test.cc:516-606`, replace:**

```cpp
// OLD: Manual socket creation
g_udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
// ... bind, etc ...
t_fdm->addReadFD(g_udp_socket, handleDNSQuery, param);
```

**With:**

```cpp
// NEW: Use makeUDPServerSockets
deferredAdd_t deferredAdds;
unsigned int count = makeUDPServerSocketsTest(deferredAdds);
std::cout << "Created " << count << " UDP server sockets" << std::endl;

// Register deferred sockets
for (const auto& deferred : deferredAdds) {
    boost::any param = static_cast<evutil_socket_t>(deferred.first);
    t_fdm->addReadFD(deferred.first, deferred.second, param);
    std::cout << "Registered socket " << deferred.first << " with multiplexer" << std::endl;
}
```

---

## Testing Checklist

- [ ] Socket creation works
- [ ] Socket binding works
- [ ] Socket registration with multiplexer works
- [ ] Incoming queries are received
- [ ] `handleDNSQuery` is called correctly
- [ ] Multiple sockets work (if testing multiple addresses)
- [ ] Error handling works (invalid address, port in use, etc.)

---

## Summary

**Easiest approach:** Copy and modify `makeUDPServerSockets()` to create a simplified test version that:
- Uses our `handleDNSQuery` instead of `handleNewUDPQuestion`
- Uses `std::cout` instead of logging system
- Adds Windows-specific socket options
- Simplifies dependencies

**This allows us to:**
- ✅ Test the upstream socket creation pattern
- ✅ Verify Windows compatibility
- ✅ Identify any issues before full integration
- ✅ Keep our code independent

