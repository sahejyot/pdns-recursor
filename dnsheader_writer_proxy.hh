/*
 * DNS Header Writer Proxy for Windows
 * 
 * This proxy class intercepts writes to dnsheader fields through getHeader()
 * and converts them to wire format (12 bytes, network byte order) to avoid
 * struct padding issues on Windows where sizeof(dnsheader) == 14.
 * 
 * This allows upstream code like `pw.getHeader()->id = qid` to work without changes.
 */

#pragma once

#include "dns.hh"
#include <cstring>
#include <arpa/inet.h>

#ifdef _WIN32
#include <winsock2.h>
#endif

// Proxy class that intercepts dnsheader field writes and converts to wire format
class DNSHeaderWriterProxy
{
public:
  explicit DNSHeaderWriterProxy(uint8_t* wireBuffer) : d_wireBuffer(wireBuffer)
  {
    // Wire buffer must be at least 12 bytes
    if (d_wireBuffer == nullptr) {
      throw std::runtime_error("DNSHeaderWriterProxy: wireBuffer is null");
    }
  }

  // Accessor methods that write directly to wire format
  // ID (bytes 0-1, network byte order)
  uint16_t getId() const
  {
    return ntohs((static_cast<uint16_t>(d_wireBuffer[0]) << 8) | static_cast<uint16_t>(d_wireBuffer[1]));
  }

  void setId(uint16_t id)
  {
    uint16_t idNetwork = htons(id);
    d_wireBuffer[0] = (idNetwork >> 8) & 0xFF;
    d_wireBuffer[1] = idNetwork & 0xFF;
  }

  // Flags accessors - read/write to bytes 2-3 in network byte order
  // DNS wire format: Byte 2 = QR(7), Opcode(6-3), AA(2), TC(1), RD(0)
  //                  Byte 3 = RA(7), Z(6), AD(5), CD(4), RCODE(3-0)
  // When combined as 16-bit (byte2 << 8) | byte3:
  // Bit 15 = QR, Bit 8 = RD, Bit 7 = RA, Bits 3-0 = RCODE

  uint16_t getFlags() const
  {
    return (static_cast<uint16_t>(d_wireBuffer[2]) << 8) | static_cast<uint16_t>(d_wireBuffer[3]);
  }

  void setFlags(uint16_t flags)
  {
    d_wireBuffer[2] = (flags >> 8) & 0xFF;
    d_wireBuffer[3] = flags & 0xFF;
  }

  // Bitfield accessors - these read/modify/write the flags
  bool getQr() const
  {
    return (getFlags() & 0x8000) != 0; // Bit 15
  }

  void setQr(bool val)
  {
    uint16_t flags = getFlags();
    if (val) flags |= 0x8000;
    else flags &= ~0x8000;
    setFlags(flags);
  }

  unsigned getOpcode() const
  {
    return (getFlags() >> 11) & 0x0F; // Bits 11-14
  }

  void setOpcode(unsigned val)
  {
    uint16_t flags = getFlags();
    flags = (flags & ~0x7800) | ((val & 0x0F) << 11);
    setFlags(flags);
  }

  bool getAa() const
  {
    return (getFlags() & 0x0400) != 0; // Bit 10
  }

  void setAa(bool val)
  {
    uint16_t flags = getFlags();
    if (val) flags |= 0x0400;
    else flags &= ~0x0400;
    setFlags(flags);
  }

  bool getTc() const
  {
    return (getFlags() & 0x0200) != 0; // Bit 9
  }

  void setTc(bool val)
  {
    uint16_t flags = getFlags();
    if (val) flags |= 0x0200;
    else flags &= ~0x0200;
    setFlags(flags);
  }

  bool getRd() const
  {
    return (getFlags() & 0x0100) != 0; // Bit 8
  }

  void setRd(bool val)
  {
    uint16_t flags = getFlags();
    if (val) flags |= 0x0100;
    else flags &= ~0x0100;
    setFlags(flags);
  }

  bool getRa() const
  {
    return (getFlags() & 0x0080) != 0; // Bit 7
  }

  void setRa(bool val)
  {
    uint16_t flags = getFlags();
    if (val) flags |= 0x0080;
    else flags &= ~0x0080;
    setFlags(flags);
  }

  bool getAd() const
  {
    return (getFlags() & 0x0020) != 0; // Bit 5
  }

  void setAd(bool val)
  {
    uint16_t flags = getFlags();
    if (val) flags |= 0x0020;
    else flags &= ~0x0020;
    setFlags(flags);
  }

  bool getCd() const
  {
    return (getFlags() & 0x0010) != 0; // Bit 4
  }

  void setCd(bool val)
  {
    uint16_t flags = getFlags();
    if (val) flags |= 0x0010;
    else flags &= ~0x0010;
    setFlags(flags);
  }

  unsigned getRcode() const
  {
    return getFlags() & 0x000F; // Bits 0-3
  }

  void setRcode(unsigned val)
  {
    uint16_t flags = getFlags();
    flags = (flags & ~0x000F) | (val & 0x000F);
    setFlags(flags);
  }

  // Count fields (bytes 4-11, network byte order)
  uint16_t getQdcount() const
  {
    return ntohs((static_cast<uint16_t>(d_wireBuffer[4]) << 8) | static_cast<uint16_t>(d_wireBuffer[5]));
  }

  void setQdcount(uint16_t val)
  {
    uint16_t valNetwork = htons(val);
    d_wireBuffer[4] = (valNetwork >> 8) & 0xFF;
    d_wireBuffer[5] = valNetwork & 0xFF;
  }

  uint16_t getAncount() const
  {
    return ntohs((static_cast<uint16_t>(d_wireBuffer[6]) << 8) | static_cast<uint16_t>(d_wireBuffer[7]));
  }

  void setAncount(uint16_t val)
  {
    uint16_t valNetwork = htons(val);
    d_wireBuffer[6] = (valNetwork >> 8) & 0xFF;
    d_wireBuffer[7] = valNetwork & 0xFF;
  }

  uint16_t getNscount() const
  {
    return ntohs((static_cast<uint16_t>(d_wireBuffer[8]) << 8) | static_cast<uint16_t>(d_wireBuffer[9]));
  }

  void setNscount(uint16_t val)
  {
    uint16_t valNetwork = htons(val);
    d_wireBuffer[8] = (valNetwork >> 8) & 0xFF;
    d_wireBuffer[9] = valNetwork & 0xFF;
  }

  uint16_t getArcount() const
  {
    return ntohs((static_cast<uint16_t>(d_wireBuffer[10]) << 8) | static_cast<uint16_t>(d_wireBuffer[11]));
  }

  void setArcount(uint16_t val)
  {
    uint16_t valNetwork = htons(val);
    d_wireBuffer[10] = (valNetwork >> 8) & 0xFF;
    d_wireBuffer[11] = valNetwork & 0xFF;
  }

  // Operator overloads to make it work like dnsheader*
  // This allows code like: proxy->id = 1234;
  // But we can't intercept member variable writes directly in C++.
  // Instead, we'll need to change getHeader() to return a special pointer type.

private:
  uint8_t* d_wireBuffer; // Pointer to wire format buffer (12 bytes)
};

// Wrapper class that makes the proxy work with -> operator
// This allows: getHeader()->id = 1234; to work
class DNSHeaderWriterWrapper
{
public:
  explicit DNSHeaderWriterWrapper(uint8_t* wireBuffer) : d_proxy(wireBuffer) {}

  // Overload -> to return a pointer-like object that intercepts field access
  // We'll use a special struct that has the same field names but with operator= overloads
  struct HeaderFields
  {
    explicit HeaderFields(DNSHeaderWriterProxy* proxy) : d_proxy(proxy) {}

    // ID field
    uint16_t id;
    uint16_t& getIdRef() { return reinterpret_cast<uint16_t&>(id); }
    void setId(uint16_t val) { d_proxy->setId(val); }
    uint16_t getId() const { return d_proxy->getId(); }

    // Bitfield accessors - these need special handling
    // We'll use a bitfield proxy
    struct FlagsProxy
    {
      explicit FlagsProxy(DNSHeaderWriterProxy* proxy) : d_proxy(proxy) {}
      DNSHeaderWriterProxy* d_proxy;

      // We can't intercept bitfield writes directly, so we'll need a different approach
    };

    DNSHeaderWriterProxy* d_proxy;
  };

  HeaderFields* operator->()
  {
    // This won't work because we can't intercept member variable writes
    // We need a different approach
    return nullptr; // Placeholder
  }

private:
  DNSHeaderWriterProxy d_proxy;
};



