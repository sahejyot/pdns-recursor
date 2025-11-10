#pragma once
#include <cstdio>

#ifndef LOG_ALERT
#define LOG_ALERT 1
#endif
#ifndef LOG_CRIT
#define LOG_CRIT 2
#endif
#ifndef LOG_ERR
#define LOG_ERR 3
#endif
#ifndef LOG_WARNING
#define LOG_WARNING 4
#endif
#ifndef LOG_NOTICE
#define LOG_NOTICE 5
#endif
#ifndef LOG_INFO
#define LOG_INFO 6
#endif
#ifndef LOG_DEBUG
#define LOG_DEBUG 7
#endif
#ifndef LOG_DAEMON
#define LOG_DAEMON 3
#endif
#ifndef LOG_PID
#define LOG_PID 0
#endif
#ifndef LOG_NDELAY
#define LOG_NDELAY 0
#endif

inline void openlog(const char*, int, int) {}
inline void closelog() {}
inline void syslog(int /*prio*/, const char* fmt, const char* msg)
{
  std::fprintf(stderr, fmt, msg);
  std::fprintf(stderr, "\n");
}












