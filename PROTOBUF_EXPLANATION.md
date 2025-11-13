# Protocol Buffers (Protobuf) in PowerDNS Recursor

## What is Protocol Buffers (Protobuf)?

**Protocol Buffers (protobuf)** is a language-neutral, platform-neutral serialization format developed by Google. It's used to:
- **Serialize structured data** (like JSON or XML, but binary and more efficient)
- **Exchange data** between different systems/services
- **Store data** in a compact, efficient format

### Key Characteristics:
- **Binary format** - More compact than text-based formats (JSON, XML)
- **Language agnostic** - Can be used with C++, Java, Python, Go, etc.
- **Schema-based** - Uses `.proto` files to define message structures
- **Efficient** - Fast serialization/deserialization, smaller message sizes
- **Type-safe** - Strongly typed based on the schema

---

## How Protobuf is Used in PowerDNS Recursor

### Purpose: DNS Query/Response Logging

PowerDNS Recursor uses protobuf to **log DNS queries and responses** to external logging systems. This enables:
- **DNS analytics** - Track DNS queries and responses
- **Security monitoring** - Detect malicious queries, DDoS attacks
- **Performance analysis** - Analyze query patterns, response times
- **Compliance** - Log DNS activity for audit purposes
- **Integration** - Send DNS data to external systems (SIEM, analytics platforms)

---

## Protobuf Message Structure

### Message Definition File
**File:** `pdns-recursor/dnsmessage.proto`

The protobuf schema defines a `PBDNSMessage` structure that contains:

```protobuf
message PBDNSMessage {
  enum Type {
    DNSQueryType = 1;              // Query received by the service
    DNSResponseType = 2;           // Response returned by the service
    DNSOutgoingQueryType = 3;      // Query sent out by the service
    DNSIncomingResponseType = 4;   // Response from remote server
  }
  
  required Type type = 1;
  optional bytes messageId = 2;    // UUID linking query and response
  optional bytes serverIdentity = 3;
  optional SocketFamily socketFamily = 4;
  optional SocketProtocol socketProtocol = 5;  // UDP, TCP, DoT, DoH, etc.
  optional bytes from = 6;          // Client IP address
  optional bytes to = 7;            // Server IP address
  optional uint64 inBytes = 8;      // Packet size
  optional uint32 timeSec = 9;      // Timestamp (seconds)
  optional uint32 timeUsec = 10;     // Timestamp (microseconds)
  optional uint32 id = 11;           // DNS message ID
  
  message DNSQuestion {
    optional string qName = 1;      // Domain name queried
    optional uint32 qType = 2;       // Query type (A, AAAA, MX, etc.)
    optional uint32 qClass = 3;     // Query class (usually IN)
  }
  optional DNSQuestion question = 12;
  
  message DNSResponse {
    optional uint32 rcode = 1;       // Response code (NOERROR, NXDOMAIN, etc.)
    repeated DNSRR rrs = 2;         // Resource records in response
    optional string appliedPolicy = 3;  // RPZ policy applied
    repeated string tags = 4;       // Policy tags
    optional VState validationState = 11;  // DNSSEC validation state
  }
  optional DNSResponse response = 13;
  
  // ... more fields for EDNS, policy, device info, etc.
}
```

---

## Two Types of Protobuf Logging

### 1. Incoming Query/Response Logging (`protobufExportConfig`)

**Purpose:** Log queries received from clients and responses sent back to clients

**Configuration:**
- Configured via `protobufServer()` in Lua config or YAML
- Logs:
  - **Queries** received from DNS clients
  - **Responses** sent back to DNS clients
  - Client IP addresses (with optional masking)
  - Query names, types, response codes
  - Applied policies (RPZ)
  - DNSSEC validation state

**Code Location:**
- **Configuration:** `rec-lua-conf.cc` - `parseProtobufOptions()`
- **Initialization:** `rec-main.cc:483` - `checkProtobufExport()`
- **Logging:** `rec-main.cc:537` - `protobufLogQuery()`
- **Logging:** `rec-main.cc:607` - `protobufLogResponse()`

### 2. Outgoing Query/Response Logging (`outgoingProtobufExportConfig`)

**Purpose:** Log queries sent to upstream servers and responses received from them

**Configuration:**
- Configured via `outgoingProtobufServer()` in Lua config or YAML
- Logs:
  - **Queries** sent to authoritative/upstream DNS servers
  - **Responses** received from authoritative/upstream servers
  - Upstream server addresses
  - Query/response details for recursive resolution

**Code Location:**
- **Configuration:** `rec-lua-conf.cc` - `parseProtobufOptions()`
- **Initialization:** `rec-main.cc:510` - `checkOutgoingProtobufExport()`
- **Usage:** Passed to `SyncRes` for logging during recursive resolution

---

## How It Works

### 1. Configuration

Protobuf logging is configured in the Lua configuration file or YAML:

```lua
-- Log incoming queries/responses
protobufServer("192.168.1.100:5300", {
    logQueries = true,
    logResponses = true,
    exportTypes = {A, AAAA, MX, TXT},
    taggedOnly = false
})

-- Log outgoing queries/responses
outgoingProtobufServer("192.168.1.101:5300", {
    logQueries = true,
    logResponses = true,
    exportTypes = {A, AAAA}
})
```

### 2. Initialization

**File:** `pdns-recursor/rec-main.cc:483`

```cpp
bool checkProtobufExport(LocalStateHolder<LuaConfigItems>& luaconfsLocal)
{
    if (!luaconfsLocal->protobufExportConfig.enabled) {
        // Disable if not configured
        if (t_protobufServers.servers) {
            t_protobufServers.servers.reset();
        }
        return false;
    }
    
    // Start protobuf logger threads
    if (t_protobufServers.generation < luaconfsLocal->generation) {
        t_protobufServers.servers = startProtobufServers(
            luaconfsLocal->protobufExportConfig, log);
        t_protobufServers.config = luaconfsLocal->protobufExportConfig;
    }
    
    return true;
}
```

**What happens:**
- Creates `RemoteLogger` instances for each configured protobuf server
- Each logger connects to the remote protobuf server (TCP socket)
- Loggers run in separate threads to avoid blocking DNS processing

### 3. Logging Queries

**File:** `pdns-recursor/pdns_recursor.cc:2796`

When a DNS query is received:

```cpp
// In doProcessUDPQuestion()
if (t_protobufServers.servers && logQuery) {
    protobufLogQuery(luaconfsLocal, uniqueId, source, destination, 
                     mappedSource, ednssubnet, false, question.size(),
                     qname, qtype, qclass, policyTags, ...);
}
```

**Function:** `rec-main.cc:537` - `protobufLogQuery()`

```cpp
void protobufLogQuery(...)
{
    // Create protobuf message
    pdns::ProtoZero::RecMessage msg{128, 64};
    msg.setType(pdns::ProtoZero::Message::MessageType::DNSQueryType);
    msg.setRequest(uniqueId, requestor, local, qname, qtype, qclass, 
                   header.id, transport, len);
    msg.setServerIdentity(SyncRes::s_serverID);
    msg.setEDNSSubnet(ednssubnet, mask);
    // ... set more fields ...
    
    // Send to all configured protobuf servers
    for (auto& server : *t_protobufServers.servers) {
        server->queueData(msg.toString());
    }
}
```

### 4. Logging Responses

**File:** `pdns-recursor/pdns_recursor.cc:2831`

When a DNS response is sent:

```cpp
// In doProcessUDPQuestion() after resolution
if (t_protobufServers.servers && logResponse) {
    protobufLogResponse(qname, qtype, dnsheader, luaconfsLocal, 
                        pbData, tval, false, source, destination, ...);
}
```

**Function:** `rec-main.cc:607` - `protobufLogResponse()`

```cpp
void protobufLogResponse(...)
{
    // Create protobuf message
    pdns::ProtoZero::RecMessage pbMessage(...);
    pbMessage.setType(pdns::ProtoZero::Message::MessageType::DNSResponseType);
    pbMessage.setQueryTime(tval.tv_sec, tval.tv_usec);
    pbMessage.setFrom(requestor);
    pbMessage.setTo(destination);
    pbMessage.setId(header->id);
    
    // Add response details
    pbMessage.setResponse(rcode, ...);
    for (const auto& record : responseRecords) {
        pbMessage.addRR(record, exportTypes, udr);
    }
    
    // Send to all configured protobuf servers
    for (auto& server : *t_protobufServers.servers) {
        server->queueData(pbMessage.toString());
    }
}
```

### 5. Remote Logger

**File:** `pdns-recursor/remote_logger.cc`

The `RemoteLogger` class:
- Maintains a **TCP connection** to the protobuf server
- **Queues messages** in a thread-safe buffer
- **Sends messages** asynchronously in a background thread
- **Handles reconnection** if connection is lost
- **Drops messages** if queue is full (to avoid memory issues)

---

## Thread-Local Storage

Protobuf servers are stored in **thread-local variables**:

```cpp
// File: rec-main.hh:364
extern thread_local ProtobufServersInfo t_protobufServers;
extern thread_local ProtobufServersInfo t_outgoingProtobufServers;
```

**Why thread-local?**
- Each worker thread has its own protobuf logger instances
- Avoids locking/contention between threads
- Better performance for high-throughput DNS servers

---

## Configuration Options

**File:** `pdns-recursor/rec-lua-conf.hh:43`

```cpp
struct ProtobufExportConfig {
    std::vector<ComboAddress> servers;  // List of protobuf server addresses
    std::set<uint16_t> exportTypes;      // RR types to export (A, AAAA, etc.)
    unsigned timeout{2};                 // Connection timeout (seconds)
    unsigned maxQueuedEntries{100};     // Max queued messages
    unsigned reconnectWaitTime{1};      // Reconnect delay (seconds)
    bool asyncConnect{true};            // Async connection
    bool enabled{false};                // Enable/disable
    bool logQueries{true};              // Log queries
    bool logResponses{true};            // Log responses
    bool taggedOnly{false};             // Only log tagged queries
    bool logMappedFrom{false};          // Log mapped source (proxy)
};
```

---

## Data Flow Diagram

```
┌─────────────────────────────────────────────────────────────┐
│ DNS Query Received                                          │
│ (doProcessUDPQuestion)                                      │
└─────────────────────────────────────────────────────────────┘
                        │
                        ▼
        ┌───────────────────────────────┐
        │ Check if protobuf enabled     │
        │ t_protobufServers.servers?    │
        └───────────────────────────────┘
                        │
                        ▼ (if enabled)
        ┌───────────────────────────────┐
        │ protobufLogQuery()             │
        │ - Create PBDNSMessage          │
        │ - Set query fields             │
        │ - Serialize to protobuf        │
        └───────────────────────────────┘
                        │
                        ▼
        ┌───────────────────────────────┐
        │ RemoteLogger::queueData()     │
        │ - Add to thread-safe queue    │
        └───────────────────────────────┘
                        │
                        ▼
        ┌───────────────────────────────┐
        │ Background Thread             │
        │ - Send via TCP socket         │
        │ - Handle reconnection         │
        └───────────────────────────────┘
                        │
                        ▼
        ┌───────────────────────────────┐
        │ External Protobuf Server      │
        │ (Analytics, SIEM, etc.)        │
        └───────────────────────────────┘
```

---

## Example Use Cases

### 1. DNS Analytics
- Track most queried domains
- Analyze query patterns by time, client IP
- Monitor query volume and types

### 2. Security Monitoring
- Detect DNS tunneling
- Identify malicious domains
- Track DDoS attacks
- Monitor policy violations (RPZ)

### 3. Performance Analysis
- Measure response times
- Track cache hit rates
- Analyze upstream server performance

### 4. Compliance & Auditing
- Log all DNS activity
- Track who queried what
- Maintain audit trails

### 5. Integration with SIEM
- Send DNS logs to Splunk, ELK, etc.
- Correlate DNS with other security events
- Real-time threat detection

---

## Key Files

| File | Purpose |
|------|---------|
| `dnsmessage.proto` | Protobuf schema definition |
| `rec-lua-conf.hh/cc` | Protobuf configuration parsing |
| `rec-main.cc` | Protobuf initialization and logging functions |
| `pdns_recursor.cc` | Calls protobuf logging during query processing |
| `remote_logger.cc` | TCP client for sending protobuf messages |
| `protozero.hh/cc` | Protobuf serialization library (ProtoZero) |

---

## Disabling Protobuf (Windows POC)

In the Windows POC implementation, protobuf is **disabled by default**:

**File:** `pdns_recursor_windows/pdns_recursor.cc:172`

```cpp
// Protobuf stubs (disabled)
bool checkProtobufExport(const LocalStateHolder<LuaConfigItems>& /* luaconfsLocal */) { 
    return false; 
}
void checkOutgoingProtobufExport(const LocalStateHolder<LuaConfigItems>& /* luaconfsLocal */) { }
void protobufLogResponse(pdns::ProtoZero::RecMessage& /* pbMessage */) { }
```

**Why disabled?**
- Protobuf is an **optional feature** for logging/analytics
- Not required for core DNS resolution functionality
- Reduces dependencies and complexity for minimal setup
- Can be enabled later if needed

---

## Summary

**Protobuf in PowerDNS Recursor:**
- **What:** Binary serialization format for logging DNS queries/responses
- **Why:** Enable DNS analytics, security monitoring, compliance
- **How:** Serializes DNS data to protobuf messages and sends to external servers
- **Where:** Configured via Lua/YAML, logged during query processing
- **Status:** Optional feature, disabled in Windows POC by default

Protobuf provides a **standardized, efficient way** to export DNS activity data to external systems for analysis, monitoring, and compliance purposes.

