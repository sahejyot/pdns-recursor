/*
 * Simple libevent test program for Windows
 * Tests that libevent is working properly on Windows
 */
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <cstdlib>

#pragma comment(lib, "ws2_32.lib")

static void on_accept(evconnlistener* listener, evutil_socket_t fd, 
                      struct sockaddr* addr, int socklen, void* arg)
{
    std::cout << "Accepted connection on socket " << fd << std::endl;
    EVUTIL_CLOSESOCKET(fd);
}

int main()
{
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }

    std::cout << "Initializing libevent..." << std::endl;
    struct event_base* base = event_base_new();
    if (!base) {
        std::cerr << "Failed to create event base" << std::endl;
        WSACleanup();
        return 1;
    }

    std::cout << "Creating TCP listener on port 8888..." << std::endl;
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(8888);
    sin.sin_addr.s_addr = htonl(0x7f000001); // 127.0.0.1

    struct evconnlistener* listener = evconnlistener_new_bind(
        base,
        on_accept,
        NULL,
        LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSABLE,
        -1,
        (struct sockaddr*)&sin,
        sizeof(sin)
    );

    if (!listener) {
        std::cerr << "Failed to create listener" << std::endl;
        event_base_free(base);
        WSACleanup();
        return 1;
    }

    std::cout << "Listening on 127.0.0.1:8888" << std::endl;
    std::cout << "Run: echo test | nc 127.0.0.1 8888" << std::endl;
    std::cout << "Press Ctrl+C to exit..." << std::endl;

    // Run event loop
    event_base_dispatch(base);

    // Cleanup
    evconnlistener_free(listener);
    event_base_free(base);
    WSACleanup();

    std::cout << "Test completed successfully!" << std::endl;
    return 0;
}


