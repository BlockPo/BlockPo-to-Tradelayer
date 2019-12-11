#ifndef TRADELAYER_WALLETCACHE_H
#define TRADELAYER_WALLETCACHE_H

class uint256;

#include <vector>

namespace mastercore
{
//! Global vector of Trade Layer transactions in the wallet
extern std::vector<uint256> walletTXIDCache;

/** Adds a txid to the wallet txid cache, performing duplicate detection */
void WalletTXIDCacheAdd(const uint256& hash);

/** Performs initial population of the wallet txid cache */
void WalletTXIDCacheInit();

/** Updates the cache and returns whether any wallet addresses were changed */
int WalletCacheUpdate();
}

#endif // TRADELAYER_WALLETCACHE_H
