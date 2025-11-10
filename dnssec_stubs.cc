// Minimal stubs to satisfy linking when HAVE_DNSSEC=0
#include "validate-recursor.hh"
#include "validate.hh"
#include "dnsrecords.hh"
#include "syncres.hh"
#include <ostream>

#if defined(HAVE_DNSSEC) && !HAVE_DNSSEC
const std::string& vStateToString(vState)
{
  static const std::string s("Insecure");
  return s;
}

std::ostream& operator<<(std::ostream& os, vState)
{
  os << "Insecure";
  return os;
}

std::ostream& operator<<(std::ostream& os, dState)
{
  os << "NoDNSSEC";
  return os;
}

DNSName getSigner(const std::vector<std::shared_ptr<const RRSIGRecordContent>>&)
{
  return DNSName();
}

bool isRRSIGNotExpired(time_t, const RRSIGRecordContent&)
{
  return true;
}

dState getDenial(const cspmap_t&, const DNSName&, uint16_t, bool, bool, pdns::validation::ValidationContext&, const OptLog&, bool, unsigned int)
{
  return dState::INSECURE;
}

void updateDNSSECValidationState(vState&, vState)
{
}

vState increaseXDNSSECStateCounter(const vState& state)
{
  return state;
}

vState increaseDNSSECStateCounter(const vState& state)
{
  return state;
}

bool isSupportedDS(const DSRecordContent&, const std::optional<LogVariant>&)
{
  return false;
}

namespace pdns {
void dedupRecords(std::vector<DNSRecord>&)
{
}
}

// Additional DNSSEC validation stubs
vState validateWithKeySet(time_t /* now */, const DNSName& /* name */,
                           const sortedRecords_t& /* toSign */,
                           const std::vector<std::shared_ptr<const RRSIGRecordContent>>& /* signatures */,
                           const skeyset_t& /* keys */,
                           const OptLog& /* log */,
                           pdns::validation::ValidationContext& /* context */,
                           bool /* validateAllSigs */)
{
  return vState::Indeterminate;
}

bool isWildcardExpanded(unsigned int /* labelCount */, const RRSIGRecordContent& /* rrsig */)
{
  return false;
}

bool isWildcardExpandedOntoItself(const DNSName& /* name */, unsigned int /* labelCount */, const RRSIGRecordContent& /* rrsig */)
{
  return false;
}
#endif


