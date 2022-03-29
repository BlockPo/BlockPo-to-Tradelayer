/**
 * @file consensushash.cpp
 *
 * This file contains the function to generate consensus hashes.
 */

#include <tradelayer/consensushash.h>

#include <tradelayer/activation.h>
#include <tradelayer/dex.h>
#include <tradelayer/log.h>
#include <tradelayer/mdex.h>
#include <tradelayer/parse_string.h>
#include <tradelayer/persistence.h>
#include <tradelayer/register.h>
#include <tradelayer/sp.h>
#include <tradelayer/fees.h>
#include <tradelayer/tradelayer.h>
#include <tradelayer/tradelayer_matrices.h>

#include <arith_uint256.h>
#include <uint256.h>

#include <algorithm>
#include <stdint.h>
#include <string>
#include <vector>

namespace mastercore
{


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

// Generates a consensus string for hashing based on a contract register
std::string GenerateConsensusString(const Register& reg, const std::string& address, const uint32_t contractId)
{
    const int64_t entryPrice = reg.getRecord(contractId, ENTRY_CPRICE);
    const int64_t position = reg.getRecord(contractId, CONTRACT_POSITION);
    const int64_t liquidationPrice = reg.getRecord(contractId, BANKRUPTCY_PRICE);
    const int64_t upnl = reg.getRecord(contractId, UPNL);
    const int64_t margin = reg.getRecord(contractId, MARGIN);
    const int64_t leverage = reg.getRecord(contractId, LEVERAGE);

    // return a blank string if all balances are empty
    if (0 == entryPrice && 0 == position && 0 == liquidationPrice && 0 == upnl && 0 == margin && leverage == 0) {
        return "";
    }

    return strprintf("%d:%d,%d,%d,%d,%d,%d", contractId, entryPrice, position, liquidationPrice, upnl, margin, leverage);
}

// Generates a consensus string for hashing based on a tally object
std::string GenerateConsensusString(const CMPTally& tallyObj, const std::string& address, const uint32_t propertyId)
{
    const int64_t balance = tallyObj.getMoney(propertyId, BALANCE);
    const int64_t sellOfferReserve = tallyObj.getMoney(propertyId, SELLOFFER_RESERVE);
    const int64_t acceptReserve = tallyObj.getMoney(propertyId, ACCEPT_RESERVE);
    const int64_t metaDExReserve = tallyObj.getMoney(propertyId, METADEX_RESERVE);
    const int64_t contractdexReserved = tallyObj.getMoney(propertyId, CONTRACTDEX_RESERVE);
    const int64_t unvested = tallyObj.getMoney(propertyId, UNVESTED);
    const int64_t pending = tallyObj.getMoney(propertyId, PENDING);

    // return a blank string if all balances are empty
    if (!balance && !sellOfferReserve && !acceptReserve && !metaDExReserve && !contractdexReserved && !unvested && !pending) {
      return "";
    }

    return strprintf("%s|%d|%d|%d|%d|%d|%d|%d|%d",  address, propertyId, balance, sellOfferReserve, acceptReserve, pending, metaDExReserve, contractdexReserved, unvested);
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

// Generates a consensus string for hashing based on a property/ contract issuer
std::string GenerateConsensusString(const uint32_t propertyId, const std::string& address)
{
    return strprintf("%d|%s", propertyId, address);
}

// Generates a consensus string for hashing based on a channel object
std::string GenerateConsensusString(const Channel& chn)
{
    return strprintf("%s|%s|%s|%d",
		     chn.getMultisig(), chn.getFirst(), chn.getSecond(),
		     chn.getLastBlock());
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
    const std::string status = (feat.status) ? "completed" : "pending";
    return strprintf("%d|%d|%d|%s|%s", feat.featureId, feat.activationBlock, feat.minClientVersion, feat.featureName, status);
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
* ---STAGE 2 - CONTRACT REGISTERS---
* Format specifiers & placeholders:
*   "%s|%d|%d|%d|%d|%d|%d" - "address|contractid|entry_price|position|BANKRUPTCY_PRICE|upnl|margin|leverage"
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
* ---STAGE 6 - PROPERTIES---
* Format specifiers & placeholders:
*   "%d|%s" - "propertyid|issueraddress"
*
* ---STAGE 7 - CONTRACTS---
* Format specifiers & placeholders:
*   "%d|%s" - "contractid|adminaddress"
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
*   "%d|%d" - "propertyId|amountaccumulated"
*
* ---STAGE 12 - FEE CACHE ORACLES---
* Format specifiers & placeholders:
*   "%d|%d" - "propertyId|amountaccumulated"
*
* ---STAGE 13 - VESTING ADDRESSES---
* Format specifiers & placeholders:
*   "%s" - "address"
*
* ---STAGE 14 - FEATURES ACTIVATIONS---
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
    CSHA256 hasher;

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
            hasher.Write((unsigned char*)dataStr.c_str(), dataStr.length());
        }
    }


    std::map<std::string, Register> registerMapSorted;
    for (std::unordered_map<string, Register>::iterator uit = mp_register_map.begin(); uit != mp_register_map.end(); ++uit)
    {
        registerMapSorted.insert(std::make_pair(uit->first,uit->second));
    }

    for (std::map<string, Register>::iterator my_it = registerMapSorted.begin(); my_it != registerMapSorted.end(); ++my_it)
    {
        const std::string& address = my_it->first;
        Register& reg = my_it->second;
        reg.init();
        uint32_t contractId = 0;
        while (0 != (contractId = (reg.next())))
        {
            std::string dataStr = GenerateConsensusString(reg, address, contractId);
            if (dataStr.empty()) continue; // skip empty balances
            if (msc_debug_consensus_hash) PrintToLog("Adding contract register data to consensus hash: %s\n", dataStr);
            hasher.Write((unsigned char*)dataStr.c_str(), dataStr.length());
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
        hasher.Write((unsigned char*)dataStr.c_str(), dataStr.length());
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
        hasher.Write((unsigned char*)dataStr.c_str(), dataStr.length());
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
         hasher.Write((unsigned char*)dataStr.c_str(), dataStr.length());
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
       hasher.Write((unsigned char*)dataStr.c_str(), dataStr.length());
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
        hasher.Write((unsigned char*)dataStr.c_str(), dataStr.length());
    }

    // Contracts
    // Placeholders: "contractid|issueraddress"
    uint32_t startContractId = 1;
    for (uint32_t contractId = startContractId; contractId < _my_cds->peekNextContractID(); contractId++)
    {
        CDInfo::Entry cd;
        if (!_my_cds->getCD(contractId, cd))
        {
	          PrintToLog("Error loading contract ID %d for consensus hashing, hash should not be trusted!\n");
	          continue;
        }

        std::string dataStr = GenerateConsensusString(contractId, cd.issuer);
        if (msc_debug_consensus_hash) PrintToLog("Adding property to consensus hash: %s\n", dataStr);
        hasher.Write((unsigned char*)dataStr.c_str(), dataStr.length());
    }

    // Trade Channels
    // Placeholders: "multisigaddress|multisigaddress|firstaddress|secondaddress|lastexchangeblock"
    for(const auto& cm : channels_Map)
    {
         const Channel& chn = cm.second;
         const std::string dataStr = GenerateConsensusString(chn);
         if (msc_debug_consensus_hash) PrintToLog("Adding Trade Channels entry to consensus hash: %s\n", dataStr);
         //channels oredered by multisig address (map key)
         hasher.Write((unsigned char*)dataStr.c_str(), dataStr.length());

    }

    // KYC list
    // Placeholders: "address|name|website|block|kycid"
    //NOTE: Possible optimization: different batch for every kind of register in db
    t_tradelistdb->kycConsensusHash(hasher);

    // Attestation list
    // Placeholders: "address|name|website|block|kycid"
    t_tradelistdb->attConsensusHash(hasher);
    
    // Cache fee (natives)
    // Placeholders: "propertyid|cacheamount"
    for (const auto& cf : g_fees->native_fees)
    {
        const uint32_t& propertyId = cf.first;
        const int64_t& cache = cf.second;
        std::string dataStr = feeGenerateConsensusString(propertyId, cache);
        if (msc_debug_consensus_hash) PrintToLog("Adding Fee cache (natives) entry to consensus hash: %s\n", dataStr);
        // cache fee is a map (ordered by propertyId key)
        hasher.Write((unsigned char*)dataStr.c_str(), dataStr.length());
    }

    // Cache fee (oracles)
    // Placeholders: "propertyid|cacheamount"
    for (const auto& cf : g_fees->oracle_fees)
    {
        const uint32_t& propertyId = cf.first;
        const int64_t& cache = cf.second;
        std::string dataStr = feeGenerateConsensusString(propertyId, cache);
        if (msc_debug_consensus_hash) PrintToLog("Adding Fee cache (oracles) entry to consensus hash: %s\n", dataStr);
        // map ordered by propertyId key
        hasher.Write((unsigned char*)dataStr.c_str(), dataStr.length());
    }
    
    // Cache fee (spot)
    // Placeholders: "propertyid|cacheamount"
    for (const auto& cf : g_fees->spot_fees)
    {
        const uint32_t& propertyId = cf.first;
        const int64_t& cache = cf.second;
        std::string dataStr = feeGenerateConsensusString(propertyId, cache);
        if (msc_debug_consensus_hash) PrintToLog("Adding Fee cache (spot) entry to consensus hash: %s\n", dataStr);
        // cache fee is a map (ordered by propertyId key)
        hasher.Write((unsigned char*)dataStr.c_str(), dataStr.length());
    }

    // Vesting addresses
    // Placeholders: "addresses"
    // std::sort (vestingAddresses.begin(), vestingAddresses.end());
    for (const auto& v : vestingAddresses)
    {
        const std::string& address = v;
        std::string dataStr = strprintf("%s", address);
        if (msc_debug_consensus_hash) PrintToLog("Adding Vesting addresses entry to consensus hash: %s\n", dataStr);
        hasher.Write((unsigned char*)dataStr.c_str(), dataStr.length());
    }

    // Pending Activations
    // Placeholders: "featureid|activationblock|minclientversion|featurename"
    std::vector<mastercore::FeatureActivation> pendings = mastercore::GetPendingActivations();
    std::vector<std::pair<uint16_t, std::string> > sortPendings;

    for(const auto& p : pendings)
    {
         const mastercore::FeatureActivation& feat = p;
         std::string dataStr = GenerateConsensusString(feat);
         sortPendings.push_back(std::make_pair(feat.featureId, dataStr));
    }

    // sorting using featureId
    std::sort(sortPendings.begin(), sortPendings.end());
    for(const auto& sp : sortPendings)
    {
        const std::string& dataStr = sp.second;
        if (msc_debug_consensus_hash) PrintToLog("Adding Pending features activations entry to consensus hash: %s\n", dataStr);
        hasher.Write((unsigned char*)dataStr.c_str(), dataStr.length());
    }

    // Completed Activations
    // Placeholders: "featureid|activationblock|minclientversion|featurename"
    std::vector<mastercore::FeatureActivation> completed = mastercore::GetCompletedActivations();
    std::vector<std::pair<uint16_t, std::string> > sortCompleted;

    for(const auto& cp : completed)
    {
         const mastercore::FeatureActivation& feat = cp;
         std::string dataStr = GenerateConsensusString(feat);
         sortCompleted.push_back(std::make_pair(feat.featureId, dataStr));
    }

    // sorting using featureId
    std::sort(sortCompleted.begin(), sortCompleted.end());
    for(const auto& cp : sortCompleted)
    {
        std::string dataStr = cp.second;
        if (msc_debug_consensus_hash) PrintToLog("Adding Completed features activations entry to consensus hash: %s\n", dataStr);
        hasher.Write((unsigned char*)dataStr.c_str(), dataStr.length());
    }

    // extract the final result and return the hash
    uint256 consensusHash;
    hasher.Finalize(consensusHash.begin());
    if (msc_debug_consensus_hash) PrintToLog("Finished generation of consensus hash.  Result: %s\n", consensusHash.GetHex());

    return consensusHash;
}

uint256 GetMetaDExHash(const uint32_t propertyId)
{
    CSHA256 hasher;

    LOCK(cs_tally);

    std::vector<std::pair<arith_uint256, std::string> > vecMetaDExTrades;
    for (md_PropertiesMap::const_iterator my_it = metadex.begin(); my_it != metadex.end(); ++my_it) {
        if (propertyId == 0 || propertyId == my_it->first) {
            const md_PricesMap& prices = my_it->second;
            for (md_PricesMap::const_iterator it = prices.begin(); it != prices.end(); ++it) {
                const md_Set& indexes = it->second;
                for (md_Set::const_iterator it = indexes.begin(); it != indexes.end(); ++it) {
                    const CMPMetaDEx& obj = *it;
                    const std::string dataStr = obj.GenerateConsensusString();
                    vecMetaDExTrades.push_back(std::make_pair(arith_uint256(obj.getHash().ToString()), dataStr));
                }
            }
        }
    }
    std::sort (vecMetaDExTrades.begin(), vecMetaDExTrades.end());
    for (std::vector<std::pair<arith_uint256, std::string> >::iterator it = vecMetaDExTrades.begin(); it != vecMetaDExTrades.end(); ++it)
    {
        const std::string& dataStr = it->second;
        hasher.Write((unsigned char*)dataStr.c_str(), dataStr.length());
    }

    uint256 metadexHash;
    hasher.Finalize(metadexHash.begin());

    return metadexHash;
}

/** Obtains a hash of the balances for a specific property. */
uint256 GetBalancesHash(const uint32_t hashPropertyId)
{
    CSHA256 hasher;

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
            hasher.Write((unsigned char*)dataStr.c_str(), dataStr.length());
        }
    }

    uint256 balancesHash;
    hasher.Finalize(balancesHash.begin());

    return balancesHash;
}

} // namespace mastercore
