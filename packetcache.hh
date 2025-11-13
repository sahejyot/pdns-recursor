/*
 * This file is part of PowerDNS or dnsdist.
 * Copyright -- PowerDNS.COM B.V. and its contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * In addition, for the avoidance of any doubt, permission is granted to
 * link this program with OpenSSL and to (re)distribute the binaries
 * produced as the result of such linking.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#pragma once
#include "ednsoptions.hh"
#include "misc.hh"
#include "iputils.hh"

class PacketCache : public boost::noncopyable
{
public:

  /* hash the packet from the provided position, which should point right after tje qname. This skips:
     - the qname itself
     - the qtype and qclass
     - the EDNS OPT record if present
     This is done so that packets with different EDNS options can share the same cache entry
  */
  static uint32_t hashAfterQname(const std::string& packet, uint16_t& pos, bool* foundEDNS = nullptr);

  /* hash the packet from the provided position, which should point right after the qname. This skips:
     - the qname itself
     - the qtype and qclass
     - the EDNS OPT record if present
     - the EDNS Client Subnet option if present
     This is done so that packets with different ECS options can share the same cache entry
  */
  static uint32_t hashAfterQnameECS(const std::string& packet, uint16_t& pos, bool* foundEDNS = nullptr, bool* foundECS = nullptr);

  void setMaxSize(size_t size)
  {
    d_maxSize = size;
  }

  [[nodiscard]] size_t getMaxSize() const
  {
    return d_maxSize;
  }

protected:
  size_t d_maxSize{0};
};

