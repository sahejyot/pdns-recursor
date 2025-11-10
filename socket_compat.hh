/*
 * Socket Compatibility Layer for Windows
 * Provides Winsock2 compatibility for Unix socket code
 */

#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdexcept>

// WSAStartup wrapper for compatibility
class WinsockInitializer {
public:
    WinsockInitializer() {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
    }
    
    ~WinsockInitializer() {
        WSACleanup();
    }
};

// Socket helper functions
inline int close_socket(int fd) {
    return closesocket(fd);
}

#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

// Unix/Linux - no wrappers needed
inline int close_socket(int fd) {
    return close(fd);
}
#endif

