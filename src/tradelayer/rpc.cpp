/**
 * @file rpc.cpp
 *
 * This file contains RPC calls for data retrieval.
 */

#include <tradelayer/rpc.h>
#include <tradelayer/activation.h>
#include <tradelayer/ce.h>
#include <tradelayer/consensushash.h>
#include <tradelayer/convert.h>
#include <tradelayer/dex.h>
#include <tradelayer/errors.h>
#include <tradelayer/externfns.h>
#include <tradelayer/fetchwallettx.h>
#include <tradelayer/log.h>
#include <tradelayer/mdex.h>
#include <tradelayer/notifications.h>
#include <tradelayer/parse_string.h>
#include <tradelayer/register.h>
#include <tradelayer/rpcrequirements.h>
#include <tradelayer/rpctx.h>
#include <tradelayer/rpctxobject.h>
#include <tradelayer/rpcvalues.h>
#include <tradelayer/rules.h>
#include <tradelayer/sp.h>
#include <tradelayer/tally.h>
#include <tradelayer/tradelayer.h>
#include <tradelayer/tx.h>
#include <tradelayer/uint256_extensions.h>
#include <tradelayer/utilsbitcoin.h>
#include <tradelayer/varint.h>
#include <tradelayer/version.h>
#include <tradelayer/wallettxs.h>

#include <amount.h>
#include <arith_uint256.h>
#include <chainparams.h>
#include <init.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <rpc/safemode.h>
#include <rpc/server.h>
#include <tinyformat.h>
#include <txmempool.h>
#include <uint256.h>
#include <util/strencodings.h>
#include <validation.h>
#ifdef ENABLE_WALLET
#include <wallet/wallet.h>
#endif

#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <univalue.h>

using std::runtime_error;
using namespace mastercore;

using mastercore::StrToInt64;
using mastercore::DoubleToInt64;

/**
 * Throws a JSONRPCError, depending on error code.
 */
void PopulateFailure(int error)
{
    switch (error) {
        case MP_TX_NOT_FOUND:
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available about transaction");
        case MP_TX_UNCONFIRMED:
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unconfirmed transactions are not supported");
        case MP_BLOCK_NOT_IN_CHAIN:
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Transaction not part of the active chain");
        case MP_INVALID_TX_IN_DB_FOUND:
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Potential database corruption: Invalid transaction found");
        case MP_TX_IS_NOT_MASTER_PROTOCOL:
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Not a Trade Layer Protocol transaction");
    }
    throw JSONRPCError(RPC_INTERNAL_ERROR, "Generic transaction population failure");
}

void PropertyToJSON(const CMPSPInfo::Entry& sProperty, UniValue& property_obj)
{
    property_obj.pushKV("name", sProperty.name);
    property_obj.pushKV("data", sProperty.data);
    property_obj.pushKV("url", sProperty.url);
    property_obj.pushKV("divisible", sProperty.isDivisible());
    property_obj.pushKV("category", sProperty.category);
    property_obj.pushKV("subcategory", sProperty.subcategory);

}

void ContractToJSON(const CDInfo::Entry& sProperty, UniValue& property_obj)
{

    property_obj.pushKV("name", sProperty.name);
    property_obj.pushKV("data", sProperty.data);
    property_obj.pushKV("url", sProperty.url);
    property_obj.pushKV("admin", sProperty.issuer);
    property_obj.pushKV("creationtxid", sProperty.txid.GetHex());
    property_obj.pushKV("creation block", sProperty.init_block);
    property_obj.pushKV("notional size", FormatDivisibleShortMP(sProperty.notional_size));
    property_obj.pushKV("collateral currency", std::to_string(sProperty.collateral_currency));
    property_obj.pushKV("margin requirement", FormatDivisibleShortMP(sProperty.margin_requirement));
    property_obj.pushKV("blocks until expiration", std::to_string(sProperty.blocks_until_expiration));
    property_obj.pushKV("inverse quoted", std::to_string(sProperty.inverse_quoted));
    if (sProperty.isOracle())
    {
        property_obj.pushKV("backup address", sProperty.backup_address);
        property_obj.pushKV("hight price", FormatDivisibleShortMP(sProperty.oracle_high));
        property_obj.pushKV("low price", FormatDivisibleShortMP(sProperty.oracle_low));
        property_obj.pushKV("last close price", FormatDivisibleShortMP(sProperty.oracle_close));
        property_obj.pushKV("type", "oracle");

    } else if (sProperty.isNative()){
        property_obj.pushKV("type", "native");

    }

}

void KYCToJSON(const CMPSPInfo::Entry& sProperty, UniValue& property_obj)
{
    std::vector<int64_t> iKyc = sProperty.kyc;
    std::string sKyc = "";

    for(auto it = iKyc.begin(); it != iKyc.end(); ++it)
    {
        sKyc += to_string(*it);
        if (it != std::prev(iKyc.end())) sKyc += ",";
    }

    property_obj.pushKV("kyc_ids allowed","["+sKyc+"]");

}


void KYCToJSON(const CDInfo::Entry& sProperty, UniValue& property_obj)
{
    std::vector<int64_t> iKyc = sProperty.kyc;
    std::string sKyc = "";

    for(auto it = iKyc.begin(); it != iKyc.end(); ++it)
    {
        sKyc += to_string(*it);
        if (it != std::prev(iKyc.end())) sKyc += ",";
    }

    property_obj.pushKV("kyc_ids allowed","["+sKyc+"]");

}


void OracleToJSON(const CDInfo::Entry& sProperty, UniValue& property_obj)
{
    property_obj.pushKV("name", sProperty.name);
    property_obj.pushKV("data", sProperty.data);
    property_obj.pushKV("url", sProperty.url);
    property_obj.pushKV("issuer", sProperty.issuer);
    property_obj.pushKV("creationtxid", sProperty.txid.GetHex());
    property_obj.pushKV("creation block", sProperty.init_block);
    property_obj.pushKV("notional size", FormatDivisibleShortMP(sProperty.notional_size));
    property_obj.pushKV("collateral currency", std::to_string(sProperty.collateral_currency));
    property_obj.pushKV("margin requirement", FormatDivisibleShortMP(sProperty.margin_requirement));
    property_obj.pushKV("blocks until expiration", std::to_string(sProperty.blocks_until_expiration));
    property_obj.pushKV("last high price", FormatDivisibleShortMP(sProperty.oracle_high));
    property_obj.pushKV("last low price", FormatDivisibleShortMP(sProperty.oracle_low));
    property_obj.pushKV("last close price",FormatDivisibleShortMP(sProperty.oracle_close));
}

void VestingToJSON(const CMPSPInfo::Entry& sProperty, UniValue& property_obj)
{
    property_obj.pushKV("name", sProperty.name);
    property_obj.pushKV("data", sProperty.data);
    property_obj.pushKV("url", sProperty.url);
    property_obj.pushKV("divisible", sProperty.isDivisible());
    property_obj.pushKV("issuer", sProperty.issuer);
    property_obj.pushKV("activation block", sProperty.init_block);

    const int64_t xglobal = globalVolumeALL_LTC;
    const double accum = (RegTest()) ? getAccumVesting(100 * xglobal) : getAccumVesting(xglobal);
    const int64_t vestedPer = 100 * mastercore::DoubleToInt64(accum);

    property_obj.pushKV("litecoin volume",  FormatDivisibleMP(xglobal));
    property_obj.pushKV("vested percentage",  FormatDivisibleMP(vestedPer));
    property_obj.pushKV("last vesting block",  sProperty.last_vesting_block);

    int64_t totalMinted = getTotalTokens(ALL);
    if (RegTest()) totalMinted -= sProperty.num_tokens;

    property_obj.pushKV("total ALL minted",  FormatDivisibleMP(totalMinted));

    const int64_t remaining = getMPbalance(getVestingAdmin(), TL_PROPERTY_VESTING, BALANCE);
    const int64_t vested =  totalVesting - remaining;

    property_obj.pushKV("total vested",  FormatDivisibleMP(vested));

    size_t n_owners_total = vestingAddresses.size();
    property_obj.pushKV("owners",  (int64_t) n_owners_total);
    property_obj.pushKV("total tokens", FormatDivisibleMP(sProperty.num_tokens));

    int block = GetHeight();

    const CConsensusParams &params = ConsensusParams();

    int period = sProperty.init_block + params.ONE_YEAR;

    bool year_ended  = (block > period) ? true : false;

    property_obj.pushKV("first yeard ended", year_ended ? "true" : "false");



}

bool BalanceToJSON(const std::string& address, uint32_t property, UniValue& balance_obj, bool divisible)
{
    // confirmed balance minus unconfirmed, spent amounts
    int64_t nAvailable = getUserAvailableMPbalance(address, property);
    int64_t nReserve = getUserReserveMPbalance(address, property);

    if (divisible) {
        balance_obj.pushKV("balance", FormatDivisibleMP(nAvailable));
        balance_obj.pushKV("reserve", FormatDivisibleMP(nReserve));
    } else {
        balance_obj.pushKV("balance", FormatIndivisibleMP(nAvailable));
        balance_obj.pushKV("reserve", FormatIndivisibleMP(nReserve));
    }

    if (nAvailable == 0) {
        return false;
    }

    return true;
}

void ReserveToJSON(const std::string& address, uint32_t contractId, UniValue& balance_obj)
{
    int64_t margin = getContractRecord(address, contractId, MARGIN);
    balance_obj.pushKV("reserve", FormatDivisibleMP(margin));
}

void UnvestedToJSON(const std::string& address, uint32_t property, UniValue& balance_obj, bool divisible)
{
    const int64_t unvested = getMPbalance(address, property, UNVESTED);
    if (divisible) {
        balance_obj.pushKV("unvested", FormatDivisibleMP(unvested));
    } else {
        balance_obj.pushKV("unvested", FormatIndivisibleMP(unvested));
    }
}

void ChannelToJSON(const std::string& address, uint32_t property, UniValue& balance_obj, bool divisible)
{
    int64_t remaining = 0;
    auto it = channels_Map.find(address);
    if (it != channels_Map.end()){
        const Channel& sChn = it->second;
        remaining = sChn.getRemaining(false, property);
        remaining += sChn.getRemaining(true, property);
    }

    if (divisible) {
        balance_obj.pushKV("channel reserve", FormatDivisibleMP(remaining));
    } else {
        balance_obj.pushKV("channel reserve", FormatIndivisibleMP(remaining));
    }
}

void MetaDexObjectToJSON(const CMPMetaDEx& obj, UniValue& metadex_obj)
{
    bool propertyIdForSaleIsDivisible = isPropertyDivisible(obj.getProperty());
    bool propertyIdDesiredIsDivisible = isPropertyDivisible(obj.getDesProperty());

    // add data to JSON object
    metadex_obj.pushKV("address", obj.getAddr());
    metadex_obj.pushKV("txid", obj.getHash().GetHex());
    metadex_obj.pushKV("propertyidforsale", (uint64_t) obj.getProperty());
    metadex_obj.pushKV("propertyidforsaleisdivisible", propertyIdForSaleIsDivisible);
    metadex_obj.pushKV("amountforsale", FormatMP(obj.getProperty(), obj.getAmountForSale()));
    metadex_obj.pushKV("amountremaining", FormatMP(obj.getProperty(), obj.getAmountRemaining()));
    metadex_obj.pushKV("propertyiddesired", (uint64_t) obj.getDesProperty());
    metadex_obj.pushKV("propertyiddesiredisdivisible", propertyIdDesiredIsDivisible);
    metadex_obj.pushKV("amountdesired", FormatMP(obj.getDesProperty(), obj.getAmountDesired()));
    metadex_obj.pushKV("amounttofill", FormatMP(obj.getDesProperty(), obj.getAmountToFill()));
    metadex_obj.pushKV("action", (int) obj.getAction());
    metadex_obj.pushKV("block", obj.getBlock());
    metadex_obj.pushKV("blocktime", obj.getBlockTime());
}

void ContractDexObjectToJSON(const CMPContractDex& obj, UniValue& contractdex_obj)
{

    // add data to JSON object
    contractdex_obj.pushKV("address", obj.getAddr());
    contractdex_obj.pushKV("txid", obj.getHash().GetHex());
    contractdex_obj.pushKV("contractid", (uint64_t) obj.getProperty());
    contractdex_obj.pushKV("amountforsale", obj.getAmountForSale());
    contractdex_obj.pushKV("tradingaction", obj.getTradingAction());
    contractdex_obj.pushKV("effectiveprice",  FormatMP(1,obj.getEffectivePrice()));
    contractdex_obj.pushKV("block", obj.getBlock());
    contractdex_obj.pushKV("idx", static_cast<uint64_t>(obj.getIdx()));
    contractdex_obj.pushKV("blocktime", obj.getBlockTime());
}

void MetaDexObjectsToJSON(std::vector<CMPMetaDEx>& vMetaDexObjs, UniValue& response)
{
    MetaDEx_compare compareByHeight;

    // sorts metadex objects based on block height and position in block
    std::sort (vMetaDexObjs.begin(), vMetaDexObjs.end(), compareByHeight);

    for (std::vector<CMPMetaDEx>::const_iterator it = vMetaDexObjs.begin(); it != vMetaDexObjs.end(); ++it) {
        UniValue metadex_obj(UniValue::VOBJ);
        MetaDexObjectToJSON(*it, metadex_obj);

        response.push_back(metadex_obj);
    }
}

void ContractDexObjectsToJSON(std::vector<CMPContractDex>& vContractDexObjs, UniValue& response)
{
    ContractDex_compare compareByHeight;

    // sorts metadex objects based on block height and position in block
    std::sort (vContractDexObjs.begin(), vContractDexObjs.end(), compareByHeight);

    for (std::vector<CMPContractDex>::const_iterator it = vContractDexObjs.begin(); it != vContractDexObjs.end(); ++it) {
        UniValue contractdex_obj(UniValue::VOBJ);
        ContractDexObjectToJSON(*it, contractdex_obj);

        response.push_back(contractdex_obj);
    }
}

// obtain the payload for a transaction
UniValue tl_getpayload(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "tl_getpayload \"txid\"\n"

            "\nGet the payload for an Trade Layer transaction.\n"

            "\nArguments:\n"
            "1. txid                 (string, required) the hash of the transaction to retrieve payload\n"

            "\nResult:\n"
            "{\n"
            "  \"payload\" : \"payloadmessage\",       (string) the decoded Trade Layer payload message\n"
            "  \"payloadsize\" : n                     (number) the size of the payload\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_getpayload", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
            + HelpExampleRpc("tl_getpayload", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
        );

    uint256 txid = ParseHashV(request.params[0], "txid");

    CTransactionRef tx;
    uint256 blockHash;
    if (!GetTransaction(txid, tx, Params().GetConsensus(), blockHash, true)) {
        PopulateFailure(MP_TX_NOT_FOUND);
    }

    int blockTime = 0;
    int blockHeight = GetHeight();
    if (!blockHash.IsNull()) {
        CBlockIndex* pBlockIndex = GetBlockIndex(blockHash);
        if (nullptr != pBlockIndex) {
            blockTime = pBlockIndex->nTime;
            blockHeight = pBlockIndex->nHeight;
        }
    }

    CMPTransaction mp_obj;
    int parseRC = ParseTransaction(*(tx), blockHeight, 0, mp_obj, blockTime);
    if (parseRC < 0) PopulateFailure(MP_TX_IS_NOT_MASTER_PROTOCOL);

    UniValue payloadObj(UniValue::VOBJ);
    payloadObj.pushKV("payload", mp_obj.getPayload());
    payloadObj.pushKV("payloadsize", mp_obj.getPayloadSize());
    return payloadObj;
}

// determine whether to automatically commit transactions
UniValue tl_setautocommit(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "tl_setautocommit \"flag\" \n"

            "\nSets the global flag that determines whether transactions are automatically committed and broadcast.\n"

            "\nArguments:\n"
            "1. flag                 (boolean, required) the flag\n"

            "\nResult:\n"
            "true|false              (boolean) the updated flag status\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_setautocommit", "false")
            + HelpExampleRpc("tl_setautocommit", "false")
        );

    LOCK(cs_tally);

    autoCommit = request.params[0].get_bool();
    return autoCommit;
}

// display the tally map & the offer/accept list(s)
UniValue mscrpc(const JSONRPCRequest& request)
{
    int extra = 0;
    int extra2 = 0, extra3 = 0;

    if (request.fHelp || request.params.size() > 3)
        throw runtime_error(
            "mscrpc\n"
            "\nReturns the number of blocks in the longest block chain.\n"
            "\nResult:\n"
            "n    (number) the current block count\n"
            "\nExamples:\n"
            + HelpExampleCli("mscrpc", "")
            + HelpExampleRpc("mscrpc", "")
        );

    if (0 < request.params.size()) extra = atoi(request.params[0].get_str());
    if (1 < request.params.size()) extra2 = atoi(request.params[1].get_str());
    if (2 < request.params.size()) extra3 = atoi(request.params[2].get_str());

    PrintToLog("%s(extra=%d,extra2=%d,extra3=%d)\n", __FUNCTION__, extra, extra2, extra3);

    bool bDivisible = isPropertyDivisible(extra2);

    // various extra tests
    switch (extra) {
        case 0:
        {
            LOCK(cs_tally);
            int64_t total = 0;
            // display all balances
            for (std::unordered_map<std::string, CMPTally>::iterator my_it = mp_tally_map.begin(); my_it != mp_tally_map.end(); ++my_it) {
                PrintToLog("%34s => ", my_it->first);
                total += (my_it->second).print(extra2, bDivisible);
            }
            PrintToLog("total for property %d  = %X is %s\n", extra2, extra2, FormatDivisibleMP(total));
            break;
        }
        case 1:
        {
            LOCK(cs_tally);
            // display the whole CMPTxList (leveldb)
            p_txlistdb->printAll();
            p_txlistdb->printStats();
            break;
        }
        case 2:
        {
            LOCK(cs_tally);
            // display smart properties
            _my_sps->printAll();
            break;
        }
        case 3:
        {
            LOCK(cs_tally);
            uint32_t id = 0;
            // for each address display all currencies it holds
            for (std::unordered_map<std::string, CMPTally>::iterator my_it = mp_tally_map.begin(); my_it != mp_tally_map.end(); ++my_it) {
                PrintToLog("%34s => ", my_it->first);
                (my_it->second).print(extra2);
                (my_it->second).init();
                while (0 != (id = (my_it->second).next())) {
                    PrintToLog("Id: %u=0x%X ", id, id);
                }
                PrintToLog("\n");
            }
            break;
        }
        case 5:
        {
            LOCK(cs_tally);
            PrintToLog("isMPinBlockRange(%d,%d)=%s\n", extra2, extra3, isMPinBlockRange(extra2, extra3, false) ? "YES" : "NO");
            break;
        }
        case 6:
        {
            // NA
            break;
        }
        case 7:
        {
            // NA
            break;
        }
        case 8:
        {
            // NA
            break;
        }
        case 9:
        {
            PrintToLog("Locking cs_tally for %d milliseconds..\n", extra2);
            LOCK(cs_tally);
            UninterruptibleSleep(std::chrono::milliseconds{extra2});
            PrintToLog("Unlocking cs_tally now\n");
            break;
        }
        case 10:
        {
            PrintToLog("Locking cs_main for %d milliseconds..\n", extra2);
            LOCK(cs_main);
            UninterruptibleSleep(std::chrono::milliseconds{extra2});
            PrintToLog("Unlocking cs_main now\n");
            break;
        }
#ifdef ENABLE_WALLET
        case 11:
        {
            PrintToLog("Locking pwalletMain->cs_wallet for %d milliseconds..\n", extra2);
            CWalletRef pwalletMain = nullptr;
            if (vpwallets.size() > 0){
                pwalletMain = vpwallets[0];
            }
            LOCK(pwalletMain->cs_wallet);
            UninterruptibleSleep(std::chrono::milliseconds{extra2});
            PrintToLog("Unlocking pwalletMain->cs_wallet now\n");
            break;
        }
#endif
        case 14:
        {
            //NA
            break;
        }
        case 15:
        {
            // test each byte boundary during compression - TODO: talk to dexx about changing into BOOST unit test
            PrintToLog("Running varint compression tests...\n");
            int passed = 0;
            int failed = 0;
            std::vector<uint64_t> testValues;
            testValues.push_back((uint64_t)0);                    // one byte min
            testValues.push_back((uint64_t)127);                  // one byte max
            testValues.push_back((uint64_t)128);                  // two bytes min
            testValues.push_back((uint64_t)16383);                // two bytes max
            testValues.push_back((uint64_t)16384);                // three bytes min
            testValues.push_back((uint64_t)2097151);              // three bytes max
            testValues.push_back((uint64_t)2097152);              // four bytes min
            testValues.push_back((uint64_t)268435455);            // four bytes max
            testValues.push_back((uint64_t)268435456);            // five bytes min
            testValues.push_back((uint64_t)34359738367);          // five bytes max
            testValues.push_back((uint64_t)34359738368);          // six bytes min
            testValues.push_back((uint64_t)4398046511103);        // six bytes max
            testValues.push_back((uint64_t)4398046511104);        // seven bytes min
            testValues.push_back((uint64_t)562949953421311);      // seven bytes max
            testValues.push_back((uint64_t)562949953421312);      // eight bytes min
            testValues.push_back((uint64_t)72057594037927935);    // eight bytes max
            testValues.push_back((uint64_t)72057594037927936);    // nine bytes min
            testValues.push_back((uint64_t)9223372036854775807);  // nine bytes max
            std::vector<uint64_t>::const_iterator valuesIt;
            for (valuesIt = testValues.begin(); valuesIt != testValues.end(); valuesIt++) {
                uint64_t n = *valuesIt;
                std::vector<uint8_t> compressedBytes = CompressInteger(n);
                uint64_t result = DecompressInteger(compressedBytes);
                std::stringstream ss;
                ss << std::hex << std::setfill('0');
                std::vector<uint8_t>::const_iterator it;
                for (it = compressedBytes.begin(); it != compressedBytes.end(); it++) {
                    ss << " " << std::setw(2) << static_cast<unsigned>(*it); // \\x
                }
                PrintToLog("Integer: %d... compressed varint bytes:%s... decompressed integer %d... ",n,ss.str(),result);
                if (result == n) {
                    PrintToLog("PASS\n");
                    passed++;
                } else {
                    PrintToLog("FAIL\n");
                    failed++;
                }
            }
            PrintToLog("Varint compression tests complete.\n");
            PrintToLog("======================================\n");
            PrintToLog("Passed: %d     Failed: %d\n", passed, failed);
            break;
        }

        default:
            break;
    }

    return GetHeight();
}

// display an MP balance via RPC
UniValue tl_getbalance(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "tl_getbalance \"address\" \"propertyid\" \n"

            "\nReturns the token balance for a given address and property.\n"

            "\nArguments:\n"
            "1. address              (string, required) the address\n"
            "2. propertyid           (number, required) the property identifier\n"

            "\nResult:\n"
            "{\n"
            "  \"balance\" : \"n.nnnnnnnn\",   (string) the available balance of the address\n"
            "  \"reserved\" : \"n.nnnnnnnn\"   (string) the amount reserved by sell offers and accepts\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_getbalance", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\" \"1\"")
            + HelpExampleRpc("tl_getbalance", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\", \"1\"")
        );

    std::string address = ParseAddress(request.params[0]);
    uint32_t propertyId = ParsePropertyId(request.params[1]);

    // RequireExistingProperty(propertyId);
    // (propertyId);

    UniValue balanceObj(UniValue::VOBJ);
    BalanceToJSON(address, propertyId, balanceObj, isPropertyDivisible(propertyId));

    return balanceObj;
}

// XXX imports for tl_getwalletbalance.
extern UniValue listreceivedbyaddress(const JSONRPCRequest& request);
extern UniValue tl_getallbalancesforaddress(const JSONRPCRequest& request);
UniValue tl_getwalletbalance(const JSONRPCRequest& request)
{
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp)
        throw runtime_error(
            "tl_getwalletbalance \n"

            "\nReturn the native and ALL balances of the wallet.\n"

            "\nArguments:\n"
            "\nResult:\n"
            "{\n"
            "  \"LTC_balance\" : \"n.nnnnnnnn\",   (string) the available native balance of the wallet\n"
            "  \"ALL_balance\" : \"n.nnnnnnnn\",   (string) the available ALL balance of the wallet\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_getwalletbalance", "")
            + HelpExampleRpc("tl_getwalletbalance", "")
        );

    // bool fVerbose = (!request.params[0].isNull() && 1 == ParseBinary(request.params[0])) ? true : false;

    UniValue addressesObj(UniValue::VARR);
    UniValue balanceObj(UniValue::VOBJ);
    int64_t LTC_balance = 0;
    int64_t ALL_balance = 0;

    // 1. listreceivedbyaddress.
    {
        JSONRPCRequest listrequest;
        addressesObj = listreceivedbyaddress(listrequest);
    }
    // 2. loop over the addresses and accumulate balance.
    {
        for (auto entry: addressesObj.getValues())
        {
            // entry[address]
            // entry[amount]


            LTC_balance += ParseAmount(entry["amount"].getValStr(), true);

            // 2b. loop over the properties of the address and accumulate balance.

            UniValue tl_entries(UniValue::VARR);
            JSONRPCRequest balancesrequest;
            balancesrequest.params.push_back(entry["address"].get_str());

            try {
                tl_entries = tl_getallbalancesforaddress(balancesrequest);
                for (auto tl_entry: tl_entries.getValues())
                {
                    // tl_entry[propertyid]
                    // tl_entry[balance]
                    // tl_entry[reserved]

                    ALL_balance += ParseAmount(tl_entry["balance"].getValStr(), true);
                }
            } catch(...)
            {
            }

        }
    }

    balanceObj.pushKV("LTC_balance", LTC_balance);
    balanceObj.pushKV("ALL_balance", ALL_balance);

    return balanceObj;
}

// display an unvested balance via RPC
UniValue tl_getunvested(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "tl_getunvested \"address\" \n"

            "\nReturns the token balance for unvested ALLs (via vesting tokens).\n"

            "\nArguments:\n"
            "1. address              (string, required) the address\n"

            "\nResult:\n"
            "{\n"
            "  \"unvested\" : \"n.nnnnnnnn\",   (string) the unvested balance of ALLs in the address\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_getunvested", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\"")
            + HelpExampleRpc("tl_getunvested", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\"")
        );

    const std::string address = ParseAddress(request.params[0]);

    // RequireExistingProperty(propertyId);
    // RequireNotContract(propertyId);

    UniValue balanceObj(UniValue::VOBJ);
    UnvestedToJSON(address, ALL, balanceObj, isPropertyDivisible(ALL));

    return balanceObj;
}

UniValue tl_getreserve(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "tl_getreserve \"address\" \"propertyid\" \n"

            "\nReturns the token reserve (margin) account using in futures contracts, for a given address and property.\n"

            "\nArguments:\n"
            "1. address              (string, required) the address\n"
            "2. contractid           (number, required) the contract identifier\n"

            "\nResult:\n"
            "{\n"
            "  \"balance\" : \"n.nnnnnnnn\",   (string) the available balance of the address\n"
            "  \"reserved\" : \"n.nnnnnnnn\"   (string) the amount reserved by sell offers and accepts\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_getreserve", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\" \"1\"")
            + HelpExampleRpc("tl_getreserve", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\", \"1\"")
        );

    const std::string address = ParseAddress(request.params[0]);
    uint32_t contractId = ParsePropertyId(request.params[1]);

    // RequireExistingProperty(propertyId);
    // RequireNotContract(propertyId);

    UniValue balanceObj(UniValue::VOBJ);
    ReserveToJSON(address, contractId, balanceObj);

    return balanceObj;
}

UniValue tl_get_channelreserve(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "tl_getchannelreserve \"address\" propertyid\n"

            "\nReturns the token reserve account for a given channel address.\n"

            "\nArguments:\n"
            "1. channel address      (string, required) the address\n"
            "2. propertyid           (number, required) the contract identifier\n"

            "\nResult:\n"
            "{\n"
            "  \"channel reserve\" : \"n.nnnnnnnn\",   (string) the available balance of the address\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_get_channelreserve", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\" \"1\"")
            + HelpExampleRpc("tl_get_channelreserve", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\", \"1\"")
        );

    const std::string address = ParseAddress(request.params[0]);
    uint32_t propertyId = ParsePropertyId(request.params[1]);

    RequireExistingProperty(propertyId);
    // RequireNotContract(propertyId);

    UniValue balanceObj(UniValue::VOBJ);
    ChannelToJSON(address, propertyId, balanceObj, isPropertyDivisible(propertyId));

    return balanceObj;
}

UniValue tl_get_channelremaining(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 3)
        throw runtime_error(
            "tl_getchannelremaining \"address\" \"channel\" propertyid\n"

            "\nReturns the token reserve account for a given single address in channel.\n"

            "\nArguments:\n"
            "1. sender               (string, required) the user address\n"
            "2. channel              (string, required) the channel address\n"
            "2. propertyid           (number, required) the contract identifier\n"

            "\nResult:\n"
            "{\n"
            "  \"channel remaining\" : \"n.nnnnnnnn\",   (string) the available balance for single address\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_get_channelremaining", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\", \"Qdj12J6FZgaY34ZNx12pVpTeF9NQdmpGzj\" \"1\"")
            + HelpExampleRpc("tl_get_channelremaining", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\", \"Qdj12J6FZgaY34ZNx12pVpTeF9NQdmpGzj\", \"1\"")
        );

    const std::string address = ParseAddress(request.params[0]);
    const std::string chn = ParseAddress(request.params[1]);
    uint32_t propertyId = ParsePropertyId(request.params[2]);

    RequireExistingProperty(propertyId);
    // RequireNotContract(propertyId);

    // checking the amount remaining in the channel
    uint64_t remaining = 0;
    auto it = channels_Map.find(chn);
    if (it != channels_Map.end()){
        const Channel& sChn = it->second;
        remaining = sChn.getRemaining(address, propertyId);
    }

    UniValue balanceObj(UniValue::VOBJ);
    balanceObj.pushKV("channel remaining", FormatMP(isPropertyDivisible(propertyId), remaining));

    return balanceObj;
}

UniValue tl_getchannel_info(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "tl_getchannel_info \"address\" \n"

            "\nReturns all multisig channel info.\n"

            "\nArguments:\n"
            "1. channel address      (string, required) the address\n"

            "\nResult:\n"
            "{ \n"
            "  \"multisig address\" : \"n.nnnnnnnn\",   (string) channel address\n"
            "  \"first address\" : \"n.nnnnnnnn\",   (string) first member of multisig\n"
            "  \"second address\" : \"n.nnnnnnnn\",   (string) second member of multisig\n"
            "  \"status\" : \"n.nnnnnnnn\",   (string) active or not\n"
            "} \n"

            "\nExamples:\n"
            + HelpExampleCli("tl_getchannel_info", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\"")
            + HelpExampleRpc("tl_getchannel_info", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\"")
        );

    const std::string address = ParseAddress(request.params[0]);

    UniValue response(UniValue::VOBJ);

    LOCK(cs_tally);

    t_tradelistdb->getChannelInfo(address,response);

    return response;
}

UniValue tl_getallbalancesforid(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "tl_getallbalancesforid \"propertyid\" \n"

            "\nReturns a list of token balances for a given currency or property identifier.\n"

            "\nArguments:\n"
            "1. propertyid           (number, required) the property identifier\n"

            "\nResult:\n"
            "[                           (array of JSON objects)\n"
            "  {\n"
            "    \"address\" : \"address\",      (string) the address\n"
            "    \"balance\" : \"n.nnnnnnnn\",   (string) the available balance of the address\n"
            "    \"reserved\" : \"n.nnnnnnnn\"   (string) the amount reserved by sell offers and accepts\n"
            "  },\n"
            "  ...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_getallbalancesforid", "\"1\"")
            + HelpExampleRpc("tl_getallbalancesforid", "\"1\"")
        );

    uint32_t propertyId = ParsePropertyId(request.params[0]);

    RequireExistingProperty(propertyId);

    UniValue response(UniValue::VARR);
    bool isDivisible = isPropertyDivisible(propertyId); // we want to check this BEFORE the loop

    LOCK(cs_tally);

    for (std::unordered_map<std::string, CMPTally>::iterator it = mp_tally_map.begin(); it != mp_tally_map.end(); ++it) {
        uint32_t id = 0;
        bool includeAddress = false;
        const std::string& address = it->first;
        (it->second).init();
        while (0 != (id = (it->second).next())) {
            if (id == propertyId) {
                includeAddress = true;
                break;
            }
        }
        if (!includeAddress) {
            continue; // ignore this address, has never transacted in this propertyId
        }
        UniValue balanceObj(UniValue::VOBJ);
        balanceObj.pushKV("address", address);
        bool nonEmptyBalance = BalanceToJSON(address, propertyId, balanceObj, isDivisible);

        if (nonEmptyBalance) {
            response.push_back(balanceObj);
        }
    }

    return response;
}

UniValue tl_getallbalancesforaddress(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "tl_getallbalancesforaddress \"address\" \n"

            "\nReturns a list of all token balances for a given address.\n"

            "\nArguments:\n"
            "1. address              (string, required) the address\n"

            "\nResult:\n"
            "[                           (array of JSON objects)\n"
            "  {\n"
            "    \"propertyid\" : n,           (number) the property identifier\n"
            "    \"balance\" : \"n.nnnnnnnn\",   (string) the available balance of the address\n"
            "    \"reserved\" : \"n.nnnnnnnn\"   (string) the amount reserved by sell offers and accepts\n"
            "  },\n"
            "  ...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_getallbalancesforaddress", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\"")
            + HelpExampleRpc("tl_getallbalancesforaddress", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\"")
        );

    const std::string address = ParseAddress(request.params[0]);

    UniValue response(UniValue::VARR);

    LOCK(cs_tally);

    CMPTally* addressTally = getTally(address);

    if (nullptr == addressTally) { // addressTally object does not exist
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Address not found");
    }

    addressTally->init();

    uint32_t propertyId = 0;
    while (0 != (propertyId = addressTally->next())) {
        UniValue balanceObj(UniValue::VOBJ);
        balanceObj.pushKV("propertyid", (uint64_t) propertyId);
        bool nonEmptyBalance = BalanceToJSON(address, propertyId, balanceObj, isPropertyDivisible(propertyId));

        if (nonEmptyBalance) {
            response.push_back(balanceObj);
        }
    }

    return response;
}

UniValue tl_getproperty(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "tl_getproperty \"propertyid\" \n"

            "\nReturns details for about the tokens or smart property to lookup.\n"

            "\nArguments:\n"
            "1. propertyid           (number, required) the identifier of the tokens or property\n"

            "\nResult:\n"
            "{\n"
            "  \"propertyid\" : n,                (number) the identifier\n"
            "  \"name\" : \"name\",               (string) the name of the tokens\n"
            "  \"data\" : \"information\",        (string) additional information or a description\n"
            "  \"url\" : \"uri\",                 (string) an URI, for example pointing to a website\n"
            "  \"divisible\" : true|false,        (boolean) whether the tokens are divisible\n"
            "  \"issuer\" : \"address\",          (string) the Bitcoin address of the issuer on record\n"
            "  \"creationtxid\" : \"hash\",       (string) the hex-encoded creation transaction hash\n"
            "  \"fixedissuance\" : true|false,    (boolean) whether the token supply is fixed\n"
            "  \"totaltokens\" : \"n.nnnnnnnn\"   (string) the total number of tokens in existence\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_getproperty", "\"3\"")
            + HelpExampleRpc("tl_getproperty", "\"3\"")
        );

    uint32_t propertyId = ParsePropertyId(request.params[0]);

    RequireExistingProperty(propertyId);

    CMPSPInfo::Entry sp;
    {
        LOCK(cs_tally);
        if (!_my_sps->getSP(propertyId, sp)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Property identifier does not exist");
        }
    }

    int64_t nTotalTokens = getTotalTokens(propertyId);
    const std::string strCreationHash = sp.txid.GetHex();
    const std::string strTotalTokens = FormatMP(propertyId, nTotalTokens);
    std::string denominator = "";

    UniValue response(UniValue::VOBJ);
    response.pushKV("propertyid", (uint64_t) propertyId);
    PropertyToJSON(sp, response); // name, data, url, divisible
    KYCToJSON(sp, response);
    response.pushKV("issuer", sp.issuer);
    response.pushKV("creationtxid", strCreationHash);
    response.pushKV("fixedissuance", sp.fixed);
    response.pushKV("totaltokens", strTotalTokens);
    response.pushKV("creation block", sp.init_block);

    if (sp.isPegged()) {
        response.pushKV("contract associated",(uint64_t) sp.contract_associated);
        // response.pushKV("series", sp.series);
    } else {
        const int64_t ltc_volume = lastVolume(propertyId, false);
        const int64_t token_volume = lastVolume(propertyId, true);
        response.pushKV("last 24h LTC volume", FormatDivisibleMP(ltc_volume));
        response.pushKV("last 24h Token volume", FormatDivisibleMP(token_volume));
    }

    return response;
}

UniValue tl_listproperties(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw runtime_error(
                "tl_listproperties \"verbose\"\n"

                "\nLists all tokens or smart properties.\n"

                "\nArguments:\n"
                "1. verbose                      (number, optional) 1 if more info is needed\n"

                "\nResult:\n"
                "[                                (array of JSON objects)\n"
                "  {\n"
                "    \"propertyid\" : n,          (number) the identifier of the tokens\n"
                "    \"name\" : \"name\",         (string) the name of the tokens\n"
                "    \"data\" : \"information\",  (string) additional information or a description\n"
                "    \"url\" : \"uri\",           (string) an URI, for example pointing to a website\n"
                "    \"divisible\" : true|false   (boolean) whether the tokens are divisible\n"
                "  },\n"
                "  ...\n"
                "]\n"

                "\nExamples:\n"
                + HelpExampleCli("tl_listproperties", "\"1\"")
                + HelpExampleRpc("tl_listproperties", "\"1\"")
                );

    bool fVerbose = (!request.params[0].isNull() && 1 == ParseBinary(request.params[0])) ? true : false;

    UniValue response(UniValue::VARR);

    LOCK(cs_tally);

    const uint32_t nextSPID = _my_sps->peekNextSPID();
    for (uint32_t propertyId = 1; propertyId < nextSPID; propertyId++)
    {
        UniValue propertyObj(UniValue::VOBJ);
        CMPSPInfo::Entry sp;
        if(_my_sps->getSP(propertyId, sp))
        {
            const int64_t nTotalTokens = getTotalTokens(propertyId);
            std::string strCreationHash = sp.txid.GetHex();
            std::string strTotalTokens = FormatMP(propertyId, nTotalTokens);
            propertyObj.pushKV("propertyid", (uint64_t) propertyId);
            PropertyToJSON(sp, propertyObj); // name, data, url, divisible
            KYCToJSON(sp, propertyObj);

            if(fVerbose)
            {
                propertyObj.pushKV("issuer", sp.issuer);
                propertyObj.pushKV("creationtxid", strCreationHash);
                propertyObj.pushKV("fixedissuance", sp.fixed);
                propertyObj.pushKV("creation block", sp.init_block);
                if (sp.isPegged()) {
                    propertyObj.pushKV("contract associated",(uint64_t) sp.contract_associated);
                    // propertyObj.pushKV("series", sp.series);
                } else {
                    const int64_t ltc_volume = lastVolume(propertyId, false);
                    const int64_t token_volume = lastVolume(propertyId, true);
                    propertyObj.pushKV("last 24h LTC volume", FormatDivisibleMP(ltc_volume));
                    propertyObj.pushKV("last 24h Token volume", FormatDivisibleMP(token_volume));
                    propertyObj.pushKV("totaltokens", strTotalTokens);
                }
            }

            response.push_back(propertyObj);
        }

    }

    return response;
}

UniValue tl_list_natives(const JSONRPCRequest& request)
{
  if (request.fHelp)
    throw runtime_error(
			"tl_listnatives\n"


			"\nLists all native contracts.\n"

			"\nResult:\n"
			"[                                (array of JSON objects)\n"
			"  {\n"
			"    \"propertyid\" : n,                (number) the identifier of the tokens\n"
			"    \"name\" : \"name\",                 (string) the name of the tokens\n"
			"    \"data\" : \"information\",          (string) additional information or a description\n"
			"    \"url\" : \"uri\",                   (string) an URI, for example pointing to a website\n"
			"  },\n"
			"  ...\n"
			"]\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_list_natives", "")
			+ HelpExampleRpc("tl_list_natives", "")
			);

  UniValue response(UniValue::VARR);

  LOCK(cs_register);

  const uint32_t nextCDID = _my_cds->peekNextContractID();
  for (uint32_t contractId = 1; contractId < nextCDID; contractId++)
  {
      CDInfo::Entry cd;
      if (_my_cds->getCD(contractId, cd))
      {
          if(!cd.isNative()) {
              continue;
          }

          UniValue propertyObj(UniValue::VOBJ);
          propertyObj.pushKV("contractid", (uint64_t) contractId);
          ContractToJSON(cd, propertyObj); // name, data, url,...
          response.push_back(propertyObj);
          const int64_t openInterest = getTotalLives(contractId);
          propertyObj.pushKV("open interest", FormatDivisibleMP(openInterest));
          auto it = cdexlastprice.find(contractId);
          int64_t lastPrice = 0;
          if (it != cdexlastprice.end()){
              const auto& blockMap = it->second;
              const auto& itt = std::prev(blockMap.end());
              if (itt != blockMap.end()) {
                 const auto& prices = itt->second;
                 const auto& ix = std::prev(prices.end());
                 lastPrice = *ix;
              }
          }
          propertyObj.pushKV("last traded price", FormatDivisibleMP(lastPrice));
          const int64_t fundBalance = 0; // NOTE: we need to write this after fundbalance logic is done
          propertyObj.pushKV("insurance fund balance", fundBalance);

      }
  }

  return response;
}

UniValue tl_list_oracles(const JSONRPCRequest& request)
{
  if (request.fHelp)
    throw runtime_error(
			"tl_list_oracles\n"

			"\nLists all oracles contracts.\n"

			"\nResult:\n"
			"[                                (array of JSON objects)\n"
			"  {\n"
			"    \"propertyid\" : n,                (number) the identifier of the tokens\n"
			"    \"name\" : \"name\",                 (string) the name of the tokens\n"
			"    \"data\" : \"information\",          (string) additional information or a description\n"
			"    \"url\" : \"uri\",                   (string) an URI, for example pointing to a website\n"
			"  },\n"
			"  ...\n"
			"]\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_list_natives", "")
			+ HelpExampleRpc("tl_list_natives", "")
			);

  UniValue response(UniValue::VARR);

  LOCK(cs_register);

  const uint32_t nextCDID = _my_cds->peekNextContractID();
  for (uint32_t contractId = 1; contractId < nextCDID; contractId++)
  {
      CDInfo::Entry cd;

      if (_my_cds->getCD(contractId, cd))
      {
          if(!cd.isOracle()) {
              continue;
          }

          UniValue propertyObj(UniValue::VOBJ);
          propertyObj.pushKV("propertyid", (uint64_t) contractId);
          OracleToJSON(cd, propertyObj); // name, data, url,...
          response.push_back(propertyObj);
          const int64_t openInterest = getTotalLives(contractId);
          propertyObj.pushKV("open interest", FormatDivisibleMP(openInterest));
          auto it = cdexlastprice.find(contractId);
          int64_t lastPrice = 0;
          if (it != cdexlastprice.end()){
              const auto& blockMap = it->second;
              const auto& itt = std::prev(blockMap.end());
              if (itt != blockMap.end()) {
                 const auto& prices = itt->second;
                 const auto& ix = std::prev(prices.end());
                 lastPrice = *ix;
              }
          }
          propertyObj.pushKV("last traded price", FormatDivisibleMP(lastPrice));
          const int64_t fundBalance = 0; // NOTE: we need to write this after fundbalance logic is done
          propertyObj.pushKV("insurance fund balance", fundBalance);
      }
  }

  return response;
}

UniValue tl_getgrants(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "tl_getgrants \"propertyid\" \n"

            "\nReturns information about granted and revoked units of managed tokens.\n"

            "\nArguments:\n"
            "1. propertyid           (number, required) the identifier of the managed tokens to lookup\n"

            "\nResult:\n"
            "{\n"
            "  \"propertyid\" : n,               (number) the identifier of the managed tokens\n"
            "  \"name\" : \"name\",                (string) the name of the tokens\n"
            "  \"issuer\" : \"address\",           (string) the Bitcoin address of the issuer on record\n"
            "  \"creationtxid\" : \"hash\",        (string) the hex-encoded creation transaction hash\n"
            "  \"totaltokens\" : \"n.nnnnnnnn\",   (string) the total number of tokens in existence\n"
            "  \"issuances\": [                  (array of JSON objects) a list of the granted and revoked tokens\n"
            "    {\n"
            "      \"txid\" : \"hash\",                (string) the hash of the transaction that granted tokens\n"
            "      \"grant\" : \"n.nnnnnnnn\"          (string) the number of tokens granted by this transaction\n"
            "    },\n"
            "    {\n"
            "      \"txid\" : \"hash\",                (string) the hash of the transaction that revoked tokens\n"
            "      \"grant\" : \"n.nnnnnnnn\"          (string) the number of tokens revoked by this transaction\n"
            "    },\n"
            "    ...\n"
            "  ]\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_getgrants", "\"31\"")
            + HelpExampleRpc("tl_getgrants", "\"31\"")
        );

    uint32_t propertyId = ParsePropertyId(request.params[0]);

    RequireExistingProperty(propertyId);
    RequireManagedProperty(propertyId);

    CMPSPInfo::Entry sp;
    {
        LOCK(cs_tally);
        if (!_my_sps->getSP(propertyId, sp)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Property identifier does not exist");
        }
    }
    UniValue response(UniValue::VOBJ);
    const uint256& creationHash = sp.txid;
    int64_t totalTokens = getTotalTokens(propertyId);

    // TODO: sort by height?

    UniValue issuancetxs(UniValue::VARR);
    std::map<uint256, std::vector<int64_t> >::const_iterator it;
    for (it = sp.historicalData.begin(); it != sp.historicalData.end(); it++) {
        const std::string& txid = it->first.GetHex();
        int64_t grantedTokens = it->second.at(0);
        int64_t revokedTokens = it->second.at(1);

        if (grantedTokens > 0) {
            UniValue granttx(UniValue::VOBJ);
            granttx.pushKV("txid", txid);
            granttx.pushKV("grant", FormatMP(propertyId, grantedTokens));
            issuancetxs.push_back(granttx);
        }

        if (revokedTokens > 0) {
            UniValue revoketx(UniValue::VOBJ);
            revoketx.pushKV("txid", txid);
            revoketx.pushKV("revoke", FormatMP(propertyId, revokedTokens));
            issuancetxs.push_back(revoketx);
        }
    }

    response.pushKV("propertyid", (uint64_t) propertyId);
    response.pushKV("name", sp.name);
    response.pushKV("issuer", sp.issuer);
    response.pushKV("creationtxid", creationHash.GetHex());
    response.pushKV("totaltokens", FormatMP(propertyId, totalTokens));
    response.pushKV("issuances", issuancetxs);

    return response;
}

UniValue tl_listblocktransactions(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "tl_listblocktransactions \"index\" \n"

            "\nLists all Trade Layer transactions in a block.\n"

            "\nArguments:\n"
            "1. index                (number, required) the block height or block index\n"

            "\nResult:\n"
            "[                       (array of string)\n"
            "  \"hash\",                 (string) the hash of the transaction\n"
            "  ...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_listblocktransactions", "\"279007\"")
            + HelpExampleRpc("tl_listblocktransactions", "\"279007\"")
        );

    int blockHeight = request.params[0].get_int();

    RequireHeightInChain(blockHeight);

    // next let's obtain the block for this height
    CBlock block;
    {
        LOCK(cs_main);
        CBlockIndex* pBlockIndex = chainActive[blockHeight];

        if (!ReadBlockFromDisk(block, pBlockIndex, Params().GetConsensus())) {
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to read block from disk");
        }
    }

    UniValue response(UniValue::VARR);

    // now we want to loop through each of the transactions in the block and run against CMPTxList::exists
    // those that return positive add to our response array

    LOCK(cs_tally);
    for(const auto &tx : block.vtx) {
        if (p_txlistdb->exists((*(tx)).GetHash())) {
            // later we can add a verbose flag to decode here, but for now callers can send returned txids into gettransaction_MP
            // add the txid into the response as it's an MP transaction
            response.push_back((*(tx)).GetHash().GetHex());
        }
    }

    return response;
}

UniValue tl_gettransaction(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() != 1)
    throw runtime_error(
			"tl_gettransaction \"txid\" \n"

			"\nGet detailed information about an Trade Layer transaction.\n"

			"\nArguments:\n"
			"1. txid                 (string, required) the hash of the transaction to lookup\n"

			"\nResult:\n"
			"{\n"
			"  \"txid\" : \"hash\",                  (string) the hex-encoded hash of the transaction\n"
			"  \"sendingaddress\" : \"address\",     (string) the Bitcoin address of the sender\n"
			"  \"referenceaddress\" : \"address\",   (string) a Bitcoin address used as reference (if any)\n"
			"  \"ismine\" : true|false,            (boolean) whether the transaction involes an address in the wallet\n"
			"  \"confirmations\" : nnnnnnnnnn,     (number) the number of transaction confirmations\n"
			"  \"fee\" : \"n.nnnnnnnn\",             (string) the transaction fee in bitcoins\n"
			"  \"blocktime\" : nnnnnnnnnn,         (number) the timestamp of the block that contains the transaction\n"
			"  \"valid\" : true|false,             (boolean) whether the transaction is valid\n"
			"  \"version\" : n,                    (number) the transaction version\n"
			"  \"type_int\" : n,                   (number) the transaction type as number\n"
			"  \"type\" : \"type\",                  (string) the transaction type as string\n"
			"  [...]                             (mixed) other transaction type specific properties\n"
			"}\n"

			"\nbExamples:\n"
			+ HelpExampleCli("tl_gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
			+ HelpExampleRpc("tl_gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
			);

  uint256 hash = ParseHashV(request.params[0], "txid");

  UniValue txobj(UniValue::VOBJ);
  int populateResult = populateRPCTransactionObject(hash, txobj);
  if (populateResult < 0) PopulateFailure(populateResult);

  return txobj;
}

UniValue tl_listtransactions(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 5)
        throw runtime_error(
            "tl_listtransactions (\"address\" \"count\" \"skip\" \"startblock\" \"endblock\") \n"

            "\nList wallet transactions, optionally filtered by an address and block boundaries.\n"

            "\nArguments:\n"
            "1. address              (string, optional) address filter (default: \"*\")\n"
            "2. count                (number, optional) show at most n transactions (default: 10)\n"
            "3. skip                 (number, optional) skip the first n transactions (default: 0)\n"
            "4. startblock           (number, optional) first block to begin the search (default: 0)\n"
            "5. endblock             (number, optional) last block to include in the search (default: 9999999)\n"

            "\nResult:\n"
            "[                                 (array of JSON objects)\n"
            "  {\n"
            "    \"txid\" : \"hash\",                  (string) the hex-encoded hash of the transaction\n"
            "    \"sendingaddress\" : \"address\",     (string) the Bitcoin address of the sender\n"
            "    \"referenceaddress\" : \"address\",   (string) a Bitcoin address used as reference (if any)\n"
            "    \"ismine\" : true|false,            (boolean) whether the transaction involes an address in the wallet\n"
            "    \"confirmations\" : nnnnnnnnnn,     (number) the number of transaction confirmations\n"
            "    \"fee\" : \"n.nnnnnnnn\",             (string) the transaction fee in bitcoins\n"
            "    \"blocktime\" : nnnnnnnnnn,         (number) the timestamp of the block that contains the transaction\n"
            "    \"valid\" : true|false,             (boolean) whether the transaction is valid\n"
            "    \"version\" : n,                    (number) the transaction version\n"
            "    \"type_int\" : n,                   (number) the transaction type as number\n"
            "    \"type\" : \"type\",                  (string) the transaction type as string\n"
            "    [...]                             (mixed) other transaction type specific properties\n"
            "  },\n"
            "  ...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_listtransactions", "")
            + HelpExampleRpc("tl_listtransactions", "")
        );

    // obtains parameters - default all wallet addresses & last 10 transactions
    std::string addressParam;
    if (request.params.size() > 0) {
        if (("*" != request.params[0].get_str()) && ("" != request.params[0].get_str())) addressParam = request.params[0].get_str();
    }
    int64_t nCount = 10;
    if (request.params.size() > 1) nCount = request.params[1].get_int64();
    if (nCount < 0) throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative count");
    int64_t nFrom = 0;
    if (request.params.size() > 2) nFrom = request.params[2].get_int64();
    if (nFrom < 0) throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative from");
    int64_t nStartBlock = 0;
    if (request.params.size() > 3) nStartBlock = request.params[3].get_int64();
    if (nStartBlock < 0) throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative start block");
    int64_t nEndBlock = 9999999;
    if (request.params.size() > 4) nEndBlock = request.params[4].get_int64();
    if (nEndBlock < 0) throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative end block");

    // obtain a sorted list of trade layer wallet transactions (including STO receipts and pending)
    std::map<std::string,uint256> walletTransactions = FetchWalletTLTransactions(nFrom+nCount, nStartBlock, nEndBlock);

    // reverse iterate over (now ordered) transactions and populate RPC objects for each one
    UniValue response(UniValue::VARR);
    for (std::map<std::string,uint256>::reverse_iterator it = walletTransactions.rbegin(); it != walletTransactions.rend(); it++) {
        const uint256& txHash = it->second;
        UniValue txobj(UniValue::VOBJ);
        int populateResult = populateRPCTransactionObject(txHash, txobj, addressParam);
        if (0 == populateResult) response.push_back(txobj);
    }

    // TODO: reenable cutting!
/*
    // cut on nFrom and nCount
    if (nFrom > (int)response.size()) nFrom = response.size();
    if ((nFrom + nCount) > (int)response.size()) nCount = response.size() - nFrom;
    UniValue::iterator first = response.begin();
    std::advance(first, nFrom);
    UniValue::iterator last = response.begin();
    std::advance(last, nFrom+nCount);
    if (last != response.end()) response.erase(last, response.end());
    if (first != response.begin()) response.erase(response.begin(), first);
    std::reverse(response.begin(), response.end());
*/
    return response;
}

UniValue tl_listpendingtransactions(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw runtime_error(
            "tl_listpendingtransactions ( \"address\" )\n"

            "\nReturns a list of unconfirmed Trade Layer transactions, pending in the memory pool.\n"
            "\nAn optional filter can be provided to only include transactions which involve the given address.\n"
            "\nNote: the validity of pending transactions is uncertain, and the state of the memory pool may "
            "change at any moment. It is recommended to check transactions after confirmation, and pending "
            "transactions should be considered as invalid.\n"

            "\nArguments:\n"
            "1. address              (string, optional) address filter (default: \"\" for no filter)\n"

            "\nResult:\n"
            "[                                 (array of JSON objects)\n"
            "  {\n"
            "    \"txid\" : \"hash\",                  (string) the hex-encoded hash of the transaction\n"
            "    \"sendingaddress\" : \"address\",     (string) the Bitcoin address of the sender\n"
            "    \"referenceaddress\" : \"address\",   (string) a Bitcoin address used as reference (if any)\n"
            "    \"ismine\" : true|false,            (boolean) whether the transaction involes an address in the wallet\n"
            "    \"fee\" : \"n.nnnnnnnn\",             (string) the transaction fee in bitcoins\n"
            "    \"version\" : n,                    (number) the transaction version\n"
            "    \"type_int\" : n,                   (number) the transaction type as number\n"
            "    \"type\" : \"type\",                  (string) the transaction type as string\n"
            "    [...]                             (mixed) other transaction type specific properties\n"
            "  },\n"
            "  ...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_listpendingtransactions", "")
            + HelpExampleRpc("tl_listpendingtransactions", "")
        );

    std::string filterAddress;
    if (request.params.size() > 0) {
        filterAddress = ParseAddressOrEmpty(request.params[0]);
    }

    std::vector<uint256> vTxid;
    mempool.queryHashes(vTxid);

    UniValue result(UniValue::VARR);
    for(const auto &hash : vTxid) {
        UniValue txObj(UniValue::VOBJ);
        if (populateRPCTransactionObject(hash, txObj, filterAddress) == 0) {
            result.push_back(txObj);
        }
    }

    return result;
}

UniValue tl_getinfo(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw runtime_error(
            "tl_getinfo\n"

            "Returns various state information of the client and protocol.\n"

            "\nResult:\n"
            "{\n"
            "  \"tradelayer_version_int\" : xxxxxxx,         (number) client version as integer\n"
            "  \"tradelayer_coreversion\" : \"x.x.x.x-xxx\", (string) client version\n"
            "  \"litecoinversion\" : \"x.x.x\",              (string) Litecoin Core version\n"
            "  \"commitinfo\" : \"xxxxxxx\",                 (string) build commit identifier\n"
            "  \"block\" : nnnnnn,                           (number) index of the last processed block\n"
            "  \"blocktime\" : nnnnnnnnnn,                   (number) timestamp of the last processed block\n"
            "  \"blocktransactions\" : nnnn,                 (number) Trade Layer transactions found in the last processed block\n"
            "  \"totaltransactions\" : nnnnnnnn,             (number) Trade Layer transactions processed in total\n"
            "  \"alerts\" : [                                (array of JSON objects) active protocol alert (if any)\n"
            "    {\n"
            "      \"alerttypeint\" : n,                     (number) alert type as integer\n"
            "      \"alerttype\" : \"xxx\",                  (string) alert type\n"
            "      \"alertexpiry\" : \"nnnnnnnnnn\",         (string) expiration criteria\n"
            "      \"alertmessage\" : \"xxx\"                (string) information about the alert\n"
            "    },\n"
            "    ...\n"
            "  ]\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_getinfo", "")
            + HelpExampleRpc("tl_getinfo", "")
        );

    UniValue infoResponse(UniValue::VOBJ);

    // provide the mastercore and bitcoin version and if available commit id
    infoResponse.pushKV("tradelayer_version_int", TL_VERSION);
    infoResponse.pushKV("tradelayer_coreversion", TradeLayerVersion());
    infoResponse.pushKV("litecoinversion", BitcoinCoreVersion());
    infoResponse.pushKV("commitinfo", BuildCommit());

    // provide the current block details
    int block = GetHeight();
    int64_t blockTime = GetLatestBlockTime();

    LOCK(cs_tally);

    int blockMPTransactions = p_txlistdb->getMPTransactionCountBlock(block);
    int totalMPTransactions = p_txlistdb->getMPTransactionCountTotal();
    infoResponse.pushKV("block", block);
    infoResponse.pushKV("blocktime", blockTime);
    infoResponse.pushKV("blocktransactions", blockMPTransactions);
    infoResponse.pushKV("totaltransactions", totalMPTransactions);

    // handle alerts
    UniValue alerts(UniValue::VARR);
    std::vector<AlertData> tlAlerts = GetTradeLayerAlerts();
    for (std::vector<AlertData>::iterator it = tlAlerts.begin(); it != tlAlerts.end(); it++) {
        AlertData alert = *it;
        UniValue alertResponse(UniValue::VOBJ);
        std::string alertTypeStr;
        switch (alert.alert_type) {
            case 1: alertTypeStr = "alertexpiringbyblock";
            break;
            case 2: alertTypeStr = "alertexpiringbyblocktime";
            break;
            case 3: alertTypeStr = "alertexpiringbyclientversion";
            break;
            default: alertTypeStr = "error";
        }
        alertResponse.pushKV("alerttypeint", alert.alert_type);
        alertResponse.pushKV("alerttype", alertTypeStr);
        alertResponse.pushKV("alertexpiry", FormatIndivisibleMP(alert.alert_expiry));
        alertResponse.pushKV("alertmessage", alert.alert_message);
        alerts.push_back(alertResponse);
    }
    infoResponse.pushKV("alerts", alerts);

    return infoResponse;
}

UniValue tl_getactivations(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw runtime_error(
            "tl_getactivations\n"

            "Returns pending and completed feature activations.\n"

            "\nResult:\n"
            "{\n"
            "  \"pendingactivations\": [       (array of JSON objects) a list of pending feature activations\n"
            "    {\n"
            "      \"featureid\" : n,              (number) the id of the feature\n"
            "      \"featurename\" : \"xxxxxxxx\",   (string) the name of the feature\n"
            "      \"activationblock\" : n,        (number) the block the feature will be activated\n"
            "      \"minimumversion\" : n          (number) the minimum client version needed to support this feature\n"
            "    },\n"
            "    ...\n"
            "  ]\n"
            "  \"completedactivations\": [     (array of JSON objects) a list of completed feature activations\n"
            "    {\n"
            "      \"featureid\" : n,              (number) the id of the feature\n"
            "      \"featurename\" : \"xxxxxxxx\",   (string) the name of the feature\n"
            "      \"activationblock\" : n,        (number) the block the feature will be activated\n"
            "      \"minimumversion\" : n          (number) the minimum client version needed to support this feature\n"
            "    },\n"
            "    ...\n"
            "  ]\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_getactivations", "")
            + HelpExampleRpc("tl_getactivations", "")
        );

    UniValue response(UniValue::VOBJ);

    UniValue arrayPendingActivations(UniValue::VARR);
    std::vector<FeatureActivation> vecPendingActivations = GetPendingActivations();
    for (std::vector<FeatureActivation>::iterator it = vecPendingActivations.begin(); it != vecPendingActivations.end(); ++it) {
        UniValue actObj(UniValue::VOBJ);
        FeatureActivation pendingAct = *it;
        actObj.pushKV("featureid", pendingAct.featureId);
        actObj.pushKV("featurename", pendingAct.featureName);
        actObj.pushKV("activationblock", pendingAct.activationBlock);
        actObj.pushKV("minimumversion", (uint64_t)pendingAct.minClientVersion);
        arrayPendingActivations.push_back(actObj);
    }

    UniValue arrayCompletedActivations(UniValue::VARR);
    std::vector<FeatureActivation> vecCompletedActivations = GetCompletedActivations();
    for (std::vector<FeatureActivation>::iterator it = vecCompletedActivations.begin(); it != vecCompletedActivations.end(); ++it) {
        UniValue actObj(UniValue::VOBJ);
        FeatureActivation completedAct = *it;
        actObj.pushKV("featureid", completedAct.featureId);
        actObj.pushKV("featurename", completedAct.featureName);
        actObj.pushKV("activationblock", completedAct.activationBlock);
        actObj.pushKV("minimumversion", (uint64_t)completedAct.minClientVersion);
        arrayCompletedActivations.push_back(actObj);
    }

    response.pushKV("pendingactivations", arrayPendingActivations);
    response.pushKV("completedactivations", arrayCompletedActivations);

    return response;
}

UniValue tl_getcurrentconsensushash(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw runtime_error(
            "tl_getcurrentconsensushash\n"

            "\nReturns the consensus hash for all balances for the current block.\n"

            "\nResult:\n"
            "{\n"
            "  \"block\" : nnnnnn,          (number) the index of the block this consensus hash applies to\n"
            "  \"blockhash\" : \"hash\",      (string) the hash of the corresponding block\n"
            "  \"consensushash\" : \"hash\"   (string) the consensus hash for the block\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_getcurrentconsensushash", "")
            + HelpExampleRpc("tl_getcurrentconsensushash", "")
        );

    UniValue response(UniValue::VOBJ);

    {
        LOCK(cs_main); // TODO - will this ensure we don't take in a new block in the couple of ms it takes to calculate the consensus hash?

        int block = GetHeight();

        CBlockIndex* pblockindex = chainActive[block];
        uint256 blockHash = pblockindex->GetBlockHash();

        uint256 consensusHash = GetConsensusHash();

        response.pushKV("block", block);
        response.pushKV("blockhash", blockHash.GetHex());
        response.pushKV("consensushash", consensusHash.GetHex());
    }

    return response;
}

bool PositionToJSON(const std::string& address, uint32_t contractId, UniValue& balance_obj)
{
    int64_t position  = getContractRecord(address, contractId, CONTRACT_POSITION);
    balance_obj.pushKV("position", position);

    return true;
}

// in progress
bool FullPositionToJSON(const std::string& address, uint32_t property, UniValue& position_obj, CDInfo::Entry sProperty)
{
  // const int64_t factor = 100000000;
  const int64_t position = getMPbalance(address, property, CONTRACT_BALANCE);
  int64_t valuePos = position * (uint64_t) sProperty.notional_size;

  if (valuePos < 0)  valuePos = -valuePos;

  position_obj.pushKV("position", FormatByType(position, 1));
  // position_obj.pushKV("liquidationPrice", FormatByType(liqPrice,2));
  position_obj.pushKV("valuePos", FormatByType(valuePos, 1));

  return true;

}

UniValue tl_getfullposition(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() != 2) {
    throw runtime_error(
			"tl_getfullposition \"address\" \"contractid\" \n"

			"\nReturns the full position for the future contract for a given address and property.\n"

			"\nArguments:\n"
			"1. address              (string, required) the address\n"
			"2. name or id           (string, required) the future contract name or identifier\n"

			"\nResult:\n"
			"{\n"
			"  \"symbol\" : \"n.nnnnnnnn\",   (string) short position of the address \n"
			"  \"notional_size\" : \"n.nnnnnnnn\"  (string) long position of the address\n"
			"  \"shortPosition\" : \"n.nnnnnnnn\",   (string) short position of the address \n"
			"  \"longPosition\" : \"n.nnnnnnnn\"  (string) long position of the address\n"
			"  \"liquidationPrice\" : \"n.nnnnnnnn\"  (string) long position of the address\n"
			"  \"positiveupnl\" : \"n.nnnnnnnn\",   (string) short position of the address \n"
			"  \"negativeupnl\" : \"n.nnnnnnnn\"  (string) long position of the address\n"
			"  \"positivepnl\" : \"n.nnnnnnnn\",   (string) short position of the address \n"
			"  \"negativepnl\" : \"n.nnnnnnnn\"  (string) long position of the address\n"
			"}\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_getfullposition", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\" \"1\"")
			+ HelpExampleRpc("tl_getfullposition", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\", \"1\"")
			);
  }

  const std::string address = ParseAddress(request.params[0]);
  uint32_t contractId  = ParseNameOrId(request.params[1]);

  // RequireContract(propertyId);

  UniValue positionObj(UniValue::VOBJ);

  CDInfo::Entry cd;
  {
    LOCK(cs_register);
    if (!_my_cds->getCD(contractId, cd)) {
      throw JSONRPCError(RPC_INVALID_PARAMETER, "Contract identifier does not exist");
    }
  }

  getFullContractRecord(address, contractId, positionObj, cd);

  return positionObj;
}


UniValue tl_getposition(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() != 2)
    throw runtime_error(
			"tl_getbalance \"address\" \"propertyid\" \n"

			"\nReturns the position for the future contract for a given address and property.\n"

			"\nArguments:\n"
			"1. address              (string, required) the address\n"
			"2. name or id           (string, required) the future contract name or id\n"

			"\nResult:\n"
			"{\n"
			"  \"shortPosition\" : \"n.nnnnnnnn\",   (string) short position of the address \n"
			"  \"longPosition\" : \"n.nnnnnnnn\"  (string) long position of the address\n"
			"}\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_getposition", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\" \"1\"")
			+ HelpExampleRpc("tl_getposition", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\", \"1\"")
			);

  const std::string address = ParseAddress(request.params[0]);
  uint32_t contractId = ParseNameOrId(request.params[1]);

  // RequireContract(contractId);

  UniValue balanceObj(UniValue::VOBJ);
  PositionToJSON(address, contractId, balanceObj);

  return balanceObj;
}

UniValue tl_getcontract_reserve(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "tl_getcotract_reserve \"address\" \"propertyid\" \n"

            "\nReturns the reserves contracts for the future contract for a given address and property.\n"

            "\nArguments:\n"
            "1. address              (string, required) the address\n"
            "2. name or id           (string, required) the future contract name or identifier\n"

            "\nResult:\n"
            "{\n"
            "  \"reserve\" : \"n.nnnnnnnn\",   (string) amount of contracts in reserve for the address \n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_getcotract_reserve", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\" \"1\"")
            + HelpExampleRpc("tl_getcotract_reserve", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\", \"1\"")
        );


    const std::string address = ParseAddress(request.params[0]);
    uint32_t contractId = ParseNameOrId(request.params[1]);

    // RequireContract(contractId);

    UniValue balanceObj(UniValue::VOBJ);
    int64_t reserve = getMPbalance(address, contractId, CONTRACTDEX_RESERVE);
    balanceObj.pushKV("contract reserve", FormatByType(reserve,2));
    return balanceObj;
}


UniValue tl_getorderbook(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw runtime_error(
            "tl_getorderbook propertyidA ( propertyidB )\n"

            "\nList active offers on the distributed token exchange.\n"

            "\nArguments:\n"
            "1. propertyidA           (number, required) filter orders by property identifier for sale\n"
            "2. propertyidB           (number, optional) filter orders by property identifier desired\n"

            "\nResult:\n"
            "[                                              (array of JSON objects)\n"
            "  {\n"
            "    \"address\" : \"address\",                         (string) the Bitcoin address of the trader\n"
            "    \"txid\" : \"hash\",                               (string) the hex-encoded hash of the transaction of the order\n"
            "    \"propertyidforsale\" : n,                       (number) the identifier of the tokens put up for sale\n"
            "    \"propertyidforsaleisdivisible\" : true|false,   (boolean) whether the tokens for sale are divisible\n"
            "    \"amountforsale\" : \"n.nnnnnnnn\",                (string) the amount of tokens initially offered\n"
            "    \"amountremaining\" : \"n.nnnnnnnn\",              (string) the amount of tokens still up for sale\n"
            "    \"propertyiddesired\" : n,                       (number) the identifier of the tokens desired in exchange\n"
            "    \"propertyiddesiredisdivisible\" : true|false,   (boolean) whether the desired tokens are divisible\n"
            "    \"amountdesired\" : \"n.nnnnnnnn\",                (string) the amount of tokens initially desired\n"
            "    \"amounttofill\" : \"n.nnnnnnnn\",                 (string) the amount of tokens still needed to fill the offer completely\n"
            "    \"action\" : n,                                  (number) the action of the transaction: (1) \"trade\", (2) \"cancel-price\", (3) \"cancel-pair\", (4) \"cancel-ecosystem\"\n"
            "    \"block\" : nnnnnn,                              (number) the index of the block that contains the transaction\n"
            "    \"blocktime\" : nnnnnnnnnn                       (number) the timestamp of the block that contains the transaction\n"
            "  },\n"
            "  ...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_getorderbook", "\"2\"")
            + HelpExampleRpc("tl_getorderbook", "\"2\"")
			    );


    bool filterDesired = (request.params.size() > 1);
    uint32_t propertyIdForSale = ParsePropertyId(request.params[0]);
    uint32_t propertyIdDesired = 0;

    RequireExistingProperty(propertyIdForSale);
    // RequireNotContract(propertyIdForSale);

    if (filterDesired) {
        propertyIdDesired = ParsePropertyId(request.params[1]);
        RequireExistingProperty(propertyIdDesired);
        // RequireNotContract(propertyIdDesired);
        RequireDifferentIds(propertyIdForSale, propertyIdDesired);
    }

    std::vector<CMPMetaDEx> vecMetaDexObjects;
    {
        LOCK(cs_tally);
        for (md_PropertiesMap::const_iterator my_it = metadex.begin(); my_it != metadex.end(); ++my_it) {
            const md_PricesMap& prices = my_it->second;
            for (md_PricesMap::const_iterator it = prices.begin(); it != prices.end(); ++it) {
                const md_Set& indexes = it->second;
                for (md_Set::const_iterator it = indexes.begin(); it != indexes.end(); ++it) {
                    const CMPMetaDEx& obj = *it;
                    if (obj.getProperty() != propertyIdForSale) continue;
                    if (!filterDesired || obj.getDesProperty() == propertyIdDesired)
                        vecMetaDexObjects.push_back(obj);
                }
            }
        }
    }

    UniValue response(UniValue::VARR);
    MetaDexObjectsToJSON(vecMetaDexObjects, response);
    return response;
}

UniValue tl_getcontract_orderbook(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() != 2)
    throw runtime_error(
			"tl_getcontract_orderbook \"contractid\" \"tradingaction\" \n"

			"\nList active offers on the distributed futures contracts exchange.\n"

			"\nArguments:\n"
			"1. name or id           (string, required) filter orders by contract name or identifier for sale\n"
			"2. tradingaction        (number, required) filter orders by trading action desired (Buy = 1, Sell = 2)\n"

			"\nResult:\n"
			"[                                              (array of JSON objects)\n"
			"  {\n"
			"    \"address\" : \"address\",                         (string) the Bitcoin address of the trader\n"
			"    \"txid\" : \"hash\",                               (string) the hex-encoded hash of the transaction of the order\n"
			"    \"propertyidforsale\" : n,                       (number) the identifier of the tokens put up for sale\n"
			"    \"propertyidforsaleisdivisible\" : true|false,   (boolean) whether the tokens for sale are divisible\n"
			"    \"amountforsale\" : \"n.nnnnnnnn\",                (string) the amount of tokens initially offered\n"
			"    \"amountremaining\" : \"n.nnnnnnnn\",              (string) the amount of tokens still up for sale\n"
			"    \"propertyiddesired\" : n,                       (number) the identifier of the tokens desired in exchange\n"
			"    \"propertyiddesiredisdivisible\" : true|false,   (boolean) whether the desired tokens are divisible\n"
			"    \"amountdesired\" : \"n.nnnnnnnn\",                (string) the amount of tokens initially desired\n"
			"    \"amounttofill\" : \"n.nnnnnnnn\",                 (string) the amount of tokens still needed to fill the offer completely\n"
			"    \"action\" : n,                                  (number) the action of the transaction: (1) \"trade\", (2) \"cancel-price\", (3) \"cancel-pair\", (4) \"cancel-ecosystem\"\n"
			"    \"block\" : nnnnnn,                              (number) the index of the block that contains the transaction\n"
			"    \"blocktime\" : nnnnnnnnnn                       (number) the timestamp of the block that contains the transaction\n"
			"  },\n"
			"  ...\n"
			"]\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_getcontract_orderbook", "\"2\"" "\"1\"")
			+ HelpExampleRpc("tl_getcontract_orderbook", "\"2\"" "\"1\"")
			);
      uint32_t contractId = ParseNameOrId(request.params[0]);
      uint8_t tradingaction = ParseContractDexAction(request.params[1]);

      std::vector<CMPContractDex> vecContractDexObjects;
      {
        LOCK(cs_tally);
        for (cd_PropertiesMap::const_iterator my_it = contractdex.begin(); my_it != contractdex.end(); ++my_it) {
          const cd_PricesMap& prices = my_it->second;
          for (cd_PricesMap::const_iterator it = prices.begin(); it != prices.end(); ++it) {
    	        const cd_Set& indexes = it->second;
    	        for (cd_Set::const_iterator it = indexes.begin(); it != indexes.end(); ++it) {
    	            const CMPContractDex& obj = *it;
    	            if (obj.getProperty() != contractId || obj.getTradingAction() != tradingaction || obj.getAmountForSale() == 0) continue;
    	            vecContractDexObjects.push_back(obj);
    	        }
           }
         }
      }

      UniValue response(UniValue::VARR);
      ContractDexObjectsToJSON(vecContractDexObjects, response);
      return response;
}

UniValue tl_gettradehistory(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() != 1)
    throw runtime_error(
			"tl_gettradehistory \"contractid\" \n"

			"\nRetrieves the history of trades on the distributed contract exchange for the specified market.\n"

			"\nArguments:\n"
			"1. name or id                                (string, required) the name of future contract\n"

			"\nResult:\n"
			"[                                      (array of JSON objects)\n"
			"  {\n"
			"    \"block\" : nnnnnn,                      (number) the index of the block that contains the trade match\n"
			"    \"unitprice\" : \"n.nnnnnnnnnnn...\" ,     (string) the unit price used to execute this trade (received/sold)\n"
			"    \"inverseprice\" : \"n.nnnnnnnnnnn...\",   (string) the inverse unit price (sold/received)\n"
			"    \"sellertxid\" : \"hash\",                 (string) the hash of the transaction of the seller\n"
			"    \"address\" : \"address\",                 (string) the Bitcoin address of the seller\n"
			"    \"amountsold\" : \"n.nnnnnnnn\",           (string) the number of tokens sold in this trade\n"
			"    \"amountreceived\" : \"n.nnnnnnnn\",       (string) the number of tokens traded in exchange\n"
			"    \"matchingtxid\" : \"hash\",               (string) the hash of the transaction that was matched against\n"
			"    \"matchingaddress\" : \"address\"          (string) the Bitcoin address of the other party of this trade\n"
			"  },\n"
			"  ...\n"
			"]\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_gettradehistory", "\"1\"")
			+ HelpExampleRpc("tl_gettradehistory", "\"1\"")
			);

  // obtain property identifiers for pair & check valid parameters
  uint32_t contractId = ParseNameOrId(request.params[0]);

  // RequireContract(contractId);

  // request pair trade history from trade db
  UniValue response(UniValue::VARR);

  LOCK(cs_tally);

  t_tradelistdb->getMatchingTrades(contractId, response);

  return response;
}

UniValue tl_gettradehistory_unfiltered(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "tl_gettradehistory_unfiltered \"contractid\" \n"

            "\nRetrieves the history of trades on the distributed contract exchange for the specified market.\n"

            "\nArguments:\n"
            "1. name or id                          (string, required) the name (or id) of future contract\n"

            "\nResult:\n"
            "[                                      (array of JSON objects)\n"
            "  {\n"
            "    \"block\" : nnnnnn,                      (number) the index of the block that contains the trade match\n"
            "    \"unitprice\" : \"n.nnnnnnnnnnn...\" ,     (string) the unit price used to execute this trade (received/sold)\n"
            "    \"inverseprice\" : \"n.nnnnnnnnnnn...\",   (string) the inverse unit price (sold/received)\n"
            "    \"sellertxid\" : \"hash\",                 (string) the hash of the transaction of the seller\n"
            "    \"address\" : \"address\",                 (string) the Bitcoin address of the seller\n"
            "    \"amountsold\" : \"n.nnnnnnnn\",           (string) the number of tokens sold in this trade\n"
            "    \"amountreceived\" : \"n.nnnnnnnn\",       (string) the number of tokens traded in exchange\n"
            "    \"matchingtxid\" : \"hash\",               (string) the hash of the transaction that was matched against\n"
            "    \"matchingaddress\" : \"address\"          (string) the Bitcoin address of the other party of this trade\n"
            "  },\n"
            "  ...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_gettradehistory_unfiltered", "\"1\"")
            + HelpExampleRpc("tl_gettradehistoryforpair", "\"1\"")
			    );

    // obtain property identifiers for pair & check valid parameters
    uint32_t contractId = ParseNameOrId(request.params[0]);

    // RequireContract(contractId);

    // request pair trade history from trade db
    UniValue response(UniValue::VARR);
    LOCK(cs_tally);

    // gets unfiltered list of trades
    t_tradelistdb->getMatchingTradesUnfiltered(contractId,response);
    return response;
}

UniValue tl_getpeggedhistory(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "tl_getpeggedhistory \"propertyid\" \n"

            "\nRetrieves the history of amounts created for a given pegged .\n"

            "\nArguments:\n"
            "1. propertyid           (number, required) the id of pegged currency\n"

            "\nResult:\n"
            "[                                      (array of JSON objects)\n"
            "  {\n"
            "    \"block\" : nnnnnn,                      (number) the index of the block that contains the trade match\n"
            "    \"unitprice\" : \"n.nnnnnnnnnnn...\" ,     (string) the unit price used to execute this trade (received/sold)\n"
            "    \"inverseprice\" : \"n.nnnnnnnnnnn...\",   (string) the inverse unit price (sold/received)\n"
            "    \"sellertxid\" : \"hash\",                 (string) the hash of the transaction of the seller\n"
            "    \"address\" : \"address\",                 (string) the Bitcoin address of the seller\n"
            "    \"amountsold\" : \"n.nnnnnnnn\",           (string) the number of tokens sold in this trade\n"
            "    \"amountreceived\" : \"n.nnnnnnnn\",       (string) the number of tokens traded in exchange\n"
            "    \"matchingtxid\" : \"hash\",               (string) the hash of the transaction that was matched against\n"
            "    \"matchingaddress\" : \"address\"          (string) the Bitcoin address of the other party of this trade\n"
            "  },\n"
            "  ...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_getpeggedhistory", "\"1\"")
            + HelpExampleRpc("tl_getpeggedhistory", "\"1\"")
        );

    // obtain property identifiers for pair & check valid parameters
    uint32_t propertyId = ParsePropertyId(request.params[0]);

    RequirePeggedCurrency(propertyId);

    // request pair trade history from trade db
    UniValue response(UniValue::VARR);

    LOCK(cs_tally);

    t_tradelistdb->getCreatedPegged(propertyId,response);

    return response;
}

UniValue tl_getallprice(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw runtime_error(
            "tl_getallprice \n"

            "\nRetrieves the ALL last price on DEx and Trade Channels (LTC) .\n"

            "\nResult:\n"
            "[                                      (array of JSON objects)\n"
            "  {\n"
            "    \"price\" : nnnnnn,                (number) the price of 1 ALL in LTC\n"
            "  },\n"
            "  ...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_getallprice", "")
            + HelpExampleRpc("tl_getallprice", "")
        );

    UniValue balanceObj(UniValue::VOBJ);

    // get token Price
    int64_t allPrice = 0;
    const uint32_t propertyId = static_cast<uint32_t>(ALL);

    auto it = lastPrice.find(propertyId);
    if(it != lastPrice.end()) {
        allPrice = it->second;
    }

    balanceObj.pushKV("unitprice", FormatDivisibleMP((uint64_t)allPrice));
    return balanceObj;
}

UniValue tl_getupnl(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() < 2 || request.params.size() > 3) {
    throw runtime_error(
			"tl_getupnl \"address\" \"name(or id)\" \"verbose\" \n"

			"\nRetrieves the unrealized PNL for trades on the distributed contract exchange for the specified market.\n"

			"\nArguments:\n"
			"1. address           (string, required) address of owner\n"
			"2. name or id        (string, required) the name (or id number) of future contract\n"
      "3. verbose           (number, optional) 1 : if you need the list of matched trades\n"

			"\nResult:\n"
			"[                                      (array of JSON objects)\n"
			"  {\n"
			"    \"event 1 \"        : nnnnnn,                 (number) the index of the block that contains the trade match\n"
			"    \"matched address\" : \"n.nnnnnnnnnnn...\" ,  (string) the unit price used to execute this trade (received/sold)\n"
			"    \"price\"           : \"n.nnnnnnnnnnn...\",   (string) the inverse unit price (sold/received)\n"
      "    \"amount  \"        : \"n.nnnnnnnnnnn...\",   (number) the inverse unit price (sold/received)\n"
      "    \"leverage  \"      : \"n.nnnnnnnnnnn...\",   (number) the inverse unit price (sold/received)\n"
			"  ...\n"
			"  ...\n"
      "   }\n"
			"]\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_getupnl", "\"13framr31ziZmV8HLjE5Gs8KRVw894P2eT\" , \"Contract 1\", \"1\"")
			+ HelpExampleRpc("tl_getupnl", "\"13framr31ziZmV8HLjE5Gs8KRVw894P2eT\" , \"Contract 1\", \"1\"")
			);
  }

  const std::string address = ParseAddress(request.params[0]);
  uint32_t contractId = ParseNameOrId(request.params[1]);
  bool showVerbose = (request.params.size() > 2 && ParseBinary(request.params[2]) == 1) ? true : false;


  RequireExistingProperty(contractId);
  // RequireContract(contractId);

  // if position is 0, upnl is 0
  RequirePosition(address, contractId);

  // request trade history from trade db
  UniValue response(UniValue::VARR);

  t_tradelistdb->getUpnInfo(address, contractId, response, showVerbose);


  return response;
}

UniValue tl_getpnl(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() != 2)
    throw runtime_error(
			"tl_getpnl \"address\" \"contractid\" \n"

			"\nRetrieves the realized PNL for trades on the distributed contract exchange for the specified market.\n"

			"\nArguments:\n"
			"1. address           (string, required) address of owner\n"
			"2. contractid           (number, required) the id of future contract\n"

			"\nResult:\n"
			"[                                      (array of JSON objects)\n"
			"  {\n"
			"    \"block\" : nnnnnn,                      (number) the index of the block that contains the trade match\n"
			"    \"unitprice\" : \"n.nnnnnnnnnnn...\" ,     (string) the unit price used to execute this trade (received/sold)\n"
			"    \"inverseprice\" : \"n.nnnnnnnnnnn...\",   (string) the inverse unit price (sold/received)\n"
			"    \"sellertxid\" : \"hash\",                 (string) the hash of the transaction of the seller\n"
			"    \"address\" : \"address\",                 (string) the Bitcoin address of the seller\n"
			"    \"amountsold\" : \"n.nnnnnnnn\",           (string) the number of tokens sold in this trade\n"
			"    \"amountreceived\" : \"n.nnnnnnnn\",       (string) the number of tokens traded in exchange\n"
			"    \"matchingtxid\" : \"hash\",               (string) the hash of the transaction that was matched against\n"
			"    \"matchingaddress\" : \"address\"          (string) the Bitcoin address of the other party of this trade\n"
			"  },\n"
			"  ...\n"
			"]\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_getpnl", "\"13framr31ziZmV8HLjE5Gs8KRVw894P2eT\" \"12\" ")
			+ HelpExampleRpc("tl_getpnl", "\"13framr31ziZmV8HLjE5Gs8KRVw894P2eT\", \"500\"")
			);

  const std::string address = ParseAddress(request.params[0]);
  uint32_t contractId = ParsePropertyId(request.params[1]);

  RequireExistingProperty(contractId);

  UniValue balanceObj(UniValue::VOBJ);

  // note: we need to retrieve last realized pnl
  int64_t upnl  = 0;
  int64_t nupnl  = 0;

  if (upnl > 0 && nupnl == 0) {
    balanceObj.pushKV("positivepnl", FormatByType(static_cast<uint64_t>(upnl),2));
    balanceObj.pushKV("negativepnl", FormatByType(0,2));
  } else if (nupnl > 0 && upnl == 0) {
    balanceObj.pushKV("positivepnl", FormatByType(0,2));
    balanceObj.pushKV("negativepnl", FormatByType(static_cast<uint64_t>(nupnl),2));
  } else{
    balanceObj.pushKV("positivepnl", FormatByType(0,2));
    balanceObj.pushKV("negativepnl", FormatByType(0,2));
  }
  return balanceObj;
}

UniValue tl_getmarketprice(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() != 1)
    throw runtime_error(
			"tl_getmarketprice \"address\" \"contractid\" \n"

			"\nRetrieves the last match price on the distributed contract exchange for the specified market.\n"

			"\nArguments:\n"
			"1. contractid           (number, required) the id of future contract\n"

			"\nResult:\n"
			"  {\n"
			"    \"block\" : nnnnnn,                      (number) the index of the block that contains the trade match\n"
			"    \"unitprice\" : \"n.nnnnnnnnnnn...\" ,   (string) the unit price used to execute this trade (received/sold)\n"
			"    \"price\" : \"n.nnnnnnnnnnn...\",        (string) the inverse unit price (sold/received)\n"
			"  ...\n"
			" }\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_getmarketprice", "\"12\"" )
			+ HelpExampleRpc("tl_getmarketprice", "\"500\"")
			);


  uint32_t contractId = ParsePropertyId(request.params[0]);

  RequireExistingProperty(contractId);
  // RequireContract(contractId);

  UniValue balanceObj(UniValue::VOBJ);
  // int index = static_cast<unsigned int>(contractId);
  // int64_t price = marketP[index];
  int64_t price = 0;

  balanceObj.pushKV("price", FormatByType(price,2));

  return balanceObj;
}

UniValue tl_getactivedexsells(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw runtime_error(
            "tl_getactivedexsells \"address\" \n"

            "\nReturns currently active offers on the distributed exchange.\n"

            "\nArguments:\n"
            "1. address              (string, optional) address filter (default: include any)\n"

            "\nResult:\n"
            "[                                   (array of JSON objects)\n"
            "  {\n"
            "    \"txid\" : \"hash\",                    (string) the hash of the transaction of this offer\n"
            "    \"propertyid\" : n,                   (number) the identifier of the tokens for sale\n"
            "    \"seller\" : \"address\",               (string) the Bitcoin address of the seller\n"
            "    \"amountavailable\" : \"n.nnnnnnnn\",   (string) the number of tokens still listed for sale and currently available\n"
            "    \"bitcoindesired\" : \"n.nnnnnnnn\",    (string) the number of bitcoins desired in exchange\n"
            "    \"unitprice\" : \"n.nnnnnnnn\" ,        (string) the unit price (LTC/token)\n"
            "    \"timelimit\" : nn,                   (number) the time limit in blocks a buyer has to pay following a successful accept\n"
            "    \"minimumfee\" : \"n.nnnnnnnn\",        (string) the minimum mining fee a buyer has to pay to accept this offer\n"
            "    \"amountaccepted\" : \"n.nnnnnnnn\",    (string) the number of tokens currently reserved for pending \"accept\" orders\n"
            "    \"accepts\": [                        (array of JSON objects) a list of pending \"accept\" orders\n"
            "      {\n"
            "        \"buyer\" : \"address\",                (string) the Bitcoin address of the buyer\n"
            "        \"block\" : nnnnnn,                   (number) the index of the block that contains the \"accept\" order\n"
            "        \"blocksleft\" : nn,                  (number) the number of blocks left to pay\n"
            "        \"amount\" : \"n.nnnnnnnn\"             (string) the amount of tokens accepted and reserved\n"
            "        \"amounttopay\" : \"n.nnnnnnnn\"        (string) the amount in bitcoins needed finalize the trade\n"
            "      },\n"
            "      ...\n"
            "    ]\n"
            "  },\n"
            "  ...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_getactivedexsells", "\"13framr31ziZmV8HLjE5Gs8KRVw894P2eT\"")
            + HelpExampleRpc("tl_getactivedexsells", "\"13framr31ziZmV8HLjE5Gs8KRVw894P2eT\"")
        );

    std::string addressFilter;  //TODO: Fix amountavalable when the maker are selling  (we need invert this) "amountavailable": "10.00000000",
                                //                                                                           "amountoffered": "90.00000000",

    if (request.params.size() > 0) {
        addressFilter = ParseAddressOrEmpty(request.params[0]);
    }

    UniValue response(UniValue::VARR);

    int curBlock = GetHeight();

    LOCK(cs_tally);

    for (OfferMap::iterator it = my_offers.begin(); it != my_offers.end(); ++it) {
        const CMPOffer& offer = it->second;
        const std::string& sellCombo = it->first;
        std::string seller = sellCombo.substr(0, sellCombo.size() - 2);

        // filtering
        if (!addressFilter.empty() && seller != addressFilter) continue;

        std::string txid = offer.getHash().GetHex();
        uint32_t propertyId = offer.getProperty();
        int64_t minFee = offer.getMinFee();
        uint8_t timeLimit = offer.getBlockTimeLimit();
        int64_t sellOfferAmount = offer.getOfferAmountOriginal(); //badly named - "Original" implies off the wire, but is amended amount
        int64_t sellLitecoinDesired = offer.getLTCDesiredOriginal(); //badly named - "Original" implies off the wire, but is amended amount
        int64_t amountAvailable = getMPbalance(seller, propertyId, SELLOFFER_RESERVE);
        int64_t amountAccepted = getMPbalance(seller, propertyId, ACCEPT_RESERVE);
        uint8_t option = offer.getOption();

        // calculate unit price and updated amount of bitcoin desired
        arith_uint256 aUnitPrice = 0;
        if ((sellOfferAmount > 0) && (sellLitecoinDesired > 0)) {
            aUnitPrice = (ConvertTo256(COIN) * (ConvertTo256(sellLitecoinDesired)) / ConvertTo256(sellOfferAmount)) ; // divide by zero protection
        }

        int64_t unitPrice = (isPropertyDivisible(propertyId)) ? ConvertTo64(aUnitPrice) : ConvertTo64(aUnitPrice) / COIN;

        int64_t bitcoinDesired = calculateDesiredLTC(sellOfferAmount, sellLitecoinDesired, amountAvailable);
        int64_t sumAccepted = 0;
        int64_t sumLtcs = 0;
        UniValue acceptsMatched(UniValue::VARR);
        for (AcceptMap::const_iterator ait = my_accepts.begin(); ait != my_accepts.end(); ++ait) {
            UniValue matchedAccept(UniValue::VOBJ);
            const CMPAccept& accept = ait->second;
            const std::string& acceptCombo = ait->first;

            // does this accept match the sell?
            if (accept.getHash() == offer.getHash()) {
                // split acceptCombo out to get the buyer address
                std::string buyer = acceptCombo.substr((acceptCombo.find("+") + 1), (acceptCombo.size()-(acceptCombo.find("+") + 1)));
                int blockOfAccept = accept.getAcceptBlock();
                int blocksLeftToPay = (blockOfAccept + offer.getBlockTimeLimit()) - curBlock;
                int64_t amountAccepted = accept.getAcceptAmountRemaining();
                // TODO: don't recalculate!

                int64_t amountToPayInLTC = calculateDesiredLTC(accept.getOfferAmountOriginal(), accept.getLTCDesiredOriginal(), amountAccepted);
                if (option == 1) {
                    sumAccepted += amountAccepted;
                    uint64_t ltcsreceived = rounduint64(unitPrice * amountAccepted / COIN);
                    sumLtcs += ltcsreceived;
                    matchedAccept.pushKV("seller", buyer);
                    matchedAccept.pushKV("amount", FormatMP(propertyId, amountAccepted));
                    matchedAccept.pushKV("ltcstoreceive", FormatDivisibleMP(ltcsreceived));
                } else if (option == 2) {
                    matchedAccept.pushKV("buyer", buyer);
                    matchedAccept.pushKV("amountdesired", FormatMP(propertyId, amountAccepted));
                    matchedAccept.pushKV("ltcstopay", FormatDivisibleMP(amountToPayInLTC));
                }

                matchedAccept.pushKV("block", blockOfAccept);
                matchedAccept.pushKV("blocksleft", blocksLeftToPay);
                acceptsMatched.push_back(matchedAccept);
            }
        }

        UniValue responseObj(UniValue::VOBJ);
        responseObj.pushKV("txid", txid);
        responseObj.pushKV("propertyid", (uint64_t) propertyId);
        responseObj.pushKV("action", (uint64_t) offer.getOption());
        if (option == 2) {
            responseObj.pushKV("seller", seller);
            responseObj.pushKV("ltcsdesired", FormatDivisibleMP(bitcoinDesired));
            responseObj.pushKV("amountavailable", FormatMP(propertyId, amountAvailable));

        } else if (option == 1){
            responseObj.pushKV("buyer", seller);
            responseObj.pushKV("ltcstopay", FormatDivisibleMP(sellLitecoinDesired - sumLtcs));
            responseObj.pushKV("amountdesired", FormatMP(propertyId, sellOfferAmount - sumAccepted));
            responseObj.pushKV("accepted", FormatMP(propertyId, amountAccepted));

        }
        responseObj.pushKV("unitprice", FormatDivisibleMP(unitPrice));
        responseObj.pushKV("timelimit", timeLimit);
        responseObj.pushKV("minimumfee", FormatDivisibleMP(minFee));
        responseObj.pushKV("accepts", acceptsMatched);

        // add sell object into response array
        response.push_back(responseObj);
    }

    return response;
}


UniValue tl_getsum_upnl(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() != 1)
    throw runtime_error(
			"tl_getsum_upnl \"address\" \n"

			"\nRetrieve the global upnl for an address.\n"

			"\nArguments:\n"
			"1. contractid           (number, required) the id of future contract\n"

			"\nResult:\n"
			"[                                      (array of JSON objects)\n"
			"  {\n"
			"    \"upnl\" : nnnnnn,                      (number) global unrealized profit and loss\n"
			"  ...\n"
			"]\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_getsum_upnl", "\"1\"" )
			+ HelpExampleRpc("tl_getsum_upnl", "\"2\"")
    );

  const std::string address = ParseAddress(request.params[0]);

  UniValue balanceObj(UniValue::VOBJ);

  auto it = sum_upnls.find(address);
  int64_t upnl = (it != sum_upnls.end()) ? it->second : 0;

  balanceObj.pushKV("upnl", FormatByType(upnl,2));

  return balanceObj;
}

UniValue tl_check_commits(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() != 1)
    throw runtime_error(
			"tl_check_commits \"sender\" \"address\" \n"

			"\nRetrieves the history of commits in the channel\n"

			"\nArguments:\n"
			"1. sender address                      (string, required) the multisig address of channel\n"

			"\nResult:\n"
			"[                                      (array of JSON objects)\n"
			"  {\n"
			"    \"sender\" : address,                        (string) the Litecoin address of sender\n"
      "    \"channel\" : address,                       (string) the Litecoin multisig channel address\n"
			"    \"propertyId\" : \"id\" ,                    (string) the property id of token commited\n"
			"    \"amount\" : \"n.nnnnnnnnnnn...\",           (string) the amount commited\n"
			"    \"block\" : \"block\",                       (number) the block of commit\n"
			"    \"block_index\" : \"index\",                 (number) the index of the block that contains the trade match\n"
			"  ...\n"
			"]\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_check_commits", "\"13framr31ziZmV8HLjE5Gs8KRVw894P2eT\"")
			+ HelpExampleRpc("tl_check_commits", "\"13framr31ziZmV8HLjE5Gs8KRVw894P2eT\"")
			);

  // obtain property identifiers for pair & check valid parameters
  const std::string address = ParseAddress(request.params[0]);

  // RequireContract(contractId);

  // request pair trade history from trade db
  UniValue response(UniValue::VARR);

  LOCK(cs_tally);

  t_tradelistdb->getAllCommits(address, response);

  return response;
}

UniValue tl_check_withdrawals(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() != 1)
    throw runtime_error(
			"tl_check_withdrawals \"senderAddress\" \n"

			"\nRetrieves the history of withdrawal for a given address in the channel\n"

			"\nArguments:\n"
			"1. sender address                      (string, required) the address related to withdrawals on the channel\n"

			"\nResult:\n"
			"[                                      (array of JSON objects)\n"
			"  {\n"
			"    \"block\" : nnnnnn,                      (number) the index of the block that contains the trade match\n"
			"    \"unitprice\" : \"n.nnnnnnnnnnn...\" ,     (string) the unit price used to execute this trade (received/sold)\n"
			"    \"inverseprice\" : \"n.nnnnnnnnnnn...\",   (string) the inverse unit price (sold/received)\n"
			"    \"sellertxid\" : \"hash\",                 (string) the hash of the transaction of the seller\n"
			"    \"address\" : \"address\",                 (string) the Bitcoin address of the seller\n"
			"    \"amountsold\" : \"n.nnnnnnnn\",           (string) the number of tokens sold in this trade\n"
			"    \"amountreceived\" : \"n.nnnnnnnn\",       (string) the number of tokens traded in exchange\n"
			"    \"matchingtxid\" : \"hash\",               (string) the hash of the transaction that was matched against\n"
			"    \"matchingaddress\" : \"address\"          (string) the Bitcoin address of the other party of this trade\n"
			"  },\n"
			"  ...\n"
			"]\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_check_withdrawals", "\"13framr31ziZmV8HLjE5Gs8KRVw894P2eT\"")
			+ HelpExampleRpc("tl_check_withdrawals", "\"13framr31ziZmV8HLjE5Gs8KRVw894P2eT\"")
			);

  // obtain property identifiers for pair & check valid parameters
  const std::string address = ParseAddress(request.params[0]);

  // request pair trade history from trade db
  UniValue response(UniValue::VARR);

  LOCK(cs_tally);

  t_tradelistdb->getAllWithdrawals(address, response);

  return response;
}

UniValue tl_check_kyc(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() != 1)
    throw runtime_error(
			"tl_check_kyc \"senderaddress\" \n"

			"\nKYC check for given address \n"

			"\nArguments:\n"
			"1. address                             (string, required) the address registered\n"

			"\nResult:\n"
			"[                                      (array of JSON objects)\n"
			"  {\n"
			"    \"result\" : nnnnnn,               (string) enabled or disabled as notary address\n"
			"  },\n"
			"  ...\n"
			"]\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_check_kyc", "\"13framr31ziZmV8HLjE5Gs8KRVw894P2eT\"")
			+ HelpExampleRpc("tl_check_kyc", "\"13framr31ziZmV8HLjE5Gs8KRVw894P2eT\"")
			);

  // obtain property identifiers for pair & check valid parameters
  const std::string address = ParseAddress(request.params[0]);
  // int registered = static_cast<int>(ParseNewValues(request.params[1]));
  // RequireContract(contractId);

  int kyc_id;

  const std::string result = (t_tradelistdb->checkAttestationReg(address, kyc_id)) ? "enabled(kyc_0)" : (!t_tradelistdb->checkKYCRegister(address, kyc_id)) ? "disabled" : "enabled";

  UniValue balanceObj(UniValue::VOBJ);
  balanceObj.pushKV("result: ", result);
  return balanceObj;

}

UniValue tl_getcache(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "tl_getmargin \"address\" \"propertyid\" \n"

            "\nReturns the fee cache for a given property.\n"

            "\nArguments:\n"
            "1. collateral                   (number, required) the contract collateral\n"

            "\nResult:\n"
            "{\n"
            "  \"amount\" : \"n.nnnnnnnn\",  (number) the available balance in the cache for the property\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_getcache", "\"2\"")
            + HelpExampleRpc("tl_getcache", "\"2\"")
        );

    uint32_t propertyId = ParsePropertyId(request.params[0]);

    // geting data from cache!
    auto it =  cachefees.find(propertyId);
    int64_t amount = (it != cachefees.end())? it->second : 0;

    UniValue balanceObj(UniValue::VOBJ);

    balanceObj.pushKV("amount", FormatByType(amount, 2));

    return balanceObj;
}

UniValue tl_getoraclecache(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "tl_getoraclecache \"address\" \"propertyid\" \n"

            "\nReturns the oracle fee cache for a given property.\n"

            "\nArguments:\n"
            "1. collateral                   (number, required) the contract collateral\n"

            "\nResult:\n"
            "{\n"
            "  \"amount\" : \"n.nnnnnnnn\",  (number) the available balance in the oracle cache for the property\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_getcache", "\"1\"")
            + HelpExampleRpc("tl_getcache", "\"1\"")
        );

    uint32_t propertyId = ParsePropertyId(request.params[0]);

    // geting data from cache!
    auto it =  cachefees_oracles.find(propertyId);
    int64_t amount = (it != cachefees_oracles.end()) ? it->second : 0;

    UniValue balanceObj(UniValue::VOBJ);

    balanceObj.pushKV("amount", FormatByType(amount,2));

    return balanceObj;
}

UniValue tl_getalltxonblock(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
      throw runtime_error(
			"tl_getalltxonblock \"block\" \n"

			"\nGet detailed information about every Trade Layer transaction in block.\n"

	    "\nArguments:\n"
	    "1. block                 (number, required) the block height \n"

	    "\nResult:\n"
	    "{\n"
	    "  \"txid\" : \"hash\",                  (string) the hex-encoded hash of the transaction\n"
	    "  \"sendingaddress\" : \"address\",     (string) the Bitcoin address of the sender\n"
	    "  \"referenceaddress\" : \"address\",   (string) a Bitcoin address used as reference (if any)\n"
	    "  \"ismine\" : true|false,            (boolean) whether the transaction involes an address in the wallet\n"
	    "  \"confirmations\" : nnnnnnnnnn,     (number) the number of transaction confirmations\n"
	    "  \"fee\" : \"n.nnnnnnnn\",             (string) the transaction fee in bitcoins\n"
	    "  \"blocktime\" : nnnnnnnnnn,         (number) the timestamp of the block that contains the transaction\n"
	    "  \"valid\" : true|false,             (boolean) whether the transaction is valid\n"
	    "  \"version\" : n,                    (number) the transaction version\n"
	    "  \"type_int\" : n,                   (number) the transaction type as number\n"
	    "  \"type\" : \"type\",                  (string) the transaction type as string\n"
	    "  [...]                             (mixed) other transaction type specific properties\n"
	    "}\n"

	    "\nbExamples:\n"
	    + HelpExampleCli("tl_getalltxonblock", "\"52021\"")
	    + HelpExampleRpc("tl_getalltxonblock", "\"54215\"")
	    );

    UniValue response(UniValue::VARR);

    int blockHeight = request.params[0].get_int();

    // next let's obtain the block for this height
    CBlock block;
    {
        LOCK(cs_main);
        CBlockIndex* pBlockIndex = chainActive[blockHeight];

        if (!ReadBlockFromDisk(block, pBlockIndex, Params().GetConsensus()))
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to read block from disk");
    }

    // now we want to loop through each of the transactions in the block and run against CMPTxList::exists
    // those that return positive add to our response array

    LOCK(cs_tally);

    for(const auto tx : block.vtx)
    {
        if (p_txlistdb->exists(tx->GetHash()))
        {
             UniValue txobj(UniValue::VOBJ);
             int populateResult = populateRPCTransactionObject(tx->GetHash(), txobj);
             if (populateResult != 0) PopulateFailure(populateResult);
             response.push_back(txobj);
         }
    }

    return response;
}

UniValue tl_get_ltcvolume(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 2)
        throw runtime_error(
            "tl_get_ltcvolume \"propertyid\" \"blockA\" \"blockB\" \n"

            "\nReturns the LTC volume for DEx and Trade Channels, in sort amount of blocks.\n"

            "\nArguments:\n"
            "1. property                 (number, required) property \n"
            "2. first block              (number, required) older limit block\n"
            "3. second block             (number, required) newer limit block\n"

            "\nResult:\n"
            "{\n"
            "  \"supply\" : \"n.nnnnnnnn\",   (number) the LTC volume traded \n"
            "  \"blockheight\" : \"n.\",      (number) last block\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_get_ltcvolume", "\"1\" \"200\" \"300\"")
            + HelpExampleRpc("tl_get_ltcvolume", "\"1\", \"200\", \"300\"")
        );

    uint32_t propertyId = ParsePropertyId(request.params[0]);
    int fblock = request.params[1].get_int();
    int sblock = request.params[2].get_int();

    if (fblock == 0 || sblock == 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Block must be greater than 0");

    if (sblock < fblock)
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Second block must be greater than first");

    // geting data from map!
    int64_t amount = mastercore::LtcVolumen(propertyId, fblock, sblock);

    UniValue balanceObj(UniValue::VOBJ);

    balanceObj.pushKV("volume", FormatDivisibleMP(amount));
    balanceObj.pushKV("blockheigh", FormatIndivisibleMP(GetHeight()));

    return balanceObj;
}

UniValue tl_getmdexvolume(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 3)
        throw runtime_error(
            "tl_getmdexvolume \"property\" \"blockA\" \"blockB\" \n"

            "\nReturns the Token volume traded in sort amount of blocks.\n"

            "\nArguments:\n"
            "1. propertyid                (number, required) the property id \n"
            "2. first block               (number, required) older limit block\n"
            "3. second block              (number, optional) newer limit block\n"

            "\nResult:\n"
            "{\n"
            "  \"volume\" : \"n.nnnnnnnn\",   (number) the available volume (of property) traded\n"
            "  \"blockheight\" : \"n.\",      (number) last block\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_getmdexvolume", "\"1\" \"200\" \"300\"")
            + HelpExampleRpc("tl_getmdexvolume", "\"1\",\"200\", \"300\"")
        );

    uint32_t property = ParsePropertyId(request.params[0]);
    int fblock = request.params[1].get_int();
    int sblock = request.params[2].get_int();

    if (fblock == 0 || sblock == 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Block must be greater than 0");

    if (sblock < fblock)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Second block must be greater than first");

    // geting data from map!
    int64_t amount = mastercore::MdexVolumen(property,fblock, sblock);

    UniValue balanceObj(UniValue::VOBJ);

    balanceObj.pushKV("volume", FormatDivisibleMP(amount));
    balanceObj.pushKV("blockheigh", FormatIndivisibleMP(GetHeight()));

    return balanceObj;
}

UniValue tl_getcurrencytotal(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "tl_getcurrencyvolume \"propertyid\" \n"

            "\nGet total outstanding supply for token \n"

            "\nArguments:\n"
            "1. propertyId                 (number, required) propertyId of token \n"

            "\nResult:\n"
            "{\n"
            "  \"total:\" : \"n.nnnnnnnn\",   (number) the amount created for token\n"
            "  \"blockheight\" : \"n.\",      (number) last block\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_getcurrencytotal", "\"1\"")
            + HelpExampleRpc("tl_getcurrencytotal", "\"1\"")
        );

    uint32_t propertyId = ParsePropertyId(request.params[0]);

    // RequireNotContract(propertyId);

    CMPSPInfo::Entry sp;
    {
        LOCK(cs_tally);
        if (!_my_sps->getSP(propertyId, sp)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Property identifier does not exist");
        }
    }

    // geting data !
    int64_t amount = sp.num_tokens;

    UniValue balanceObj(UniValue::VOBJ);

    balanceObj.pushKV("total", FormatDivisibleMP(amount));
    balanceObj.pushKV("blockheigh", FormatIndivisibleMP(GetHeight()));

    return balanceObj;
}

UniValue tl_listkyc(const JSONRPCRequest& request)
{
    if (request.fHelp)
      throw runtime_error(
			  "tl_listkyc\n"

			  "\nLists all kyc registers.\n"

			  "\nResult:\n"
			  "[                                (array of JSON objects)\n"
			  "  {\n"
			  "    \"propertyid\" : n,                (number) the identifier of the tokens\n"
			  "    \"name\" : \"name\",                 (string) the name of the tokens\n"
			  "    \"data\" : \"information\",          (string) additional information or a description\n"
			  "    \"url\" : \"uri\",                   (string) an URI, for example pointing to a website\n"
			  "    \"divisible\" : true|false         (boolean) whether the tokens are divisible\n"
			  "  },\n"
			  "  ...\n"
			  "]\n"

			  "\nExamples:\n"
			  + HelpExampleCli("tl_listkyc", "")
			  + HelpExampleRpc("tl_listkyc", "")
		  );

    UniValue response(UniValue::VARR);

    t_tradelistdb->kycLoop(response);

    return response;
}

UniValue tl_list_attestation(const JSONRPCRequest& request)
{
  if (request.fHelp)
    throw runtime_error(
			"tl_list_attestation\n"

			"\nLists all kyc attestations.\n"

			"\nResult:\n"
			"[                                (array of JSON objects)\n"
			"  {\n"
			"    \"propertyid\" : n,                (number) the identifier of the tokens\n"
			"    \"name\" : \"name\",                 (string) the name of the tokens\n"
			"    \"data\" : \"information\",          (string) additional information or a description\n"
			"    \"url\" : \"uri\",                   (string) an URI, for example pointing to a website\n"
			"    \"divisible\" : true|false         (boolean) whether the tokens are divisible\n"
			"  },\n"
			"  ...\n"
			"]\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_list_attestation", "")
			+ HelpExampleRpc("tl_list_attestation", "")
			);

  UniValue response(UniValue::VARR);

  t_tradelistdb->attLoop(response);

  return response;
}

UniValue tl_getopen_interest(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
      throw runtime_error(
			  "tl_getopen_interest \"name(or id)\" \n"

			  "\nAmounts of live contracts.\n"

        "\nArguments:\n"
        "1. name or id                    (string, required) name (or id number) of the future contract \n"


			  "\nResult:\n"
			  "[                                (array of JSON objects)\n"
			  "  {\n"
			  "    \"total live contracts\" : n      (number) total amount of contracts lives\n"
			  "  },\n"
			  "  ...\n"
			  "]\n"

			  "\nExamples:\n"
			  + HelpExampleCli("tl_getopen_interest", "\"Contract 1\"")
			  + HelpExampleRpc("tl_getopen_interest", "\"Contract 1\"")
		  );

    uint32_t contractId = ParseNameOrId(request.params[0]);

    PrintToLog("%s(): contractId: %d \n",__func__, contractId);

    UniValue amountObj(UniValue::VOBJ);
    int64_t totalContracts = mastercore::getTotalLives(contractId);

    amountObj.pushKV("totalLives", totalContracts);

    return amountObj;
}

//tl_getmax_peggedcurrency
//input : JSONRPCREquest which contains: 1)address of creator, 2) contract ID which is collaterilized in ALL
//return: UniValue which is JSON object that is max pegged currency you can create
UniValue tl_getmax_peggedcurrency(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
      throw runtime_error(
			  "tl_getmax_peggedcurrency \"fromaddress\" \"name (or id)\""

			  "\nGet max pegged currency address can create\n"

			  "\n arguments: \n"
			  "\n 1) fromaddress (string, required) the address to send from\n"
			  "\n 2) name of contract requiered \n"
			);
    std::string fromAddress = ParseAddress(request.params[0]);
    uint32_t contractId = ParseNameOrId(request.params[1]);

    CDInfo::Entry cd;
    assert(_my_cds->getCD(contractId, cd));
  // Get available ALL because dCurrency is a hedge of ALL and ALL denominated Short Contracts
  // obtain parameters & info

  int64_t tokenBalance = getMPbalance(fromAddress, ALL, BALANCE);

  // get token Price
  int64_t tokenPrice = 0;
  auto it = market_priceMap.find(cd.collateral_currency);
  if (it != market_priceMap.end())
  {
      const auto &auxMap = it->second;
      auto itt = auxMap.find(static_cast<uint32_t>(dUSD));
      if (itt != auxMap.end())
          tokenPrice = itt->second;
  }


  // multiply token balance for address times the token price (which is denominated in dUSD)
  int64_t max_dUSD = tokenBalance * tokenPrice;

  //compare to short Position
  //get # short contract
  int64_t position = getMPbalance(fromAddress, contractId, CONTRACT_BALANCE);
  //determine which is less and use that one as max you can peg
  int64_t maxpegged = (max_dUSD > position) ? position : max_dUSD;
  //create UniValue object to return
  UniValue max_pegged(UniValue::VOBJ);
  //add value maxpegged to maxPegged json object
  max_pegged.pushKV("maxPegged (dUSD)", FormatDivisibleMP(maxpegged));
  //return UniValue JSON object
  return max_pegged;

}

UniValue tl_getcontract(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "tl_getcontract contract \"name (or id)\" \n"

            "\nReturns details for about the future contract.\n"

            "\nArguments:\n"
            "1. name or id                        (string, required) the name  of the future contract, or the number id\n"

            "\nResult:\n"
            "{\n"
            "  \"contractid\" : n,                 (number) the identifier\n"
            "  \"name\" : \"name\",                 (string) the name of the tokens\n"
            "  \"data\" : \"information\",          (string) additional information or a description\n"
            "  \"url\" : \"uri\",                   (string) an URI, for example pointing to a website\n"
            "  \"divisible\" : true|false,        (boolean) whether the tokens are divisible\n"
            "  \"issuer\" : \"address\",            (string) the Bitcoin address of the issuer on record\n"
            "  \"creationtxid\" : \"hash\",         (string) the hex-encoded creation transaction hash\n"
            "  \"fixedissuance\" : true|false,    (boolean) whether the token supply is fixed\n"
            "  \"totaltokens\" : \"n.nnnnnnnn\"     (string) the total number of tokens in existence\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_getcontract", "\"Contract1\"")
            + HelpExampleRpc("tl_getcontract", "\"Contract1\"")
        );

    uint32_t contractId = ParseNameOrId(request.params[0]);

    CDInfo::Entry cd;
    assert(_my_cds->getCD(contractId, cd));

    UniValue response(UniValue::VOBJ);
    response.pushKV("contractid", (uint64_t) contractId);
    ContractToJSON(cd, response); // name, data, url,
    KYCToJSON(cd, response);
    auto it = cdexlastprice.find(contractId);
    int64_t lastPrice = 0;
    if (it != cdexlastprice.end()){
        const auto& blockMap = it->second;
        const auto& itt = std::prev(blockMap.end());
        if (itt != blockMap.end()) {
           const auto& prices = itt->second;
           const auto& ix = std::prev(prices.end());
           lastPrice = *ix;
        }
    }
    response.pushKV("last traded price", FormatDivisibleMP(lastPrice));
    const int64_t fundBalance = 0; // NOTE: we need to write this after fundbalance logic
    response.pushKV("insurance fund balance", fundBalance);
    return response;
}


UniValue tl_getvesting_info(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw runtime_error(
            "tl_getvesting_info\n"

            "\nReturns details for about vesting tokens.\n"

            "\nResult:\n"
            "{\n"
            "  \"propertyid\" : n,                          (number) the identifier\n"
            "  \"name\" : \"name\",                         (string) the name of the tokens\n"
            "  \"data\" : \"information\",                  (string) additional information or a description\n"
            "  \"url\" : \"uri\",                           (string) an URI, for example pointing to a website\n"
            "  \"divisible\" : true|false,                  (boolean) whether the tokens are divisible\n"
            "  \"issuer\" : \"address\",                    (string) the Litecoin address of the issuer on record\n"
            "  \"activation block\" : \"block\",            (number) the activation block for vesting\n"
            "  \"litecoin volume\" : \"n.nnnnnnnn\",        (string) the accumulated litecoin amount traded\n"
            "  \"vested percentage\" : \"n.nnnnnnnn\",      (string) the accumulated percentage amount vested\n"
            "  \"last vesting block\" : \"block\"           (number) the last block with vesting action\n"
            "  \"total vested \" : \"n.nnnnnnnn\",          (string) the accumulated amount vested\n"
            "  \"owners \" : \"owners\",                    (number) the ALL owners number \n"
            "  \"total tokens\" : \"n.nnnnnnnn\"            (string) the total number of tokens in existence\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_getvesting_info", "")
            + HelpExampleRpc("tl_getvesting_info", "")
        );

    CMPSPInfo::Entry sp;
    {
        if (!_my_sps->getSP(VT, sp)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Property identifier does not exist");
        }
    }


    UniValue response(UniValue::VOBJ);
    response.pushKV("propertyid", (uint64_t) VT);
    VestingToJSON(sp, response); // name, data, url,
    KYCToJSON(sp, response);

    return response;
}

UniValue tl_listvesting_addresses(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw runtime_error(
            "tl_listvesting_addresses\n"

            "\nReturns all receiver addresses vesting tokens.\n"

            "\nResult:\n"
            "[                                      (array of addresses)\n"
      			"    \"address\",                       (string) vesting token address \n"
      			"  ...\n"
      			"]\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_listvesting_addresses", "")
            + HelpExampleRpc("tl_listvesting_addresses", "")
        );

    UniValue response(UniValue::VARR);

    for_each(vestingAddresses.begin() ,vestingAddresses.end(), [&response] (const std::string& address) { response.push_back(address);});

    return response;
}


UniValue tl_listchannel_addresses(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw runtime_error(
            "tl_listchannel_addresses\n"

            "\nReturns details for about vesting tokens.\n"

            "\nResult:\n"
            "[                                      (array of addresses)\n"
      			"    \"address\",                       (string) channel address 1 \n"
            "    \"address\",                       (string) channel address 2 \n"
            "        ...                                                       \n"
            "    \"address\",                       (string) channel address n \n"
      			"  ...\n"
      			"]\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_listchannel_addresses", "")
            + HelpExampleRpc("tl_listchannel_addresses", "")
        );

    UniValue response(UniValue::VARR);

    for_each(channels_Map.begin() ,channels_Map.end(), [&response] (const std::pair<std::string,Channel>& sChn) { response.push_back(sChn.first);});

    return response;

}

UniValue tl_listnodereward_addresses(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw runtime_error(
            "tl_listnodereward_addresses\n"

            "\nReturns all node reward addresses.\n"

            "\nResult:\n"
            "[                                      (array of addresses)\n"
      			"    \"address\",                       (string) node address 1 \n"
            "    \"address\",                       (string) node address 2 \n"
            "        ...                                                       \n"
            "    \"address\",                       (string) node address n \n"
      			"  ...\n"
      			"]\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_listnodereward_addresses", "")
            + HelpExampleRpc("tl_listnodereward_addresses", "")
        );

    UniValue response(UniValue::VARR);

    const std::map<string, bool>& addressMap = nR.getnodeRewardAddrs();

    for_each(addressMap.begin() , addressMap.end(), [&response] (const std::pair<std::string, bool>& nr)
    {
       UniValue details(UniValue::VOBJ);
       details.pushKV("address:", nr.first);
       // details.pushKV("claiming reward:", nr.second ? "true": "false");
       response.push_back(details);
    });

    return response;
}

UniValue tl_getlast_winners(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw runtime_error(
            "tl_getlast_winners\n"

            "\nReturns all last node reward winner addresses.\n"

            "\nResult:\n"
            "[                                      (array of addresses)\n"
      			"    \"address\",                       (string) node address 1 \n"
            "    \"address\",                       (string) node address 2 \n"
            "        ...                                                       \n"
            "    \"address\",                       (string) node address n \n"
      			"  ...\n"
      			"]\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_getlast_winners", "")
            + HelpExampleRpc("tl_getlast_winners", "")
        );

    UniValue response(UniValue::VARR);

    const std::map<string, int64_t>& addrSt = nR.getWinners();

    const int& lastBlock = nR.getLastBlock();

    UniValue details(UniValue::VOBJ);

    details.pushKV("block:", lastBlock);

    response.push_back(details);

    for_each(addrSt.begin() , addrSt.end(), [&response] (const std::pair<string, int64_t>& nr)
    {
       response.push_back(nr.first);
    });

    return response;
}

UniValue tl_getnextreward(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw runtime_error(
            "tl_getnextreward\n"

            "\nReturns reward amount to share.\n"

            "\nResult:\n"
            "[                                     (amount of ALL token)\n"
      			"    \"amount\",                       (string) amount  \n"
      			"]\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_getnextreward", "")
            + HelpExampleRpc("tl_getnextreward", "")
        );

    UniValue response(UniValue::VOBJ);

    const int64_t reward = nR.getNextReward();
    response.pushKV("amount", FormatDivisibleMP(reward));

    return response;
}

UniValue tl_isaddresswinner(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
      throw runtime_error(
          "tl_isaddresswinner\n"

          "\nArguments:\n"
          "1. address                      (string, required) node reward address\n"
          "2. consensushash                (string, optional) block hash, if it's not given we assume block hash of actual block\n"

          "\nReturns if address is winner of node reward at current block.\n"

          "\nResult:\n"
          "[                                                        \n"
          "    \"resutl\",                       (bool) true/false  \n"
          "]\n"

          "\nExamples:\n"
          + HelpExampleCli("tl_isaddresswinner", "mgrNNyDCdAWeYfkvcarsQKRzMhEFQiDmnH")
          + HelpExampleRpc("tl_isaddresswinner", "mgrNNyDCdAWeYfkvcarsQKRzMhEFQiDmnH")
      );

  UniValue response(UniValue::VOBJ);

  LOCK(cs_main);

  std::string blockHash;

  if (request.params.size() == 1) {
      int block = GetHeight();
      CBlockIndex* pblockindex = chainActive[block];
      blockHash = pblockindex->GetBlockHash().GetHex();

  } else {
      blockHash = request.params[1].get_str();
  }

  const std::string address = ParseAddress(request.params[0]);

  bool result = nR.isWinnerAddress(blockHash, address, RegTest());
  response.pushKV("result", result ? "true" : "false");

  return response;
}

UniValue tl_getmdextradehistoryforaddress(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 2 || request.params.size() > 3)
      throw runtime_error(
          "tl_getmdextradehistoryforaddress \"address\" \"count\" \"property\"\n"

          "\nRetrieves the history of orders on the meta distributed exchange for the supplied address.\n"

          "\nArguments:\n"
          "1. address       (string) address to retrieve history for\n"
          "2. count         (number) number of orders to retrieve\n"
          "3. propertyid    (number) filter by property identifier transacted\n"

          "\nResult:\n"
          "[                                              (array of JSON objects)\n"
          "  {\n"
          "    \"txid\" : \"hash\",                               (string) the hex-encoded hash of the transaction of the order\n"
          "    \"sendingaddress\" : \"address\",                  (string) the Bitcoin address of the trader\n"
          "    \"ismine\" : true|false,                         (boolean) whether the order involes an address in the wallet\n"
          "    \"confirmations\" : nnnnnnnnnn,                  (number) the number of transaction confirmations\n"
          "    \"fee\" : \"n.nnnnnnnn\",                          (string) the transaction fee in bitcoins\n"
          "    \"blocktime\" : nnnnnnnnnn,                      (number) the timestamp of the block that contains the transaction\n"
          "    \"valid\" : true|false,                          (boolean) whether the transaction is valid\n"
          "    \"version\" : n,                                 (number) the transaction version\n"
          "    \"type_int\" : n,                                (number) the transaction type as number\n"
          "    \"type\" : \"type\",                               (string) the transaction type as string\n"
          "    \"propertyidforsale\" : n,                       (number) the identifier of the tokens put up for sale\n"
          "    \"propertyidforsaleisdivisible\" : true|false,   (boolean) whether the tokens for sale are divisible\n"
          "    \"amountforsale\" : \"n.nnnnnnnn\",                (string) the amount of tokens initially offered\n"
          "    \"propertyiddesired\" : n,                       (number) the identifier of the tokens desired in exchange\n"
          "    \"propertyiddesiredisdivisible\" : true|false,   (boolean) whether the desired tokens are divisible\n"
          "    \"amountdesired\" : \"n.nnnnnnnn\",                (string) the amount of tokens initially desired\n"
          "    \"unitprice\" : \"n.nnnnnnnnnnn...\"               (string) the unit price (shown in the property desired)\n"
          "    \"status\" : \"status\"                            (string) the status of the order (\"open\", \"cancelled\", \"filled\", ...)\n"
          "    \"canceltxid\" : \"hash\",                         (string) the hash of the transaction that cancelled the order (if cancelled)\n"
          "    \"matches\": [                                     (array of JSON objects) a list of matched orders and executed trades\n"
          "      {\n"
          "        \"txid\" : \"hash\",                               (string) the hash of the transaction that was matched against\n"
          "        \"block\" : nnnnnn,                                (number) the index of the block that contains this transaction\n"
          "        \"address\" : \"address\",                         (string) the Litecoin address of the other trader\n"
          "        \"amountsold\" : \"n.nnnnnnnn\",                   (string) the number of tokens sold in this trade\n"
          "        \"amountreceived\" : \"n.nnnnnnnn\"                (string) the number of tokens traded in exchange\n"
          "      },\n"
          "      ...\n"
          "    ]\n"
          "  },\n"
          "  ...\n"
          "]\n"

          "\nExamples:\n"
          + HelpExampleCli("tl_gettradehistoryforaddress", "\"38CYEC81MhsAPYFUD6MNMZAuPeJRddaDqW\", \"300\" , \"4\"")
          + HelpExampleRpc("tl_gettradehistoryforaddress", "\"38CYEC81MhsAPYFUD6MNMZAuPeJRddaDqW\" \"32\" , \"3\"")
        );

    std::string address = ParseAddress(request.params[0]);
    uint64_t count = (request.params.size() > 1) ? request.params[1].get_int64() : 10;
    uint32_t propertyId = 0;

    if (request.params.size() > 2) {
        propertyId = ParsePropertyId(request.params[2]);
        RequireExistingProperty(propertyId);
    }

    // Obtain a sorted vector of txids for the address trade history
    std::vector<uint256> vecTransactions;
    {
        LOCK(cs_tally);
        t_tradelistdb->getTradesForAddress(address, vecTransactions, propertyId);
    }

    // Populate the address trade history into JSON objects until we have processed count transactions
    UniValue response(UniValue::VARR);
    uint32_t processed = 0;
    for(std::vector<uint256>::reverse_iterator it = vecTransactions.rbegin(); it != vecTransactions.rend(); ++it) {
        UniValue txobj(UniValue::VOBJ);
        int populateResult = populateRPCTransactionObject(*it, txobj);
        if (0 == populateResult) {
            response.push_back(txobj);
            processed++;
            if (processed >= count) break;
        }
    }

    return response;
}

UniValue tl_getmdextradehistoryforpair(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 2 || request.params.size() > 3)
      throw runtime_error(
          "tl_getmdextradehistoryforpair \"propertyA\" \"propertyB\" \"count\"\n"

          "\nnRetrieves the history of trades on the distributed token exchange for the specified market.\n"

          "\nArguments:\n"
          "1. propertyid               (number) property id\n"
          "2. propertyidsecond         (number) property desired id\n"
          "3. count                    (number) number of orders to retrieve\n"

          "\nResult:\n"
          "[                                      (array of JSON objects)\n"
          "  {\n"
          "    \"block\" : nnnnnn,                        (number) the index of the block that contains the trade match\n"
          "    \"unitprice\" : \"n.nnnnnnnnnnn...\" ,     (string) the unit price used to execute this trade (received/sold)\n"
          "    \"inverseprice\" : \"n.nnnnnnnnnnn...\",   (string) the inverse unit price (sold/received)\n"
          "    \"sellertxid\" : \"hash\",                 (string) the hash of the transaction of the seller\n"
          "    \"address\" : \"address\",                 (string) the Litecoin address of the seller\n"
          "    \"amountsold\" : \"n.nnnnnnnn\",           (string) the number of tokens sold in this trade\n"
          "    \"amountreceived\" : \"n.nnnnnnnn\",       (string) the number of tokens traded in exchange\n"
          "    \"matchingtxid\" : \"hash\",               (string) the hash of the transaction that was matched against\n"
          "    \"matchingaddress\" : \"address\"          (string) the Litecoin address of the other party of this trade\n"
          "  },\n"
          "  ...\n"
          "]\n"

          "\nExamples:\n"
          + HelpExampleCli("tl_getmdextradehistoryforpair", "\"1\" \"12\" \"500\"")
          + HelpExampleRpc("tl_getmdextradehistoryforpair", "\"1\", \"12\", \"500\"")
        );

      // obtain property identifiers for pair & check valid parameters
     uint32_t propertyIdSideA = ParsePropertyId(request.params[0]);
     uint32_t propertyIdSideB = ParsePropertyId(request.params[1]);
     uint64_t count = (request.params.size() > 2) ? request.params[2].get_int64() : 10;

     RequireExistingProperty(propertyIdSideA);
     RequireExistingProperty(propertyIdSideB);
     RequireDifferentIds(propertyIdSideA, propertyIdSideB);

     // request pair trade history from trade db
     UniValue response(UniValue::VARR);
     LOCK(cs_tally);
     t_tradelistdb->getTradesForPair(propertyIdSideA, propertyIdSideB, response, count);

    return response;
}

UniValue tl_getdextradehistoryforaddress(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 2 || request.params.size() > 3)
      throw runtime_error(
          "tl_getdextradehistoryforaddress \"address\" \"count\" \"propertyid\" \n"

          "\nnRetrieves the history of trades on the distributed token/LTC exchange, for an address.\n"

          "\nArguments:\n"
          "1. address                  (string) address for seller o buyer\n"
          "2. count                    (number) number of orders to retrieve\n"
          "3. propertyid               (number) property id\n"

          "\nResult:\n"
          "[                                      (array of JSON objects)\n"
          "  {\n"
          "    \"block\" : nnnnnn,                        (number) the index of the block that contains the trade match\n"
          "    \"buyertxid\" : \"hash\",                  (string) the hash of the transaction of the token buyer\n"
          "    \"selleraddress\" : \"address\",           (string) the Litecoin address of the token seller\n"
          "    \"buyeraddress\" :  \"address\",           (string) the Litecoin address of the token buyer\n"
          "    \"propertyid\" : \"n.nnnnnnnn\",           (number) the propertyid of exchanged token\n"
          "    \"amountbuyed\" : \"n.nnnnnnnn\",          (string) the number of tokens sold in this trade\n"
          "    \"amountpaid\" : \"n.nnnnnnnn\",           (string) the number of Litecoins traded in exchange\n"
          "  },\n"
          "  ...\n"
          "]\n"

          "\nExamples:\n"
          + HelpExampleCli("tl_getdextradehistoryforaddress", "\"38CYEC81MhsAPYFUD6MNMZAuPeJRddaDqW\", \"1000\", \"4\"")
          + HelpExampleRpc("tl_getdextradehistoryforaddress", "\"38CYEC81MhsAPYFUD6MNMZAuPeJRddaDqW\" \"200\", \"3\"")
        );

    // obtain property identifiers for pair & check valid parameters


    std::string address = ParseAddress(request.params[0]);
    uint64_t count = (request.params.size() > 1) ? request.params[1].get_int64() : 10;
    uint32_t propertyId = 0;

    if (request.params.size() > 2) {
        propertyId = ParsePropertyId(request.params[2]);
        RequireExistingProperty(propertyId);
    }

    RequireExistingProperty(propertyId);

    // request pair trade history from trade db
    UniValue response(UniValue::VARR);
    LOCK(cs_tally);
    p_txlistdb->getDExTrades(address, propertyId, response, count);

    return response;
}

UniValue tl_getchannel_historyforaddress(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 3 || request.params.size() > 4)
      throw runtime_error(
          "tl_getchannel_historyforaddress \"address\" \"channel\" \"count\" \"property\" \n"

          "\nnRetrieves the history for token/LTC exchange on trade channels, for an address.\n"

          "\nArguments:\n"
          "1. address                  (string) address for seller o buyer\n"
          "2. channel                  (string) address of trade channel\n"
          "3. count                    (number) number of orders to retrieve\n"
          "4. propertyid               (number) property id\n"

          "\nResult:\n"
          "[                                      (array of JSON objects)\n"
          "  {\n"
          "    \"block\" : nnnnnn,                        (number) the index of the block that contains the trade match\n"
          "    \"buyertxid\" : \"hash\",                  (string) the hash of the transaction of the token buyer\n"
          "    \"selleraddress\" : \"address\",           (string) the Litecoin address of the token seller\n"
          "    \"buyeraddress\" :  \"address\",           (string) the Litecoin address of the token buyer\n"
          "    \"channeladdress\" :  \"address\",         (string) the Litecoin address of Trade Channel\n"
          "    \"propertyid\" : \"n.nnnnnnnn\",           (number) the propertyid of exchanged token\n"
          "    \"amountbuyed\" : \"n.nnnnnnnn\",          (string) the number of tokens sold in this trade\n"
          "    \"amountpaid\" : \"n.nnnnnnnn\",           (string) the number of Litecoins traded in exchange\n"
          "  },\n"
          "  ...\n"
          "]\n"

          "\nExamples:\n"
          + HelpExampleCli("tl_getchannel_historyforaddress", "\"38CYEC81MhsAPYFUD6MNMZAuPeJRddaDqW\", \"2N2sNJSgXmWft2wtdFFGwhh9thog4mGKTT11000\", \"4\" \"3\"")
          + HelpExampleRpc("tl_getchannel_historyforaddress", "\"38CYEC81MhsAPYFUD6MNMZAuPeJRddaDqW\" , \"2N2sNJSgXmWft2wtdFFGwhh9thog4mGKTT11000\", \"200\", \"3\"")
        );

    std::string address = ParseAddress(request.params[0]);
    std::string channel = ParseAddress(request.params[1]);
    uint64_t count = (request.params.size() > 2) ? request.params[2].get_int64() : 10;
    uint32_t propertyId = 0;

    if (request.params.size() > 3) {
        propertyId = ParsePropertyId(request.params[3]);
        RequireExistingProperty(propertyId);
    }

    RequireExistingProperty(propertyId);

    // request pair trade history from trade db
    UniValue response(UniValue::VARR);
    LOCK(cs_tally);
    p_txlistdb->getChannelTrades(address, channel, propertyId, response, count);

    return response;
}

UniValue tl_getchannel_tokenhistoryforaddress(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() < 2 || request.params.size() > 4)
      throw runtime_error(
          "tl_getchannel_tokenhistoryforaddress \"address\" \"channel\"  \"count\" \"property\"\n"

          "\nRetrieves the trade history in token/token exchange in  a trade channels , for supplied address.\n"

          "\nArguments:\n"
          "1. address       (string) address to retrieve history for\n"
          "2. channel       (string) trade channel address\n"
          "3. count         (number) number of orders to retrieve\n"
          "4. propertyid    (number) filter by property identifier transacted\n"

          "\nResult:\n"
          "[                                              (array of JSON objects)\n"
          "  {\n"
          "    \"txid\" : \"hash\",                               (string) the hex-encoded hash of the transaction of the order\n"
          "    \"sendingaddress\" : \"address\",                  (string) the Litecoin address of the trader\n"
          "    \"ismine\" : true|false,                         (boolean) whether the order involes an address in the wallet\n"
          "    \"confirmations\" : nnnnnnnnnn,                  (number) the number of transaction confirmations\n"
          "    \"fee\" : \"n.nnnnnnnn\",                          (string) the transaction fee in bitcoins\n"
          "    \"blocktime\" : nnnnnnnnnn,                      (number) the timestamp of the block that contains the transaction\n"
          "    \"valid\" : true|false,                          (boolean) whether the transaction is valid\n"
          "    \"version\" : n,                                 (number) the transaction version\n"
          "    \"type_int\" : n,                                (number) the transaction type as number\n"
          "    \"type\" : \"type\",                               (string) the transaction type as string\n"
          "    \"propertyidforsale\" : n,                       (number) the identifier of the tokens put up for sale\n"
          "    \"propertyidforsaleisdivisible\" : true|false,   (boolean) whether the tokens for sale are divisible\n"
          "    \"amountforsale\" : \"n.nnnnnnnn\",                (string) the amount of tokens initially offered\n"
          "    \"propertyiddesired\" : n,                       (number) the identifier of the tokens desired in exchange\n"
          "    \"propertyiddesiredisdivisible\" : true|false,   (boolean) whether the desired tokens are divisible\n"
          "    \"amountdesired\" : \"n.nnnnnnnn\",                (string) the amount of tokens initially desired\n"
          "    \"unitprice\" : \"n.nnnnnnnnnnn...\"               (string) the unit price (shown in the property desired)\n"
          "    \"status\" : \"status\"                            (string) the status of the order (\"open\", \"cancelled\", \"filled\", ...)\n"
          "    \"canceltxid\" : \"hash\",                         (string) the hash of the transaction that cancelled the order (if cancelled)\n"
          "    \"matches\": [                                     (array of JSON objects) a list of matched orders and executed trades\n"
          "      {\n"
          "        \"txid\" : \"hash\",                               (string) the hash of the transaction that was matched against\n"
          "        \"block\" : nnnnnn,                                (number) the index of the block that contains this transaction\n"
          "        \"address\" : \"address\",                         (string) the Litecoin address of the other trader\n"
          "        \"amountsold\" : \"n.nnnnnnnn\",                   (string) the number of tokens sold in this trade\n"
          "        \"amountreceived\" : \"n.nnnnnnnn\"                (string) the number of tokens traded in exchange\n"
          "      },\n"
          "      ...\n"
          "    ]\n"
          "  },\n"
          "  ...\n"
          "]\n"

          "\nExamples:\n"
          + HelpExampleCli("tl_getchannel_tokenhistoryforaddress", "\"38CYEC81MhsAPYFUD6MNMZAuPeJRddaDqW\", \"38CYEC81MhsAPYFUD6MNMZAuPeJRddaDqW\", \"2\", \"4\"")
          + HelpExampleRpc("tl_getchannel_tokenhistoryforaddress", "\"38CYEC81MhsAPYFUD6MNMZAuPeJRddaDqW\", \"38CYEC81MhsAPYFUD6MNMZAuPeJRddaDqW\", \"2\", \"4\"")
        );

    std::string address = ParseAddress(request.params[0]);
    std::string channel = ParseAddress(request.params[1]);
    uint64_t count = (request.params.size() > 2) ? request.params[2].get_int64() : 10;
    uint32_t propertyId = 0;

    if (request.params.size() > 3) {
        propertyId = ParsePropertyId(request.params[3]);
        RequireExistingProperty(propertyId);
    }

    // request pair trade history from trade db
    UniValue response(UniValue::VARR);
    LOCK(cs_tally);
    t_tradelistdb->getTokenChannelTrades(address, channel, propertyId, response, count);

    return response;
}

UniValue tl_getchannel_historyforpair(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 3 || request.params.size() > 4)
      throw runtime_error(
          "tl_getchannel_historyforpair \"channel\" \"propertyA\" \"propertyB\" \"count\"\n"

          "\nnRetrieves the history of token/token  on a given trade channel for the specified market.\n"

          "\nArguments:\n"
          "1. channel                  (string) channel address\n"
          "2. propertyid               (number) property id\n"
          "3. propertyidsecond         (number) property desired id\n"
          "4. count                    (number) number of orders to retrieve\n"

          "\nResult:\n"
          "[                                      (array of JSON objects)\n"
          "  {\n"
          "    \"block\" : nnnnnn,                        (number) the index of the block that contains the trade match\n"
          "    \"unitprice\" : \"n.nnnnnnnnnnn...\" ,     (string) the unit price used to execute this trade (received/sold)\n"
          "    \"inverseprice\" : \"n.nnnnnnnnnnn...\",   (string) the inverse unit price (sold/received)\n"
          "    \"sellertxid\" : \"hash\",                 (string) the hash of the transaction of the seller\n"
          "    \"address\" : \"address\",                 (string) the Litecoin address of the seller\n"
          "    \"amountsold\" : \"n.nnnnnnnn\",           (string) the number of tokens sold in this trade\n"
          "    \"amountreceived\" : \"n.nnnnnnnn\",       (string) the number of tokens traded in exchange\n"
          "    \"matchingtxid\" : \"hash\",               (string) the hash of the transaction that was matched against\n"
          "    \"matchingaddress\" : \"address\"          (string) the Litecoin address of the other party of this trade\n"
          "  },\n"
          "  ...\n"
          "]\n"

          "\nExamples:\n"
          + HelpExampleCli("tl_getchannel_historyforpair", "\"38CYEC81MhsAPYFUD6MNMZAuPeJRddaDqW\", \"1\" \"12\" \"500\"")
          + HelpExampleRpc("tl_getchannel_historyforpair", "\"38CYEC81MhsAPYFUD6MNMZAuPeJRddaDqW\", \"1\", \"12\", \"500\"")
        );

     // obtain property identifiers for pair & check valid parameters
     std::string channel = ParseAddress(request.params[0]);
     uint32_t propertyIdSideA = ParsePropertyId(request.params[1]);
     uint32_t propertyIdSideB = ParsePropertyId(request.params[2]);
     uint64_t count = (request.params.size() > 3) ? request.params[3].get_int64() : 10;

     RequireExistingProperty(propertyIdSideA);
     RequireExistingProperty(propertyIdSideB);
     RequireDifferentIds(propertyIdSideA, propertyIdSideB);

     // request pair trade history from trade db
     UniValue response(UniValue::VARR);
     LOCK(cs_tally);
     t_tradelistdb->getChannelTradesForPair(channel, propertyIdSideA, propertyIdSideB, response, count);

    return response;
}

static const CRPCCommand commands[] =
{ //  category                             name                            actor (function)               okSafeMode
  //  ------------------------------------ ------------------------------- ------------------------------ ----------
  { "trade layer (data retrieval)", "tl_getinfo",                              &tl_getinfo,                           {} },
  { "trade layer (data retrieval)", "tl_getactivations",                       &tl_getactivations,                    {} },
  { "trade layer (data retrieval)", "tl_getallbalancesforid",                  &tl_getallbalancesforid,               {} },
  { "trade layer (data retrieval)", "tl_getbalance",                           &tl_getbalance,                        {} },
  { "trade layer (data retrieval)", "tl_gettransaction",                       &tl_gettransaction,                    {} },
  { "trade layer (data retrieval)", "tl_getproperty",                          &tl_getproperty,                       {} },
  { "trade layer (data retrieval)", "tl_listproperties",                       &tl_listproperties,                    {} },
  { "trade layer (data retrieval)", "tl_getgrants",                            &tl_getgrants,                         {} },
  { "trade layer (data retrieval)", "tl_listblocktransactions",                &tl_listblocktransactions,             {} },
  { "trade layer (data retrieval)", "tl_listpendingtransactions",              &tl_listpendingtransactions,           {} },
  { "trade layer (data retrieval)", "tl_getallbalancesforaddress",             &tl_getallbalancesforaddress,          {} },
  { "trade layer (data retrieval)", "tl_getcurrentconsensushash",              &tl_getcurrentconsensushash,           {} },
  { "trade layer (data retrieval)", "tl_getpayload",                           &tl_getpayload,                        {} },
#ifdef ENABLE_WALLET
  { "trade layer (data retrieval)", "tl_listtransactions",                     &tl_listtransactions,                  {} },
  { "trade layer (configuration)",  "tl_setautocommit",                        &tl_setautocommit,                     {} },
#endif
  { "hidden",                       "mscrpc",                                  &mscrpc,                               {} },
  { "trade layer (data retieval)",  "tl_getchannel_historyforpair",            &tl_getchannel_historyforpair,         {} },
  { "trade layer (data retieval)",  "tl_getchannel_tokenhistoryforaddress",    &tl_getchannel_tokenhistoryforaddress, {} },
  { "trade layer (data retieval)",  "tl_getchannel_historyforaddress",         &tl_getchannel_historyforaddress,      {} },
  { "trade layer (data retieval)",  "tl_getdextradehistoryforaddress",         &tl_getdextradehistoryforaddress,      {} },
  { "trade layer (data retieval)",  "tl_getmdextradehistoryforaddress",        &tl_getmdextradehistoryforaddress,     {} },
  { "trade layer (data retieval)",  "tl_getmdextradehistoryforpair",           &tl_getmdextradehistoryforpair,        {} },
  { "trade layer (data retrieval)", "tl_getposition",                          &tl_getposition,                       {} },
  { "trade layer (data retrieval)", "tl_getfullposition",                      &tl_getfullposition,                   {} },
  { "trade layer (data retrieval)", "tl_getcontract_orderbook",                &tl_getcontract_orderbook,             {} },
  { "trade layer (data retrieval)", "tl_gettradehistory",                      &tl_gettradehistory,                   {} },
  { "trade layer (data retrieval)", "tl_gettradehistory_unfiltered",           &tl_gettradehistory_unfiltered,        {} },
  { "trade layer (data retrieval)", "tl_getupnl",                              &tl_getupnl,                           {} },
  { "trade layer (data retrieval)", "tl_getpnl",                               &tl_getpnl,                            {} },
  { "trade layer (data retrieval)",  "tl_getactivedexsells",                    &tl_getactivedexsells ,                {} },
  { "trade layer (data retrieval)" , "tl_getorderbook",                         &tl_getorderbook,                      {} },
  { "trade layer (data retrieval)" , "tl_getpeggedhistory",                     &tl_getpeggedhistory,                  {} },
  { "trade layer (data retrieval)" , "tl_getcontract_reserve",                  &tl_getcontract_reserve,               {} },
  { "trade layer (data retrieval)" , "tl_getreserve",                           &tl_getreserve,                        {} },
  { "trade layer (data retrieval)" , "tl_getallprice",                          &tl_getallprice,                       {} },
  { "trade layer (data retrieval)" , "tl_getmarketprice",                       &tl_getmarketprice,                    {} },
  { "trade layer (data retrieval)" , "tl_getsum_upnl",                          &tl_getsum_upnl,                       {} },
  { "trade layer (data retrieval)" , "tl_check_commits",                        &tl_check_commits,                     {} },
  { "trade layer (data retrieval)" , "tl_get_channelreserve",                   &tl_get_channelreserve,                {} },
  { "trade layer (data retrieval)" , "tl_getchannel_info",                      &tl_getchannel_info,                   {} },
  { "trade layer (data retrieval)" , "tl_getcache",                             &tl_getcache,                          {} },
  { "trade layer (data retrieval)" , "tl_check_kyc",                            &tl_check_kyc,                         {} },
  { "trade layer (data retrieval)" , "tl_list_natives",                         &tl_list_natives,                      {} },
  { "trade layer (data retrieval)" , "tl_list_oracles",                         &tl_list_oracles,                      {} },
  { "trade layer (data retrieval)" , "tl_getalltxonblock",                      &tl_getalltxonblock,                   {} },
  { "trade layer (data retrieval)" , "tl_check_withdrawals",                    &tl_check_withdrawals,                 {} },
  { "trade layer (data retrieval)" , "tl_get_ltcvolume",                        &tl_get_ltcvolume,                     {} },
  { "trade layer (data retrieval)" , "tl_getmdexvolume",                        &tl_getmdexvolume,                     {} },
  { "trade layer (data retrieval)" , "tl_getcurrencytotal",                     &tl_getcurrencytotal,                  {} },
  { "trade layer (data retrieval)" , "tl_listkyc",                              &tl_listkyc,                           {} },
  { "trade layer (data retrieval)" , "tl_getoraclecache",                       &tl_getoraclecache,                    {} },
  { "trade layer (data retrieval)",  "tl_getmax_peggedcurrency",                &tl_getmax_peggedcurrency,             {} },
  { "trade layer (data retrieval)",  "tl_getunvested",                          &tl_getunvested,                       {} },
  { "trade layer (data retrieval)",  "tl_list_attestation",                     &tl_list_attestation,                  {} },
  { "trade layer (data retrieval)",  "tl_getcontract",                          &tl_getcontract,                       {} },
  { "trade layer (data retrieval)",  "tl_getopen_interest",                     &tl_getopen_interest,                  {} },
  { "trade layer (data retrieval)",  "tl_getvesting_info",                      &tl_getvesting_info,                   {} },
  { "trade layer (data retrieval)",  "tl_listvesting_addresses",                &tl_listvesting_addresses,             {} },
  { "trade layer (data retrieval)",  "tl_get_channelremaining",                 &tl_get_channelremaining,              {} },
  { "trade layer (data retrieval)",  "tl_list_attestation",                     &tl_list_attestation,                  {} },
  { "trade layer (data retrieval)",  "tl_getwalletbalance",                     &tl_getwalletbalance,                  {} },
  { "trade layer (data retrieval)",  "tl_listchannel_addresses",                &tl_listchannel_addresses,             {} },
  {"trade layer (data retrieval)",   "tl_listnodereward_addresses",             &tl_listnodereward_addresses,          {} },
  {"trade layer (data retrieval)",   "tl_getnextreward",                        &tl_getnextreward,                     {} },
  { "trade layer (data retrieval)", "tl_isaddresswinner",                       &tl_isaddresswinner,                   {} },
  { "trade layer (data retrieval)" , "tl_getlast_winners",                      &tl_getlast_winners,                   {} }
};

void RegisterTLDataRetrievalRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
