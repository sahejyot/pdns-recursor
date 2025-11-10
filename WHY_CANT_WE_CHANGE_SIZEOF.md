# Why We Can't Change `sizeof(dnsheader)` to 12 on Windows

## The Question

Can we add a condition for Windows that if the struct size is 14 bytes, we "set it to 12"?

## The Answer: **No, We Cannot**

### Why `sizeof()` Cannot Be Changed

`sizeof()` is a **compile-time operator** that returns the actual size of a type in memory. It's not a variable we can modify - it's determined by the compiler based on the struct's memory layout.

```c++
struct dnsheader {
    uint16_t id;              // 2 bytes
    unsigned rd :1;           // Bitfields: 2 bytes
    // ... more bitfields ...
    // PADDING: 2 bytes (added by MinGW compiler)
    uint16_t qdcount;         // 2 bytes
    // ... more fields ...
};

// On Windows: sizeof(dnsheader) == 14 (compile-time constant)
// We CANNOT change this - it's what the compiler calculates!
```

### What We Could Try (But Won't Work)

#### Option 1: Restructure the Struct
We could define a Windows-specific struct without bitfields:

```c++
#ifdef _WIN32
struct dnsheader {
    uint16_t id;
    uint16_t flags;  // Instead of bitfields
    uint16_t qdcount;
    // ...
};
#endif
```

**Problem**: This breaks compatibility with upstream code that uses:
- `dh->rd = 1;` (bitfield access)
- `dh->qr = 1;` (bitfield access)
- `dh->rcode = 0;` (bitfield access)

Upstream code expects bitfields, not a `uint16_t flags` field.

#### Option 2: Use a Proxy/Wrapper
We tried this in `dnsheader_writer_proxy.hh`, but C++ cannot intercept direct member writes:

```c++
// This DOESN'T work in C++:
dh->rd = 1;  // We can't intercept this write
```

C++ doesn't have property accessors like C# or Python. Once you have a pointer to a struct, writes go directly to memory.

#### Option 3: Force Compiler to Pack Differently
We already use `#pragma pack(push,1)` and `GCCPACKATTRIBUTE`, but MinGW still adds padding after bitfields. This is **compiler behavior** - we can't override it.

### The Correct Solution: Manual Conversion

We **accept** that `sizeof(dnsheader) == 14` on Windows and handle the conversion manually:

1. **When writing (struct → wire format)**:
   - Copy bytes 0-3 (ID + Flags) ✓
   - Skip bytes 4-5 (padding) ✗
   - Copy bytes 6-13 → wire bytes 4-11 ✓

2. **When reading (wire format → struct)**:
   - Copy wire bytes 0-3 → struct bytes 0-3 ✓
   - Leave struct bytes 4-5 as padding ✗
   - Copy wire bytes 4-11 → struct bytes 6-13 ✓

This is implemented in:
- `dnswriter.cc` constructor (struct → wire)
- `pdns_recursor_poc_parts.cc` `handleUDPServerResponse` (wire → struct)

### Summary

- ❌ **Cannot change `sizeof()`** - it's a compile-time constant
- ❌ **Cannot restructure struct** - breaks upstream compatibility
- ❌ **Cannot use proxy** - C++ can't intercept member writes
- ✅ **Can handle conversion manually** - this is what we do

The padding fix is the **correct and only viable solution** for Windows.



