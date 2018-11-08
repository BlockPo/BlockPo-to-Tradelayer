/**
 * @file omnicore.cpp
 *
 * This file contains the core of Omni Core.
 */

#include "omnicore/omnicore.h"

#include "omnicore/activation.h"
#include "omnicore/consensushash.h"
#include "omnicore/convert.h"
#include "omnicore/dex.h"
#include "omnicore/encoding.h"
#include "omnicore/errors.h"
#include "omnicore/log.h"
#include "omnicore/notifications.h"
#include "omnicore/pending.h"
#include "omnicore/persistence.h"
#include "omnicore/rules.h"
#include "omnicore/script.h"
#include "omnicore/sp.h"
#include "omnicore/tally.h"
#include "omnicore/tx.h"
#include "omnicore/uint256_extensions.h"
#include "omnicore/utilsbitcoin.h"
#include "omnicore/version.h"
#include "omnicore/walletcache.h"
#include "omnicore/wallettxs.h"
#include "omnicore/mdex.h"
#include "arith_uint256.h"
#include "base58.h"
#include "chainparams.h"
#include <wallet/coincontrol.h>
#include "coins.h"
#include "core_io.h"
#include "init.h"
#include "validation.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "script/script.h"
#include "script/standard.h"
#include "sync.h"
#include "tinyformat.h"
#include "uint256.h"
#include "ui_interface.h"
#include "util.h"
#include "utilstrencodings.h"
#include "utiltime.h"
#include "wallet/wallet.h"
#include "validation.h"
#include "consensus/validation.h"
#include "consensus/params.h"
#include "consensus/tx_verify.h"

#include "net.h" // for g_connman.get()
#ifdef ENABLE_WALLET
#include "script/ismine.h"
#include "wallet/wallet.h"
#include "serialize.h"
#endif

#include <univalue.h>

#include <boost/algorithm/string.hpp>
#include <boost/exception/to_string.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/lexical_cast.hpp>

#include <openssl/sha.h>

#include "leveldb/db.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <fstream>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "tradelayer_matrices.h"
#include "externfns.h"

using boost::algorithm::token_compress_on;
using boost::to_string;

using leveldb::Iterator;
using leveldb::Slice;
using leveldb::Status;

using std::endl;
using std::make_pair;
using std::map;
using std::ofstream;
using std::pair;
using std::string;
using std::vector;

using namespace mastercore;

CCriticalSection cs_tally;

static string exodus_address = "1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P";

static const string exodus_mainnet = "1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P";
static const string exodus_testnet = "mfjQ3ow4STCqW2zMeUeBWvs4y9Fgx7Gxfh";  // one of our litecoin testnet addresses
// static const string getmoney_testnet = "moneyqMan7uh8FqdCA2BV5yZ8qVrc9ikLP";

static int nWaterlineBlock = 0;

//! Available balances of wallet properties
std::map<uint32_t, int64_t> global_balance_money;
//! Vector containing a list of properties relative to the wallet
std::set<uint32_t> global_wallet_property_list;

/**
 * Used to indicate, whether to automatically commit created transactions.
 *
 * Can be set with configuration "-autocommit" or RPC "setautocommit_OMNI".
 */
bool autoCommit = true;

static boost::filesystem::path MPPersistencePath;

static int mastercoreInitialized = 0;

static int reorgRecoveryMode = 0;
static int reorgRecoveryMaxHeight = 0;

extern int64_t factorE;
extern double denMargin;
extern uint64_t marketP[NPTYPES];
extern std::vector<std::map<std::string, std::string>> path_ele;
extern int idx_expiration;
extern int expirationAchieve;
extern double globalPNLALL_DUSD;
extern int64_t globalVolumeALL_DUSD;

CMPTxList *mastercore::p_txlistdb;
CMPTradeList *mastercore::t_tradelistdb;
COmniTransactionDB *mastercore::p_OmniTXDB;

// indicate whether persistence is enabled at this point, or not
// used to write/read files, for breakout mode, debugging, etc.
static bool writePersistence(int block_now)
{
  // if too far away from the top -- do not write
  if (GetHeight() > (block_now + MAX_STATE_HISTORY)) return false;

  return true;
}

std::string mastercore::strMPProperty(uint32_t propertyId)
{
    std::string str = "*unknown*";

    // test user-token
    if (0x80000000 & propertyId) {
        str = strprintf("Test token: %d : 0x%08X", 0x7FFFFFFF & propertyId, propertyId);
    } else {
        switch (propertyId) {
            case OMNI_PROPERTY_BTC: str = "BTC";
                break;
            case OMNI_PROPERTY_MSC: str = "OMNI";
                break;
            case OMNI_PROPERTY_TMSC: str = "TOMNI";
                break;
            default:
                str = strprintf("SP token: %d", propertyId);
        }
    }

    return str;
}

std::string FormatDivisibleShortMP(int64_t n)
{
    int64_t n_abs = (n > 0 ? n : -n);
    int64_t quotient = n_abs / COIN;
    int64_t remainder = n_abs % COIN;
    std::string str = strprintf("%d.%08d", quotient, remainder);
    // clean up trailing zeros - good for RPC not so much for UI
    str.erase(str.find_last_not_of('0') + 1, std::string::npos);
    if (str.length() > 0) {
        std::string::iterator it = str.end() - 1;
        if (*it == '.') {
            str.erase(it);
        }
    } //get rid of trailing dot if non decimal
    return str;
}

std::string FormatDivisibleMP(int64_t n, bool fSign)
{
    // Note: not using straight sprintf here because we do NOT want
    // localized number formatting.
    int64_t n_abs = (n > 0 ? n : -n);
    int64_t quotient = n_abs / COIN;
    int64_t remainder = n_abs % COIN;
    std::string str = strprintf("%d.%08d", quotient, remainder);

    if (!fSign) return str;

    if (n < 0)
        str.insert((unsigned int) 0, 1, '-');
    else
        str.insert((unsigned int) 0, 1, '+');
    return str;
}

std::string mastercore::FormatIndivisibleMP(int64_t n)
{
    return strprintf("%d", n);
}

std::string FormatShortMP(uint32_t property, int64_t n)
{
    if (isPropertyDivisible(property)) {
        return FormatDivisibleShortMP(n);
    } else {
        return FormatIndivisibleMP(n);
    }
}

std::string FormatMP(uint32_t property, int64_t n, bool fSign)
{
    if (isPropertyDivisible(property)) {
        return FormatDivisibleMP(n, fSign);
    } else {
        return FormatIndivisibleMP(n);
    }
}

std::string FormatByType(int64_t amount, uint16_t propertyType)
{
    if (propertyType & MSC_PROPERTY_TYPE_INDIVISIBLE) {
        return FormatIndivisibleMP(amount);
    } else {
        return FormatDivisibleMP(amount);
    }
}

std::string FormatByDivisibility(int64_t amount, bool divisible)
{
    if (!divisible) {
        return FormatIndivisibleMP(amount);
    } else {
        return FormatDivisibleMP(amount);
    }
}

/////////////////////////////////////////
/*New property type No 3 Contract*/
std::string mastercore::FormatContractMP(int64_t n)
{
    return strprintf("%d", n);
}
/////////////////////////////////////////
/*New things for contracts*/////////////////////////////////////////////////////
double FormatContractShortMP(int64_t n)
{
    int64_t n_abs = (n > 0 ? n : -n);
    int64_t quotient = n_abs / COIN;
    int64_t remainder = n_abs % COIN;
    std::string str = strprintf("%d.%08d", quotient, remainder);
    // clean up trailing zeros - good for RPC not so much for UI
    str.erase(str.find_last_not_of('0') + 1, std::string::npos);
    if (str.length() > 0) {
        std::string::iterator it = str.end() - 1;
        if (*it == '.') {
            str.erase(it);
        }
    } //get rid of trailing dot if non decimal
    double q = atof(str.c_str());
    return q;
}

long int FormatShortIntegerMP(int64_t n)
{
    int64_t n_abs = (n > 0 ? n : -n);
    int64_t quotient = n_abs / COIN;
    int64_t remainder = n_abs % COIN;
    std::string str = strprintf("%d.%08d", quotient, remainder);
    // clean up trailing zeros - good for RPC not so much for UI
    str.erase(str.find_last_not_of('0') + 1, std::string::npos);
    if (str.length() > 0) {
      std::string::iterator it = str.end() - 1;
      if (*it == '.') {
	str.erase(it);
      }
    } //get rid of trailing dot if non decimal
    long int q = atof(str.c_str());
    return q;
}
/////////////////////////////////////////
OfferMap mastercore::my_offers;
AcceptMap mastercore::my_accepts;
CMPSPInfo *mastercore::_my_sps;
CrowdMap mastercore::my_crowds;
// this is the master list of all amounts for all addresses for all properties, map is unsorted
std::unordered_map<std::string, CMPTally> mastercore::mp_tally_map;

CMPTally* mastercore::getTally(const std::string& address)
{
    std::unordered_map<std::string, CMPTally>::iterator it = mp_tally_map.find(address);
    if (it != mp_tally_map.end()) return &(it->second);
    return (CMPTally *) NULL;
}

// look at balance for an address
int64_t getMPbalance(const std::string& address, uint32_t propertyId, TallyType ttype)
{
    int64_t balance = 0;
    if (TALLY_TYPE_COUNT <= ttype) {
        return 0;
    }

    LOCK(cs_tally);
    const std::unordered_map<std::string, CMPTally>::iterator my_it = mp_tally_map.find(address);
    if (my_it != mp_tally_map.end()) {
        balance = (my_it->second).getMoney(propertyId, ttype);
    }

    return balance;
}

int64_t getUserAvailableMPbalance(const std::string& address, uint32_t propertyId)
{
    int64_t money = getMPbalance(address, propertyId, BALANCE);
    int64_t pending = getMPbalance(address, propertyId, PENDING);

    if (0 > pending) {
        return (money + pending); // show the decrease in available money
    }

    return money;
}

bool mastercore::isTestEcosystemProperty(uint32_t propertyId)
{
    if ((OMNI_PROPERTY_TMSC == propertyId) || (TEST_ECO_PROPERTY_1 <= propertyId)) return true;

    return false;
}

bool mastercore::isMainEcosystemProperty(uint32_t propertyId)
{
    if ((OMNI_PROPERTY_BTC != propertyId) && !isTestEcosystemProperty(propertyId)) return true;

    return false;
}

std::string mastercore::getTokenLabel(uint32_t propertyId)
{
    std::string tokenStr;
    if (propertyId < 3) {
        if (propertyId == 1) {
            tokenStr = " OMNI";
        } else {
            tokenStr = " TOMNI";
        }
    } else {
        tokenStr = strprintf(" SPT#%d", propertyId);
    }
    return tokenStr;
}

// get total tokens for a property
// optionally counts the number of addresses who own that property: n_owners_total
int64_t mastercore::getTotalTokens(uint32_t propertyId, int64_t* n_owners_total)
{
    int64_t prev = 0;
    int64_t owners = 0;
    int64_t totalTokens = 0;

    LOCK(cs_tally);

    CMPSPInfo::Entry property;
    if (false == _my_sps->getSP(propertyId, property)) {
        return 0; // property ID does not exist
    }

    if (!property.fixed || n_owners_total) {
        for (std::unordered_map<std::string, CMPTally>::const_iterator it = mp_tally_map.begin(); it != mp_tally_map.end(); ++it) {
            const CMPTally& tally = it->second;

            totalTokens += tally.getMoney(propertyId, BALANCE);

            if (prev != totalTokens) {
                prev = totalTokens;
                owners++;
            }
        }
    }

    if (property.fixed) {
        totalTokens = property.num_tokens; // only valid for TX50
    }

    if (n_owners_total) *n_owners_total = owners;

    return totalTokens;
}

// return true if everything is ok
bool mastercore::update_tally_map(const std::string& who, uint32_t propertyId, int64_t amount, TallyType ttype)
{
    if (0 == amount) {
        PrintToLog("%s(%s, %u=0x%X, %+d, ttype=%d) ERROR: amount to credit or debit is zero\n", __func__, who, propertyId, propertyId, amount, ttype);
        return false;
    }
    if (ttype >= TALLY_TYPE_COUNT) {
        PrintToLog("%s(%s, %u=0x%X, %+d, ttype=%d) ERROR: invalid tally type\n", __func__, who, propertyId, propertyId, amount, ttype);
        return false;
    }

    bool bRet = false;
    int64_t before = 0;
    int64_t after = 0;

    LOCK(cs_tally);

    before = getMPbalance(who, propertyId, ttype);

    std::unordered_map<std::string, CMPTally>::iterator my_it = mp_tally_map.find(who);
    if (my_it == mp_tally_map.end()) {
        // insert an empty element
        my_it = (mp_tally_map.insert(std::make_pair(who, CMPTally()))).first;
    }

    CMPTally& tally = my_it->second;
    bRet = tally.updateMoney(propertyId, amount, ttype);

    after = getMPbalance(who, propertyId, ttype);
    if (!bRet) {
        assert(before == after);
        PrintToLog("%s(%s, %u=0x%X, %+d, ttype=%d) ERROR: insufficient balance (=%d)\n", __func__, who, propertyId, propertyId, amount, ttype, before);
    }
    if (msc_debug_tally) {
        PrintToLog("%s(%s, %u=0x%X, %+d, ttype=%d): before=%d, after=%d\n", __func__, who, propertyId, propertyId, amount, ttype, before, after);
    }

    return bRet;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// some old TODOs
//  6) verify large-number calculations (especially divisions & multiplications)
//  9) build in consesus checks with the masterchain.info & masterchest.info -- possibly run them automatically, daily (?)
// 10) need a locking mechanism between Core & Qt -- to retrieve the tally, for instance, this and similar to this: LOCK(wallet->cs_wallet);
//

uint32_t mastercore::GetNextPropertyId(bool maineco)
{
    if (maineco) {
        return _my_sps->peekNextSPID(1);
    } else {
        return _my_sps->peekNextSPID(2);
    }
}

// Perform any actions that need to be taken when the total number of tokens for a property ID changes
void NotifyTotalTokensChanged(uint32_t propertyId)
{
  //
}

// The idea is have tools to see the origin of pegged currency created !!!
void CMPTradeList::NotifyPeggedCurrency(const uint256& txid, string address, uint32_t propertyId, uint64_t amount, std::string series)
{
    if (!pdb) return;
    const string key = txid.ToString();
    const string value = strprintf("%s:%d:%d:%d:%s",address, propertyId, GetHeight(), amount, series);
    PrintToConsole ("saved: s% : %s\n",key,value);
    Status status;
    if (pdb)
    {
        status = pdb->Put(writeoptions, key, value);
        ++ nWritten;
        // if (msc_debug_tradedb) PrintToLog("%s(): %s\n", __FUNCTION__, status.ToString());
    }
}

void CheckWalletUpdate(bool forceUpdate)
{
  if (!WalletCacheUpdate()) {
    // no balance changes were detected that affect wallet addresses, signal a generic change to overall Omni state
    if (!forceUpdate) {
      // uiInterface.OmniStateChanged();
      return;
    }
  }
#ifdef ENABLE_WALLET
    LOCK(cs_tally);

     // balance changes were found in the wallet, update the global totals and signal a Omni balance change
     global_balance_money.clear();

    // populate global balance totals and wallet property list - note global balances do not include additional balances from watch-only addresses
    for (std::unordered_map<std::string, CMPTally>::iterator my_it = mp_tally_map.begin(); my_it != mp_tally_map.end(); ++my_it) {
        // check if the address is a wallet address (including watched addresses)
        std::string address = my_it->first;
        int addressIsMine = IsMyAddress(address);
        if (!addressIsMine) continue;
        // iterate only those properties in the TokenMap for this address
        my_it->second.init();
        uint32_t propertyId;
        while (0 != (propertyId = (my_it->second).next())) {
            // add to the global wallet property list
            global_wallet_property_list.insert(propertyId);
            // check if the address is spendable (only spendable balances are included in totals)
            if (addressIsMine != ISMINE_SPENDABLE) continue;
            // work out the balances and add to globals
            global_balance_money[propertyId] += getUserAvailableMPbalance(address, propertyId);
        }
    }
    // signal an Omni balance change
    // uiInterface.OmniBalanceChanged();
#endif
}

/**
 * Returns the encoding class, used to embed a payload.
 *    Class A (dex payments)
 *    Class D (op-return compressed)
 */
int mastercore::GetEncodingClass(const CTransaction& tx, int nBlock)
{
    bool hasExodus = false;
    bool hasOpReturn = false;

    /* Fast Search
     * Perform a string comparison on hex for each scriptPubKey & look directly for omni marker bytes
     * This allows to drop non-Omni transactions with less work
     */
    std::string strClassD = "7071"; /*the pepe marker*/
    bool examineClosely = false;
    for (unsigned int n = 0; n < tx.vout.size(); ++n) {
        const CTxOut& output = tx.vout[n];
        std::string strSPB = HexStr(output.scriptPubKey.begin(), output.scriptPubKey.end());
        PrintToLog("strSPB: %s\n",strSPB);
        if (strSPB.find(strClassD) != std::string::npos) {
            examineClosely = true;
            PrintToLog("examineClosely = true 1\n");
            break;
        }
    }

    // Examine everything when not on mainnet
    if (isNonMainNet()) {
        PrintToLog("examineClosely = true  2\n");
        examineClosely = true;
    }

    if (!examineClosely) return NO_MARKER;

    for (unsigned int n = 0; n < tx.vout.size(); ++n) {
        const CTxOut& output = tx.vout[n];

        txnouttype outType;
        if (!GetOutputType(output.scriptPubKey, outType)) {
            //continue;
        }
        if (!IsAllowedOutputType(outType, nBlock)) {
            //continue;
        }

        if (outType == TX_PUBKEYHASH) {
            CTxDestination dest;
            if (ExtractDestination(output.scriptPubKey, dest)) {
                std::string address = EncodeDestination(dest);
                if (address == ExodusAddress()) {
                    hasExodus = true;
                }
            }
        }

        if (outType == TX_NULL_DATA) {
            // Ensure there is a payload, and the first pushed element equals,
            // or starts with the "ol" marker
            std::vector<std::string> scriptPushes;
            if (!GetScriptPushes(output.scriptPubKey, scriptPushes)) {
                continue;
            }
            if (!scriptPushes.empty()) {
                std::vector<unsigned char> vchMarker = GetOmMarker();
                std::vector<unsigned char> vchPushed = ParseHex(scriptPushes[0]);
                PrintToLog("vchPushed size : %d\n",vchPushed.size());
                PrintToLog("vchMarker size : %d\n",vchMarker.size());
                if (vchPushed.size() < vchMarker.size()) {
                    continue;
                }
                if (std::equal(vchMarker.begin(), vchMarker.end(), vchPushed.begin())) {
                    hasOpReturn = true;
                }
            }
        }
    }

    if (hasOpReturn) {
        PrintToLog("OP_RETURN FOUND \n");
        return OMNI_CLASS_D;
    }

    if (hasExodus) {
        return OMNI_CLASS_A;
    }


    return NO_MARKER;
}

// TODO: move
CCoinsView mastercore::viewDummy;
CCoinsViewCache mastercore::view(&viewDummy);

//! Guards coins view cache
CCriticalSection mastercore::cs_tx_cache;

static unsigned int nCacheHits = 0;
static unsigned int nCacheMiss = 0;

/**
 * Fetches transaction inputs and adds them to the coins view cache.
 *
 * @param tx[in]  The transaction to fetch inputs for
 * @return True, if all inputs were successfully added to the cache
 */
static bool FillTxInputCache(const CTransaction& tx)
{
    static unsigned int nCacheSize = gArgs.GetArg("-omnitxcache", 500000);

    if (view.GetCacheSize() > nCacheSize) {
        PrintToLog("%s(): clearing cache before insertion [size=%d, hit=%d, miss=%d]\n",
                __func__, view.GetCacheSize(), nCacheHits, nCacheMiss);
        view.Flush();
    }

    for (std::vector<CTxIn>::const_iterator it = tx.vin.begin(); it != tx.vin.end(); ++it) {
        const CTxIn& txIn = *it;
        unsigned int nOut = txIn.prevout.n;
        PrintToLog("prevout.n : %s\n",txIn.prevout.n);
        if (view.HaveCoin(txIn.prevout)){
             ++nCacheHits;
             continue;
        }else{
            ++nCacheMiss;
        }

        CTransactionRef txPrev;
        uint256 hashBlock = uint256();

        if (!GetTransaction(txIn.prevout.hash, txPrev, Params().GetConsensus(), hashBlock, true)) {
           return false;
        }

        if (txPrev.get()->vout.size() <= nOut){
            return false;
        }

        Coin newCoin(txPrev.get()->vout[nOut], 0, tx.IsCoinBase());
        view.AddCoin(txIn.prevout, std::move(newCoin), false);
    }


    return true;
}

// idx is position within the block, 0-based
// int msc_tx_push(const CTransaction &wtx, int nBlock, unsigned int idx)
// INPUT: bRPConly -- set to true to avoid moving funds; to be called from various RPC calls like this
// RETURNS: 0 if parsed a MP TX
// RETURNS: < 0 if a non-MP-TX or invalid
// RETURNS: >0 if 1 or more payments have been made
static int parseTransaction(bool bRPConly, const CTransaction& wtx, int nBlock, unsigned int idx, CMPTransaction& mp_tx, unsigned int nTime)
{
    assert(bRPConly == mp_tx.isRpcOnly());
    mp_tx.Set(wtx.GetHash(), nBlock, idx, nTime);

    // ### CLASS IDENTIFICATION AND MARKER CHECK ###
    int omniClass = GetEncodingClass(wtx, nBlock);

    if (omniClass == NO_MARKER) {
        return -1; // No Omni marker, thus not a valid Omni transaction
    }

    if (!bRPConly || msc_debug_parser_readonly) {
        PrintToLog("____________________________________________________________________________________________________________________________________\n");
        PrintToLog("%s(block=%d, %s idx= %d); txid: %s\n", __FUNCTION__, nBlock, DateTimeStrFormat("%Y-%m-%d %H:%M:%S", nTime), idx, wtx.GetHash().GetHex());
    }

    LOCK(cs_tx_cache);

    // Add previous transaction inputs to the cache
    if (!FillTxInputCache(wtx)) {
        PrintToLog("%s() ERROR: failed to get inputs for %s\n", __func__, wtx.GetHash().GetHex());
        return -101;
    }

    assert(view.HaveInputs(wtx));

    // ### SENDER IDENTIFICATION ###
    std::string strSender;

    // determine the sender, but invalidate transaction, if the input is not accepted
    unsigned int vin_n = 0; // the first input
   // if (true) PrintToLog("vin=%d:%s\n", vin_n, ScriptToAsmStr(wtx.vin[vin_n].scriptSig));

    const CTxIn& txIn = wtx.vin[vin_n];
    PrintToLog("Special checkpoint\n");
    const CTxOut& txOut = view.GetOutputFor(txIn);
    assert(!txOut.IsNull());

    PrintToLog("Second checkpoint \n");

    txnouttype whichType;
    if (!GetOutputType(txOut.scriptPubKey, whichType)) {
        return -108;
    }
    //if (!IsAllowedInputType(whichType, nBlock)) {
       // return -109;
    //}
    CTxDestination source;
    if (ExtractDestination(txOut.scriptPubKey, source)) {
        strSender = EncodeDestination(source);
        PrintToLog("destination: %s\n",strSender);
    } else return -110;

    PrintToLog("3rd checkpoint \n");


    int64_t inAll = view.GetValueIn(wtx);
    int64_t outAll = wtx.GetValueOut();
    int64_t txFee = inAll - outAll; // miner fee

    if (!strSender.empty()) {
        if (msc_debug_verbose) PrintToLog("The Sender: %s : fee= %s\n", strSender, FormatDivisibleMP(txFee));
    } else {
        PrintToLog("The sender is still EMPTY !!! txid: %s\n", wtx.GetHash().GetHex());
       // return -5;
    }

    // ### DATA POPULATION ### - save output addresses, values and scripts
    std::string strReference;
    unsigned char single_pkt[65535];
    unsigned int packet_size = 0;
    std::vector<std::string> script_data;
    std::vector<std::string> address_data;
    std::vector<int64_t> value_data;

    for (unsigned int n = 0; n < wtx.vout.size(); ++n) {
        txnouttype whichType;
        if (!GetOutputType(wtx.vout[n].scriptPubKey, whichType)) {
            continue;
        }
        if (!IsAllowedOutputType(whichType, nBlock)) {
            continue;
        }
        CTxDestination dest;
        if (ExtractDestination(wtx.vout[n].scriptPubKey, dest)) {
            std::string address = EncodeDestination(dest);
            // saving for Class A processing or reference
            GetScriptPushes(wtx.vout[n].scriptPubKey, script_data);
            address_data.push_back(address);
            value_data.push_back(wtx.vout[n].nValue);
            // if (msc_debug_parser_data) PrintToLog("saving address_data #%d: %s:%s\n", n, address.ToString(), ScriptToAsmStr(wtx.vout[n].scriptPubKey));
        }
    }
    if (msc_debug_parser_data) PrintToLog(" address_data.size=%lu\n script_data.size=%lu\n value_data.size=%lu\n", address_data.size(), script_data.size(), value_data.size());

    // ### CLASS C / CLASS D PARSING ###
    if (msc_debug_parser_data) PrintToLog("Beginning reference identification\n");
    bool referenceFound = false; // bool to hold whether we've found the reference yet
    bool changeRemoved = false; // bool to hold whether we've ignored the first output to sender as change
    unsigned int potentialReferenceOutputs = 0; // int to hold number of potential reference outputs
    for (unsigned k = 0; k < address_data.size(); ++k) { // how many potential reference outputs do we have, if just one select it right here
        const std::string& addr = address_data[k];
        if (msc_debug_parser_data) PrintToLog("ref? data[%d]:%s: %s (%s)\n", k, script_data[k], addr, FormatIndivisibleMP(value_data[k]));
        ++potentialReferenceOutputs;
        if (1 == potentialReferenceOutputs) {
            strReference = addr;
            referenceFound = true;
            if (msc_debug_parser_data) PrintToLog("Single reference potentially id'd as follows: %s \n", strReference);
        } else { //as soon as potentialReferenceOutputs > 1 we need to go fishing
            strReference.clear(); // avoid leaving strReference populated for sanity
            referenceFound = false;
            if (msc_debug_parser_data) PrintToLog("More than one potential reference candidate, blanking strReference, need to go fishing\n");
        }
    }
    if (!referenceFound) { // do we have a reference now? or do we need to dig deeper
        if (msc_debug_parser_data) PrintToLog("Reference has not been found yet, going fishing\n");
        for (unsigned k = 0; k < address_data.size(); ++k) {
            const std::string& addr = address_data[k];
            if (addr == strSender && !changeRemoved) {
                changeRemoved = true; // per spec ignore first output to sender as change if multiple possible ref addresses
                if (msc_debug_parser_data) PrintToLog("Removed change\n");
            } else {
                strReference = addr; // this may be set several times, but last time will be highest vout
                if (msc_debug_parser_data) PrintToLog("Resetting strReference as follows: %s \n ", strReference);
            }
        }
    }
    if (msc_debug_parser_data) PrintToLog("Ending reference identification\nFinal decision on reference identification is: %s\n", strReference);

    // ### CLASS D SPECIFIC PARSING ###
    if (omniClass == OMNI_CLASS_D) {
        std::vector<std::string> op_return_script_data;

        // ### POPULATE OP RETURN SCRIPT DATA ###
        for (unsigned int n = 0; n < wtx.vout.size(); ++n) {
            txnouttype whichType;
            if (!GetOutputType(wtx.vout[n].scriptPubKey, whichType)) {
                continue;
            }
            if (!IsAllowedOutputType(whichType, nBlock)) {
                continue;
            }
            if (whichType == TX_NULL_DATA) {
                // only consider outputs, which are explicitly tagged
                std::vector<std::string> vstrPushes;
                if (!GetScriptPushes(wtx.vout[n].scriptPubKey, vstrPushes)) {
                    continue;
                }
                // TODO: maybe encapsulate the following sort of messy code
                if (!vstrPushes.empty()) {
                    std::vector<unsigned char> vchMarker = GetOmMarker();
                    std::vector<unsigned char> vchPushed = ParseHex(vstrPushes[0]);
                    if (vchPushed.size() < vchMarker.size()) {
                        continue;
                    }
                    if (std::equal(vchMarker.begin(), vchMarker.end(), vchPushed.begin())) {
                        size_t sizeHex = vchMarker.size() * 2;
                        // strip out the marker at the very beginning
                        vstrPushes[0] = vstrPushes[0].substr(sizeHex);
                        // add the data to the rest
                        op_return_script_data.insert(op_return_script_data.end(), vstrPushes.begin(), vstrPushes.end());

                        if (msc_debug_parser_data) {
                            PrintToLog("Class C transaction detected: %s parsed to %s at vout %d\n", wtx.GetHash().GetHex(), vstrPushes[0], n);
                        }
                    }
                }
            }
        }
        // ### EXTRACT PAYLOAD FOR CLASS D ###
        for (unsigned int n = 0; n < op_return_script_data.size(); ++n) {
            if (!op_return_script_data[n].empty()) {
                assert(IsHex(op_return_script_data[n])); // via GetScriptPushes()
                std::vector<unsigned char> vch = ParseHex(op_return_script_data[n]);
                unsigned int payload_size = vch.size();
                if (packet_size + payload_size > 65535) {
                    payload_size = 65535 - packet_size;
                    PrintToLog("limiting payload size to %d byte\n", packet_size + payload_size);
                }
                if (payload_size > 0) {
                    memcpy(single_pkt+packet_size, &vch[0], payload_size);
                    packet_size += payload_size;
                }
                if (65535 == packet_size) {
                    break;
                }
            }
        }
    }

    // ### SET MP TX INFO ###
    if (msc_debug_verbose) PrintToLog("single_pkt: %s\n", HexStr(single_pkt, packet_size + single_pkt));
    mp_tx.Set(strSender, strReference, 0, wtx.GetHash(), nBlock, idx, (unsigned char *)&single_pkt, packet_size, omniClass, (inAll-outAll));

    if (omniClass == OMNI_CLASS_A && packet_size == 0) {
        return 1;
    }

    return 0;
}

/**
 * Provides access to parseTransaction in read-only mode.
 */
int ParseTransaction(const CTransaction& tx, int nBlock, unsigned int idx, CMPTransaction& mptx, unsigned int nTime)
{
    return parseTransaction(true, tx, nBlock, idx, mptx, nTime);
}

/**
 * Handles potential DEx payments.
 *
 * Note: must *not* be called outside of the transaction handler, and it does not
 * check, if a transaction marker exists.
 *
 * @return True, if valid
 */
static bool HandleDExPayments(const CTransaction& tx, int nBlock, const std::string& strSender)
{
    int count = 0;
    PrintToConsole("Inside HandleDExPayments function <<<<<<<<<<<<<<<<<<<<<\n");
    for (unsigned int n = 0; n < tx.vout.size(); ++n) {
        CTxDestination dest;
        if (ExtractDestination(tx.vout[n].scriptPubKey, dest)) {
            std::string address = EncodeDestination(dest);
            if (address == ExodusAddress() || address == strSender) {
                continue;
            }

            // if (msc_debug_parser_dex) PrintToLog("payment #%d %s %s\n", count, strAddress, FormatIndivisibleMP(tx.vout[n].nValue));

            // check everything and pay BTC for the property we are buying here...
            if (0 == DEx_payment(tx.GetHash(), n, address, strSender, tx.vout[n].nValue, nBlock)) ++count;
        }
    }

    return (count > 0);
}
/**
 * Reports the progress of the initial transaction scanning.
 *
 * The progress is printed to the console, written to the debug log file, and
 * the RPC status, as well as the splash screen progress label, are updated.
 *
 * @see msc_initial_scan()
 */
 class ProgressReporter
 {
  private:
     const CBlockIndex* m_pblockFirst;
     const CBlockIndex* m_pblockLast;
     const int64_t m_timeStart;

     /** Returns the estimated remaining time in milliseconds. */
     int64_t estimateRemainingTime(double progress) const
     {
         int64_t timeSinceStart = GetTimeMillis() - m_timeStart;

         double timeRemaining = 3600000.0; // 1 hour
         if (progress > 0.0 && timeSinceStart > 0) {
             timeRemaining = (100.0 - progress) / progress * timeSinceStart;
         }

         return static_cast<int64_t>(timeRemaining);
     }

     /** Converts a time span to a human readable string. */
     std::string remainingTimeAsString(int64_t remainingTime) const
     {
         int64_t secondsTotal = 0.001 * remainingTime;
         int64_t hours = secondsTotal / 3600;
         int64_t minutes = secondsTotal / 60;
         int64_t seconds = secondsTotal % 60;

         if (hours > 0) {
             return strprintf("%d:%02d:%02d hours", hours, minutes, seconds);
         } else if (minutes > 0) {
             return strprintf("%d:%02d minutes", minutes, seconds);
         } else {
             return strprintf("%d seconds", seconds);
         }
     }

 public:
     ProgressReporter(const CBlockIndex* pblockFirst, const CBlockIndex* pblockLast)
     : m_pblockFirst(pblockFirst), m_pblockLast(pblockLast), m_timeStart(GetTimeMillis())
     {
     }

     /** Prints the current progress to the console and notifies the UI. */
     void update(const CBlockIndex* pblockNow) const
     {
         int nLastBlock = m_pblockLast->nHeight;
         int nCurrentBlock = pblockNow->nHeight;
         unsigned int nFirst = m_pblockFirst->nChainTx;
         unsigned int nCurrent = pblockNow->nChainTx;
         unsigned int nLast = m_pblockLast->nChainTx;

         double dProgress = 100.0 * (nCurrent - nFirst) / (nLast - nFirst);
         int64_t nRemainingTime = estimateRemainingTime(dProgress);
         std::string strProgress = strprintf(
                 "Still scanning.. at block %d of %d. Progress: %.2f %%, about %s remaining..\n",
                 nCurrentBlock, nLastBlock, dProgress, remainingTimeAsString(nRemainingTime));
         std::string strProgressUI = strprintf(
                 "Still scanning.. at block %d of %d.\nProgress: %.2f %% (about %s remaining)",
                 nCurrentBlock, nLastBlock, dProgress, remainingTimeAsString(nRemainingTime));

         PrintToConsole(strProgress);
         // uiInterface.InitMessage(strProgressUI);
     }
 };

/**
 * Scans the blockchain for meta transactions.
 *
 * It scans the blockchain, starting at the given block index, to the current
 * tip, much like as if new block were arriving and being processed on the fly.
 *
 * Every 30 seconds the progress of the scan is reported.
 *
 * In case the current block being processed is not part of the active chain, or
 * if a block could not be retrieved from the disk, then the scan stops early.
 * Likewise, global shutdown requests are honored, and stop the scan progress.
 *
 * @see mastercore_handler_block_begin()
 * @see mastercore_handler_tx()
 * @see mastercore_handler_block_end()
 *
 * @param nFirstBlock[in]  The index of the first block to scan
 * @return An exit code, indicating success or failure
 */
static int msc_initial_scan(int nFirstBlock)
{
    int nTimeBetweenProgressReports = gArgs.GetArg("-omniprogressfrequency", 30);  // seconds
    int64_t nNow = GetTime();
    unsigned int nTxsTotal = 0;
    unsigned int nTxsFoundTotal = 0;
    int nBlock = 9999999;
    const int nLastBlock = GetHeight();

    // this function is useless if there are not enough blocks in the blockchain yet!
    if (nFirstBlock < 0 || nLastBlock < nFirstBlock) return -1;
    PrintToConsole("Scanning for transactions in block %d to block %d..\n", nFirstBlock, nLastBlock);

    // used to print the progress to the console and notifies the UI
    ProgressReporter progressReporter(chainActive[nFirstBlock], chainActive[nLastBlock]);

    for (nBlock = nFirstBlock; nBlock <= nLastBlock; ++nBlock)
    {
        if (ShutdownRequested()) {
            PrintToLog("Shutdown requested, stop scan at block %d of %d\n", nBlock, nLastBlock);
            break;
        }

        CBlockIndex* pblockindex = chainActive[nBlock];
        if (NULL == pblockindex) break;
        std::string strBlockHash = pblockindex->GetBlockHash().GetHex();

        if (msc_debug_exo) PrintToLog("%s(%d; max=%d):%s, line %d, file: %s\n",
            __FUNCTION__, nBlock, nLastBlock, strBlockHash, __LINE__, __FILE__);

        if (GetTime() >= nNow + nTimeBetweenProgressReports) {
             progressReporter.update(pblockindex);
             nNow = GetTime();
        }

        unsigned int nTxNum = 0;
        unsigned int nTxsFoundInBlock = 0;
        mastercore_handler_block_begin(nBlock, pblockindex);

        CBlock block;
        if (!ReadBlockFromDisk(block, pblockindex, Params().GetConsensus())) break;
          for(CTransactionRef& tx : block.vtx) {
             if (mastercore_handler_tx(*(tx.get()), nBlock, nTxNum, pblockindex)) ++nTxsFoundInBlock;
             ++nTxNum;
          }

        nTxsFoundTotal += nTxsFoundInBlock;
        nTxsTotal += nTxNum;
        mastercore_handler_block_end(nBlock, pblockindex, nTxsFoundInBlock);
    }

    if (nBlock < nLastBlock) {
        PrintToConsole("Scan stopped early at block %d of block %d\n", nBlock, nLastBlock);
    }

    PrintToConsole("%d transactions processed, %d meta transactions found\n", nTxsTotal, nTxsFoundTotal);

    return 0;
}

int input_msc_balances_string(const std::string& s)
{
    // "address=propertybalancedata"
    std::vector<std::string> addrData;
    boost::split(addrData, s, boost::is_any_of("="), boost::token_compress_on);
    if (addrData.size() != 2) return -1;

    std::string strAddress = addrData[0];

    // split the tuples of properties
    std::vector<std::string> vProperties;
    boost::split(vProperties, addrData[1], boost::is_any_of(";"), boost::token_compress_on);

    std::vector<std::string>::const_iterator iter;
    for (iter = vProperties.begin(); iter != vProperties.end(); ++iter) {
        if ((*iter).empty()) {
            continue;
        }

        // "propertyid:balancedata"
        std::vector<std::string> curProperty;
        boost::split(curProperty, *iter, boost::is_any_of(":"), boost::token_compress_on);
        if (curProperty.size() != 2) return -1;

        /*New things for contracts: save contractdex tally *////////////////////
        std::vector<std::string> curBalance;
        boost::split(curBalance, curProperty[1], boost::is_any_of(","), boost::token_compress_on);
        if (curBalance.size() != 13) return -1;


        uint32_t propertyId = boost::lexical_cast<uint32_t>(curProperty[0]);
        int64_t balance = boost::lexical_cast<int64_t>(curBalance[0]);
        int64_t sellReserved = boost::lexical_cast<int64_t>(curBalance[1]);
        int64_t acceptReserved = boost::lexical_cast<int64_t>(curBalance[2]);
        int64_t metadexReserved = boost::lexical_cast<int64_t>(curBalance[3]);
        int64_t contractdexReserved = boost::lexical_cast<int64_t>(curBalance[4]);
        int64_t positiveBalance = boost::lexical_cast<int64_t>(curBalance[5]);
        int64_t negativeBalance = boost::lexical_cast<int64_t>(curBalance[6]);
        int64_t realizeProfit = boost::lexical_cast<int64_t>(curBalance[7]);
        int64_t realizeLosses = boost::lexical_cast<int64_t>(curBalance[8]);
        int64_t count = boost::lexical_cast<int64_t>(curBalance[9]);
        int64_t remaining = boost::lexical_cast<int64_t>(curBalance[10]);
        int64_t liquidationPrice = boost::lexical_cast<int64_t>(curBalance[11]);
        int64_t upnl = boost::lexical_cast<int64_t>(curBalance[12]);
        int64_t nupnl = boost::lexical_cast<int64_t>(curBalance[13]);

        if (balance) update_tally_map(strAddress, propertyId, balance, BALANCE);
        if (sellReserved) update_tally_map(strAddress, propertyId, sellReserved, SELLOFFER_RESERVE);
        if (acceptReserved) update_tally_map(strAddress, propertyId, acceptReserved, ACCEPT_RESERVE);
        if (metadexReserved) update_tally_map(strAddress, propertyId, metadexReserved, METADEX_RESERVE);

        if (contractdexReserved) update_tally_map(strAddress, propertyId, contractdexReserved, CONTRACTDEX_RESERVE);
        if (positiveBalance) update_tally_map(strAddress, propertyId, positiveBalance, POSSITIVE_BALANCE);
        if (negativeBalance) update_tally_map(strAddress, propertyId, negativeBalance, NEGATIVE_BALANCE);
        if (realizeProfit) update_tally_map(strAddress, propertyId, realizeProfit, REALIZED_PROFIT);
        if (realizeLosses) update_tally_map(strAddress, propertyId, realizeLosses, REALIZED_LOSSES);
        if (count) update_tally_map(strAddress, propertyId, count, COUNT);
        if (remaining) update_tally_map(strAddress, propertyId, remaining, REMAINING);
        if (liquidationPrice) update_tally_map(strAddress, propertyId, liquidationPrice, LIQUIDATION_PRICE);
        if (upnl) update_tally_map(strAddress, propertyId, upnl, UPNL);
        if (upnl) update_tally_map(strAddress, propertyId, nupnl, NUPNL);
    }

    return 0;
}

int input_mp_offers_string(const std::string& s)
{
    std::vector<std::string> vstr;
    boost::split(vstr, s, boost::is_any_of(" ,="), boost::token_compress_on);

    if (9 != vstr.size()) return -1;

    int i = 0;

    std::string sellerAddr = vstr[i++];
    int offerBlock = boost::lexical_cast<int>(vstr[i++]);
    int64_t amountOriginal = boost::lexical_cast<int64_t>(vstr[i++]);
    uint32_t prop = boost::lexical_cast<uint32_t>(vstr[i++]);
    int64_t btcDesired = boost::lexical_cast<int64_t>(vstr[i++]);
    // uint32_t prop_desired = boost::lexical_cast<uint32_t>(vstr[i++]);
    int64_t minFee = boost::lexical_cast<int64_t>(vstr[i++]);
    uint8_t blocktimelimit = boost::lexical_cast<unsigned int>(vstr[i++]); // lexical_cast can't handle char!
    uint256 txid = uint256S(vstr[i++]);
    uint8_t option = 2;
    // TODO: should this be here? There are usually no sanity checks..
      // if (OMNI_PROPERTY_BTC != prop_desired) return -1;

    const std::string combo = STR_SELLOFFER_ADDR_PROP_COMBO(sellerAddr, prop);
    CMPOffer newOffer(offerBlock, amountOriginal, prop, btcDesired, minFee, blocktimelimit, txid, option);

    if (!my_offers.insert(std::make_pair(combo, newOffer)).second) return -1;

    return 0;
}

int input_globals_state_string(const string &s)
{
  unsigned int nextSPID, nextTestSPID;
  std::vector<std::string> vstr;
  boost::split(vstr, s, boost::is_any_of(" ,="), token_compress_on);
  if (2 != vstr.size()) return -1;

  int i = 0;
  nextSPID = boost::lexical_cast<unsigned int>(vstr[i++]);
  nextTestSPID = boost::lexical_cast<unsigned int>(vstr[i++]);
  PrintToLog("input_global_state, nextSPID : %s \n",nextSPID);
  PrintToLog("input_global_state, nextTestSPID : %s \n",nextTestSPID);
  _my_sps->init(nextSPID, nextTestSPID);
  return 0;
}

// addr,propertyId,nValue,property_desired,deadline,early_bird,percentage,txid
int input_mp_crowdsale_string(const std::string& s)
{
    std::vector<std::string> vstr;
    boost::split(vstr, s, boost::is_any_of(" ,"), boost::token_compress_on);

    if (9 > vstr.size()) return -1;

    unsigned int i = 0;

    std::string sellerAddr = vstr[i++];
    uint32_t propertyId = boost::lexical_cast<uint32_t>(vstr[i++]);
    int64_t nValue = boost::lexical_cast<int64_t>(vstr[i++]);
    uint32_t property_desired = boost::lexical_cast<uint32_t>(vstr[i++]);
    int64_t deadline = boost::lexical_cast<int64_t>(vstr[i++]);
    uint8_t early_bird = boost::lexical_cast<unsigned int>(vstr[i++]); // lexical_cast can't handle char!
    uint8_t percentage = boost::lexical_cast<unsigned int>(vstr[i++]); // lexical_cast can't handle char!
    int64_t u_created = boost::lexical_cast<int64_t>(vstr[i++]);
    int64_t i_created = boost::lexical_cast<int64_t>(vstr[i++]);

    CMPCrowd newCrowdsale(propertyId, nValue, property_desired, deadline, early_bird, percentage, u_created, i_created);

    // load the remaining as database pairs
    while (i < vstr.size()) {
        std::vector<std::string> entryData;
        boost::split(entryData, vstr[i++], boost::is_any_of("="), boost::token_compress_on);
        if (2 != entryData.size()) return -1;

        std::vector<std::string> valueData;
        boost::split(valueData, entryData[1], boost::is_any_of(";"), boost::token_compress_on);

        std::vector<int64_t> vals;
        for (std::vector<std::string>::const_iterator it = valueData.begin(); it != valueData.end(); ++it) {
            vals.push_back(boost::lexical_cast<int64_t>(*it));
        }

        uint256 txHash = uint256S(entryData[0]);
        newCrowdsale.insertDatabase(txHash, vals);
    }

    if (!my_crowds.insert(std::make_pair(sellerAddr, newCrowdsale)).second) {
        return -1;
    }

    return 0;
}

/* New things for contracts *///////////////////////////////////////////////////
int input_mp_contractdexorder_string(const std::string& s)
{
    std::vector<std::string> vstr;
    boost::split(vstr, s, boost::is_any_of(" ,="), boost::token_compress_on);

    if (12 != vstr.size()) return -1;

    int i = 0;

    std::string addr = vstr[i++];
    int block = boost::lexical_cast<int>(vstr[i++]);
    int64_t amount_forsale = boost::lexical_cast<int64_t>(vstr[i++]);
    uint32_t property = boost::lexical_cast<uint32_t>(vstr[i++]);
    int64_t amount_desired = boost::lexical_cast<int64_t>(vstr[i++]);
    uint32_t desired_property = boost::lexical_cast<uint32_t>(vstr[i++]);
    uint8_t subaction = boost::lexical_cast<unsigned int>(vstr[i++]); // lexical_cast can't handle char!
    unsigned int idx = boost::lexical_cast<unsigned int>(vstr[i++]);
    uint256 txid = uint256S(vstr[i++]);
    int64_t amount_remaining = boost::lexical_cast<int64_t>(vstr[i++]);
    uint64_t effective_price = boost::lexical_cast<uint64_t>(vstr[i++]);
    uint8_t trading_action = boost::lexical_cast<unsigned int>(vstr[i++]);

    CMPContractDex mdexObj(addr, block, property, amount_forsale, desired_property,
            amount_desired, txid, idx, subaction, amount_remaining, effective_price, trading_action);

    if (!ContractDex_INSERT(mdexObj)) return -1;

    return 0;
}

int input_market_prices_string(const std::string& s)
{
   std::vector<std::string> vstr;
   boost::split(vstr, s, boost::is_any_of(" ,="), boost::token_compress_on);

   if (NPTYPES != vstr.size()) return -1;
   for (int i = 0; i < NPTYPES; i++) marketP[i] = boost::lexical_cast<uint64_t>(vstr[i]);

   return 0;
}
////////////////////////////////////////////////////////////////////////////////

// address, block, amount for sale, property, amount desired, property desired, subaction, idx, txid, amount remaining
int input_mp_mdexorder_string(const std::string& s)
{
    std::vector<std::string> vstr;
    boost::split(vstr, s, boost::is_any_of(" ,="), boost::token_compress_on);

    if (10 != vstr.size()) return -1;

    int i = 0;

    std::string addr = vstr[i++];
    int block = boost::lexical_cast<int>(vstr[i++]);
    int64_t amount_forsale = boost::lexical_cast<int64_t>(vstr[i++]);
    uint32_t property = boost::lexical_cast<uint32_t>(vstr[i++]);
    int64_t amount_desired = boost::lexical_cast<int64_t>(vstr[i++]);
    uint32_t desired_property = boost::lexical_cast<uint32_t>(vstr[i++]);
    uint8_t subaction = boost::lexical_cast<unsigned int>(vstr[i++]); // lexical_cast can't handle char!
    unsigned int idx = boost::lexical_cast<unsigned int>(vstr[i++]);
    uint256 txid = uint256S(vstr[i++]);
    int64_t amount_remaining = boost::lexical_cast<int64_t>(vstr[i++]);

    CMPMetaDEx mdexObj(addr, block, property, amount_forsale, desired_property,
            amount_desired, txid, idx, subaction, amount_remaining);

    if (!MetaDEx_INSERT(mdexObj)) return -1;

    return 0;
}

static int msc_file_load(const string &filename, int what, bool verifyHash = false)
{
  int lines = 0;
  int (*inputLineFunc)(const string &) = NULL;

  SHA256_CTX shaCtx;
  SHA256_Init(&shaCtx);

  switch (what)
  {
    case FILETYPE_BALANCES:
      mp_tally_map.clear();
      inputLineFunc = input_msc_balances_string;
      break;

    case FILETYPE_GLOBALS:
      inputLineFunc = input_globals_state_string;
      break;

    case FILETYPE_CROWDSALES:
      my_crowds.clear();
      inputLineFunc = input_mp_crowdsale_string;
      break;

    case FILETYPE_CDEXORDERS:
        contractdex.clear();
        inputLineFunc = input_mp_contractdexorder_string;
        break;

    case FILETYPE_MARKETPRICES:
        inputLineFunc = input_market_prices_string;
        break;

    case FILETYPE_OFFERS:
      my_offers.clear();
      inputLineFunc = input_mp_offers_string;
      break;

    case FILETYPE_MDEXORDERS:
        // FIXME
        // memory leak ... gotta unallocate inner layers first....
        // TODO
        // ...
        metadex.clear();
        inputLineFunc = input_mp_mdexorder_string;
        break;

    default:
      return -1;
  }

  if (msc_debug_persistence)
  {
    LogPrintf("Loading %s ... \n", filename);
    PrintToLog("%s(%s), line %d, file: %s\n", __FUNCTION__, filename, __LINE__, __FILE__);
  }

  std::ifstream file;
  file.open(filename.c_str());
  if (!file.is_open())
  {
    if (msc_debug_persistence) LogPrintf("%s(%s): file not found, line %d, file: %s\n", __FUNCTION__, filename, __LINE__, __FILE__);
    return -1;
  }

  int res = 0;

  std::string fileHash;
  while (file.good())
  {
    std::string line;
    std::getline(file, line);
    if (line.empty() || line[0] == '#') continue;

    // remove \r if the file came from Windows
    line.erase( std::remove( line.begin(), line.end(), '\r' ), line.end() ) ;

    // record and skip hashes in the file
    if (line[0] == '!') {
      fileHash = line.substr(1);
      continue;
    }

    // update hash?
    if (verifyHash) {
      SHA256_Update(&shaCtx, line.c_str(), line.length());
    }

    if (inputLineFunc) {
      if (inputLineFunc(line) < 0) {
        res = -1;
        break;
      }
    }

    ++lines;
  }

  file.close();

  if (verifyHash && res == 0) {
    // generate and wite the double hash of all the contents written
    uint256 hash1;
    SHA256_Final((unsigned char*)&hash1, &shaCtx);
    uint256 hash2;
    SHA256((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);

    if (false == boost::iequals(hash2.ToString(), fileHash)) {
      PrintToLog("File %s loaded, but failed hash validation!\n", filename);
      res = -1;
    }
  }

  PrintToLog("%s(%s), loaded lines= %d, res= %d\n", __FUNCTION__, filename, lines, res);
  LogPrintf("%s(): file: %s , loaded lines= %d, res= %d\n", __FUNCTION__, filename, lines, res);

  return res;
}

static char const * const statePrefix[NUM_FILETYPES] = {
    "balances",
    "globals",
    "crowdsales",
    "cdexorders",
    "marketprices",
    "offers",
    "mdexorders",

};

// returns the height of the state loaded
static int load_most_relevant_state()
{
  int res = -1;
  // check the SP database and roll it back to its latest valid state
  // according to the active chain
  uint256 spWatermark;
  if (!_my_sps->getWatermark(spWatermark)) {
    //trigger a full reparse, if the SP database has no watermark
    return -1;
  }

  CBlockIndex const *spBlockIndex = GetBlockIndex(spWatermark);
  if (NULL == spBlockIndex) {
    //trigger a full reparse, if the watermark isn't a real block
    return -1;
  }

  while (NULL != spBlockIndex && false == chainActive.Contains(spBlockIndex)) {
    int remainingSPs = _my_sps->popBlock(spBlockIndex->GetBlockHash());
    if (remainingSPs < 0) {
      // trigger a full reparse, if the levelDB cannot roll back
      return -1;
    } /*else if (remainingSPs == 0) {
      // potential optimization here?
    }*/
    spBlockIndex = spBlockIndex->pprev;
    if (spBlockIndex != NULL) {
        _my_sps->setWatermark(spBlockIndex->GetBlockHash());
    }
  }

  // prepare a set of available files by block hash pruning any that are
  // not in the active chain
  std::set<uint256> persistedBlocks;
  boost::filesystem::directory_iterator dIter(MPPersistencePath);
  boost::filesystem::directory_iterator endIter;
  for (; dIter != endIter; ++dIter) {
    if (false == boost::filesystem::is_regular_file(dIter->status()) || dIter->path().empty()) {
      // skip funny business
      continue;
    }

    std::string fName = (*--dIter->path().end()).string();
    std::vector<std::string> vstr;
    boost::split(vstr, fName, boost::is_any_of("-."), token_compress_on);
    if (  vstr.size() == 3 &&
          boost::equals(vstr[2], "dat")) {
      uint256 blockHash;
      blockHash.SetHex(vstr[1]);
      CBlockIndex *pBlockIndex = GetBlockIndex(blockHash);
      if (pBlockIndex == NULL || false == chainActive.Contains(pBlockIndex)) {
        continue;
      }

      // this is a valid block in the active chain, store it
      persistedBlocks.insert(blockHash);
    }
  }

  // using the SP's watermark after its fixed-up as the tip
  // walk backwards until we find a valid and full set of persisted state files
  // for each block we discard, roll back the SP database
  // Note: to avoid rolling back all the way to the genesis block (which appears as if client is hung) abort after MAX_STATE_HISTORY attempts
  CBlockIndex const *curTip = spBlockIndex;
  int abortRollBackBlock;
  if (curTip != NULL) abortRollBackBlock = curTip->nHeight - (MAX_STATE_HISTORY+1);
  while (NULL != curTip && persistedBlocks.size() > 0 && curTip->nHeight > abortRollBackBlock) {
    if (persistedBlocks.find(spBlockIndex->GetBlockHash()) != persistedBlocks.end()) {
      int success = -1;
      for (int i = 0; i < NUM_FILETYPES; ++i) {
        boost::filesystem::path path = MPPersistencePath / strprintf("%s-%s.dat", statePrefix[i], curTip->GetBlockHash().ToString());
        const std::string strFile = path.string();
        success = msc_file_load(strFile, i, true);
        if (success < 0) {
          break;
        }
      }

      if (success >= 0) {
        res = curTip->nHeight;
        break;
      }

      // remove this from the persistedBlock Set
      persistedBlocks.erase(spBlockIndex->GetBlockHash());
    }

    // go to the previous block
    if (0 > _my_sps->popBlock(curTip->GetBlockHash())) {
      // trigger a full reparse, if the levelDB cannot roll back
      return -1;
    }
    curTip = curTip->pprev;
    if (curTip != NULL) {
        _my_sps->setWatermark(curTip->GetBlockHash());
    }
  }

  if (persistedBlocks.size() == 0) {
    // trigger a reparse if we exhausted the persistence files without success
    return -1;
  }

  // return the height of the block we settled at
  return res;
}

static int write_msc_balances(std::ofstream& file, SHA256_CTX* shaCtx)
{
    std::unordered_map<std::string, CMPTally>::iterator iter;
    for (iter = mp_tally_map.begin(); iter != mp_tally_map.end(); ++iter) {
        bool emptyWallet = true;

        std::string lineOut = (*iter).first;
        lineOut.append("=");
        CMPTally& curAddr = (*iter).second;
        curAddr.init();
        uint32_t propertyId = 0;
        while (0 != (propertyId = curAddr.next())) {
            int64_t balance = (*iter).second.getMoney(propertyId, BALANCE);
            int64_t sellReserved = (*iter).second.getMoney(propertyId, SELLOFFER_RESERVE);
            int64_t acceptReserved = (*iter).second.getMoney(propertyId, ACCEPT_RESERVE);
            int64_t metadexReserved = (*iter).second.getMoney(propertyId, METADEX_RESERVE);

            int64_t contractdexReserved = (*iter).second.getMoney(propertyId, CONTRACTDEX_RESERVE);
            int64_t positiveBalance = (*iter).second.getMoney(propertyId, POSSITIVE_BALANCE);
            int64_t negativeBalance = (*iter).second.getMoney(propertyId, NEGATIVE_BALANCE);
            int64_t realizedProfit = (*iter).second.getMoney(propertyId, REALIZED_PROFIT);
            int64_t realizedLosses = (*iter).second.getMoney(propertyId, REALIZED_LOSSES);
            int64_t count = (*iter).second.getMoney(propertyId, COUNT);
            int64_t remaining = (*iter).second.getMoney(propertyId, REMAINING);
            int64_t liquidationPrice = (*iter).second.getMoney(propertyId, LIQUIDATION_PRICE);
            int64_t upnl = (*iter).second.getMoney(propertyId, UPNL);
            int64_t nupnl = (*iter).second.getMoney(propertyId, NUPNL);
            // we don't allow 0 balances to read in, so if we don't write them
            // it makes things match up better between persisted state and processed state
            if (0 == balance && 0 == sellReserved && 0 == acceptReserved && 0 == metadexReserved && contractdexReserved == 0 && positiveBalance == 0 && negativeBalance == 0 && realizedProfit == 0 && realizedLosses == 0 && count == 0 && remaining == 0 && liquidationPrice == 0 && upnl == 0 && nupnl == 0) {
                continue;
            }

            emptyWallet = false;

            lineOut.append(strprintf("%d:%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d;",
                    propertyId,
                    balance,
                    sellReserved,
                    acceptReserved,
                    metadexReserved,
                    contractdexReserved,
                    positiveBalance,
                    negativeBalance,
                    realizedProfit,
                    realizedLosses,
                    count,
                    remaining,
                    liquidationPrice,
                    upnl,
                    nupnl));
          //  PrintToConsole("Inside write_msc_balances ...no problem with strprintf!!!\n");
        }

        if (false == emptyWallet) {
            // add the line to the hash
            SHA256_Update(shaCtx, lineOut.c_str(), lineOut.length());

            // write the line
            file << lineOut << endl;
        }
    }

    return 0;
}

static int write_globals_state(ofstream &file, SHA256_CTX *shaCtx)
{
  unsigned int nextSPID = _my_sps->peekNextSPID(OMNI_PROPERTY_MSC);
  unsigned int nextTestSPID = _my_sps->peekNextSPID(OMNI_PROPERTY_TMSC);
  std::string lineOut = strprintf("%d,%d",
    nextSPID,
    nextTestSPID);

  PrintToLog("write_global_state, lineOut : %s \n",lineOut);

  // add the line to the hash
  SHA256_Update(shaCtx, lineOut.c_str(), lineOut.length());

  // write the line
  file << lineOut << endl;

  return 0;
}

static int write_mp_crowdsales(std::ofstream& file, SHA256_CTX* shaCtx)
{
    for (CrowdMap::const_iterator it = my_crowds.begin(); it != my_crowds.end(); ++it) {
        // decompose the key for address
        const CMPCrowd& crowd = it->second;
        crowd.saveCrowdSale(file, shaCtx, it->first);
    }

    return 0;
}

////////////////////////////
/** New things for Contract */
static int write_mp_contractdex(ofstream &file, SHA256_CTX *shaCtx)
{
  for (cd_PropertiesMap::iterator my_it = contractdex.begin(); my_it != contractdex.end(); ++my_it)
  {
    cd_PricesMap &prices = my_it->second;
    for (cd_PricesMap::iterator it = prices.begin(); it != prices.end(); ++it)
    {
      cd_Set &indexes = (it->second);
      for (cd_Set::iterator it = indexes.begin(); it != indexes.end(); ++it)
      {
        CMPContractDex contract = *it;
        contract.saveOffer(file, shaCtx);
      }
    }
  }
  return 0;
}

static int write_market_pricescd(ofstream &file, SHA256_CTX *shaCtx)
{
  std::string lineOut;

  for (int i = 0; i < NPTYPES; i++) lineOut.append(strprintf("%d", marketP[i]));
  // add the line to the hash
  SHA256_Update(shaCtx, lineOut.c_str(), lineOut.length());
  // write the line
  file << lineOut << endl;

  return 0;
}

static int write_mp_metadex(ofstream &file, SHA256_CTX *shaCtx)
{
  for (md_PropertiesMap::iterator my_it = metadex.begin(); my_it != metadex.end(); ++my_it)
  {
    md_PricesMap & prices = my_it->second;
    for (md_PricesMap::iterator it = prices.begin(); it != prices.end(); ++it)
    {
      md_Set & indexes = (it->second);
      for (md_Set::iterator it = indexes.begin(); it != indexes.end(); ++it)
      {
        CMPMetaDEx meta = *it;
        meta.saveOffer(file, shaCtx);
      }
    }
  }

  return 0;
}

static int write_mp_offers(ofstream &file, SHA256_CTX *shaCtx)
{
  OfferMap::const_iterator iter;
  for (iter = my_offers.begin(); iter != my_offers.end(); ++iter) {
    // decompose the key for address
    std::vector<std::string> vstr;
    boost::split(vstr, (*iter).first, boost::is_any_of("-"), token_compress_on);
    CMPOffer const &offer = (*iter).second;
    offer.saveOffer(file, shaCtx, vstr[0]);
  }
  return 0;
}

static int write_state_file( CBlockIndex const *pBlockIndex, int what )
{
  boost::filesystem::path path = MPPersistencePath / strprintf("%s-%s.dat", statePrefix[what], pBlockIndex->GetBlockHash().ToString());
  const std::string strFile = path.string();

  std::ofstream file;
  file.open(strFile.c_str());

  SHA256_CTX shaCtx;
  SHA256_Init(&shaCtx);

  int result = 0;

  switch(what) {
  case FILETYPE_BALANCES:
    result = write_msc_balances(file, &shaCtx);
    break;

  case FILETYPE_GLOBALS:
    result = write_globals_state(file, &shaCtx);
    break;

  case FILETYPE_CROWDSALES:
    result = write_mp_crowdsales(file, &shaCtx);
    break;

  case FILETYPE_CDEXORDERS:
     result = write_mp_contractdex(file, &shaCtx);
     break;

  case FILETYPE_MARKETPRICES:
     result = write_market_pricescd(file, &shaCtx);
     break;

  case FILETYPE_OFFERS:
     result = write_mp_offers(file, &shaCtx);
     break;

  case FILETYPE_MDEXORDERS:
      result = write_mp_metadex(file, &shaCtx);
      break;

  }

  // generate and wite the double hash of all the contents written
  uint256 hash1;
  SHA256_Final((unsigned char*)&hash1, &shaCtx);
  uint256 hash2;
  SHA256((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
  file << "!" << hash2.ToString() << endl;

  file.flush();
  file.close();
  return result;
}

static bool is_state_prefix( std::string const &str )
{
  for (int i = 0; i < NUM_FILETYPES; ++i) {
    if (boost::equals(str,  statePrefix[i])) {
      return true;
    }
  }

  return false;
}

static void prune_state_files( CBlockIndex const *topIndex )
{
  // build a set of blockHashes for which we have any state files
  std::set<uint256> statefulBlockHashes;

  boost::filesystem::directory_iterator dIter(MPPersistencePath);
  boost::filesystem::directory_iterator endIter;
  for (; dIter != endIter; ++dIter) {
    std::string fName = dIter->path().empty() ? "<invalid>" : (*--dIter->path().end()).string();
    if (false == boost::filesystem::is_regular_file(dIter->status())) {
      // skip funny business
      PrintToLog("Non-regular file found in persistence directory : %s\n", fName);
      continue;
    }

    std::vector<std::string> vstr;
    boost::split(vstr, fName, boost::is_any_of("-."), token_compress_on);
    if (  vstr.size() == 3 &&
          is_state_prefix(vstr[0]) &&
          boost::equals(vstr[2], "dat")) {
      uint256 blockHash;
      blockHash.SetHex(vstr[1]);
      statefulBlockHashes.insert(blockHash);
    } else {
      PrintToLog("None state file found in persistence directory : %s\n", fName);
    }
  }

  // for each blockHash in the set, determine the distance from the given block
  std::set<uint256>::const_iterator iter;
  for (iter = statefulBlockHashes.begin(); iter != statefulBlockHashes.end(); ++iter) {
    // look up the CBlockIndex for height info
    CBlockIndex const *curIndex = GetBlockIndex(*iter);

    // if we have nothing int the index, or this block is too old..
    if (NULL == curIndex || (topIndex->nHeight - curIndex->nHeight) > MAX_STATE_HISTORY ) {
     if (msc_debug_persistence)
     {
      if (curIndex) {
        PrintToLog("State from Block:%s is no longer need, removing files (age-from-tip: %d)\n", (*iter).ToString(), topIndex->nHeight - curIndex->nHeight);
      } else {
        PrintToLog("State from Block:%s is no longer need, removing files (not in index)\n", (*iter).ToString());
      }
     }

      // destroy the associated files!
      std::string strBlockHash = iter->ToString();
      for (int i = 0; i < NUM_FILETYPES; ++i) {
        boost::filesystem::path path = MPPersistencePath / strprintf("%s-%s.dat", statePrefix[i], strBlockHash);
        boost::filesystem::remove(path);
      }
    }
  }
}

int mastercore_save_state( CBlockIndex const *pBlockIndex )
{
    // write the new state as of the given block
    write_state_file(pBlockIndex, FILETYPE_BALANCES);
    write_state_file(pBlockIndex, FILETYPE_GLOBALS);
    write_state_file(pBlockIndex, FILETYPE_CROWDSALES);
    /*New things for contracts*/////////////////////////////////////////////////
    write_state_file(pBlockIndex, FILETYPE_CDEXORDERS);
    write_state_file(pBlockIndex, FILETYPE_MARKETPRICES);
    write_state_file(pBlockIndex, FILETYPE_OFFERS);
    write_state_file(pBlockIndex, FILETYPE_MDEXORDERS);

    ////////////////////////////////////////////////////////////////////////////
    // clean-up the directory
    prune_state_files(pBlockIndex);

    _my_sps->setWatermark(pBlockIndex->GetBlockHash());

    return 0;
}

/**
 * Clears the state of the system.
 */
void clear_all_state()
{
    LOCK2(cs_tally, cs_pending);

    // Memory based storage
    mp_tally_map.clear();
    my_crowds.clear();
    my_pending.clear();
    my_offers.clear();
    my_accepts.clear();
    metadex.clear();
    my_pending.clear();
    contractdex.clear();
    ResetConsensusParams();
    // ClearActivations();
    // ClearAlerts();
    //
    // LevelDB based storage
     _my_sps->Clear();
     // s_stolistdb->Clear();
     t_tradelistdb->Clear();
     p_txlistdb->Clear();
     p_OmniTXDB->Clear();

     // assert(p_txlistdb->setDBVersion() == DB_VERSION); // new set of databases, set DB version
}

/**
 * Global handler to initialize Omni Core.
 *
 * @return An exit code, indicating success or failure
 */
int mastercore_init()
{
    LOCK(cs_tally);

    if (mastercoreInitialized) {
        // nothing to do
        return 0;
    }

    // PrintToConsole("Initializing Omni Core Lite v%s [%s]\n", OmniCoreVersion(), Params().NetworkIDString());
    //
    PrintToLog("\nInitializing Omni Core Lite\n");
    PrintToLog("Startup time: %s\n", DateTimeStrFormat("%Y-%m-%d %H:%M:%S", GetTime()));
    // PrintToLog("Build date: %s, based on commit: %s\n", BuildDate(), BuildCommit());

    InitDebugLogLevels();
    ShrinkDebugLog();

    // check for --autocommit option and set transaction commit flag accordingly
     if (!gArgs.GetBoolArg("-autocommit", true)) {
         PrintToLog("Process was started with --autocommit set to false. "
                 "Created Omni transactions will not be committed to wallet or broadcast.\n");
         autoCommit = false;
     }

    // check for --startclean option and delete MP_ folders if present
     bool startClean = false;
    if (gArgs.GetBoolArg("-startclean", false)) {
         PrintToLog("Process was started with --startclean option, attempting to clear persistence files..\n");
         try {
             boost::filesystem::path persistPath = GetDataDir() / "OCL_persist";
             boost::filesystem::path txlistPath = GetDataDir() / "OCL_txlist";
             boost::filesystem::path spPath = GetDataDir() / "OCL_spinfo";
             boost::filesystem::path omniTXDBPath = GetDataDir() / "OCL_TXDB";
             if (boost::filesystem::exists(persistPath)) boost::filesystem::remove_all(persistPath);
             if (boost::filesystem::exists(txlistPath)) boost::filesystem::remove_all(txlistPath);
             if (boost::filesystem::exists(spPath)) boost::filesystem::remove_all(spPath);
             if (boost::filesystem::exists(omniTXDBPath)) boost::filesystem::remove_all(omniTXDBPath);
             PrintToLog("Success clearing persistence files in datadir %s\n", GetDataDir().string());
             startClean = true;
         } catch (const boost::filesystem::filesystem_error& e) {
             PrintToLog("Failed to delete persistence folders: %s\n", e.what());
             PrintToConsole("Failed to delete persistence folders: %s\n", e.what());
         }
    }

    p_txlistdb = new CMPTxList(GetDataDir() / "OCL_txlist", fReindex);
    _my_sps = new CMPSPInfo(GetDataDir() / "OCL_spinfo", fReindex);
    p_OmniTXDB = new COmniTransactionDB(GetDataDir() / "OCL_TXDB", fReindex);
    t_tradelistdb = new CMPTradeList(GetDataDir()/"OCL_tradelist", fReindex);
    MPPersistencePath = GetDataDir() / "OCL_persist";
    TryCreateDirectory(MPPersistencePath);

     bool wrongDBVersion = (p_txlistdb->getDBVersion() != DB_VERSION);

     ++mastercoreInitialized;

    nWaterlineBlock = load_most_relevant_state();
    bool noPreviousState = (nWaterlineBlock <= 0);

     if (startClean) {
         assert(p_txlistdb->setDBVersion() == DB_VERSION); // new set of databases, set DB version
     } else if (wrongDBVersion) {
          nWaterlineBlock = -1; // force a clear_all_state and parse from start
     }

     if (nWaterlineBlock > 0) {
         PrintToConsole("Loading persistent state: OK [block %d]\n", nWaterlineBlock);
     } else {
         std::string strReason = "unknown";
         if (wrongDBVersion) strReason = "client version changed";
         if (noPreviousState) strReason = "no usable previous state found";
         if (startClean) strReason = "-startclean parameter used";
         PrintToConsole("Loading persistent state: NONE (%s)\n", strReason);
     }

    if (nWaterlineBlock < 0) {
        //persistence says we reparse!, nuke some stuff in case the partial loads left stale bits
        clear_all_state();
    }

    // legacy code, setting to pre-genesis-block
    int snapshotHeight = ConsensusParams().GENESIS_BLOCK - 1;

    if (nWaterlineBlock < snapshotHeight) {
        nWaterlineBlock = snapshotHeight;
    }

    // advance the waterline so that we start on the next unaccounted for block
    nWaterlineBlock += 1;

    // load feature activation messages from txlistdb and process them accordingly
    p_txlistdb->LoadActivations(nWaterlineBlock);

    // load all alerts from levelDB (and immediately expire old ones)
    p_txlistdb->LoadAlerts(nWaterlineBlock);

    // initial scan
    msc_initial_scan(nWaterlineBlock);

    PrintToLog("Omni Core Lite initialization completed\n");

    return 0;
}

/**
 * Global handler to shut down Omni Core.
 *
 * In particular, the LevelDB databases of the global state objects are closed
 * properly.
 *
 * @return An exit code, indicating success or failure
 */
int mastercore_shutdown()
{
    LOCK(cs_tally);

    if (p_txlistdb) {
        delete p_txlistdb;
        p_txlistdb = NULL;
    }
    if (t_tradelistdb) {
        delete t_tradelistdb;
        t_tradelistdb = NULL;
    }
    if (_my_sps) {
        delete _my_sps;
        _my_sps = NULL;
    }
    if (p_OmniTXDB) {
        delete p_OmniTXDB;
        p_OmniTXDB = NULL;
    }

    mastercoreInitialized = 0;

    PrintToLog("\nOmni Core Lite shutdown completed\n");
    PrintToLog("Shutdown time: %s\n", DateTimeStrFormat("%Y-%m-%d %H:%M:%S", GetTime()));

    PrintToConsole("Omni Core Lite shutdown completed\n");

    return 0;
}

/**
 * This handler is called for every new transaction that comes in (actually in block parsing loop).
 *
 * @return True, if the transaction was a valid Omni transaction
 */
bool mastercore_handler_tx(const CTransaction& tx, int nBlock, unsigned int idx, const CBlockIndex* pBlockIndex)
{
  extern volatile int id_contract;

  LOCK(cs_tally);

  if (!mastercoreInitialized)
    {
      mastercore_init();
    }

  // clear pending, if any
  // NOTE1: Every incoming TX is checked, not just MP-ones because:
  // if for some reason the incoming TX doesn't pass our parser validation steps successfuly, I'd still want to clear pending amounts for that TX.
  // NOTE2: Plus I wanna clear the amount before that TX is parsed by our protocol, in case we ever consider pending amounts in internal calculations.
  PendingDelete(tx.GetHash());

  // we do not care about parsing blocks prior to our waterline (empty blockchain defense)
  if (nBlock < nWaterlineBlock) return false;
  int64_t nBlockTime = pBlockIndex->GetBlockTime();

  CMPTransaction mp_obj;
  mp_obj.unlockLogic();

  int expirationBlock = 0, actualBlock = 0, checkExpiration = 0;
  CMPSPInfo::Entry sp;
  if ( id_contract != 0 )
    {
      if (_my_sps->getSP(id_contract, sp) && sp.subcategory ==  "Futures Contracts")
	{
	  expirationBlock = static_cast<int>(sp.blocks_until_expiration);
	  actualBlock = static_cast<int>(pBlockIndex->nHeight);
        }
    }
  PrintToLog("nBlockTime: %d\n", static_cast<int>(nBlockTime));
  PrintToLog("expirationBlock: %d\n", static_cast<int>(expirationBlock));

  int deadline = sp.init_block + expirationBlock;
  if ( actualBlock != 0 && deadline != 0 )
    checkExpiration = actualBlock == deadline ? 1 : 0;

  if (checkExpiration)
    {
      idx_expiration += 1;
      if ( idx_expiration == 2 )
        {
	  PrintToLog("actualBlock: %d\n", actualBlock);
	  PrintToLog("deadline: %d\n", deadline);
	  PrintToLog("\nExpiration Date Achieve\n");
	  PrintToLog("checkExpiration: %d\n", checkExpiration);
	  expirationAchieve = 1;
        }
      else expirationAchieve = 0;
    }
  else
    {
      PrintToLog("actualBlock: %d\n", actualBlock);
      PrintToLog("deadline: %d\n", deadline);
      expirationAchieve = 0;
    }

  bool fFoundTx = false;
  int pop_ret = parseTransaction(false, tx, nBlock, idx, mp_obj, nBlockTime);

  if (0 == pop_ret)
    {
      assert(mp_obj.getEncodingClass() != NO_MARKER);
      assert(mp_obj.getSender().empty() == false);

      int interp_ret = mp_obj.interpretPacket();
      if (interp_ret) PrintToLog("!!! interpretPacket() returned %d !!!\n", interp_ret);

      // Only structurally valid transactions get recorded in levelDB
      // PKT_ERROR - 2 = interpret_Transaction failed, structurally invalid payload
      if (interp_ret != PKT_ERROR - 2)
	      {
	          bool bValid = (0 <= interp_ret);
	          p_txlistdb->recordTX(tx.GetHash(), bValid, nBlock, mp_obj.getType(), mp_obj.getNewAmount());
	          p_OmniTXDB->RecordTransaction(tx.GetHash(), idx);
        }

        fFoundTx |= (interp_ret == 0);
    }
  else if (pop_ret > 0)
    fFoundTx |= HandleDExPayments(tx, nBlock, mp_obj.getSender()); // testing the payment handler

  if (fFoundTx && msc_debug_consensus_hash_every_transaction)
    {
      uint256 consensusHash = GetConsensusHash();
      PrintToLog("Consensus hash for transaction %s: %s\n", tx.GetHash().GetHex(), consensusHash.GetHex());
    }

  return fFoundTx;
}

/**
 * Determines, whether it is valid to use a Class C transaction for a given payload size.
 *
 * @param nDataSize The length of the payload
 * @return True, if Class C is enabled and the payload is small enough
 */
bool mastercore::UseEncodingClassC(size_t nDataSize)
{
    size_t nTotalSize = nDataSize + GetOmMarker().size(); // Marker "ol"
    bool fDataEnabled = gArgs.GetBoolArg("-datacarrier", true);
    int nBlockNow = GetHeight();
    if (!IsAllowedOutputType(TX_NULL_DATA, nBlockNow)) {
        fDataEnabled = false;
    }
    return nTotalSize <= nMaxDatacarrierBytes && fDataEnabled;
}

// This function requests the wallet create an Omni transaction using the supplied parameters and payload
int mastercore::WalletTxBuilder(const std::string& senderAddress, const std::string& receiverAddress, int64_t referenceAmount,
     const std::vector<unsigned char>& data, uint256& txid, std::string& rawHex, bool commit, unsigned int minInputs)
{
#ifdef ENABLE_WALLET
    CWalletRef pwalletMain = NULL;
    if (vpwallets.size() > 0){
        pwalletMain = vpwallets[0];
    }

    if (pwalletMain == NULL) return MP_ERR_WALLET_ACCESS;

    /**
    Omni Core Lite's core purpose is to be light weight thus:
    + multisig and plain address encoding are banned
    + nulldata encoding must use compression (varint)
    **/

    if (nMaxDatacarrierBytes < (data.size()+GetOmMarker().size())) return MP_ERR_PAYLOAD_TOO_BIG;

    //TODO verify datacarrier is enabled at startup, otherwise won't be able to send transactions

    // Prepare the transaction - first setup some vars
    CCoinControl coinControl;
    coinControl.fAllowOtherInputs = true;
    CWalletTx wtxNew;
    CAmount nFeeRet = 0;
    int nChangePosInOut = -1;
    std::string strFailReason;
    std::vector<std::pair<CScript, int64_t> > vecSend;
    CReserveKey reserveKey(pwalletMain);

    // Next, we set the change address to the sender
    coinControl.destChange = DecodeDestination(senderAddress);

    // Select the inputs
    if (0 > SelectCoins(senderAddress, coinControl, referenceAmount, minInputs)) { return MP_INPUTS_INVALID; }

    // Encode the data outputs

    if(!OmniCore_Encode_ClassD(data,vecSend)) { return MP_ENCODING_ERROR; }


    // Then add a paytopubkeyhash output for the recipient (if needed) - note we do this last as we want this to be the highest vout
    if (!receiverAddress.empty()) {
        CScript scriptPubKey = GetScriptForDestination(DecodeDestination(receiverAddress));
        vecSend.push_back(std::make_pair(scriptPubKey, 0 < referenceAmount ? referenceAmount : 50000));
    }

    // Now we have what we need to pass to the wallet to create the transaction, perform some checks first

    if (!coinControl.HasSelected()) return MP_ERR_INPUTSELECT_FAIL;

    std::vector<CRecipient> vecRecipients;
    for (size_t i = 0; i < vecSend.size(); ++i) {
        const std::pair<CScript, int64_t>& vec = vecSend[i];
        CRecipient recipient = {vec.first, CAmount(vec.second), false};
        vecRecipients.push_back(recipient);
    }

    // Ask the wallet to create the transaction (note mining fee determined by Bitcoin Core params)
    if (!pwalletMain->CreateTransaction(vecRecipients, wtxNew, reserveKey, nFeeRet, nChangePosInOut, strFailReason, coinControl, true)) {
        return MP_ERR_CREATE_TX; }

    // Workaround for SigOps limit
    {
         if (!FillTxInputCache(*(wtxNew.tx))) {
             PrintToLog("%s ERROR: failed to get inputs for %s after createtransaction\n", __func__, wtxNew.GetHash().GetHex());
         }

        unsigned int nBytesPerSigOp = 20; // default of Bitcoin Core 12.1
        unsigned int nSize = ::GetSerializeSize(wtxNew, SER_NETWORK, PROTOCOL_VERSION);
        unsigned int nSigOps = GetLegacySigOpCount(*(wtxNew.tx));
        nSigOps += GetP2SHSigOpCount(*(wtxNew.tx), view);

        if (nSigOps > nSize / nBytesPerSigOp) {
             std::vector<COutPoint> vInputs;
             coinControl.ListSelected(vInputs);

             // Ensure the requested number of inputs was available, so there may be more
             if (vInputs.size() >= minInputs) {
                 //Build a new transaction and try to select one additional input to
                 //shift the bytes per sigops ratio in our favor
                 ++minInputs;
                 return WalletTxBuilder(senderAddress, receiverAddress, referenceAmount, data, txid, rawHex, commit, minInputs);
             } else {
                 PrintToLog("%s WARNING: %s has %d sigops, and may not confirm in time\n",
                         __func__, wtxNew.GetHash().GetHex(), nSigOps);
             }
         }
    }

    // If this request is only to create, but not commit the transaction then display it and exit
    if (!commit) {
        rawHex = EncodeHexTx(*(wtxNew.tx));
        return 0;
    } else {
        // Commit the transaction to the wallet and broadcast)
        CValidationState state;
        if (!pwalletMain->CommitTransaction(wtxNew, reserveKey, g_connman.get(), state)) return MP_ERR_COMMIT_TX;
        txid = wtxNew.GetHash();
        return 0;
    }
#else
    return MP_ERR_WALLET_ACCESS;
#endif

}

void COmniTransactionDB::RecordTransaction(const uint256& txid, uint32_t posInBlock)
{
     assert(pdb);

     const std::string key = txid.ToString();
     const std::string value = strprintf("%d", posInBlock);

     Status status = pdb->Put(writeoptions, key, value);
     ++nWritten;
}

uint32_t COmniTransactionDB::FetchTransactionPosition(const uint256& txid)
{
    assert(pdb);

    const std::string key = txid.ToString();
    std::string strValue;
    uint32_t posInBlock = 999999; // setting an initial arbitrarily high value will ensure transaction is always "last" in event of bug/exploit

    Status status = pdb->Get(readoptions, key, &strValue);
    if (status.ok()) {
        posInBlock = boost::lexical_cast<uint32_t>(strValue);
    }

    return posInBlock;
}

void CMPTxList::LoadActivations(int blockHeight)
{
     if (!pdb) return;

     Slice skey, svalue;
     Iterator* it = NewIterator();

     PrintToLog("Loading feature activations from levelDB\n");

     std::vector<std::pair<int64_t, uint256> > loadOrder;

     for (it->SeekToFirst(); it->Valid(); it->Next()) {
         std::string itData = it->value().ToString();
         std::vector<std::string> vstr;
         boost::split(vstr, itData, boost::is_any_of(":"), token_compress_on);
         if (4 != vstr.size()) continue; // unexpected number of tokens
         if (atoi(vstr[2]) != OMNICORE_MESSAGE_TYPE_ACTIVATION || atoi(vstr[0]) != 1) continue; // we only care about valid activations
         uint256 txid = uint256S(it->key().ToString());;
         loadOrder.push_back(std::make_pair(atoi(vstr[1]), txid));
     }

     std::sort (loadOrder.begin(), loadOrder.end());

     for (std::vector<std::pair<int64_t, uint256> >::iterator it = loadOrder.begin(); it != loadOrder.end(); ++it) {
         uint256 hash = (*it).second;
         uint256 blockHash;
         CTransactionRef wtx;
         CMPTransaction mp_obj;

         if (!GetTransaction(hash, wtx, Params().GetConsensus(), blockHash, true)) {
              PrintToLog("ERROR: While loading activation transaction %s: tx in levelDB but does not exist.\n", hash.GetHex());
              continue;
         }

         if (blockHash.IsNull() || (NULL == GetBlockIndex(blockHash))) {
             PrintToLog("ERROR: While loading activation transaction %s: failed to retrieve block hash.\n", hash.GetHex());
             continue;
         }
         CBlockIndex* pBlockIndex = GetBlockIndex(blockHash);
         if (NULL == pBlockIndex) {
             PrintToLog("ERROR: While loading activation transaction %s: failed to retrieve block index.\n", hash.GetHex());
             continue;
         }
         int blockHeight = pBlockIndex->nHeight;
         if (0 != ParseTransaction(*(wtx), blockHeight, 0, mp_obj)) {
             PrintToLog("ERROR: While loading activation transaction %s: failed ParseTransaction.\n", hash.GetHex());
             continue;
         }
         if (!mp_obj.interpret_Transaction()) {
             PrintToLog("ERROR: While loading activation transaction %s: failed interpret_Transaction.\n", hash.GetHex());
             continue;
         }
         if (OMNICORE_MESSAGE_TYPE_ACTIVATION != mp_obj.getType()) {
             PrintToLog("ERROR: While loading activation transaction %s: levelDB type mismatch, not an activation.\n", hash.GetHex());
             continue;
         }
         mp_obj.unlockLogic();
         if (0 != mp_obj.interpretPacket()) {
             PrintToLog("ERROR: While loading activation transaction %s: non-zero return from interpretPacket\n", hash.GetHex());
             continue;
         }
     }
     delete it;
     CheckLiveActivations(blockHeight);


    // This alert never expires as long as custom activations are used
    // if (mapArgs.count("-omniactivationallowsender") || mapArgs.count("-omniactivationignoresender")) {
    //     AddAlert("omnicore", ALERT_CLIENT_VERSION_EXPIRY, std::numeric_limits<uint32_t>::max(),
    //              "Authorization for feature activation has been modified.  Data provided by this client should not be trusted.");
    // }
}

void CMPTxList::LoadAlerts(int blockHeight)
{
     if (!pdb) return;
    Slice skey, svalue;
    Iterator* it = NewIterator();

    std::vector<std::pair<int64_t, uint256> > loadOrder;

    for (it->SeekToFirst(); it->Valid(); it->Next()) {
         std::string itData = it->value().ToString();
         std::vector<std::string> vstr;
         boost::split(vstr, itData, boost::is_any_of(":"), token_compress_on);
         if (4 != vstr.size()) continue; // unexpected number of tokens
         if (atoi(vstr[2]) != OMNICORE_MESSAGE_TYPE_ALERT || atoi(vstr[0]) != 1) continue; // not a valid alert
         uint256 txid = uint256S(it->key().ToString());;
         loadOrder.push_back(std::make_pair(atoi(vstr[1]), txid));
     }

     std::sort (loadOrder.begin(), loadOrder.end());

     for (std::vector<std::pair<int64_t, uint256> >::iterator it = loadOrder.begin(); it != loadOrder.end(); ++it) {
         uint256 txid = (*it).second;
         uint256 blockHash;
         CTransaction wtx;
         CMPTransaction mp_obj;
         // if (!GetTransaction(txid, wtx, Params().GetConsensus(), blockHash, true)) {
         //      PrintToLog("ERROR: While loading alert %s: tx in levelDB but does not exist.\n", txid.GetHex());
         //      continue;
         // }

         if (0 != ParseTransaction(wtx, blockHeight, 0, mp_obj)) {
             PrintToLog("ERROR: While loading alert %s: failed ParseTransaction.\n", txid.GetHex());
             continue;
         }
         if (!mp_obj.interpret_Transaction()) {
             PrintToLog("ERROR: While loading alert %s: failed interpret_Transaction.\n", txid.GetHex());
             continue;
         }
         if (OMNICORE_MESSAGE_TYPE_ALERT != mp_obj.getType()) {
             PrintToLog("ERROR: While loading alert %s: levelDB type mismatch, not an alert.\n", txid.GetHex());
             continue;
         }
         if (!CheckAlertAuthorization(mp_obj.getSender())) {
             PrintToLog("ERROR: While loading alert %s: sender is not authorized to send alerts.\n", txid.GetHex());
             continue;
         }

         if (mp_obj.getAlertType() == 65535) { // set alert type to FFFF to clear previously sent alerts
             DeleteAlerts(mp_obj.getSender());
         } else {
             AddAlert(mp_obj.getSender(), mp_obj.getAlertType(), mp_obj.getAlertExpiry(), mp_obj.getAlertMessage());
         }
     }

     delete it;
     int64_t blockTime = 0;
     {
         LOCK(cs_main);
         CBlockIndex* pBlockIndex = chainActive[blockHeight-1];
         if (pBlockIndex != NULL) {
             blockTime = pBlockIndex->GetBlockTime();
         }
     }
     if (blockTime > 0) {
         CheckExpiredAlerts(blockHeight, blockTime);
     }
}

/*
 * Gets the DB version from txlistdb
 *
 * Returns the current version
 */
int CMPTxList::getDBVersion()
{
    std::string strValue;
    int verDB = 0;

    Status status = pdb->Get(readoptions, "dbversion", &strValue);
    if (status.ok()) {
        verDB = boost::lexical_cast<uint64_t>(strValue);
    }

    if (msc_debug_txdb) PrintToLog("%s(): dbversion %s status %s, line %d, file: %s\n", __FUNCTION__, strValue, status.ToString(), __LINE__, __FILE__);

    return verDB;
}

/*
 * Sets the DB version for txlistdb
 *
 * Returns the current version after update
 */
int CMPTxList::setDBVersion()
{
    std::string verStr = boost::lexical_cast<std::string>(DB_VERSION);
    Status status = pdb->Put(writeoptions, "dbversion", verStr);

    if (msc_debug_txdb) PrintToLog("%s(): dbversion %s status %s, line %d, file: %s\n", __FUNCTION__, verStr, status.ToString(), __LINE__, __FILE__);

    return getDBVersion();
}

/**
 * Returns the number of sub records.
 */
 int CMPTxList::getNumberOfSubRecords(const uint256& txid)
{
     int numberOfSubRecords = 0;

     std::string strValue;
     Status status = pdb->Get(readoptions, txid.ToString(), &strValue);
     if (status.ok()) {
         std::vector<std::string> vstr;
         boost::split(vstr, strValue, boost::is_any_of(":"), boost::token_compress_on);
         if (4 <= vstr.size()) {
             numberOfSubRecords = boost::lexical_cast<int>(vstr[3]);
         }
     }

     return numberOfSubRecords;
}

int CMPTxList::getMPTransactionCountTotal()
{
     int count = 0;
     Slice skey, svalue;
     Iterator* it = NewIterator();
     for(it->SeekToFirst(); it->Valid(); it->Next())
     {
         skey = it->key();
         if (skey.ToString().length() == 64) { ++count; } //extra entries for cancels and purchases are more than 64 chars long
     }
     delete it;
     return count;
}

int CMPTxList::getMPTransactionCountBlock(int block)
{
     int count = 0;
     Slice skey, svalue;
     Iterator* it = NewIterator();
     for(it->SeekToFirst(); it->Valid(); it->Next())
     {
         skey = it->key();
         svalue = it->value();
         if (skey.ToString().length() == 64)
         {
             string strValue = svalue.ToString();
             std::vector<std::string> vstr;
             boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);
             if (4 == vstr.size())
             {
                 if (atoi(vstr[1]) == block) { ++count; }
             }
         }
     }
     delete it;
     return count;
}

string CMPTxList::getKeyValue(string key)
{
     if (!pdb) return "";
     string strValue;
     Status status = pdb->Get(readoptions, key, &strValue);
     if (status.ok()) { return strValue; } else { return ""; }
}

/**
 * Retrieves details about a "send all" record.
*/
bool CMPTxList::getSendAllDetails(const uint256& txid, int subSend, uint32_t& propertyId, int64_t& amount)
{
     std::string strKey = strprintf("%s-%d", txid.ToString(), subSend);
     std::string strValue;
     leveldb::Status status = pdb->Get(readoptions, strKey, &strValue);
     if (status.ok()) {
         std::vector<std::string> vstr;
         boost::split(vstr, strValue, boost::is_any_of(":"), boost::token_compress_on);
         if (2 == vstr.size()) {
             propertyId = boost::lexical_cast<uint32_t>(vstr[0]);
             amount = boost::lexical_cast<int64_t>(vstr[1]);
             return true;
         }
     }
     return false;
}

/**
 * Records a "send all" sub record.
 */
void CMPTxList::recordSendAllSubRecord(const uint256& txid, int subRecordNumber, uint32_t propertyId, int64_t nValue)
{
     std::string strKey = strprintf("%s-%d", txid.ToString(), subRecordNumber);
     std::string strValue = strprintf("%d:%d", propertyId, nValue);

     leveldb::Status status = pdb->Put(writeoptions, strKey, strValue);
     ++nWritten;
    // if (msc_debug_txdb) PrintToLog("%s(): store: %s=%s, status: %s\n", __func__, strKey, strValue, status.ToString());
}

void CMPTxList::recordTX(const uint256 &txid, bool fValid, int nBlock, unsigned int type, uint64_t nValue)
{
    if (!pdb) return;

    // overwrite detection, we should never be overwriting a tx, as that means we have redone something a second time
    // reorgs delete all txs from levelDB above reorg_chain_height
    if (p_txlistdb->exists(txid)) PrintToLog("LEVELDB TX OVERWRITE DETECTION - %s\n", txid.ToString());

    const string key = txid.ToString();
    const string value = strprintf("%u:%d:%u:%lu", fValid ? 1:0, nBlock, type, nValue);
    Status status;

//   PrintToLog("%s(%s, valid=%s, block= %d, type= %d, value= %lu)\n",
//    __FUNCTION__, txid.ToString(), fValid ? "YES":"NO", nBlock, type, nValue);

    if (pdb)
    {
        status = pdb->Put(writeoptions, key, value);
        ++nWritten;
         if (msc_debug_txdb) PrintToLog("%s(): %s, line %d, file: %s\n", __FUNCTION__, status.ToString(), __LINE__, __FILE__);
    }
}

bool CMPTxList::exists(const uint256 &txid)
{
   if (!pdb) return false;

    string strValue;
    Status status = pdb->Get(readoptions, txid.ToString(), &strValue);

   if (!status.ok())
   {
     if (status.IsNotFound()) return false;
   }

   return true;
 }

bool CMPTxList::getTX(const uint256 &txid, string &value)
{
  Status status = pdb->Get(readoptions, txid.ToString(), &value);

  ++nRead;

  if (status.ok())
   {
     return true;
   }

   return false;
}

void CMPTxList::printStats()
{
  PrintToLog("CMPTxList stats: nWritten= %d , nRead= %d\n", nWritten, nRead);
}

 void CMPTxList::printAll()
{
   int count = 0;
   Slice skey, svalue;
   Iterator* it = NewIterator();

   for(it->SeekToFirst(); it->Valid(); it->Next())
   {
     skey = it->key();
     svalue = it->value();
     ++count;
     PrintToConsole("entry #%8d= %s:%s\n", count, skey.ToString(), svalue.ToString());
   }

   delete it;
 }

 void CMPTxList::recordPaymentTX(const uint256 &txid, bool fValid, int nBlock, unsigned int vout, unsigned int propertyId, uint64_t nValue, string buyer, string seller)
 {
   if (!pdb) return;

        // Prep - setup vars
        unsigned int type = 99999999;
        uint64_t numberOfPayments = 1;
        unsigned int paymentNumber = 1;
        uint64_t existingNumberOfPayments = 0;

        // Step 1 - Check TXList to see if this payment TXID exists
        bool paymentEntryExists = p_txlistdb->exists(txid);

        // Step 2a - If doesn't exist leave number of payments & paymentNumber set to 1
        // Step 2b - If does exist add +1 to existing number of payments and set this paymentNumber as new numberOfPayments
        if (paymentEntryExists)
        {
            //retrieve old numberOfPayments
            std::vector<std::string> vstr;
            string strValue;
            Status status = pdb->Get(readoptions, txid.ToString(), &strValue);
            if (status.ok())
            {
                // parse the string returned
                boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);

                // obtain the existing number of payments
                if (4 <= vstr.size())
                {
                    existingNumberOfPayments = atoi(vstr[3]);
                    paymentNumber = existingNumberOfPayments + 1;
                    numberOfPayments = existingNumberOfPayments + 1;
                }
            }
        }

        // Step 3 - Create new/update master record for payment tx in TXList
        const string key = txid.ToString();
        const string value = strprintf("%u:%d:%u:%lu", fValid ? 1:0, nBlock, type, numberOfPayments);
        Status status;
        PrintToLog("DEXPAYDEBUG : Writing master record %s(%s, valid=%s, block= %d, type= %d, number of payments= %lu)\n", __FUNCTION__, txid.ToString(), fValid ? "YES":"NO", nBlock, type, numberOfPayments);
        if (pdb)
        {
            status = pdb->Put(writeoptions, key, value);
            PrintToLog("DEXPAYDEBUG : %s(): %s, line %d, file: %s\n", __FUNCTION__, status.ToString(), __LINE__, __FILE__);
        }

        // Step 4 - Write sub-record with payment details
        const string txidStr = txid.ToString();
        const string subKey = STR_PAYMENT_SUBKEY_TXID_PAYMENT_COMBO(txidStr, paymentNumber);
        const string subValue = strprintf("%d:%s:%s:%d:%lu", vout, buyer, seller, propertyId, nValue);
        Status subStatus;
        PrintToLog("DEXPAYDEBUG : Writing sub-record %s with value %s\n", subKey, subValue);
        if (pdb)
        {
            subStatus = pdb->Put(writeoptions, subKey, subValue);
            PrintToLog("DEXPAYDEBUG : %s(): %s, line %d, file: %s\n", __FUNCTION__, subStatus.ToString(), __LINE__, __FILE__);
        }
 }

// figure out if there was at least 1 Master Protocol transaction within the block range, or a block if starting equals ending
// block numbers are inclusive
// pass in bDeleteFound = true to erase each entry found within the block range
 bool CMPTxList::isMPinBlockRange(int starting_block, int ending_block, bool bDeleteFound)
{
    leveldb::Slice skey, svalue;
    unsigned int count = 0;
    std::vector<std::string> vstr;
    int block;
    unsigned int n_found = 0;

    leveldb::Iterator* it = NewIterator();

    for(it->SeekToFirst(); it->Valid(); it->Next())
    {
        skey = it->key();
        svalue = it->value();

       ++count;

       string strvalue = it->value().ToString();

       // parse the string returned, find the validity flag/bit & other parameters
       boost::split(vstr, strvalue, boost::is_any_of(":"), token_compress_on);

       // only care about the block number/height here
       if (2 <= vstr.size())
       {
           block = atoi(vstr[1]);

           if ((starting_block <= block) && (block <= ending_block))
           {
               ++n_found;
               PrintToLog("%s() DELETING: %s=%s\n", __FUNCTION__, skey.ToString(), svalue.ToString());
               if (bDeleteFound) pdb->Delete(writeoptions, skey);
           }
        }
    }
    PrintToLog("%s(%d, %d); n_found= %d\n", __FUNCTION__, starting_block, ending_block, n_found);
    delete it;

    return (n_found);
 }

// global wrapper, block numbers are inclusive, if ending_block is 0 top of the chain will be used
bool mastercore::isMPinBlockRange(int starting_block, int ending_block, bool bDeleteFound)
 {
   if (!p_txlistdb) return false;

   if (0 == ending_block) ending_block = GetHeight(); // will scan 'til the end

   return p_txlistdb->isMPinBlockRange(starting_block, ending_block, bDeleteFound);
}

// call it like so (variable # of parameters):
// int block = 0;
// ...
// uint64_t nNew = 0;
//
// if (getValidMPTX(txid, &block, &type, &nNew)) // if true -- the TX is a valid MP TX

bool mastercore::getValidMPTX(const uint256 &txid, int *block, unsigned int *type, uint64_t *nAmended)
{
  string result;
  int validity = 0;

  if (msc_debug_txdb) PrintToLog("%s()\n", __FUNCTION__);

  if (!p_txlistdb) return false;

  if (!p_txlistdb->getTX(txid, result)) return false;

  // parse the string returned, find the validity flag/bit & other parameters
  std::vector<std::string> vstr;
  boost::split(vstr, result, boost::is_any_of(":"), token_compress_on);

  if (msc_debug_txdb) PrintToLog("%s() size=%lu : %s\n", __FUNCTION__, vstr.size(), result);

  if (1 <= vstr.size()) validity = atoi(vstr[0]);

  if (block)
    {
      if (2 <= vstr.size()) *block = atoi(vstr[1]);
      else *block = 0;
    }

  if (type)
    {
      if (3 <= vstr.size()) *type = atoi(vstr[2]);
      else *type = 0;
    }

  if (nAmended)
    {
      if (4 <= vstr.size()) *nAmended = boost::lexical_cast<boost::uint64_t>(vstr[3]);
      else nAmended = 0;
    }

  if (msc_debug_txdb) p_txlistdb->printStats();

  if ((int)0 == validity) return false;

  return true;
}

int mastercore_handler_block_begin(int nBlockPrev, CBlockIndex const * pBlockIndex)
{
  LOCK(cs_tally);

  if (reorgRecoveryMode > 0) {
    reorgRecoveryMode = 0; // clear reorgRecovery here as this is likely re-entrant

    // NOTE: The blockNum parameter is inclusive, so deleteAboveBlock(1000) will delete records in block 1000 and above.
    p_txlistdb->isMPinBlockRange(pBlockIndex->nHeight, reorgRecoveryMaxHeight, true);
    reorgRecoveryMaxHeight = 0;

    nWaterlineBlock = ConsensusParams().GENESIS_BLOCK - 1;

    int best_state_block = load_most_relevant_state();
    if (best_state_block < 0) {
      // unable to recover easily, remove stale stale state bits and reparse from the beginning.
      clear_all_state();
    } else {
      nWaterlineBlock = best_state_block;
    }

    // clear the global wallet property list, perform a forced wallet update and tell the UI that state is no longer valid, and UI views need to be reinit
    global_wallet_property_list.clear();
    CheckWalletUpdate(true);
    //uiInterface.OmniStateInvalidated();

    if (nWaterlineBlock < nBlockPrev) {
      // scan from the block after the best active block to catch up to the active chain
      msc_initial_scan(nWaterlineBlock + 1);
    }
  }

  // handle any features that go live with this block
  CheckLiveActivations(pBlockIndex->nHeight);
  addInterestPegged(nBlockPrev,pBlockIndex);
  eraseExpiredCrowdsale(pBlockIndex);
  _my_sps->rollingContractsBlock(pBlockIndex); // NOTE: we are checking every contract expiration
  return 0;
}

// called once per block, after the block has been processed
// TODO: consolidate into *handler_block_begin() << need to adjust Accept expiry check.............
// it performs cleanup and other functions
int mastercore_handler_block_end(int nBlockNow, CBlockIndex const * pBlockIndex,
        unsigned int countMP)
{
    LOCK(cs_tally);

    if (!mastercoreInitialized) {
        mastercore_init();
    }

    // check the alert status, do we need to do anything else here?
    CheckExpiredAlerts(nBlockNow, pBlockIndex->GetBlockTime());

     // transactions were found in the block, signal the UI accordingly
     if (countMP > 0) CheckWalletUpdate(true);

     // calculate and print a consensus hash if required
     if (msc_debug_consensus_hash_every_block) {
         uint256 consensusHash = GetConsensusHash();
         PrintToLog("Consensus hash for block %d: %s\n", nBlockNow, consensusHash.GetHex());
     }

     // request checkpoint verification
     bool checkpointValid = VerifyCheckpoint(nBlockNow, pBlockIndex->GetBlockHash());
     if (!checkpointValid) {
         // failed checkpoint, can't be trusted to provide valid data - shutdown client
         const std::string& msg = strprintf("Shutting down due to failed checkpoint for block %d (hash %s)\n", nBlockNow, pBlockIndex->GetBlockHash().GetHex());
         PrintToLog(msg);
  //     if (!GetBoolArg("-overrideforcedshutdown", false)) AbortNode(msg, msg);
  // TODO: fix AbortNode
     } else {
         // save out the state after this block
         if (writePersistence(nBlockNow)) {
             mastercore_save_state(pBlockIndex);
         }
     }

    return 0;
}

int mastercore_handler_disc_begin(int nBlockNow, CBlockIndex const * pBlockIndex)
{
    LOCK(cs_tally);

    reorgRecoveryMode = 1;
    reorgRecoveryMaxHeight = (pBlockIndex->nHeight > reorgRecoveryMaxHeight) ? pBlockIndex->nHeight: reorgRecoveryMaxHeight;
    return 0;
}

int mastercore_handler_disc_end(int nBlockNow, CBlockIndex const * pBlockIndex)
{
    LOCK(cs_tally);

    return 0;
}
/* New things for contracts *///////////////////////////////////////////////////

// obtains a vector of txids where the supplied address participated in a trade (needed for gettradehistory_MP)
// optional property ID parameter will filter on propertyId transacted if supplied
// sorted by block then index
void CMPTradeList::getTradesForAddress(std::string address, std::vector<uint256>& vecTransactions, uint32_t propertyIdFilter)
{
  if (!pdb) return;
  std::map<std::string,uint256> mapTrades;
  leveldb::Iterator* it = NewIterator();
  for(it->SeekToFirst(); it->Valid(); it->Next()) {
      std::string strKey = it->key().ToString();
      std::string strValue = it->value().ToString();
      std::vector<std::string> vecValues;
      if (strKey.size() != 64) continue; // only interested in trades
      uint256 txid = uint256S(strKey);
      size_t addressMatch = strValue.find(address);
      if (addressMatch == std::string::npos) continue;
      boost::split(vecValues, strValue, boost::is_any_of(":"), token_compress_on);
      if (vecValues.size() != 5) {
          PrintToLog("TRADEDB error - unexpected number of tokens in value (%s)\n", strValue);
          continue;
      }
      uint32_t propertyIdForSale = boost::lexical_cast<uint32_t>(vecValues[1]);
      uint32_t propertyIdDesired = boost::lexical_cast<uint32_t>(vecValues[2]);
      int64_t blockNum = boost::lexical_cast<uint32_t>(vecValues[3]);
      int64_t txIndex = boost::lexical_cast<uint32_t>(vecValues[4]);
      if (propertyIdFilter != 0 && propertyIdFilter != propertyIdForSale && propertyIdFilter != propertyIdDesired) continue;
      std::string sortKey = strprintf("%06d%010d", blockNum, txIndex);
      mapTrades.insert(std::make_pair(sortKey, txid));
  }
  delete it;
  for (std::map<std::string,uint256>::iterator it = mapTrades.begin(); it != mapTrades.end(); it++) {
      vecTransactions.push_back(it->second);
  }
}

void CMPTradeList::recordNewTrade(const uint256& txid, const std::string& address, uint32_t propertyIdForSale, uint32_t propertyIdDesired, int blockNum, int blockIndex)
{
  if (!pdb) return;
  std::string strValue = strprintf("%s:%d:%d:%d:%d", address, propertyIdForSale, propertyIdDesired, blockNum, blockIndex);
  Status status = pdb->Put(writeoptions, txid.ToString(), strValue);
  ++nWritten;
  // if (msc_debug_tradedb) PrintToLog("%s(): %s\n", __FUNCTION__, status.ToString());
}

void CMPTradeList::recordNewTrade(const uint256& txid, const std::string& address, uint32_t propertyIdForSale, uint32_t propertyIdDesired, int blockNum, int blockIndex, int64_t reserva)
{
  if (!pdb) return;
  std::string strValue = strprintf("%s:%d:%d:%d:%d:%d", address, propertyIdForSale, propertyIdDesired, blockNum, blockIndex,reserva);
  Status status = pdb->Put(writeoptions, txid.ToString(), strValue);
  ++nWritten;
  // if (msc_debug_tradedb) PrintToLog("%s(): %s\n", __FUNCTION__, status.ToString());
}

void CMPTradeList::recordMatchedTrade(const uint256 txid1, const uint256 txid2, string address1, string address2, unsigned int prop1, unsigned int prop2, uint64_t amount1, uint64_t amount2, int blockNum, int64_t fee)
{
    if (!pdb) return;
    const string key = txid1.ToString() + "+" + txid2.ToString();
    const string value = strprintf("%s:%s:%u:%u:%lu:%lu:%d:%d", address1, address2, prop1, prop2, amount1, amount2, blockNum, fee);

    Status status;
    if (pdb)
    {
        status = pdb->Put(writeoptions, key, value);
        ++nWritten;
        // if (msc_debug_tradedb) PrintToLog("%s(): %s\n", __FUNCTION__, status.ToString());
    }
}
/********************************************************/
/** New things for Contract */
void CMPTradeList::recordMatchedTrade(const uint256 txid1, const uint256 txid2, string address1, string address2, uint64_t effective_price, uint64_t amount_maker, uint64_t amount_taker, int blockNum1, int blockNum2, uint32_t property_traded, string tradeStatus, int64_t lives_s0, int64_t lives_s1, int64_t lives_s2, int64_t lives_s3, int64_t lives_b0, int64_t lives_b1, int64_t lives_b2, int64_t lives_b3, string s_maker0, string s_taker0, string s_maker1, string s_taker1, string s_maker2, string s_taker2, string s_maker3, string s_taker3, int64_t nCouldBuy0, int64_t nCouldBuy1, int64_t nCouldBuy2, int64_t nCouldBuy3)
{
  if (!pdb) return;

  extern volatile int idx_q;
  extern volatile int path_length;
  std::map<std::string, std::string> edgeEle;
  bool savedata_bool = false;

  double UPNL1 = 0, UPNL2 = 0;
  /********************************************************************/
  const string key = txid1.ToString() + "+" + txid2.ToString();
  const string value = strprintf("%s:%s:%lu:%lu:%lu:%d:%d:%s:%s:%d:%d:%d", address1, address2, effective_price, amount_maker, amount_taker, blockNum1, blockNum2, s_maker0, s_taker0, lives_s0, lives_b0, property_traded);

  const string line0 = gettingLineOut(address1, s_maker0, lives_s0, address2, s_taker0, lives_b0, nCouldBuy0, effective_price);
  const string line1 = gettingLineOut(address1, s_maker1, lives_s1, address2, s_taker1, lives_b1, nCouldBuy1, effective_price);
  const string line2 = gettingLineOut(address1, s_maker2, lives_s2, address2, s_taker2, lives_b2, nCouldBuy2, effective_price);
  const string line3 = gettingLineOut(address1, s_maker3, lives_s3, address2, s_taker3, lives_b3, nCouldBuy3, effective_price);

  bool status_bool1 = s_maker0 == "OpenShortPosByLongPosNetted" || s_maker0 == "OpenLongPosByShortPosNetted";
  bool status_bool2 = s_taker0 == "OpenShortPosByLongPosNetted" || s_taker0 == "OpenLongPosByShortPosNetted";

  std::fstream fileSixth;
  fileSixth.open ("graphInfoSixth.txt", std::fstream::in | std::fstream::out | std::fstream::app);
  if ( status_bool1 || status_bool2 )
    {
      if ( s_maker3 == "EmptyStr" && s_taker3 == "EmptyStr" ) savedata_bool = true;
      saveDataGraphs(fileSixth, line1, line2, line3, savedata_bool);
    }
  else saveDataGraphs(fileSixth, line0);
  fileSixth.close();
  /********************************************************************/
  if ( status_bool1 || status_bool2 )
    {
      buildingEdge(edgeEle, address1, address2, s_maker1, s_taker1, lives_s1, lives_b1, nCouldBuy1, effective_price, idx_q, 0);
      path_ele.push_back(edgeEle);
      buildingEdge(edgeEle, address1, address2, s_maker2, s_taker2, lives_s2, lives_b2, nCouldBuy2, effective_price, idx_q, 0);
      path_ele.push_back(edgeEle);
      PrintToLog("Line 1: %s\n", line1);
      PrintToLog("Line 2: %s\n", line2);
      if ( s_maker3 != "EmptyStr" && s_taker3 != "EmptyStr" )
	{
	  buildingEdge(edgeEle, address1, address2, s_maker3, s_taker3, lives_s3, lives_b3,nCouldBuy3,effective_price,idx_q,0);
	  path_ele.push_back(edgeEle);
	  PrintToLog("Line 3: %s\n", line3);
	}
    }
  else
    {
      buildingEdge(edgeEle, address1, address2, s_maker0, s_taker0, lives_s0, lives_b0, nCouldBuy0, effective_price, idx_q, 0);
      path_ele.push_back(edgeEle);
      PrintToLog("Line 0: %s\n", line0);
    }

  loopForEntryPrice(path_ele, path_length, address1, address2, UPNL1, UPNL2, effective_price);
  path_length = path_ele.size();
  PrintToLog("UPNL1 = %d, UPNL2 = %d\n", UPNL1, UPNL2);

  unsigned int contractId = static_cast<unsigned int>(property_traded);

  if ( contractId == MSC_PROPERTY_TYPE_CONTRACT )
    {
      globalPNLALL_DUSD += UPNL1 + UPNL2;
      globalVolumeALL_DUSD += nCouldBuy0;
    }

  int64_t volumeToCompare = 0;
  bool perpetualBool = callingPerpetualSettlement(globalPNLALL_DUSD, globalVolumeALL_DUSD, volumeToCompare);
  if (perpetualBool) PrintToLog("Perpetual Settlement Online");

  PrintToLog("\nglobalPNLALL_DUSD = %d, globalVolumeALL_DUSD = %d, contractId = %d\n", globalPNLALL_DUSD, globalVolumeALL_DUSD, contractId);

  std::fstream fileglobalPNLALL_DUSD;
  fileglobalPNLALL_DUSD.open ("globalPNLALL_DUSD.txt", std::fstream::in | std::fstream::out | std::fstream::app);
  if ( contractId == MSC_PROPERTY_TYPE_CONTRACT )
    saveDataGraphs(fileglobalPNLALL_DUSD, std::to_string(globalPNLALL_DUSD));
  fileglobalPNLALL_DUSD.close();

  std::fstream fileglobalVolumeALL_DUSD;
  fileglobalVolumeALL_DUSD.open ("globalVolumeALL_DUSD.txt", std::fstream::in | std::fstream::out | std::fstream::app);
  if ( contractId == MSC_PROPERTY_TYPE_CONTRACT )
    saveDataGraphs(fileglobalVolumeALL_DUSD, std::to_string(FormatShortIntegerMP(globalVolumeALL_DUSD)));
  fileglobalVolumeALL_DUSD.close();
  
  // adding the upnl
  double uPNL1 = static_cast<double>(factorE * UPNL1);
  double uPNL2 = static_cast<double>(factorE * UPNL2);
  int64_t iUPNL1 = static_cast<int64_t>(uPNL1);
  int64_t iUPNL2 = static_cast<int64_t>(uPNL2);

  PrintToLog("UPNL1 to tally : %d ------------------------------------\n",iUPNL1);
  PrintToLog("UPNL2 to tally : %d ------------------------------------\n",iUPNL2);

  if (iUPNL1 > 0 ) {
      update_tally_map(address1, property_traded , iUPNL1, UPNL);
  } else if (iUPNL1 < 0 ) {
      update_tally_map(address1, property_traded , -iUPNL1, NUPNL);
  }

  if (iUPNL2 > 0 ) {
      update_tally_map(address2, property_traded , iUPNL2, UPNL);
  } else if (iUPNL2 < 0 ) {
      update_tally_map(address2, property_traded , -iUPNL2, NUPNL);
  }

  Status status;
  if (pdb)
    {
      status = pdb->Put(writeoptions, key, value);
      ++nWritten;
    }
  PrintToLog("\n\nEnd of recordMatchedTrade <------------------------------\n");
}

bool callingPerpetualSettlement(double globalPNLALL_DUSD, int64_t globalVolumeALL_DUSD, int64_t volumeToCompare)
{
  bool perpetualBool = false;

  if ( globalPNLALL_DUSD == 0 )
    {
      PrintToLog("Liquidate Forward Positions");
      perpetualBool = true;
    }
  else if ( globalVolumeALL_DUSD > volumeToCompare )
    PrintToLog("Take decisions for globalVolumeALL_DUSD > volumeToCompare");

  return perpetualBool;
}

void fillingMatrix(MatrixTLS &M_file, MatrixTLS &ndatabase, std::vector<std::map<std::string, std::string>> &path_ele)
{
  for (unsigned int i = 0; i < path_ele.size(); ++i)
    {
      M_file[i][0] = ndatabase[i][0] = path_ele[i]["addrs_src"];
      M_file[i][1] = ndatabase[i][1] = path_ele[i]["status_src"];
      M_file[i][2] = ndatabase[i][2] = path_ele[i]["lives_src"];
      M_file[i][3] = ndatabase[i][3] = path_ele[i]["addrs_trk"];
      M_file[i][4] = ndatabase[i][4] = path_ele[i]["status_trk"];
      M_file[i][5] = ndatabase[i][5] = path_ele[i]["lives_trk"];
      M_file[i][6] = ndatabase[i][6] = path_ele[i]["amount_trd"];
      M_file[i][7] = ndatabase[i][7] = path_ele[i]["matched_price"];
      M_file[i][8] = ndatabase[i][8] = path_ele[i]["amount_trd"];
      M_file[i][9] = ndatabase[i][9] = path_ele[i]["amount_trd"];
    }
}

double PNL_function(uint64_t entry_price, uint64_t exit_price, int64_t amount_trd, std::string netted_status)
{
  double PNL = 0;

  if ( finding_string("Long", netted_status) )
    PNL = (double)amount_trd*(1/(double)entry_price-1/(double)exit_price);
  else if ( finding_string("Short", netted_status) )
    PNL = (double)amount_trd*(1/(double)exit_price-1/(double)entry_price);

  return PNL;
}

void loopForEntryPrice(std::vector<std::map<std::string, std::string>> path_ele, unsigned int path_length, std::string address1, std::string address2, double &UPNL1, double &UPNL2, uint64_t exit_price)
{
  std::vector<std::map<std::string, std::string>>::iterator it_path_ele;
  std::vector<std::map<std::string, std::string>>::reverse_iterator reit_path_ele;
  std::string addrs_srch;
  std::string addrs_trkh;

  int idx_path = 0;
  unsigned int limInf = 0;
  uint64_t entry_pricesrc = 0, entry_pricetrk = 0;
  double UPNLSRC, UPNLTRK;
  double UPNLSRC_sum = 0, UPNLTRK_sum = 0;

  for (it_path_ele = path_ele.begin()+path_length; it_path_ele != path_ele.end(); ++it_path_ele)
    {
      idx_path += 1;
      if ( finding_string("Netted", (*it_path_ele)["status_src"]) )
      	{
      	  addrs_srch = (*it_path_ele)["addrs_src"];
	  limInf = path_ele.size()-(path_length+idx_path)+1;

      	  for (reit_path_ele = path_ele.rbegin()+limInf; reit_path_ele != path_ele.rend(); ++reit_path_ele)
      	    {
      	      if ( addrs_srch == (*reit_path_ele)["addrs_src"] && finding_string("Open", (*reit_path_ele)["status_src"]) )
      	      	{
		  entry_pricesrc = static_cast<uint64_t>(stol((*reit_path_ele)["matched_price"]));
		  break;
		}
      	      else if ( addrs_srch == (*reit_path_ele)["addrs_trk"] && finding_string("Open", (*reit_path_ele)["status_trk"]) )
      	      	{
		  entry_pricesrc = static_cast<uint64_t>(stol((*reit_path_ele)["matched_price"]));
		  break;
      	      	}
      	    }
	  UPNLSRC = PNL_function(entry_pricesrc, exit_price, static_cast<uint64_t>(stol((*it_path_ele)["amount_trd"])), (*it_path_ele)["status_src"]);
	  UPNLSRC_sum += UPNLSRC;
      	}
      if ( finding_string("Netted", (*it_path_ele)["status_trk"]) )
      	{
      	  addrs_trkh = (*it_path_ele)["addrs_trk"];
	  limInf = path_ele.size()-(path_length+idx_path)+1;
	  PrintToConsole("Exit Price: %d\n", (*it_path_ele)["matched_price"]);

	  for (reit_path_ele = path_ele.rbegin()+limInf; reit_path_ele != path_ele.rend(); ++reit_path_ele)
      	    {
      	      if ( addrs_trkh == (*reit_path_ele)["addrs_src"] && finding_string("Open", (*reit_path_ele)["status_src"]) )
      	      	{
		  entry_pricetrk = static_cast<uint64_t>(stol((*reit_path_ele)["matched_price"]));
		  break;
		}
	      else if ( addrs_trkh == (*reit_path_ele)["addrs_trk"] && finding_string("Open", (*reit_path_ele)["status_trk"]) )
		{
		  entry_pricetrk = static_cast<uint64_t>(stol((*reit_path_ele)["matched_price"]));
		  break;
	      	}
      	    }
	  UPNLTRK = PNL_function(entry_pricetrk, exit_price, static_cast<uint64_t>(stol((*it_path_ele)["amount_trd"])), (*it_path_ele)["status_trk"]);
	  UPNLTRK_sum += UPNLTRK;
      	}
    }
  UPNL1 = UPNLSRC_sum;
  UPNL2 = UPNLTRK_sum;
}

const string gettingLineOut(std::string address1, std::string s_status1, int64_t lives_maker, std::string address2, std::string s_status2, int64_t lives_taker, int64_t nCouldBuy, uint64_t effective_price)
{
  const string lineOut = strprintf("%s\t %s\t %d\t %s\t %s\t %d\t %d\t %d",
				   address1, s_status1, FormatContractShortMP(lives_maker),
				   address2, s_status2, FormatContractShortMP(lives_taker),
				   FormatContractShortMP(nCouldBuy), FormatContractShortMP(effective_price));
  return lineOut;
}

void buildingEdge(std::map<std::string, std::string> &edgeEle, std::string addrs_src, std::string addrs_trk, std::string status_src, std::string status_trk, int64_t lives_src, int64_t lives_trk, int64_t amount_path, int64_t matched_price, int idx_q, int ghost_edge)
{
  edgeEle["addrs_src"]     = addrs_src;
  edgeEle["addrs_trk"]     = addrs_trk;
  edgeEle["status_src"]    = status_src;
  edgeEle["status_trk"]    = status_trk;
  edgeEle["lives_src"]     = std::to_string(FormatShortIntegerMP(lives_src));
  edgeEle["lives_trk"]     = std::to_string(FormatShortIntegerMP(lives_trk));
  edgeEle["amount_trd"]    = std::to_string(FormatShortIntegerMP(amount_path));
  edgeEle["matched_price"] = std::to_string(FormatContractShortMP(matched_price));
  edgeEle["edge_row"]      = std::to_string(idx_q);
  edgeEle["ghost_edge"]    = std::to_string(ghost_edge);
}

void printing_edges_database(std::map<std::string, std::string> &path_ele)
{
  cout <<"\n{ "<<"addrs_src : "<<path_ele["addrs_src"]<<", status_src : "<<path_ele["status_src"]<<", lives_src : "<<path_ele["lives_src"]<<", addrs_trk : "<<path_ele["addrs_trk"]<<", status_trk : "<<path_ele["status_trk"]<<", lives_trk : "<<path_ele["lives_trk"]<<", amount_trd : "<<path_ele["amount_trd"]<<", matched_price : "<<path_ele["matched_price"]<<", edge_row : "<<path_ele["edge_row"]<<", ghost_edge : "<<path_ele["ghost_edge"]<<" }\n";
}

/********************************************************/
/** New things for contracts */
bool CMPTradeList::getMatchingTrades(uint32_t propertyId, UniValue& tradeArray)
{
  if (!pdb) return false;

  int count = 0;

  std::vector<std::string> vstr;
  // string txidStr = txid.ToString();

  leveldb::Iterator* it = NewIterator(); // Allocation proccess

  for(it->SeekToFirst(); it->Valid(); it->Next()) {
      // search key to see if this is a matching trade
      std::string strKey = it->key().ToString();
      std::string strValue = it->value().ToString();

      // ensure correct amount of tokens in value string
      boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);
      if (vstr.size() != 17) {
          PrintToLog("TRADEDB error - unexpected number of tokens in value (%s)\n", strValue);
          // PrintToConsole("TRADEDB error - unexpected number of tokens in value %d \n",vstr.size());
          continue;
      }

     // decode the details from the value string
      uint32_t prop1 = boost::lexical_cast<uint32_t>(vstr[11]);
      std::string address1 = vstr[0];
      std::string address2 = vstr[1];
      int64_t amountTraded = boost::lexical_cast<int64_t>(vstr[2]);
      int block = boost::lexical_cast<int>(vstr[6]);
      int64_t price = boost::lexical_cast<int64_t>(vstr[12]);
      int64_t takerAction = boost::lexical_cast<int64_t>(vstr[13]);
      std::string txidmaker = vstr[14];
      std::string txidtaker = vstr[15];
      int idx = boost::lexical_cast<int>(vstr[16]);
      PrintToConsole("amount traded: %d\n",amountTraded);
      PrintToConsole("price: %d\n",price);
      PrintToConsole("contract id: %d\n",prop1);
      PrintToConsole("taker action: %d\n",takerAction);
      PrintToConsole("maker txid: %d\n",txidmaker);
      PrintToConsole("maker txid: %d\n",txidmaker);
      PrintToConsole("block: %d\n",block);
      PrintToConsole("block index: %d\n",idx);

      // populate trade object and add to the trade array, correcting for orientation of trade

      if (prop1 != propertyId) {
         continue;
      }

      UniValue trade(UniValue::VOBJ);

      trade.push_back(Pair("maker_address", address1));
      trade.push_back(Pair("maker_txid", txidmaker));
      trade.push_back(Pair("taker_address", address2));
      trade.push_back(Pair("taker_txid", txidtaker));
      trade.push_back(Pair("amount_traded", FormatByType(amountTraded,1)));
      trade.push_back(Pair("price", FormatByType(price,2)));
      trade.push_back(Pair("taker_action",takerAction));
      trade.push_back(Pair("taker_block",block));
      trade.push_back(Pair("taker_index_block",idx));
      tradeArray.push_back(trade);
      ++count;
      PrintToConsole("count : %d\n",count);
  }
  // clean up
  delete it; // Desallocation proccess
  if (count) { return true; } else { return false; }
}

bool CMPTradeList::getCreatedPegged(uint32_t propertyId, UniValue& tradeArray)
{
  if (!pdb) return false;

  int count = 0;

  std::vector<std::string> vstr;
  // string txidStr = txid.ToString();

  leveldb::Iterator* it = NewIterator(); // Allocation proccess

  for(it->SeekToFirst(); it->Valid(); it->Next()) {
      // search key to see if this is a matching trade
      std::string strKey = it->key().ToString();
      std::string strValue = it->value().ToString();

      // ensure correct amount of tokens in value string
      boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);
      if (vstr.size() != 5) {
          PrintToLog("TRADEDB error - unexpected number of tokens in value (%s)\n", strValue);
          continue;
      }

     // decode the details from the value string

      uint32_t propId = boost::lexical_cast<uint32_t>(vstr[1]);

      if (propId != propertyId) {
         continue;
      }
      std::string address = vstr[0];
      int64_t height = boost::lexical_cast<int64_t>(vstr[2]);
      int64_t amount = (boost::lexical_cast<int64_t>(vstr[3]) / factorE);
      std::string series = vstr[4];
      PrintToConsole("address who create: %s\n",address);
      PrintToConsole("height: %d\n",height);
      PrintToConsole("amount created: %d\n",amount);
      PrintToConsole("property id: %d\n",propId);
      PrintToConsole("series: %s\n",series);
      // populate trade object and add to the trade array, correcting for orientation of trade


      UniValue trade(UniValue::VOBJ);

      trade.push_back(Pair("creator address", address));
      trade.push_back(Pair("height", height));
      trade.push_back(Pair("txid", strKey));
      trade.push_back(Pair("amount created", amount));
      trade.push_back(Pair("series", series));
      tradeArray.push_back(trade);
      ++count;
      PrintToConsole("count : %d\n",count);
  }
  // clean up
  delete it; // Desallocation proccess
  if (count) { return true; } else { return false; }
}


/* TODO: use this for redeem the ALLs tokens
int64_t CMPTradeList::getTradeAllsByTxId(uint256& txid)  // function that return the initial margin (ALLs) for a trade txid
{
  if (!pdb) return false;

  int count = 0;
  int64_t alls = 0;
  std::string txidStr = txid.ToString();
  PrintToConsole("txid from argument : %s \n",txidStr);
  std::vector<std::string> vstr;

  leveldb::Iterator* it = NewIterator(); // Allocation proccess

  for(it->SeekToFirst(); it->Valid(); it->Next()) {
      // search key to see if this is a matching trade
      std::string strKey = it->key().ToString();
      std::string strValue = it->value().ToString();

      // ensure correct amount of tokens in value string
      boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);
      if (vstr.size() != 6) {
          PrintToLog("TRADEDB error - unexpected number of tokens in value (%s)\n", strValue);
          continue;
      }

      if (strKey != txidStr){
          continue;
      }

     // decode the details from the value string
      alls = boost::lexical_cast<int64_t>(vstr[5]);
      PrintToLog("txid from db: %s\n",strKey);
      PrintToLog("alls for margin: %s\n",alls);
      PrintToLog("count : %d\n",count);
      ++count;
  }
  // clean up
  delete it; // Desallocation proccess
  return alls;
}*/

void CMPTradeList::recordForUPNL(const uint256 txid, string address, uint32_t property_traded, int64_t effectivePrice)
{

  if (!pdb) return;
  PrintToConsole("address : %s\n",address);
  PrintToConsole("contract traded: %d\n",property_traded);
  PrintToConsole("effective price: %d\n",effectivePrice);
  arith_uint256 effPr = mastercore::ConvertTo256(effectivePrice) / mastercore::ConvertTo256(factorE);
  const string key = txid.ToString();
  const string value = strprintf("%s:%d:%d", address , property_traded, mastercore::ConvertTo64(effPr));

  Status status;
  if (pdb)
  {
      status = pdb->Put(writeoptions, key, value);
      ++nWritten;
      // if (msc_debug_tradedb) PrintToLog("%s(): %s\n", __FUNCTION__, status.ToString());
  }
  PrintToLog("Exit the recordForUPNL function \n");

}

double CMPTradeList::getPNL(string address, int64_t contractsClosed, int64_t price, uint32_t property, uint32_t marginRequirementContract, uint32_t notionalSize, std::string Status)
{
    PrintToConsole("________________________________________\n");
    PrintToConsole("Inside getPNL!!!\n");
    PrintToConsole("Checking Extern Volatil Variable coming from x_Trade\n");
    // extern volatile uint64_t marketPrice;
    // PrintToConsole("Market Price in Omnicore: %d\n", marketPrice);

    int count = 0;
    int64_t totalAmount = 0;

    int64_t totalContracts = 0;
    int64_t totalAux = 0;
    int64_t pCouldBuy = 0;

    int64_t d_contractsClosed = static_cast<int64_t>(contractsClosed/factorE);
    int64_t aux = getMPbalance(address, property, REMAINING);
    std::vector<std::string> vstr;

    leveldb::Iterator* it = NewIterator();
    for(it->SeekToFirst(); it->Valid(); it->Next()) {

        std::string strKey = it->key().ToString();
        std::string strValue = it->value().ToString();

        boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);

        if (vstr.size() != 17) {

            PrintToLog("TRADEDB error - unexpected number of tokens in value (%s)\n", strValue);
            // PrintToConsole("TRADEDB error - unexpected number of tokens in value : (%s) \n",strValue);
            continue;
        }
        if (address != vstr[0] && address != vstr[1]) continue;
        // Decode the details from the value string
        std::string address1 = vstr[0];
        std::string address2 = vstr[1];
        int64_t effectivePrice = (boost::lexical_cast<int64_t>(vstr[12]))/factorE;
        int64_t nCouldBuy = (boost::lexical_cast<int64_t>(vstr[2]))/factorE;

        PrintToLog("aux: %d\n",aux);
        PrintToLog("nCouldBuy: %d\n",nCouldBuy);
        PrintToLog("EffectivePrice: %d\n",effectivePrice);
        PrintToLog("Contracts closed: %d\n",d_contractsClosed);

        // making some calculations needed for PNL
        if(aux > totalAux){
          if (nCouldBuy > aux - totalAux){
              pCouldBuy = nCouldBuy - (aux - totalAux);
              totalAux = totalAux + aux - totalAux;
          }else {
              totalAux = totalAux + nCouldBuy;
              continue;
          }
        }

        if(d_contractsClosed > totalContracts){
            if (pCouldBuy > 0){
                nCouldBuy = pCouldBuy;
            }
            if (nCouldBuy > d_contractsClosed - totalContracts){
                int64_t diff = d_contractsClosed - totalContracts;
                totalAmount += effectivePrice*diff;
                totalContracts += diff;
            } else {
                totalAmount += effectivePrice*nCouldBuy;
                totalContracts += nCouldBuy;
            }

            PrintToConsole("totalAmount: %d\n",totalAmount);
            PrintToConsole("totalContracts: %d\n",totalContracts);
            pCouldBuy = 0;
         } else {
            if ( totalContracts > 0 ) {
                //assert(update_tally_map(address, property, totalContracts, REMAINING));
            }
            break;
        }
        ++count;
    }

    // // clean up
    delete it;
    // double averagePrice = static_cast<double>(totalAmount/d_contractsClosed);
    // double PNL_num = static_cast<double>((d_price - averagePrice)*(notionalSize*d_contractsClosed));
    // double PNL_den = static_cast<double>(averagePrice*marginRequirementContract);
    double PNL = 1;

    // if ((Status == "Long Netted") || (Status == "Netted_L")){
    //    PNL = static_cast<double>(PNL_num/PNL_den);
    //    PrintToConsole("Long Side\n");
    // } else if ((Status == "Short Netted") || (Status == "Netted_S")){
    //    PNL = static_cast<double>(-PNL_num/PNL_den);
    //    PrintToConsole("Short Side\n");
    // }
    //
    // PrintToConsole("Total Amount : %d\n",totalAmount);
    // PrintToConsole("Contracts Remaining: %d\n",totalContracts);
    // PrintToConsole("Average Price Pi: %d\n",averagePrice);
    // PrintToConsole("marginRequirementContract : %d\n",marginRequirementContract);
    // PrintToConsole("notionalSize : %d\n",notionalSize);
    // PrintToConsole("contractsClosed : %d\n",d_contractsClosed);
    // PrintToConsole("PNL_num : %d\n",PNL_num);
    // PrintToConsole("PNL_den : %d\n",PNL_den);
    // PrintToConsole("________________________________________\n");
    return PNL;
}

double CMPTradeList::getUPNL(string address, uint32_t contractId)
{
   // int count = 0;
   // int64_t sumPrice = 0;
   // const double factor = 100000000;
   // int index = static_cast<int>(contractId);
   // double marketPrice = static_cast<double>((marketP [index])/factor);
   // CMPSPInfo::Entry sp;
   // _my_sps->getSP(contractId, sp);
   // uint32_t notionalSize = sp.notional_size;
   // uint32_t marginRequirement = sp.margin_requirement;
   // PrintToConsole("------------------------------------------------------------\n");
   // PrintToConsole("Inside getUPNL\n");
   // PrintToConsole("marketPrice for this contract : %d\n",marketPrice);
   // PrintToConsole("notionalSize : %d\n",notionalSize);
   // PrintToConsole("marginRequirement : %d\n",marginRequirement);
   // std::vector<std::string> vstr;
   // leveldb::Iterator* it = NewIterator();
   // for(it->SeekToFirst(); it->Valid(); it->Next()) {
   //
   //     std::string strKey = it->key().ToString();
   //     std::string strValue = it->value().ToString();
   //
   //      boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);

   //     if (vstr.size() != 3) {

   //         PrintToLog("TRADEDB error - unexpected number of tokens in value (%s)\n", strValue);
            // PrintToConsole("TRADEDB error - unexpected number of tokens in value : (%s) \n",strValue);
   //         continue;
   //     }
   //     if (address != vstr[0]) continue;
        // Decode the details from the value string
   //     std::string address1 = vstr[0];
   //     uint32_t property_traded = boost::lexical_cast<uint32_t>(vstr[1]);
   //     int64_t effectivePrice = boost::lexical_cast<int64_t>(vstr[2]);
   //     sumPrice += effectivePrice;
   //     PrintToConsole("address1: %s\n",address1);
   //     PrintToConsole("property_traded: %d\n",property_traded);
   //     PrintToConsole("EffectivePrice: %d\n",effectivePrice);
   //     ++count;
   //   }
    // // clean up
   //  delete it;
   //  double averagePrice = 0;
   //  if(count > 0){
   //     averagePrice = static_cast<double>(sumPrice/count);
   //  }
   //  PrintToConsole("averagePrice: %d\n",averagePrice);
   //  PrintToConsole("sumPrice: %d\n",sumPrice);
   //  PrintToConsole("final count: %d\n",count);

   //  int64_t longPosition = getMPbalance(address, contractId, POSSITIVE_BALANCE);
   //  int64_t shortPosition = getMPbalance(address, contractId, NEGATIVE_BALANCE);
   //  double PNL_num = 0;
     double PNL = 0;
   //  if (longPosition > 0 && shortPosition == 0) {
   //     PNL_num = static_cast<double>((marketPrice - averagePrice)*(notionalSize*longPosition));
   //  } else if (longPosition == 0 && shortPosition > 0) {
   //     PNL_num = static_cast<double>((averagePrice - marketPrice)*(notionalSize*shortPosition));
   //  }

   // double PNL_den = static_cast<double>(averagePrice*marginRequirement);
    //if (PNL_den != 0){
    //   PNL = static_cast<double>(PNL_num/PNL_den);
    //}
   // PrintToConsole("shortPosition : %d\n",shortPosition);
   // PrintToConsole("longPosition : %d\n",longPosition);
   // PrintToConsole("PNL_num : %d\n",PNL_num);
   // PrintToConsole("PNL_den : %d\n",PNL_den);
   // PrintToConsole("PNL : %d\n",PNL);
   // PrintToConsole("Exit the getUPNL function <--------------------------\n");
    return PNL;

}
////////////////////////////////////////
/** New things for Contracts */
void CMPTradeList::marginLogic(uint32_t property) // Vector of matching address for a given contract traded
{
    PrintToConsole("___________________________________________________________\n");
    PrintToConsole("Looking for Address in marginLogic function\n");

    std::vector<std::string> vstr;
    std::vector<std::string> addr;   // Address vector

    extern volatile uint64_t marketPrice;
    leveldb::Iterator* it = NewIterator();

    for(it->SeekToFirst(); it->Valid(); it->Next()) {

        std::string strKey = it->key().ToString();
        std::string strValue = it->value().ToString();

        boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);

        if (vstr.size() != 17) {

            PrintToLog("TRADEDB error - unexpected size of vector (%s)\n", strValue);
            continue;
        }
        std::string address1 = vstr[0];
        std::string address2 = vstr[1];

        if (std::find(addr.begin(), addr.end(), address1) == addr.end() ) {
            addr.push_back(address1);
        }
        if ((std::find(addr.begin(), addr.end(), address2) == addr.end()) && (address1 != address2)) {
            addr.push_back(address2);
        }
      }

      for(std::vector<std::string>::iterator it = addr.begin() ; it != addr.end(); ++it){

        PrintToConsole("Into for loop watching addres on vector addr\n");
        PrintToConsole("Address in vector addr: %s\n", *it);

        int64_t liqPrice = getMPbalance(*it,property, LIQUIDATION_PRICE);
        int64_t shortBalance = getMPbalance(*it, property, NEGATIVE_BALANCE);
        int64_t longBalance = getMPbalance(*it, property, POSSITIVE_BALANCE);
        int trading_action = shortBalance > 0 ? BUY : ( longBalance > 0 ? SELL : ACTIONINVALID );
        // int64_t amountInOrder = trading_action == BUY ? shortBalance : longBalance;
        uint64_t nLiqPrice = static_cast<uint64_t> (liqPrice);

        PrintToConsole("shortBalance: %d\n", shortBalance);
        PrintToConsole("LiqPrice: %d\n", FormatContractShortMP(liqPrice));
        PrintToConsole("longBalance: %d\n", longBalance);
        PrintToConsole("trading action: %d\n", trading_action);
        PrintToConsole("marketPrice in marginLogic function : %d\n", FormatContractShortMP(marketPrice));
        PrintToConsole("String LiqPrice in marginLogic function: %d\n", FormatContractShortMP(nLiqPrice));

        if( (FormatContractShortMP(marketPrice) <= FormatContractShortMP(nLiqPrice)) && (liqPrice > 0) && (trading_action == SELL)){

           PrintToConsole("//////////////////////////////////////MARGIN CALL !!!!!!!!!!!\n");
           PrintToConsole("marketPrice <= LiqPrice\n");
           PrintToConsole("Liquidation price in marginLogic function: %s\n",nLiqPrice);
          //  int rc = marginCall(*it, property, marketPrice, trading_action, amountInOrder);
          //  assert(update_tally_map(*it, property,-liqPrice, LIQUIDATION_PRICE));
          //  PrintToConsole("Margin call number rc %d\n", rc);

        } else if( (FormatContractShortMP(marketPrice) >= FormatContractShortMP(nLiqPrice)) && (liqPrice > 0) && (trading_action == BUY)) {

          PrintToConsole("//////////////////////////////////////MARGIN CALL !!!!!!!!!!!\n");
          PrintToConsole("marketPrice >= LiqPrice\n");
          PrintToConsole("Liquidation price in marginLogic function: %s\n",nLiqPrice);
          // int rc = marginCall(*it, property, marketPrice, trading_action, amountInOrder);
          // assert(update_tally_map(*it, property,-liqPrice, LIQUIDATION_PRICE));
          // PrintToConsole("Margin call number rc %d\n", rc);

        } else {
           PrintToConsole("No margin call!\n");

        }

      }
      delete it;

}
////////////////////////////////////////
/** New things for Contracts */
int marginCall(const std::string& address, uint32_t propertyId, uint64_t marketPrice, uint8_t trading_action, int64_t amountInOrder)
{
    int rc = 0;
    PrintToConsole("____________________________________________________________\n");
    PrintToConsole("Into the marginCall function\n");

    const uint256 tx;
    // int rc = ContractDex_ADD(address, propertyId, amountInOrder, tx, 1, marketPrice, trading_action,0);
    if (rc == 0) {
        PrintToConsole("return of ContractDex_ADD: %d\n",rc);
    }
    return rc;
}

rational_t mastercore::notionalChange(uint32_t contractId)
{
    int index = static_cast<unsigned int>(contractId);
    int64_t den = static_cast<int64_t>(marketP[index]);
    if (den == 0) {
        PrintToConsole("returning 1\n");
        return rational_t(1,1);
    }

    rational_t uPrice = rational_t(1,den);

    switch (contractId) {
            case CONTRACT_ALL_DUSD:
                if (uPrice > 0){
                    return uPrice;
                }
            break;
            case CONTRACT_ALL_LTC:
                if (uPrice > 0){
                    return uPrice;
                }
            break;

            default: return rational_t(1,1);
        }
        return rational_t(1,1);
}

bool mastercore::marginNeeded(const std::string address, int64_t amountTraded, uint32_t contractId)
{
    // CMPSPInfo::Entry sp;
    // assert(_my_sps->getSP(contractId, sp));
    // double marginRe = static_cast<double>(sp.margin_requirement) / denMargin ;
    // int64_t nBalance = getMPbalance(address, sp.collateral_currency, BALANCE);
    // double conv = notionalChange(contractId,marketP[static_cast<int>(contractId)]);
    // double amountToReserve = amountTraded * marginRe * factor * conv ;
    // int64_t toReserve = static_cast<int64_t>(amountToReserve);
    // PrintToConsole("#######################################################\n");
    // PrintToConsole(" Inside marginNeeded function\n");
    // PrintToConsole("amount to Reserve: %d\n", amountToReserve);
    // PrintToConsole("Balance: %d\n", nBalance);
    // PrintToConsole("amountTraded: %d\n", amountTraded);
    // PrintToConsole("marginRe: %s\n", marginRe);
    // PrintToConsole("conv: %d\n", conv);
    // if (nBalance >= toReserve && nBalance != 0 && toReserve != 0){
    //     update_tally_map(address, sp.collateral_currency, -toReserve, BALANCE);
    //     update_tally_map(address, sp.collateral_currency, toReserve, CONTRACTDEX_RESERVE);
    // } else {
    //     // do we need upnl?
    //     LOCK(cs_tally);
    //     uint32_t nextSPID = _my_sps->peekNextSPID(1);
    //     for (uint32_t contractId = 1; contractId < nextSPID; contractId++) {  // looping on the properties
    //         CMPSPInfo::Entry sp;
    //         assert(_my_sps->getSP(contractId, sp));
    //         if (sp.subcategory != "Futures Contracts") {
    //             continue;
    //         }
    //
    //         int64_t amountTaken = getMPbalance(address, contractId, UPNL); // Collecting money!
    //         if (amountTaken >= toReserve) {
    //             PrintToConsole(" Ready with amountToReserve\n");
    //             update_tally_map(address, contractId, -toReserve, UPNL);
    //             update_tally_map(address, sp.collateral_currency, toReserve, CONTRACTDEX_RESERVE);
    //             break;   // ready with amountToReserve
    //         } else {
    //             PrintToConsole(" we need more money\n");
    //             update_tally_map(address, contractId, -amountTaken, UPNL); // we need more
    //             update_tally_map(address, sp.collateral_currency, amountTaken, CONTRACTDEX_RESERVE);
    //             toReserve -= amountTaken;
    //         }
    //
    //     }
    //
    //     if (toReserve > 0) { //  If we still need money, use fractional liquidation when closing to the threshold.
    //         PrintToConsole("fractional liquidation\n");
    //         PrintToConsole("Remaining amount: %d\n",toReserve);
    //         return false;
    //     }
    //
    // }
    // PrintToConsole("#######################################################\n");
    return true;

}

bool mastercore::marginBack(const std::string address, int64_t amountTraded, uint32_t contractId)
{
    // PrintToConsole("#######################################################\n");
    // PrintToConsole(" Inside marginBack function\n");
    // CMPSPInfo::Entry sp;
    // double percent = 0;
    // assert(_my_sps->getSP(contractId, sp));
    // double marginRe = sp.margin_requirement / denMargin;
    // int64_t longs = getMPbalance(address, contractId, POSSITIVE_BALANCE);
    // int64_t shorts = getMPbalance(address, contractId, NEGATIVE_BALANCE);
    // PrintToConsole("longs: %d\n", longs);
    // PrintToConsole("short: %d\n", shorts);
    // PrintToConsole("amountTraded: %d\n", amountTraded);
    // if (longs > 0 && shorts == 0) {
    //     PrintToConsole("Enter in the long side\n");
    //     percent = static_cast<double> (amountTraded) / longs;
    //     // boost::multiprecision::backends::divide_unsigned_helper() // TODO: improve the math!
    // } else if (longs == 0 && shorts > 0) {
    //     PrintToConsole("Enter in the short side\n");
    //     percent = static_cast<double> (amountTraded) / shorts;
    // }
    //
    // int64_t margin = getMPbalance(address, sp.collateral_currency, CONTRACTDEX_RESERVE);
    // double product = static_cast<double> (margin * percent);
    // int64_t amountBack = static_cast<int64_t>(product);
    // PrintToConsole("amount Back: %d\n",amountBack);
    // PrintToConsole("margin: %d\n", margin);
    // PrintToConsole("marginRe: %d\n", marginRe);
    // PrintToConsole("percent: %d\n", percent);
    // PrintToConsole("product: %d\n", product);
    // if (margin >= amountBack && amountBack > 0) {
    //    update_tally_map(address, sp.collateral_currency, -amountBack, CONTRACTDEX_RESERVE);
    //    update_tally_map(address, sp.collateral_currency, amountBack, BALANCE);
    // } else {
    //     PrintToConsole("wrong amount of margin: %d\n",margin);
    //     return false;
    // }
    // PrintToConsole("#######################################################\n");
    return true;
}

////////////////////////////////////////////////////////////////////////////////
const std::string ExodusAddress()
{
    if (isNonMainNet()) {
        return exodus_testnet;
    } else {
        return exodus_mainnet;
    }

}
/**
 * @return The marker for class C transactions.
 */
const std::vector<unsigned char> GetOmMarker()
{
    static unsigned char pch[] = {0x70, 0x71}; // Hex-encoded: "pq"

    return std::vector<unsigned char>(pch, pch + sizeof(pch) / sizeof(pch[0]));
}
