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
// Utility class specification.
#pragma once

#ifdef NEED_POSIX_TYPEDEF
typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
#endif

#ifdef _WIN32
# include <winsock2.h>
# include <ws2tcpip.h>
# include <windows.h>
# include <io.h>
#else
# include <arpa/inet.h>
# include <netinet/in.h>
# include <sys/socket.h>
# include <sys/time.h>
# include <sys/uio.h>
# include <csignal>
# include <pthread.h>
# include <semaphore.h>
# include <cerrno>
# include <unistd.h>
#endif
#include <string>

#include "namespaces.hh"

#ifdef _WIN32

// Minimal Windows shims: only to satisfy compilation, not used in our POC
class Semaphore
{
public:
  Semaphore(const Semaphore&) = delete;
  void operator=(const Semaphore&) = delete;
  explicit Semaphore(unsigned int /*value*/ = 0) {}
  ~Semaphore() = default;
  int post() { return 0; }
  int wait() { return 0; }
  int tryWait() { return 0; }
  int getValue(int* /*sval*/) { return 0; }
};

class Utility
{
public:
  typedef int sock_t;
  typedef ::socklen_t socklen_t;
  static int timed_connect(sock_t, const sockaddr*, socklen_t, int, int) { return -1; }
  static int gettimeofday(struct timeval* tv, void* /*tz*/ = nullptr) {
    if (!tv) return -1;
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULONGLONG ticks = (static_cast<ULONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    const ULONGLONG EPOCH_DIFF = 116444736000000000ULL; // 1601 to 1970 in 100ns
    if (ticks < EPOCH_DIFF) return -1;
    ticks -= EPOCH_DIFF;
    tv->tv_sec = static_cast<long>(ticks / 10000000ULL);
    tv->tv_usec = static_cast<long>((ticks % 10000000ULL) / 10ULL);
    return 0;
  }
  static int inet_aton(const char* cp, struct in_addr* inp) { return ::inet_pton(AF_INET, cp, inp); }
  static int inet_pton(int af, const char* src, void* dst) { return ::inet_pton(af, src, dst); }
  static const char* inet_ntop(int af, const char* src, char* dst, size_t size) { return ::inet_ntop(af, src, dst, (socklen_t)size); }
  static void setBindAny(int, Utility::sock_t) {}
  static unsigned int sleep(unsigned int seconds) { ::Sleep(seconds * 1000); return 0; }
  static void usleep(unsigned long usec) { ::Sleep((DWORD)(usec / 1000)); }
  static time_t timegm(struct tm* tm) { return _mkgmtime(tm); }
};

#else

//! A semaphore class.
class Semaphore
{
private:
  typedef int sem_value_t;

#if defined(_AIX) || defined(__APPLE__)
  uint32_t       m_magic;
  pthread_mutex_t m_lock;
  pthread_cond_t  m_gtzero;
  sem_value_t     m_count;
  uint32_t       m_nwaiters;
#else
  std::unique_ptr<sem_t> m_pSemaphore;
#endif

protected:
public:
  Semaphore(const Semaphore&) = delete;
  void operator=(const Semaphore&) = delete;
  //! Default constructor.
  Semaphore( unsigned int value = 0 );

  //! Destructor.
  ~Semaphore();

  //! Posts to a semaphore.
  int post();

  //! Waits for a semaphore.
  int wait();

  //! Tries to wait for a semaphore.
  int tryWait();

  //! Retrieves the semaphore value.
  int getValue( Semaphore::sem_value_t *sval );
};

//! This is a utility class used for platform independent abstraction.
class Utility
{
public:
  typedef ::iovec iovec;
  typedef ::pid_t pid_t;
  typedef int sock_t;
  typedef ::socklen_t socklen_t;

  static int timed_connect(sock_t sock,
    const sockaddr *addr,
    socklen_t sockaddr_size,
    int timeout_sec,
    int timeout_usec);
  static pid_t getpid();
  static int gettimeofday( struct timeval *tv, void *tz = NULL );
  static int inet_aton( const char *cp, struct in_addr *inp );
  static int inet_pton( int af, const char *src, void *dst );
  static const char *inet_ntop( int af, const char *src, char *dst, size_t size );
  static int writev( Utility::sock_t socket, const iovec *vector, size_t count );
  static void dropGroupPrivs( uid_t uid, gid_t gid );
  static void dropUserPrivs( uid_t uid );
  static void setBindAny ( int af, Utility::sock_t socket );
  static unsigned int sleep( unsigned int seconds );
  static void usleep( unsigned long usec );
  static time_t timegm(struct tm *tm);
};

#endif
