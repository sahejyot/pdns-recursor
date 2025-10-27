cmake_minimum_required(VERSION 3.20)
project(pdns-recursor-windows VERSION 5.3.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Platform detection
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)  # Windows 7+
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-DNOMINMAX)
endif()

# Find dependencies
find_package(Boost REQUIRED COMPONENTS context system container)
find_package(OpenSSL REQUIRED)
find_package(Libevent REQUIRED)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${LIBEVENT_INCLUDE_DIRS}
)

# Compiler flags
if(MSVC)
    add_compile_options(
        /W4                    # Warning level 4
        /WX-                   # Don't treat warnings as errors (for now)
        /permissive-          # Standards conformance
        /Zc:__cplusplus       # Enable updated __cplusplus macro
        /EHsc                 # Exception handling model
        /MP                   # Multi-processor compilation
        /bigobj               # Large object files
    )
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-D_SCL_SECURE_NO_WARNINGS)
endif()

# Source files
set(PDNS_RECURSOR_SOURCES
    dnsname.cc
    # dnsparser.cc  # Commented out for now - will enable incrementally
    # dnswriter.cc
    # qtype.cc
    # misc.cc
)

# Main executable (test compilation)
add_library(pdns_core STATIC ${PDNS_RECURSOR_SOURCES})

target_link_libraries(pdns_core PRIVATE
    Boost::context
    Boost::system
    Boost::container
    OpenSSL::SSL
    OpenSSL::Crypto
    libevent::core
    libevent::extra
    ws2_32  # Windows sockets
    iphlpapi  # IP Helper API
)

# Installation rules
# install(TARGETS pdns_recursor RUNTIME DESTINATION bin)

message(STATUS "PowerDNS Recursor Windows Port - CMake Configuration")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Boost version: ${Boost_VERSION}")
message(STATUS "  OpenSSL version: ${OPENSSL_VERSION}")

