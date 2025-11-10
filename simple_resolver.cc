/*
 * Simple DNS Resolver for Windows - Implementation
 * Minimal implementation for Sprint 4
 */

#include "simple_resolver.hh"
#include <iostream>
#include <cstring>

SimpleDNSResolver::SimpleDNSResolver() {
    std::cout << "SimpleDNSResolver initialized" << std::endl;
}

SimpleDNSResolver::~SimpleDNSResolver() {
    std::cout << "SimpleDNSResolver destroyed" << std::endl;
}

bool SimpleDNSResolver::resolve(const std::string& query, std::vector<uint8_t>& response) {
    std::cout << "Resolving query: " << query << std::endl;
    
    // For now, just return a hardcoded response
    // TODO: Implement actual DNS resolution
    response = {0x00, 0x00, 0x81, 0x80, 0x00, 0x00, 0x00, 0x01};
    return true;
}

bool SimpleDNSResolver::parseQuery(const uint8_t* data, size_t len, std::string& qname, uint16_t& qtype) {
    if (len < 12) {
        return false; // DNS header is 12 bytes
    }
    
    // Parse DNS header
    const SimpleDNSHeader* dh = reinterpret_cast<const SimpleDNSHeader*>(data);
    
    // Check if it's a query (QR bit should be 0)
    if (dh->flags & 0x8000) {
        return false;
    }
    
    // For now, just return dummy values
    qname = "example.com";
    qtype = 1; // A record
    
    return true;
}

bool SimpleDNSResolver::createResponse(const std::string& qname, uint16_t qtype, std::vector<uint8_t>& response) {
    // Create a simple DNS response
    response.clear();
    
    // DNS header (12 bytes)
    response.insert(response.end(), {0x00, 0x00, 0x81, 0x80, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00});
    
    // TODO: Add actual DNS records
    // For now, just return the header
    
    return true;
}

std::string SimpleDNSResolver::getHardcodedResponse(const std::string& qname, uint16_t qtype) {
    // Simple hardcoded responses for testing
    if (qname == "google.com" && qtype == 1) {
        return "8.8.8.8";
    }
    if (qname == "example.com" && qtype == 1) {
        return "93.184.216.34";
    }
    
    return ""; // No response
}
