/**
 * @file rpc.cpp
 *
 * This file contains RPC calls for data retrieval.
 */

#include <tradelayer/rpc.h>

#include <tradelayer/activation.h>
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

extern int64_t totalVesting;
extern std::map<uint32_t, std::map<std::string, double>> addrs_upnlc;
extern std::map<std::string, int64_t> sum_upnls;
extern std::map<uint32_t, std::map<uint32_t, int64_t>> market_priceMap;
extern std::map<uint32_t, std::vector<int64_t>> mapContractAmountTimesPrice;
extern volatile int64_t globalVolumeALL_LTC;
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
        case MP_CROWDSALE_WITHOUT_PROPERTY:
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Potential database corruption: \
                                                  \"Crowdsale Purchase\" without valid property identifier");
        case MP_INVALID_TX_IN_DB_FOUND:
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Potential database corruption: Invalid transaction found");
        case MP_TX_IS_NOT_MASTER_PROTOCOL:
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Not a Master Protocol transaction");
    }
    throw JSONRPCError(RPC_INTERNAL_ERROR, "Generic transaction population failure");
}

void PropertyToJSON(const CMPSPInfo::Entry& sProperty, UniValue& property_obj)
{
    property_obj.push_back(Pair("name", sProperty.name));
    property_obj.push_back(Pair("data", sProperty.data));
    property_obj.push_back(Pair("url", sProperty.url));
    property_obj.push_back(Pair("divisible", sProperty.isDivisible()));
    property_obj.push_back(Pair("category", sProperty.category));
    property_obj.push_back(Pair("subcategory", sProperty.subcategory));

}

void ContractToJSON(const CMPSPInfo::Entry& sProperty, UniValue& property_obj)
{
    property_obj.push_back(Pair("name", sProperty.name));
    property_obj.push_back(Pair("data", sProperty.data));
    property_obj.push_back(Pair("url", sProperty.url));
    property_obj.push_back(Pair("admin", sProperty.issuer));
    property_obj.push_back(Pair("creationtxid", sProperty.txid.GetHex()));
    property_obj.push_back(Pair("creation block", sProperty.init_block));
    property_obj.push_back(Pair("notional size", FormatDivisibleShortMP(sProperty.notional_size)));
    property_obj.push_back(Pair("collateral currency", std::to_string(sProperty.collateral_currency)));
    property_obj.push_back(Pair("margin requirement", FormatDivisibleShortMP(sProperty.margin_requirement)));
    property_obj.push_back(Pair("blocks until expiration", std::to_string(sProperty.blocks_until_expiration)));
    property_obj.push_back(Pair("inverse quoted", std::to_string(sProperty.inverse_quoted)));
    if (sProperty.isOracle())
    {
        property_obj.push_back(Pair("backup address", sProperty.backup_address));
        property_obj.push_back(Pair("hight price", FormatDivisibleShortMP(sProperty.oracle_high)));
        property_obj.push_back(Pair("low price", FormatDivisibleShortMP(sProperty.oracle_low)));
        property_obj.push_back(Pair("last close price", FormatDivisibleShortMP(sProperty.oracle_close)));
        property_obj.push_back(Pair("type", "oracle"));

    } else if (sProperty.isNative()){
        property_obj.push_back(Pair("type", "native"));

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

    property_obj.push_back(Pair("kyc_ids allowed","["+sKyc+"]"));

}


void OracleToJSON(const CMPSPInfo::Entry& sProperty, UniValue& property_obj)
{
    property_obj.push_back(Pair("name", sProperty.name));
    property_obj.push_back(Pair("data", sProperty.data));
    property_obj.push_back(Pair("url", sProperty.url));
    property_obj.push_back(Pair("issuer", sProperty.issuer));
    property_obj.push_back(Pair("creationtxid", sProperty.txid.GetHex()));
    property_obj.push_back(Pair("creation block", sProperty.init_block));
    property_obj.push_back(Pair("notional size", FormatDivisibleShortMP(sProperty.notional_size)));
    property_obj.push_back(Pair("collateral currency", std::to_string(sProperty.collateral_currency)));
    property_obj.push_back(Pair("margin requirement", FormatDivisibleShortMP(sProperty.margin_requirement)));
    property_obj.push_back(Pair("blocks until expiration", std::to_string(sProperty.blocks_until_expiration)));
    property_obj.push_back(Pair("last high price", FormatDivisibleShortMP(sProperty.oracle_high)));
    property_obj.push_back(Pair("last low price", FormatDivisibleShortMP(sProperty.oracle_low)));
    property_obj.push_back(Pair("last close price",FormatDivisibleShortMP(sProperty.oracle_close)));
}

void VestingToJSON(const CMPSPInfo::Entry& sProperty, UniValue& property_obj)
{
    property_obj.push_back(Pair("name", sProperty.name));
    property_obj.push_back(Pair("data", sProperty.data));
    property_obj.push_back(Pair("url", sProperty.url));
    property_obj.push_back(Pair("divisible", sProperty.isDivisible()));
    property_obj.push_back(Pair("issuer", sProperty.issuer));
    property_obj.push_back(Pair("activation block", sProperty.init_block));

    const int64_t xglobal = globalVolumeALL_LTC;
    const double accum = (isNonMainNet()) ? getAccumVesting(100 * xglobal) : getAccumVesting(xglobal);
    const int64_t vestedPer = 100 * mastercore::DoubleToInt64(accum);

    property_obj.push_back(Pair("litecoin volume",  FormatDivisibleMP(xglobal)));
    property_obj.push_back(Pair("vested percentage",  FormatDivisibleMP(vestedPer)));
    property_obj.push_back(Pair("last vesting block",  sProperty.last_vesting_block));

    int64_t totalVested = getTotalTokens(ALL);
    if (RegTest()) totalVested -= sProperty.num_tokens;

    property_obj.push_back(Pair("total vested",  FormatDivisibleMP(totalVested)));

    size_t n_owners_total = vestingAddresses.size();
    property_obj.push_back(Pair("owners",  n_owners_total));
    property_obj.push_back(Pair("total tokens", FormatDivisibleMP(sProperty.num_tokens)));

}

bool BalanceToJSON(const std::string& address, uint32_t property, UniValue& balance_obj, bool divisible)
{
    // confirmed balance minus unconfirmed, spent amounts
    int64_t nAvailable = getUserAvailableMPbalance(address, property);
    int64_t nReserve = getUserReserveMPbalance(address, property);

    if (divisible) {
        balance_obj.push_back(Pair("balance", FormatDivisibleMP(nAvailable)));
        balance_obj.push_back(Pair("reserve", FormatDivisibleMP(nReserve)));
    } else {
        balance_obj.push_back(Pair("balance", FormatIndivisibleMP(nAvailable)));
        balance_obj.push_back(Pair("reserve", FormatIndivisibleMP(nReserve)));
    }

    if (nAvailable == 0) {
        return false;
    }

    return true;
}

void ReserveToJSON(const std::string& address, uint32_t property, UniValue& balance_obj, bool divisible)
{
    int64_t margin = getMPbalance(address, property, CONTRACTDEX_RESERVE);
    if (divisible) {
        balance_obj.push_back(Pair("reserve", FormatDivisibleMP(margin)));
    } else {
        balance_obj.push_back(Pair("reserve", FormatIndivisibleMP(margin)));
    }
}

void UnvestedToJSON(const std::string& address, uint32_t property, UniValue& balance_obj, bool divisible)
{
    const int64_t unvested = getMPbalance(address, property, UNVESTED);
    if (divisible) {
        balance_obj.push_back(Pair("unvested", FormatDivisibleMP(unvested)));
    } else {
        balance_obj.push_back(Pair("unvested", FormatIndivisibleMP(unvested)));
    }
}

void ChannelToJSON(const std::string& address, uint32_t property, UniValue& balance_obj, bool divisible)
{
    const int64_t margin = getMPbalance(address, property, CHANNEL_RESERVE);
    if (divisible) {
        balance_obj.push_back(Pair("channel reserve", FormatDivisibleMP(margin)));
    } else {
        balance_obj.push_back(Pair("channel reserve", FormatIndivisibleMP(margin)));
    }
}

void MetaDexObjectToJSON(const CMPMetaDEx& obj, UniValue& metadex_obj)
{
    bool propertyIdForSaleIsDivisible = isPropertyDivisible(obj.getProperty());
    bool propertyIdDesiredIsDivisible = isPropertyDivisible(obj.getDesProperty());

    // add data to JSON object
    metadex_obj.push_back(Pair("address", obj.getAddr()));
    metadex_obj.push_back(Pair("txid", obj.getHash().GetHex()));
    metadex_obj.push_back(Pair("propertyidforsale", (uint64_t) obj.getProperty()));
    metadex_obj.push_back(Pair("propertyidforsaleisdivisible", propertyIdForSaleIsDivisible));
    metadex_obj.push_back(Pair("amountforsale", FormatMP(obj.getProperty(), obj.getAmountForSale())));
    metadex_obj.push_back(Pair("amountremaining", FormatMP(obj.getProperty(), obj.getAmountRemaining())));
    metadex_obj.push_back(Pair("propertyiddesired", (uint64_t) obj.getDesProperty()));
    metadex_obj.push_back(Pair("propertyiddesiredisdivisible", propertyIdDesiredIsDivisible));
    metadex_obj.push_back(Pair("amountdesired", FormatMP(obj.getDesProperty(), obj.getAmountDesired())));
    metadex_obj.push_back(Pair("amounttofill", FormatMP(obj.getDesProperty(), obj.getAmountToFill())));
    metadex_obj.push_back(Pair("action", (int) obj.getAction()));
    metadex_obj.push_back(Pair("block", obj.getBlock()));
    metadex_obj.push_back(Pair("blocktime", obj.getBlockTime()));
}

void ContractDexObjectToJSON(const CMPContractDex& obj, UniValue& contractdex_obj)
{

    // add data to JSON object
    contractdex_obj.push_back(Pair("address", obj.getAddr()));
    contractdex_obj.push_back(Pair("txid", obj.getHash().GetHex()));
    contractdex_obj.push_back(Pair("contractid", (uint64_t) obj.getProperty()));
    contractdex_obj.push_back(Pair("amountforsale", obj.getAmountForSale()));
    contractdex_obj.push_back(Pair("tradingaction", obj.getTradingAction()));
    contractdex_obj.push_back(Pair("effectiveprice",  FormatMP(1,obj.getEffectivePrice())));
    contractdex_obj.push_back(Pair("block", obj.getBlock()));
    contractdex_obj.push_back(Pair("idx", static_cast<uint64_t>(obj.getIdx())));
    contractdex_obj.push_back(Pair("blocktime", obj.getBlockTime()));
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
    if (request.params.size() != 1 || request.fHelp)
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
    payloadObj.push_back(Pair("payload", mp_obj.getPayload()));
    payloadObj.push_back(Pair("payloadsize", mp_obj.getPayloadSize()));
    return payloadObj;
}

// determine whether to automatically commit transactions
UniValue tl_setautocommit(const JSONRPCRequest& request)
{
    if (request.params.size() != 1 || request.fHelp)
        throw runtime_error(
            "tl_setautocommit flag\n"
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

    if (request.params.size() > 3 || request.fHelp)
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
        case 4:
        {
            LOCK(cs_tally);
            for (CrowdMap::const_iterator it = my_crowds.begin(); it != my_crowds.end(); ++it) {
                (it->second).print(it->first);
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
    if (request.params.size() != 2 || request.fHelp)
        throw runtime_error(
            "tl_getbalance \"address\" propertyid\n"
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
            + HelpExampleCli("tl_getbalance", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\" 1")
            + HelpExampleRpc("tl_getbalance", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\", 1")
        );

    std::string address = ParseAddress(request.params[0]);
    uint32_t propertyId = ParsePropertyId(request.params[1]);

    // RequireExistingProperty(propertyId);
    // RequireNotContract(propertyId);

    UniValue balanceObj(UniValue::VOBJ);
    BalanceToJSON(address, propertyId, balanceObj, isPropertyDivisible(propertyId));

    return balanceObj;
}

// display an unvested balance via RPC
UniValue tl_getunvested(const JSONRPCRequest& request)
{
    if (request.params.size() != 1 || request.fHelp)
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
            + HelpExampleCli("tl_getunvested", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\" ")
            + HelpExampleRpc("tl_getunvested", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\", ")
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
    if (request.params.size() != 2 || request.fHelp)
        throw runtime_error(
            "tl_getmargin \"address\" propertyid\n"
            "\nReturns the token reserve account using in futures contracts, for a given address and property.\n"
            "\nArguments:\n"
            "1. address              (string, required) the address\n"
            "2. propertyid           (number, required) the contract identifier\n"
            "\nResult:\n"
            "{\n"
            "  \"balance\" : \"n.nnnnnnnn\",   (string) the available balance of the address\n"
            "  \"reserved\" : \"n.nnnnnnnn\"   (string) the amount reserved by sell offers and accepts\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("tl_getmargin", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\" 1")
            + HelpExampleRpc("tl_getmargin", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\", 1")
        );

    const std::string address = ParseAddress(request.params[0]);
    uint32_t propertyId = ParsePropertyId(request.params[1]);

    RequireExistingProperty(propertyId);
    RequireNotContract(propertyId);

    UniValue balanceObj(UniValue::VOBJ);
    ReserveToJSON(address, propertyId, balanceObj, isPropertyDivisible(propertyId));

    return balanceObj;
}

UniValue tl_get_channelreserve(const JSONRPCRequest& request)
{
    if (request.params.size() != 2 || request.fHelp)
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
            + HelpExampleCli("tl_get_channelreserve", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\" 1")
            + HelpExampleRpc("tl_get_channelreserve", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\", 1")
        );

    const std::string address = ParseAddress(request.params[0]);
    uint32_t propertyId = ParsePropertyId(request.params[1]);

    RequireExistingProperty(propertyId);
    RequireNotContract(propertyId);

    UniValue balanceObj(UniValue::VOBJ);
    ChannelToJSON(address, propertyId, balanceObj, isPropertyDivisible(propertyId));

    return balanceObj;
}

UniValue tl_get_channelremaining(const JSONRPCRequest& request)
{
    if (request.params.size() != 3 || request.fHelp)
        throw runtime_error(
            "tl_getchannelremaining \"address\" propertyid\n"
            "\nReturns the token reserve account for a given single address in channel.\n"
            "\nArguments:\n"
            "1. sender               (string, required) the user address\n"
            "2. channel              (string, required) the channel address\n"
            "2. propertyid           (number, required) the contract identifier\n"
            "\nResult:\n"
            "{\n"
            "  \"channel reserve\" : \"n.nnnnnnnn\",   (string) the available balance for single address\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("tl_get_channelreserve", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\", \"Qdj12J6FZgaY34ZNx12pVpTeF9NQdmpGzj\" 1")
            + HelpExampleRpc("tl_get_channelreserve", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\", \"Qdj12J6FZgaY34ZNx12pVpTeF9NQdmpGzj\", 1")
        );

    const std::string address = ParseAddress(request.params[0]);
    const std::string chn = ParseAddress(request.params[1]);
    uint32_t propertyId = ParsePropertyId(request.params[2]);

    RequireExistingProperty(propertyId);
    RequireNotContract(propertyId);

    // checking the amount remaining in the channel
    uint64_t remaining = 0;
    auto it = channels_Map.find(chn);
    if (it != channels_Map.end()){
        const Channel& sChn = it->second;
        remaining = sChn.getRemaining(address, propertyId);
    }

    UniValue balanceObj(UniValue::VOBJ);
    balanceObj.push_back(Pair("channel reserve", FormatMP(isPropertyDivisible(propertyId), remaining)));

    return balanceObj;
}

UniValue tl_getchannel_info(const JSONRPCRequest& request)
{
    if (request.params.size() != 1 || request.fHelp)
        throw runtime_error(
            "tl_getchannel_info \"address\" \n"
            "\nReturns all multisig channel info.\n"
            "\nArguments:\n"
            "1. channel address      (string, required) the address\n"
            "\nResult:\n"
            "{\n"
            "  \"channel address\" : \"n.nnnnnnnn\",   (string) the available balance of the address\n"
            "}\n"
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
    if (request.params.size() != 1 || request.fHelp)
        throw runtime_error(
            "tl_getallbalancesforid propertyid\n"
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
            + HelpExampleCli("tl_getallbalancesforid", "1")
            + HelpExampleRpc("tl_getallbalancesforid", "1")
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
        balanceObj.push_back(Pair("address", address));
        bool nonEmptyBalance = BalanceToJSON(address, propertyId, balanceObj, isDivisible);

        if (nonEmptyBalance) {
            response.push_back(balanceObj);
        }
    }

    return response;
}

UniValue tl_getallbalancesforaddress(const JSONRPCRequest& request)
{
    if (request.params.size() != 1 || request.fHelp)
        throw runtime_error(
            "tl_getallbalancesforaddress \"address\"\n"
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
        balanceObj.push_back(Pair("propertyid", (uint64_t) propertyId));
        bool nonEmptyBalance = BalanceToJSON(address, propertyId, balanceObj, isPropertyDivisible(propertyId));

        if (nonEmptyBalance) {
            response.push_back(balanceObj);
        }
    }

    return response;
}

UniValue tl_getproperty(const JSONRPCRequest& request)
{
    if (request.params.size() != 1 || request.fHelp)
        throw runtime_error(
            "tl_getproperty propertyid\n"
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
            + HelpExampleCli("tl_getproperty", "3")
            + HelpExampleRpc("tl_getproperty", "3")
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
    response.push_back(Pair("propertyid", (uint64_t) propertyId));
    PropertyToJSON(sp, response); // name, data, url, divisible
    KYCToJSON(sp, response);
    response.push_back(Pair("issuer", sp.issuer));
    response.push_back(Pair("creationtxid", strCreationHash));
    response.push_back(Pair("fixedissuance", sp.fixed));
    response.push_back(Pair("totaltokens", strTotalTokens));
    response.push_back(Pair("creation block", sp.init_block));

    if (sp.isNative())
    {
        response.push_back(Pair("notional size", FormatDivisibleShortMP(sp.notional_size)));
        response.push_back(Pair("collateral currency", std::to_string(sp.collateral_currency)));
        response.push_back(Pair("margin requirement", FormatDivisibleShortMP(sp.margin_requirement)));
        response.push_back(Pair("blocks until expiration", std::to_string(sp.blocks_until_expiration)));
        response.push_back(Pair("inverse quoted", std::to_string(sp.inverse_quoted)));
        const int64_t openInterest = getTotalLives(propertyId);
        response.push_back(Pair("open interest", FormatDivisibleMP(openInterest)));
        auto it = cdexlastprice.find(propertyId);
        const int64_t& lastPrice = it->second;
        response.push_back(Pair("last traded price", FormatDivisibleMP(lastPrice)));

    } else if (sp.isOracle()) {
        response.push_back(Pair("notional size", FormatDivisibleShortMP(sp.notional_size)));
        response.push_back(Pair("collateral currency", std::to_string(sp.collateral_currency)));
        response.push_back(Pair("margin requirement", FormatDivisibleShortMP(sp.margin_requirement)));
        response.push_back(Pair("blocks until expiration", std::to_string(sp.blocks_until_expiration)));
        response.push_back(Pair("backup address", sp.backup_address));
        response.push_back(Pair("hight price", FormatDivisibleShortMP(sp.oracle_high)));
        response.push_back(Pair("low price", FormatDivisibleShortMP(sp.oracle_low)));
        response.push_back(Pair("last close price", FormatDivisibleShortMP(sp.oracle_close)));
        response.push_back(Pair("inverse quoted", std::to_string(sp.inverse_quoted)));
        const int64_t openInterest = getTotalLives(propertyId);
        response.push_back(Pair("open interest", FormatDivisibleMP(openInterest)));
        auto it = cdexlastprice.find(propertyId);
        const int64_t& lastPrice = it->second;
        response.push_back(Pair("last traded price", FormatDivisibleMP(lastPrice)));
    } else if (sp.isPegged()) {
        response.push_back(Pair("contract associated",(uint64_t) sp.contract_associated));
        response.push_back(Pair("series", sp.series));
    } else {
        const int64_t ltc_volume = lastVolume(propertyId, false);
        const int64_t token_volume = lastVolume(propertyId, true);
        response.push_back(Pair("last 24h LTC volume", FormatDivisibleMP(ltc_volume)));
        response.push_back(Pair("last 24h Token volume", FormatDivisibleMP(token_volume)));
    }

    return response;
}

UniValue tl_listproperties(const JSONRPCRequest& request)
{
  if (request.fHelp)
    throw runtime_error(
			"tl_listproperties\n"
      "\nArguments:\n"
      "1. verbose                      (number, optional) 1 if more info is needed\n"
			"\nLists all tokens or smart properties.\n"
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
			+ HelpExampleCli("tl_listproperties", "1")
			+ HelpExampleRpc("tl_listproperties", "1")
			);

  uint8_t showVerbose = (request.params.size() == 1) ? ParseBinary(request.params[0]) : 0;

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
          propertyObj.push_back(Pair("propertyid", (uint64_t) propertyId));
          PropertyToJSON(sp, propertyObj); // name, data, url, divisible
          KYCToJSON(sp, propertyObj);

          if(showVerbose)
          {
              propertyObj.push_back(Pair("issuer", sp.issuer));
              propertyObj.push_back(Pair("creationtxid", strCreationHash));
              propertyObj.push_back(Pair("fixedissuance", sp.fixed));
              propertyObj.push_back(Pair("creation block", sp.init_block));
              if (sp.isNative())
              {
                  propertyObj.push_back(Pair("notional size", FormatDivisibleShortMP(sp.notional_size)));
                  propertyObj.push_back(Pair("collateral currency", std::to_string(sp.collateral_currency)));
                  propertyObj.push_back(Pair("margin requirement", FormatDivisibleShortMP(sp.margin_requirement)));
                  propertyObj.push_back(Pair("blocks until expiration", std::to_string(sp.blocks_until_expiration)));
                  propertyObj.push_back(Pair("inverse quoted", std::to_string(sp.inverse_quoted)));
                  const int64_t openInterest = getTotalLives(propertyId);
                  propertyObj.push_back(Pair("open interest", FormatDivisibleMP(openInterest)));
                  auto it = cdexlastprice.find(propertyId);
                  const int64_t& lastPrice = it->second;
                  propertyObj.push_back(Pair("last traded price", FormatDivisibleMP(lastPrice)));
                  const int64_t fundBalance = 0; // NOTE: we need to write this after fundbalance logic
                  propertyObj.push_back(Pair("insurance fund balance", fundBalance));

              } else if (sp.isOracle()) {
                  propertyObj.push_back(Pair("notional size", FormatDivisibleShortMP(sp.notional_size)));
                  propertyObj.push_back(Pair("collateral currency", std::to_string(sp.collateral_currency)));
                  propertyObj.push_back(Pair("margin requirement", FormatDivisibleShortMP(sp.margin_requirement)));
                  propertyObj.push_back(Pair("blocks until expiration", std::to_string(sp.blocks_until_expiration)));
                  propertyObj.push_back(Pair("backup address", sp.backup_address));
                  propertyObj.push_back(Pair("hight price", FormatDivisibleShortMP(sp.oracle_high)));
                  propertyObj.push_back(Pair("low price", FormatDivisibleShortMP(sp.oracle_low)));
                  propertyObj.push_back(Pair("last close price", FormatDivisibleShortMP(sp.oracle_close)));
                  propertyObj.push_back(Pair("inverse quoted", std::to_string(sp.inverse_quoted)));
                  const int64_t openInterest = getTotalLives(propertyId);
                  propertyObj.push_back(Pair("open interest", FormatDivisibleMP(openInterest)));
                  auto it = cdexlastprice.find(propertyId);
                  const int64_t& lastPrice = it->second;
                  propertyObj.push_back(Pair("last traded price", FormatDivisibleMP(lastPrice)));
                  const int64_t fundBalance = 0; // NOTE: we need to write this after fundbalance logic is done
                  propertyObj.push_back(Pair("insurance fund balance", FormatDivisibleMP(fundBalance)));

              } else if (sp.isPegged()) {
                  propertyObj.push_back(Pair("contract associated",(uint64_t) sp.contract_associated));
                  propertyObj.push_back(Pair("series", sp.series));
              } else {
                  const int64_t ltc_volume = lastVolume(propertyId, false);
                  const int64_t token_volume = lastVolume(propertyId, true);
                  propertyObj.push_back(Pair("last 24h LTC volume", FormatDivisibleMP(ltc_volume)));
                  propertyObj.push_back(Pair("last 24h Token volume", FormatDivisibleMP(token_volume)));
                  propertyObj.push_back(Pair("totaltokens", strTotalTokens));
              }
          }

      }

      response.push_back(propertyObj);
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

  LOCK(cs_tally);

  const uint32_t nextSPID = _my_sps->peekNextSPID();
  for (uint32_t propertyId = 1; propertyId < nextSPID; propertyId++)
  {
      CMPSPInfo::Entry sp;
      if (_my_sps->getSP(propertyId, sp))
      {
          if(!sp.isNative())
              continue;

          UniValue propertyObj(UniValue::VOBJ);
          propertyObj.push_back(Pair("propertyid", (uint64_t) propertyId));
          ContractToJSON(sp, propertyObj); // name, data, url,...
          response.push_back(propertyObj);
          const int64_t openInterest = getTotalLives(propertyId);
          propertyObj.push_back(Pair("open interest", FormatDivisibleMP(openInterest)));
          auto it = cdexlastprice.find(propertyId);
          const int64_t& lastPrice = it->second;
          propertyObj.push_back(Pair("last traded price", FormatDivisibleMP(lastPrice)));
          const int64_t fundBalance = 0; // NOTE: we need to write this after fundbalance logic is done
          propertyObj.push_back(Pair("insurance fund balance", fundBalance));

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

  LOCK(cs_tally);

  const uint32_t nextSPID = _my_sps->peekNextSPID();
  for (uint32_t propertyId = 1; propertyId < nextSPID; propertyId++)
  {
      CMPSPInfo::Entry sp;

      if (_my_sps->getSP(propertyId, sp))
      {
          if(!sp.isOracle())
              continue;

          UniValue propertyObj(UniValue::VOBJ);
          propertyObj.push_back(Pair("propertyid", (uint64_t) propertyId));
          OracleToJSON(sp, propertyObj); // name, data, url,...
          response.push_back(propertyObj);
          const int64_t openInterest = getTotalLives(propertyId);
          propertyObj.push_back(Pair("open interest", FormatDivisibleMP(openInterest)));
          auto it = cdexlastprice.find(propertyId);
          const int64_t& lastPrice = it->second;
          propertyObj.push_back(Pair("last traded price", FormatDivisibleMP(lastPrice)));
          const int64_t fundBalance = 0; // NOTE: we need to write this after fundbalance logic is done
          propertyObj.push_back(Pair("insurance fund balance", fundBalance));
      }
  }

  return response;
}

UniValue tl_getcrowdsale(const JSONRPCRequest& request)
{
    if (request.params.size() < 1 || request.params.size() > 2 || request.fHelp)
        throw runtime_error(
            "tl_getcrowdsale propertyid ( verbose )\n"
            "\nReturns information about a crowdsale.\n"
            "\nArguments:\n"
            "1. propertyid           (number, required) the identifier of the crowdsale\n"
            "2. verbose              (boolean, optional) list crowdsale participants (default: false)\n"
            "\nResult:\n"
            "{\n"
            "  \"propertyid\" : n,                     (number) the identifier of the crowdsale\n"
            "  \"name\" : \"name\",                      (string) the name of the tokens issued via the crowdsale\n"
            "  \"active\" : true|false,                (boolean) whether the crowdsale is still active\n"
            "  \"issuer\" : \"address\",                 (string) the Bitcoin address of the issuer on record\n"
            "  \"propertyiddesired\" : n,              (number) the identifier of the tokens eligible to participate in the crowdsale\n"
            "  \"tokensperunit\" : \"n.nnnnnnnn\",       (string) the amount of tokens granted per unit invested in the crowdsale\n"
            "  \"earlybonus\" : n,                     (number) an early bird bonus for participants in percent per week\n"
            "  \"percenttoissuer\" : n,                (number) a percentage of tokens that will be granted to the issuer\n"
            "  \"starttime\" : nnnnnnnnnn,             (number) the start time of the of the crowdsale as Unix timestamp\n"
            "  \"deadline\" : nnnnnnnnnn,              (number) the deadline of the crowdsale as Unix timestamp\n"
            "  \"amountraised\" : \"n.nnnnnnnn\",        (string) the amount of tokens invested by participants\n"
            "  \"tokensissued\" : \"n.nnnnnnnn\",        (string) the total number of tokens issued via the crowdsale\n"
            "  \"addedissuertokens\" : \"n.nnnnnnnn\",   (string) the amount of tokens granted to the issuer as bonus\n"
            "  \"closedearly\" : true|false,           (boolean) whether the crowdsale ended early (if not active)\n"
            "  \"maxtokens\" : true|false,             (boolean) whether the crowdsale ended early due to reaching the limit of max. issuable tokens (if not active)\n"
            "  \"endedtime\" : nnnnnnnnnn,             (number) the time when the crowdsale ended (if closed early)\n"
            "  \"closetx\" : \"hash\",                   (string) the hex-encoded hash of the transaction that closed the crowdsale (if closed manually)\n"
            "  \"participanttransactions\": [          (array of JSON objects) a list of crowdsale participations (if verbose=true)\n"
            "    {\n"
            "      \"txid\" : \"hash\",                      (string) the hex-encoded hash of participation transaction\n"
            "      \"amountsent\" : \"n.nnnnnnnn\",          (string) the amount of tokens invested by the participant\n"
            "      \"participanttokens\" : \"n.nnnnnnnn\",   (string) the tokens granted to the participant\n"
            "      \"issuertokens\" : \"n.nnnnnnnn\"         (string) the tokens granted to the issuer as bonus\n"
            "    },\n"
            "    ...\n"
            "  ]\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("tl_getcrowdsale", "3 true")
            + HelpExampleRpc("tl_getcrowdsale", "3, true")
        );

    uint32_t propertyId = ParsePropertyId(request.params[0]);
    bool showVerbose = (request.params.size() > 1) ? request.params[1].get_bool() : false;

    RequireExistingProperty(propertyId);
    RequireCrowdsale(propertyId);

    CMPSPInfo::Entry sp;
    {
        LOCK(cs_tally);
        if (!_my_sps->getSP(propertyId, sp)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Property identifier does not exist");
        }
    }

    const uint256& creationHash = sp.txid;

    CTransactionRef tx;
    uint256 hashBlock;
    if (!GetTransaction(creationHash, tx, Params().GetConsensus(), hashBlock, true)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available about transaction");
    }

    UniValue response(UniValue::VOBJ);
    bool active = isCrowdsaleActive(propertyId);
    std::map<uint256, std::vector<int64_t> > database;

    if (active) {
        bool crowdFound = false;

        LOCK(cs_tally);

        for (CrowdMap::const_iterator it = my_crowds.begin(); it != my_crowds.end(); ++it) {
            const CMPCrowd& crowd = it->second;
            if (propertyId == crowd.getPropertyId()) {
                crowdFound = true;
                database = crowd.getDatabase();
            }
        }
        if (!crowdFound) {
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Crowdsale is flagged active but cannot be retrieved");
        }
    } else {
        database = sp.historicalData;
    }

    int64_t tokensIssued = getTotalTokens(propertyId);
    const std::string& txidClosed = sp.txid_close.GetHex();

    int64_t startTime = -1;
    if (!hashBlock.IsNull() && GetBlockIndex(hashBlock)) {
      startTime = GetBlockIndex(hashBlock)->nTime;
    }

    // note the database is already deserialized here and there is minimal performance penalty to iterate recipients to calculate amountRaised
    int64_t amountRaised = 0;
    uint16_t propertyIdType = isPropertyDivisible(propertyId) ? ALL_PROPERTY_TYPE_DIVISIBLE : ALL_PROPERTY_TYPE_INDIVISIBLE;
    uint16_t desiredIdType = isPropertyDivisible(sp.property_desired) ? ALL_PROPERTY_TYPE_DIVISIBLE : ALL_PROPERTY_TYPE_INDIVISIBLE;
    std::map<std::string, UniValue> sortMap;
    for (std::map<uint256, std::vector<int64_t> >::const_iterator it = database.begin(); it != database.end(); it++) {
        UniValue participanttx(UniValue::VOBJ);
        std::string txid = it->first.GetHex();
        amountRaised += it->second.at(0);
        participanttx.push_back(Pair("txid", txid));
        participanttx.push_back(Pair("amountsent", FormatByType(it->second.at(0), desiredIdType)));
        participanttx.push_back(Pair("participanttokens", FormatByType(it->second.at(2), propertyIdType)));
        participanttx.push_back(Pair("issuertokens", FormatByType(it->second.at(3), propertyIdType)));
        std::string sortKey = strprintf("%d-%s", it->second.at(1), txid);
        sortMap.insert(std::make_pair(sortKey, participanttx));
    }

    response.push_back(Pair("propertyid", (uint64_t) propertyId));
    response.push_back(Pair("name", sp.name));
    response.push_back(Pair("active", active));
    response.push_back(Pair("issuer", sp.issuer));
    response.push_back(Pair("propertyiddesired", (uint64_t) sp.property_desired));
    response.push_back(Pair("tokensperunit", FormatMP(propertyId, sp.num_tokens)));
    response.push_back(Pair("earlybonus", sp.early_bird));
    response.push_back(Pair("percenttoissuer", sp.percentage));
    response.push_back(Pair("starttime", startTime));
    response.push_back(Pair("deadline", sp.deadline));
    response.push_back(Pair("amountraised", FormatMP(sp.property_desired, amountRaised)));
    response.push_back(Pair("tokensissued", FormatMP(propertyId, tokensIssued)));
    response.push_back(Pair("addedissuertokens", FormatMP(propertyId, sp.missedTokens)));

    // TODO: return fields every time?
    if (!active){
        response.push_back(Pair("closedearly", sp.close_early));
        response.push_back(Pair("maxtokens", sp.max_tokens));
    }

    if (sp.close_early) response.push_back(Pair("endedtime", sp.timeclosed));
    if (sp.close_early && !sp.max_tokens) response.push_back(Pair("closetx", txidClosed));

    if (showVerbose) {
        UniValue participanttxs(UniValue::VARR);
        for (std::map<std::string, UniValue>::iterator it = sortMap.begin(); it != sortMap.end(); ++it) {
            participanttxs.push_back(it->second);
        }
        response.push_back(Pair("participanttransactions", participanttxs));
    }

    return response;
}

UniValue tl_getactivecrowdsales(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw runtime_error(
            "tl_getactivecrowdsales\n"
            "\nLists currently active crowdsales.\n"
            "\nResult:\n"
            "[                                 (array of JSON objects)\n"
            "  {\n"
            "    \"propertyid\" : n,                 (number) the identifier of the crowdsale\n"
            "    \"name\" : \"name\",                  (string) the name of the tokens issued via the crowdsale\n"
            "    \"issuer\" : \"address\",             (string) the Bitcoin address of the issuer on record\n"
            "    \"propertyiddesired\" : n,          (number) the identifier of the tokens eligible to participate in the crowdsale\n"
            "    \"tokensperunit\" : \"n.nnnnnnnn\",   (string) the amount of tokens granted per unit invested in the crowdsale\n"
            "    \"earlybonus\" : n,                 (number) an early bird bonus for participants in percent per week\n"
            "    \"percenttoissuer\" : n,            (number) a percentage of tokens that will be granted to the issuer\n"
            "    \"starttime\" : nnnnnnnnnn,         (number) the start time of the of the crowdsale as Unix timestamp\n"
            "    \"deadline\" : nnnnnnnnnn           (number) the deadline of the crowdsale as Unix timestamp\n"
            "  },\n"
            "  ...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("tl_getactivecrowdsales", "")
            + HelpExampleRpc("tl_getactivecrowdsales", "")
        );

    UniValue response(UniValue::VARR);

    LOCK2(cs_main, cs_tally);

    for (CrowdMap::const_iterator it = my_crowds.begin(); it != my_crowds.end(); ++it) {
        const CMPCrowd& crowd = it->second;
        uint32_t propertyId = crowd.getPropertyId();

        CMPSPInfo::Entry sp;
        if (!_my_sps->getSP(propertyId, sp)) {
            continue;
        }

        const uint256& creationHash = sp.txid;

        CTransactionRef tx;
        uint256 hashBlock;
        if (!GetTransaction(creationHash, tx, Params().GetConsensus(), hashBlock, true)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available about transaction");
        }

        int64_t startTime = -1;
        if (!hashBlock.IsNull() && GetBlockIndex(hashBlock)) {
            startTime = GetBlockIndex(hashBlock)->nTime;
        }

        UniValue responseObj(UniValue::VOBJ);
        responseObj.push_back(Pair("propertyid", (uint64_t) propertyId));
        responseObj.push_back(Pair("name", sp.name));
        responseObj.push_back(Pair("issuer", sp.issuer));
        responseObj.push_back(Pair("propertyiddesired", (uint64_t) sp.property_desired));
        responseObj.push_back(Pair("tokensperunit", FormatMP(propertyId, sp.num_tokens)));
        responseObj.push_back(Pair("earlybonus", sp.early_bird));
        responseObj.push_back(Pair("percenttoissuer", sp.percentage));
        responseObj.push_back(Pair("starttime", startTime));
        responseObj.push_back(Pair("deadline", sp.deadline));
        response.push_back(responseObj);
    }

    return response;
}

UniValue tl_getgrants(const JSONRPCRequest& request)
{
    if (request.params.size() != 1 || request.fHelp)
        throw runtime_error(
            "tl_getgrants propertyid\n"
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
            + HelpExampleCli("tl_getgrants", "31")
            + HelpExampleRpc("tl_getgrants", "31")
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
            granttx.push_back(Pair("txid", txid));
            granttx.push_back(Pair("grant", FormatMP(propertyId, grantedTokens)));
            issuancetxs.push_back(granttx);
        }

        if (revokedTokens > 0) {
            UniValue revoketx(UniValue::VOBJ);
            revoketx.push_back(Pair("txid", txid));
            revoketx.push_back(Pair("revoke", FormatMP(propertyId, revokedTokens)));
            issuancetxs.push_back(revoketx);
        }
    }

    response.push_back(Pair("propertyid", (uint64_t) propertyId));
    response.push_back(Pair("name", sp.name));
    response.push_back(Pair("issuer", sp.issuer));
    response.push_back(Pair("creationtxid", creationHash.GetHex()));
    response.push_back(Pair("totaltokens", FormatMP(propertyId, totalTokens)));
    response.push_back(Pair("issuances", issuancetxs));

    return response;
}

UniValue tl_listblocktransactions(const JSONRPCRequest& request)
{
    if (request.params.size() != 1 || request.fHelp)
        throw runtime_error(
            "tl_listblocktransactions index\n"
            "\nLists all Trade Layer transactions in a block.\n"
            "\nArguments:\n"
            "1. index                (number, required) the block height or block index\n"
            "\nResult:\n"
            "[                       (array of string)\n"
            "  \"hash\",                 (string) the hash of the transaction\n"
            "  ...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_listblocktransactions", "279007")
            + HelpExampleRpc("tl_listblocktransactions", "279007")
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
  if (request.params.size() != 1 || request.fHelp)
    throw runtime_error(
			"tl_gettransaction \"txid\"\n"
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
  if (populateResult != 0) PopulateFailure(populateResult);

  return txobj;
}

UniValue tl_listtransactions(const JSONRPCRequest& request)
{
    if (request.params.size() > 5 || request.fHelp)
        throw runtime_error(
            "tl_listtransactions ( \"address\" count skip startblock endblock )\n"
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
    if (request.params.size() > 1 || request.fHelp)
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
    infoResponse.push_back(Pair("tradelayer_version_int", TL_VERSION));
    infoResponse.push_back(Pair("tradelayer_coreversion", TradeLayerVersion()));
    infoResponse.push_back(Pair("litecoinversion", BitcoinCoreVersion()));
    infoResponse.push_back(Pair("commitinfo", BuildCommit()));

    // provide the current block details
    int block = GetHeight();
    int64_t blockTime = GetLatestBlockTime();

    LOCK(cs_tally);

    int blockMPTransactions = p_txlistdb->getMPTransactionCountBlock(block);
    int totalMPTransactions = p_txlistdb->getMPTransactionCountTotal();
    infoResponse.push_back(Pair("block", block));
    infoResponse.push_back(Pair("blocktime", blockTime));
    infoResponse.push_back(Pair("blocktransactions", blockMPTransactions));
    infoResponse.push_back(Pair("totaltransactions", totalMPTransactions));

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
        alertResponse.push_back(Pair("alerttypeint", alert.alert_type));
        alertResponse.push_back(Pair("alerttype", alertTypeStr));
        alertResponse.push_back(Pair("alertexpiry", FormatIndivisibleMP(alert.alert_expiry)));
        alertResponse.push_back(Pair("alertmessage", alert.alert_message));
        alerts.push_back(alertResponse);
    }
    infoResponse.push_back(Pair("alerts", alerts));

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
        actObj.push_back(Pair("featureid", pendingAct.featureId));
        actObj.push_back(Pair("featurename", pendingAct.featureName));
        actObj.push_back(Pair("activationblock", pendingAct.activationBlock));
        actObj.push_back(Pair("minimumversion", (uint64_t)pendingAct.minClientVersion));
        arrayPendingActivations.push_back(actObj);
    }

    UniValue arrayCompletedActivations(UniValue::VARR);
    std::vector<FeatureActivation> vecCompletedActivations = GetCompletedActivations();
    for (std::vector<FeatureActivation>::iterator it = vecCompletedActivations.begin(); it != vecCompletedActivations.end(); ++it) {
        UniValue actObj(UniValue::VOBJ);
        FeatureActivation completedAct = *it;
        actObj.push_back(Pair("featureid", completedAct.featureId));
        actObj.push_back(Pair("featurename", completedAct.featureName));
        actObj.push_back(Pair("activationblock", completedAct.activationBlock));
        actObj.push_back(Pair("minimumversion", (uint64_t)completedAct.minClientVersion));
        arrayCompletedActivations.push_back(actObj);
    }

    response.push_back(Pair("pendingactivations", arrayPendingActivations));
    response.push_back(Pair("completedactivations", arrayCompletedActivations));

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

        response.push_back(Pair("block", block));
        response.push_back(Pair("blockhash", blockHash.GetHex()));
        response.push_back(Pair("consensushash", consensusHash.GetHex()));
    }

    return response;
}

bool PositionToJSON(const std::string& address, uint32_t property, UniValue& balance_obj, bool divisible)
{
    int64_t longPosition  = getMPbalance(address, property, POSITIVE_BALANCE);
    int64_t shortPosition = getMPbalance(address, property, NEGATIVE_BALANCE);
    balance_obj.push_back(Pair("longPosition", longPosition));
    balance_obj.push_back(Pair("shortPosition", shortPosition));
    // balance_obj.push_back(Pair("liquidationPrice", FormatByType(liqPrice,2)));

    return true;
}

// in progress
bool FullPositionToJSON(const std::string& address, uint32_t property, UniValue& position_obj, bool divisible, CMPSPInfo::Entry sProperty)
{
  // const int64_t factor = 100000000;
  int64_t longPosition = getMPbalance(address, property, POSITIVE_BALANCE);
  int64_t shortPosition = getMPbalance(address, property, NEGATIVE_BALANCE);
  int64_t valueLong = longPosition * (uint64_t) sProperty.notional_size;
  int64_t valueShort = shortPosition * (uint64_t) sProperty.notional_size;

  position_obj.push_back(Pair("longPosition", FormatByType(longPosition, 1)));
  position_obj.push_back(Pair("shortPosition", FormatByType(shortPosition, 1)));
  // position_obj.push_back(Pair("liquidationPrice", FormatByType(liqPrice,2)));
  position_obj.push_back(Pair("valueLong", FormatByType(valueLong, 1)));
  position_obj.push_back(Pair("valueShort", FormatByType(valueShort, 1)));

  return true;

}

UniValue tl_getfullposition(const JSONRPCRequest& request)
{
  if (request.params.size() != 2 || request.fHelp) {
    throw runtime_error(
			"tl_getfullposition \"address\" propertyid\n"
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
			+ HelpExampleCli("tl_getfullposition", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\" 1")
			+ HelpExampleRpc("tl_getfullposition", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\", 1")
			);
  }

  const std::string address = ParseAddress(request.params[0]);
  uint32_t propertyId  = ParseNameOrId(request.params[1]);

  RequireContract(propertyId);

  UniValue positionObj(UniValue::VOBJ);

  CMPSPInfo::Entry sp;
  {
    LOCK(cs_tally);
    if (!_my_sps->getSP(propertyId, sp)) {
      throw JSONRPCError(RPC_INVALID_PARAMETER, "Property identifier does not exist");
    }
  }

  positionObj.push_back(Pair("symbol", sp.name));
  positionObj.push_back(Pair("notional_size", (uint64_t) sp.notional_size)); // value of position = short or long position (balance) * notional_size
  // PTJ -> short/longPosition /liquidation price
  // bool flag = false;

  FullPositionToJSON(address, propertyId, positionObj,sp.isContract(), sp);

  LOCK(cs_tally);

  double upnl = addrs_upnlc[propertyId][address];

  if (upnl >= 0) {
    positionObj.push_back(Pair("positiveupnl", upnl));
    positionObj.push_back(Pair("negativeupnl", FormatByType(0,2)));
  } else {
    positionObj.push_back(Pair("positiveupnl", FormatByType(0,2)));
    positionObj.push_back(Pair("negativeupnl", upnl));
  }

  // pnl
  uint32_t& collateralCurrency = sp.collateral_currency;
  uint64_t realizedProfits  = static_cast<uint64_t>(COIN * getMPbalance(address, collateralCurrency, REALIZED_PROFIT));
  uint64_t realizedLosses  = static_cast<uint64_t>(COIN * getMPbalance(address, collateralCurrency, REALIZED_LOSSES));

  if (realizedProfits > 0 && realizedLosses == 0) {
    positionObj.push_back(Pair("positivepnl", FormatByType(realizedProfits,2)));
    positionObj.push_back(Pair("negativepnl", FormatByType(0,2)));
  } else if (realizedLosses > 0 && realizedProfits == 0) {
    positionObj.push_back(Pair("positivepnl", FormatByType(0,2)));
    positionObj.push_back(Pair("negativepnl", FormatByType(realizedProfits,2)));
  } else if (realizedLosses == 0 && realizedProfits == 0) {
    positionObj.push_back(Pair("positivepnl", FormatByType(0,2)));
    positionObj.push_back(Pair("negativepnl", FormatByType(0,2)));
  }

  return positionObj;
}


UniValue tl_getposition(const JSONRPCRequest& request)
{
  if (request.params.size() != 2 || request.fHelp)
    throw runtime_error(
			"tl_getbalance \"address\" propertyid\n"
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
			+ HelpExampleCli("tl_getposition", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\" 1")
			+ HelpExampleRpc("tl_getposition", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\", 1")
			);

  const std::string address = ParseAddress(request.params[0]);
  uint32_t contractId = ParseNameOrId(request.params[1]);

  RequireContract(contractId);

  UniValue balanceObj(UniValue::VOBJ);
  PositionToJSON(address, contractId, balanceObj, isPropertyContract(contractId));

  return balanceObj;
}

UniValue tl_getcontract_reserve(const JSONRPCRequest& request)
{
    if (request.params.size() != 2 || request.fHelp)
        throw runtime_error(
            "tl_getcotract_reserve \"address\" propertyid\n"
            "\nReturns the reserves contracts for the future contract for a given address and property.\n"
            "\nArguments:\n"
            "1. address              (string, required) the address\n"
            "2. name or id           (string, required) the future contract name or identifier\n"
            "\nResult:\n"
            "{\n"
            "  \"reserve\" : \"n.nnnnnnnn\",   (string) amount of contracts in reserve for the address \n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("tl_getcotract_reserve", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\" 1")
            + HelpExampleRpc("tl_getcotract_reserve", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\", 1")
        );

    const std::string address = ParseAddress(request.params[0]);
    uint32_t contractId = ParseNameOrId(request.params[1]);

    RequireContract(contractId);

    UniValue balanceObj(UniValue::VOBJ);
    int64_t reserve = getMPbalance(address, contractId, CONTRACTDEX_RESERVE);
    balanceObj.push_back(Pair("contract reserve", FormatByType(reserve,2)));
    return balanceObj;
}


UniValue tl_getorderbook(const JSONRPCRequest& request)
{
    if (request.params.size() < 1 || request.params.size() > 2 || request.fHelp)
        throw runtime_error(
            "tl_getorderbook propertyid ( propertyid )\n"
            "\nList active offers on the distributed token exchange.\n"
            "\nArguments:\n"
            "1. propertyid           (number, required) filter orders by property identifier for sale\n"
            "2. propertyid           (number, optional) filter orders by property identifier desired\n"
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
            + HelpExampleCli("tl_getorderbook", "2")
            + HelpExampleRpc("tl_getorderbook", "2")
			    );


    bool filterDesired = (request.params.size() > 1);
    uint32_t propertyIdForSale = ParsePropertyId(request.params[0]);
    uint32_t propertyIdDesired = 0;

    RequireExistingProperty(propertyIdForSale);
    RequireNotContract(propertyIdForSale);

    if (filterDesired) {
        propertyIdDesired = ParsePropertyId(request.params[1]);
        RequireExistingProperty(propertyIdDesired);
        RequireNotContract(propertyIdDesired);
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
  if (request.params.size() != 2 || request.fHelp)
    throw runtime_error(
			"tl_getcontract_orderbook contractid tradingaction\n"
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
			+ HelpExampleCli("tl_getcontract_orderbook", "2" "1")
			+ HelpExampleRpc("tl_getcontract_orderbook", "2" "1")
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
  if (request.params.size() != 1 || request.fHelp)
    throw runtime_error(
			"tl_gettradehistory contractid propertyid ( count )\n"
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
			+ HelpExampleCli("tl_gettradehistory", "1")
			+ HelpExampleRpc("tl_gettradehistory", "1")
			);

  // obtain property identifiers for pair & check valid parameters
  uint32_t contractId  = ParseNameOrId(request.params[0]);

  RequireContract(contractId);

  // request pair trade history from trade db
  UniValue response(UniValue::VARR);

  LOCK(cs_tally);

  t_tradelistdb->getMatchingTrades(contractId, response);

  return response;
}

UniValue tl_gettradehistory_unfiltered(const JSONRPCRequest& request)
{
    if (request.params.size() != 1 || request.fHelp)
        throw runtime_error(
            "tl_gettradehistory contractid propertyid ( count )\n"
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
            + HelpExampleCli("tl_gettradehistoryforpair", "1 12 500")
            + HelpExampleRpc("tl_gettradehistoryforpair", "1, 12, 500")
			    );

    // obtain property identifiers for pair & check valid parameters
    uint32_t contractId = ParseNameOrId(request.params[0]);

    RequireContract(contractId);

    // request pair trade history from trade db
    UniValue response(UniValue::VARR);
    LOCK(cs_tally);

    // gets unfiltered list of trades
    t_tradelistdb->getMatchingTradesUnfiltered(contractId,response);
    return response;
}

UniValue tl_getpeggedhistory(const JSONRPCRequest& request)
{
    if (request.params.size() != 1 || request.fHelp)
        throw runtime_error(
            "tl_getpeggedhistory propertyid ( count )\n"
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
            + HelpExampleCli("tl_getpeggedhistory", "1")
            + HelpExampleRpc("tl_getpeggedhistory", "1")
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
            "\nRetrieves the ALL price in metadex (in dUSD) .\n"

            "\nResult:\n"
            "[                                      (array of JSON objects)\n"
            "  {\n"
            "    \"price\" : nnnnnn,                       (number) the price of 1 ALL in dUSD\n"
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

    auto it = market_priceMap.find(static_cast<uint32_t>(ALL));
    if (it != market_priceMap.end())
    {
        const auto &auxMap = it->second;
        auto itt = auxMap.find(static_cast<uint32_t>(dUSD));
        if (itt != auxMap.end())
            allPrice = itt->second;
    }

    balanceObj.push_back(Pair("unitprice", FormatByType(static_cast<uint64_t>(allPrice),2)));
    return balanceObj;
}

UniValue tl_getupnl(const JSONRPCRequest& request)
{
  if (request.params.size() < 2 || request.params.size() > 3 || request.fHelp) {
    throw runtime_error(
			"tl_getupnl address name\n"
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
			+ HelpExampleCli("tl_getupnl", "address1 , Contract 1, 1")
			+ HelpExampleRpc("tl_getupnl", "address2 , Contract 1, 1")
			);
  }

  const std::string address = ParseAddress(request.params[0]);
  uint32_t contractId = ParseNameOrId(request.params[1]);
  bool showVerbose = (request.params.size() > 2 && ParseBinary(request.params[2]) == 1) ? true : false;


  RequireExistingProperty(contractId);
  RequireContract(contractId);

  // if position is 0, upnl is 0
  RequirePosition(address, contractId);

  // request trade history from trade db
  UniValue response(UniValue::VARR);

  t_tradelistdb->getUpnInfo(address, contractId, response, showVerbose);


  return response;
}

UniValue tl_getpnl(const JSONRPCRequest& request)
{
  if (request.params.size() != 2 || request.fHelp)
    throw runtime_error(
			"tl_getpnl address contractid\n"
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
			+ HelpExampleCli("tl_getpnl", "address 12 ")
			+ HelpExampleRpc("tl_getpnl", "address, 500")
			);

  const std::string address = ParseAddress(request.params[0]);
  uint32_t contractId = ParsePropertyId(request.params[1]);

  RequireExistingProperty(contractId);

  UniValue balanceObj(UniValue::VOBJ);
  int64_t upnl  = getMPbalance(address, contractId, REALIZED_PROFIT);
  int64_t nupnl  = getMPbalance(address, contractId, REALIZED_LOSSES);

  if (upnl > 0 && nupnl == 0) {
    balanceObj.push_back(Pair("positivepnl", FormatByType(static_cast<uint64_t>(upnl),2)));
    balanceObj.push_back(Pair("negativepnl", FormatByType(0,2)));
  } else if (nupnl > 0 && upnl == 0) {
    balanceObj.push_back(Pair("positivepnl", FormatByType(0,2)));
    balanceObj.push_back(Pair("negativepnl", FormatByType(static_cast<uint64_t>(nupnl),2)));
  } else{
    balanceObj.push_back(Pair("positivepnl", FormatByType(0,2)));
    balanceObj.push_back(Pair("negativepnl", FormatByType(0,2)));
  }
  return balanceObj;
}

UniValue tl_getmarketprice(const JSONRPCRequest& request)
{
  if (request.params.size() != 1 || request.fHelp)
    throw runtime_error(
			"tl_getmarketprice addres contractid\n"
			"\nRetrieves the last match price on the distributed contract exchange for the specified market.\n"
			"\nArguments:\n"
			"1. contractid           (number, required) the id of future contract\n"
			"\nResult:\n"
			"[                                      (array of JSON objects)\n"
			"  {\n"
			"    \"block\" : nnnnnn,                      (number) the index of the block that contains the trade match\n"
			"    \"unitprice\" : \"n.nnnnnnnnnnn...\" ,   (string) the unit price used to execute this trade (received/sold)\n"
			"    \"price\" : \"n.nnnnnnnnnnn...\",        (string) the inverse unit price (sold/received)\n"
			"  ...\n"
			"]\n"
			"\nExamples:\n"
			+ HelpExampleCli("tl_getmarketprice", "12" )
			+ HelpExampleRpc("tl_getmarketprice", "500")
			);


  uint32_t contractId = ParsePropertyId(request.params[0]);

  RequireExistingProperty(contractId);
  RequireContract(contractId);

  UniValue balanceObj(UniValue::VOBJ);
  // int index = static_cast<unsigned int>(contractId);
  // int64_t price = marketP[index];
  int64_t price = 0;

  balanceObj.push_back(Pair("price", FormatByType(price,2)));

  return balanceObj;
}

UniValue tl_getactivedexsells(const JSONRPCRequest& request)
{
    if (request.params.size() > 1 || request.fHelp)
        throw runtime_error(
            "tl_getactivedexsells ( address )\n"
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
            + HelpExampleCli("tl_getactivedexsells", "")
            + HelpExampleRpc("tl_getactivedexsells", "")
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
        int64_t sellBitcoinDesired = offer.getLTCDesiredOriginal(); //badly named - "Original" implies off the wire, but is amended amount
        int64_t amountAvailable = getMPbalance(seller, propertyId, SELLOFFER_RESERVE);
        int64_t amountAccepted = getMPbalance(seller, propertyId, ACCEPT_RESERVE);
        uint8_t option = offer.getOption();

        // calculate unit price and updated amount of bitcoin desired
        arith_uint256 aUnitPrice = 0;
        if ((sellOfferAmount > 0) && (sellBitcoinDesired > 0)) {
            aUnitPrice = (COIN * (ConvertTo256(sellBitcoinDesired)) / ConvertTo256(sellOfferAmount)) ; // divide by zero protection
        }

        int64_t unitPrice = (isPropertyDivisible(propertyId)) ? ConvertTo64(aUnitPrice) : ConvertTo64(aUnitPrice) / COIN;

        PrintToLog("%s(): sellBitcoinDesired: %d, sellOfferAmount : %d, unitPrice: %d\n",__func__, sellBitcoinDesired, sellOfferAmount, unitPrice);

        int64_t bitcoinDesired = calculateDesiredLTC(sellOfferAmount, sellBitcoinDesired, amountAvailable);
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
                    matchedAccept.push_back(Pair("seller", buyer));
                    matchedAccept.push_back(Pair("amount", FormatMP(propertyId, amountAccepted)));
                    matchedAccept.push_back(Pair("ltcstoreceive", FormatDivisibleMP(ltcsreceived)));
                } else if (option == 2) {
                    matchedAccept.push_back(Pair("buyer", buyer));
                    matchedAccept.push_back(Pair("amountdesired", FormatMP(propertyId, amountAccepted)));
                    matchedAccept.push_back(Pair("ltcstopay", FormatDivisibleMP(amountToPayInLTC)));
                }

                matchedAccept.push_back(Pair("block", blockOfAccept));
                matchedAccept.push_back(Pair("blocksleft", blocksLeftToPay));
                acceptsMatched.push_back(matchedAccept);
            }
        }

        UniValue responseObj(UniValue::VOBJ);
        responseObj.push_back(Pair("txid", txid));
        responseObj.push_back(Pair("propertyid", (uint64_t) propertyId));
        responseObj.push_back(Pair("action", (uint64_t) offer.getOption()));
        if (option == 2) {
            responseObj.push_back(Pair("seller", seller));
            responseObj.push_back(Pair("ltcsdesired", FormatDivisibleMP(bitcoinDesired)));
            responseObj.push_back(Pair("amountavailable", FormatMP(propertyId, amountAvailable)));

        } else if (option == 1){
            responseObj.push_back(Pair("buyer", seller));
            responseObj.push_back(Pair("ltcstopay", FormatDivisibleMP(sellBitcoinDesired - sumLtcs)));
            responseObj.push_back(Pair("amountdesired", FormatMP(propertyId, sellOfferAmount - sumAccepted)));
            responseObj.push_back(Pair("accepted", FormatMP(propertyId, amountAccepted)));

        }
        responseObj.push_back(Pair("unitprice", FormatDivisibleMP(unitPrice)));
        responseObj.push_back(Pair("timelimit", timeLimit));
        responseObj.push_back(Pair("minimumfee", FormatDivisibleMP(minFee)));
        responseObj.push_back(Pair("accepts", acceptsMatched));

        // add sell object into response array
        response.push_back(responseObj);
    }

    return response;
}


UniValue tl_getsum_upnl(const JSONRPCRequest& request)
{
  if (request.params.size() != 1 || request.fHelp)
    throw runtime_error(
			"tl_getsum_upnl addres \n"
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
			+ HelpExampleCli("tl_getsum_upnl", "address1" )
			+ HelpExampleRpc("tl_getsum_upnl", "address2")
			);

  const std::string address = ParseAddress(request.params[0]);

  UniValue balanceObj(UniValue::VOBJ);

  auto it = sum_upnls.find(address);
  int64_t upnl = (it != sum_upnls.end()) ? it->second : 0;

  balanceObj.push_back(Pair("upnl", FormatByType(upnl,2)));

  return balanceObj;
}

UniValue tl_check_commits(const JSONRPCRequest& request)
{
  if (request.params.size() != 1 || request.fHelp)
    throw runtime_error(
			"tl_check_commits sender address \n"
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
			+ HelpExampleCli("tl_check_commits", "address")
			+ HelpExampleRpc("tl_check_commits", "address")
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
  if (request.params.size() != 1 || request.fHelp)
    throw runtime_error(
			"tl_check_withdrawals senderAddress \n"
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
			+ HelpExampleCli("tl_check_withdrawals", "address1")
			+ HelpExampleRpc("tl_check_withdrawals", "address1")
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
  if (request.params.size() != 1 || request.fHelp)
    throw runtime_error(
			"tl_check_kyc senderAddress \n"
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
			+ HelpExampleCli("tl_check_kyc", "address1")
			+ HelpExampleRpc("tl_check_kyc", "address1")
			);

  // obtain property identifiers for pair & check valid parameters
  std::string result;
  const std::string address = ParseAddress(request.params[0]);
  // int registered = static_cast<int>(ParseNewValues(request.params[1]));
  // RequireContract(contractId);

  int kyc_id;
  (!t_tradelistdb->checkKYCRegister(address, kyc_id)) ? result = "disabled" : result = "enabled";

  UniValue balanceObj(UniValue::VOBJ);
  balanceObj.push_back(Pair("result: ", result));
  return balanceObj;

}

UniValue tl_getcache(const JSONRPCRequest& request)
{
    if (request.params.size() != 1 || request.fHelp)
        throw runtime_error(
            "tl_getmargin \"address\" propertyid\n"
            "\nReturns the fee cach for a given property.\n"
            "\nArguments:\n"
            "1. collateral                     (number, required) the contract collateral\n"
            "\nResult:\n"
            "{\n"
            "  \"amount\" : \"n.nnnnnnnn\",   (number) the available balance in the cache for the property\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("tl_getcache", "\"\" 1")
            + HelpExampleRpc("tl_getcache", "\"\", 1")
        );

    uint32_t propertyId = ParsePropertyId(request.params[0]);

    // geting data from cache!
    auto it =  cachefees.find(propertyId);
    int64_t amount = (it != cachefees.end())? it->second : 0;

    UniValue balanceObj(UniValue::VOBJ);

    balanceObj.push_back(Pair("amount", FormatByType(amount, 2)));

    return balanceObj;
}

UniValue tl_getoraclecache(const JSONRPCRequest& request)
{
    if (request.params.size() != 1 || request.fHelp)
        throw runtime_error(
            "tl_getmargin \"address\" propertyid\n"
            "\nReturns the oracle fee cache for a given property.\n"
            "\nArguments:\n"
            "1. collateral                     (number, required) the contract collateral\n"
            "\nResult:\n"
            "{\n"
            "  \"amount\" : \"n.nnnnnnnn\",   (number) the available balance in the oracle cache for the property\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("tl_getcache", "\"\" 1")
            + HelpExampleRpc("tl_getcache", "\"\", 1")
        );

    uint32_t propertyId = ParsePropertyId(request.params[0]);

    // geting data from cache!
    auto it =  cachefees_oracles.find(propertyId);
    int64_t amount = (it != cachefees_oracles.end()) ? it->second : 0;

    UniValue balanceObj(UniValue::VOBJ);

    balanceObj.push_back(Pair("amount", FormatByType(amount,2)));

    return balanceObj;
}

UniValue tl_getalltxonblock(const JSONRPCRequest& request)
{
    if (request.params.size() != 1 || request.fHelp)
      throw runtime_error(
			"tl_getalltxonblock\"block\"\n"
			"\nGet detailed information about every Trade Layer transaction in block.\n"
	    "\nArguments:\n"
	    "1. txid                 (number, required) the block height \n"
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

UniValue tl_getdexvolume(const JSONRPCRequest& request)
{
    if (request.params.size() < 2 || request.fHelp)
        throw runtime_error(
            "tl_getdexvolume \n"
            "\nReturns the LTC volume in sort amount of blocks.\n"
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
            + HelpExampleCli("tl_getdexvolume", "\"\"")
            + HelpExampleRpc("tl_getdexvolume", "\"\",")
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

    balanceObj.push_back(Pair("volume", FormatDivisibleMP(amount)));
    balanceObj.push_back(Pair("blockheigh", FormatIndivisibleMP(GetHeight())));

    return balanceObj;
}

UniValue tl_getmdexvolume(const JSONRPCRequest& request)
{
    if (request.params.size() != 3 || request.fHelp)
        throw runtime_error(
            "tl_getmdexvolume \n"
            "\nReturns the token volume traded in sort amount of blocks.\n"
            "\nArguments:\n"
            "1. propertyA                 (number, required) the property id \n"
            "2. first block               (number, required) older limit block\n"
            "3. second block              (number, optional) newer limit block\n"
            "\nResult:\n"
            "{\n"
            "  \"volume\" : \"n.nnnnnnnn\",   (number) the available volume (of property) traded\n"
            "  \"blockheight\" : \"n.\",      (number) last block\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("tl_getdexvolume", "\"\"")
            + HelpExampleRpc("tl_getdexvolume", "\"\",")
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

    balanceObj.push_back(Pair("volume", FormatDivisibleMP(amount)));
    balanceObj.push_back(Pair("blockheigh", FormatIndivisibleMP(GetHeight())));

    return balanceObj;
}

UniValue tl_getcurrencytotal(const JSONRPCRequest& request)
{
    if (request.params.size() != 1 || request.fHelp)
        throw runtime_error(
            "tl_getcurrencyvolume \n"
            "\nGet total outstanding supply for token \n"
            "\nArguments:\n"
            "1. propertyId                 (number, required) propertyId of token \n"
            "\nResult:\n"
            "{\n"
            "  \"total:\" : \"n.nnnnnnnn\",   (number) the amount created for token\n"
            "  \"blockheight\" : \"n.\",      (number) last block\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("tl_getcurrencytotal", "\"\"")
            + HelpExampleRpc("tl_getcurrencytotal", "\"\",")
        );

    uint32_t propertyId = ParsePropertyId(request.params[0]);

    RequireNotContract(propertyId);

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

    balanceObj.push_back(Pair("total", FormatDivisibleMP(amount)));
    balanceObj.push_back(Pair("blockheigh", FormatIndivisibleMP(GetHeight())));

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
			"tl_listattestation\n"
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
    if (request.params.size() != 1 || request.fHelp)
      throw runtime_error(
			  "tl_getopen_interest\n"
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
			  + HelpExampleCli("tl_getopen_interest", "Contract 1, 1")
			  + HelpExampleRpc("tl_getopen_interest", "Contract 1, 1")
		  );

    uint32_t contractId = ParseNameOrId(request.params[0]);

    UniValue amountObj(UniValue::VOBJ);
    int64_t totalContracts = mastercore::getTotalLives(contractId);

    amountObj.push_back(Pair("totalLives", totalContracts));

    return amountObj;
}

//tl_getmax_peggedcurrency
//input : JSONRPCREquest which contains: 1)address of creator, 2) contract ID which is collaterilized in ALL
//return: UniValue which is JSON object that is max pegged currency you can create
UniValue tl_getmax_peggedcurrency(const JSONRPCRequest& request)
{
    if (request.params.size() != 2 || request.fHelp)
      throw runtime_error(
			  "tl_getmax_peggedcurrency\"fromaddress\""
			  "\nGet max pegged currency address can create\n"
			  "\n arguments: \n"
			  "\n 1) fromaddress (string, required) the address to send from\n"
			  "\n 2) name of contract requiered \n"
			);
    std::string fromAddress = ParseAddress(request.params[0]);
    uint32_t contractId = ParseNameOrId(request.params[1]);

    CMPSPInfo::Entry sp;
    assert(_my_sps->getSP(contractId, sp));
  // Get available ALL because dCurrency is a hedge of ALL and ALL denominated Short Contracts
  // obtain parameters & info

  int64_t tokenBalance = getMPbalance(fromAddress, ALL, BALANCE);

  // get token Price
  int64_t tokenPrice = 0;
  auto it = market_priceMap.find(sp.collateral_currency);
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
  int64_t shortPosition = getMPbalance(fromAddress, contractId, NEGATIVE_BALANCE);
  //determine which is less and use that one as max you can peg
  int64_t maxpegged = (max_dUSD > shortPosition) ? shortPosition : max_dUSD;
  //create UniValue object to return
  UniValue max_pegged(UniValue::VOBJ);
  //add value maxpegged to maxPegged json object
  max_pegged.push_back(Pair("maxPegged (dUSD)", FormatDivisibleMP(maxpegged)));
  //return UniValue JSON object
  return max_pegged;

}

UniValue tl_getcontract(const JSONRPCRequest& request)
{
    if (request.params.size() != 1  || request.fHelp)
        throw runtime_error(
            "tl_getcontract contract\n"
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
            + HelpExampleCli("tl_getcontract", "Contract1")
            + HelpExampleRpc("tl_getcontract", "Contract1")
        );

    uint32_t propertyId = ParseNameOrId(request.params[0]);

    CMPSPInfo::Entry sp;
    assert(_my_sps->getSP(propertyId, sp));

    UniValue response(UniValue::VOBJ);
    response.push_back(Pair("propertyid", (uint64_t) propertyId));
    ContractToJSON(sp, response); // name, data, url,
    KYCToJSON(sp, response);
    auto it = cdexlastprice.find(propertyId);
    const int64_t& lastPrice = it->second;
    response.push_back(Pair("last traded price", FormatDivisibleMP(lastPrice)));
    const int64_t fundBalance = 0; // NOTE: we need to write this after fundbalance logic
    response.push_back(Pair("insurance fund balance", fundBalance));
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
    response.push_back(Pair("propertyid", (uint64_t) VT));
    VestingToJSON(sp, response); // name, data, url,
    KYCToJSON(sp, response);

    return response;
}

UniValue tl_listvesting_addresses(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw runtime_error(
            "tl_listvesting_addresses\n"
            "\nReturns details for about vesting tokens.\n"
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

static const CRPCCommand commands[] =
{ //  category                             name                            actor (function)               okSafeMode
  //  ------------------------------------ ------------------------------- ------------------------------ ----------
  { "trade layer (data retrieval)", "tl_getinfo",                   &tl_getinfo,                    {} },
  { "trade layer (data retrieval)", "tl_getactivations",            &tl_getactivations,             {} },
  { "trade layer (data retrieval)", "tl_getallbalancesforid",       &tl_getallbalancesforid,        {} },
  { "trade layer (data retrieval)", "tl_getbalance",                &tl_getbalance,                 {} },
  { "trade layer (data retrieval)", "tl_gettransaction",            &tl_gettransaction,             {} },
  { "trade layer (data retrieval)", "tl_getproperty",               &tl_getproperty,                {} },
  { "trade layer (data retrieval)", "tl_listproperties",            &tl_listproperties,             {} },
  { "trade layer (data retrieval)", "tl_getcrowdsale",              &tl_getcrowdsale,               {} },
  { "trade layer (data retrieval)", "tl_getgrants",                 &tl_getgrants,                  {} },
  { "trade layer (data retrieval)", "tl_getactivecrowdsales",       &tl_getactivecrowdsales,        {} },
  { "trade layer (data retrieval)", "tl_listblocktransactions",     &tl_listblocktransactions,      {} },
  { "trade layer (data retrieval)", "tl_listpendingtransactions",   &tl_listpendingtransactions,    {} },
  { "trade layer (data retrieval)", "tl_getallbalancesforaddress",  &tl_getallbalancesforaddress,   {} },
  { "trade layer (data retrieval)", "tl_getcurrentconsensushash",   &tl_getcurrentconsensushash,    {} },
  { "trade layer (data retrieval)", "tl_getpayload",                &tl_getpayload,                 {} },
#ifdef ENABLE_WALLET
  { "trade layer (data retrieval)", "tl_listtransactions",          &tl_listtransactions,           {} },
  { "trade layer (configuration)",  "tl_setautocommit",             &tl_setautocommit,              {} },
#endif
  { "hidden",                       "mscrpc",                       &mscrpc,                        {} },
  { "trade layer (data retrieval)", "tl_getposition",               &tl_getposition,                {} },
  { "trade layer (data retrieval)", "tl_getfullposition",           &tl_getfullposition,            {} },
  { "trade layer (data retrieval)", "tl_getcontract_orderbook",     &tl_getcontract_orderbook,      {} },
  { "trade layer (data retrieval)", "tl_gettradehistory",           &tl_gettradehistory,            {} },
  { "trade layer (data retrieval)", "tl_gettradehistory_unfiltered",&tl_gettradehistory_unfiltered, {} },
  { "trade layer (data retrieval)", "tl_getupnl",                   &tl_getupnl,                    {} },
  { "trade layer (data retrieval)", "tl_getpnl",                    &tl_getpnl,                     {} },
  { "trade layer (data retieval)",  "tl_getactivedexsells",         &tl_getactivedexsells ,         {} },
  { "trade layer (data retieval)" , "tl_getorderbook",              &tl_getorderbook,               {} },
  { "trade layer (data retieval)" , "tl_getpeggedhistory",          &tl_getpeggedhistory,           {} },
  { "trade layer (data retieval)" , "tl_getcontract_reserve",       &tl_getcontract_reserve,        {} },
  { "trade layer (data retieval)" , "tl_getreserve",                &tl_getreserve,                 {} },
  { "trade layer (data retieval)" , "tl_getallprice",               &tl_getallprice,                {} },
  { "trade layer (data retieval)" , "tl_getmarketprice",            &tl_getmarketprice,             {} },
  { "trade layer (data retieval)" , "tl_getsum_upnl",               &tl_getsum_upnl,                {} },
  { "trade layer (data retieval)" , "tl_check_commits",             &tl_check_commits,              {} },
  { "trade layer (data retieval)" , "tl_get_channelreserve",        &tl_get_channelreserve,         {} },
  { "trade layer (data retieval)" , "tl_getchannel_info",           &tl_getchannel_info,            {} },
  { "trade layer (data retieval)" , "tl_getcache",                  &tl_getcache,                   {} },
  { "trade layer (data retieval)" , "tl_check_kyc",                 &tl_check_kyc,                  {} },
  { "trade layer (data retieval)" , "tl_list_natives",              &tl_list_natives,               {} },
  { "trade layer (data retieval)" , "tl_list_oracles",              &tl_list_oracles,               {} },
  { "trade layer (data retieval)" , "tl_getalltxonblock",           &tl_getalltxonblock,            {} },
  { "trade layer (data retieval)" , "tl_check_withdrawals",         &tl_check_withdrawals,          {} },
  { "trade layer (data retieval)" , "tl_getdexvolume",              &tl_getdexvolume,               {} },
  { "trade layer (data retieval)" , "tl_getmdexvolume",             &tl_getmdexvolume,              {} },
  { "trade layer (data retieval)" , "tl_getcurrencytotal",          &tl_getcurrencytotal,           {} },
  { "trade layer (data retieval)" , "tl_listkyc",                   &tl_listkyc,                    {} },
  { "trade layer (data retieval)" , "tl_getoraclecache",            &tl_getoraclecache,             {} },
  { "trade layer (data retieval)",  "tl_getmax_peggedcurrency",     &tl_getmax_peggedcurrency,      {} },
  { "trade layer (data retieval)",  "tl_getunvested",               &tl_getunvested,                {} },
  { "trade layer (data retieval)",  "tl_list_attestation",          &tl_list_attestation,           {} },
  { "trade layer (data retieval)",  "tl_getcontract",               &tl_getcontract,                {} },
  { "trade layer (data retieval)",  "tl_getopen_interest",          &tl_getopen_interest,           {} },
  { "trade layer (data retieval)",  "tl_getvesting_info",           &tl_getvesting_info,            {} },
  { "trade layer (data retieval)",  "tl_listvesting_addresses",     &tl_listvesting_addresses,      {} },
  { "trade layer (data retieval)",  "tl_get_channelremaining",      &tl_get_channelremaining,       {} },
};

void RegisterTLDataRetrievalRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
