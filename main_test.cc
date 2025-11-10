/*
 * PowerDNS Recursor Windows - POC Main Program
 * Sprint 4: Basic DNS Resolution with Simple Resolver (No Socket Dependencies)
 */

#include <iostream>
#include <stdexcept>
#include <cstring>
#include <cstdint>
#include <vector>
#include <string>
#include <boost/any.hpp>

// PowerDNS includes
#include "mplexer.hh"
#include "dnsname.hh"
#include "qtype.hh"
#include "dnsparser.hh"
#include "dnswriter.hh"
#include "dnsrecords.hh"
#include "recursor_cache.hh"
#include "negcache.hh"
#include "misc.hh"
#include "syncres.hh"
#include "mtasker.hh"
#include "utility.hh"
#include "dns.hh"  // For RCode
#include "root-addresses.hh"  // For root hints
#include <event2/util.h>  // For evutil_make_socket_nonblocking
#include <iomanip>
#include <thread>
#include "dnsrecords.hh"  // For reportAllTypes

// Global caches (defined in globals_stub.cc, declared here as extern)
extern std::unique_ptr<MemRecursorCache> g_recCache;
extern std::unique_ptr<NegCache> g_negCache;

// Forward declarations for thread-local variables from pdns_recursor_poc_parts.cc
// These will be initialized in main() before starting the event loop
typedef MTasker<std::shared_ptr<PacketID>, PacketBuffer, PacketIDCompare> MT_t;
extern thread_local std::unique_ptr<MT_t> g_multiTasker;
extern thread_local std::unique_ptr<FDMultiplexer> t_fdm;

// Initialization function to avoid incomplete type issues
extern void initializeMTaskerInfrastructure();

// Prime root hints into cache (based on putDefaultHintsIntoCache from reczones-helpers.cc)
static void primeRootHints(time_t now)
{
    // Use boost::none for 'from' parameter so root hints are accessible regardless of query source
    // Root NS records should be globally accessible, not filtered by client IP
    const boost::optional<ComboAddress> from = boost::none;
    std::vector<DNSRecord> nsvec;

    DNSRecord arr;
    DNSRecord aaaarr;
    DNSRecord nsrr;

    nsrr.d_name = g_rootdnsname;
    arr.d_type = QType::A;
    aaaarr.d_type = QType::AAAA;
    nsrr.d_type = QType::NS;
    // TTL of 3600000 seconds (about 41 days) for root hints
    arr.d_ttl = aaaarr.d_ttl = nsrr.d_ttl = now + 3600000;

    std::string templ = "a.root-servers.net.";

    // Both arrays have 13 entries (a.root through m.root)
    for (size_t letter = 0; letter < rootIps4.size() && letter < rootIps6.size(); ++letter) {
        templ.at(0) = static_cast<char>(letter + 'a');
        aaaarr.d_name = arr.d_name = DNSName(templ);
        nsrr.setContent(std::make_shared<NSRecordContent>(DNSName(templ)));
        nsvec.push_back(nsrr);

        if (!rootIps4.at(letter).empty()) {
            arr.setContent(std::make_shared<ARecordContent>(ComboAddress(rootIps4.at(letter))));
            // Insert as non-auth so real responses from root servers can update these records
            // Use boost::none for 'from' so root nameserver A records are accessible regardless of query source
            g_recCache->replace(now, DNSName(templ), QType::A, {arr}, {}, {}, false, g_rootdnsname, boost::none, boost::none, vState::Insecure, from);
        }
        if (!rootIps6.at(letter).empty()) {
            aaaarr.setContent(std::make_shared<AAAARecordContent>(ComboAddress(rootIps6.at(letter))));
            g_recCache->replace(now, DNSName(templ), QType::AAAA, {aaaarr}, {}, {}, false, g_rootdnsname, boost::none, boost::none, vState::Insecure, from);
        }
    }

    // Store root NS records in cache
    // Use boost::none for 'from' so root NS records are accessible regardless of query source (d_cacheRemote)
    g_recCache->doWipeCache(g_rootdnsname, false, QType::NS);
    g_recCache->replace(now, g_rootdnsname, QType::NS, nsvec, {}, {}, false, g_rootdnsname, boost::none, boost::none, vState::Insecure, from);
    
    std::cout << "Primed root hints: " << nsvec.size() << " NS records" << std::endl;
}

// Global variables for the DNS server
static int g_udp_socket = -1;

// Minimal wire-format question parser (fallback when MOADNSParser fails)
static bool parseWireName(const char* data, size_t size, size_t& pos, std::string& out)
{
    out.clear();
    size_t startPos = pos;
    bool jumped = false;
    size_t safety = 0;
    while (pos < size && safety++ < 128) {
        uint8_t len = static_cast<uint8_t>(data[pos]);
        if (len == 0) {
            if (!jumped) pos += 1; // consume the root label only if not jumped
            return true;
        }
        if ((len & 0xC0) == 0xC0) { // pointer
            if (pos + 1 >= size) return false;
            uint16_t off = ((len & 0x3F) << 8) | static_cast<uint8_t>(data[pos + 1]);
            if (off >= size) return false;
            if (!jumped) pos += 2; // consume pointer only once
            jumped = true;
            size_t p = off;
            std::string suffix;
            if (!parseWireName(data, size, p, suffix)) return false;
            if (!out.empty() && !suffix.empty()) out.push_back('.');
            out.append(suffix);
            return true;
        }
        // label
        pos += 1;
        if (pos + len > size) return false;
        if (!out.empty()) out.push_back('.');
        out.append(&data[pos], &data[pos] + len);
        pos += len;
    }
    // safety exceeded or out of bounds
    pos = startPos;
    return false;
}

static bool parseWireQuestion(const char* data, size_t size, std::string& outQName, uint16_t& outQType, uint16_t& outQClass)
{
    if (size < 12) return false;
    size_t pos = 12; // after header
    if (!parseWireName(data, size, pos, outQName)) return false;
    if (pos + 4 > size) return false;
    outQType = (static_cast<uint8_t>(data[pos]) << 8) | static_cast<uint8_t>(data[pos+1]);
    outQClass = (static_cast<uint8_t>(data[pos+2]) << 8) | static_cast<uint8_t>(data[pos+3]);
    return true;
}

// Job structure for passing DNS query context to MTasker task
struct ResolveJob {
    evutil_socket_t sock;
    sockaddr_in to;
    std::string fqdn;
    uint16_t qtype;
    uint16_t qclass;
    uint16_t qid;
    uint8_t rd;
};

// Forward declaration
static void resolveTaskFunc(void* pv);

// DNS query handler
void handleDNSQuery(int fd, boost::any& param) {
#ifdef _WIN32
    using NativeFD = evutil_socket_t;
#else
    using NativeFD = int;
#endif
    NativeFD sockfd = static_cast<NativeFD>(fd);
    if (!param.empty()) {
        try {
            sockfd = boost::any_cast<NativeFD>(param);
        } catch (const boost::bad_any_cast&) {
            // ignore, fall back to fd
        }
    }
    // Note: This will be called from either:
    // 1. LIBEVENT/WSAEventSelect (primary) - logged in libeventmplexer.cc
    // 2. Manual select() workaround (fallback) - logged before this call
    std::cout << "[handleDNSQuery] CALLED! fd=" << fd << std::endl;
    
    char buffer[512];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Receive DNS query
    ssize_t bytes_received = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                                     (struct sockaddr*)&client_addr, &client_len);

    std::cout << "[handleDNSQuery] Called, bytes_received=" << bytes_received;
    if (bytes_received > 0) {
        std::cout << " from " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port);
    }
    std::cout << std::endl;
    
    if (bytes_received <= 0) {
        if (bytes_received < 0) {
#ifdef _WIN32
            const int werr = WSAGetLastError();
            if (werr == WSAEWOULDBLOCK) {
                // No data yet on non-blocking socket, try again later
                return;
            }
            std::cerr << "Error receiving DNS query: WSA " << werr << ", sockfd=" << (uint64_t)sockfd << std::endl;
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }
            std::cerr << "Error receiving DNS query: " << strerror(errno) << " (errno=" << errno << ")" << std::endl;
#endif
        }
        return;
    }
    
    std::cout << "Received DNS query (" << bytes_received << " bytes) from " 
              << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << std::endl;
    
    // Always use wire-question parser to bypass MOADNSParser packing issues
    // Debug hexdump first bytes
    std::cout << "[DEBUG] First bytes:";
    for (int i = 0; i < (bytes_received < 32 ? bytes_received : 32); ++i) {
        unsigned v = static_cast<unsigned char>(buffer[i]);
        std::cout << ' ' << std::hex << std::setw(2) << std::setfill('0') << v;
    }
    std::cout << std::dec << std::setfill(' ') << std::endl;
    uint16_t qid = (static_cast<uint8_t>(buffer[0]) << 8) | static_cast<uint8_t>(buffer[1]);
    std::string fqdn; uint16_t qtypeWire = 1; uint16_t qclassWire = 1;
    
    // TEST: Try MOADNSParser first (with fallback to wire parser)
    bool useMOADNSParser = false;
    try {
        std::string queryPacket(buffer, bytes_received);
        MOADNSParser mdp(true, queryPacket);
        // MOADNSParser worked! Use it
        fqdn = mdp.d_qname.toString();
        qtypeWire = mdp.d_qtype;
        qclassWire = mdp.d_qclass;
        qid = ntohs(mdp.d_header.id);
        useMOADNSParser = true;
        std::cout << "[DEBUG] MOADNSParser SUCCESS: qname=" << fqdn << " qtype=" << qtypeWire << " qclass=" << qclassWire << " qid=" << qid << std::endl;
    } catch (const std::exception& e) {
        // MOADNSParser failed, fall back to wire parser
        std::cout << "[DEBUG] MOADNSParser failed: " << e.what() << ", falling back to wire parser" << std::endl;
        useMOADNSParser = false;
    }
    
    // Fallback: Use wire parser if MOADNSParser failed
    if (!useMOADNSParser) {
        std::cout << "[DEBUG] Using wire parser..." << std::endl;
        if (!parseWireQuestion(buffer, static_cast<size_t>(bytes_received), fqdn, qtypeWire, qclassWire)) {
            std::cout << "[DEBUG] Wire parser failed, sending minimal SERVFAIL" << std::endl;
            // Minimal SERVFAIL
            if (bytes_received >= 12) {
                unsigned char h[12];
                memcpy(h, buffer, 12);
                h[2] |= 0x80; // QR=1
                h[3] = 0x82;  // RA=1, RCODE=2
                sendto(sockfd, reinterpret_cast<const char*>(h), 12, 0,
                       (struct sockaddr*)&client_addr, client_len);
            }
            return;
        }
        std::cout << "[DEBUG] Wire parser OK: qname=" << fqdn << ", qtype=" << qtypeWire << ", qclass=" << qclassWire << std::endl;
    }
    // Hand off resolution so the event loop can keep running (like upstream)
    const uint8_t rdflag = (static_cast<uint8_t>(buffer[2]) & 0x01);
    sockaddr_in replyAddr = client_addr; // copy
    evutil_socket_t sendSock = sockfd;
    auto* job = new ResolveJob{ sendSock, replyAddr, fqdn, qtypeWire, qclassWire, qid, rdflag };
    if (g_multiTasker) {
        g_multiTasker->makeThread(resolveTaskFunc, job);
        std::cout << "[DEBUG] Enqueued resolve task to MTasker" << std::endl;
    } else {
        resolveTaskFunc(job);
    }
    // Return quickly to keep pumping the event loop
}

// MTasker task function for DNS resolution
// CRITICAL: Match upstream pattern exactly - create SyncRes at start, let it be destroyed when function returns
// Upstream: pdns_recursor.cc:973-2017 - startDoResolve() creates resolver at line 976, uses it throughout, destroys when function returns
static void resolveTaskFunc(void* pv) {
    std::cout << "[DEBUG] MT: task started" << std::endl;
    // Ensure thread-local MTasker infrastructure exists in this worker thread
    if (!t_fdm) {
        initializeMTaskerInfrastructure();
        std::cout << "[DEBUG] MT: initialized thread-local t_fdm and UDP client socks" << std::endl;
    }
    std::unique_ptr<ResolveJob> j(static_cast<ResolveJob*>(pv));
    timeval now{};
    Utility::gettimeofday(&now, nullptr);
    
    // Initialize t_sstorage.domainmap if needed (required by getBestAuthZone/getBestNSFromCache)
    if (!SyncRes::t_sstorage.domainmap) {
        SyncRes::t_sstorage.domainmap = std::make_shared<SyncRes::domainmap_t>();
        std::cout << "[DEBUG] MT: initialized t_sstorage.domainmap (empty, no forwarders)" << std::endl;
    }
    // Initialize t_Counters in this thread by accessing it (triggers thread-local initialization)
    // This is needed because t_Counters is thread_local and needs to be initialized per thread
    // t_Counters is defined in syncres.cc as: thread_local rec::TCounters t_Counters(g_Counters);
    // We need to ensure it's initialized by accessing it
    try {
        // Access t_Counters through SyncRes to trigger initialization
        // The actual initialization happens in syncres.cc, but accessing it here ensures
        // the thread-local variable is initialized in this worker thread
        extern thread_local rec::TCounters t_Counters;
        extern rec::GlobalCounters g_Counters;
        // Try to access a counter to force initialization
        try {
            t_Counters.at(rec::Counter::outqueries);
            std::cout << "[DEBUG] MT: t_Counters accessed successfully" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[DEBUG] MT: t_Counters access failed: " << e.what() << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[DEBUG] MT: t_Counters initialization exception: " << e.what() << std::endl;
    }
    
    // Initialize SyncRes static members that are normally set by initSyncRes
    // These need to be set before creating SyncRes instances
    if (SyncRes::s_maxqperq == 0) {
        SyncRes::s_maxqperq = 50; // Default max queries per query (upstream uses ::arg().asNum("max-qperq"))
        std::cout << "[DEBUG] MT: initialized SyncRes::s_maxqperq=" << SyncRes::s_maxqperq << std::endl;
    }
    
    // CRITICAL: Match upstream pattern - create SyncRes at start of function, destroy when function returns
    // Upstream: pdns_recursor.cc:976 - SyncRes resolver(comboWriter->d_now);
    // Upstream lets resolver be destroyed when startDoResolve() function returns (line 2017)
    std::cout << "[DEBUG] MT: about to construct SyncRes resolver for " << j->fqdn << std::endl;
    SyncRes resolver(now);
    std::cout << "[DEBUG] MT: SyncRes resolver constructed for " << j->fqdn << std::endl;
    
    try {
        // Note: We don't set a custom async callback - SyncRes will use the global asyncresolve()
        // function which calls our asendto/arecvfrom functions (like upstream does)
        // Provide client source to resolver (helps ECS/selection)
        ComboAddress src(std::string(inet_ntoa(j->to.sin_addr)));
        resolver.setQuerySource(src, boost::none);
        std::cout << "[DEBUG] MT: query source set" << std::endl;
        // Root hints are already manually primed at startup via primeRootHints()
        // No need to do root NS priming via beginResolve() as it may overwrite manually primed hints
        // with entries that have different cache keys (d_cacheRemote/ednsmask)
        // The manually primed hints use boost::none for both, making them globally accessible
        std::cout << "[DEBUG] MT: Skipping root NS priming via beginResolve (using manually primed hints instead)" << std::endl;
        std::cout << "[DEBUG] MT: About to resolve: fqdn=\"" << j->fqdn << "\" qtype=" << j->qtype << " qclass=" << j->qclass << std::endl;
        DNSName queryName(j->fqdn);
        std::cout << "[DEBUG] MT: DNSName constructed: \"" << queryName << "\" wirelength()=" << queryName.wirelength() << std::endl;
        std::vector<DNSRecord> ret;
        int rcode = resolver.beginResolve(queryName, QType(j->qtype), QClass(j->qclass), ret);
        std::cout << "[DEBUG] MT: beginResolve done: rcode=" << rcode << " (RCode::NXDomain=" << RCode::NXDomain << "), records=" << ret.size() << std::endl;
        // Debug: Log all records for NXDOMAIN
        if (rcode == RCode::NXDomain) {
            std::cout << "[DEBUG] MT: NXDOMAIN response - logging all records:" << std::endl;
            for (const auto& rec : ret) {
                std::cout << "[DEBUG] MT:   Record - name=" << rec.d_name << " type=" << rec.d_type << " place=" << static_cast<int>(rec.d_place) << " ttl=" << rec.d_ttl << std::endl;
            }
        }
        std::vector<uint8_t> resp;
        DNSPacketWriter w(resp, DNSName(j->fqdn), j->qtype, j->qclass);
        
        // ========================================================================
        // WINDOWS FIX: DNS Header Padding in Response Construction (main_test.cc)
        // ========================================================================
        // PROBLEM: On Windows (MinGW), sizeof(dnsheader) = 14 bytes due to struct padding,
        //          but DNS wire format header is always 12 bytes. Using w.getHeader()->id
        //          or w.getHeader()->ra writes to struct fields which may be misaligned.
        //
        // WHY THIS FIX IS NEEDED:
        //   1. w.getHeader() returns a pointer to a 14-byte struct, but the wire format
        //      buffer is only 12 bytes. Writing through the struct pointer may corrupt
        //      adjacent bytes or write to wrong offsets.
        //   2. Struct fields store values in host byte order, but DNS wire format requires
        //      network byte order. Direct writes to wire format bytes ensure correct byte order.
        //
        // SOLUTION: Write ID and flags directly to wire format bytes using htons() for
        //           byte order conversion and bitwise operations for flags.
        //
        // CRITICAL: DO NOT REMOVE THIS FIX - it is essential for correct response
        //           construction on Windows. Without it, responses will be malformed.
        // ========================================================================
        uint8_t* respPtr = resp.data();
        if (resp.size() >= 12) {
          // Write ID (bytes 0-1) in network byte order
          uint16_t idNetwork = htons(j->qid);
          respPtr[0] = (idNetwork >> 8) & 0xFF;
          respPtr[1] = idNetwork & 0xFF;
          
          // Write flags (bytes 2-3) in network byte order
          // DNS wire format: Byte 2 = QR(7), Opcode(6-3), AA(2), TC(1), RD(0)
          //                  Byte 3 = RA(7), Z(6), AD(5), CD(4), RCODE(3-0)
          // When combined as 16-bit (byte2 << 8) | byte3:
          // Bit 15 = QR, Bit 8 = RD, Bit 7 = RA, Bits 3-0 = RCODE
          // Upstream sets: aa=0, ra=1, qr=1, tc=0, rd=query.rd, cd=query.cd
          uint16_t flags = 0x8000; // QR=1 (response, bit 15)
          flags |= 0x0080; // RA=1 (recursion available, bit 7)
          // AA=0 (not authoritative, bit 10) - already 0
          // TC=0 (not truncated, bit 9) - already 0
          if (j->rd) {
            flags |= 0x0100; // RD=1 (recursion desired, bit 8) - echo query RD flag
          }
          flags |= (rcode & 0x0F); // RCODE in bits 3-0
          respPtr[2] = (flags >> 8) & 0xFF;
          respPtr[3] = flags & 0xFF;
        }
        
        // OLD CODE (commented out for potential revert):
        // We were calling commit() after each record, but upstream pattern is:
        // - Add all records without commit() in loop
        // - Call commit() once after all records (pdns_recursor.cc:1574-1575)
        // bool added = false;
        // int answerCount = 0;
        // for (const auto& rec : ret) {
        //     if (rec.d_place == DNSResourceRecord::ANSWER || rec.d_place == DNSResourceRecord::AUTHORITY || rec.d_place == DNSResourceRecord::ADDITIONAL) {
        //         std::cout << "[DEBUG] MT: Adding record - name=" << rec.d_name << " type=" << rec.d_type << " place=" << static_cast<int>(rec.d_place) << " ttl=" << rec.d_ttl << std::endl;
        //         w.startRecord(rec.d_name, rec.d_type, rec.d_ttl, rec.d_class, rec.d_place);
        //         rec.getContent()->toPacket(w);
        //         w.commit(); // OLD: commit() called after each record
        //         if (rec.d_place == DNSResourceRecord::ANSWER) {
        //             answerCount++;
        //         }
        //         added = true;
        //     }
        // }
        // std::cout << "[DEBUG] MT: Added " << answerCount << " ANSWER records, total records=" << ret.size() << std::endl;
        // if (added) {
        //     w.commit(); // OLD: commit() called again at end
        // }
        
        // NEW CODE: Match upstream pattern - add records without commit() in loop, then commit() once at end
        bool added = false;
        int answerCount = 0;
        for (const auto& rec : ret) {
            if (rec.d_place == DNSResourceRecord::ANSWER || rec.d_place == DNSResourceRecord::AUTHORITY || rec.d_place == DNSResourceRecord::ADDITIONAL) {
                std::cout << "[DEBUG] MT: Adding record - name=" << rec.d_name << " type=" << rec.d_type << " place=" << static_cast<int>(rec.d_place) << " ttl=" << rec.d_ttl << std::endl;
                w.startRecord(rec.d_name, rec.d_type, rec.d_ttl, rec.d_class, rec.d_place);
                rec.getContent()->toPacket(w);
                // Note: startRecord() calls commit() internally to finalize previous record
                // So we don't need to call commit() here - just once at the end
                if (rec.d_place == DNSResourceRecord::ANSWER) {
                    answerCount++;
                }
                added = true;
            }
        }
        std::cout << "[DEBUG] MT: Added " << answerCount << " ANSWER records, total records=" << ret.size() << std::endl;
        // Match upstream pattern: commit() once after all records (pdns_recursor.cc:1574-1575)
        if (added) {
            w.commit();
        }
        
        // OLD CODE (commented out for potential revert):
        // We were writing header fields here AFTER all commits, but upstream sets them BEFORE adding records
        // // CRITICAL FIX: Write header fields directly to wire format bytes AFTER commit() to avoid struct padding issues
        // uint8_t* respPtr = resp.data();
        // if (resp.size() >= 12) {
        //   // Write ID (bytes 0-1) in network byte order
        //   uint16_t idNetwork = htons(j->qid);
        //   respPtr[0] = (idNetwork >> 8) & 0xFF;
        //   respPtr[1] = idNetwork & 0xFF;
        //   // ... rest of header writing code ...
        // }
        
        // Debug: Verify header fields after commit()
        respPtr = resp.data();
        if (resp.size() >= 12) {
          // Read count fields from wire format (network byte order, big-endian)
          // Bytes 4-5 = QDCOUNT, bytes 6-7 = ANCOUNT, etc.
          // Network byte order: high byte first, then low byte
          // So bytes [4][5] = 0x00 0x01 means value 1 in network byte order
          // When we combine them: (respPtr[4] << 8) | respPtr[5], we get the value as if it were in host byte order
          // But the bytes are in network byte order, so we need to interpret them correctly
          // CRITICAL FIX: The bytes are already in network byte order in the packet.
          // When we read (respPtr[4] << 8) | respPtr[5], we're creating a host byte order representation
          // of a network byte order value. We should NOT call ntohs() - the value is already correct!
          // OLD CODE (commented out for potential revert):
          // uint16_t qdcount_raw = (static_cast<uint16_t>(respPtr[4]) << 8) | static_cast<uint16_t>(respPtr[5]);
          // uint16_t qdcount = ntohs(qdcount_raw); // WRONG - this swaps bytes incorrectly
          // Instead, read the bytes directly - they're already in the correct order for interpretation
          uint16_t qdcount = (static_cast<uint16_t>(respPtr[4]) << 8) | static_cast<uint16_t>(respPtr[5]);
          uint16_t ancount = (static_cast<uint16_t>(respPtr[6]) << 8) | static_cast<uint16_t>(respPtr[7]);
          uint16_t nscount = (static_cast<uint16_t>(respPtr[8]) << 8) | static_cast<uint16_t>(respPtr[9]);
          uint16_t arcount = (static_cast<uint16_t>(respPtr[10]) << 8) | static_cast<uint16_t>(respPtr[11]);
          std::cout << "[DEBUG] MT: Response header after commit - raw bytes: qd=[0x" << std::hex << static_cast<unsigned>(respPtr[4]) << " 0x" << static_cast<unsigned>(respPtr[5]) << "] an=[0x" << static_cast<unsigned>(respPtr[6]) << " 0x" << static_cast<unsigned>(respPtr[7]) << std::dec << "]" << std::endl;
          std::cout << "[DEBUG] MT: Response header after commit - values: id=" << j->qid << " rcode=" << rcode << " qdcount=" << qdcount << " ancount=" << ancount << " nscount=" << nscount << " arcount=" << arcount << " records=" << ret.size() << std::endl;
          if (ancount != static_cast<uint16_t>(ret.size())) {
            std::cerr << "[ERROR] MT: ancount mismatch! ancount=" << ancount << " but ret.size()=" << ret.size() << std::endl;
          }
          if (qdcount != 1) {
            std::cerr << "[ERROR] MT: qdcount should be 1 but got " << qdcount << std::endl;
          }
        }
        socklen_t tolen = sizeof(j->to);
        std::cout << "[DEBUG] MT: sending response of size " << resp.size() << std::endl;
        // Debug: Hex dump first 100 bytes of response packet
        std::cout << "[DEBUG] MT: Response packet hexdump (first " << (resp.size() < 100 ? resp.size() : 100) << " bytes):";
        for (size_t i = 0; i < resp.size() && i < 100; ++i) {
            std::cout << ' ' << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(resp[i]);
        }
        std::cout << std::dec << std::setfill(' ') << std::endl;
        ssize_t sent = sendto(j->sock, reinterpret_cast<const char*>(resp.data()), static_cast<int>(resp.size()), 0,
                               reinterpret_cast<sockaddr*>(&j->to), tolen);
#ifdef _WIN32
        if (sent < 0) {
            int werr = WSAGetLastError();
            std::cerr << "MT: sendto() failed (WSA " << werr << ") for " << j->fqdn << ", payload=" << resp.size() << std::endl;
        }
#else
        if (sent < 0) {
            std::cerr << "MT: sendto() failed (errno=" << errno << ": " << strerror(errno) << ") for " << j->fqdn << ", payload=" << resp.size() << std::endl;
        }
#endif
        std::cout << "MT: sent wire-parse " << (added ? "ANSWER" : "SERVFAIL") << " (" << sent << " bytes) for " << j->fqdn << " rcode=" << rcode << " records=" << ret.size() << std::endl;
        std::cout.flush(); // Ensure output is flushed
              std::cout << "[DEBUG] MT: After sendto() for " << j->fqdn << ", resolver will be destroyed when function returns" << std::endl;
              std::cout.flush();
          } catch (const std::exception& e) {
              std::cerr << "MT: exception during resolve/send: " << e.what() << std::endl;
              std::cerr.flush();
          }
          // CRITICAL: Match upstream pattern - resolver is destroyed here when function returns
          // Upstream: pdns_recursor.cc:2017 - function just returns, resolver destructor runs automatically
          // No explicit scope block - let C++ handle destruction when function returns
          std::string fqdn_copy = j->fqdn; // Capture fqdn before resolver destruction
          std::cout << "[DEBUG] MT: About to return from task function for " << fqdn_copy << ", resolver will be destroyed" << std::endl;
          std::cout.flush();
          // Resolver goes out of scope here when function returns - destructor will be called
          // If destructor blocks, the function will never return and MTasker won't process new queries
      }
      // Task function return will automatically destroy resolver and return control to MTasker scheduler
      // No need to call yield() - that would re-queue the task, which we don't want

// Simple DNS resolver test
int main() {
    try {
        std::cout << "PowerDNS Recursor Windows POC - Starting DNS Server..." << std::endl;
        
        // CRITICAL: Register all DNS record types (NS, A, AAAA, etc.) in the typemap
        // This must be called before any DNS parsing happens
        reportAllTypes();
        std::cout << "DNS record types registered" << std::endl;
        
#ifdef _WIN32
        // Initialize Winsock
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "Failed to initialize Winsock" << std::endl;
            return 1;
        }
        std::cout << "Winsock initialized" << std::endl;
#endif
        
        // Create UDP socket
        g_udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (g_udp_socket < 0) {
            std::cerr << "Failed to create UDP socket: " << strerror(errno) << std::endl;
            return 1;
        }
        std::cout << "UDP socket created: " << g_udp_socket << std::endl;
        
        // Use non-blocking socket; libevent expects non-blocking sockets
        u_long mode = 1; // 1 = non-blocking
        ioctlsocket(g_udp_socket, FIONBIO, &mode);

#ifdef _WIN32
        // Allow quick rebinding after restart
        BOOL reuse = TRUE;
        if (setsockopt(g_udp_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) != 0) {
            std::cerr << "Warning: setsockopt(SO_REUSEADDR) failed, WSA " << WSAGetLastError() << std::endl;
        }
#else
        int reuse = 1;
        setsockopt(g_udp_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif
        
        // Bind to port 5533 (alternative DNS port to avoid permission issues)
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(5533);
        
        if (bind(g_udp_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
#ifdef _WIN32
            int error = WSAGetLastError();
            std::cerr << "Failed to bind to port 5533: WSA Error " << error << std::endl;
#else
            std::cerr << "Failed to bind to port 5533: " << strerror(errno) << std::endl;
#endif
            closesocket(g_udp_socket);
            return 1;
        }
        std::cout << "Bound to port 5533" << std::endl;
        
        // Initialize global caches (required by SyncRes)
        std::cout << "[DEBUG] About to initialize caches, g_recCache=" << (g_recCache ? "not null" : "null") << ", g_negCache=" << (g_negCache ? "not null" : "null") << std::endl;
        if (!g_recCache) {
            g_recCache = std::make_unique<MemRecursorCache>(1); // 1 shard for POC
            std::cout << "Initialized record cache" << std::endl;
        }
        if (!g_negCache) {
            g_negCache = std::make_unique<NegCache>(1); // 1 shard for POC
            std::cout << "Initialized negative cache" << std::endl;
        }
        
        // Prime root hints into cache for iterative resolution
        timeval now{};
        Utility::gettimeofday(&now, nullptr);
        primeRootHints(now.tv_sec);
        
        // Ensure no forwarders are set (empty domain map = no forwarding, pure iterative)
        // SyncRes will use root hints we just primed
        std::cout << "Root hints primed - iterative resolution enabled (no forwarders)" << std::endl;
        
        // Initialize MTasker infrastructure for UDP resolution
        // These must be initialized before SyncRes can use asendto/arecvfrom
        initializeMTaskerInfrastructure();
        
        if (!t_fdm) {
            std::cerr << "Failed to create FDMultiplexer" << std::endl;
            closesocket(g_udp_socket);
            return 1;
        }
        
        // Minimal SyncRes runtime defaults (upstream would set via config)
        SyncRes::s_doIPv4 = true;
        SyncRes::s_doIPv6 = false; // enable later when needed
        SyncRes::s_noEDNS = false;
        SyncRes::s_qnameminimization = true;

        std::cout << "Initialized MTasker infrastructure:" << std::endl;
        std::cout << "  - g_multiTasker: ready" << std::endl;
        std::cout << "  - t_fdm: " << t_fdm->getName() << std::endl;
        std::cout << "  - t_udpclientsocks: ready" << std::endl;
        
        // Register incoming socket with t_fdm (single multiplexer like upstream)
        // t_fdm will handle both incoming queries and outgoing responses
        boost::any param;
#ifdef _WIN32
        param = static_cast<evutil_socket_t>(g_udp_socket);
#endif
        t_fdm->addReadFD(g_udp_socket, handleDNSQuery, param);
        std::cout << "Registered incoming socket with t_fdm multiplexer" << std::endl;
        
        // No direct recv test to avoid consuming datagrams
        
        std::cout << "DNS server running on port 5533. Press Ctrl+C to stop." << std::endl;
        
        // Main event loop - replicates recLoop() pattern from upstream
        timeval g_now{};
        int loop_count = 0;
        while (true) {
            try {
                // Update current time at start of each iteration
                Utility::gettimeofday(&g_now, nullptr);
                
                // 1. Run MTasker tasks (processes async UDP responses)
                // CRITICAL: schedule() processes timeouts and wakes up waiting tasks
                // This is how upstream handles stuck queries - timeouts are checked here
                // WARNING: If a task blocks (e.g., in destructor), schedule() may block waiting for it
                int schedule_count = 0;
                while (g_multiTasker && g_multiTasker->schedule(g_now)) {
                    // MTasker letting the mthreads do their thing
                    // schedule() returns true if any task made progress
                    schedule_count++;
                    if (schedule_count > 100 && loop_count < 10) {
                        std::cout << "[DEBUG] Event loop: schedule() called " << schedule_count << " times (possible blocking task?)" << std::endl;
                    }
                    Utility::gettimeofday(&g_now, nullptr); // Update time between iterations
                }
                if (schedule_count > 0 && loop_count < 10) {
                    std::cout << "[DEBUG] Event loop: schedule() loop completed after " << schedule_count << " iterations" << std::endl;
                }
                
                // WINDOWS WORKAROUND: DISABLED FOR TESTING
                // WSAEventSelect is working correctly - all events are handled by LIBEVENT/WSAEventSelect
                // No MANUAL_SELECT logs appeared in testing, indicating workarounds are not needed
                // If issues arise, re-enable these workarounds
#ifdef _WIN32
                // DISABLED: Manual select() check for incoming queries (BEFORE multiplexer)
                // All incoming queries are handled by t_fdm->run() via WSAEventSelect
                /*
                if (g_udp_socket >= 0) {
                    fd_set readfds;
                    FD_ZERO(&readfds);
                    FD_SET(g_udp_socket, &readfds);
                    struct timeval tv = {0, 0};
                    int select_result = select(g_udp_socket + 1, &readfds, nullptr, nullptr, &tv);
                    if (select_result > 0 && FD_ISSET(g_udp_socket, &readfds)) {
                        std::cout << "[IO_TRACKING] MANUAL_SELECT (BEFORE multiplexer): Detected data on socket " << g_udp_socket << ", calling handleDNSQuery" << std::endl;
                        boost::any param = static_cast<evutil_socket_t>(g_udp_socket);
                        handleDNSQuery(g_udp_socket, param);
                    }
                }
                */
                
                // DISABLED: Manual select() check for outgoing UDP sockets (BEFORE multiplexer)
                // All outgoing responses are handled by t_fdm->run() via WSAEventSelect
                /*
                if (g_multiTasker && t_fdm) {
                    const auto& waiters = g_multiTasker->getWaiters();
                    // Only process if there are waiters (avoid unnecessary work)
                    if (!waiters.empty()) {
                        if (loop_count % 100 == 0 || loop_count < 10) {
                            std::cout << "[DEBUG] Windows workaround: checking " << waiters.size() << " waiting queries (loop=" << loop_count << ")" << std::endl;
                        }
                        
                        fd_set readfds;
                        FD_ZERO(&readfds);
                        int maxfd = -1;
                        std::map<int, std::shared_ptr<PacketID>> fdToPident;
                        
                        // Build fd_set from waiting UDP sockets
                        for (const auto& waiter : waiters) {
                            if (waiter.key->fd >= 0 && !waiter.key->closed) {
                                // Validate socket is still valid
                                int optval = 0;
                                socklen_t optlen = sizeof(optval);
                                int sock_check = getsockopt(waiter.key->fd, SOL_SOCKET, SO_TYPE, reinterpret_cast<char*>(&optval), &optlen);
                                if (sock_check == 0) {
                                    FD_SET(waiter.key->fd, &readfds);
                                    if (waiter.key->fd > maxfd) maxfd = waiter.key->fd;
                                    fdToPident[waiter.key->fd] = waiter.key;
                                }
                            }
                        }
                        
                        if (maxfd >= 0) {
                            // Non-blocking check for ready sockets
                            struct timeval tv = {0, 0};
                            int select_result = select(maxfd + 1, &readfds, nullptr, nullptr, &tv);
                            
                            if (select_result > 0) {
                                std::cout << "[DEBUG] Windows workaround: select() detected " << select_result << " ready socket(s)" << std::endl;
                                // Some sockets are ready - manually read and process responses
                                for (const auto& [fd, pident] : fdToPident) {
                                    if (FD_ISSET(fd, &readfds)) {
                                        std::cout << "[DEBUG] Windows workaround: reading response from fd=" << fd << " qid=" << pident->id << " domain=" << pident->domain << std::endl;
                                        try {
                                            PacketBuffer packet;
                                            packet.resize(65536);
                                            ComboAddress fromaddr;
                                            socklen_t addrlen = sizeof(fromaddr);
                                            ssize_t len = recvfrom(fd, reinterpret_cast<char*>(packet.data()), packet.size(), 0, reinterpret_cast<sockaddr*>(&fromaddr), &addrlen);
                                            if (len > 0) {
                                                std::cout << "[DEBUG] Windows workaround: SUCCESS - recvfrom got " << len << " bytes on fd=" << fd << " qid=" << pident->id << " domain=" << pident->domain << ", sending to MTasker" << std::endl;
                                                packet.resize(len);
                                                int send_result = g_multiTasker->sendEvent(pident, &packet);
                                                std::cout << "[DEBUG] Windows workaround: sendEvent() returned " << send_result << " for qid=" << pident->id << std::endl;
                                                if (send_result > 0) {
                                                    // Successfully woke up the waiting task - it will handle returning the socket
                                                    std::cout << "[DEBUG] Windows workaround: Successfully woke up waiting task for qid=" << pident->id << std::endl;
                                                } else {
                                                    std::cout << "[DEBUG] Windows workaround: WARNING - sendEvent() returned " << send_result << " (no waiter found?)" << std::endl;
                                                }
                                            } else {
                                                int werr = WSAGetLastError();
                                                if (werr == WSAEWOULDBLOCK || werr == WSAENOTSOCK) {
                                                    // Socket is not ready or invalid - this is OK, just means no data yet or socket was closed
                                                    if (loop_count % 100 == 0 || loop_count < 10) {
                                                        std::cout << "[DEBUG] Windows workaround: recvfrom would block or socket invalid on fd=" << fd << " qid=" << pident->id << " WSA error=" << werr << std::endl;
                                                    }
                                                } else {
                                                    std::cout << "[DEBUG] Windows workaround: recvfrom failed on fd=" << fd << " qid=" << pident->id << " WSA error=" << werr << std::endl;
                                                }
                                            }
                                        } catch (const std::exception& e) {
                                            std::cerr << "[DEBUG] Windows workaround: exception: " << e.what() << std::endl;
                                        }
                                    }
                                }
                            } else if (select_result == 0) {
                                // CRITICAL FIX: select() may return 0 even when data is available on connected UDP sockets
                                // Try to peek/read from all waiting sockets anyway (select() is unreliable on Windows)
                                if (loop_count % 100 == 0 || loop_count < 10) {
                                    std::cout << "[DEBUG] Windows workaround: select() returned 0, but trying MSG_PEEK on all waiting sockets (select() may be unreliable)" << std::endl;
                                }
                                // Try MSG_PEEK on all waiting sockets
                                for (const auto& [fd, pident] : fdToPident) {
                                    char peek_buf[12];
                                    ssize_t peek_result = recv(fd, peek_buf, sizeof(peek_buf), MSG_PEEK);
                                    if (peek_result > 0) {
                                        std::cout << "[DEBUG] Windows workaround: MSG_PEEK detected " << peek_result << " bytes on fd=" << fd << " qid=" << pident->id << " (select() missed it!)" << std::endl;
                                        // Read the full response
                                        try {
                                            PacketBuffer packet;
                                            packet.resize(65536);
                                            ComboAddress fromaddr;
                                            socklen_t addrlen = sizeof(fromaddr);
                                            ssize_t len = recvfrom(fd, reinterpret_cast<char*>(packet.data()), packet.size(), 0, reinterpret_cast<sockaddr*>(&fromaddr), &addrlen);
                                            if (len > 0) {
                                                std::cout << "[DEBUG] Windows workaround: SUCCESS - recvfrom got " << len << " bytes on fd=" << fd << " qid=" << pident->id << ", sending to MTasker" << std::endl;
                                                packet.resize(len);
                                                int send_result = g_multiTasker->sendEvent(pident, &packet);
                                                std::cout << "[DEBUG] Windows workaround: sendEvent() returned " << send_result << " for qid=" << pident->id << std::endl;
                                            }
                                        } catch (const std::exception& e) {
                                            std::cerr << "[DEBUG] Windows workaround: exception: " << e.what() << std::endl;
                                        }
                                    }
                                }
                            } else {
                                int werr = WSAGetLastError();
                                std::cout << "[DEBUG] Windows workaround: select() failed WSA error=" << werr << std::endl;
                            }
                        }
                    }
                }
                */
#endif
                
                // 2. Run FD multiplexer to handle I/O events
                // This processes both incoming queries and outgoing UDP responses
                auto timeoutUsec = g_multiTasker ? g_multiTasker->nextWaiterDelayUsec(500000) : 500000;
                int timeoutMsec = static_cast<int>(timeoutUsec / 1000);
                
                // Run single multiplexer (t_fdm) - handles both incoming queries and outgoing responses
                // This matches upstream pattern: single t_fdm->run() processes all I/O
                if (loop_count < 20) {  // Increased from 10 to 20 to see more iterations
                    std::cout << "Event loop iteration " << loop_count << ", calling t_fdm->run() with timeout=" << timeoutMsec << "ms" << std::endl;
                }
                
                int events = 0;
                if (t_fdm) {
                    events = t_fdm->run(&g_now, timeoutMsec);
                }
                
                // Debug: Log every 1000 iterations (roughly every ~500ms)
                if (++loop_count % 1000 == 0) {
                    std::cout << "Event loop iteration " << loop_count << ", events: " << events << std::endl;
                }
                
                // Debug: Log first few to see return values
                if (loop_count < 20) {  // Increased from 10 to 20
                    std::cout << "  -> t_fdm->run() returned: " << events << " (iteration " << loop_count << ")" << std::endl;
                }
                
                // WINDOWS WORKAROUND: DISABLED FOR TESTING
                // Manual select() check after t_fdm->run() - not needed, WSAEventSelect handles all events
#ifdef _WIN32
                // DISABLED: Manual select() check for incoming queries (AFTER multiplexer)
                /*
                if (g_udp_socket >= 0) {
                    fd_set readfds;
                    FD_ZERO(&readfds);
                    FD_SET(g_udp_socket, &readfds);
                    struct timeval tv = {0, 0};
                    int select_result = select(g_udp_socket + 1, &readfds, nullptr, nullptr, &tv);
                    if (select_result > 0 && FD_ISSET(g_udp_socket, &readfds)) {
                        std::cout << "[IO_TRACKING] MANUAL_SELECT (AFTER multiplexer): Detected data on socket " << g_udp_socket << ", calling handleDNSQuery" << std::endl;
                        boost::any param = static_cast<evutil_socket_t>(g_udp_socket);
                        handleDNSQuery(g_udp_socket, param);
                    }
                }
                */
#endif
                
                // Note: t_fdm->run() already called above - single multiplexer handles all I/O
                
                // WINDOWS WORKAROUND: DISABLED - Testing if this is causing blocking
                // Responses might have arrived during t_fdm->run()
                // but WSAEventSelect callback didn't fire
#ifdef _WIN32
                // DISABLED FOR TESTING
                if (false && g_multiTasker && t_fdm) {
                    const auto& waiters_after_io = g_multiTasker->getWaiters();
                    if (!waiters_after_io.empty()) {
                        // Check again for responses that arrived during I/O processing
                        fd_set readfds;
                        FD_ZERO(&readfds);
                        int maxfd = -1;
                        std::map<int, std::shared_ptr<PacketID>> fdToPident;
                        
                        for (const auto& waiter : waiters_after_io) {
                            if (waiter.key->fd >= 0 && !waiter.key->closed) {
                                FD_SET(waiter.key->fd, &readfds);
                                if (waiter.key->fd > maxfd) maxfd = waiter.key->fd;
                                fdToPident[waiter.key->fd] = waiter.key;
                            }
                        }
                        
                        if (maxfd >= 0) {
                            struct timeval tv = {0, 0};
                            int select_result = select(maxfd + 1, &readfds, nullptr, nullptr, &tv);
                            
                            if (select_result > 0) {
                                std::cout << "[DEBUG] Windows workaround (after I/O): select() detected " << select_result << " ready socket(s)" << std::endl;
                                for (const auto& [fd, pident] : fdToPident) {
                                    if (FD_ISSET(fd, &readfds)) {
                                        std::cout << "[DEBUG] Windows workaround (after I/O): reading response from fd=" << fd << " qid=" << pident->id << std::endl;
                                        try {
                                            PacketBuffer packet;
                                            packet.resize(65536);
                                            ComboAddress fromaddr;
                                            socklen_t addrlen = sizeof(fromaddr);
                                            ssize_t len = recvfrom(fd, reinterpret_cast<char*>(packet.data()), packet.size(), 0, reinterpret_cast<sockaddr*>(&fromaddr), &addrlen);
                                            if (len > 0) {
                                                std::cout << "[DEBUG] Windows workaround (after I/O): SUCCESS - recvfrom got " << len << " bytes on fd=" << fd << " qid=" << pident->id << ", sending to MTasker" << std::endl;
                                                packet.resize(len);
                                                int send_result = g_multiTasker->sendEvent(pident, &packet);
                                                std::cout << "[DEBUG] Windows workaround (after I/O): sendEvent() returned " << send_result << " for qid=" << pident->id << std::endl;
                                            }
                                        } catch (const std::exception& e) {
                                            std::cerr << "[DEBUG] Windows workaround (after I/O): exception: " << e.what() << std::endl;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
#endif
                
                // WINDOWS WORKAROUND: DISABLED - Testing if this is causing blocking
                // Upstream relies on epoll (level-triggered) which doesn't have this issue
                // On Windows with WSAEventSelect, we need to manually check for responses
                // that libevent's win32 backend might miss
#if 0
                // DISABLED FOR TESTING - This workaround was added because we thought WSAEventSelect
                // was edge-triggered, but the real issue was probably closing sockets twice.
                // Disabling to test if it's causing the blocking issue.
                // OLD CODE (commented out for potential revert):
                //
                if (g_multiTasker && t_fdm) {
                    // CRITICAL: Check all waiting UDP sockets for data using select()
                    // This is our workaround for WSAEventSelect edge-triggered behavior
                    // Upstream doesn't need this because epoll is level-triggered
                    const auto& waiters = g_multiTasker->getWaiters();
                    // Debug: Always check waiters count (more aggressive)
                    if (true) {  // Always log when checking
                        std::cout << "[DEBUG] Windows workaround: waiters.size()=" << waiters.size() << " (loop=" << loop_count << ")";
                        if (!waiters.empty()) {
                            std::cout << " waiters:";
                            for (const auto& waiter : waiters) {
                                if (waiter.key->fd >= 0) {
                                    std::cout << " fd=" << waiter.key->fd << " qid=" << waiter.key->id << " domain=" << waiter.key->domain;
                                }
                            }
                        }
                        std::cout << std::endl;
                    }
                    // Always process when waiters exist
                    if (!waiters.empty()) {
                        // Log every iteration when queries are waiting (very aggressive for debugging)
                        std::cout << "[DEBUG] Windows workaround: checking " << waiters.size() << " waiting queries (loop=" << loop_count << ")" << std::endl;
                        // List waiting queries for debugging
                        for (const auto& waiter : waiters) {
                            if (waiter.key->fd >= 0) {
                                std::cout << "  - Waiting: fd=" << waiter.key->fd << " qid=" << waiter.key->id << " domain=" << waiter.key->domain << " remote=" << waiter.key->remote.toStringWithPort() << std::endl;
                            }
                        }
                        fd_set readfds;
                        FD_ZERO(&readfds);
                        int maxfd = -1;
                        std::map<int, std::shared_ptr<PacketID>> fdToPident;
                        
                        // Build fd_set from waiting UDP sockets
                        // Validate each fd before adding to avoid WSAENOTSOCK errors
                        for (const auto& waiter : waiters) {
                            if (waiter.key->fd >= 0 && !waiter.key->closed) {
                                // Validate socket is still valid before adding to fd_set
                                // Use getsockopt to check if socket is valid
                                int optval = 0;
                                socklen_t optlen = sizeof(optval);
                                int sock_check = getsockopt(waiter.key->fd, SOL_SOCKET, SO_TYPE, reinterpret_cast<char*>(&optval), &optlen);
                                if (sock_check == 0) {
                                    // Socket is valid, add to fd_set
                                    FD_SET(waiter.key->fd, &readfds);
                                    if (waiter.key->fd > maxfd) maxfd = waiter.key->fd;
                                    fdToPident[waiter.key->fd] = waiter.key;
                                } else {
                                    // Socket is invalid (likely closed), skip it
                                    int werr = WSAGetLastError();
                                    std::cout << "[DEBUG] Windows workaround: skipping invalid fd=" << waiter.key->fd << " (WSA error=" << werr << ")" << std::endl;
                                }
                            }
                        }
                        
                        if (maxfd >= 0) {
                            // Non-blocking check for ready sockets
                            struct timeval tv = {0, 0};
                            int select_result = select(maxfd + 1, &readfds, nullptr, nullptr, &tv);
                            // Always log select results when there are waiters (very aggressive for debugging)
                            if (true) {  // Always log when waiters exist
                                std::cout << "[DEBUG] Windows workaround: select() returned " << select_result << " for " << fdToPident.size() << " waiting sockets";
                                if (select_result < 0) {
                                    int werr = WSAGetLastError();
                                    std::cout << " WSA error=" << werr;
                                } else if (select_result > 0) {
                                    std::cout << " ready fds:";
                                    for (const auto& [fd, pident] : fdToPident) {
                                        if (FD_ISSET(fd, &readfds)) {
                                            std::cout << " " << fd << "(qid=" << pident->id << ",domain=" << pident->domain << ")";
                                        }
                                    }
                                }
                                std::cout << " maxfd=" << maxfd << " all fds:";
                                for (const auto& [fd, pident] : fdToPident) {
                                    std::cout << " " << fd << "(qid=" << pident->id << ")";
                                }
                                std::cout << std::endl;
                            }
                            if (select_result > 0) {
                                // Some sockets are ready - manually trigger callbacks
                                for (const auto& [fd, pident] : fdToPident) {
                                    if (FD_ISSET(fd, &readfds)) {
                                        std::cout << "[DEBUG] Windows workaround: select() detected data on fd=" << fd << ", manually triggering callback" << std::endl;
                                        // Manually trigger the callback registered for this fd
                                        // Use t_fdm's callback mechanism to call handleUDPServerResponse
                                        // The callback is registered when we call addReadFD in asendto
                                        try {
                                            boost::any param = pident;
                                            // Get the callback from t_fdm - we need to access it
                                            // Since we can't easily access private members, let's use
                                            // a helper function or directly call via the callback
                                            // Actually, we can use the callback that was registered
                                            // But we need access to the callback... let's try a different approach:
                                            // Trigger the libevent callback manually by simulating the event
                                            // Or better: use t_fdm's internal mechanism
                                            // For now, let's just call the registered callback directly
                                            // We'll need to make handleUDPServerResponse accessible
                                            // Actually, the simplest: call it through the callback we registered
                                            // But since it's static, we need to make it accessible
                                            // Let's use a function pointer or make it non-static
                                            // Actually, let's just use the FDMultiplexer's mechanism
                                            // We can't easily do this, so let's make handleUDPServerResponse non-static
                                            // Or declare it in a header
                                            // For now, let's use a workaround: call recvfrom directly and process
                                            PacketBuffer packet;
                                            packet.resize(65536);
                                            ComboAddress fromaddr;
                                            socklen_t addrlen = sizeof(fromaddr);
                                            ssize_t len = recvfrom(fd, reinterpret_cast<char*>(packet.data()), packet.size(), 0, reinterpret_cast<sockaddr*>(&fromaddr), &addrlen);
                                            if (len > 0) {
                                                std::cout << "[DEBUG] Windows workaround: recvfrom got " << len << " bytes, sending to MTasker" << std::endl;
                                                packet.resize(len);
                                                g_multiTasker->sendEvent(pident, &packet);
                                            }
                                        } catch (const std::exception& e) {
                                            std::cerr << "[DEBUG] Windows workaround: exception: " << e.what() << std::endl;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
#endif
                
                if (events < 0) {
                    std::cerr << "Multiplexer error: " << events << std::endl;
                    break;
                }
                // Continue loop
            } catch (const std::exception& e) {
                std::cerr << "Event loop error: " << e.what() << std::endl;
                break;
            }
        }
        
        // Cleanup
        if (t_fdm && g_udp_socket >= 0) {
            t_fdm->removeReadFD(g_udp_socket);
        }
        closesocket(g_udp_socket);
        
#ifdef _WIN32
        WSACleanup();
#endif
        
        std::cout << "DNS server stopped" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
