#ifndef TRADELAYER_FETCHWALLETTX_H
#define TRADELAYER_FETCHWALLETTX_H

class uint256;

#include <map>
#include <string>

namespace mastercore
{
/** Gets the byte offset of a transaction from the transaction index. */
unsigned int GetTransactionByteOffset(const uint256& txid);

/** Returns an ordered list of Trade Layer transactions that are relevant to the wallet. */
std::map<std::string, uint256> FetchWalletTLTransactions(unsigned int count, int startBlock = 0, int endBlock = 9999999);
}

#endif // TRADELAYER_FETCHWALLETTX_H
