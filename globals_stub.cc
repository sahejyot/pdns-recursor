/*
 * Minimal stub file for global variable definitions needed for linking
 * In a full implementation, these would be initialized in pdns_recursor.cc/main
 */

#include "syncres.hh"
#include "negcache.hh"
#include "recursor_cache.hh"
#include "arguments.hh"
#include "logging.hh"
#include "logr.hh"
#include "tcpiohandler.hh"

// Global cache instances (will be initialized by main)
std::unique_ptr<MemRecursorCache> g_recCache;
std::unique_ptr<NegCache> g_negCache;

// Global configuration variables
bool g_lowercaseOutgoing = false;
unsigned int g_networkTimeoutMsec = 1500;
uint16_t g_outgoingEDNSBufsize = 4096;
unsigned int g_maxMThreads = 2048; // Default MTasker thread limit
bool g_logCommonErrors = true; // Log common UDP errors

// DNSSEC-related globals (referenced even when DNSSEC is disabled)
GlobalStateHolder<SuffixMatchNode> g_xdnssec;
GlobalStateHolder<SuffixMatchNode> g_dontThrottleNames;
GlobalStateHolder<NetmaskGroup> g_dontThrottleNetmasks;
GlobalStateHolder<SuffixMatchNode> g_DoTToAuthNames;
DNSSECMode g_dnssecmode{DNSSECMode::ProcessNoValidate};
// Note: g_paddingOutgoing and g_ECSHardening are defined in lwres.cc, not here

// Global logger instance (structured loggers are defined elsewhere)
std::shared_ptr<Logging::Logger> g_slog{nullptr};
std::shared_ptr<Logr::Logger> g_slogudpin{nullptr};

// Global argument map instance
static ArgvMap s_arg;
ArgvMap& arg()
{
  return s_arg;
}

// TCPIOHandler unit test flag is defined upstream as const in tcpiohandler.cc; no definition here

