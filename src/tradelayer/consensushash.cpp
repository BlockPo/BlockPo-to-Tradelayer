/**
 * @file consensushash.cpp
 *
 * This file contains the function to generate consensus hashes.
 */
#include "tradelayer/activation.h"
#include "tradelayer/consensushash.h"
#include "tradelayer/dex.h"
#include "tradelayer/mdex.h"
#include "tradelayer/log.h"
#include "tradelayer/parse_string.h"
#include "tradelayer/persistence.h"
#include "tradelayer/sp.h"
#include "tradelayer/tradelayer.h"

#include "arith_uint256.h"
#include "uint256.h"

#include <stdint.h>
#include <algorithm>
#include <string>
#include <vector>

#include <openssl/sha.h>
#include "tradelayer_matrices.h"

extern std::map<std::string,channel> channels_Map;
extern std::map<uint32_t, int64_t> cachefees;
extern std::map<uint32_t, int64_t> cachefees_oracles;
extern std::vector<std::string> vestingAddresses;

namespace mastercore
{

CMPTradeList *d_tradelistdb;


bool ShouldConsensusHashBlock(int block)
{
    if (msc_debug_consensus_hash_every_block) {
        return true;
    }

    if (!gArgs.IsArgSet("-tlshowblockconsensushash")) {
        return false;
    }

    const std::vector<std::string>& vecBlocks = gArgs.GetArgs("-tlshowblockconsensushash");
    for (auto it = vecBlocks.begin(); it != vecBlocks.end(); ++it)
    {
        int64_t paramBlock = StrToInt64(*it, false);
        if (paramBlock < 1) continue; // ignore non numeric values
        if (paramBlock == block) {
            return true;
        }
    }

    return false;
}

// Generates a consensus string for hashing based on a tally object
std::string GenerateConsensusString(const CMPTally& tallyObj, const std::string& address, const uint32_t propertyId)
{
    int64_t balance = tallyObj.getMoney(propertyId, BALANCE);
    int64_t sellOfferReserve = tallyObj.getMoney(propertyId, SELLOFFER_RESERVE);
    int64_t acceptReserve = tallyObj.getMoney(propertyId, ACCEPT_RESERVE);
    int64_t metaDExReserve = tallyObj.getMoney(propertyId, METADEX_RESERVE);
    int64_t contractdexReserved = tallyObj.getMoney(propertyId, CONTRACTDEX_RESERVE);
    int64_t positiveBalance = tallyObj.getMoney(propertyId, POSITIVE_BALANCE);
    int64_t negativeBalance = tallyObj.getMoney(propertyId, NEGATIVE_BALANCE);
    int64_t realizedProfit = tallyObj.getMoney(propertyId, REALIZED_PROFIT);
    int64_t realizedLosses = tallyObj.getMoney(propertyId, REALIZED_LOSSES);
    int64_t remaining = tallyObj.getMoney(propertyId, REMAINING);
    int64_t unvested = tallyObj.getMoney(propertyId, UNVESTED);

    // return a blank string if all balances are empty
    if (!balance && !sellOfferReserve && !acceptReserve && !metaDExReserve && !contractdexReserved && !positiveBalance && !negativeBalance && !realizedProfit && !realizedLosses && !remaining && !unvested) {
      return "";
    }

    return strprintf("%s|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d",  address, propertyId, balance, sellOfferReserve, acceptReserve,
		     metaDExReserve,contractdexReserved, positiveBalance, negativeBalance, realizedProfit, realizedLosses,
		     remaining, unvested);
}

// Generates a consensus string for hashing based on a DEx sell offer object
std::string GenerateConsensusString(const CMPOffer& offerObj, const std::string& address)
{
    return strprintf("%s|%s|%d|%d|%d|%d|%d|%d|%d|%d",
		     offerObj.getHash().GetHex(), address, offerObj.getOfferBlock(), offerObj.getProperty(), offerObj.getOfferAmountOriginal(),
		     offerObj.getLTCDesiredOriginal(), offerObj.getMinFee(), offerObj.getBlockTimeLimit(), offerObj.getSubaction(), offerObj.getOption());
}

// Generates a consensus string for hashing based on a DEx accept object
std::string GenerateConsensusString(const CMPAccept& acceptObj, const std::string& address)
{
    return strprintf("%s|%s|%d|%d|%d",
		     acceptObj.getHash().GetHex(), address, acceptObj.getAcceptAmount(), acceptObj.getAcceptAmountRemaining(),
             acceptObj.getAcceptBlock());
}


// Generates a consensus string for hashing based on a crowdsale object
std::string GenerateConsensusString(const CMPCrowd& crowdObj)
{
    return strprintf("%d|%d|%d|%d|%d",
		     crowdObj.getPropertyId(), crowdObj.getCurrDes(), crowdObj.getDeadline(), crowdObj.getUserCreated(),
		     crowdObj.getIssuerCreated());
}

// Generates a consensus string for hashing based on a property issuer
std::string GenerateConsensusString(const uint32_t propertyId, const std::string& address)
{
    return strprintf("%d|%s", propertyId, address);
}


// Generates a consensus string for hashing based on a channel object
std::string GenerateConsensusString(const channel& chn)
{
    return strprintf("%s|%s|%s|%d|%d",
		     chn.multisig, chn.first, chn.second, chn.expiry_height,
		     chn.last_exchange_block);
}

// Generates a consensus string for hashing based on a kyc register
std::string kycGenerateConsensusString(const std::vector<std::string>& vstr)
{
    return strprintf("%s|%s|%s|%s|%s",
		     vstr[0], vstr[1], vstr[2], vstr[3],
		     vstr[5]);
}

// Generates a consensus string for hashing based on a attestation register
std::string attGenerateConsensusString(const std::vector<std::string>& vstr)
{
    return strprintf("%s|%s|%d|%d",
		     vstr[0], vstr[1], vstr[2], vstr[4]);
}

// Generates a consensus string for hashing based on a cachefee (natives and oracles) register
std::string feeGenerateConsensusString(const uint32_t& propertyId, const int64_t& cache)
{
    return strprintf("%d|%d",propertyId, cache);
}

// Generates a consensus string for hashing based on features activation register
std::string GenerateConsensusString(const mastercore::FeatureActivation& feat)
{
    return strprintf("%d|%d|%d|%s", feat.featureId, feat.activationBlock, feat.minClientVersion, feat.featureName);
}

/**
* Obtains a hash of the active state to use for consensus verification and checkpointing.
*
* For increased flexibility, so other implementations like Trade Layer Wallet can
* also apply this methodology without necessarily using the same exact data types (which
* would be needed to hash the data bytes directly), create a string in the following
* format for each entry to use for hashing:
*
* ---STAGE 1 - BALANCES---
* Format specifiers & placeholders:
*   "%s|%d|%d|%d|%d|%d" - "address|propertyid|balance|selloffer_reserve|accept_reserve|metadex_reserve"
*
* Note: empty balance records and the pending tally are ignored. Addresses are sorted based
* on lexicographical order, and balance records are sorted by the property identifiers.
*
* ---STAGE 2 - DEX SELL OFFERS---
* Format specifiers & placeholders:
*   "%s|%s|%d|%d|%d|%d|%d" - "txid|address|propertyid|offeramount|btcdesired|minfee|timelimit"
*
* Note: ordered ascending by txid.
*
* ---STAGE 3 - DEX ACCEPTS---
* Format specifiers & placeholders:
*   "%s|%s|%d|%d|%d" - "matchedselloffertxid|buyer|acceptamount|acceptamountremaining|acceptblock"
*
* Note: ordered ascending by matchedselloffertxid followed by buyer.
*
* ---STAGE 4 - METADEX TRADES---
* Format specifiers & placeholders:
*   "%s|%s|%d|%d|%d|%d|%d" - "txid|address|propertyidforsale|amountforsale|propertyiddesired|amountdesired|amountremaining"
*
* Note: ordered ascending by txid.
*
* ---STAGE 5 - CONTRACTDEX TRADES---
* Format specifiers & placeholders:
*   "%s|%s|%d|%d|%d|%d|%d" - "txid|address|propertyidforsale|amountforsale|propertyiddesired|amountdesired|amountremaining"
*
* Note: ordered ascending by txid.
*
* ---STAGE 6 - CROWDSALES---
* Format specifiers & placeholders:
*   "%d|%d|%d|%d|%d" - "propertyid|propertyiddesired|deadline|usertokens|issuertokens"
*
* Note: ordered by property ID.
*
* ---STAGE 7 - PROPERTIES---
* Format specifiers & placeholders:
*   "%d|%s" - "propertyid|issueraddress"
*
* ---STAGE 8 - TRADE CHANNELS---
* Format specifiers & placeholders:
*   "%s|%s|%s|%s|%d|%d" - "multisigaddress|multisigaddress|firstaddress|secondaddress|expiryheight|lastexchangeblock"
*
* ---STAGE 9 - KYC LIST---
* Format specifiers & placeholders:
*   "%s|%s|%s|%d|%d" - "address|name|website|block|kycid"
*
* ---STAGE 10 - ATTESTATION LIST---
* Format specifiers & placeholders:
*   "%s|%s|%s|%s|%d|%d" - "multisigaddress|multisigaddress|firstaddress|secondaddress|expiryheight|lastexchangeblock"
*
* ---STAGE 11 - FEE CACHE NATIVES---
* Format specifiers & placeholders:
*   "%s|%s|%s|%s|%d|%d" - "multisigaddress|multisigaddress|firstaddress|secondaddress|expiryheight|lastexchangeblock"
*
* ---STAGE 12 - FEE CACHE ORACLES---
* Format specifiers & placeholders:
*   "%s|%s|%s|%s|%d|%d" - "multisigaddress|multisigaddress|firstaddress|secondaddress|expiryheight|lastexchangeblock"
*
* ---STAGE 12 - VESTING ADDRESSES---
* Format specifiers & placeholders:
*   "%s" - "address"
*
* ---STAGE 13 - FEATURES ACTIVATIONS---
* Format specifiers & placeholders:
*   "%d|%d|%d|%s" - "featureid|activationblock|minclientversion|featurename"
*
* Note: ordered by property ID.
*
* The byte order is important, and we assume:
*   SHA256("abc") = "ad1500f261ff10b49c7a1796a36103b02322ae5dde404141eacf018fbf1678ba"
*
*/

uint256 GetConsensusHash()
{
    // allocate and init a SHA256_CTX
    SHA256_CTX shaCtx;
    SHA256_Init(&shaCtx);

    LOCK(cs_tally);

    if (msc_debug_consensus_hash) PrintToLog("Beginning generation of current consensus hash...\n");

    // Balances - loop through the tally map, updating the sha context with the data from each balance and tally type
    // Placeholders:  "address|propertyid|balance|selloffer_reserve|accept_reserve|metadex_reserve"
    // Sort alphabetically first
    std::map<std::string, CMPTally> tallyMapSorted;
    for (std::unordered_map<string, CMPTally>::iterator uoit = mp_tally_map.begin(); uoit != mp_tally_map.end(); ++uoit)
    {
        tallyMapSorted.insert(std::make_pair(uoit->first,uoit->second));
    }

    for (std::map<string, CMPTally>::iterator my_it = tallyMapSorted.begin(); my_it != tallyMapSorted.end(); ++my_it)
    {
        const std::string& address = my_it->first;
        CMPTally& tally = my_it->second;
        tally.init();
        uint32_t propertyId = 0;
        while (0 != (propertyId = (tally.next())))
        {
            std::string dataStr = GenerateConsensusString(tally, address, propertyId);
            if (dataStr.empty()) continue; // skip empty balances
            if (msc_debug_consensus_hash) PrintToLog("Adding balance data to consensus hash: %s\n", dataStr);
            SHA256_Update(&shaCtx, dataStr.c_str(), dataStr.length());
        }
    }

    // DEx sell offers - loop through the DEx and add each sell offer to the consensus hash (ordered by txid)
    // Placeholders: "txid|address|propertyid|offeramount|btcdesired|minfee|timelimit"
    std::vector<std::pair<arith_uint256, std::string> > vecDExOffers;
    for (OfferMap::iterator it = my_offers.begin(); it != my_offers.end(); ++it)
    {
        const CMPOffer& selloffer = it->second;
        const std::string& sellCombo = it->first;
        std::string seller = sellCombo.substr(0, sellCombo.size() - 2);
        std::string dataStr = GenerateConsensusString(selloffer, seller);
        vecDExOffers.push_back(std::make_pair(arith_uint256(selloffer.getHash().ToString()), dataStr));
    }

    std::sort (vecDExOffers.begin(), vecDExOffers.end());
    for (std::vector<std::pair<arith_uint256, std::string> >::iterator it = vecDExOffers.begin(); it != vecDExOffers.end(); ++it)
    {
        const std::string& dataStr = it->second;
        if (msc_debug_consensus_hash) PrintToLog("Adding DEx offer data to consensus hash: %s\n", dataStr);
        SHA256_Update(&shaCtx, dataStr.c_str(), dataStr.length());
    }

    // DEx accepts - loop through the accepts map and add each accept to the consensus hash (ordered by matchedtxid then buyer)
    // Placeholders: "matchedselloffertxid|buyer|acceptamount|acceptamountremaining|acceptblock"
    std::vector<std::pair<std::string, std::string> > vecAccepts;
    for (AcceptMap::const_iterator it = my_accepts.begin(); it != my_accepts.end(); ++it)
    {
        const CMPAccept& accept = it->second;
        const std::string& acceptCombo = it->first;
        std::string buyer = acceptCombo.substr((acceptCombo.find("+") + 1), (acceptCombo.size()-(acceptCombo.find("+") + 1)));
        std::string dataStr = GenerateConsensusString(accept, buyer);
        std::string sortKey = strprintf("%s-%s", accept.getHash().GetHex(), buyer);
        vecAccepts.push_back(std::make_pair(sortKey, dataStr));
    }

    std::sort (vecAccepts.begin(), vecAccepts.end());
    for (std::vector<std::pair<std::string, std::string> >::iterator it = vecAccepts.begin(); it != vecAccepts.end(); ++it)
    {
        const std::string& dataStr = it->second;
        if (msc_debug_consensus_hash) PrintToLog("Adding DEx accept to consensus hash: %s\n", dataStr);
        SHA256_Update(&shaCtx, dataStr.c_str(), dataStr.length());
    }

    // MetaDEx trades - loop through the MetaDEx maps and add each open trade to the consensus hash (ordered by txid)
    // Placeholders: "txid|address|propertyidforsale|amountforsale|propertyiddesired|amountdesired|amountremaining"
    std::vector<std::pair<arith_uint256, std::string> > vecMetaDExTrades;
    for (md_PropertiesMap::const_iterator my_it = metadex.begin(); my_it != metadex.end(); ++my_it)
    {
        const md_PricesMap& prices = my_it->second;
        for (md_PricesMap::const_iterator it = prices.begin(); it != prices.end(); ++it)
        {
            const md_Set& indexes = it->second;
            for (md_Set::const_iterator it = indexes.begin(); it != indexes.end(); ++it)
            {
	              const CMPMetaDEx& obj = *it;
	              std::string dataStr = obj.GenerateConsensusString();
	              vecMetaDExTrades.push_back(std::make_pair(arith_uint256(obj.getHash().ToString()), dataStr));
            }
        }
    }

    std::sort (vecMetaDExTrades.begin(), vecMetaDExTrades.end());
    for (std::vector<std::pair<arith_uint256, std::string> >::iterator it = vecMetaDExTrades.begin(); it != vecMetaDExTrades.end(); ++it)
    {
        const std::string& dataStr = it->second;
        if (msc_debug_consensus_hash) PrintToLog("Adding MetaDEx trade data to consensus hash: %s\n", dataStr);
        SHA256_Update(&shaCtx, dataStr.c_str(), dataStr.length());
    }

    // ContractDex trades - loop through the ContractDex maps and add each open trade to the consensus hash (ordered by txid)
    // Placeholders: "txid|address|propertyidforsale|amountforsale|propertyiddesired|amountdesired|amountremaining|effectivePrice|tradignAction"
    std::vector<std::pair<arith_uint256, std::string> > vecContractDexTrades;
    for (cd_PropertiesMap::const_iterator my_it = contractdex.begin(); my_it != contractdex.end(); ++my_it)
    {
        const cd_PricesMap& prices = my_it->second;
        for (cd_PricesMap::const_iterator it = prices.begin(); it != prices.end(); ++it)
        {
            const cd_Set& indexes = it->second;
            for (cd_Set::const_iterator it = indexes.begin(); it != indexes.end(); ++it)
            {
	              const CMPContractDex& obj = *it;
	              std::string dataStr = obj.GenerateConsensusString();          /*-------the txid-------|---consensus string---*/
	              vecContractDexTrades.push_back(std::make_pair(arith_uint256(obj.getHash().ToString()), dataStr));
            }
        }
    }

    std::sort (vecContractDexTrades.begin(), vecContractDexTrades.end());
    for (std::vector<std::pair<arith_uint256, std::string> >::iterator it = vecContractDexTrades.begin(); it != vecContractDexTrades.end(); ++it)
    {
        const std::string& dataStr = it->second;
        if (msc_debug_consensus_hash) PrintToLog("Adding ContractDex trade data to consensus hash: %s\n", dataStr);
        SHA256_Update(&shaCtx, dataStr.c_str(), dataStr.length());
    }

    // Crowdsales - loop through open crowdsales and add to the consensus hash (ordered by property ID)
    // Note: the variables of the crowdsale (amount, bonus etc) are not part of the crowdsale map and not included here to
    // avoid additionalal loading of SP entries from the database
    // Placeholders: "propertyid|propertyiddesired|deadline|usertokens|issuertokens"
    std::vector<std::pair<uint32_t, std::string> > vecCrowds;
    for (CrowdMap::const_iterator it = my_crowds.begin(); it != my_crowds.end(); ++it)
    {
        const CMPCrowd& crowd = it->second;
        uint32_t propertyId = crowd.getPropertyId();
        std::string dataStr = GenerateConsensusString(crowd);
        vecCrowds.push_back(std::make_pair(propertyId, dataStr));
    }

    std::sort (vecCrowds.begin(), vecCrowds.end());
    for (std::vector<std::pair<uint32_t, std::string> >::iterator it = vecCrowds.begin(); it != vecCrowds.end(); ++it)
    {
        std::string dataStr = (*it).second;
        if (msc_debug_consensus_hash) PrintToLog("Adding Crowdsale entry to consensus hash: %s\n", dataStr);
        SHA256_Update(&shaCtx, dataStr.c_str(), dataStr.length());
    }

    // Properties - loop through each property and store the issuer (to capture state changes via change issuer transactions)
    // Note: we are loading every SP from the DB to check the issuer, if using consensus_hash_every_block debug option this
    //       will slow things down dramatically.  Not an issue to do it once every 10,000 blocks for checkpoint verification.
    // Placeholders: "propertyid|issueraddress"
    uint32_t startPropertyId = 1;
    for (uint32_t propertyId = startPropertyId; propertyId < _my_sps->peekNextSPID(); propertyId++)
    {
        CMPSPInfo::Entry sp;
        if (!_my_sps->getSP(propertyId, sp))
        {
	          PrintToLog("Error loading property ID %d for consensus hashing, hash should not be trusted!\n");
	          continue;
        }

        std::string dataStr = GenerateConsensusString(propertyId, sp.issuer);
        if (msc_debug_consensus_hash) PrintToLog("Adding property to consensus hash: %s\n", dataStr);
        SHA256_Update(&shaCtx, dataStr.c_str(), dataStr.length());
    }

    // Trade Channels
    // Placeholders: "multisigaddress|multisigaddress|firstaddress|secondaddress|expiryheight|lastexchangeblock"
    std::vector<std::pair<std::string, std::string> > vecChannels;
    for(auto it = channels_Map.begin(); it != channels_Map.end(); ++it)
    {
         const std::string& multisig = it->first;
         const channel& chn = it->second;
         std::string dataStr = GenerateConsensusString(chn);
         vecChannels.push_back(std::make_pair(multisig, dataStr));
    }
    // ordering  channels by multisig address
    std::sort (vecChannels.begin(), vecChannels.end());
    for (auto it = vecChannels.begin(); it != vecChannels.end(); ++it)
    {
        std::string dataStr = it->second;
        if (msc_debug_consensus_hash) PrintToLog("Adding Trade Channels entry to consensus hash: %s\n", dataStr);
        SHA256_Update(&shaCtx, dataStr.c_str(), dataStr.length());
    }

    //FIXME: big timeouts accessing the db !!!
    // KYC list
    // Placeholders: "address|name|website|block|kycid"
    // d_tradelistdb->kycConsensusHash(shaCtx);

    // Attestation list
    // Placeholders: "address|name|website|block|kycid"
    // d_tradelistdb->attConsensusHash(shaCtx);

    // Cache fee (natives)
    // Placeholders: "propertyid|cacheamount"
    for (auto itt = cachefees.begin(); itt != cachefees.end(); ++itt)
    {
        // decompose the key for address
        const uint32_t& propertyId = itt->first;
        const int64_t& cache = itt->second;
        std::string dataStr = feeGenerateConsensusString(propertyId, cache);
        if (msc_debug_consensus_hash) PrintToLog("Adding Fee cache (natives) entry to consensus hash: %s\n", dataStr);
        SHA256_Update(&shaCtx, dataStr.c_str(), dataStr.length());
    }

    // Cache fee (oracles)
    // Placeholders: "propertyid|cacheamount"
    for (auto itt = cachefees_oracles.begin(); itt != cachefees_oracles.end(); ++itt)
    {
        // decompose the key for address
        const uint32_t& propertyId = itt->first;
        const int64_t& cache = itt->second;
        std::string dataStr = feeGenerateConsensusString(propertyId, cache);
        if (msc_debug_consensus_hash) PrintToLog("Adding Fee cache (oracles) entry to consensus hash: %s\n", dataStr);
        SHA256_Update(&shaCtx, dataStr.c_str(), dataStr.length());
    }

    // Vesting addresses
    // Placeholders: "addresses"
    std::sort (vestingAddresses.begin(), vestingAddresses.end());
    for (auto it = vestingAddresses.begin(); it != vestingAddresses.end(); ++it)
    {
        // decompose the key for address
        const std::string& address = *it;
        std::string dataStr = strprintf("%s", address);
        if (msc_debug_consensus_hash) PrintToLog("Adding Vesting addresses entry to consensus hash: %s\n", dataStr);
        SHA256_Update(&shaCtx, dataStr.c_str(), dataStr.length());
    }

    // Pending Activations
    // Placeholders: "featureid|activationblock|minclientversion|featurename"
    std::vector<mastercore::FeatureActivation> pendings = mastercore::GetPendingActivations();
    std::vector<std::pair<uint16_t, std::string> > sortPendings;

    for(auto it = pendings.begin(); it != pendings.end(); ++it)
    {
         const mastercore::FeatureActivation& feat = *it;
         std::string dataStr = GenerateConsensusString(feat);
         sortPendings.push_back(std::make_pair(feat.featureId, dataStr));
    }

    // sorting using featureId
    std::sort(sortPendings.begin(), sortPendings.end());

    for(auto itt = sortPendings.begin(); itt != sortPendings.end(); ++itt)
    {
        std::string dataStr = itt->second;
        if (msc_debug_consensus_hash) PrintToLog("Adding Pensing features activations entry to consensus hash: %s\n", dataStr);
        SHA256_Update(&shaCtx, dataStr.c_str(), dataStr.length());
    }

    // Completed Activations
    // Placeholders: "featureid|activationblock|minclientversion|featurename"
    std::vector<mastercore::FeatureActivation> completed = mastercore::GetCompletedActivations();
    std::vector<std::pair<uint16_t, std::string> > sortCompleted;

    for(auto it = completed.begin(); it != completed.end(); ++it)
    {
         const mastercore::FeatureActivation& feat = *it;
         std::string dataStr = GenerateConsensusString(feat);
         sortCompleted.push_back(std::make_pair(feat.featureId, dataStr));
    }

    // sorting using featureId
    std::sort(sortCompleted.begin(), sortCompleted.end());

    for(auto itt = sortCompleted.begin(); itt != sortCompleted.end(); ++itt)
    {
        std::string dataStr = itt->second;
        if (msc_debug_consensus_hash) PrintToLog("Adding Completed features activations entry to consensus hash: %s\n", dataStr);
        SHA256_Update(&shaCtx, dataStr.c_str(), dataStr.length());
    }

    // extract the final result and return the hash
    uint256 consensusHash;
    SHA256_Final((unsigned char*)&consensusHash, &shaCtx);
    if (msc_debug_consensus_hash) PrintToLog("Finished generation of consensus hash.  Result: %s\n", consensusHash.GetHex());

    return consensusHash;
}

uint256 GetMetaDExHash(const uint32_t propertyId)
{
    SHA256_CTX shaCtx;
    SHA256_Init(&shaCtx);

    LOCK(cs_tally);

    std::vector<std::pair<arith_uint256, std::string> > vecMetaDExTrades;
    for (md_PropertiesMap::const_iterator my_it = metadex.begin(); my_it != metadex.end(); ++my_it)
    {
        if (propertyId == 0 || propertyId == my_it->first)
        {
            const md_PricesMap& prices = my_it->second;
            for (md_PricesMap::const_iterator it = prices.begin(); it != prices.end(); ++it)
            {
                const md_Set& indexes = it->second;
                for (md_Set::const_iterator it = indexes.begin(); it != indexes.end(); ++it)
                {
                    const CMPMetaDEx& obj = *it;
                    std::string dataStr = obj.GenerateConsensusString();
                    vecMetaDExTrades.push_back(std::make_pair(arith_uint256(obj.getHash().ToString()), dataStr));
                }
            }
        }
    }

    std::sort (vecMetaDExTrades.begin(), vecMetaDExTrades.end());
    for (std::vector<std::pair<arith_uint256, std::string> >::iterator it = vecMetaDExTrades.begin(); it != vecMetaDExTrades.end(); ++it)
    {
        const std::string& dataStr = it->second;
        SHA256_Update(&shaCtx, dataStr.c_str(), dataStr.length());
    }

    uint256 metadexHash;
    SHA256_Final((unsigned char*)&metadexHash, &shaCtx);

    return metadexHash;
}

/** Obtains a hash of the balances for a specific property. */
uint256 GetBalancesHash(const uint32_t hashPropertyId)
{
    SHA256_CTX shaCtx;
    SHA256_Init(&shaCtx);

    LOCK(cs_tally);

    std::map<std::string, CMPTally> tallyMapSorted;
    for (std::unordered_map<string, CMPTally>::iterator uoit = mp_tally_map.begin(); uoit != mp_tally_map.end(); ++uoit)
    {
        tallyMapSorted.insert(std::make_pair(uoit->first,uoit->second));
    }

    for (std::map<string, CMPTally>::iterator my_it = tallyMapSorted.begin(); my_it != tallyMapSorted.end(); ++my_it)
    {
        const std::string& address = my_it->first;
        CMPTally& tally = my_it->second;
        tally.init();
        uint32_t propertyId = 0;
        while (0 != (propertyId = (tally.next())))
        {
            if (propertyId != hashPropertyId) continue;
            std::string dataStr = GenerateConsensusString(tally, address, propertyId);
            if (dataStr.empty()) continue;
            if (msc_debug_consensus_hash) PrintToLog("Adding data to balances hash: %s\n", dataStr);
            SHA256_Update(&shaCtx, dataStr.c_str(), dataStr.length());
        }
    }

    uint256 balancesHash;
    SHA256_Final((unsigned char*)&balancesHash, &shaCtx);

    return balancesHash;
}

} // namespace mastercore
