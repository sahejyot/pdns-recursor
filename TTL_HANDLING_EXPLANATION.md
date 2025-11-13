# TTL (Time To Live) Handling in PowerDNS Recursor

## Overview
TTL (Time To Live) determines how long a DNS record can be cached. The TTL value flows through the system from the server response to the cache and back to client responses.

---

## 1. TTL Source: Server Response

**TTL comes from the authoritative server** in the DNS response packet. Each DNS record in the response includes a TTL field (32-bit unsigned integer, in seconds).

### Example:
```
github.com.  IN  A  140.82.121.3
             TTL: 300  (5 minutes)
```

---

## 2. TTL Processing Flow

### Step 1: Parse from Server Response
- **Location**: `dnsparser.cc` (MOADNSParser, PacketReader)
- **Action**: TTL is read directly from the DNS wire format packet
- **Result**: `DNSRecord.d_ttl` contains the TTL value from the server

### Step 2: Apply Minimum TTL Override (Optional)
- **Location**: `syncres.cc:5684-5688` (`processAnswer()`)
- **Code**:
  ```cpp
  if (s_minimumTTL != 0) {
    for (auto& rec : lwr.d_records) {
      rec.d_ttl = max(rec.d_ttl, s_minimumTTL);  // Cap TTL to minimum
    }
  }
  ```
- **Purpose**: Ensures TTL is never below a configured minimum (e.g., 30 seconds)
- **Configuration**: `minimum-ttl-override` argument

### Step 3: Cap to Maximum Cache TTL
- **Location**: `syncres.cc:4724` (`updateCacheFromRecords()`)
- **Code**:
  ```cpp
  rec.d_ttl = min(s_maxcachettl, rec.d_ttl);
  ```
- **Purpose**: Prevents caching records for too long (e.g., max 7 days)
- **Configuration**: `max-cache-ttl` argument (default: 15 seconds minimum)

### Step 4: Convert TTL to TTD (Time To Die) for Cache Storage
- **Location**: `syncres.cc` → `recursor_cache.cc:724-729` (`replace()`)
- **Process**:
  1. **Before calling `replace()`**: TTL is converted to TTD (Time To Die)
     ```cpp
     // In syncres.cc, before calling g_recCache->replace():
     record.d_ttl = now + ttl;  // Convert TTL to TTD
     ```
  2. **In `replace()`**: TTD is stored in cache
     ```cpp
     cacheEntry.d_ttd = min(maxTTD, static_cast<time_t>(record.d_ttl));
     cacheEntry.d_orig_ttl = cacheEntry.d_ttd - ttl_time;  // Store original TTL
     ```
- **Key Point**: The cache stores **TTD** (absolute time when record expires), not TTL (relative seconds)

### Step 5: Convert TTD back to TTL when Retrieving from Cache
- **Location**: `recursor_cache.cc:242` (`handleHit()`)
- **Code**:
  ```cpp
  result.d_ttl = static_cast<uint32_t>(entry->d_ttd);  // TTD is stored, but...
  ```
- **Actually**: When retrieving, TTD is converted back to TTL:
  ```cpp
  // In syncres.cc:2988-2991 (doCacheCheck)
  if (j->d_ttl > (unsigned int)d_now.tv_sec) {
    DNSRecord dnsRecord = *j;
    dnsRecord.d_ttl -= d_now.tv_sec;  // Convert TTD back to TTL
    dnsRecord.d_ttl = std::min(dnsRecord.d_ttl, capTTL);
  }
  ```

---

## 3. The Issue in `main_test.cc`

### Problem:
In `main_test.cc:67`, when manually creating root hints:
```cpp
arr.d_ttl = aaaarr.d_ttl = nsrr.d_ttl = now + 3600000;
```

**This is setting TTD (Time To Die), not TTL!**

### Why This Works:
When you call `g_recCache->replace()`, the function expects `record.d_ttl` to already contain **TTD** (absolute time), not TTL (relative seconds). The code comment in `recursor_cache.cc:724` confirms this:

```cpp
/* Yes, we have altered the d_ttl value by adding time(nullptr) to it
   prior to calling this function, so the TTL actually holds a TTD. */
cacheEntry.d_ttd = min(maxTTD, static_cast<time_t>(record.d_ttl));
```

### The Correct Pattern:
1. **For manual records** (like root hints):
   ```cpp
   record.d_ttl = now + desired_ttl_seconds;  // Set TTD
   g_recCache->replace(now, name, qtype, {record}, ...);
   ```

2. **For records from server responses**:
   ```cpp
   // TTL comes from server (already in seconds)
   // syncres.cc converts it to TTD before calling replace():
   record.d_ttl = now + original_ttl_from_server;
   g_recCache->replace(now, name, qtype, {record}, ...);
   ```

---

## 4. TTL Modifications by Recursor

The recursor can modify TTL values for various reasons:

### A. Minimum TTL Override
- **Location**: `syncres.cc:5684-5688`
- **Purpose**: Ensure TTL never goes below a minimum
- **Example**: If server sends TTL=10, but `minimum-ttl-override=30`, TTL becomes 30

### B. Maximum Cache TTL
- **Location**: `syncres.cc:4724`
- **Purpose**: Prevent extremely long cache times
- **Example**: If server sends TTL=604800 (7 days), but `max-cache-ttl=3600`, TTL becomes 3600

### C. ECS-Specific Minimum TTL
- **Location**: `syncres.cc:5692-5698`
- **Purpose**: Higher minimum TTL for ECS-specific answers
- **Configuration**: `ecs-minimum-ttl-override`

### D. Packet Cache TTL Capping
- **Location**: `pdns_recursor.cc:1199-1210` (`capPacketCacheTTL()`)
- **Purpose**: Different TTL limits for packet cache based on response type
- **Types**:
  - Negative responses (NXDomain): `packetcache-negative-ttl`
  - ServFail responses: `packetcache-servfail-ttl`
  - Normal responses: `packetcache-ttl`

### E. DNSSEC Validation State
- **Location**: `syncres.cc:2972`
- **Purpose**: Cap TTL for bogus (invalid) DNSSEC records
- **Configuration**: `max-cache-bogus-ttl`

---

## 5. TTL in Response Construction

When building a response to send to the client:

### From Cache:
- **Location**: `syncres.cc:2988-2991` (`doCacheCheck()`)
- **Process**:
  ```cpp
  if (j->d_ttl > (unsigned int)d_now.tv_sec) {  // Check if not expired
    DNSRecord dnsRecord = *j;
    dnsRecord.d_ttl -= d_now.tv_sec;  // Convert TTD to TTL
    dnsRecord.d_ttl = std::min(dnsRecord.d_ttl, capTTL);  // Cap if needed
    ret.push_back(dnsRecord);
  }
  ```

### From Fresh Resolution:
- **Location**: `pdns_recursor.cc:1332-1336` (`startDoResolve()`)
- **Process**:
  ```cpp
  uint32_t minTTL = comboWriter->d_ttlCap;  // Minimum TTL from resolver
  // Records from resolver already have correct TTL (relative seconds)
  // This minTTL is used to cap all record TTLs in the response
  ```

### When Writing to Packet:
- **Location**: `main_test.cc:478` or `pdns_recursor.cc` (via `DNSPacketWriter`)
- **Code**:
  ```cpp
  w.startRecord(rec.d_name, rec.d_type, rec.d_ttl, rec.d_class, rec.d_place);
  ```
- **Important**: At this point, `rec.d_ttl` must be in **relative seconds** (not TTD)

---

## 6. Summary: TTL vs TTD

| Stage | Value Type | Example | Notes |
|-------|------------|---------|-------|
| **Server Response** | TTL (seconds) | `300` | Relative time from now |
| **After Parsing** | TTL (seconds) | `300` | Still relative |
| **After Min/Max Caps** | TTL (seconds) | `300` (or capped value) | Still relative |
| **Before `replace()`** | **TTD** (absolute time) | `now + 300` | Converted to absolute time |
| **In Cache (`d_ttd`)** | **TTD** (absolute time) | `1704067200` | Stored as absolute timestamp |
| **After Retrieval** | TTL (seconds) | `300` | Converted back to relative |
| **In Response Packet** | TTL (seconds) | `300` | Relative seconds for client |

---

## 7. Common Issues and Fixes

### Issue 1: Setting TTL Instead of TTD
**Problem**: Setting `record.d_ttl = 3600` before `replace()`
**Fix**: Set `record.d_ttl = now + 3600` (TTD)

### Issue 2: Using TTD in Response
**Problem**: Using TTD value when writing response packet
**Fix**: Convert TTD to TTL: `ttl = ttd - now`

### Issue 3: Negative TTL
**Problem**: TTL becomes negative when TTD < now (expired)
**Fix**: Check `if (ttd > now)` before converting to TTL

---

## 8. Configuration Options

| Option | Default | Purpose |
|--------|---------|---------|
| `minimum-ttl-override` | `0` (disabled) | Minimum TTL for all records |
| `ecs-minimum-ttl-override` | `0` (disabled) | Minimum TTL for ECS-specific answers |
| `max-cache-ttl` | `15` (minimum) | Maximum TTL for record cache |
| `max-negative-ttl` | `3600` | Maximum TTL for negative cache entries |
| `max-cache-bogus-ttl` | `3600` | Maximum TTL for bogus DNSSEC records |
| `packetcache-ttl` | `3600` | Maximum TTL for packet cache |
| `packetcache-negative-ttl` | `60` | Maximum TTL for negative packet cache entries |
| `packetcache-servfail-ttl` | `60` | Maximum TTL for ServFail packet cache entries |

---

## 9. Key Code Locations

### TTL Processing:
- **Parse from packet**: `dnsparser.cc` (MOADNSParser, PacketReader)
- **Apply minimum TTL**: `syncres.cc:5684-5688` (`processAnswer()`)
- **Cap to max TTL**: `syncres.cc:4724` (`updateCacheFromRecords()`)
- **Convert to TTD**: `syncres.cc` (before `g_recCache->replace()`)
- **Store in cache**: `recursor_cache.cc:724-729` (`replace()`)
- **Retrieve from cache**: `recursor_cache.cc:242` (`handleHit()`)
- **Convert TTD to TTL**: `syncres.cc:2988-2991` (`doCacheCheck()`)
- **Write to response**: `main_test.cc:478` or `DNSPacketWriter::startRecord()`

### TTL Configuration:
- **Initialize**: `rec-main.cc:1809-1813` (`initSyncRes()`)
- **Arguments**: `arguments.cc` (argument definitions)

---

## 10. The `main_test.cc` Root Hints Case

In `main_test.cc:67`, the code sets:
```cpp
arr.d_ttl = aaaarr.d_ttl = nsrr.d_ttl = now + 3600000;
```

**This is correct!** Because:
1. `g_recCache->replace()` expects TTD (absolute time), not TTL (relative seconds)
2. Setting `now + 3600000` means the record expires in ~41 days
3. When retrieved from cache, the TTD is converted back to TTL: `ttl = ttd - now`

**If you had set `d_ttl = 3600000` (without `now +`)**:
- The cache would store `d_ttd = 3600000` (absolute timestamp in 1970!)
- The record would appear expired immediately
- Retrieval would fail or return negative TTL

---

## Conclusion

**TTL comes from the server**, but the recursor can modify it (minimum/maximum caps). The cache stores **TTD** (absolute time), not TTL (relative seconds). When manually creating records (like root hints), you must set `d_ttl = now + desired_ttl_seconds` to provide TTD, not TTL.

