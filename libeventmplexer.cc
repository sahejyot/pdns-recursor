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
#include "mplexer.hh"
#ifdef _WIN32
#include "windows-compat.h"  // Includes Winsock2 and defines close()
#else
#include <unistd.h>
#include <sys/socket.h>
#endif
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/util.h>  // For evutil_make_socket_nonblocking
#include <event2/event_struct.h>  // For event_config
#include <iostream>
// Don't include misc.hh - it pulls in DNS dependencies we don't need
// Just use string directly for error messages
#include <string>
#include <map>
#include <functional>

// Structure to hold event and fd information for callbacks
struct FDCallbackInfo;

class LibeventFDMultiplexer : public FDMultiplexer
{
public:
  LibeventFDMultiplexer(unsigned int maxEventsHint);
  ~LibeventFDMultiplexer() override
  {
    // Clean up all events
    for (auto& pair : d_readEvents) {
      if (pair.second) {
        event_del(pair.second);
        event_free(pair.second);
      }
    }
    d_readEvents.clear();
    
    for (auto& pair : d_writeEvents) {
      if (pair.second) {
        event_del(pair.second);
        event_free(pair.second);
      }
    }
    d_writeEvents.clear();
    
    if (d_eventBase != nullptr) {
      event_base_free(d_eventBase);
    }
  }

  int run(struct timeval* tv, int timeout = 500) override;
  void getAvailableFDs(std::vector<int>& fds, int timeout) override;

  void addFD(int fd, FDMultiplexer::EventKind kind) override;
  void removeFD(int fd, FDMultiplexer::EventKind kind) override;
  void alterFD(int fd, FDMultiplexer::EventKind from, FDMultiplexer::EventKind to) override;

  std::string getName() const override
  {
    return "libevent";
  }

private:
  struct event_base* d_eventBase;
  std::map<int, struct event*> d_readEvents;
  std::map<int, struct event*> d_writeEvents;
  unsigned int d_maxEvents;
  
  static void eventCallback(evutil_socket_t fd, short what, void* arg);
};

struct FDCallbackInfo
{
  LibeventFDMultiplexer* mplex;
  int fd;
  bool isRead;
};

static FDMultiplexer* makeLibevent(unsigned int maxEventsHint)
{
  return new LibeventFDMultiplexer(maxEventsHint);
}

static struct LibeventRegisterOurselves
{
  LibeventRegisterOurselves()
  {
    FDMultiplexer::getMultiplexerMap().emplace(1, &makeLibevent); // Priority 1 for libevent
  }
} doItLibevent;

// Implement the static getMultiplexerSilent function
FDMultiplexer* FDMultiplexer::getMultiplexerSilent(unsigned int maxEventsHint)
{
  // Find the first available multiplexer
  auto& map = getMultiplexerMap();
  if (!map.empty()) {
    return map.begin()->second(maxEventsHint);
  }
  return nullptr;
}

LibeventFDMultiplexer::LibeventFDMultiplexer(unsigned int maxEventsHint) :
  d_eventBase(nullptr), d_maxEvents(maxEventsHint)
{
  // On Windows, libevent uses win32 backend (WSAEventSelect)
  // Microsoft docs say WSAEventSelect is level-triggered for FD_READ
  // However, we need to ensure events are properly re-enabled after recv()
  struct event_config* cfg = event_config_new();
  if (cfg) {
#ifdef _WIN32
    // Note: Microsoft docs say WSAEventSelect is level-triggered for FD_READ
    // We use EV_PERSIST to keep events enabled, but if there are still issues,
    // we can try forcing select backend as a fallback
    
    // List available methods and avoid problematic ones
    const char** methods = event_get_supported_methods();
    std::cout << "[DEBUG] libevent: Available methods before config:";
    for (int i = 0; methods[i] != nullptr; i++) {
      std::cout << " " << methods[i];
    }
    std::cout << std::endl;
    
      // Use win32 backend (WSAEventSelect) - Microsoft docs say it's level-triggered
    // We'll fix stale event/callback issues to make it work properly
    // Don't avoid win32 - we want to use WSAEventSelect
    event_config_set_flag(cfg, EVENT_BASE_FLAG_NOLOCK); // No threading needed for single-threaded POC
#endif
    d_eventBase = event_base_new_with_config(cfg);
    event_config_free(cfg);
  }
  
  if (d_eventBase == nullptr) {
    d_eventBase = event_base_new();  // Fallback to default
  }
  
  if (d_eventBase == nullptr) {
    throw FDMultiplexerException("Failed to create libevent base");
  }
  
  const char* method = event_base_get_method(d_eventBase);
  std::cout << "[DEBUG] libevent: Using backend: " << (method ? method : "unknown") << std::endl;
  
  // List all available backends for debugging
  const char** methods = event_get_supported_methods();
  std::cout << "[DEBUG] libevent: Available backends:";
  for (int i = 0; methods[i] != nullptr; i++) {
    std::cout << " " << methods[i];
  }
  std::cout << std::endl;

  // Self-test: create a temporary socket to verify libevent works
  int testfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (testfd >= 0) {
    try {
      addReadFD(testfd, [](int, funcparam_t&){});
      removeReadFD(testfd);
      close(testfd);
    }
    catch (const FDMultiplexerException& fe) {
      close(testfd);
      event_base_free(d_eventBase);
      throw FDMultiplexerException("libevent multiplexer failed self-test: " + std::string(fe.what()));
    }
  }
}

// Static callback that routes to the correct callback
void LibeventFDMultiplexer::eventCallback(evutil_socket_t fd, short what, void* arg)
{
  std::cout << "[DEBUG] libevent: eventCallback fired for fd=" << fd << " what=" << what << " (EV_READ=" << EV_READ << " EV_WRITE=" << EV_WRITE << ")" << std::endl;
  auto* info = static_cast<FDCallbackInfo*>(arg);
  auto* mplex = info->mplex;
  int fdInt = static_cast<int>(fd);

  std::cout << "[DEBUG] libevent: callback fired fd=" << fdInt << " what=" << what
            << " (EV_READ=" << (what & EV_READ) << " EV_WRITE=" << (what & EV_WRITE) << ")" << std::endl;
  
  // Do not read here; leave data for the registered callback
  
  if (what & EV_READ) {
    const auto& iter = mplex->d_readCallbacks.find(fdInt);
    if (iter != mplex->d_readCallbacks.end()) {
      std::cout << "[IO_TRACKING] LIBEVENT/WSAEventSelect: Detected event, calling read callback for fd=" << fdInt << std::endl;
      // Set flag to indicate this is from libevent/WSAEventSelect (primary mechanism)
      // Note: The flag is set via a function call in the callback chain
      iter->d_callback(iter->d_fd, iter->d_parameter);
      
      // CRITICAL FIX for WSAEventSelect level-triggered behavior:
      // After calling the callback (which may read data), we need to ensure
      // the event is still enabled. With EV_PERSIST, libevent should handle this,
      // but for connected UDP sockets on Windows, we may need to explicitly
      // re-add the event to ensure WSAEventSelect state is cleared and re-enabled.
      // This is especially important if the callback only partially reads data.
      // Check if event is still in our map (hasn't been removed)
      auto evIt = mplex->d_readEvents.find(fdInt);
      if (evIt != mplex->d_readEvents.end() && evIt->second) {
        // Re-add the event to ensure WSAEventSelect state is cleared
        // This helps with level-triggered behavior for connected UDP sockets
        int re_add_result = event_add(evIt->second, nullptr);
        if (re_add_result == 0) {
          std::cout << "[DEBUG] libevent: re-added read event for fd=" << fdInt << " to ensure level-triggered behavior" << std::endl;
        } else {
          std::cout << "[DEBUG] libevent: WARNING - failed to re-add read event for fd=" << fdInt << std::endl;
        }
      }
    } else {
      std::cout << "[DEBUG] libevent: no read callback registered for fd=" << fdInt << std::endl;
    }
  }
  if (what & EV_WRITE) {
    const auto& iter = mplex->d_writeCallbacks.find(fdInt);
    if (iter != mplex->d_writeCallbacks.end()) {
      std::cout << "[DEBUG] libevent: calling write callback for fd=" << fdInt << std::endl;
      iter->d_callback(iter->d_fd, iter->d_parameter);
      
      // Same fix for write events
      auto evIt = mplex->d_writeEvents.find(fdInt);
      if (evIt != mplex->d_writeEvents.end() && evIt->second) {
        int re_add_result = event_add(evIt->second, nullptr);
        if (re_add_result == 0) {
          std::cout << "[DEBUG] libevent: re-added write event for fd=" << fdInt << " to ensure level-triggered behavior" << std::endl;
        }
      }
    } else {
      std::cout << "[DEBUG] libevent: no write callback registered for fd=" << fdInt << std::endl;
    }
  }
}

int LibeventFDMultiplexer::run(struct timeval* tv, int timeout)
{
  InRun guard(d_inrun);
  
  // Update timeval if provided
  struct timeval now;
  gettimeofday(&now, nullptr);
  if (tv) {
    *tv = now;
  }
  
  int ret = 0;
  
  if (timeout == 0) {
    // Non-blocking mode - check once and return
    ret = event_base_loop(d_eventBase, EVLOOP_NONBLOCK | EVLOOP_ONCE);
  }
  else if (timeout == -1) {
    // Blocking mode - wait indefinitely
    ret = event_base_loop(d_eventBase, 0);
  }
  else {
    // Timed mode - use a timeout event to ensure loop exits
    struct timeval tv_timeout;
    tv_timeout.tv_sec = timeout / 1000;
    tv_timeout.tv_usec = (timeout % 1000) * 1000;
    
    // Debug: show timeout value
    std::cout << "[DEBUG] libevent: Timeout set to " << tv_timeout.tv_sec << "s " << tv_timeout.tv_usec << "us" << std::endl;
    
    // Create a one-shot timeout event that will exit the loop
    static int timeout_count = 0;
    struct event* timeout_event = evtimer_new(d_eventBase, [](evutil_socket_t, short, void* base) {
      timeout_count++;
      std::cout << "[DEBUG] libevent: Timeout event fired (count=" << timeout_count << ")" << std::endl;
      // This callback exits the event loop
      event_base_loopexit(static_cast<event_base*>(base), nullptr);
    }, d_eventBase);
    
    // Add the timeout event (one-shot)
    evtimer_add(timeout_event, &tv_timeout);
    
    // Run loop - will exit when timeout expires or events occur
    std::cout << "[DEBUG] libevent: Starting loop with timeout=" << timeout << "ms" << std::endl;
    ret = event_base_loop(d_eventBase, 0);  // Use 0 instead of EVLOOP_ONCE to respect timeout
    std::cout << "[DEBUG] libevent: Loop exited, ret=" << ret << std::endl;
    
    // Clean up timeout event
    evtimer_del(timeout_event);
    event_free(timeout_event);
  }
  
  if (ret < 0) {
    throw FDMultiplexerException("libevent loop failed");
  }
  
  // event_base_loop returns:
  //   0 = normal exit (timeout expired OR loopexit called OR no events)
  //  -1 = error
  // We can't easily distinguish timeout from events processed
  // For now, return 0 (timeout) since we can't tell if events fired
  // The callback debug output will show if events actually fired
  return 0; // Assume timeout for now - callbacks will fire if events occur
}

void LibeventFDMultiplexer::getAvailableFDs(std::vector<int>& fds, int timeout)
{
  // Trigger the event loop
  struct timeval tv;
  run(&tv, timeout);
  
  // FDs with callbacks are already in d_readCallbacks and d_writeCallbacks
  // We need to return all FDs that have events
  fds.clear();
  
  for (const auto& pair : d_readEvents) {
    if (event_pending(pair.second, EV_READ, nullptr)) {
      fds.push_back(pair.first);
    }
  }
  for (const auto& pair : d_writeEvents) {
    if (event_pending(pair.second, EV_WRITE, nullptr)) {
      fds.push_back(pair.first);
    }
  }
}

void LibeventFDMultiplexer::addFD(int fd, FDMultiplexer::EventKind kind)
{
  std::cout << "[DEBUG] libevent: addFD called for fd=" << fd << " kind=" 
            << (kind == EventKind::Read ? "Read" : kind == EventKind::Write ? "Write" : "Both") << std::endl;
  
  // CRITICAL FIX: On Windows, if a socket is closed and fd is reused,
  // we must remove any stale events AND callbacks first, otherwise WSAEventSelect
  // won't be properly re-registered for the new socket, and stale callbacks will linger
  if (kind == EventKind::Read || kind == EventKind::Both) {
    if (d_readEvents.find(fd) != d_readEvents.end()) {
      std::cout << "[DEBUG] libevent: WARNING - removing stale read event for reused fd=" << fd << std::endl;
      // Remove from events map (cleans libevent state)
      removeFD(fd, EventKind::Read);
      // Also remove stale callback if it exists (prevents stale pointer issues)
      // Use the FDBasedTag index to find and erase from multi_index_container
      auto& idx = d_readCallbacks.get<FDMultiplexer::FDBasedTag>();
      auto it = idx.find(fd);
      if (it != idx.end()) {
        std::cout << "[DEBUG] libevent: WARNING - also removing stale read callback for reused fd=" << fd << std::endl;
        idx.erase(it);
      }
    }
  }
  if (kind == EventKind::Write || kind == EventKind::Both) {
    if (d_writeEvents.find(fd) != d_writeEvents.end()) {
      std::cout << "[DEBUG] libevent: WARNING - removing stale write event for reused fd=" << fd << std::endl;
      // Remove from events map (cleans libevent state)
      removeFD(fd, EventKind::Write);
      // Also remove stale callback if it exists (prevents stale pointer issues)
      // Use the FDBasedTag index to find and erase from multi_index_container
      auto& idx = d_writeCallbacks.get<FDMultiplexer::FDBasedTag>();
      auto it = idx.find(fd);
      if (it != idx.end()) {
        std::cout << "[DEBUG] libevent: WARNING - also removing stale write callback for reused fd=" << fd << std::endl;
        idx.erase(it);
      }
    }
  }
  
  short eventFlags = 0;
  
  if (kind == EventKind::Read || kind == EventKind::Both) {
    eventFlags |= EV_READ;
  }
  if (kind == EventKind::Write || kind == EventKind::Both) {
    eventFlags |= EV_WRITE;
  }
  // Make events persistent so they keep firing without being re-added
  eventFlags |= EV_PERSIST;
  
  // Create callback info
  auto* info = new FDCallbackInfo;
  info->mplex = this;
  info->fd = fd;
  info->isRead = (kind == EventKind::Read || kind == EventKind::Both);
  
  // Do not force non-blocking on Windows UDP; we handle readiness via events
  
  // CRITICAL: Always create a NEW event with event_new() for each socket registration
  // This ensures WSAEventSelect is properly registered for the socket
  // Even if the fd was reused, event_new() will create a fresh event structure
  std::cout << "[DEBUG] libevent: calling event_new() for fd=" << fd << " flags=" << eventFlags << std::endl;
  struct event* ev = event_new(d_eventBase, fd, eventFlags, eventCallback, info);
  if (ev == nullptr) {
    delete info;
    throw FDMultiplexerException("Failed to create libevent event for fd " + std::to_string(fd));
  }
  std::cout << "[DEBUG] libevent: event_new() succeeded for fd=" << fd << " event=" << static_cast<void*>(ev) << std::endl;
  
  // Store the event
  if (kind == EventKind::Read || kind == EventKind::Both) {
    d_readEvents[fd] = ev;
  }
  if (kind == EventKind::Write || kind == EventKind::Both) {
    d_writeEvents[fd] = ev;
  }
  
  // CRITICAL: Add the event to the event base - this registers WSAEventSelect
  // event_add() will call WSAEventSelect() internally via libevent's win32 backend
  std::cout << "[DEBUG] libevent: calling event_add() for fd=" << fd << " event=" << static_cast<void*>(ev) << std::endl;
  int add_result = event_add(ev, nullptr);
  if (add_result != 0) {
    std::cout << "[DEBUG] libevent: ERROR - event_add() failed for fd=" << fd << std::endl;
    event_free(ev);
    delete info;
    throw FDMultiplexerException("Failed to add libevent event for fd " + std::to_string(fd));
  }
  std::cout << "[DEBUG] libevent: event_add() succeeded for fd=" << fd << std::endl;
  
  // Verify the event is in the event base
  int pending = event_pending(ev, EV_READ | EV_WRITE, nullptr);
  std::cout << "libevent: Added event for fd=" << fd << " kind=" 
            << (kind == EventKind::Read ? "Read" : kind == EventKind::Write ? "Write" : "Both")
            << " flags=" << eventFlags << " pending=" << pending << " event=" << static_cast<void*>(ev) << std::endl;
  
#ifdef _WIN32
  // CRITICAL: After adding event, verify WSAEventSelect is properly registered
  // Check if data is already available (shouldn't be, but verify)
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(fd, &readfds);
  struct timeval tv = {0, 0};
  int select_result = select(fd + 1, &readfds, nullptr, nullptr, &tv);
  if (select_result > 0 && FD_ISSET(fd, &readfds)) {
    std::cout << "[DEBUG] libevent: WARNING - data already available on fd=" << fd << " immediately after event_add (shouldn't happen)" << std::endl;
  } else if (select_result < 0) {
    int werr = WSAGetLastError();
    std::cout << "[DEBUG] libevent: select() check failed for fd=" << fd << " WSA error=" << werr << std::endl;
  } else {
    std::cout << "[DEBUG] libevent: socket fd=" << fd << " is ready for event monitoring (no data yet)" << std::endl;
  }
  
  // Verify socket is still valid
  {
    sockaddr_in test_addr{};
    socklen_t test_len = sizeof(test_addr);
    int getpeername_result = getpeername(fd, reinterpret_cast<sockaddr*>(&test_addr), &test_len);
    if (getpeername_result == 0) {
      std::cout << "[DEBUG] libevent: socket fd=" << fd << " is still connected after event_add" << std::endl;
    } else {
      int werr = WSAGetLastError();
      std::cout << "[DEBUG] libevent: WARNING - getpeername failed for fd=" << fd << " WSA error=" << werr << " after event_add" << std::endl;
    }
  }
#endif
}

void LibeventFDMultiplexer::removeFD(int fd, FDMultiplexer::EventKind kind)
{
  std::cout << "[DEBUG] libevent: removeFD called for fd=" << fd << " kind=" 
            << (kind == EventKind::Read ? "Read" : kind == EventKind::Write ? "Write" : "Both") << std::endl;
  
  if (kind == EventKind::Read || kind == EventKind::Both) {
    auto it = d_readEvents.find(fd);
    if (it != d_readEvents.end()) {
      std::cout << "[DEBUG] libevent: removing read event for fd=" << fd << " event=" << static_cast<void*>(it->second) << std::endl;
      
      // CRITICAL: Remove callback from d_readCallbacks BEFORE deleting event
      // This prevents stale callback pointers
      {
        auto& idx = d_readCallbacks.get<FDMultiplexer::FDBasedTag>();
        auto cbIt = idx.find(fd);
        if (cbIt != idx.end()) {
          std::cout << "[DEBUG] libevent: removing read callback for fd=" << fd << std::endl;
          idx.erase(cbIt);
        } else {
          std::cout << "[DEBUG] libevent: WARNING - no read callback found for fd=" << fd << " (may already be removed)" << std::endl;
        }
      }
      
      // Extract callback info before freeing
      FDCallbackInfo* info = static_cast<FDCallbackInfo*>(event_get_callback_arg(it->second));
      
      // CRITICAL: event_del() removes the event from libevent's event base
      // This should also clear WSAEventSelect association in libevent's win32 backend
      std::cout << "[DEBUG] libevent: calling event_del() for fd=" << fd << std::endl;
      event_del(it->second);
      
      // CRITICAL: event_free() frees the event structure completely
      // This ensures libevent's backend releases all resources (including WSAEventSelect handle)
      std::cout << "[DEBUG] libevent: calling event_free() for fd=" << fd << std::endl;
      event_free(it->second);
      
      // Delete the callback info
      delete info;
      
      // Remove from our map
      d_readEvents.erase(it);
      std::cout << "[DEBUG] libevent: read event completely removed and freed for fd=" << fd << std::endl;
    } else {
      std::cout << "[DEBUG] libevent: no read event found for fd=" << fd << " (already removed?)" << std::endl;
    }
  }
  
  if (kind == EventKind::Write || kind == EventKind::Both) {
    auto it = d_writeEvents.find(fd);
    if (it != d_writeEvents.end()) {
      std::cout << "[DEBUG] libevent: removing write event for fd=" << fd << " event=" << static_cast<void*>(it->second) << std::endl;
      
      // CRITICAL: Remove callback from d_writeCallbacks BEFORE deleting event
      {
        auto& idx = d_writeCallbacks.get<FDMultiplexer::FDBasedTag>();
        auto cbIt = idx.find(fd);
        if (cbIt != idx.end()) {
          std::cout << "[DEBUG] libevent: removing write callback for fd=" << fd << std::endl;
          idx.erase(cbIt);
        }
      }
      
      // Extract callback info before freeing
      FDCallbackInfo* info = static_cast<FDCallbackInfo*>(event_get_callback_arg(it->second));
      
      std::cout << "[DEBUG] libevent: calling event_del() for write fd=" << fd << std::endl;
      event_del(it->second);
      
      std::cout << "[DEBUG] libevent: calling event_free() for write fd=" << fd << std::endl;
      event_free(it->second);
      
      delete info;
      d_writeEvents.erase(it);
      std::cout << "[DEBUG] libevent: write event completely removed and freed for fd=" << fd << std::endl;
    } else {
      std::cout << "[DEBUG] libevent: no write event found for fd=" << fd << " (already removed?)" << std::endl;
    }
  }
  
#ifdef _WIN32
  // CRITICAL: After event_del() and event_free(), also explicitly clear WSAEventSelect
  // This ensures that if the fd is reused, there's no stale WSAEventSelect state
  // Even though libevent should have done this, we ensure it's cleared
  // Note: Only do this if socket is still valid (not closed yet)
  if (kind == EventKind::Read || kind == EventKind::Both) {
    // Check if socket is still valid before trying to clear WSAEventSelect
    int optval = 0;
    socklen_t optlen = sizeof(optval);
    int sock_check = getsockopt(fd, SOL_SOCKET, SO_TYPE, reinterpret_cast<char*>(&optval), &optlen);
    if (sock_check == 0) {
      // Socket is still valid, clear WSAEventSelect
      WSAEventSelect(fd, NULL, 0);
      std::cout << "[DEBUG] libevent: explicitly cleared WSAEventSelect for fd=" << fd << " after event removal" << std::endl;
    } else {
      // Socket is already closed, skip WSAEventSelect clear
      std::cout << "[DEBUG] libevent: socket fd=" << fd << " is already closed, skipping WSAEventSelect clear" << std::endl;
    }
  }
#endif
}

void LibeventFDMultiplexer::alterFD(int fd, FDMultiplexer::EventKind from, FDMultiplexer::EventKind to)
{
  // Remove old event
  removeFD(fd, from);
  
  // Add new event
  addFD(fd, to);
}
