/*
 * PowerDNS Recursor Windows - POC Main Program
 * Sprint 4: Basic DNS Resolution with Simple Resolver
 */

#include "socket_compat.hh"
#include "simple_resolver.hh"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <cstdint>
#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

#ifdef _WIN32
static WinsockInitializer g_winsock;
#endif

// Simple UDP DNS server with basic resolution
int main() {
    try {
        std::cout << "PowerDNS Recursor Windows POC - Starting with DNS Resolution..." << std::endl;
        
        // Create DNS resolver
        SimpleDNSResolver resolver;
        
        // Create UDP socket
        int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sockfd < 0) {
            throw std::runtime_error("Failed to create socket");
        }
        
        // Bind to port 5353
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(5353);
        
        if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close_socket(sockfd);
            throw std::runtime_error("Failed to bind socket");
        }
        
        std::cout << "Listening on port 5353..." << std::endl;
        
        // Simple UDP receive loop with DNS resolution
        char buffer[512];
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        while (true) {
            int n = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                           (struct sockaddr*)&client_addr, &client_len);
            if (n > 0) {
                std::cout << "Received " << n << " bytes" << std::endl;
                
                // Parse DNS query
                std::string qname;
                uint16_t qtype;
                if (resolver.parseQuery(reinterpret_cast<uint8_t*>(buffer), n, qname, qtype)) {
                    std::cout << "Query: " << qname << " (type " << qtype << ")" << std::endl;
                    
                    // Resolve query
                    std::vector<uint8_t> response;
                    if (resolver.resolve(qname, response)) {
                        // Send response
                        sendto(sockfd, reinterpret_cast<const char*>(response.data()), 
                              response.size(), 0, (struct sockaddr*)&client_addr, client_len);
                        std::cout << "Sent DNS response" << std::endl;
                    } else {
                        std::cout << "Failed to resolve query" << std::endl;
                    }
                } else {
                    std::cout << "Failed to parse DNS query" << std::endl;
                }
            }
        }
        
        close_socket(sockfd);
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}

