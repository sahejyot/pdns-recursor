/*
 * Windows Compatibility Layer for PowerDNS Recursor
 * 
 * This header provides Windows alternatives for POSIX/Unix functions
 * and headers that don't exist on Windows.
 */
#pragma once

#ifdef _WIN32

// Windows headers
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <io.h>  // For _close()

// MSVC doesn't have strings.h, but string.h provides similar functions
#include <string.h>

// String comparison functions (strings.h equivalents)
#ifndef strcasecmp
#define strcasecmp _stricmp
#endif

#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif

// POSIX compatibility
#ifndef SHUT_RD
#define SHUT_RD SD_RECEIVE
#endif

#ifndef SHUT_WR
#define SHUT_WR SD_SEND
#endif

#ifndef SHUT_RDWR
#define SHUT_RDWR SD_BOTH
#endif

// Syslog priorities (not used on Windows, but defined for compatibility)
#define LOG_EMERG   0
#define LOG_ALERT   1
#define LOG_CRIT    2
#define LOG_ERR     3
#define LOG_WARNING 4
#define LOG_NOTICE  5
#define LOG_INFO    6
#define LOG_DEBUG   7

// GCC attributes that MSVC doesn't support
#ifndef __attribute__
#define __attribute__(x)
#endif

// MSVC doesn't have ssize_t
#ifndef ssize_t
#ifdef _WIN64
typedef __int64 ssize_t;
#else
typedef int ssize_t;
#endif
#endif

// Unix close() compatibility - MinGW already provides close() in io.h
// No need to redefine it

// Unix types not available on Windows
#ifndef uid_t
typedef unsigned int uid_t;
#endif
#ifndef gid_t
typedef unsigned int gid_t;
#endif

// Control message header
struct cmsghdr {
  size_t cmsg_len;
  int cmsg_level;
  int cmsg_type;
  /* unsigned char cmsg_data[]; */
};

// I/O vector structure (for scatter/gather I/O)
struct iovec {
  void* iov_base;
  size_t iov_len;
};

// Message header structure (for sendmsg/recvmsg)
struct msghdr {
  void* msg_name;          // Optional address
  int msg_namelen;         // Size of address
  struct iovec* msg_iov;   // Scatter/gather array
  size_t msg_iovlen;       // Number of elements in msg_iov
  void* msg_control;       // Ancillary data
  size_t msg_controllen;   // Ancillary data buffer length
  int msg_flags;           // Flags on received message
};

// Unix socket address
struct sockaddr_un {
  unsigned short sun_family;      // AF_UNIX
  char sun_path[108];              // Pathname
};

#endif // _WIN32

