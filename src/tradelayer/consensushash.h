#ifndef TRADELAYER_CONSENSUSHASH_H
#define TRADELAYER_CONSENSUSHASH_H

#include <uint256.h>

namespace mastercore
{

/** Obtains a hash of all balances to use for consensus verification and checkpointing. */
uint256 GetConsensusHash();

std::string kycGenerateConsensusString(const std::vector<std::string>& vstr);
std::string attGenerateConsensusString(const std::vector<std::string>& vstr);

}


#endif // TRADELAYER_CONSENSUSHASH_H
