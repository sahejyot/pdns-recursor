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

# Windows platform definitions
add_definitions(-D_WIN32_WINNT=0x0601)  # Windows 7+
add_definitions(-DWIN32_LEAN_AND_MEAN)
add_definitions(-DNOMINMAX)

# Find dependencies
find_package(Boost QUIET COMPONENTS context system container)
find_package(OpenSSL QUIET)
find_package(Libevent QUIET)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/protozero/include
)

# Add dependency includes if found
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
endif()

if(OPENSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIR})
endif()

if(LIBEVENT_FOUND)
    include_directories(${LIBEVENT_INCLUDE_DIRS})
endif()

# Compiler-specific flags
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC/MinGW compiler flags (MSYS2 build)
    add_compile_options(
        -Wall                  # Enable all warnings
        -Wextra               # Extra warnings
        -Wno-unused-parameter # Suppress unused parameter
        -O2                   # Optimization level 2
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # MSVC compiler flags
    add_compile_options(
        /W3                   # Warning level 3
        /O2                   # Optimization level 2
    )
endif()

# Source files - Sprint 4: Core DNS Resolution (Test dnsparser/dnswriter with QClass)
set(PDNS_RECURSOR_SOURCES
    # Simple DNS resolver (Sprint 4 - minimal implementation)
    simple_resolver.cc

    # Core DNS files - Test with QClass fixes
    dnsname.cc
    qtype.cc
    dnsparser.cc
    dnswriter.cc
    dnsrecords.cc
    
    # Missing PowerDNS functions
    dnslabeltext.cc
    svc-records.cc
    misc.cc
    
    # Windows compatibility layer
    regex_compat.cc
    ext/arc4random/arc4random_windows.cc
    base32.cc
    ednscookies.cc
    ednspadding.cc
    
    # I/O multiplexer
    libeventmplexer.cc
    
    # Core resolver pieces (local copies)
    syncres.cc
    lwres.cc
    recursor_cache.cc
    negcache.cc
    logger.cc
    # DNSSEC disabled on Windows POC - aggressive_nsec.cc requires DNSSEC functions
    # aggressive_nsec.cc
    # validate.cc
    # dnssecinfra.cc
    remote_logger.cc
    # Additional required sources
    iputils.cc
    arguments.cc
    rcpgenerator.cc
    base64.cc
    filterpo.cc
    globals_stub.cc

    rec-taskqueue.cc
    # rec-xfr.cc
    rec-protozero.cc
    protozero.cc
    gettime.cc
    uuid-utils.cc
    threadname.cc
    taskqueue.cc
    mtasker_context.cc
    # validate-recursor.cc
    libssl.cc
    tcpiohandler.cc
    # zonemd.cc
    ednssubnet.cc
    dns.cc
    dnssec_stubs.cc
    lwres_stubs.cc
    rec-tcounters.cc
    query-local-address.cc
    rec-tcpout.cc
    version.cc
)

# Enable upstream UDP client pieces from pdns_recursor (required for UDP resolution)
option(ENABLE_WINDOWS_POC_PARTS "Enable minimal upstream UDP parts (asendto/arecvfrom, UDPClientSocks)" ON)
if(ENABLE_WINDOWS_POC_PARTS)
    list(APPEND PDNS_RECURSOR_SOURCES pdns_recursor_poc_parts.cc)
endif()

# Main executable - POC
add_executable(pdns_recursor_poc2 main_test.cc ${PDNS_RECURSOR_SOURCES})
target_compile_definitions(pdns_recursor_poc2 PRIVATE RECURSOR)

# Add ENABLE_WINDOWS_POC_PARTS flag after target is created
if(ENABLE_WINDOWS_POC_PARTS)
    target_compile_definitions(pdns_recursor_poc2 PRIVATE ENABLE_WINDOWS_POC_PARTS)
endif()

# Force-include Windows compatibility and local misc for upstream-sourced files
set_source_files_properties(
    syncres.cc lwres.cc recursor_cache.cc negcache.cc
    PROPERTIES COMPILE_FLAGS "-include ${CMAKE_CURRENT_SOURCE_DIR}/windows-compat.h -include ${CMAKE_CURRENT_SOURCE_DIR}/misc.hh"
)

# Link libraries conditionally
if(Boost_FOUND)
    target_link_libraries(pdns_recursor_poc2 PRIVATE
        Boost::context
        Boost::system
        Boost::container
    )
endif()

if(OPENSSL_FOUND)
    target_link_libraries(pdns_recursor_poc2 PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(pdns_recursor_poc2 PRIVATE HAVE_LIBCRYPTO)
else()
    # Fallback for MSYS2/MinGW where headers are in default include path but FindOpenSSL fails
    if(MINGW)
        target_link_libraries(pdns_recursor_poc2 PRIVATE crypto ssl)
        target_compile_definitions(pdns_recursor_poc2 PRIVATE HAVE_LIBCRYPTO)
    endif()
endif()

# Generate config.h from template (like Autotools)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

# Set feature detection results for config.h
set(HAVE_LIBCRYPTO ${OPENSSL_FOUND})
set(HAVE_BOOST ${Boost_FOUND})
set(HAVE_BOOST_CONTEXT ${Boost_FOUND})
set(HAVE_BOOST_FILESYSTEM ${Boost_FOUND})
set(HAVE_BOOST_THREAD ${Boost_FOUND})

# Windows-specific feature detection
if(WIN32)
    # Set to 0 for Windows (features not available)
    set(HAVE_ARC4RANDOM 0)
    set(HAVE_ARC4RANDOM_BUF 0)
    set(HAVE_ARC4RANDOM_UNIFORM 0)
    set(HAVE_STRERROR_R 0)
    set(HAVE_LOCALTIME_R 0)
    set(HAVE_ACCEPT4 0)
    set(HAVE_RECVMMSG 0)
    set(HAVE_SENDMMSG 0)
    set(HAVE_PTHREAD_SETAFFINITY_NP 0)
    set(HAVE_PTHREAD_GETATTR_NP 0)
    set(HAVE_GETIFADDRS 0)
    set(HAVE_GETENTROPY 0)
    set(HAVE_GETRANDOM 0)
    set(HAVE_DLFCN_H 0)
    set(HAVE_UNISTD_H 0)
    set(HAVE_SYS_RANDOM_H 0)
    set(HAVE_SYS_STAT_H 0)
    set(HAVE_SYS_TYPES_H 0)
    set(HAVE_STRINGS_H 0)
    set(HAVE_STRING_H 0)
    set(HAVE_STDINT_H 0)
    set(HAVE_STDIO_H 0)
    set(HAVE_STDLIB_H 0)
    set(HAVE_INTTYPES_H 0)
    set(HAVE_WCHAR_H 0)
    set(HAVE_LIBCAP 0)
    set(HAVE_LIBCURL 0)
    set(HAVE_LIBSODIUM 0)
    set(HAVE_LUA 0)
    add_definitions(-DHAVE_LUA=0)  # Define as preprocessor macro for conditional compilation
    set(HAVE_CLOCK_GETTIME 0)
    set(HAVE_CRYPTO_MEMCMP 0)
    set(HAVE_EXPLICIT_BZERO 0)
    set(HAVE_GMTIME_R 0)
    set(HAVE_OPENSSL_INIT_CRYPTO 0)
    set(HAVE_RAND_BYTES 0)
    set(HAVE_RAND_PSEUDO_BYTES 0)
    set(HAVE_RSA_GET0_KEY 0)
    set(HAVE_SODIUM_MEMCMP 0)
    set(HAVE_EVP_MD_CTX_FREE 0)
    set(HAVE_EVP_MD_CTX_NEW 0)
    set(HAVE_EVP_PKEY_CTX_SET1_SCRYPT_SALT 0)
    set(HAVE_DECL_STRERROR_R 0)
    set(HAVE_PTHREAD_SETNAME_NP_2 0)
    set(STRERROR_R_CHAR_P 0)
else()
    # Set to 1 for Linux/Unix (features available)
    set(HAVE_ARC4RANDOM 1)
    set(HAVE_ARC4RANDOM_BUF 1)
    set(HAVE_ARC4RANDOM_UNIFORM 1)
    set(HAVE_STRERROR_R 1)
    set(HAVE_LOCALTIME_R 1)
    set(HAVE_ACCEPT4 1)
    set(HAVE_RECVMMSG 1)
    set(HAVE_SENDMMSG 1)
    set(HAVE_PTHREAD_SETAFFINITY_NP 1)
    set(HAVE_PTHREAD_GETATTR_NP 1)
    set(HAVE_GETIFADDRS 1)
    set(HAVE_GETENTROPY 1)
    set(HAVE_GETRANDOM 1)
    set(HAVE_DLFCN_H 1)
    set(HAVE_UNISTD_H 1)
    set(HAVE_SYS_RANDOM_H 1)
    set(HAVE_SYS_STAT_H 1)
    set(HAVE_SYS_TYPES_H 1)
    set(HAVE_STRINGS_H 1)
    set(HAVE_STRING_H 1)
    set(HAVE_STDINT_H 1)
    set(HAVE_STDIO_H 1)
    set(HAVE_STDLIB_H 1)
    set(HAVE_INTTYPES_H 1)
    set(HAVE_WCHAR_H 1)
    set(HAVE_CLOCK_GETTIME 1)
    set(HAVE_GMTIME_R 1)
    set(HAVE_DECL_STRERROR_R 1)
    set(HAVE_PTHREAD_SETNAME_NP_2 1)
    set(STRERROR_R_CHAR_P 1)
endif()

# Set build info
set(BUILD_HOST "${CMAKE_HOST_SYSTEM_NAME}-${CMAKE_HOST_SYSTEM_PROCESSOR}")
set(PROJECT_VERSION "5.3.0.2.relrec53x.g594400838")

# Include generated config.h
target_include_directories(pdns_recursor_poc2 PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Disable Lua integration on Windows for now
target_compile_definitions(pdns_recursor_poc2 PRIVATE
    HAVE_LUA=0
    HAVE_LUA_RECURSOR=0
    HAVE_LUA_RECORDS=0
    HAVE_DNSSEC=0
)

# Link libevent libraries directly
target_link_libraries(pdns_recursor_poc2 PRIVATE
    event
    event_extra
)

# Windows libraries (always needed)
target_link_libraries(pdns_recursor_poc2 PRIVATE
    ws2_32      # Windows sockets
    iphlpapi    # IP Helper API
)

# Installation rules
# install(TARGETS pdns_recursor RUNTIME DESTINATION bin)

message(STATUS "PowerDNS Recursor Windows Port - CMake Configuration")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

if(Boost_FOUND)
    message(STATUS "  Boost version: ${Boost_VERSION}")
endif()

if(OPENSSL_FOUND)
    message(STATUS "  OpenSSL version: ${OPENSSL_VERSION}")
endif()

