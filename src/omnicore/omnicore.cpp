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
#include "omnicore/operators_algo_clearing.h"
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
#include <cmath>
#include <iostream>
#include <numeric>

#include "tradelayer_matrices.h"
#include "externfns.h"
#include "omnicore/parse_string.h"

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
typedef boost::multiprecision::uint128_t ui128;
typedef boost::multiprecision::cpp_dec_float_100 dec_float;

CCriticalSection cs_tally;

static string exodus_address = "1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P";

static const string exodus_mainnet = "1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P";
static const string exodus_testnet = "mffpEfNWbCjDFEUGHadLB36bzfi8xQD3VY";  // one of our litecoin testnet (private) addresses
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
extern int64_t globalNumPrice;
extern int64_t globalDenPrice;
extern int64_t factorE;
extern double denMargin;
extern uint64_t marketP[NPTYPES];
extern std::vector<std::map<std::string, std::string>> path_ele;
extern int idx_expiration;
extern int expirationAchieve;
extern double globalPNLALL_DUSD;
extern int64_t globalVolumeALL_DUSD;
extern int lastBlockg;
extern int twapBlockg;
extern int vestingActivationBlock;
extern volatile int64_t globalVolumeALL_LTC;
extern std::vector<std::string> vestingAddresses;

CMPTxList *mastercore::p_txlistdb;
CMPTradeList *mastercore::t_tradelistdb;
COmniTransactionDB *mastercore::p_OmniTXDB;
extern MatrixTLS *pt_ndatabase;
extern int n_cols;
extern int n_rows;
//extern std::vector<std::map<std::string, std::string>> path_ele;
extern std::vector<std::map<std::string, std::string>> path_elef;
extern std::map<uint32_t, std::map<uint32_t, int64_t>> market_priceMap;
extern std::map<uint32_t, std::map<uint32_t, int64_t>> numVWAPMap;
extern std::map<uint32_t, std::map<uint32_t, int64_t>> denVWAPMap;
extern std::map<uint32_t, std::map<uint32_t, int64_t>> VWAPMap;
extern std::map<uint32_t, std::map<uint32_t, int64_t>> VWAPMapSubVector;
extern std::map<uint32_t, std::map<uint32_t, std::vector<int64_t>>> numVWAPVector;
extern std::map<uint32_t, std::map<uint32_t, std::vector<int64_t>>> denVWAPVector;
extern std::map<uint32_t, std::vector<int64_t>> mapContractAmountTimesPrice;
extern std::map<uint32_t, std::vector<int64_t>> mapContractVolume;
extern std::map<uint32_t, int64_t> VWAPMapContracts;
//extern volatile std::vector<std::map<std::string, std::string>> path_eleg;
extern std::string setExoduss;
/************************************************/
/** TWAP containers **/
extern std::map<uint32_t, std::vector<uint64_t>> cdextwap_ele;
extern std::map<uint32_t, std::vector<uint64_t>> cdextwap_vec;
extern std::map<uint32_t, std::map<uint32_t, std::vector<uint64_t>>> mdextwap_ele;
extern std::map<uint32_t, std::map<uint32_t, std::vector<uint64_t>>> mdextwap_vec;
/************************************************/
extern std::map<uint32_t, std::map<std::string, double>> addrs_upnlc;
extern std::map<std::string, int64_t> sum_upnls;

using mastercore::StrToInt64;

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
  if (0x80000000 & propertyId)
    {
      str = strprintf("Test token: %d : 0x%08X", 0x7FFFFFFF & propertyId, propertyId);
    }
  else
    {
      switch (propertyId)
	{
	case OMNI_PROPERTY_BTC: str = "BTC";
	  break;
	case OMNI_PROPERTY_ALL: str = "ALL";
	  break;
	case OMNI_PROPERTY_TALL: str = "TALL";
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
  if (propertyType & ALL_PROPERTY_TYPE_INDIVISIBLE) {
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
/** New things for contracts */
std::string FormatDivisibleZeroClean(int64_t n)
{
  int64_t n_abs = (n > 0 ? n : -n);
  int64_t quotient = n_abs/COIN;
  int64_t remainder = n_abs % COIN;
  std::string str = strprintf("%d.%08d", quotient/COIN, remainder);

  str.erase(str.find_last_not_of('0') + 1, std::string::npos);
  if (str.length() > 0) {
    std::string::iterator it = str.end() - 1;
    if (*it == '.')
      str.erase(it);
  }
  return str;
}

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
    long int q = atol(str.c_str());
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

int64_t getUserReserveMPbalance(const std::string& address, uint32_t propertyId)
{
    int64_t money = getMPbalance(address, propertyId, CONTRACTDEX_RESERVE);

    return money;
}

bool mastercore::isTestEcosystemProperty(uint32_t propertyId)
{
  if ((OMNI_PROPERTY_TALL == propertyId) || (TEST_ECO_PROPERTY_1 <= propertyId)) return true;

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
      tokenStr = " ALL";
    } else {
      tokenStr = " TALL";
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

void creatingVestingTokens()
{
  extern int64_t amountVesting;
  extern int64_t totalVesting;
  extern std::string admin_addrs;

  CMPSPInfo::Entry newSP;

  newSP.name = "Vesting Tokens";
  newSP.data = "Divisible Tokens";
  newSP.url  = "www.tradelayer.org";
  newSP.category = "N/A";
  newSP.subcategory = "N/A";
  newSP.prop_type = ALL_PROPERTY_TYPE_DIVISIBLE;
  newSP.num_tokens = amountVesting;
  newSP.attribute_type = ALL_PROPERTY_TYPE_VESTING;

  const uint32_t propertyIdVesting = _my_sps->putSP(OMNI_PROPERTY_ALL, newSP);
  assert(propertyIdVesting > 0);

  assert(update_tally_map(admin_addrs, propertyIdVesting, totalVesting, BALANCE));
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
    std::string strClassD = "5054"; /*the PT marker*/
    bool examineClosely = false;
    for (unsigned int n = 0; n < tx.vout.size(); ++n) {
        const CTxOut& output = tx.vout[n];
        std::string strSPB = HexStr(output.scriptPubKey.begin(), output.scriptPubKey.end());
        if (strSPB.find(strClassD) != std::string::npos) {
            examineClosely = true;
            break;
        }
    }

    // Examine everything when not on mainnet
    if (isNonMainNet()) {
        examineClosely = true;
    }

    if (!examineClosely) return NO_MARKER;

    for (unsigned int n = 0; n < tx.vout.size(); ++n) {
        const CTxOut& output = tx.vout[n];

        txnouttype outType;
        if (!GetOutputType(output.scriptPubKey, outType)) {
            continue;
        }
        if (!IsAllowedOutputType(outType, nBlock)) {
            continue;
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
            // or starts with the "tl" marker
            std::vector<std::string> scriptPushes;
            if (!GetScriptPushes(output.scriptPubKey, scriptPushes)) {
                continue;
            }
            if (!scriptPushes.empty()) {
                std::vector<unsigned char> vchMarker = GetOmMarker();
                std::vector<unsigned char> vchPushed = ParseHex(scriptPushes[0]);
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
    LOCK(cs_tx_cache);
    static unsigned int nCacheSize = gArgs.GetArg("-omnitxcache", 500000);

    if (view.GetCacheSize() > nCacheSize) {
        PrintToLog("%s(): clearing cache before insertion [size=%d, hit=%d, miss=%d]\n",
                __func__, view.GetCacheSize(), nCacheHits, nCacheMiss);
        view.Flush();
    }

    for (std::vector<CTxIn>::const_iterator it = tx.vin.begin(); it != tx.vin.end(); ++it) {
        const CTxIn& txIn = *it;
        unsigned int nOut = txIn.prevout.n;
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
    const CTxOut& txOut = view.GetOutputFor(txIn);
    assert(!txOut.IsNull());

    txnouttype whichType;
    if (!GetOutputType(txOut.scriptPubKey, whichType)) {
        return -108;
    }
    if (!IsAllowedInputType(whichType, nBlock)) {
       return -109;
    }
    CTxDestination source;
    if (ExtractDestination(txOut.scriptPubKey, source)) {
        strSender = EncodeDestination(source);
        PrintToLog("%s: strSender: %s ",__func__, strSender);
    } else return -110;


    int64_t inAll = view.GetValueIn(wtx);
    int64_t outAll = wtx.GetValueOut();
    int64_t txFee = inAll - outAll; // miner fee

    if (!strSender.empty()) {
        if (msc_debug_verbose) PrintToLog("The Sender: %s : fee= %s\n", strSender, FormatDivisibleMP(txFee));
    } else {
        PrintToLog("The sender is still EMPTY !!! txid: %s\n", wtx.GetHash().GetHex());
        return -5;
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
  for (unsigned int n = 0; n < tx.vout.size(); ++n) {
    CTxDestination dest;
    if (ExtractDestination(tx.vout[n].scriptPubKey, dest)) {
      std::string address = EncodeDestination(dest);
      PrintToLog("destination address: %s\n",address);
      PrintToLog("sender's address: %s\n",strSender);
      if (address == ExodusAddress() || address == strSender)
      {
           PrintToLog("continue...\n");
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

    // PrintToConsole("%d transactions processed, %d meta transactions found\n", nTxsTotal, nTxsFoundTotal);

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
        if (curBalance.size() != 15) return -1;


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
        int64_t unvested = boost::lexical_cast<int64_t>(curBalance[14]);

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
        if (unvested) update_tally_map(strAddress, propertyId, unvested, UNVESTED);
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

  // PrintToLog("%s(%s), loaded lines= %d, res= %d\n", __FUNCTION__, filename, lines, res);
  // LogPrintf("%s(): file: %s , loaded lines= %d, res= %d\n", __FUNCTION__, filename, lines, res);

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
            int64_t unvested = (*iter).second.getMoney(propertyId, UNVESTED);
            // we don't allow 0 balances to read in, so if we don't write them
            // it makes things match up better between persisted state and processed state
            if (0 == balance && 0 == sellReserved && 0 == acceptReserved && 0 == metadexReserved && contractdexReserved == 0 && positiveBalance == 0 && negativeBalance == 0 && realizedProfit == 0 && realizedLosses == 0 && count == 0 && remaining == 0 && liquidationPrice == 0 && upnl == 0 && nupnl == 0 && unvested == 0) {
                continue;
            }

            emptyWallet = false;

            lineOut.append(strprintf("%d:%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d;",
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
                    nupnl,
                    unvested));
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
  unsigned int nextSPID = _my_sps->peekNextSPID(OMNI_PROPERTY_ALL);
  unsigned int nextTestSPID = _my_sps->peekNextSPID(OMNI_PROPERTY_TALL);

  std::string lineOut = strprintf("%d,%d", nextSPID, nextTestSPID);

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
    write_state_file(pBlockIndex, FILETYPE_CDEXORDERS);
    write_state_file(pBlockIndex, FILETYPE_MARKETPRICES);
    write_state_file(pBlockIndex, FILETYPE_OFFERS);
    write_state_file(pBlockIndex, FILETYPE_MDEXORDERS);

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

     assert(p_txlistdb->setDBVersion() == DB_VERSION); // new set of databases, set DB version
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

  // PrintToConsole("Omni Core Lite shutdown completed\n");

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
  extern std::vector<std::string> vestingAddresses;
  extern volatile int64_t Lastx_Axis;
  extern volatile int64_t LastLinear;
  extern volatile int64_t LastQuad;
  extern volatile int64_t LastLog;
  extern std::vector<std::map<std::string, std::string>> lives_longs_vg;
  extern std::vector<std::map<std::string, std::string>> lives_shorts_vg;
  extern int BlockS;
  // std::vector<std::string> addrsl_vg;
  // std::vector<std::string> addrss_vg;

  ui128 numLog128;
  ui128 numQuad128;

  LOCK(cs_tally);

  if (!mastercoreInitialized) {
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

  int nBlockNow = GetHeight();

  /***********************************************************************/
  /** Calling The Settlement Algorithm **/

  if (nBlockNow%BlockS == 0 && nBlockNow != 0 && path_elef.size() != 0 && lastBlockg != nBlockNow) {

    /*****************************************************************************/

    PrintToLog("\nSettlement every 8 hours here. nBlockNow = %d\n", nBlockNow);
    pt_ndatabase = new MatrixTLS(path_elef.size(), n_cols); MatrixTLS &ndatabase = *pt_ndatabase;
    MatrixTLS M_file(path_elef.size(), n_cols);
    fillingMatrix(M_file, ndatabase, path_elef);
    n_rows = size(M_file, 0);
    PrintToLog("Matrix for Settlement: dim = (%d, %d)\n\n", n_rows, n_cols);
    //printing_matrix(M_file);

    /**********************************************************************/
    /** TWAP vector **/

    PrintToLog("\nTWAP Prices = \n");
    struct FutureContractObject *pfuture = getFutureContractObject(ALL_PROPERTY_TYPE_CONTRACT, "ALL F18");
    uint32_t property_traded = pfuture->fco_propertyId;

    // PrintToLog("\nVector CDExtwap_vec =\n");
    // for (unsigned int i = 0; i < cdextwap_vec[property_traded].size(); i++)
    //   PrintToLog("%s\n", FormatDivisibleMP(cdextwap_vec[property_traded][i]));

    uint64_t num_cdex = accumulate(cdextwap_vec[property_traded].begin(), cdextwap_vec[property_traded].end(), 0.0);
    rational_t twap_priceRatCDEx(num_cdex/COIN, cdextwap_vec[property_traded].size());
    int64_t twap_priceCDEx = mastercore::RationalToInt64(twap_priceRatCDEx);
    PrintToLog("\nTvwap Price CDEx = %s\n", FormatDivisibleMP(twap_priceCDEx));

    struct TokenDataByName *pfuture_ALL = getTokenDataByName("ALL");
    struct TokenDataByName *pfuture_USD = getTokenDataByName("dUSD");
    uint32_t property_all = pfuture_ALL->data_propertyId;
    uint32_t property_usd = pfuture_USD->data_propertyId;

    // PrintToLog("\nVector MDExtwap_vec =\n");
    // for (unsigned int i = 0; i < mdextwap_vec[property_all][property_usd].size(); i++)
    //   PrintToLog("%s\n", FormatDivisibleMP(mdextwap_vec[property_all][property_usd][i]));

    uint64_t num_mdex = accumulate(mdextwap_vec[property_all][property_usd].begin(), mdextwap_vec[property_all][property_usd].end(), 0.0);
    rational_t twap_priceRatMDEx(num_mdex/COIN, mdextwap_vec[property_all][property_usd].size());
    int64_t twap_priceMDEx = mastercore::RationalToInt64(twap_priceRatMDEx);
    PrintToLog("\nTvwap Price MDEx = %s\n", FormatDivisibleMP(twap_priceMDEx));

    /** Interest formula:  **/

    int64_t interest = clamp_function(abs(twap_priceCDEx-twap_priceMDEx), 0.05);
    PrintToLog("Interes to Pay = %s", FormatDivisibleMP(interest));

    /*****************************************************************************/
    cout << "\n\n";
    PrintToLog("\nCalling the Settlement Algorithm:\n\n");
    settlement_algorithm_fifo(M_file, interest, twap_priceCDEx);

    /**********************************************************************/
    /** Unallocating Dynamic Memory **/

    //path_elef.clear();
    market_priceMap.clear();
    numVWAPMap.clear();
    denVWAPMap.clear();
    VWAPMap.clear();
    VWAPMapSubVector.clear();
    numVWAPVector.clear();
    denVWAPVector.clear();
    mapContractAmountTimesPrice.clear();
    mapContractVolume.clear();
    VWAPMapContracts.clear();
    cdextwap_vec.clear();
  }
  /***********************************************************************/
  /** Vesting Tokens to Balance **/

  int64_t x_Axis = globalVolumeALL_LTC;
  int64_t LogAxis = mastercore::DoubleToInt64(log(static_cast<double>(x_Axis)/COIN));

  rational_t Factor1over3(1, 3);
  int64_t Factor1over3_64t = mastercore::RationalToInt64(Factor1over3);

  int64_t XAxis = x_Axis/COIN;
  PrintToLog("\nXAxis Decimal Scale = %d, x_Axis = %s, Lastx_Axis = %s\n", XAxis, FormatDivisibleMP(x_Axis), FormatDivisibleMP(Lastx_Axis));

  bool cond_first = x_Axis != 0;
  bool cond_secnd = x_Axis != Lastx_Axis;
  bool cond_third = vestingAddresses.size() != 0;

  if (findConjTrueValue(cond_first, cond_secnd, cond_third))
    {
      int64_t line64_t = 0, quad64_t = 0, log64_t  = 0;

      PrintToLog("\nALLs UNVESTED = %d\n", getMPbalance(vestingAddresses[0], OMNI_PROPERTY_ALL, UNVESTED));
      PrintToLog("ALLs BALANCE = %d\n", getMPbalance(vestingAddresses[0], OMNI_PROPERTY_ALL, BALANCE));

      for (unsigned int i = 0; i < vestingAddresses.size(); i++)
      	{
	  //PrintToLog("\nIteration #%d Inside Vesting function. Address = %s\n", i, vestingAddresses[i]);
	  int64_t vestingBalance = getMPbalance(vestingAddresses[i], OMNI_PROPERTY_VESTING, BALANCE);
	  int64_t unvestedALLBal = getMPbalance(vestingAddresses[i], OMNI_PROPERTY_ALL, UNVESTED);
      	  if (vestingBalance != 0 && unvestedALLBal != 0)
      	    {
      	      if (XAxis >= 0 && XAxis <= 300000)
      	  	{/** y = 1/3x **/

		  //PrintToLog("\nLinear Function\n");
		  arith_uint256 line256_t = mastercore::ConvertTo256(Factor1over3_64t)*mastercore::ConvertTo256(x_Axis)/COIN;
		  line64_t = mastercore::ConvertTo64(line256_t);

		  //PrintToLog("line64_t = %s, LastLinear = %s\n", FormatDivisibleMP(line64_t), FormatDivisibleMP(LastLinear));
      	  	  int64_t linearBalance = line64_t-LastLinear;
      	  	  arith_uint256 linew256_t = mastercore::ConvertTo256(linearBalance)*mastercore::ConvertTo256(vestingBalance)/COIN;
      	  	  int64_t linew64_t = mastercore::ConvertTo64(linew256_t);

      	  	  rational_t linearRationalw(linew64_t, (int64_t)TOTAL_AMOUNT_VESTING_TOKENS);
      	  	  int64_t linearWeighted = mastercore::RationalToInt64(linearRationalw);

      	  	  //PrintToLog("linearBalance = %s, vestingBalance = %s\n", FormatDivisibleMP(linearBalance), FormatDivisibleMP(vestingBalance));
      	  	  //PrintToLog("linearWeighted = %s\n", FormatDivisibleMP(linearWeighted));

		  assert(update_tally_map(vestingAddresses[i], OMNI_PROPERTY_ALL, -linearWeighted, UNVESTED));
		  assert(update_tally_map(vestingAddresses[i], OMNI_PROPERTY_ALL, linearWeighted, BALANCE));
      	   	}
	      else if (XAxis > 300000 && XAxis <= 10000000)
      		{ /** y = 100K+7/940900000(x^2-600Kx+90) */
      		  //PrintToLog("\nQuadratic Function\n");

      		  dec_float SecndTermnf = dec_float(7)*dec_float(XAxis)*dec_float(XAxis)/dec_float(940900000);
      		  int64_t SecndTermn64_t = mastercore::StrToInt64(SecndTermnf.str(DISPLAY_PRECISION_LEN, std::ios_base::fixed), true);
      		  // PrintToLog("SecndTermnf = %d\n", FormatDivisibleMP(SecndTermn64_t));

      		  dec_float ThirdTermnf = dec_float(7)*dec_float(600000)*dec_float(XAxis)/dec_float(940900000);
      		  int64_t ThirdTermn64_t = mastercore::StrToInt64(ThirdTermnf.str(DISPLAY_PRECISION_LEN, std::ios_base::fixed), true);
      		  // PrintToLog("ThirdTermnf = %d\n", FormatDivisibleMP(ThirdTermn64_t));

      		  dec_float ForthTermnf = dec_float(7)*dec_float(90000000000)/dec_float(940900000);
      		  int64_t ForthTermn64_t = mastercore::StrToInt64(ForthTermnf.str(DISPLAY_PRECISION_LEN, std::ios_base::fixed), true);
      		  // PrintToLog("ForthTermnf = %d\n", FormatDivisibleMP(ForthTermn64_t));

      		  quad64_t = (int64_t)(100000*COIN) + SecndTermn64_t - ThirdTermn64_t + ForthTermn64_t;
      		  int64_t quadBalance = quad64_t - LastQuad;
      		  // PrintToLog("quad64_t = %s, LastQuad = %s\n", FormatDivisibleMP(quad64_t), FormatDivisibleMP(LastQuad));

      		  multiply(numQuad128, (int64_t)quadBalance, (int64_t)vestingBalance);
      		  // PrintToLog("numQuad128 = %s\n", xToString(numQuad128/COIN));

      		  rational_t quadRationalw(numQuad128/COIN, (int64_t)TOTAL_AMOUNT_VESTING_TOKENS);
      		  int64_t quadWeighted = mastercore::RationalToInt64(quadRationalw);

      		  // PrintToLog("quadBalance = %s, vestingBalance = %s\n", FormatDivisibleMP(quadBalance), FormatDivisibleMP(vestingBalance));
      		  // PrintToLog("quadWeighted = %d\n", FormatDivisibleMP(quadWeighted));

      		  assert(update_tally_map(vestingAddresses[i], OMNI_PROPERTY_ALL, -quadWeighted, UNVESTED));
      		  assert(update_tally_map(vestingAddresses[i], OMNI_PROPERTY_ALL, quadWeighted, BALANCE));
      		}
      	      else if (XAxis > 10000000 && XAxis <= 1000000000)
      		{ /** y =  -1650000 + (152003 * ln(x)) */
      		  // PrintToLog("\nLogarithmic Function\n");

      		  arith_uint256 secndTermn256_t = mastercore::ConvertTo256((int64_t)(152003*COIN))*mastercore::ConvertTo256(LogAxis)/COIN;
      		  int64_t secndTermn64_t = mastercore::ConvertTo64(secndTermn256_t);
      		  // PrintToLog("secndTermn64_t = %s\n", FormatDivisibleMP(secndTermn64_t));

      		  log64_t = (int64_t)secndTermn64_t - (int64_t)(1650000*COIN);
      		  // PrintToLog("log64_t = %s\n", FormatDivisibleMP(log64_t));
      		  int64_t logBalance = log64_t - LastLog;

      		  // PrintToLog("logBalance = %s, vestingBalance = %s\n", FormatDivisibleMP(logBalance), FormatDivisibleMP(vestingBalance));
      		  multiply(numLog128, (int64_t)logBalance, (int64_t)vestingBalance);
      		  // PrintToLog("numLog128 = %s\n", xToString(numLog128/COIN));

      		  rational_t logRationalw(numLog128/COIN, TOTAL_AMOUNT_VESTING_TOKENS);
      		  int64_t logWeighted = mastercore::RationalToInt64(logRationalw);
      		  // PrintToLog("logWeighted = %s, LastLog = %s\n", FormatDivisibleMP(logWeighted), FormatDivisibleMP(LastLog));

      		  if (logWeighted)
      		    {
      		      if (getMPbalance(vestingAddresses[i], OMNI_PROPERTY_ALL, UNVESTED) >= logWeighted)
      			{
      			  assert(update_tally_map(vestingAddresses[i], OMNI_PROPERTY_ALL, -logWeighted, UNVESTED));
      			  assert(update_tally_map(vestingAddresses[i], OMNI_PROPERTY_ALL, logWeighted, BALANCE));
      			}
      		      else
      			{
      			  int64_t remaining = getMPbalance(vestingAddresses[i], OMNI_PROPERTY_ALL, UNVESTED);
      			  if (getMPbalance(vestingAddresses[i], OMNI_PROPERTY_ALL, UNVESTED) >= remaining && remaining >= 0)
      			    {
      			      assert(update_tally_map(vestingAddresses[i], OMNI_PROPERTY_ALL, -remaining, UNVESTED));
      			      assert(update_tally_map(vestingAddresses[i], OMNI_PROPERTY_ALL, remaining, BALANCE));
      			    }
      			}
      		    }
      		}
      	      else if (XAxis > 1000000000 && getMPbalance(vestingAddresses[i], OMNI_PROPERTY_ALL, UNVESTED) != 0)
      		{
      		  // PrintToLog("\nLogarithmic Function\n");

      		  arith_uint256 secndTermn256_t = mastercore::ConvertTo256((int64_t)(152003*COIN))*mastercore::ConvertTo256(LogAxis)/COIN;
      		  int64_t secndTermn64_t = mastercore::ConvertTo64(secndTermn256_t);
      		  // PrintToLog("secndTermn64_t = %s\n", FormatDivisibleMP(secndTermn64_t));

      		  log64_t = (int64_t)secndTermn64_t - (int64_t)(1650000*COIN);
      		  // PrintToLog("log64_t = %s\n", FormatDivisibleMP(log64_t));
      		  int64_t logBalance = log64_t - LastLog;

      		  // PrintToLog("logBalance = %s, vestingBalance = %s\n", FormatDivisibleMP(logBalance), FormatDivisibleMP(vestingBalance));
      		  multiply(numLog128, (int64_t)logBalance, (int64_t)vestingBalance);
      		  // PrintToLog("numLog128 = %s\n", xToString(numLog128/COIN));

      		  rational_t logRationalw(numLog128/COIN, TOTAL_AMOUNT_VESTING_TOKENS);
      		  int64_t logWeighted = mastercore::RationalToInt64(logRationalw);

      		  if (getMPbalance(vestingAddresses[i], OMNI_PROPERTY_ALL, UNVESTED))
      		    {
      		      if (getMPbalance(vestingAddresses[i], OMNI_PROPERTY_ALL, UNVESTED) < logWeighted)
      			{
      			  int64_t remaining = getMPbalance(vestingAddresses[i], OMNI_PROPERTY_ALL, UNVESTED);
      			  assert(update_tally_map(vestingAddresses[i], OMNI_PROPERTY_ALL, -remaining, UNVESTED));
      			  assert(update_tally_map(vestingAddresses[i], OMNI_PROPERTY_ALL, remaining, BALANCE));
      			}
      		      else
      			{
      			  assert(update_tally_map(vestingAddresses[i], OMNI_PROPERTY_ALL, -logWeighted, UNVESTED));
      			  assert(update_tally_map(vestingAddresses[i], OMNI_PROPERTY_ALL, logWeighted, BALANCE));
      			}
      		    }
      		}
	    }
	}

      PrintToLog("\nALLs UNVESTED = %d\n", getMPbalance(vestingAddresses[0], OMNI_PROPERTY_ALL, UNVESTED));
      PrintToLog("ALLs BALANCE = %d\n", getMPbalance(vestingAddresses[0], OMNI_PROPERTY_ALL, BALANCE));

      Lastx_Axis = x_Axis;
      LastLinear = line64_t;
      LastQuad = quad64_t;
      LastLog = log64_t;
    }
  /***********************************************/
  CMPTransaction mp_obj;
  mp_obj.unlockLogic();

  int expirationBlock = 0, tradeBlock = 0, checkExpiration = 0;
  CMPSPInfo::Entry sp;
  if ( id_contract != 0 )
    {
      if (_my_sps->getSP(id_contract, sp) && sp.prop_type == ALL_PROPERTY_TYPE_CONTRACT)
	{
	  expirationBlock = static_cast<int>(sp.blocks_until_expiration);
	  tradeBlock = static_cast<int>(pBlockIndex->nHeight);
	}
    }

  lastBlockg = static_cast<int>(pBlockIndex->nHeight);
  const CConsensusParams &params = ConsensusParams();
  vestingActivationBlock = params.MSC_VESTING_BLOCK;

  if (static_cast<int>(pBlockIndex->nHeight) == params.MSC_VESTING_BLOCK) creatingVestingTokens();

  int deadline = sp.init_block + expirationBlock;
  if ( tradeBlock != 0 && deadline != 0 ) checkExpiration = tradeBlock == deadline ? 1 : 0;

  if (checkExpiration) {
    idx_expiration += 1;
    if ( idx_expiration == 2 ) {
      expirationAchieve = 1;
    } else expirationAchieve = 0;
  } else expirationAchieve = 0;

  bool fFoundTx = false;
  int pop_ret = parseTransaction(false, tx, nBlock, idx, mp_obj, nBlockTime);

  if (0 == pop_ret) {
    assert(mp_obj.getEncodingClass() != NO_MARKER);
    assert(mp_obj.getSender().empty() == false);

    int interp_ret = mp_obj.interpretPacket();
    if (interp_ret) PrintToLog("!!! interpretPacket() returned %d !!!\n", interp_ret);

    // Only structurally valid transactions get recorded in levelDB
    // PKT_ERROR - 2 = interpret_Transaction failed, structurally invalid payload
    if (interp_ret != PKT_ERROR - 2) {
      bool bValid = (0 <= interp_ret);
      p_txlistdb->recordTX(tx.GetHash(), bValid, nBlock, mp_obj.getType(), mp_obj.getNewAmount());
      p_OmniTXDB->RecordTransaction(tx.GetHash(), idx);
    }
    fFoundTx |= (interp_ret == 0);
  } else if (pop_ret > 0) fFoundTx |= HandleDExPayments(tx, nBlock, mp_obj.getSender()); // testing the payment handler

  if (fFoundTx && msc_debug_consensus_hash_every_transaction) {
    uint256 consensusHash = GetConsensusHash();
    PrintToLog("Consensus hash for transaction %s: %s\n", tx.GetHash().GetHex(), consensusHash.GetHex());
  }

  return fFoundTx;
}

bool TxValidNodeReward(std::string ConsensusHash, std::string Tx)
{
  int NLast = 2;

  std::string LastTwoCharConsensus = ConsensusHash.substr(ConsensusHash.size() - NLast);
  std::string LastTwoCharTx = Tx.substr(Tx.size() - NLast);

  PrintToLog("\nLastTwoCharConsensus = %s\t LastTwoCharTx = %s\n", LastTwoCharConsensus,LastTwoCharTx);

  if (LastTwoCharConsensus == LastTwoCharTx)
    return true;
  else
    return false;
}

void lookingin_globalvector_pastlivesperpetuals(std::vector<std::map<std::string, std::string>> &lives_g, MatrixTLS M_file, std::vector<std::string> addrs_vg, std::vector<std::map<std::string, std::string>> &lives_h)
{
  for (std::vector<std::string>::iterator it = addrs_vg.begin(); it != addrs_vg.end(); ++it)
    lookingaddrs_inside_M_file(*it, M_file, lives_g, lives_h);
}

void lookingaddrs_inside_M_file(std::string addrs, MatrixTLS M_file, std::vector<std::map<std::string, std::string>> &lives_g, std::vector<std::map<std::string, std::string>> &lives_h)
{
  for (int i = 0; i < size(M_file, 0); ++i)
    {
      VectorTLS jrow_database(size(M_file, 1));
      sub_row(jrow_database, M_file, i);
      struct status_amounts *pt_status_amounts_byaddrs = get_status_amounts_byaddrs(jrow_database, addrs);

      /***********************************************************************************/
      /** Checking if the address is inside the new settlement database **/
      if (addrs == pt_status_amounts_byaddrs->addrs_src || addrs == pt_status_amounts_byaddrs->addrs_trk)
	{
	  for (std::vector<std::map<std::string, std::string>>::iterator it = lives_g.begin(); it != lives_g.end(); ++it)
	    {
	      struct status_lives_edge *pt_status_bylivesedge = get_status_bylivesedge(*it);
	      if (addrs == pt_status_bylivesedge->addrs)
		lives_h.push_back(*it);
	    }
	  break;
	}
      // if (addrs == pt_status_amounts_byaddrs->addrs_src)
      // 	{
      // long int lives_srch = pt_status_amounts_byaddrs->lives_src;
      // if (lives_srch != 0)
      //   {
      //     //PrintToLog("pt_status_amounts_byaddrs->lives_src = %d\t", pt_status_amounts_byaddrs->lives_src);
      //     continue;
      //   }
      // else
      //   {
      //     PrintToLog("pt_status_amounts_byaddrs->lives_src = %d\t", pt_status_amounts_byaddrs->lives_src);
      //     /** Deleting entry idx_q containing addrs from lives vector. ¡¡lives_src = 0!!**/
      //     int idx_q = finding_idxforaddress(addrs, lives_g);
      //     lives_g[idx_q]["addrs"]       = std::string();
      //     lives_g[idx_q]["status"]      = std::string();
      //     lives_g[idx_q]["lives"]       = std::to_string(0);
      //     lives_g[idx_q]["entry_price"] = std::to_string(0);
      //     lives_g[idx_q]["edge_row"]    = std::to_string(0);
      //     lives_g[idx_q]["path_number"] = std::to_string(0);
      //     break;
      //   }
      // }
      // else if (addrs == pt_status_amounts_byaddrs->addrs_trk)
      //   {
      // long int lives_trkh = pt_status_amounts_byaddrs->lives_trk;
      // if (lives_trkh != 0)
      //   {
      //     //PrintToLog("pt_status_amounts_byaddrs->lives_trk = %d\t", pt_status_amounts_byaddrs->lives_trk);
      //     continue;
      //   }
      // else
      //   {
      //     PrintToLog("pt_status_amounts_byaddrs->lives_trk = %d\t", pt_status_amounts_byaddrs->lives_trk);
      //     /** Deleting entry idx_q containing addrs from lives vector. ¡¡lives_trk = 0!!**/
      //     int idx_q = finding_idxforaddress(addrs, lives_g);
      //     lives_g[idx_q]["addrs"]       = std::string();
      //     lives_g[idx_q]["status"]      = std::string();
      //     lives_g[idx_q]["lives"]       = std::to_string(0);
      //     lives_g[idx_q]["entry_price"] = std::to_string(0);
      //     lives_g[idx_q]["edge_row"]    = std::to_string(0);
      //     lives_g[idx_q]["path_number"] = std::to_string(0);
      //     break;
      //   }
      // 	}
      //   else
      // 	continue;
    }
}

int finding_idxforaddress(std::string addrs, std::vector<std::map<std::string, std::string>> lives)
{
  int idx_q = 0;
  for (std::vector<std::map<std::string, std::string>>::iterator it = lives.begin(); it != lives.end(); ++it)
    {
      idx_q += 1;
      struct status_lives_edge *pt_status_bylivesedge = get_status_bylivesedge(*it);
      if (addrs == pt_status_bylivesedge->addrs) break;
    }
  return idx_q-1;
}

inline int64_t clamp_function(int64_t diff, int64_t nclamp)
{
  int64_t interest = diff > nclamp ? diff-nclamp+1 : (diff < -nclamp ? diff+nclamp+1 : 0.01);
  return interest;
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
				const std::vector<unsigned char>& data, uint256& txid, std::string& rawHex, bool commit,
				unsigned int minInputs)
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
    vecSend.push_back(std::make_pair(scriptPubKey, 0 < referenceAmount ? referenceAmount : 50000000));
  }

  // Now we have what we need to pass to the wallet to create the transaction, perform some checks first

  if (!coinControl.HasSelected()) return MP_ERR_INPUTSELECT_FAIL;

  std::vector<CRecipient> vecRecipients;
  for (size_t i = 0; i < vecSend.size(); ++i)
    {
      const std::pair<CScript, int64_t>& vec = vecSend[i];
      CRecipient recipient = {vec.first, CAmount(vec.second), false};
      vecRecipients.push_back(recipient);
    }

  // Ask the wallet to create the transaction (note mining fee determined by Bitcoin Core params)
  if (!pwalletMain->CreateTransaction(vecRecipients, wtxNew, reserveKey, nFeeRet, nChangePosInOut, strFailReason, coinControl, true))
    {
      return MP_ERR_CREATE_TX;
    }

  // Workaround for SigOps limit
  {
    if (!FillTxInputCache(*(wtxNew.tx)))
      {
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
         CTransactionRef wtx;
         CMPTransaction mp_obj;
         if (!GetTransaction(txid, wtx, Params().GetConsensus(), blockHash, true)) {
              PrintToLog("ERROR: While loading alert %s: tx in levelDB but does not exist.\n", txid.GetHex());
              continue;
         }

         if (0 != ParseTransaction(*(wtx), blockHeight, 0, mp_obj)) {
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
    if (msc_debug_txdb) PrintToLog("%s(): store: %s=%s, status: %s\n", __func__, strKey, strValue, status.ToString());
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
        // PrintToLog("DEXPAYDEBUG : Writing master record %s(%s, valid=%s, block= %d, type= %d, number of payments= %lu)\n", __FUNCTION__, txid.ToString(), fValid ? "YES":"NO", nBlock, type, numberOfPayments);
        if (pdb)
        {
            status = pdb->Put(writeoptions, key, value);
            // PrintToLog("DEXPAYDEBUG : %s(): %s, line %d, file: %s\n", __FUNCTION__, status.ToString(), __LINE__, __FILE__);
        }

        // Step 4 - Write sub-record with payment details
        const string txidStr = txid.ToString();
        const string subKey = STR_PAYMENT_SUBKEY_TXID_PAYMENT_COMBO(txidStr, paymentNumber);
        const string subValue = strprintf("%d:%s:%s:%d:%lu", vout, buyer, seller, propertyId, nValue);
        Status subStatus;
        // PrintToLog("DEXPAYDEBUG : Writing sub-record %s with value %s\n", subKey, subValue);
        if (pdb)
        {
            subStatus = pdb->Put(writeoptions, subKey, subValue);
            // PrintToLog("DEXPAYDEBUG : %s(): %s, line %d, file: %s\n", __FUNCTION__, subStatus.ToString(), __LINE__, __FILE__);
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
               // PrintToLog("%s() DELETING: %s=%s\n", __FUNCTION__, skey.ToString(), svalue.ToString());
               if (bDeleteFound) pdb->Delete(writeoptions, skey);
           }
        }
    }
    // PrintToLog("%s(%d, %d); n_found= %d\n", __FUNCTION__, starting_block, ending_block, n_found);
    delete it;

    return (n_found);
 }

void CMPTxList::getMPTransactionAddresses(std::vector<std::string> &vaddrs)
{
  if (!pdb) return;

  Slice skey, svalue;
  Iterator* it = NewIterator();

  for(it->SeekToFirst(); it->Valid(); it->Next())
    {
      svalue = it->value();
      string strValue = svalue.ToString();

      std::vector<std::string> vstr;
      boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);

      if (vstr.size() != 17) continue;

      std::string address1 = vstr[0];
      std::string address2 = vstr[1];

      PrintToLog("Inside getMPTransactionAddresses\n");
      PrintToLog("address1 = %s,\t address2 =%s", address1, address2);

      if(!find_string_strv(address1, vaddrs)) vaddrs.push_back(address1);
      if(!find_string_strv(address2, vaddrs)) vaddrs.push_back(address2);
    }
  delete it;
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
  update_sum_upnls();
  marginMain(pBlockIndex->nHeight);
  // addInterestPegged(nBlockPrev,pBlockIndex);
  // eraseExpiredCrowdsale(pBlockIndex);
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
     // TODO: fix AbortNode to be compatible with litecoin 16.0.3
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
          // PrintToLog("TRADEDB error - unexpected number of tokens in value (%s)\n", strValue);
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

void CMPTradeList::recordNewCommit(const uint256& txid, const std::string& channelAddress, const std::string& sender, uint32_t propertyId, uint64_t amountCommited, uint32_t vOut, int blockNum, int blockIndex)
{
  if (!pdb) return;
  std::string strValue = strprintf("%s:%s:%d:%d:%d:%d:%d", channelAddress, sender, propertyId, amountCommited, vOut, blockNum, blockIndex);
  const string key = blockNum + "+" + txid.ToString(); // order by blockNum
  Status status = pdb->Put(writeoptions, txid.ToString(), strValue);
  ++nWritten;
  // if (msc_debug_tradedb)
  PrintToLog("%s(): %s\n", __FUNCTION__, status.ToString());
}

void CMPTradeList::recordNewWithdrawal(const uint256& txid, const std::string& channelAddress, const std::string& sender, uint32_t propertyId, uint64_t amountToWithdrawal, uint32_t vOut, int blockNum, int blockIndex)
{
  if (!pdb) return;
  std::string strValue = strprintf("%s:%s:%d:%d:%d:%d:%d", channelAddress, sender, propertyId, amountToWithdrawal, vOut, blockNum, blockIndex);
  const string key = blockNum + "+" + txid.ToString(); // order by blockNum
  Status status = pdb->Put(writeoptions, txid.ToString(), strValue);
  ++nWritten;
  // if (msc_debug_tradedb)
  PrintToLog("%s(): %s\n", __FUNCTION__, status.ToString());
}



void CMPTradeList::recordMatchedTrade(const uint256 txid1, const uint256 txid2, string address1, string address2, unsigned int prop1, unsigned int prop2, uint64_t amount1, uint64_t amount2, int blockNum, int64_t fee)
{
  if (!pdb) return;
  const string key = txid1.ToString() + "+" + txid2.ToString();
  const string value = strprintf("%s:%s:%u:%u:%lu:%lu:%d:%d", address1, address2, prop1, prop2, amount1, amount2, blockNum, fee);

  int64_t volumeALL64_t = 0;
  extern volatile int64_t factorALLtoLTC;

  /********************************************************************/
  if (prop1 == OMNI_PROPERTY_ALL)
    {
      // PrintToLog("factorALLtoLTC =%s, amount1 = %s: CMPMetaDEx\n", FormatDivisibleMP(factorALLtoLTC), FormatDivisibleMP(amount1));
      arith_uint256 volumeALL256_t = mastercore::ConvertTo256(factorALLtoLTC)*mastercore::ConvertTo256(amount1)/COIN;
      // PrintToLog("ALLs involved in the traded 256 Bits ~ %s ALL\n", volumeALL256_t.ToString());
      volumeALL64_t = mastercore::ConvertTo64(volumeALL256_t);
      // PrintToLog("ALLs involved in the traded 64 Bits ~ %s ALL\n", FormatDivisibleMP(volumeALL64_t));
    }
  else if (prop2 == OMNI_PROPERTY_ALL)
    {
      // PrintToLog("factorALLtoLTC =%s, amount1 = %s: CMPMetaDEx\n", FormatDivisibleMP(factorALLtoLTC), FormatDivisibleMP(amount2));
      arith_uint256 volumeALL256_t = mastercore::ConvertTo256(factorALLtoLTC)*mastercore::ConvertTo256(amount2)/COIN;
      // PrintToLog("ALLs involved in the traded 256 Bits ~ %s ALL\n", volumeALL256_t.ToString());
      volumeALL64_t = mastercore::ConvertTo64(volumeALL256_t);
      // PrintToLog("ALLs involved in the traded 64 Bits ~ %s ALL\n", FormatDivisibleMP(volumeALL64_t));
    }

  // PrintToLog("Number of Traded Contracts ~ %s LTC\n", FormatDivisibleMP(volumeALL64_t));
  // PrintToLog("\nGlobal LTC Volume No Updated: CMPMetaDEx = %s \n", FormatDivisibleMP(globalVolumeALL_LTC));
  globalVolumeALL_LTC += volumeALL64_t;
  // PrintToLog("\nGlobal LTC Volume Updated: CMPMetaDEx = %s\n", FormatDivisibleMP(globalVolumeALL_LTC));

  /****************************************************/
  /** Building TWAP vector MDEx **/

  struct TokenDataByName *pfuture_ALL = getTokenDataByName("ALL");
  struct TokenDataByName *pfuture_USD = getTokenDataByName("dUSD");

  uint32_t property_all = pfuture_ALL->data_propertyId;
  uint32_t property_usd = pfuture_USD->data_propertyId;

  Filling_Twap_Vec(mdextwap_ele, mdextwap_vec, property_all, property_usd, market_priceMap[property_all][property_usd]);
  PrintToLog("\nMDExtwap_ele.size() = %d\t property_all = %d\t property_usd = %d\t market_priceMap = %s\n",
	     mdextwap_ele[property_all][property_usd].size(), property_all, property_usd,
	     FormatDivisibleMP(market_priceMap[property_all][property_usd]));
  // PrintToLog("\nVector MDExtwap_ele =\n");
  // for (unsigned int i = 0; i < mdextwap_ele[property_all][property_usd].size(); i++)
  //   PrintToLog("%s\n", FormatDivisibleMP(mdextwap_ele[property_all][property_usd][i]));

  /****************************************************/

  Status status;
  if (pdb)
    {
      status = pdb->Put(writeoptions, key, value);
      ++nWritten;
      // if (msc_debug_tradedb) PrintToLog("%s(): %s\n", __FUNCTION__, status.ToString());
    }
}

void CMPTradeList::recordMatchedTrade(const uint256 txid1, const uint256 txid2, string address1, string address2, uint64_t effective_price, uint64_t amount_maker, uint64_t amount_taker, int blockNum1, int blockNum2, uint32_t property_traded, string tradeStatus, int64_t lives_s0, int64_t lives_s1, int64_t lives_s2, int64_t lives_s3, int64_t lives_b0, int64_t lives_b1, int64_t lives_b2, int64_t lives_b3, string s_maker0, string s_taker0, string s_maker1, string s_taker1, string s_maker2, string s_taker2, string s_maker3, string s_taker3, int64_t nCouldBuy0, int64_t nCouldBuy1, int64_t nCouldBuy2, int64_t nCouldBuy3,uint64_t amountpnew, uint64_t amountpold)
{
  if (!pdb) return;

  extern volatile int idx_q;
  //extern volatile unsigned int path_length;
  std::map<std::string, std::string> edgeEle;
  std::map<std::string, double>::iterator it_addrs_upnlm;
  std::map<uint32_t, std::map<std::string, double>>::iterator it_addrs_upnlc;
  std::vector<std::map<std::string, std::string>>::iterator it_path_ele;
  std::vector<std::map<std::string, std::string>>::reverse_iterator reit_path_ele;
  //std::vector<std::map<std::string, std::string>> path_eleh;
  bool savedata_bool = false;
  extern volatile int64_t factorALLtoLTC;
  std::string sblockNum2 = std::to_string(blockNum2);
  double UPNL1 = 0, UPNL2 = 0;
  /********************************************************************/
  const string key =  sblockNum2 + "+" + txid1.ToString() + "+" + txid2.ToString(); //order with block of taker.
  const string value = strprintf("%s:%s:%lu:%lu:%lu:%d:%d:%s:%s:%d:%d:%d:%s:%s:%d:%d:%d", address1, address2, effective_price, amount_maker, amount_taker, blockNum1, blockNum2, s_maker0, s_taker0, lives_s0, lives_b0, property_traded, txid1.ToString(), txid2.ToString(), nCouldBuy0,amountpold, amountpnew);

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
  int number_lines = 0;
  if ( status_bool1 || status_bool2 )
    {
      buildingEdge(edgeEle, address1, address2, s_maker1, s_taker1, lives_s1, lives_b1, nCouldBuy1, effective_price, idx_q, 0);
      //path_ele.push_back(edgeEle);
      //path_eleh.push_back(edgeEle);

      path_elef.push_back(edgeEle);
      buildingEdge(edgeEle, address1, address2, s_maker2, s_taker2, lives_s2, lives_b2, nCouldBuy2, effective_price, idx_q, 0);
      //path_ele.push_back(edgeEle);
      //path_eleh.push_back(edgeEle);

      path_elef.push_back(edgeEle);
      // PrintToLog("Line 1: %s\n", line1);
      // PrintToLog("Line 2: %s\n", line2);
      number_lines += 2;
      if ( s_maker3 != "EmptyStr" && s_taker3 != "EmptyStr" )
	{
	  buildingEdge(edgeEle, address1, address2, s_maker3, s_taker3, lives_s3, lives_b3,nCouldBuy3,effective_price,idx_q,0);
	  //path_ele.push_back(edgeEle);
	  //path_eleh.push_back(edgeEle);

	  path_elef.push_back(edgeEle);
	  // PrintToLog("Line 3: %s\n", line3);
	  number_lines += 1;
	}
    }
  else
    {
      buildingEdge(edgeEle, address1, address2, s_maker0, s_taker0, lives_s0, lives_b0, nCouldBuy0, effective_price, idx_q, 0);
      //path_ele.push_back(edgeEle);
      //path_eleh.push_back(edgeEle);

      path_elef.push_back(edgeEle);
      // PrintToLog("Line 0: %s\n", line0);
      number_lines += 1;
    }

  // PrintToLog("\nPath Ele inside recordMatchedTrade. Length last match = %d\n", number_lines);
  // for (it_path_ele = path_ele.begin(); it_path_ele != path_ele.end(); ++it_path_ele) printing_edges_database(*it_path_ele);

  /********************************************/
  /** Building TWAP vector CDEx **/

  Filling_Twap_Vec(cdextwap_ele, cdextwap_vec, property_traded, 0, effective_price);
  PrintToLog("\ncdextwap_ele.size() = %d\n", cdextwap_ele[property_traded].size());
  // PrintToLog("\nVector CDExtwap_ele =\n");
  // for (unsigned int i = 0; i < cdextwap_ele[property_traded].size(); i++)
  //   PrintToLog("%s\n", FormatDivisibleMP(cdextwap_ele[property_traded][i]));

  /********************************************/

  // loopForUPNL(path_ele, path_eleh, path_length, address1, address2, s_maker0, s_taker0, UPNL1, UPNL2, effective_price, nCouldBuy0);
  // unsigned int limSup = path_ele.size()-path_length;
  // path_length = path_ele.size();

  // // PrintToLog("UPNL1 = %d, UPNL2 = %d\n", UPNL1, UPNL2);
  // addrs_upnlc[property_traded][address1] = UPNL1;
  // addrs_upnlc[property_traded][address2] = UPNL2;

  // for (it_addrs_upnlc = addrs_upnlc.begin(); it_addrs_upnlc != addrs_upnlc.end(); ++it_addrs_upnlc)
  //   {
  //     for (it_addrs_upnlm = it_addrs_upnlc->second.begin(); it_addrs_upnlm != it_addrs_upnlc->second.end(); ++it_addrs_upnlm)
  //     	{
  // 	  if (it_addrs_upnlm->first != address1 && it_addrs_upnlm->first != address2)
  // 	    {
  // 	      double entry_price_first = 0;
  // 	      int idx_price_first = 0;
  // 	      uint64_t entry_pricefirst_num = 0;
  // 	      double exit_priceh = (double)effective_price/COIN;
  // 	      uint64_t amount = 0;
  // 	      std::string status = "";
  // 	      std::string last_match_status = "";

  // 	      for (reit_path_ele = path_ele.rbegin(); reit_path_ele != path_ele.rend(); ++reit_path_ele)
  // 		{
  // 		  if(finding_string(it_addrs_upnlm->first, (*reit_path_ele)["addrs_src"]))
  // 		    {
  // 		      last_match_status = (*reit_path_ele)["status_src"];
  // 		      break;
  // 		    }
  // 		  else if(finding_string(it_addrs_upnlm->first, (*reit_path_ele)["addrs_trk"]))
  // 		    {
  // 		      last_match_status = (*reit_path_ele)["status_trk"];
  // 		      break;
  // 		    }
  // 		}
  // 	      loopforEntryPrice(path_ele, path_eleh, it_addrs_upnlm->first, last_match_status, entry_price_first, idx_price_first, entry_pricefirst_num, limSup, exit_priceh, amount, status);
  // 	      // PrintToLog("\namount for UPNL_show: %d\n", amount);
  // 	      double UPNL_show = PNL_function(entry_price_first, exit_priceh, amount, status);
  // 	      // PrintToLog("\nUPNL_show = %d\n", UPNL_show);
  // 	      addrs_upnlc[it_addrs_upnlc->first][it_addrs_upnlm->first] = UPNL_show;
  // 	    }
  // 	}
  //   }

  // for (it_addrs_upnlc = addrs_upnlc.begin(); it_addrs_upnlc != addrs_upnlc.end(); ++it_addrs_upnlc)
  //   {
  //     // PrintToLog("\nMap with addrs:upnl for propertyId = %d\n", it_addrs_upnlc->first);
  //     for (it_addrs_upnlm = it_addrs_upnlc->second.begin(); it_addrs_upnlm != it_addrs_upnlc->second.end(); ++it_addrs_upnlm)
  //     	{
  // 	  // PrintToLog("ADDRS = %s, UPNL = %d\n", it_addrs_upnlm->first, it_addrs_upnlm->second);
  // 	}

  unsigned int contractId = static_cast<unsigned int>(property_traded);
  CMPSPInfo::Entry sp;
  assert(_my_sps->getSP(property_traded, sp));
  uint32_t NotionalSize = sp.notional_size;

  globalPNLALL_DUSD += UPNL1 + UPNL2;
  globalVolumeALL_DUSD += nCouldBuy0;

  arith_uint256 volumeALL256_t = mastercore::ConvertTo256(NotionalSize)*mastercore::ConvertTo256(nCouldBuy0)/COIN;
  // PrintToLog("ALLs involved in the traded 256 Bits ~ %s ALL\n", volumeALL256_t.ToString());

  int64_t volumeALL64_t = mastercore::ConvertTo64(volumeALL256_t);
  // PrintToLog("ALLs involved in the traded 64 Bits ~ %s ALL\n", FormatDivisibleMP(volumeALL64_t));

  arith_uint256 volumeLTC256_t = mastercore::ConvertTo256(factorALLtoLTC)*mastercore::ConvertTo256(volumeALL64_t)/COIN;
  // PrintToLog("LTCs involved in the traded 256 Bits ~ %s LTC\n", volumeLTC256_t.ToString());

  int64_t volumeLTC64_t = mastercore::ConvertTo64(volumeLTC256_t);
  // PrintToLog("LTCs involved in the traded 64 Bits ~ %d LTC\n", FormatDivisibleMP(volumeLTC64_t));

  globalVolumeALL_LTC += volumeLTC64_t;
  // PrintToLog("\nGlobal LTC Volume Updated: CMPContractDEx = %d \n", FormatDivisibleMP(globalVolumeALL_LTC));

  int64_t volumeToCompare = 0;
  bool perpetualBool = callingPerpetualSettlement(globalPNLALL_DUSD, globalVolumeALL_DUSD, volumeToCompare);
  if (perpetualBool) PrintToLog("Perpetual Settlement Online");

  // PrintToLog("\nglobalPNLALL_DUSD = %d, globalVolumeALL_DUSD = %d, contractId = %d\n", globalPNLALL_DUSD, globalVolumeALL_DUSD, contractId);

  std::fstream fileglobalPNLALL_DUSD;
  fileglobalPNLALL_DUSD.open ("globalPNLALL_DUSD.txt", std::fstream::in | std::fstream::out | std::fstream::app);
  if ( contractId == ALL_PROPERTY_TYPE_CONTRACT )
    saveDataGraphs(fileglobalPNLALL_DUSD, std::to_string(globalPNLALL_DUSD));
  fileglobalPNLALL_DUSD.close();

  std::fstream fileglobalVolumeALL_DUSD;
  fileglobalVolumeALL_DUSD.open ("globalVolumeALL_DUSD.txt", std::fstream::in | std::fstream::out | std::fstream::app);
  if ( contractId == ALL_PROPERTY_TYPE_CONTRACT )
    saveDataGraphs(fileglobalVolumeALL_DUSD, std::to_string(FormatShortIntegerMP(globalVolumeALL_DUSD)));
  fileglobalVolumeALL_DUSD.close();

  Status status;
  if (pdb)
    {
      status = pdb->Put(writeoptions, key, value);
      ++nWritten;
    }

  // PrintToLog("\n\nEnd of recordMatchedTrade <------------------------------\n");
}

void Filling_Twap_Vec(std::map<uint32_t, std::vector<uint64_t>> &twap_ele, std::map<uint32_t, std::vector<uint64_t>> &twap_vec,
		      uint32_t property_traded, uint32_t property_desired, uint64_t effective_price)
{
  int nBlockNow = GetHeight();
  std::vector<uint64_t> twap_minmax;
  PrintToLog("\nCheck here CDEx:\t nBlockNow = %d\t twapBlockg = %d\n", nBlockNow, twapBlockg);

  if (nBlockNow == twapBlockg)
    twap_ele[property_traded].push_back(effective_price);
  else
    {
      if (twap_ele[property_traded].size() != 0)
	{
	  twap_minmax = min_max(twap_ele[property_traded]);
	  uint64_t numerator = twap_ele[property_traded].front()+twap_minmax[0]+twap_minmax[1]+twap_ele[property_traded].back();
	  rational_t twapRat(numerator/COIN, 4);
	  int64_t twap_elej = mastercore::RationalToInt64(twapRat);
	  PrintToLog("\ntwap_elej CDEx = %s\n", FormatDivisibleMP(twap_elej));
	  cdextwap_vec[property_traded].push_back(twap_elej);
	}
      twap_ele[property_traded].clear();
      twap_ele[property_traded].push_back(effective_price);
    }
  twapBlockg = nBlockNow;
}

void Filling_Twap_Vec(std::map<uint32_t, std::map<uint32_t, std::vector<uint64_t>>> &twap_ele,
		      std::map<uint32_t, std::map<uint32_t, std::vector<uint64_t>>> &twap_vec,
		      uint32_t property_traded, uint32_t property_desired, uint64_t effective_price)
{
  int nBlockNow = GetHeight();
  std::vector<uint64_t> twap_minmax;
  PrintToLog("\nCheck here MDEx:\t nBlockNow = %d\t twapBlockg = %d\n", nBlockNow, twapBlockg);

  if (nBlockNow == twapBlockg)
    twap_ele[property_traded][property_desired].push_back(effective_price);
  else
    {
      if (twap_ele[property_traded][property_desired].size() != 0)
	{
	  twap_minmax = min_max(twap_ele[property_traded][property_desired]);
	  uint64_t numerator = twap_ele[property_traded][property_desired].front()+twap_minmax[0]+
	    twap_minmax[1]+twap_ele[property_traded][property_desired].back();
	  rational_t twapRat(numerator/COIN, 4);
	  int64_t twap_elej = mastercore::RationalToInt64(twapRat);
	  PrintToLog("\ntwap_elej MDEx = %s\n", FormatDivisibleMP(twap_elej));
	  mdextwap_vec[property_traded][property_desired].push_back(twap_elej);
	}
      twap_ele[property_traded][property_desired].clear();
      twap_ele[property_traded][property_desired].push_back(effective_price);
    }
  twapBlockg = nBlockNow;
}

bool callingPerpetualSettlement(double globalPNLALL_DUSD, int64_t globalVolumeALL_DUSD, int64_t volumeToCompare)
{
  bool perpetualBool = false;

  if ( globalPNLALL_DUSD == 0 )
    {
      // PrintToLog("\nLiquidate Forward Positions\n");
      perpetualBool = true;
    }
  else if ( globalVolumeALL_DUSD > volumeToCompare )
    {
      // PrintToLog("\nTake decisions for globalVolumeALL_DUSD > volumeToCompare\n");
    }

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

double PNL_function(double entry_price, double exit_price, int64_t amount_trd, std::string netted_status)
{
  double PNL = 0;

  if ( finding_string("Long", netted_status) )
    PNL = (double)amount_trd*(1/(double)entry_price-1/(double)exit_price);
  else if ( finding_string("Short", netted_status) )
    PNL = (double)amount_trd*(1/(double)exit_price-1/(double)entry_price);

  return PNL;
}

void loopForUPNL(std::vector<std::map<std::string, std::string>> path_ele, std::vector<std::map<std::string, std::string>> path_eleh, unsigned int path_length, std::string address1, std::string address2, std::string status1, std::string status2, double &UPNL1, double &UPNL2, uint64_t exit_price, int64_t nCouldBuy0)
{
  std::vector<std::map<std::string, std::string>>::iterator it_path_ele;

  double entry_pricesrc = 0, entry_pricetrk = 0;
  double exit_priceh = (double)exit_price/COIN;

  int idx_price_src = 0, idx_price_trk = 0;
  uint64_t entry_pricesrc_num = 0, entry_pricetrk_num = 0;
  std::string addrs_upnl = address1;
  unsigned int limSup = path_ele.size()-path_length;
  uint64_t amount_src = 0, amount_trk = 0;
  std::string status_src = "", status_trk = "";

  loopforEntryPrice(path_ele, path_eleh, address1, status1, entry_pricesrc, idx_price_src, entry_pricesrc_num, limSup, exit_priceh, amount_src, status_src);
  // PrintToLog("\nentry_pricesrc = %d, address1 = %s, exit_price = %d, amount_src = %d\n", entry_pricesrc, address1, exit_priceh, amount_src);
  UPNL1 = PNL_function(entry_pricesrc, exit_priceh, amount_src, status_src);

  loopforEntryPrice(path_ele, path_eleh, address2, status2, entry_pricetrk, idx_price_trk, entry_pricetrk_num, limSup, exit_priceh, amount_trk, status_trk);
  // PrintToLog("\nentry_pricetrk = %d, address2 = %s, exit_price = %d, amount_trk = %d\n", entry_pricetrk, address2, exit_priceh, amount_src);
  UPNL2 = PNL_function(entry_pricetrk, exit_priceh, amount_trk, status_trk);
}

void loopforEntryPrice(std::vector<std::map<std::string, std::string>> path_ele, std::vector<std::map<std::string, std::string>> path_eleh, std::string addrs_upnl, std::string status_match, double &entry_price, int &idx_price, uint64_t entry_price_num, unsigned int limSup, double exit_priceh, uint64_t &amount, std::string &status)
{
  std::vector<std::map<std::string, std::string>>::reverse_iterator reit_path_ele;
  std::vector<std::map<std::string, std::string>>::iterator it_path_eleh;
  double price_num_w = 0;
  extern VectorTLS *pt_changepos_status; VectorTLS &changepos_status  = *pt_changepos_status;

  if (finding(status_match, changepos_status))
    {
      for (it_path_eleh = path_eleh.begin(); it_path_eleh != path_eleh.end(); ++it_path_eleh)
      	{
      	  if (addrs_upnl == (*it_path_eleh)["addrs_src"] && !finding_string("Netted", (*it_path_eleh)["status_src"]))
      	    {
      	      price_num_w += stod((*it_path_eleh)["matched_price"])*stod((*it_path_eleh)["amount_trd"]);
      	      amount += static_cast<uint64_t>(stol((*it_path_eleh)["amount_trd"]));
      	    }
	  if (addrs_upnl == (*it_path_eleh)["addrs_trk"] && !finding_string("Netted", (*it_path_eleh)["status_trk"]))
      	    {
      	      price_num_w += stod((*it_path_eleh)["matched_price"])*stod((*it_path_eleh)["amount_trd"]);
      	      amount += static_cast<uint64_t>(stol((*it_path_eleh)["amount_trd"]));
      	    }
      	}
      entry_price = price_num_w/(double)amount;

      for (reit_path_ele = path_eleh.rbegin(); reit_path_ele != path_eleh.rend(); ++reit_path_ele)
	{
	  if (addrs_upnl == (*reit_path_ele)["addrs_src"] && finding_string("Open", (*reit_path_ele)["status_src"]))
	    {
	      status = (*reit_path_ele)["status_src"];
	    }
	  else if (addrs_upnl == (*reit_path_ele)["addrs_trk"] && finding_string("Open", (*reit_path_ele)["status_trk"]))
	    {
	      status = (*reit_path_ele)["status_trk"];
	    }
	}
    }
  else
    {
      // PrintToLog("\nLoop in the Path Element\n");
      for (it_path_eleh = path_eleh.begin(); it_path_eleh != path_eleh.end(); ++it_path_eleh)
      	{
      	  if (addrs_upnl == (*it_path_eleh)["addrs_src"] || addrs_upnl == (*it_path_eleh)["addrs_trk"])
      	    {
      	      printing_edges_database(*it_path_eleh);
      	      price_num_w += stod((*it_path_eleh)["matched_price"])*stod((*it_path_eleh)["amount_trd"]);
      	      amount += static_cast<uint64_t>(stol((*it_path_eleh)["amount_trd"]));
      	    }
      	}

      // PrintToLog("\nInside LoopForEntryPrice:\n");
      for (reit_path_ele = path_ele.rbegin()+limSup; reit_path_ele != path_ele.rend(); ++reit_path_ele)
	{
	  if (addrs_upnl == (*reit_path_ele)["addrs_src"])
	    {
	      if (finding_string("Open", (*reit_path_ele)["status_src"]) || finding_string("Increased", (*reit_path_ele)["status_src"]))
		{
		  // PrintToLog("\nRow Reverse Loop for addrs_upnl = %s\n", addrs_upnl);
		  printing_edges_database(*reit_path_ele);
		  idx_price += 1;
		  entry_price_num += static_cast<uint64_t>(stol((*reit_path_ele)["matched_price"]));
		  amount += static_cast<uint64_t>(stol((*reit_path_ele)["amount_trd"]));

		  price_num_w += stod((*reit_path_ele)["matched_price"])*stod((*reit_path_ele)["amount_trd"]);

		  if (finding_string("Open", (*reit_path_ele)["status_src"]))
		    {
		      // PrintToLog("\naddrs = %s, price_num_w trk = %d, amount = %d\n", addrs_upnl, price_num_w, amount);
		      entry_price = price_num_w/(double)amount;
		      status = (*reit_path_ele)["status_src"];
		      break;
		    }
		}
	    }
	  else if ( addrs_upnl == (*reit_path_ele)["addrs_trk"])
	    {
	      if (finding_string("Open", (*reit_path_ele)["status_trk"]) || finding_string("Increased", (*reit_path_ele)["status_trk"]))
		{
		  // PrintToLog("\nRow Reverse Loop for addrs_upnl = %s\n", addrs_upnl);
		  printing_edges_database(*reit_path_ele);
		  idx_price += 1;
		  entry_price_num += static_cast<uint64_t>(stol((*reit_path_ele)["matched_price"]));
		  amount += static_cast<uint64_t>(stol((*reit_path_ele)["amount_trd"]));

		  price_num_w += stod((*reit_path_ele)["matched_price"])*stod((*reit_path_ele)["amount_trd"]);

		  if (finding_string("Open", (*reit_path_ele)["status_trk"]))
		    {
		      // PrintToLog("\naddrs = %s, price_num_w trk = %d, amount = %d\n", addrs_upnl, price_num_w, amount);
		      entry_price = price_num_w/(double)amount;
		      status = (*reit_path_ele)["status_trk"];
		      break;
		    }
		}
	    }
	  else
	    continue;
	}
      if (idx_price == 0)
	{
	  entry_price = exit_priceh;
	  status = status_match;
	}
    }
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
  PrintToLog("{ addrs_src : %s , status_src : %s, lives_src : %d, addrs_trk : %s , status_trk : %s, lives_trk : %d, amount_trd : %d, matched_price : %d, edge_row : %d, ghost_edge : %d }\n", path_ele["addrs_src"], path_ele["status_src"], path_ele["lives_src"], path_ele["addrs_trk"], path_ele["status_trk"], path_ele["lives_trk"], path_ele["amount_trd"], path_ele["matched_price"], path_ele["edge_row"], path_ele["ghost_edge"]);
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

  for(it->SeekToLast(); it->Valid(); it->Prev()) {
      // search key to see if this is a matching trade
      std::string strKey = it->key().ToString();
      // PrintToLog("key of this match: %s ****************************\n",strKey);
      std::string strValue = it->value().ToString();

      // ensure correct amount of tokens in value string
      boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);
      if (vstr.size() != 17) {
          //PrintToLog("TRADEDB error - unexpected number of tokens in value (%s)\n", strValue);
          // PrintToConsole("TRADEDB error - unexpected number of tokens in value %d \n",vstr.size());
          continue;
      }

     // decode the details from the value string
      // const string value = strprintf("%s:%s:%lu:%lu:%lu:%d:%d:%s:%s:%d:%d:%d:%s:%s:%d", address1, address2, effective_price, amount_maker, amount_taker, blockNum1,
      // blockNum2, s_maker0, s_taker0, lives_s0, lives_b0, property_traded, txid1.ToString(), txid2.ToString(), nCouldBuy0);

      uint32_t prop1 = boost::lexical_cast<uint32_t>(vstr[11]);
      std::string address1 = vstr[0];
      std::string address2 = vstr[1];
      int64_t amount1 = boost::lexical_cast<int64_t>(vstr[15]);
      int64_t amount2 = boost::lexical_cast<int64_t>(vstr[16]);
      int block = boost::lexical_cast<int>(vstr[6]);
      int64_t price = boost::lexical_cast<int64_t>(vstr[2]);
      int64_t amount_traded = boost::lexical_cast<int64_t>(vstr[14]);
      std::string txidmaker = vstr[12];
      std::string txidtaker = vstr[13];

      // populate trade object and add to the trade array, correcting for orientation of trade

      if (prop1 != propertyId) {
         continue;
      }

      UniValue trade(UniValue::VOBJ);
      if (tradeArray.size() <= 9)
      {
          trade.push_back(Pair("maker_address", address1));
          trade.push_back(Pair("maker_txid", txidmaker));
          trade.push_back(Pair("taker_address", address2));
          trade.push_back(Pair("taker_txid", txidtaker));
          trade.push_back(Pair("amount_maker", FormatByType(amount1,2)));
          trade.push_back(Pair("amount_taker", FormatByType(amount2,2)));
          trade.push_back(Pair("price", FormatByType(price,2)));
          trade.push_back(Pair("taker_block",block));
          trade.push_back(Pair("amount_traded",FormatByType(amount_traded,2)));
          tradeArray.push_back(trade);
          ++count;
      }
  }
  // clean up
  delete it; // Desallocation proccess
  if (count) { return true; } else { return false; }
}

// unfiltered list of trades
bool CMPTradeList::getMatchingTradesUnfiltered(uint32_t propertyId, UniValue& tradeArray)
{
  if (!pdb) return false;

  int count = 0;

  std::vector<std::string> vstr;
  // string txidStr = txid.ToString();

  leveldb::Iterator* it = NewIterator(); // Allocation proccess

  for(it->SeekToLast(); it->Valid(); it->Prev()) {
      // search key to see if this is a matching trade
      std::string strKey = it->key().ToString();
      // PrintToLog("key of this match: %s ****************************\n",strKey);
      std::string strValue = it->value().ToString();

      // ensure correct amount of tokens in value string
      boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);
      if (vstr.size() != 17)
          continue;

      // decode the details from the value string
      // const string value = strprintf("%s:%s:%lu:%lu:%lu:%d:%d:%s:%s:%d:%d:%d:%s:%s:%d", address1, address2, effective_price, amount_maker, amount_taker, blockNum1,
      // blockNum2, s_maker0, s_taker0, lives_s0, lives_b0, property_traded, txid1.ToString(), txid2.ToString(), nCouldBuy0);

      uint32_t prop1 = boost::lexical_cast<uint32_t>(vstr[11]);
      std::string address1 = vstr[0];
      std::string address2 = vstr[1];
      int64_t amount1 = boost::lexical_cast<int64_t>(vstr[15]);
      int64_t amount2 = boost::lexical_cast<int64_t>(vstr[16]);
      int block = boost::lexical_cast<int>(vstr[6]);
      int64_t price = boost::lexical_cast<int64_t>(vstr[2]);
      int64_t amount_traded = boost::lexical_cast<int64_t>(vstr[14]);
      std::string txidmaker = vstr[12];
      std::string txidtaker = vstr[13];

      // populate trade object and add to the trade array, correcting for orientation of trade

      if (prop1 != propertyId) {
         continue;
      }

      UniValue trade(UniValue::VOBJ);

      // removing the 10 item filter
      // if (tradeArray.size() <= 9)
      // {
          trade.push_back(Pair("maker_address", address1));
          trade.push_back(Pair("maker_txid", txidmaker));
          trade.push_back(Pair("taker_address", address2));
          trade.push_back(Pair("taker_txid", txidtaker));
          trade.push_back(Pair("amount_maker", FormatByType(amount1,2)));
          trade.push_back(Pair("amount_taker", FormatByType(amount2,2)));
          trade.push_back(Pair("price", FormatByType(price,2)));
          trade.push_back(Pair("taker_block",block));
          trade.push_back(Pair("amount_traded",FormatByType(amount_traded,2)));
          tradeArray.push_back(trade);
          ++count;
      // }
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
          // PrintToLog("TRADEDB error - unexpected number of tokens in value (%s)\n", strValue);
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

      // populate trade object and add to the trade array, correcting for orientation of trade
      UniValue trade(UniValue::VOBJ);

      trade.push_back(Pair("creator address", address));
      trade.push_back(Pair("height", height));
      trade.push_back(Pair("txid", strKey));
      trade.push_back(Pair("amount created", amount));
      trade.push_back(Pair("series", series));
      tradeArray.push_back(trade);
      ++count;
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

rational_t mastercore::notionalChange(uint32_t contractId)
{
    rational_t inversePrice;
    if (globalDenPrice != 0) {
        inversePrice = rational_t(globalNumPrice,globalDenPrice);
    } else {
        inversePrice = rational_t(1,1);
    }

    return inversePrice;
}

bool mastercore::marginMain(int Block)
{
  //checking in map for address and the UPNL.
    PrintToLog("Block in marginMain: %d\n",Block);
    LOCK(cs_tally);
    uint32_t nextSPID = _my_sps->peekNextSPID(1);
    for (uint32_t contractId = 1; contractId < nextSPID; contractId++)
    {
        CMPSPInfo::Entry sp;
        if (_my_sps->getSP(contractId, sp))
        {
            PrintToLog("Property Id: %d\n",contractId);
            if (sp.prop_type != ALL_PROPERTY_TYPE_CONTRACT)
            {
                PrintToLog("Property is not future contract\n");
                continue;
            }
        }

        uint32_t collateralCurrency = sp.collateral_currency;
        //int64_t notionalSize = static_cast<int64_t>(sp.notional_size);

        // checking the upnl map
        std::map<uint32_t, std::map<std::string, double>>::iterator it = addrs_upnlc.find(contractId);
        std::map<std::string, double> upnls = it->second;

        //  if upnls is < 0, we need to cancel orders or liquidate contracts.
        for(std::map<std::string, double>::iterator it2 = upnls.begin(); it2 != upnls.end(); ++it2)
	  {
            const std::string address = it2->first;

            int64_t upnl = static_cast<int64_t>(it2->second * factorE);
            PrintToLog("upnl: %d",upnl);
            // if upnl is positive, keep searching
            if (upnl >= 0)
                continue;

            PrintToLog("upnl: %d",upnl);
            PrintToLog("sum_check_upnl: %d",sum_check_upnl(address));

            // if sum of upnl is bigger than this upnl, skip address.
            if (sum_check_upnl(address) > upnl)
                continue;

            // checking position margin
            int64_t posMargin = pos_margin(contractId, address, sp.prop_type, sp.margin_requirement);

            // if there's no position, something is wrong!
            if (posMargin < 0)
                continue;

            // checking the initMargin (init_margin = position_margin + active_orders_margin)
            int64_t initMargin = getMPbalance(address,collateralCurrency,CONTRACTDEX_MARGIN); ;

            rational_t percent = rational_t(-upnl,initMargin);

            int64_t ordersMargin = initMargin - posMargin;

            PrintToLog("\n--------------------------------------------------\n");
            PrintToLog("\ninitMargin= %d\n", initMargin);
            PrintToLog("\npositionMargin= %d\n", posMargin);
            PrintToLog("\nordersMargin= %d\n", ordersMargin);
            PrintToLog("upnl= %d\n",upnl);
            PrintToLog("factor= %d\n",factor);
            PrintToLog("proportion upnl/initMargin= %d\n",xToString(percent));
            PrintToLog("\n--------------------------------------------------\n");

            // if the upnl loss is more than 80% of the initial Margin
            if (factor <= percent)
            {
                const uint256 txid;
                unsigned char ecosystem = '\0';
                PrintToLog("factor <= percent : %d <= %d\n",xToString(factor), xToString(percent));
                PrintToLog("margin call!\n");
                ContractDex_CLOSE_POSITION(txid, Block, address, ecosystem, contractId, collateralCurrency);
                continue;

            // if the upnl loss is more than 20% and minus 80% of the Margin
            } else if (factor2 <= percent) {
                PrintToLog("CALLING CANCEL IN ORDER\n");
                PrintToLog("factor2 <= percent : %s <= %s\n",xToString(factor2),xToString(percent));
                int64_t fbalance, diff;
                int64_t margin = getMPbalance(address,collateralCurrency,CONTRACTDEX_MARGIN);
                int64_t ibalance = getMPbalance(address,collateralCurrency, BALANCE);
                int64_t left = - 0.2 * margin - upnl;

                bool orders = false;

                do
                {
                      PrintToLog("margin before cancel: %s\n", margin);
                      if(ContractDex_CANCEL_IN_ORDER(address, contractId) == 1)
                          orders = true;
                      fbalance = getMPbalance(address,collateralCurrency, BALANCE);
                      diff = fbalance - ibalance;

                      PrintToLog("ibalance: %s\n",ibalance);
                      PrintToLog("fbalance: %s\n",fbalance);
                      PrintToLog("diff: %d\n",diff);
                      PrintToLog("left: %d\n",left);

                      if ( left <= diff ) {
                          PrintToLog("left <= diff !\n");
                      }

                      if (orders) {
                          PrintToLog("orders=true !\n");
                      } else
                         PrintToLog("orders=false\n");

                } while(diff < left && !orders);

                // if left is negative, the margin is above the first limit (more than 80% maintMargin)
                if (0 < left)
                {
                    PrintToLog("orders can't cover, we have to check the balance to refill margin\n");
                    PrintToLog("left: %d\n", left);
                    //we have to see if we can cover this with the balance
                    int64_t balance = getMPbalance(address,collateralCurrency,BALANCE);

                    if(balance >= left) // recover to 80% of maintMargin
                    {
                        PrintToLog("\n balance >= left\n");
                        update_tally_map(address, collateralCurrency, -left, BALANCE);
                        update_tally_map(address, collateralCurrency, left, CONTRACTDEX_MARGIN);
                        continue;

                    } else { // not enough money in balance to recover margin, so we use position

                         PrintToLog("not enough money in balance to recover margin, so we use position\n");
                         if (balance > 0)
                         {
                             update_tally_map(address, collateralCurrency, -balance, BALANCE);
                             update_tally_map(address, collateralCurrency, balance, CONTRACTDEX_MARGIN);
                         }

                         const uint256 txid;
                         unsigned int idx = 0;
                         uint8_t option;
                         int64_t fcontracts;

                         int64_t longs = getMPbalance(address,contractId,POSSITIVE_BALANCE);
                         int64_t shorts = getMPbalance(address,contractId,NEGATIVE_BALANCE);

                         PrintToLog("longs: %d\n", longs);
                         PrintToLog("shorts: %d\n", shorts);

                         (longs > 0 && shorts == 0) ? option = SELL, fcontracts = longs : option = BUY, fcontracts = shorts;

                         PrintToLog("option: %d\n", option);
                         PrintToLog("upnl: %d",upnl);
                         PrintToLog("posMargin: %d", posMargin);

                         arith_uint256 contracts = DivideAndRoundUp(ConvertTo256(posMargin) + ConvertTo256(-upnl), ConvertTo256(static_cast<int64_t>(sp.margin_requirement)));
                         int64_t icontracts = ConvertTo64(contracts);


                         PrintToLog("icontracts: %d\n", icontracts);
                         PrintToLog("fcontracts before: %d\n", fcontracts);

                         if (icontracts > fcontracts)
                             icontracts = fcontracts;

                         PrintToLog("fcontracts after: %d\n", fcontracts);

                         ContractDex_ADD_MARKET_PRICE(address, contractId, icontracts, Block, txid, idx, option, 0);


                    }

                }

            } else {
                PrintToLog("the upnl loss is LESS than 20% of the margin, nothing happen\n");

            }
        }
    }

    return true;

}


int64_t mastercore::sum_check_upnl(std::string address)
{
    std::map<std::string, int64_t>::iterator it = sum_upnls.find(address);
    int64_t upnl = it->second;
    return upnl;
}


void mastercore::update_sum_upnls()
{
    //cleaning the sum_upnls map
    if(!sum_upnls.empty())
        sum_upnls.clear();

    LOCK(cs_tally);
    uint32_t nextSPID = _my_sps->peekNextSPID(1);

    for (uint32_t contractId = 1; contractId < nextSPID; contractId++)
    {
        CMPSPInfo::Entry sp;
        if (_my_sps->getSP(contractId, sp))
        {
            if (sp.prop_type != ALL_PROPERTY_TYPE_CONTRACT)
                continue;

            std::map<uint32_t, std::map<std::string, double>>::iterator it = addrs_upnlc.find(contractId);
            std::map<std::string, double> upnls = it->second;

            for(std::map<std::string, double>::iterator it1 = upnls.begin(); it1 != upnls.end(); ++it1)
            {
                const std::string address = it1->first;
                int64_t upnl = static_cast<int64_t>(it1->second * factorE);

                //add this in the sumupnl vector
                sum_upnls[address] += upnl;
            }

        }
    }
}

/* margin needed for a given position */
int64_t mastercore::pos_margin(uint32_t contractId, std::string address, uint16_t prop_type, uint32_t margin_requirement)
{
        arith_uint256 maintMargin;

        if (prop_type != ALL_PROPERTY_TYPE_CONTRACT)
        {
            PrintToLog("this is not a future contract\n");
            return -1;
        }

        int64_t longs = getMPbalance(address,contractId,POSSITIVE_BALANCE);
        int64_t shorts = getMPbalance(address,contractId,NEGATIVE_BALANCE);

        PrintToLog("longs: %d, shorts: %d\n",longs,shorts);
        PrintToLog("margin requirement: %d\n",margin_requirement);

        if (longs > 0 && shorts == 0)
        {
            maintMargin = (ConvertTo256(longs) * ConvertTo256(static_cast<int64_t>(margin_requirement))) / ConvertTo256(factorE);
        } else if (shorts > 0 && longs == 0){
            maintMargin = (ConvertTo256(shorts) * ConvertTo256(static_cast<int64_t>(margin_requirement))) / ConvertTo256(factorE);
        } else {
            PrintToLog("there's no position avalaible\n");
            return -2;
        }

        int64_t maint_margin = ConvertTo64(maintMargin);
        PrintToLog("maint margin: %d\n",maint_margin);
        return maint_margin;
}

uint64_t int64ToUint64(int64_t value)
{
  uint64_t uvalue = value;

  uvalue += INT64_MAX;
  uvalue += 1;

  return uvalue;
}

const std::string ExodusAddress()
{
//     if(RegTest()){
//        return setExoduss;
//     }

    if (isNonMainNet()) {
        return exodus_testnet;
    } else {
        return exodus_mainnet;
    }

}

/**
 * @retrieve commits for a channel
 */
 bool CMPTradeList::getAllCommits(std::string channelAddress, UniValue& tradeArray)
 {
   if (!pdb) return false;

   int count = 0;

   std::vector<std::string> vstr;
   // string txidStr = txid.ToString();

   leveldb::Iterator* it = NewIterator(); // Allocation proccess

   for(it->SeekToLast(); it->Valid(); it->Prev()) {

       PrintToLog("Inside looop in db\n");
       // search key to see if this is a matching trade
       std::string strKey = it->key().ToString();
       // PrintToLog("key of this match: %s ****************************\n",strKey);
       std::string strValue = it->value().ToString();

       // ensure correct amount of tokens in value string
       boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);
       if (vstr.size() != 7) {
           //PrintToLog("TRADEDB error - unexpected number of tokens in value (%s)\n", strValue);
           // PrintToConsole("TRADEDB error - unexpected number of tokens in value %d \n",vstr.size());
           continue;
       }

       //channelAddress, sender, propertyId, amountCommited, vOut, blockIndex

       std::string cAddress = vstr[0];

       if(channelAddress != cAddress)
           continue;

       std::string sender = vstr[1];
       uint32_t propertyId = boost::lexical_cast<uint32_t>(vstr[2]);

       int64_t amount = boost::lexical_cast<int64_t>(vstr[3]);
       uint32_t vOut = boost::lexical_cast<uint32_t>(vstr[4]);
       int blockNum = boost::lexical_cast<int>(vstr[5]);
       int blockIndex = boost::lexical_cast<int>(vstr[6]);

       // populate trade object and add to the trade array, correcting for orientation of trade


       UniValue trade(UniValue::VOBJ);
       if (tradeArray.size() <= 20)
       {
           trade.push_back(Pair("sender", sender));
           trade.push_back(Pair("amount", FormatByType(amount,2)));
           trade.push_back(Pair("vOut", FormatByType(vOut,1)));
           trade.push_back(Pair("block",blockNum));
           trade.push_back(Pair("block_index",blockIndex));
           tradeArray.push_back(trade);
           ++count;
       }
   }
   // clean up
   delete it; // Desallocation proccess
   if (count) { return true; } else { return false; }
 }

 uint64_t CMPTradeList::getSumofCommits(const std::string& channelAddress, const std::string& senderAddress, uint32_t propertyId)
 {

   if (!pdb) return false;

   int count = 0;
   uint64_t sumAmount = 0;

   std::vector<std::string> vstr;
   // string txidStr = txid.ToString();

   leveldb::Iterator* it = NewIterator(); // Allocation proccess

   for(it->SeekToLast(); it->Valid(); it->Prev()) {

       PrintToLog("Inside looop in db\n");
       // search key to see if this is a matching trade
       std::string strKey = it->key().ToString();
       // PrintToLog("key of this match: %s ****************************\n",strKey);
       std::string strValue = it->value().ToString();

       // ensure correct amount of tokens in value string
       boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);
       if (vstr.size() != 7) {
           //PrintToLog("TRADEDB error - unexpected number of tokens in value (%s)\n", strValue);
           // PrintToConsole("TRADEDB error - unexpected number of tokens in value %d \n",vstr.size());
           continue;
       }

       //channelAddress, sender, propertyId, amountCommited, vOut, blockIndex

       std::string cAddress = vstr[0];

       if(channelAddress != cAddress)
           continue;

       std::string sender = vstr[1];

       if(senderAddress != sender)
           continue;

       uint32_t propId = boost::lexical_cast<uint32_t>(vstr[2]);

       if(propertyId != propId)
           continue;

       uint64_t amount = boost::lexical_cast<uint64_t>(vstr[3]);
       // populate trade object and add to the trade array, correcting for orientation of trade
       sumAmount += amount;
   }

   // clean up
   delete it; // Desallocation proccess

   return sumAmount;
   
 }


 /**
  * @retrieve withdrawal for a channel
  */
  bool CMPTradeList::getAllWithdrawals(std::string senderAddress, UniValue& tradeArray)
  {
    if (!pdb) return false;

    int count = 0;

    std::vector<std::string> vstr;
    // string txidStr = txid.ToString();

    leveldb::Iterator* it = NewIterator(); // Allocation proccess

    for(it->SeekToLast(); it->Valid(); it->Prev()) {

        PrintToLog("Inside looop in db\n");
        // search key to see if this is a matching trade
        std::string strKey = it->key().ToString();
        // PrintToLog("key of this match: %s ****************************\n",strKey);
        std::string strValue = it->value().ToString();

        // ensure correct amount of tokens in value string
        boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);
        if (vstr.size() != 7) {
            //PrintToLog("TRADEDB error - unexpected number of tokens in value (%s)\n", strValue);
            // PrintToConsole("TRADEDB error - unexpected number of tokens in value %d \n",vstr.size());
            continue;
        }

        //channelAddress, sender, propertyId, amountCommited, vOut, blockIndex

        std::string cAddress = vstr[0];

        std::string sender = vstr[1];

        if(sender != senderAddress)
            continue;

        uint32_t propertyId = boost::lexical_cast<uint32_t>(vstr[2]);

        int64_t withdrawalAmount = boost::lexical_cast<int64_t>(vstr[3]);
        uint32_t vOut = boost::lexical_cast<uint32_t>(vstr[4]);
        int blockNum = boost::lexical_cast<int>(vstr[5]);
        int blockIndex = boost::lexical_cast<int>(vstr[6]);

        // populate trade object and add to the trade array, correcting for orientation of trade


        UniValue trade(UniValue::VOBJ);
        if (tradeArray.size() <= 20)
        {
            trade.push_back(Pair("sender", sender));
            trade.push_back(Pair("withdrawalAmount", FormatByType(withdrawalAmount,2)));
            trade.push_back(Pair("vOut", FormatByType(vOut,1)));
            trade.push_back(Pair("block",blockNum));
            trade.push_back(Pair("block_index",blockIndex));
            tradeArray.push_back(trade);
            ++count;
        }
    }
    // clean up
    delete it; // Desallocation proccess
    if (count) { return true; } else { return false; }
  }

/**
 * @return The marker for class C transactions.
 */
const std::vector<unsigned char> GetOmMarker()
{
     static unsigned char pch[] = {0x50, 0x54}; // Hex-encoded: "PT"

    return std::vector<unsigned char>(pch, pch + sizeof(pch) / sizeof(pch[0]));
}
