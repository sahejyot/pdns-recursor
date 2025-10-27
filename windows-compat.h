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

#endif // _WIN32

