# SO_REUSEPORT on Windows: Configuration and Behavior

## Overview

**SO_REUSEPORT** is a Linux/BSD feature that allows multiple sockets to bind to the same address:port combination. This enables kernel-level load balancing across multiple threads/processes.

**Windows does NOT have SO_REUSEPORT**, but upstream PowerDNS Recursor handles this automatically.

---

## How Upstream Handles SO_REUSEPORT

### **1. Default Behavior**

**Location:** `pdns_recursor.cc:81`

```cpp
bool g_reusePort{false};  // Defaults to false
```

### **2. Configuration Check**

**Location:** `rec-main.cc:1933-1935`

```cpp
#ifdef SO_REUSEPORT
  g_reusePort = ::arg().mustDo("reuseport");
#endif
```

**Key Points:**
- `g_reusePort` is **only set** if `SO_REUSEPORT` is **defined** (available on the platform)
- On Windows, `SO_REUSEPORT` is **not defined**, so `g_reusePort` **stays `false`**
- **No configuration needed** - it's automatic!

### **3. Socket Creation Logic**

**Location:** `rec-main.cc:1939-1971`

```cpp
if (g_reusePort) {
    // WITH SO_REUSEPORT: Each thread gets its own socket
    for (unsigned int i = 0; i < RecThreadInfo::numUDPWorkers(); i++) {
        auto& info = RecThreadInfo::info(threadNum);
        auto& deferredAdds = info.getDeferredAdds();
        makeUDPServerSockets(deferredAdds, ...);  // Separate socket per thread
    }
}
else {
    // WITHOUT SO_REUSEPORT: All threads share the same socket
    makeUDPServerSockets(s_deferredUDPadds, ...);  // Shared socket
}
```

### **4. Socket Registration**

**Location:** `rec-main.cc:2979-2991`

```cpp
if (threadInfo.isListener()) {
    if (g_reusePort) {
        // Each thread registers its own socket
        for (const auto& deferred : threadInfo.getDeferredAdds()) {
            t_fdm->addReadFD(deferred.first, deferred.second);
        }
    }
    else {
        // All threads register the same shared socket
        for (const auto& deferred : s_deferredUDPadds) {
            t_fdm->addReadFD(deferred.first, deferred.second);
        }
    }
}
```

---

## Windows Behavior (Automatic)

### **What Happens on Windows:**

1. **`SO_REUSEPORT` is not defined** → `g_reusePort` stays `false`
2. **Upstream automatically uses shared sockets** → All threads listen on the same socket
3. **No configuration needed** → It just works!

### **Flow on Windows:**

```
initDistribution()
  └─> g_reusePort = false (SO_REUSEPORT not defined)
  └─> else branch (line 1966)
       └─> makeUDPServerSockets(s_deferredUDPadds, ...)
            └─> Creates ONE socket per address:port
            └─> Adds to s_deferredUDPadds (shared)

recursorThread() (each thread)
  └─> else branch (line 2986)
       └─> for (deferred : s_deferredUDPadds)
            └─> t_fdm->addReadFD(deferred.first, deferred.second)
                 └─> All threads register the SAME socket
```

**Result:** All threads share the same listening socket. The multiplexer (`t_fdm`) distributes incoming queries to available threads.

---

## SO_REUSEADDR vs SO_REUSEPORT

### **SO_REUSEADDR (Available on Windows):**

- **Purpose:** Allows binding to an address:port that's in TIME_WAIT state
- **Use Case:** Quick restart after server shutdown
- **Windows:** ✅ Available and works correctly
- **Upstream:** Uses `setReuseAddr()` function (sets `SO_REUSEADDR`)

### **SO_REUSEPORT (NOT Available on Windows):**

- **Purpose:** Allows multiple sockets to bind to the same address:port simultaneously
- **Use Case:** Kernel-level load balancing across threads/processes
- **Windows:** ❌ Not available
- **Upstream:** Automatically falls back to shared sockets

### **Key Difference:**

| Feature | SO_REUSEADDR | SO_REUSEPORT |
|---------|--------------|--------------|
| **Available on Windows** | ✅ Yes | ❌ No |
| **Purpose** | Quick restart | Load balancing |
| **Multiple Sockets** | ❌ No | ✅ Yes |
| **Upstream Uses** | ✅ Yes (via `setReuseAddr()`) | ✅ Yes (if available) |

---

## Configuration Options

### **On Linux/BSD (SO_REUSEPORT available):**

```yaml
# recursor.yml
reuseport: yes  # Enable SO_REUSEPORT (default: yes since 4.9.0)
```

**Effect:**
- Each worker thread gets its own socket
- Kernel distributes queries across sockets
- Better performance on multi-core systems

### **On Windows (SO_REUSEPORT NOT available):**

```yaml
# recursor.yml
reuseport: yes  # Ignored - SO_REUSEPORT not available
```

**Effect:**
- Setting is ignored (not even checked)
- Automatically uses shared sockets
- All threads listen on the same socket
- Multiplexer distributes queries to threads

**No configuration needed!** Upstream handles it automatically.

---

## Performance Implications

### **With SO_REUSEPORT (Linux/BSD):**

- ✅ Kernel-level load balancing
- ✅ Reduces "thundering herd" problem
- ✅ Better performance on multi-core systems
- ✅ Each thread has its own socket

### **Without SO_REUSEPORT (Windows):**

- ✅ All threads share one socket
- ✅ Multiplexer (`t_fdm`) distributes queries
- ✅ Still works correctly
- ⚠️ Slightly less efficient than kernel-level distribution
- ⚠️ All threads compete for the same socket

**Note:** The performance difference is usually minimal for DNS workloads. The multiplexer efficiently distributes queries.

---

## What We Need to Do for Windows

### **Nothing! It's Already Handled:**

1. ✅ **`g_reusePort` defaults to `false`** → Correct for Windows
2. ✅ **`SO_REUSEPORT` check is `#ifdef`** → Won't be set on Windows
3. ✅ **Upstream uses shared sockets** → Correct fallback
4. ✅ **No configuration needed** → Automatic

### **Optional: Ensure SO_REUSEADDR is Set**

While `SO_REUSEPORT` is not needed, `SO_REUSEADDR` should be set for quick restarts.

**Current Status:**
- ✅ TCP sockets: `SO_REUSEADDR` is set (`rec-tcp.cc:1123`)
- ❌ UDP sockets: `SO_REUSEADDR` is **NOT** set in `makeUDPServerSockets()`

**For Windows Integration:**
We may want to add `SO_REUSEADDR` to UDP sockets in `makeUDPServerSockets()`:

```cpp
// In makeUDPServerSockets(), before bind()
int reuse = 1;
if (setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, 
               (const char*)&reuse, sizeof(reuse)) < 0) {
    // Log warning but continue (not critical)
}
```

**Note:** This is optional but recommended for quick restarts on Windows.

---

## Summary

| Question | Answer |
|----------|--------|
| **Can we configure SO_REUSEPORT on Windows?** | ❌ No - it's not available |
| **Do we need to configure it?** | ❌ No - upstream handles it automatically |
| **What happens on Windows?** | ✅ Automatically uses shared sockets (all threads share one socket) |
| **Is this correct?** | ✅ Yes - this is the correct fallback behavior |
| **Do we need SO_REUSEADDR?** | ✅ Yes - for quick restarts (should already be set) |
| **Performance impact?** | ⚠️ Minimal - multiplexer handles distribution efficiently |

**Conclusion:** No configuration needed. Upstream automatically detects that `SO_REUSEPORT` is not available and uses the shared socket approach, which is correct for Windows.

