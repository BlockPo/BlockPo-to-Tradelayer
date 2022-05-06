/**
 * @file tradelayer.cpp
 *
 * This file contains the core of Trade Layer.
 */

#include <tradelayer/tradelayer.h>
#include <tradelayer/activation.h>
#include <tradelayer/consensushash.h>
#include <tradelayer/convert.h>
#include <tradelayer/dex.h>
#include <tradelayer/ce.h>
#include <tradelayer/encoding.h>
#include <tradelayer/errors.h>
#include <tradelayer/externfns.h>
#include <tradelayer/log.h>
#include <tradelayer/mdex.h>
#include <tradelayer/notifications.h>
#include <tradelayer/operators_algo_clearing.h>
#include <tradelayer/parse_string.h>
#include <tradelayer/pending.h>
#include <tradelayer/persistence.h>
#include <tradelayer/register.h>
#include <tradelayer/rules.h>
#include <tradelayer/script.h>
#include <tradelayer/sp.h>
#include <tradelayer/fees.h>
#include <tradelayer/tally.h>
#include <tradelayer/tradelayer_matrices.h>
#include <tradelayer/tx.h>
#include <tradelayer/uint256_extensions.h>
#include <tradelayer/utilsbitcoin.h>
#include <tradelayer/version.h>
#include <tradelayer/walletcache.h>
#include <tradelayer/wallettxs.h>
#include <tradelayer/tupleutils.hpp>

#include <tradelayer/insurancefund.h>

#include <arith_uint256.h>
#include <base58.h>
#include <chainparams.h>
#include <coins.h>
#include <consensus/params.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <core_io.h>
#include <init.h>
#include <net.h> // for g_connman.get()
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <script/standard.h>
#include <serialize.h>
#include <sync.h>
#include <tinyformat.h>
#include <ui_interface.h>
#include <uint256.h>
#include <util/system.h>
#include <util/strencodings.h>
#include <util/time.h>
#include <validation.h>
#include <script/ismine.h>

#ifdef ENABLE_WALLET
#include <wallet/wallet.h>
#include <wallet/fees.h>
#include <wallet/coincontrol.h>
#endif

#include <leveldb/db.h>

#include <assert.h>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <univalue.h>
#include <unordered_map>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/exception/to_string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/math/special_functions/sign.hpp>
#include <boost/multiprecision/cpp_int.hpp>

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
typedef boost::multiprecision::cpp_dec_float_100 dec_float;

CCriticalSection cs_tally;
typedef boost::multiprecision::uint128_t ui128;

static int nWaterlineBlock = 0;

//! Available balances of wallet properties
std::map<uint32_t, int64_t> global_balance_money;
//! Vector containing a list of properties relative to the wallet
std::set<uint32_t> global_wallet_property_list;
//! Active channels
std::map<std::string,Channel> channels_Map;
//! Vesting receivers
std::set<std::string> vestingAddresses;
//! Insurance fund instance
std::unique_ptr<InsuranceFund> g_fund;
//! Last unit price for token/BTC
std::map<uint32_t, int64_t> lastPrice;

std::map<std::string, int64_t> sum_upnls;

std::map<uint32_t, std::map<uint32_t, int64_t>> market_priceMap;

//!Used to indicate, whether to automatically commit created transactions.
bool autoCommit = true;

static fs::path MPPersistencePath;
static int mastercoreInitialized = 0;
static int reorgRecoveryMode = 0;
static int reorgRecoveryMaxHeight = 0;

int idx_expiration;
int expirationAchieve;
double globalPNLALL_DUSD;
int64_t globalVolumeALL_DUSD;
int lastBlockg;
int twapBlockg;
int64_t globalVolumeALL_LTC = 0;
int n_rows = 0;
int idx_q;
unsigned int path_length;

CMPTxList *mastercore::p_txlistdb;
CMPTradeList *mastercore::t_tradelistdb;
CtlTransactionDB *mastercore::p_TradeTXDB;
OfferMap mastercore::my_offers;
AcceptMap mastercore::my_accepts;
CMPSPInfo *mastercore::_my_sps;
CDInfo *mastercore::_my_cds;

//node reward object
nodeReward nR;
//settlement object
blocksettlement bS;

std::map<uint32_t, std::map<uint32_t, int64_t>> VWAPMap;
std::map<uint32_t, std::map<uint32_t, int64_t>> VWAPMapSubVector;
std::map<uint32_t, std::map<uint32_t, std::vector<int64_t>>> numVWAPVector;
std::map<uint32_t, std::map<uint32_t, std::vector<int64_t>>> denVWAPVector;
std::map<uint32_t, std::vector<int64_t>> mapContractAmountTimesPrice;
std::map<uint32_t, std::vector<int64_t>> mapContractVolume;
std::map<uint32_t, int64_t> VWAPMapContracts;
std::vector<std::map<std::string, std::string>> path_ele;
std::vector<std::map<std::string, std::string>> path_elef;

std::map<uint32_t, std::vector<uint64_t>> cdextwap_ele;
std::map<uint32_t, std::vector<uint64_t>> cdextwap_vec;
std::map<uint32_t, std::map<uint32_t, std::vector<uint64_t>>> mdextwap_ele;
std::map<uint32_t, std::map<uint32_t, std::vector<uint64_t>>> mdextwap_vec;

std::map<uint32_t, std::map<std::string, double>> addrs_upnlc;

using mastercore::StrToInt64;

// indicate whether persistence is enabled at this point, or not
// used to write/read files, for breakout mode, debugging, etc.
static bool writePersistence(int block_now)
{
  // if too far away from the top -- do not write
  if (GetHeight() > (block_now + MAX_STATE_HISTORY)
          && (block_now % STORE_EVERY_N_BLOCK != 0)) {
      return false;
  }

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
	case TL_PROPERTY_BTC: str = "BTC";
	  break;
	case TL_PROPERTY_ALL: str = "ALL";
	  break;
	case TL_PROPERTY_TALL: str = "TALL";
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

/*New property type No 3 Contract*/
std::string mastercore::FormatContractMP(int64_t n)
{
    return strprintf("%d", n);
}

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

// this is the master list of all amounts for all addresses for all properties, map is unsorted
std::unordered_map<std::string, CMPTally> mastercore::mp_tally_map;

CMPTally* mastercore::getTally(const std::string& address)
{
    std::unordered_map<std::string, CMPTally>::iterator it = mp_tally_map.find(address);
    if (it != mp_tally_map.end()) return &(it->second);
    return static_cast<CMPTally*>(nullptr);
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


std::string mastercore::getTokenLabel(uint32_t propertyId)
{
  std::string tokenStr;
  if (propertyId < 3) {
      tokenStr = (propertyId == 1) ? " ALL" : " TALL";
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
  if (! _my_sps->getSP(propertyId, property)) {
    return 0; // property ID does not exist
  }

  if (!property.fixed || n_owners_total) {
    for (std::unordered_map<std::string, CMPTally>::const_iterator it = mp_tally_map.begin(); it != mp_tally_map.end(); ++it) {
      const CMPTally& tally = it->second;
      totalTokens += tally.getMoney(propertyId, BALANCE);
      totalTokens += tally.getMoney(propertyId, SELLOFFER_RESERVE);
      totalTokens += tally.getMoney(propertyId, ACCEPT_RESERVE);
      totalTokens += tally.getMoney(propertyId, METADEX_RESERVE);
      totalTokens += tally.getMoney(propertyId, CONTRACTDEX_RESERVE); // amount in margin
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

uint32_t mastercore::GetNextPropertyId()
{
    return _my_sps->peekNextSPID();
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
    // PrintToConsole ("saved: s% : %s\n",key,value);
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
    // no balance changes were detected that affect wallet addresses, signal a generic change to overall Trade Layer state
    if (!forceUpdate) {
      // uiInterface.TLStateChanged();
      return;
    }
  }

#ifdef ENABLE_WALLET
    LOCK(cs_tally);

     // balance changes were found in the wallet, update the global totals and signal a balance change
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
    // signal an Trade Layer balance change
    // uiInterface.TLBalanceChanged();
#endif
}


/**
 * Returns the Admin address for vesting tokens.
 *
 * Main network:
 *   ... ? ...
 *
 * Test network:
 *   moCYruRphhYgejzH75bxWD49qRFan8eGES
 *
 * Regtest network:
 *   mgrNNyDCdAWeYfkvcarsQKRzMhEFQiDmnH
 *
 * @return The specific address
 */
const std::string mastercore::getVestingAdmin()
{
    if (RegTest())
    {
        // regtest (private key: cTkpBcU7YzbJBi7U59whwahAMcYwKT78yjZ2zZCbLsCZ32Qp5Wta)
        const std::string regAddress = "QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPb";
        return regAddress;

    } else if (TestNet()) {
        // testnet address
        const std::string testAddress = "QQGwLt5cFRTxMuY9ij6DzTY8H1hJwn6aV4";
        return testAddress;

    }

    const std::string mainAddress = "MK3MqrS7PSkjcdiEyCNB92w74vfAEveZnu";
    return mainAddress;

}

void creatingVestingTokens(int block)
{
   CMPSPInfo::Entry newSP;
   newSP.name = "Vesting Tokens";
   newSP.data = "Divisible Tokens";
   newSP.url  = "www.tradelayer.org";
   newSP.category = "N/A";
   newSP.subcategory = "N/A";
   newSP.prop_type = ALL_PROPERTY_TYPE_DIVISIBLE;
   newSP.num_tokens = totalVesting;
   newSP.attribute_type = ALL_PROPERTY_TYPE_VESTING;
   newSP.init_block = block;
   newSP.issuer = getVestingAdmin();
   newSP.last_vesting = 0;
   newSP.last_vesting_block = 0;

   const uint32_t propertyIdVesting = _my_sps->putSP(newSP);
   assert(propertyIdVesting > 0);

   if(msc_debug_vesting) PrintToLog("%s(): admin_addrs : %s, propertyIdVesting: %d \n",__func__, getVestingAdmin(), propertyIdVesting);


   assert(update_tally_map(getVestingAdmin(), propertyIdVesting, 500000 * COIN, BALANCE));
   assert(update_tally_map(getVestingAdmin(), ALL, 500000 * COIN, UNVESTED));

   std::vector<std::string> investors;
   investors.push_back("MVNp1xa6ZrQoxokyKoB6oT279z4ezV95oM");
   investors.push_back("MVdKutPi53cv9MjMeufHZsuWMqSSnRj8gV");
   investors.push_back("MLZ3w54h27FSVEcV8xbt5T4sqY91x6QCVL");
   investors.push_back("MVYn5iLapDB1snfhBhiMzGvL7HUT9ssjcm");
   investors.push_back("MJ5F3QMDbhNbVNhmKxrQy7MVMQxrGt1muV");

   for (const auto& addr : investors) {
       assert(update_tally_map(addr, propertyIdVesting, 200000 * COIN, BALANCE));
       assert(update_tally_map(addr, ALL, 200000 * COIN, UNVESTED));
   }

   // only for testing
   if (RegTest()) assert(update_tally_map(getVestingAdmin(), ALL, totalVesting, BALANCE));
}

/**
 * Returns the encoding class, used to embed a payload.
 *    Class D (op-return compressed)
 */
int mastercore::GetEncodingClass(const CTransaction& tx, int nBlock)
{
    bool hasOpReturn = false;

    /* Fast Search
     * Perform a string comparison on hex for each scriptPubKey & look directly for Trade Layer marker bytes
     * This allows to drop non- Trade Layer transactions with less work
     */
    std::string strClassD = "7474"; /*the tt marker*/
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

        if (outType == TX_NULL_DATA) {
            // Ensure there is a payload, and the first pushed element equals,
            // or starts with the "tl" marker
            std::vector<std::string> scriptPushes;
            if (!GetScriptPushes(output.scriptPubKey, scriptPushes)) {
                continue;
            }

            if (!scriptPushes.empty()) {
                std::vector<unsigned char> vchMarker = GetTLMarker();
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

        PrintToLog("%s(): HAS OP RETURN!\n",__func__);
        return TL_CLASS_D;
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
static bool FillTxInputCache(const CTransaction& tx, const std::shared_ptr<std::map<COutPoint, Coin>> removedCoins)
{
    static unsigned int nCacheSize = gArgs.GetArg("-tltxcache", 500000);

    if (view.GetCacheSize() > nCacheSize) {
        if(msc_debug_fill_tx_input_cache) PrintToLog("%s(): clearing cache before insertion [size=%d, hit=%d, miss=%d]\n", __func__, view.GetCacheSize(), nCacheHits, nCacheMiss);
        view.Flush();
    }

    for (std::vector<CTxIn>::const_iterator it = tx.vin.begin(); it != tx.vin.end(); ++it)
    {
        const CTxIn& txIn = *it;
        unsigned int nOut = txIn.prevout.n;
        const Coin& coin = view.AccessCoin(txIn.prevout);

        if (!coin.IsSpent())
        {
            if(msc_debug_fill_tx_input_cache) PrintToLog("%s(): coin.IsSpent() == false, nCacheHits: %d \n",__func__, nCacheHits);
            ++nCacheHits;
            continue;
        } else {
            if(msc_debug_fill_tx_input_cache) PrintToLog("%s(): coin.IsSpent() == true, nCacheHits: %d \n",__func__, nCacheHits);
            ++nCacheMiss;
        }

        CTransactionRef txPrev;
        uint256 hashBlock;
        Coin newcoin;
        if (removedCoins && removedCoins->find(txIn.prevout) != removedCoins->end())
        {
            newcoin = removedCoins->find(txIn.prevout)->second;
            if(msc_debug_fill_tx_input_cache) PrintToLog("%s(): newcoin = removedCoins->find(txIn.prevout)->second \n",__func__);
        } else if (GetTransaction(txIn.prevout.hash, txPrev, Params().GetConsensus(), hashBlock, true)) {

            newcoin.out.scriptPubKey = txPrev->vout[nOut].scriptPubKey;
            newcoin.out.nValue = txPrev->vout[nOut].nValue;
            if(msc_debug_fill_tx_input_cache) PrintToLog("%s():GetTransaction == true, nValue: %d\n",__func__, txPrev->vout[nOut].nValue);
            BlockMap::iterator bit = mapBlockIndex.find(hashBlock);
            newcoin.nHeight = bit != mapBlockIndex.end() ? bit->second->nHeight : 1;
        } else {
            if(msc_debug_fill_tx_input_cache) PrintToLog("%s():GetTransaction == false\n",__func__);
            return false;
        }

        if(msc_debug_fill_tx_input_cache) PrintToLog("%s(): nCacheHits: %d, nCacheMiss: %d, nCacheSize: %d\n",__func__, nCacheHits, nCacheMiss, nCacheSize);

        view.AddCoin(txIn.prevout, std::move(newcoin), true);
    }

    return true;
}

// idx is position within the block, 0-based
// int msc_tx_push(const CTransaction &wtx, int nBlock, unsigned int idx)
// INPUT: bRPConly -- set to true to avoid moving funds; to be called from various RPC calls like this
// RETURNS: 0 if parsed a MP TX
// RETURNS: < 0 if a non-MP-TX or invalid
// RETURNS: >0 if 1 or more payments have been made

static int parseTransaction(bool bRPConly, const CTransaction& wtx, int nBlock, unsigned int idx, CMPTransaction& mp_tx, unsigned int nTime, const std::shared_ptr<std::map<COutPoint, Coin>> removedCoins = nullptr)
{
    assert(bRPConly == mp_tx.isRpcOnly());
    mp_tx.Set(wtx.GetHash(), nBlock, idx, nTime);

    // ### CLASS IDENTIFICATION AND MARKER CHECK ###
    int tlClass = GetEncodingClass(wtx, nBlock);

    if (tlClass == NO_MARKER) {
        return -1; // No Trade Layer marker, thus not a valid protocol transaction
    }

    if (!bRPConly || msc_debug_parser_readonly) {
        PrintToLog("____________________________________________________________________________________________________________________________________\n");
        PrintToLog("%s(block=%d, %s idx= %d); txid: %s\n", __FUNCTION__, nBlock, FormatISO8601Date(nTime), idx, wtx.GetHash().GetHex());
    }

    // ### SENDER IDENTIFICATION ###
    std::string strSender;
    int64_t inAll = 0;

    { // needed to ensure the cache isn't cleared in the meantime when doing parallel queries
        LOCK2(cs_main, cs_tx_cache); // cs_main should be locked first to avoid deadlocks with cs_tx_cache at FillTxInputCache(...)->GetTransaction(...)->LOCK(cs_main)

        // Add previous transaction inputs to the cache
        if (!FillTxInputCache(wtx, removedCoins)) {
            PrintToLog("%s() ERROR: failed to get inputs for %s\n", __func__, wtx.GetHash().GetHex());
            return -101;
        }

        assert(view.HaveInputs(wtx));

        // determine the sender, but invalidate transaction, if the input is not accepted
        unsigned int vin_n = 0; // the first input
        if (msc_debug_parser_data) PrintToLog("vin=%d:%s\n", vin_n, ScriptToAsmStr(wtx.vin[vin_n].scriptSig));

        const CTxIn& txIn = wtx.vin[vin_n];
        const Coin& coin = view.AccessCoin(txIn.prevout);
        const CTxOut& txOut = coin.out;

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
            PrintToLog("%s(): strSender: %s \n",__func__, strSender);
        } else return -110;

        inAll = view.GetValueIn(wtx);

    } // end of LOCK(cs_tx_cache)

    int64_t outAll = wtx.GetValueOut();
    int64_t txFee = inAll - outAll; // miner fee

    if (!strSender.empty()) {
        if (msc_debug_verbose) PrintToLog("The Sender: %s : fee= %s\n", strSender, FormatDivisibleMP(txFee));
    } else {
        PrintToLog("%s: The sender is still EMPTY !!! txid: %s\n", __func__, wtx.GetHash().GetHex());
        return -5;
    }

    // ### DATA POPULATION ### - save output addresses, values and scripts
    std::string strReference, spReference;
    unsigned char single_pkt[65535];
    unsigned int packet_size = 0;
    std::vector<std::string> script_data;
    std::vector<std::string> address_data;
    std::vector<int64_t> value_data;

    for (unsigned int n = 0; n < wtx.vout.size(); ++n)
    {
        txnouttype whichType;
        if (!GetOutputType(wtx.vout[n].scriptPubKey, whichType)) {
            continue;
        }

        if (!IsAllowedOutputType(whichType, nBlock)) {
            continue;
        }

        CTxDestination dest;
        if (ExtractDestination(wtx.vout[n].scriptPubKey, dest))
        {
            std::string address = EncodeDestination(dest);
            // saving for Class A processing or reference
            GetScriptPushes(wtx.vout[n].scriptPubKey, script_data);
            address_data.push_back(address);
            value_data.push_back(wtx.vout[n].nValue);
            if (msc_debug_parser_data) PrintToLog("saving address_data #%d: %s:%s\n", n, address, ScriptToAsmStr(wtx.vout[n].scriptPubKey));
        }
    }

    if (msc_debug_parser_data) PrintToLog(" address_data.size=%lu\n script_data.size=%lu\n value_data.size=%lu\n", address_data.size(), script_data.size(), value_data.size());

    // ### CLASS D PARSING ###
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
                if (k > 0) spReference = address_data[k-1]; // the idea here is take first ref and second ref then.
                strReference = addr; // this may be set several times, but last time will be highest vout
                if (msc_debug_parser_data) PrintToLog("Resetting spReference : %s, and strReference as follows: %s \n ", spReference, strReference);
            }
        }
    }

    if (msc_debug_parser_data) PrintToLog("Ending reference identification\nFinal decision on reference identification is: %s\n", strReference);

    // ### CLASS D SPECIFIC PARSING ###
    if (tlClass == TL_CLASS_D) {
        std::vector<std::string> op_return_script_data;

        // ### POPULATE OP RETURN SCRIPT DATA ###
        for (unsigned int n = 0; n < wtx.vout.size(); ++n)
        {
            txnouttype whichType;
            if (!GetOutputType(wtx.vout[n].scriptPubKey, whichType)) {
                continue;
            }

            if (!IsAllowedOutputType(whichType, nBlock)) {
                continue;
            }

            if (whichType == TX_NULL_DATA)
            {
                // only consider outputs, which are explicitly tagged
                std::vector<std::string> vstrPushes;
                if (!GetScriptPushes(wtx.vout[n].scriptPubKey, vstrPushes)) {
                    continue;
                }
                // TODO: maybe encapsulate the following sort of messy code
                if (!vstrPushes.empty()) {
                    std::vector<unsigned char> vchMarker = GetTLMarker();
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
                            PrintToLog("Class D transaction detected: %s parsed to %s at vout %d\n", wtx.GetHash().GetHex(), vstrPushes[0], n);
                        }
                    }
                }
            }
        }

        // ### EXTRACT PAYLOAD FOR CLASS D ###
        for (unsigned int n = 0; n < op_return_script_data.size(); ++n)
        {
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
    mp_tx.Set(strSender, strReference, spReference, 0, wtx.GetHash(), nBlock, idx, (unsigned char *)&single_pkt, packet_size, tlClass, txFee);
    if (address_data.size() > 1) {

        // we need to erase here strSender
        auto it = find(address_data.begin(), address_data.end(), strSender);
        if (it != address_data.end()) {
            address_data.erase(it);
        }


        auto v = std::vector<std::string>(address_data.cbegin(), address_data.cend());
        mp_tx.SetAddresses(v);
    }

    PrintToLog("%s(): mp_tx object: strSender: %s, spReference: %s, strReference: %s, single_pkt: %s, tlClass: %d \n",__func__, strSender, spReference, strReference, HexStr(single_pkt, packet_size + single_pkt), tlClass);

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
     int64_t nvalue = 0;
     int count = 0;

     for (unsigned int n = 0; n < tx.vout.size(); ++n)
     {
         CTxDestination dest;
         if (ExtractDestination(tx.vout[n].scriptPubKey, dest))
         {
             const std::string address = EncodeDestination(dest);

             if (msc_debug_handle_dex_payment) PrintToLog("%s: destination address: %s, sender's address: %s \n", __func__, address, strSender);

             if (address == strSender) {
                  continue;
             }

             if (msc_debug_handle_dex_payment) PrintToLog("payment #%d %s %s\n", count, address, FormatIndivisibleMP(tx.vout[n].nValue));

             nvalue = tx.vout[n].nValue;
             // check everything and pay BTC for the property we are buying here...
             if (0 == DEx_payment(tx.GetHash(), n, address, strSender, nvalue, nBlock)) ++count;
         }
     }

     /** Adding LTC into volume */
     if (count > 0)
     {
         globalVolumeALL_LTC += nvalue;
         if (msc_debug_handle_dex_payment) PrintToLog("%s(): nvalue: %d, globalVolumeALL_LTC: %d \n",__func__, nvalue, globalVolumeALL_LTC);

     }

     return (count > 0);
 }

static bool Instant_payment(const uint256& txid, const std::string& buyer, const std::string& seller, const std::string sender, uint32_t property, uint64_t amount_forsale, uint64_t nvalue, uint64_t price, int block, int idx)
{
    bool status = false;

    if(msc_debug_instant_payment) PrintToLog("%s(): buyer : %s, seller : %s, sender : %s, property : %d, amount_forsale : %d, nvalue : %d, price : %d, block: %d, idx : %d\n",__func__, buyer, seller, sender, property, amount_forsale, nvalue, price, block, idx);

    const arith_uint256 amount_forsale256 = ConvertTo256(amount_forsale);
    const arith_uint256 amountLTC_Desired256 = ConvertTo256(price);
    const arith_uint256 amountLTC_Paid256 = (nvalue > price) ? amountLTC_Desired256 : ConvertTo256(nvalue);

    // actual calculation; round up
    const arith_uint256 amountPurchased256 = DivideAndRoundUp((amountLTC_Paid256 * amount_forsale256), amountLTC_Desired256);
    // convert back to int64_t
    int64_t amount_purchased = ConvertTo64(amountPurchased256);

    if(msc_debug_instant_payment) PrintToLog("%s(): amount_purchased: %d\n",__func__, amount_purchased);


    if(msc_debug_instant_payment) PrintToLog("%s(): channelAddr: %s\n",__func__, sender);

    // retrieving channel struct
    auto it = channels_Map.find(sender);
    if (it == channels_Map.end()){
        PrintToLog("%s(): channel not found!\n",__func__);
        return false;
    }

    Channel& sChn = it->second;

    // adding buyer to channel if it wasn't added before
    if(!sChn.isPartOfChannel(buyer) && sChn.getSecond() == CHANNEL_PENDING){
        sChn.setSecond(buyer);
    }

    const int64_t remaining = sChn.getRemaining(seller, property);

    if(msc_debug_instant_payment)
    {
        PrintToLog("%s(): seller : %s, buyer: %s, channelAddr : %s, amount_purchased: %d\n",__func__, seller, buyer, sender, amount_purchased);
        PrintToLog("%s(): remaining amount for seller (%s) : %d\n",__func__, seller, remaining);
    }

    if (remaining < amount_purchased)
    {
          PrintToLog("%s(): not enough tokens in trade channel, seller remaining( %s) : %d\n",__func__, seller, remaining);
          return status;
    }

    // copy of amount purchased without fees.
    const int64_t selleramount = amount_purchased;

    // taking fees
    Token_LTC_Fees(amount_purchased, property);

    if(0 == amount_purchased)
    {
        PrintToLog("%s(): amount purchased is zero\n",__func__);
        return status;
    }

    assert(update_tally_map(buyer, property, amount_purchased, BALANCE));
    assert(sChn.updateChannelBal(seller, property, -selleramount));

    p_txlistdb->recordNewInstantLTCTrade(txid, sender, seller , buyer, property, amount_purchased, price, block, idx);

    // saving DEx token volume
    MapTokenVolume[block][property] += amount_purchased;

    const arith_uint256 unitPrice256 = (isPropertyDivisible(property)) ? (ConvertTo256(COIN) * amountLTC_Desired256) / amount_forsale256 : amountLTC_Desired256 / amount_forsale256;

    const int64_t unitPrice = ConvertTo64(unitPrice256);

    // adding last price
    lastPrice[property] = unitPrice;

    // adding numerator of vwap
    tokenvwap[property][block].push_back(std::make_pair(unitPrice, nvalue));

    // adding LTC volume to map
    MapLTCVolume[block][property] += nvalue;

    // adding LTC to global
    globalVolumeALL_LTC += nvalue;

    // updating last exchange block
    // assert(sChn.updateLastExBlock(block));

    return !status;

 }

 // special address is the change address.
 static bool HandleLtcInstantTrade(const CTransaction& tx, int nBlock, int idx, const std::string& sender, const std::string& special, const std::string& receiver, uint32_t property, uint64_t amount_forsale, uint64_t price)
 {
     bool count = false;
     uint64_t nvalue = 0;
     const unsigned int vSize = tx.vout.size();

     if (msc_debug_handle_instant) PrintToLog("%s(): nBlock : %d, sender : %s, special: %s, receiver : %s, property : %d, amount_forsale : %d, price : %d, tx.vout.size(): %d\n",__func__, nBlock, sender, special, receiver, property, amount_forsale, price, tx.vout.size());

     for (unsigned int n = 0; n < vSize ; ++n)
     {
         CTxDestination dest;
         if (ExtractDestination(tx.vout[n].scriptPubKey, dest))
         {
             const std::string address = EncodeDestination(dest);

             if(address != receiver){
                 continue;
             }

             if (msc_debug_handle_instant) PrintToLog("%s(): destination address: %s, dest address: %s\n", __func__, address, receiver);

             nvalue += tx.vout[n].nValue;

         }
     }

     if (msc_debug_handle_instant) PrintToLog("%s(): nvalue : %d, price: %d \n",__func__, nvalue, price);

     if (nvalue > 0)
     {
         if (msc_debug_handle_instant) PrintToLog("%s: litecoins found..., receiver address: %s, litecoin amount: %d\n", __func__, receiver, nvalue);
         // function to update tally in order to paid litecoins

         if (Instant_payment(tx.GetHash(), special, receiver, sender, property, amount_forsale, nvalue, price, nBlock, idx)) count = true;
     }

     if (count)
     {
         if (msc_debug_handle_instant) PrintToLog("%s: Successfully litecoins traded \n", __func__);

         /** Adding LTC into volume **/
         globalVolumeALL_LTC += nvalue;
         const int64_t globalVolumeALL_LTCh = globalVolumeALL_LTC;

         if (msc_debug_handle_instant) PrintToLog("%s(): nvalue: %d, globalVolumeALL_LTC: %d \n",__func__, nvalue, globalVolumeALL_LTCh);

     } else {
         PrintToLog("%s(): ERROR: instant ltc payment failed \n",__func__);
     }

     return count;
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
         int64_t minutes = (secondsTotal / 60) % 60;
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
     {}

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
                 "Still scanning.. at block %d of %d. Progress: %.2f percent, about %s remaining..\n",
                 nCurrentBlock, nLastBlock, dProgress, remainingTimeAsString(nRemainingTime));
         std::string strProgressUI = strprintf(
                 "Still scanning.. at block %d of %d.\nProgress: %.2f percent, (about %s remaining)",
                 nCurrentBlock, nLastBlock, dProgress, remainingTimeAsString(nRemainingTime));

         PrintToLog(strProgress);
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
    int nTimeBetweenProgressReports = gArgs.GetArg("-tlprogressfrequency", 30);  // seconds
    int64_t nNow = GetTime();
    unsigned int nTxsTotal = 0;
    unsigned int nTxsFoundTotal = 0;
    int nBlock = 9999999;
    const int nLastBlock = GetHeight();

    // this function is useless if there are not enough blocks in the blockchain yet!
    if (nFirstBlock < 0 || nLastBlock < nFirstBlock){
        return -1;
    }

    // used to print the progress to the console and notifies the UI
    ProgressReporter progressReporter(chainActive[nFirstBlock], chainActive[nLastBlock]);

    for (nBlock = nFirstBlock; nBlock <= nLastBlock; ++nBlock)
    {
        if (ShutdownRequested()) {
            PrintToLog("Shutdown requested, stop scan at block %d of %d\n", nBlock, nLastBlock);
            break;
        }

        CBlockIndex* pblockindex = chainActive[nBlock];
        if (nullptr == pblockindex) break;

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
          for(const CTransactionRef& tx : block.vtx) {
             if (mastercore_handler_tx(*tx, nBlock, nTxNum, pblockindex, nullptr)) ++nTxsFoundInBlock;
             ++nTxNum;
          }

        nTxsFoundTotal += nTxsFoundInBlock;
        nTxsTotal += nTxNum;
        mastercore_handler_block_end(nBlock, pblockindex, nTxsFoundInBlock);
    }

    if (nBlock < nLastBlock) {
        PrintToLog("Scan stopped early at block %d of block %d\n", nBlock, nLastBlock);
    }

    PrintToLog("%d transactions processed, %d meta transactions found\n", nTxsTotal, nTxsFoundTotal);

    return 0;
}

static int input_msc_balances_string(const std::string& s)
{
    // "address=propertybalancedata"
    std::vector<std::string> addrData;
    boost::split(addrData, s, boost::is_any_of("="), boost::token_compress_on);
    if (addrData.size() != 2) {
        return -1;
    }

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

        std::vector<std::string> curBalance;
        boost::split(curBalance, curProperty[1], boost::is_any_of(","), boost::token_compress_on);

        if (curBalance.size() != 7) return -1;

        uint32_t propertyId = 0;
        int64_t balance = 0;
        int64_t sellReserved = 0;
        int64_t acceptReserved = 0;
        int64_t pending = 0;
        int64_t metadexReserved = 0;
        int64_t contractdexReserved = 0;
        int64_t unvested = 0;

        try {
            propertyId = boost::lexical_cast<uint32_t>(curProperty[0]);
            balance = boost::lexical_cast<int64_t>(curBalance[0]);
            sellReserved = boost::lexical_cast<int64_t>(curBalance[1]);
            acceptReserved = boost::lexical_cast<int64_t>(curBalance[2]);
            pending = boost::lexical_cast<int64_t>(curBalance[3]);
            metadexReserved = boost::lexical_cast<int64_t>(curBalance[4]);
            contractdexReserved = boost::lexical_cast<int64_t>(curBalance[5]);
            unvested = boost::lexical_cast<int64_t>(curBalance[6]);
        } catch (...) {
            PrintToLog("%s(): lexical_cast issue \n",__func__);
            return -1;
        }

        if (balance) update_tally_map(strAddress, propertyId, balance, BALANCE);
        if (sellReserved) update_tally_map(strAddress, propertyId, sellReserved, SELLOFFER_RESERVE);
        if (acceptReserved) update_tally_map(strAddress, propertyId, acceptReserved, ACCEPT_RESERVE);
        if (pending) update_tally_map(strAddress, propertyId, pending, PENDING);
        if (metadexReserved) update_tally_map(strAddress, propertyId, metadexReserved, METADEX_RESERVE);

        if (contractdexReserved) update_tally_map(strAddress, propertyId, contractdexReserved, CONTRACTDEX_RESERVE);

        if (unvested) update_tally_map(strAddress, propertyId, unvested, UNVESTED);
    }

    return 0;
}

static int input_mp_offers_string(const std::string& s)
{
    std::vector<std::string> vstr;
    boost::split(vstr, s, boost::is_any_of(" ,="), boost::token_compress_on);

    if (10 != vstr.size()) return -1;

    int i = 0;
    int offerBlock = 0;
    int64_t amountOriginal = 0;
    uint32_t prop = 0;
    int64_t btcDesired = 0;
    int64_t minFee = 0;
    uint8_t blocktimelimit = 0;
    uint256 txid;
    uint8_t subaction = 0;
    uint8_t option = 0;

    const std::string &sellerAddr = vstr[i++];

    try {
        offerBlock = boost::lexical_cast<int>(vstr[i++]);
        amountOriginal = boost::lexical_cast<int64_t>(vstr[i++]);
        prop = boost::lexical_cast<uint32_t>(vstr[i++]);
        btcDesired = boost::lexical_cast<int64_t>(vstr[i++]);
        minFee = boost::lexical_cast<int64_t>(vstr[i++]);
        blocktimelimit = boost::lexical_cast<unsigned int>(vstr[i++]); // lexical_cast can't handle char!
        txid = uint256S(vstr[i++]);
        subaction = boost::lexical_cast<unsigned int>(vstr[i++]);
        option = boost::lexical_cast<unsigned int>(vstr[i++]);
    } catch (...) {
        PrintToLog("%s(): lexical_cast issue \n",__func__);
        return -1;
    }

    const std::string combo = STR_SELLOFFER_ADDR_PROP_COMBO(sellerAddr, prop);
    CMPOffer newOffer(offerBlock, amountOriginal, prop, btcDesired, minFee, blocktimelimit, txid, subaction, option);

    if (!my_offers.insert(std::make_pair(combo, newOffer)).second) return -1;

    return 0;
}

// seller-address, property, buyer-address, amount, fee, block
// 13z1JFtDMGTYQvtMq5gs4LmCztK3rmEZga,1, 148EFCFXbk2LrUhEHDfs9y3A5dJ4tttKVd,100000,11000,299126
static int input_mp_accepts_string(const std::string& s)
{
    std::vector<std::string> vstr;
    boost::split(vstr, s, boost::is_any_of(" ,="), boost::token_compress_on);
    int i = 0;

    if (10 != vstr.size()) return -1;

    unsigned int prop = 0;
    std::string buyerAddr;
    int nBlock = 0;
    int64_t amountRemaining = 0;
    int64_t amountOriginal = 0;
    unsigned char blocktimelimit = 0;
    int64_t offerOriginal = 0;
    int64_t btcDesired = 0;
    std::string txidStr;


    const std::string &sellerAddr = vstr[i++];

    try {
        prop = boost::lexical_cast<unsigned int>(vstr[i++]);
        buyerAddr = vstr[i++];
        nBlock = atoi(vstr[i++]);
        amountRemaining = boost::lexical_cast<int64_t>(vstr[i++]);
        amountOriginal = boost::lexical_cast<uint64_t>(vstr[i++]);
        blocktimelimit = atoi(vstr[i++]);
        offerOriginal = boost::lexical_cast<int64_t>(vstr[i++]);
        btcDesired = boost::lexical_cast<int64_t>(vstr[i++]);
        txidStr = vstr[i++];
    } catch (...) {
        PrintToLog("%s(): lexical_cast issue \n",__func__);
        return -1;
    }

    const std::string combo = STR_ACCEPT_ADDR_PROP_ADDR_COMBO(sellerAddr, buyerAddr, prop);
    CMPAccept newAccept(amountOriginal, amountRemaining, nBlock, blocktimelimit, prop, offerOriginal, btcDesired, uint256S(txidStr));
    if (my_accepts.insert(std::make_pair(combo, newAccept)).second) {
        return 0;
    } else {
        return -1;
    }
}

static int input_globals_state_string(const string &s)
{
  std::vector<std::string> vstr;
  boost::split(vstr, s, boost::is_any_of(" ,="), token_compress_on);
  if (1 != vstr.size()) return -1;

  int i = 0;
  unsigned int nextSPID = 0;

  try
  {
      nextSPID = boost::lexical_cast<unsigned int>(vstr[i++]);
  } catch (...) {
        PrintToLog("%s(): lexical_cast issue!\n",__func__);
  }

  _my_sps->init(nextSPID);

  return 0;
}

static int input_contract_globals_state_string(const string &s)
{
  std::vector<std::string> vstr;
  boost::split(vstr, s, boost::is_any_of(" ,="), token_compress_on);

  if (1 != vstr.size()) {
    return -1;
  }

  int i = 0;
  unsigned int nextCDID = 0;

  try {
      nextCDID = boost::lexical_cast<unsigned int>(vstr[i++]);
  } catch (...) {
        PrintToLog("%s(): lexical_cast issue!\n",__func__);
        return -1;
  }

  _my_cds->init(nextCDID);
  return 0;
}

static int input_global_vars_string(const string &s)
{
  std::vector<std::string> vstr;
  boost::split(vstr, s, boost::is_any_of(" ,="), token_compress_on);
  if (1 != vstr.size()) return -1;

  int i = 0;
  int64_t lastVolume = 0;

  try {
      lastVolume = boost::lexical_cast<int64_t>(vstr[i++]);
  } catch (...) {
      PrintToLog("%s(): lexical_cast issue!\n",__func__);
      return -1;
  }

  globalVolumeALL_LTC = lastVolume;

  return 0;
}

static int input_mp_contractdexorder_string(const std::string& s)
{
    std::vector<std::string> vstr;
    boost::split(vstr, s, boost::is_any_of(" ,="), boost::token_compress_on);

    if (13 != vstr.size()) return -1;

    int i = 0;

    int block = 0;
    int64_t amount_forsale = 0;
    uint32_t property = 0;
    int64_t amount_desired = 0;
    uint32_t desired_property = 0;
    uint8_t subaction = 0;
    unsigned int idx = 0;
    uint256 txid;
    int64_t amount_remaining = 0;
    uint64_t effective_price = 0;
    uint8_t trading_action = 0;
    int64_t amount_reserved = 0;

    const std::string addr = vstr[i++];
    try {
        block = boost::lexical_cast<int>(vstr[i++]);
        amount_forsale = boost::lexical_cast<int64_t>(vstr[i++]);
        property = boost::lexical_cast<uint32_t>(vstr[i++]);
        amount_desired = boost::lexical_cast<int64_t>(vstr[i++]);
        desired_property = boost::lexical_cast<uint32_t>(vstr[i++]);
        subaction = boost::lexical_cast<unsigned int>(vstr[i++]); // lexical_cast can't handle char!
        idx = boost::lexical_cast<unsigned int>(vstr[i++]);
        txid = uint256S(vstr[i++]);
        amount_remaining = boost::lexical_cast<int64_t>(vstr[i++]);
        effective_price = boost::lexical_cast<uint64_t>(vstr[i++]);
        trading_action = boost::lexical_cast<unsigned int>(vstr[i++]);
        amount_reserved = boost::lexical_cast<int64_t>(vstr[i++]);

    } catch (...) {
        PrintToLog("%s(): lexical_cast issue \n",__func__);
        return -1;
    }

    CMPContractDex mdexObj(addr, block, property, amount_forsale, desired_property,
            amount_desired, txid, idx, subaction, amount_remaining, effective_price, trading_action, amount_reserved);

    if (!ContractDex_INSERT(mdexObj)) return -1;

    return 0;
}

static int input_mp_token_ltc_string(const std::string& s)
{
   std::vector<std::string> vstr;
   boost::split(vstr, s, boost::is_any_of(" ,="), boost::token_compress_on);

   uint32_t propertyId = 0;
   int64_t price = 0;

   try {
     propertyId = boost::lexical_cast<uint32_t>(vstr[0]);
     price = boost::lexical_cast<int64_t>(vstr[1]);
   } catch (...) {
       PrintToLog("%s(): lexical_cast issue \n",__func__);
       return -1;
   }



   if (!lastPrice.insert(std::make_pair(propertyId, price)).second) return -1;

   return 0;
}


static int input_cachefees_string(const std::string& s)
{
    auto t = NC::Parse<uint32_t,int64_t,int64_t,int64_t>(s);
    
    // format:{pid:native,oracle,spot}
    auto key= std::get<0>(t);
    auto b1 = std::get<1>(t);
    auto b2 = std::get<2>(t);
    auto b3 = std::get<3>(t);

    if (b1 > -1) g_fees->native_fees.insert(std::make_pair(key, b1));
    if (b2 > -1) g_fees->oracle_fees.insert(std::make_pair(key, b2));
    if (b3 > -1) g_fees->spot_fees.insert(std::make_pair(key, b3));

   return 0;
}

// static int input_cachefees_oracles_string(const std::string& s)
// {
//    std::vector<std::string> vstr;
//    boost::split(vstr, s, boost::is_any_of(" ,="), boost::token_compress_on);
//    uint32_t propertyId = 0;
//    int64_t amount = 0;

//    try {
//        propertyId = boost::lexical_cast<uint32_t>(vstr[0]);
//        amount = boost::lexical_cast<int64_t>(vstr[1]);
//    } catch (...) {
//        PrintToLog("%s(): lexical_cast issue \n",__func__);
//        return -1;
//    }


//    if (!g_fees->oracle_fees.insert(std::make_pair(propertyId, amount)).second) return -1;

//    return 0;
// }

static int input_withdrawals_string(const std::string& s)
{
    std::vector<std::string> vstr;
    boost::split(vstr, s, boost::is_any_of(" ,="), boost::token_compress_on);

    withdrawalAccepted w;

    const std::string& chnAddr = vstr[0];
    w.address = vstr[1];

    try {
         w.deadline_block = boost::lexical_cast<int>(vstr[2]);
         w.propertyId = boost::lexical_cast<uint32_t>(vstr[3]);
         w.amount = boost::lexical_cast<int64_t>(vstr[4]);
         w.txid = uint256S(vstr[5]);
    } catch (...) {
        PrintToLog("%s(): lexical_cast issue \n",__func__);
        return -1;
    }

    auto it = withdrawal_Map.find(chnAddr);

    // channel found !
    if(it != withdrawal_Map.end())
    {
        vector<withdrawalAccepted>& whAc = it->second;
        whAc.push_back(w);
        return 0;
    }

    // if there's no channel yet
    vector<withdrawalAccepted> whAcc;
    whAcc.push_back(w);
    if(!withdrawal_Map.insert(std::make_pair(chnAddr,whAcc)).second) return -1;

    return 0;

}

static int input_tokenvwap_string(const std::string& s)
{

  PrintToLog("%s(): s: %s\n",__func__,s);

  std::vector<std::string> vstr;
  boost::split(vstr, s, boost::is_any_of(":"), boost::token_compress_on);

 if (4 != vstr.size()) return -1;

// 4:2046870:100000000000:100000000000


  try {
       const uint32_t propertyId = boost::lexical_cast<uint32_t>(vstr[0]);
       const int block = boost::lexical_cast<int>(vstr[1]);
       const int64_t unitPrice = boost::lexical_cast<int64_t>(vstr[2]);
       const int64_t amount = boost::lexical_cast<int64_t>(vstr[3]);
       tokenvwap[propertyId][block].push_back(std::make_pair(unitPrice, amount));
       PrintToLog("%s(): propertyId = %d, block = %d\n",__func__, propertyId, block);
  } catch(...) {
       PrintToLog("%s(): lexical_cast issue \n",__func__);
       return -1;
  }


  // std::vector<std::string> vpair;
  // boost::split(vpair, vstr[2], boost::is_any_of(","), boost::token_compress_on);
  //
  // for (const auto& np : vpair)
  // {
  //     PrintToLog("%s(): np: %s\n",__func__, np);
  //
  //     std::vector<std::string> pAmount;
  //     boost::split(pAmount, np, boost::is_any_of(":"), boost::token_compress_on);
  //
  //     try {
  //
  //     } catch(...) {
  //        PrintToLog("%s(): lexical_cast issue (2)\n",__func__);
  //        return -1;
  //     }
  //
  // }

  return 0;
}

static int input_activechannels_string(const std::string& s)
{
    std::vector<std::string> vstr;
    boost::split(vstr, s, boost::is_any_of(" ,=+"), boost::token_compress_on);

    int last_exchange_block = 0;
    const std::string& chnAddr = vstr[0];

    try {
        last_exchange_block = boost::lexical_cast<int>(vstr[4]);
    } catch (...) {
       PrintToLog("%s(): lexical_cast issue (1)\n",__func__);
       return -1;
    }

    Channel chn(vstr[1], vstr[2], vstr[3], last_exchange_block);
    // split general data + balances
    std::vector<std::string> vBalance;
    boost::split(vBalance, vstr[5], boost::is_any_of(";"), boost::token_compress_on);

    //address-property:amount;
    for (const auto &v : vBalance)
    {
        //property:amount
        std::vector<std::string> adReg;
        boost::split(adReg, v, boost::is_any_of("-"), boost::token_compress_on);
        const std::string& address = adReg[0];
        std::vector<std::string> reg;
        boost::split(reg, adReg[1], boost::is_any_of(":"), boost::token_compress_on);
        try {
            const uint32_t property = boost::lexical_cast<uint32_t>(reg[0]);
            const int64_t amount = boost::lexical_cast<int64_t>(reg[1]);
            chn.setBalance(address, property, amount);
        } catch (...) {
             PrintToLog("%s(): lexical_cast issue (2)\n",__func__);
             return -1;
        }

    }

    // //inserting chn into map
    if(!channels_Map.insert(std::make_pair(chnAddr,chn)).second) return -1;

    return 0;

}

static int input_mp_mdexorder_string(const std::string& s)
{
    std::vector<std::string> vstr;
    boost::split(vstr, s, boost::is_any_of(" ,="), boost::token_compress_on);

    if (10 != vstr.size()) return -1;

    int i = 0;
    int block = 0;
    int64_t amount_forsale = 0;
    uint32_t property = 0;
    int64_t amount_desired = 0;
    uint32_t desired_property = 0;
    uint8_t subaction = 0;
    unsigned int idx = 0;
    uint256 txid;
    int64_t amount_remaining = 0;

    const std::string &addr = vstr[i++];

    try {
        block = boost::lexical_cast<int>(vstr[i++]);
        amount_forsale = boost::lexical_cast<int64_t>(vstr[i++]);
        property = boost::lexical_cast<uint32_t>(vstr[i++]);
        amount_desired = boost::lexical_cast<int64_t>(vstr[i++]);
        desired_property = boost::lexical_cast<uint32_t>(vstr[i++]);
        subaction = boost::lexical_cast<unsigned int>(vstr[i++]); // lexical_cast can't handle char!
        idx = boost::lexical_cast<unsigned int>(vstr[i++]);
        txid = uint256S(vstr[i++]);
        amount_remaining = boost::lexical_cast<int64_t>(vstr[i++]);
    } catch (...) {
        PrintToLog("%s(): lexical_cast issue \n",__func__);
        return -1;
    }

    CMPMetaDEx mdexObj(addr, block, property, amount_forsale, desired_property,
            amount_desired, txid, idx, subaction, amount_remaining);

    if (!MetaDEx_INSERT(mdexObj)) return -1;

    return 0;
}

static int input_mp_dexvolume_string(const std::string& s)
{
    PrintToLog("%s(): s: %s\n",__func__,s);

    std::vector<std::string> vstr;
    boost::split(vstr, s, boost::is_any_of(" ,="), boost::token_compress_on);

    int block = 0;
    uint32_t propertyId = 0;
    int64_t amount = 0;

    try {
        block = boost::lexical_cast<int>(vstr[0]);
        propertyId = boost::lexical_cast<uint32_t>(vstr[1]);
        amount = boost::lexical_cast<int64_t>(vstr[2]);
    } catch(...) {
        PrintToLog("%s(): lexical_cast issue \n",__func__);
        return -1;
    }

    auto it = MapTokenVolume.find(block);
    if (it != MapTokenVolume.end()){
        auto pMap = it->second;
        pMap[propertyId] = amount;
        return 0;
    }

    std::map<uint32_t,int64_t> pMapp;
    pMapp[propertyId] = amount;
    if(!MapTokenVolume.insert(std::make_pair(block, pMapp)).second) return -1;

    return 0;

}

static int input_mp_mdexvolume_string(const std::string& s)
{
    std::vector<std::string> vstr;
    boost::split(vstr, s, boost::is_any_of(" ,="), boost::token_compress_on);

    int block = 0;
    uint32_t property = 0;
    int64_t amount = 0;

    try {
        block = boost::lexical_cast<int>(vstr[0]);
        property = boost::lexical_cast<uint32_t>(vstr[1]);
        amount = boost::lexical_cast<int64_t>(vstr[2]);

    } catch (...) {
        PrintToLog("%s(): lexical_cast issue \n",__func__);
        return -1;
    }

    auto it = metavolume.find(block);
    if (it != metavolume.end()){
        auto pMap = it->second;
        pMap[property] = amount;
        return 0;
    }

    std::map<uint32_t, int64_t> pMapp;
    pMapp[property] = amount;

    if(!metavolume.insert(std::make_pair(block, pMapp)).second) return -1;

    return 0;

}

static int input_mp_ltcvolume_string(const std::string& s)
{
    std::vector<std::string> vstr;
    boost::split(vstr, s, boost::is_any_of(" ,="), boost::token_compress_on);

    int block = 0;
    uint32_t property = 0;
    int64_t amount = 0;

    try {
      block = boost::lexical_cast<int>(vstr[0]);
      property = boost::lexical_cast<uint32_t>(vstr[1]);
      amount = boost::lexical_cast<int64_t>(vstr[2]);

    } catch (...) {
        PrintToLog("%s(): lexical_cast issue \n",__func__);
        return -1;
    }

    auto it = MapLTCVolume.find(block);
    if (it != MapLTCVolume.end()){
        auto pMap = it->second;
        pMap[property] = amount;
        return 0;
    }

    std::map<uint32_t, int64_t> pMapp;
    pMapp[property] = amount;

    if(!MapLTCVolume.insert(std::make_pair(block, pMapp)).second) return -1;

    return 0;

}


static int input_vestingaddresses_string(const std::string& s)
{
   std::vector<std::string> vstr;
   size_t elements = vestingAddresses.size();

   boost::split(vstr, s, boost::is_any_of(" ,="), boost::token_compress_on);

   try {
       const std::string& address = vstr[0];
       vestingAddresses.insert(address);

   } catch (...) {
       PrintToLog("%s(): lexical_cast issue \n",__func__);
       return -1;
   }

   return ((vestingAddresses.size() > elements) ? 0 : -1);
}

// contract register persistence here
static int input_register_string(const std::string& s)
{
    // "address=contract_register"
    std::vector<std::string> addrData;
    boost::split(addrData, s, boost::is_any_of("="), boost::token_compress_on);
    if (addrData.size() != 2) return -1;

    std::string strAddress = addrData[0];

    std::vector<std::string> vRegisters;
    boost::split(vRegisters, addrData[1], boost::is_any_of("#"), boost::token_compress_on);

    for (auto iter : vRegisters)
    {
        // spliting full register (records / entries)
        std::vector<std::string> split;
        boost::split(split, iter, boost::is_any_of(";"), boost::token_compress_on);

        // contract id + records
        std::vector<std::string> idRecord;
        boost::split(idRecord, split[0], boost::is_any_of(":"), boost::token_compress_on);

        // just records
        std::vector<std::string> vRecord;
        boost::split(vRecord, idRecord[1], boost::is_any_of(","), boost::token_compress_on);

        uint32_t contractId = 0;
        int64_t entryPrice = 0;
        int64_t position = 0;
        int64_t liquidationPrice = 0;
        int64_t upnl = 0;
        int64_t margin = 0;
        int64_t leverage = 0;

        try {
            contractId = boost::lexical_cast<uint32_t>(idRecord[0]);
            entryPrice = boost::lexical_cast<int64_t>(vRecord[0]);
            position = boost::lexical_cast<int64_t>(vRecord[1]);
            liquidationPrice = boost::lexical_cast<int64_t>(vRecord[2]);
            upnl = boost::lexical_cast<int64_t>(vRecord[3]);
            margin = boost::lexical_cast<int64_t>(vRecord[4]);
            leverage = boost::lexical_cast<int64_t>(vRecord[5]);

        } catch (...) {
            PrintToLog("%s(): lexical_cast issue (1)\n",__func__);
            return -1;
        }

        // PrintToLog("%s(): contractId: %d, entryPrice: %d, position: %d, liqPrice: %d, upnl: %d, margin: %d, leverage: %d\n",__func__, contractId, entryPrice, position, liquidationPrice, upnl, margin, leverage);

        if (entryPrice > 0) update_register_map(strAddress, contractId, entryPrice, ENTRY_CPRICE);
        if (position != 0) update_register_map(strAddress, contractId, position, CONTRACT_POSITION);
        if (liquidationPrice > 0) update_register_map(strAddress, contractId, liquidationPrice, BANKRUPTCY_PRICE);
        if (upnl != 0) update_register_map(strAddress, contractId, upnl, UPNL);
        if (margin > 0) update_register_map(strAddress, contractId, margin, MARGIN);
        if (leverage >= 1) update_register_map(strAddress, contractId, leverage, LEVERAGE);


        // skip if there's only record
        if (split.size() == 1) {
            continue;
        }

        // PrintToLog("%s(): split[1]: %s\n",__func__, split[1]);

        // just entries
        std::vector<std::string> entries;
        boost::split(entries, split[1], boost::is_any_of("|"), boost::token_compress_on);

        for (auto it : entries)
        {
            //contractid, amount
            int64_t amount = 0;
            int64_t price = 0;

            std::vector<std::string> entry;
            boost::split(entry, it, boost::is_any_of(","), boost::token_compress_on);
            try {
                amount = boost::lexical_cast<int64_t>(entry[0]);
                price = boost::lexical_cast<int64_t>(entry[1]);

            } catch (...) {
                PrintToLog("%s(): lexical_cast issue (2)\n",__func__);
                return -1;
            }

            assert(insert_entry(strAddress, contractId, amount, price));

        }

    }

    return 0;
}

static int input_node_reward(const std::string& s)
{
  PrintToLog("%s(): s: %s\n",__func__,s);

  std::vector<std::string> addrData;
  boost::split(addrData, s, boost::is_any_of("+"), boost::token_compress_on);

  if (addrData.size() != 2)
  {
      // address:status
      std::vector<std::string> addrMap;
      boost::split(addrMap, s, boost::is_any_of(":"), boost::token_compress_on);


      if (addrMap.size() != 2) {
          return -1;
      }

      const std::string& address = addrMap[0];
      if (addrMap[1] == "true") {
          PrintToLog("%s(): address: %s\n",__func__, address);
          nR.updateAddressStatus(address, true);

         // we need a number here
      }  else if (addrMap[1] != "false"){

          try
          {
              const int64_t amount = boost::lexical_cast<int64_t>(addrMap[1]);
              nR.addWinner(address, amount);

          } catch (...) {
              PrintToLog("%s(): lexical_cast issue (1)\n",__func__);
              return -1;
          }
      }

  } else {

      int64_t reward = 0;
      int block = 0;

      try
      {
          reward = boost::lexical_cast<int64_t>(addrData[0]);
          block = boost::lexical_cast<int>(addrData[1]);

          PrintToLog("%s(): reward: %d, block: %d\n",__func__, reward, block);

      } catch (...) {
          PrintToLog("%s(): lexical_cast issue (1)\n",__func__);
          return -1;
      }

      nR.setLastReward(reward);
      nR.setLastBlock(block);
  }

  return 0;

}

static int msc_file_load(const string &filename, int what, bool verifyHash = false)
{
  int lines = 0;
  int (*inputLineFunc)(const string &) = nullptr;

  CHash256 hasher;
  switch (what)
  {
    case FILETYPE_BALANCES:
        mp_tally_map.clear();
        inputLineFunc = input_msc_balances_string;
        break;

    case FILETYPE_GLOBALS:
        inputLineFunc = input_globals_state_string;
        break;

    case FILETYPE_CONTRACT_GLOBALS:
        inputLineFunc = input_contract_globals_state_string;
        break;

    case FILETYPE_OFFERS:
        my_offers.clear();
        inputLineFunc = input_mp_offers_string;
        break;

    case FILETYPE_ACCEPTS:
        my_accepts.clear();
        inputLineFunc = input_mp_accepts_string;
        break;

    case FILETYPE_MDEXORDERS:
        // FIXME
        // memory leak ... gotta unallocate inner layers first....
        // TODO
        // ...
        metadex.clear();
        inputLineFunc = input_mp_mdexorder_string;
        break;

    case FILETYPE_CDEXORDERS:
        contractdex.clear();
        inputLineFunc = input_mp_contractdexorder_string;
        break;

    case FILETYPE_CACHEFEES:
        g_fees->native_fees.clear();
        g_fees->oracle_fees.clear();
        g_fees->spot_fees.clear();
        inputLineFunc = input_cachefees_string;
        break;

    // case FILETYPE_CACHEFEES_ORACLES:
    //     g_fees->oracle_fees.clear();
    //     inputLineFunc = input_cachefees_oracles_string;
    //     break;

    case FILETYPE_WITHDRAWALS:
        withdrawal_Map.clear();
        inputLineFunc = input_withdrawals_string;
        break;

    case FILETYPE_ACTIVE_CHANNELS:
        channels_Map.clear();
        inputLineFunc = input_activechannels_string;
        break;

    case FILETYPE_DEX_VOLUME:
        MapTokenVolume.clear();
        inputLineFunc = input_mp_dexvolume_string;
        break;

    case FILETYPE_MDEX_VOLUME:
        metavolume.clear();
        inputLineFunc = input_mp_mdexvolume_string;
        break;

    case FILETYPE_GLOBAL_VARS:
        inputLineFunc = input_global_vars_string;
        break;

    case FILE_TYPE_VESTING_ADDRESSES:
        vestingAddresses.clear();
        inputLineFunc = input_vestingaddresses_string;
        break;

    case FILE_TYPE_LTC_VOLUME:
        MapLTCVolume.clear();
        inputLineFunc = input_mp_ltcvolume_string;
        break;

    case FILE_TYPE_TOKEN_LTC_PRICE:
        lastPrice.clear();
        inputLineFunc = input_mp_token_ltc_string;
        break;

    case FILE_TYPE_TOKEN_VWAP:
        tokenvwap.clear();
        inputLineFunc = input_tokenvwap_string;
        break;

    case FILE_TYPE_REGISTER:
        mp_register_map.clear();
        inputLineFunc = input_register_string;
        break;

    case FILE_TYPE_NODE_ADDRESSES:
        nR.clearNodeRewardMap();
        nR.clearWinners();
        inputLineFunc = input_node_reward;
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

        // add the line to the hash
        if (verifyHash) {
            hasher.Write((unsigned char*)line.c_str(), line.length());
        }

        if (inputLineFunc) {
            if (inputLineFunc(line) < 0) {
                PrintToLog("%s(): inputLineFunc(line) < 0, filename: %s \n",__func__, filename);
                res = -1;
                break;
            }
        }

        ++lines;
    }

    file.close();

    if (verifyHash && res == 0) {
        // generate and write the double hash of all the contents written
        uint256 hash;
        hasher.Finalize(hash.begin());

        if (false == boost::iequals(hash.ToString(), fileHash)) {
            PrintToLog("File %s loaded, but failed hash validation!\n", filename);
            res = -1;
        }
    }

    if (msc_debug_persistence) PrintToLog("%s(%s), loaded lines= %d, res= %d\n", __func__, filename, lines, res);


    return res;
}

static char const * const statePrefix[NUM_FILETYPES] = {
  "balances",
  "globals",
  "cdexorders",
  "mdexorders",
  "offers",
  "accepts",
  "cachefees",
  //"cachefeesoracles",
  "withdrawals",
  "activechannels",
  "dexvolume",
  "mdexvolume",
  "globalvars",
  "vestingaddresses",
  "ltcvolume",
  "tokenltcprice",
  "tokenvwap",
  "register",
  "contractglobals",
  "nodeaddresses"
};

// returns the height of the state loaded
static int load_most_relevant_state()
{
  int res = -1;
  // check the SP  databases and roll it back to its latest valid state
  // according to the active chain
  uint256 spWatermark;
  {
        LOCK(cs_tally);
        if (!_my_sps->getWatermark(spWatermark)) {
            //trigger a full reparse, if the SP database has no watermark
              PrintToLog("%s(): trigger a full reparse, if the SP database has no watermark\n",__func__);
            return -1;
        }
  }

  CBlockIndex const *spBlockIndex = GetBlockIndex(spWatermark);


  // Watermark block not found.
  if (nullptr == spBlockIndex)
  {

      PrintToLog("spWatermark not found: %s\n", spWatermark.ToString());
      // Try and load an historical state
      fs::directory_iterator dIter(MPPersistencePath);
      fs::directory_iterator endIter;
      std::map<int, const CBlockIndex*> foundBlocks;

      for (; dIter != endIter; ++dIter)
      {
          if (false == fs::is_regular_file(dIter->status()) || dIter->path().empty())
          {
              // skip funny business
              continue;
          }

          std::string fName = (*--dIter->path().end()).string();
          std::vector<std::string> vstr;
          boost::split(vstr, fName, boost::is_any_of("-."), boost::token_compress_on);
          if (vstr.size() == 3 && boost::equals(vstr[2], "dat")) {
              uint256 blockHash;
              blockHash.SetHex(vstr[1]);
              CBlockIndex *pBlockIndex = GetBlockIndex(blockHash);
              if (pBlockIndex == nullptr) {
                  continue;
              }

              // Add to found blocks
              foundBlocks.emplace(pBlockIndex->nHeight, pBlockIndex);
          }
      }

      // Was unable to find valid previous state, full reparse required.
      if (foundBlocks.empty()) {
          PrintToLog("Failed to load historical state: watermark isn't a real block\n");
          return -1;
      }

      spBlockIndex = foundBlocks.rbegin()->second;
      _my_sps->setWatermark(spBlockIndex->GetBlockHash());

      PrintToLog("Watermark not found. New one set from state files: %s\n", spBlockIndex->GetBlockHash().ToString());
  }

  PrintToLog("spWatermark found!: %s\n", spWatermark.ToString());

  std::set<uint256> persistedBlocks;
  {
      LOCK2(cs_main, cs_tally);

      while (nullptr != spBlockIndex && false == chainActive.Contains(spBlockIndex))
      {
          int remainingSPs = _my_sps->popBlock(spBlockIndex->GetBlockHash());
          PrintToLog("%s(): first while loop, remainingSPs: %d\n",__func__, remainingSPs);

          if (remainingSPs < 0) {
              // trigger a full reparse, if the levelDB cannot roll back
              PrintToLog("%s(): trigger a full reparse, if the levelDB cannot roll back\n",__func__);
              return -1;
          }

          spBlockIndex = spBlockIndex->pprev;
          if (spBlockIndex != nullptr) {
              _my_sps->setWatermark(spBlockIndex->GetBlockHash());
          }

          PrintToLog("spBlockIndex->GetBlockHash: %s\n", spBlockIndex->GetBlockHash().ToString());
      }

      // prepare a set of available files by block hash pruning any that are
      // not in the active chain
      fs::directory_iterator dIter(MPPersistencePath);
      fs::directory_iterator endIter;
      for (; dIter != endIter; ++dIter)
      {
          if (!fs::is_regular_file(dIter->status()) || dIter->path().empty()) {
              // skip funny business
              continue;
          }

          std::string fName = (*--dIter->path().end()).string();
          std::vector<std::string> vstr;
          boost::split(vstr, fName, boost::is_any_of("-."),  boost::token_compress_on);
          if (vstr.size() == 3 && boost::equals(vstr[2], "dat"))
          {
              uint256 blockHash;
              blockHash.SetHex(vstr[1]);
              CBlockIndex *pBlockIndex = GetBlockIndex(blockHash);
              if (pBlockIndex == nullptr || !chainActive.Contains(pBlockIndex)) {
                  continue;
              }

              // this is a valid block in the active chain, store it
              persistedBlocks.insert(blockHash);
           }
      }

    }

    {
      LOCK(cs_tally);
      // using the SP's watermark after its fixed-up as the tip
      // walk backwards until we find a valid and full set of persisted state files
      // for each block we discard, roll back the SP database
      // Note: to avoid rolling back all the way to the genesis block (which appears as if client is hung) abort after MAX_STATE_HISTORY attempts
      CBlockIndex const *curTip = spBlockIndex;
      int abortRollBackBlock;
      if (curTip != nullptr) {
            abortRollBackBlock = curTip->nHeight - (MAX_STATE_HISTORY + 1);
      }

      while (nullptr != curTip && persistedBlocks.size() > 0 && curTip->nHeight > abortRollBackBlock)
      {
          if (persistedBlocks.find(spBlockIndex->GetBlockHash()) != persistedBlocks.end())
          {
              int success = -1;
              for (int i = 0; i < NUM_FILETYPES; ++i)
              {
                  fs::path path = MPPersistencePath / strprintf("%s-%s.dat", statePrefix[i], curTip->GetBlockHash().ToString());
                  const std::string strFile = path.string();
                  success = msc_file_load(strFile, i, true);
                  if (success < 0) {
                    PrintToLog("Found a state inconsistency at block height %d. "
                            "Reverting up to %d blocks.. this may take a few minutes.\n",
                            curTip->nHeight, (curTip->nHeight - abortRollBackBlock - 1));
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
              PrintToLog("Failed to load historical state: no valid state found after rolling back SP database (2)\n");
              return -1;
          }


          curTip = curTip->pprev;
          if (curTip != nullptr) {
             _my_sps->setWatermark(curTip->GetBlockHash());
          }
      }

  }

  if (persistedBlocks.size() == 0) {
      // trigger a reparse if we exhausted the persistence files without success
      PrintToLog("Failed to load historical state: no valid state found after exhausting persistence files\n");
      return -1;
  }

  // return the height of the block we settled at
  return res;
}

static int write_msc_balances(std::ofstream& file, CHash256& hasher)
{
    std::unordered_map<std::string, CMPTally>::iterator iter;
    for (iter = mp_tally_map.begin(); iter != mp_tally_map.end(); ++iter)
    {
        bool emptyWallet = true;
        std::string lineOut = (*iter).first;
        lineOut.append("=");
        CMPTally& curAddr = (*iter).second;
        curAddr.init();
        uint32_t propertyId = 0;
        while (0 != (propertyId = curAddr.next())) {
            const int64_t balance = (*iter).second.getMoney(propertyId, BALANCE);
            const int64_t sellReserved = (*iter).second.getMoney(propertyId, SELLOFFER_RESERVE);
            const int64_t acceptReserved = (*iter).second.getMoney(propertyId, ACCEPT_RESERVE);
            const int64_t pending = (*iter).second.getMoney(propertyId, PENDING);
            const int64_t metadexReserved = (*iter).second.getMoney(propertyId, METADEX_RESERVE);
            const int64_t contractdexReserved = (*iter).second.getMoney(propertyId, CONTRACTDEX_RESERVE);
            const int64_t unvested = (*iter).second.getMoney(propertyId, UNVESTED);

            // we don't allow 0 balances to read in, so if we don't write them
            // it makes things match up better between persisted state and processed state
            if (0 == balance && 0 == sellReserved && 0 == acceptReserved && 0 == pending && 0 == metadexReserved && contractdexReserved == 0 && unvested == 0) {
                continue;
            }

            emptyWallet = false;

            lineOut.append(strprintf("%d:%d,%d,%d,%d,%d,%d,%d;",
                    propertyId,
                    balance,
                    sellReserved,
                    acceptReserved,
                    pending,
                    metadexReserved,
                    contractdexReserved,
                    unvested));
        }

        if (!emptyWallet) {
            // add the line to the hash
            hasher.Write((unsigned char*)lineOut.c_str(), lineOut.length());

            // write the line
            file << lineOut << std::endl;
        }
    }

    return 0;
}

static int write_globals_state(ofstream &file, CHash256& hasher)
{
    unsigned int nextSPID = _my_sps->peekNextSPID();
    const std::string lineOut = strprintf("%d", nextSPID);

    // add the line to the hash
    hasher.Write((unsigned char*)lineOut.c_str(), lineOut.length());

    // write the line
    file << lineOut << endl;

    return 0;
}


static int write_contract_globals_state(ofstream &file, CHash256& hasher)
{
    unsigned int nextCDID = _my_cds->peekNextContractID();
    const std::string lineOut = strprintf("%d", nextCDID);

    // add the line to the hash
    hasher.Write((unsigned char*)lineOut.c_str(), lineOut.length());

    file << lineOut << endl;

    return 0;
}

static int write_mp_contractdex(ofstream &file, CHash256& hasher)
{
    for (const auto con : contractdex)
    {
        const cd_PricesMap &prices = con.second;

        for (const auto p : prices)
        {
            const cd_Set &indexes = p.second;

            for (const auto in : indexes)
            {

                const CMPContractDex& contract = in;
                contract.saveOffer(file, hasher);
            }
        }
    }

    return 0;
}

static int write_global_vars(ofstream &file, CHash256& hasher)
{
    const int64_t lastVolume = globalVolumeALL_LTC;
    std::string lineOut = strprintf("%d", lastVolume);
    // add the line to the hash
    hasher.Write((unsigned char*)lineOut.c_str(), lineOut.length());

    // write the line
    file << lineOut << endl;

    return 0;
}



static int write_mp_metadex(ofstream &file, CHash256& hasher)
{
    for (const auto my_it : metadex)
    {
        const md_PricesMap & prices = my_it.second;

        for (const auto it : prices)
        {
            const md_Set & indexes = it.second;

            for (const auto in : indexes)
            {
                const CMPMetaDEx& meta = in;
                meta.saveOffer(file, hasher);
            }
        }
    }

    return 0;
}

static int write_mp_offers(ofstream &file, CHash256& hasher)
{
    for (const auto of : my_offers)
    {
        // decompose the key for address
        std::vector<std::string> vstr;
        boost::split(vstr, of.first, boost::is_any_of("-"), token_compress_on);
        CMPOffer const &offer = of.second;
        offer.saveOffer(file, vstr[0], hasher);
    }

    return 0;
}

static int write_mp_accepts(std::ofstream& file,  CHash256& hasher)
{
    for (const auto acc : my_accepts)
    {
        // decompose the key for address
        std::vector<std::string> vstr;
        boost::split(vstr, acc.first, boost::is_any_of("-+"), boost::token_compress_on);
        const CMPAccept& accept = acc.second;
        accept.saveAccept(file, hasher, vstr[0], vstr[2]);
    }

    return 0;
}

static int write_mp_token_ltc_prices(std::ofstream& file, CHash256& hasher)
{
    for (const auto &p : lastPrice)
    {
        const uint32_t& propertyId = p.first;
        const int64_t& price = p.second;

        const std::string lineOut = strprintf("%d,%d",propertyId, price);
        // add the line to the hash
        hasher.Write((unsigned char*)lineOut.c_str(), lineOut.length());
        // write the line
        file << lineOut << endl;
    }

    return 0;
}

static int write_mp_cachefees(std::ofstream& file, CHash256& hasher)
{
    std::set<uint32_t> keys;
    boost::copy(g_fees->native_fees | boost::adaptors::map_keys, std::inserter(keys, keys.begin()));
    boost::copy(g_fees->oracle_fees | boost::adaptors::map_keys, std::inserter(keys, keys.begin()));
    boost::copy(g_fees->spot_fees   | boost::adaptors::map_keys, std::inserter(keys, keys.begin()));

    for (auto k : keys)
    {
        auto b1 = get_fees_balance(g_fees->native_fees, k, -1);
        auto b2 = get_fees_balance(g_fees->oracle_fees, k, -1);
        auto b3 = get_fees_balance(g_fees->spot_fees, k, -1);

        // format:{pid:native,oracle,spot}
        auto e = strprintf("%d;%d,%d,%d", k, b1, b2, b3);
        hasher.Write((unsigned char*)e.c_str(), e.length());
        file << e << endl;
    }

    return 0;
}

static void savingLine(const withdrawalAccepted&  w, const std::string chnAddr, std::ofstream& file,  CHash256& hasher)
{
    const std::string lineOut = strprintf("%s,%s,%d,%d,%d,%s", chnAddr, w.address, w.deadline_block, w.propertyId, w.amount, (w.txid).ToString());
    hasher.Write((unsigned char*)lineOut.c_str(), lineOut.length());
    // write the line
    file << lineOut << std::endl;
}

/** Saving pending withdrawals **/
static int write_mp_withdrawals(std::ofstream& file, CHash256& hasher)
{
    for (const auto w : withdrawal_Map)
    {
        const std::string& chnAddr = w.first;
        const vector<withdrawalAccepted>& whd = w.second;

        for_each(whd.begin(), whd.end(), [&file, &hasher, &chnAddr] (const withdrawalAccepted&  w) { savingLine(w, chnAddr, file, hasher);});

    }

    return 0;
}


/** adding channel balances**/
void addBalances(const std::map<std::string,map<uint32_t, int64_t>>& balances, std::string& lineOut)
{
    for(auto b = balances.begin(); b != balances.end(); ++b)
    {

        if (b != balances.begin()) {
            lineOut.append(strprintf(";"));
        }

        const std::string& address = b->first;
        const auto &pMap = b->second;

        for (auto p = pMap.begin(); p != pMap.end(); ++p){

            if (p != pMap.begin()) {
                lineOut.append(strprintf(";"));
            }

            const uint32_t& property = p->first;
            const int64_t&  amount = p->second;
            lineOut.append(strprintf("%s-%d:%d",address, property, amount));

        }

    }


}

static int write_mp_tokenvwap(std::ofstream& file, CHash256& hasher)
{
    for (const auto &mp : tokenvwap)
    {
        const uint32_t& propertyId = mp.first;
        const auto &blcmap = mp.second;

        for (const auto &vec : blcmap){
            // vector of pairs
            const auto &vpairs = vec.second;

            for (auto p = vpairs.begin(); p != vpairs.end(); ++p)
            {
                std::string lineOut = strprintf("%d:",propertyId);
                // adding block number
                lineOut.append(strprintf("%d:",vec.first));

                const std::pair<int64_t,int64_t>& vwPair = *p;
                lineOut.append(strprintf("%d:%d", vwPair.first, vwPair.second));

                // add the line to the hash
                hasher.Write((unsigned char*)lineOut.c_str(), lineOut.length());
                // write the line
                file << lineOut << std::endl;

            }

        }



    }

    return 0;
}

/**Saving map of active channels**/
static int write_mp_active_channels(std::ofstream& file, CHash256& hasher)
{
    for (const auto &chn : channels_Map)
    {
        // decompose the key for address
        const std::string& chnAddr = chn.first;
        const Channel& chnObj = chn.second;

        std::string lineOut = strprintf("%s,%s,%s,%s,%d", chnAddr, chnObj.getMultisig(), chnObj.getFirst(), chnObj.getSecond(), chnObj.getLastBlock());
        lineOut.append("+");
        addBalances(chnObj.getBalanceMap(), lineOut);
        // add the line to the hash
        hasher.Write((unsigned char*)lineOut.c_str(), lineOut.length());
        // write the line
        file << lineOut << std::endl;

    }

    return 0;
}

static int write_mp_nodeaddresses(ofstream &file, CHash256& hasher)
{
    nR.saveNodeReward(file, hasher);

    return 0;
}

static void iterWrite(std::ofstream& file, CHash256& hasher, const std::map<int, std::map<uint32_t,int64_t>>& aMap)
{
    for(const auto &m : aMap)
    {
       // decompose the key for address
       const uint32_t& block = m.first;
       const auto &pMap = m.second;

       for(const auto &p : pMap)
       {
           const uint32_t& propertyId = p.first;
           const int64_t& amount = p.second;
           const std::string lineOut = strprintf("%d,%d,%d", block, propertyId, amount);
           // add the line to the hash
           hasher.Write((unsigned char*)lineOut.c_str(), lineOut.length());
           // write the line
           file << lineOut << std::endl;
       }
    }

}

/** Saving DexMap volume **/
static int write_mp_dexvolume(std::ofstream& file, CHash256& hasher)
{
    iterWrite(file, hasher, MapTokenVolume);
    return 0;
}


/** Saving DEx and Channel LTC volume **/
static int write_mp_ltcvolume(std::ofstream& file, CHash256& hasher)
{
    iterWrite(file, hasher, MapLTCVolume);
    return 0;
}

/** Saving MDEx Map volume **/
static int write_mp_mdexvolume(std::ofstream& file, CHash256& hasher)
{
    iterWrite(file, hasher, metavolume);
    return 0;
}

static void savingLine(const std::string& address, std::ofstream& file, CHash256& hasher)
{
    const std::string lineOut = strprintf("%s",address);
    // add the line to the hash
    hasher.Write((unsigned char*)lineOut.c_str(), lineOut.length());

    // write the line
    file << lineOut << std::endl;
}

/** Saving vesting token addresses **/
static int write_mp_vesting_addresses(std::ofstream& file,  CHash256& hasher)
{
    for_each(vestingAddresses.begin(), vestingAddresses.end(), [&file, &hasher] (const std::string& address) { savingLine(address, file, hasher);});

    return 0;
}

/** Saving contract position data **/
static int write_mp_register(std::ofstream& file,  CHash256& hasher)
{
  for (auto iter = mp_register_map.begin(); iter != mp_register_map.end(); ++iter)
  {
      bool emptyWallet = true;
      int count = 0;
      std::string lineOut = iter->first;
      lineOut.append("=");
      Register& reg = iter->second;
      reg.init();
      uint32_t contractId = 0;
      while (0 != (contractId = reg.next()))
      {

          if (count > 0) lineOut.append(strprintf("#"));

          const int64_t entryPrice = reg.getRecord(contractId, ENTRY_CPRICE);
          const int64_t position = reg.getRecord(contractId, CONTRACT_POSITION);
          const int64_t liquidationPrice = reg.getRecord(contractId, BANKRUPTCY_PRICE);
          const int64_t upnl = reg.getRecord(contractId, UPNL);
          const int64_t margin = reg.getRecord(contractId, MARGIN);
          const int64_t leverage = reg.getRecord(contractId, LEVERAGE);

          // we don't allow 0 balances to read in, so if we don't write them
          // it makes things match up better between persisted state and processed state
          if (0 == entryPrice && 0 == position && 0 == liquidationPrice && 0 == upnl && 0 == margin && leverage == 0) {
              continue;
          }

          emptyWallet = false;

          count++;

          // saving each record
          lineOut.append(strprintf("%d:%d,%d,%d,%d,%d,%d",
                  contractId,
                  entryPrice,
                  position,
                  liquidationPrice,
                  upnl,
                  margin,
                  leverage));

          // saving now the entries (contracts, price)
          const Entries* entry = reg.getEntries(contractId);

          if(entry != nullptr)
          {
              for (Entries::const_iterator it = entry->begin(); it != entry->end(); ++it)
              {
                  if (it == entry->begin()) lineOut.append(strprintf(";"));

                  const std::pair<int64_t,int64_t>& pair = *it;
                  const int64_t& amount = pair.first;
                  const int64_t& price = pair.second;
                  lineOut.append(strprintf("%d,%d", amount, price));

                  if (it != std::prev(entry->end())) {
                      lineOut.append(strprintf("|"));
                  }
               }
           }

       }

       if (!emptyWallet) {
          // add the line to the hash
          hasher.Write((unsigned char*)lineOut.c_str(), lineOut.length());

          // write the line
          file << lineOut << std::endl;
        }

    }

    return 0;
}

static int write_state_file(CBlockIndex const *pBlockIndex, int what)
{
    fs::path path = MPPersistencePath / strprintf("%s-%s.dat", statePrefix[what], pBlockIndex->GetBlockHash().ToString());
    const std::string strFile = path.string();

    std::ofstream file;
    file.open(strFile.c_str());

    CHash256 hasher;

    int result = 0;

    switch(what) {
    case FILETYPE_BALANCES:
        result = write_msc_balances(file, hasher);
        break;

    case FILETYPE_GLOBALS:
        result = write_globals_state(file, hasher);
        break;

    case FILETYPE_CONTRACT_GLOBALS:
        result = write_contract_globals_state(file, hasher);
        break;

    case FILETYPE_CDEXORDERS:
        result = write_mp_contractdex(file, hasher);
        break;

    case FILETYPE_OFFERS:
        result = write_mp_offers(file, hasher);
        break;

    case FILETYPE_ACCEPTS:
        result = write_mp_accepts(file, hasher);
        break;

    case FILETYPE_MDEXORDERS:
        result = write_mp_metadex(file, hasher);
        break;

    case FILETYPE_CACHEFEES:
        result = write_mp_cachefees(file, hasher);
        break;

    // case FILETYPE_CACHEFEES_ORACLES:
    //     result = write_mp_cachefees_oracles(file, hasher);
    //     break;

    case FILETYPE_WITHDRAWALS:
        result = write_mp_withdrawals(file, hasher);
        break;

    case FILETYPE_ACTIVE_CHANNELS:
        result = write_mp_active_channels(file, hasher);
        break;

    case FILETYPE_DEX_VOLUME:
        result = write_mp_dexvolume(file, hasher);
        break;

    case FILETYPE_MDEX_VOLUME:
        result = write_mp_mdexvolume(file, hasher);
        break;

    case FILETYPE_GLOBAL_VARS:
        result = write_global_vars(file, hasher);
        break;

    case FILE_TYPE_VESTING_ADDRESSES:
        result = write_mp_vesting_addresses(file, hasher);
        break;

    case FILE_TYPE_LTC_VOLUME:
        result = write_mp_ltcvolume(file, hasher);
        break;

    case FILE_TYPE_TOKEN_LTC_PRICE:
        result = write_mp_token_ltc_prices(file, hasher);
        break;

    case FILE_TYPE_TOKEN_VWAP:
        result = write_mp_tokenvwap(file, hasher);
        break;

    case FILE_TYPE_REGISTER:
        result = write_mp_register(file, hasher);
        break;

    case FILE_TYPE_NODE_ADDRESSES:
        result = write_mp_nodeaddresses(file, hasher);
        break;
    }

    // generate and write the double hash of all the contents written

    uint256 hash;
    hasher.Finalize(hash.begin());
    file << "!" << hash.ToString() << std::endl;

    file.flush();
    file.close();

    return result;
}

static bool is_state_prefix(std::string const &str)
{
    for (int i = 0; i < NUM_FILETYPES; ++i)
    {
        if (boost::equals(str,  statePrefix[i]))
            return true;
    }

    return false;
}

static void prune_state_files(CBlockIndex const *topIndex)
{
    // build a set of blockHashes for which we have any state files
    std::set<uint256> statefulBlockHashes;
    fs::directory_iterator dIter(MPPersistencePath);
    fs::directory_iterator endIter;
    for (; dIter != endIter; ++dIter)
    {
        std::string fName = dIter->path().empty() ? "<invalid>" : (*--dIter->path().end()).string();
        if (!fs::is_regular_file(dIter->status()))
        {
            // skip funny business
            PrintToLog("Non-regular file found in persistence directory : %s\n", fName);
            continue;
        }

        std::vector<std::string> vstr;
        boost::split(vstr, fName, boost::is_any_of("-."), token_compress_on);
        if (vstr.size() == 3 && is_state_prefix(vstr[0]) && boost::equals(vstr[2], "dat"))
        {
            uint256 blockHash;
            blockHash.SetHex(vstr[1]);
            statefulBlockHashes.insert(blockHash);
        } else {
            PrintToLog("None state file found in persistence directory : %s\n", fName);
        }
    }

    // for each blockHash in the set, determine the distance from the given block
    std::set<uint256>::const_iterator iter;
    for (iter = statefulBlockHashes.begin(); iter != statefulBlockHashes.end(); ++iter)
    {
        // look up the CBlockIndex for height info
        CBlockIndex const *curIndex = GetBlockIndex(*iter);

        // if we have nothing int the index, or this block is too old..
        if (nullptr == curIndex || (((topIndex->nHeight - curIndex->nHeight) > MAX_STATE_HISTORY)
                && (curIndex->nHeight % STORE_EVERY_N_BLOCK != 0)))
        {
            if (msc_debug_persistence)
            {
                if (curIndex)
                {
                    PrintToLog("State from Block:%s is no longer need, removing files (age-from-tip: %d)\n", (*iter).ToString(), topIndex->nHeight - curIndex->nHeight);
                } else {
                    PrintToLog("State from Block:%s is no longer need, removing files (not in index)\n", (*iter).ToString());
                }
            }

            // destroy the associated files!
            std::string strBlockHash = iter->ToString();
            for (int i = 0; i < NUM_FILETYPES; ++i)
            {
                fs::path path = MPPersistencePath / strprintf("%s-%s.dat", statePrefix[i], strBlockHash);
                fs::remove(path);
            }
        }
    }
}

int mastercore_save_state(CBlockIndex const *pBlockIndex)
{
    // write the new state as of the given block
    write_state_file(pBlockIndex, FILETYPE_BALANCES);
    write_state_file(pBlockIndex, FILETYPE_GLOBALS);
    write_state_file(pBlockIndex, FILETYPE_CDEXORDERS);
    write_state_file(pBlockIndex, FILETYPE_MDEXORDERS);
    write_state_file(pBlockIndex, FILETYPE_OFFERS);
    write_state_file(pBlockIndex, FILETYPE_ACCEPTS);
    write_state_file(pBlockIndex, FILETYPE_CACHEFEES);
    //write_state_file(pBlockIndex, FILETYPE_CACHEFEES_ORACLES);
    write_state_file(pBlockIndex, FILETYPE_WITHDRAWALS);
    write_state_file(pBlockIndex, FILETYPE_ACTIVE_CHANNELS);
    write_state_file(pBlockIndex, FILETYPE_DEX_VOLUME);
    write_state_file(pBlockIndex, FILETYPE_MDEX_VOLUME);
    write_state_file(pBlockIndex, FILETYPE_GLOBAL_VARS);
    write_state_file(pBlockIndex, FILE_TYPE_VESTING_ADDRESSES);
    write_state_file(pBlockIndex, FILE_TYPE_LTC_VOLUME);
    write_state_file(pBlockIndex, FILE_TYPE_TOKEN_LTC_PRICE);
    write_state_file(pBlockIndex, FILE_TYPE_TOKEN_VWAP);
    write_state_file(pBlockIndex, FILE_TYPE_REGISTER);
    write_state_file(pBlockIndex, FILETYPE_CONTRACT_GLOBALS);
    write_state_file(pBlockIndex, FILE_TYPE_NODE_ADDRESSES);

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
    g_fees->native_fees.clear();
    g_fees->oracle_fees.clear();
    mp_tally_map.clear();
    my_pending.clear();
    my_offers.clear();
    my_accepts.clear();
    metadex.clear();
    my_pending.clear();
    contractdex.clear();
    channels_Map.clear();
    withdrawal_Map.clear();
    MapLTCVolume.clear();
    MapTokenVolume.clear();
    metavolume.clear();
    vestingAddresses.clear();
    lastPrice.clear();
    tokenvwap.clear();
    mp_register_map.clear();

    ResetConsensusParams();
    ClearActivations();

    // LevelDB based storage
    _my_sps->Clear();
    _my_cds->Clear();
    t_tradelistdb->Clear();
    p_txlistdb->Clear();
    p_TradeTXDB->Clear();

    assert(p_txlistdb->setDBVersion() == DB_VERSION); // new set of databases, set DB version
}

/**
 * Global handler to initialize Trade Layer.
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

  PrintToLog("\nInitializing Trade Layer\n");
  PrintToLog("Startup time: %s\n", FormatISO8601Date(GetTime()));
  // PrintToLog("Build date: %s, based on commit: %s\n", BuildDate(), BuildCommit());

  InitDebugLogLevels();
  ShrinkDebugLog();

  // check for --autocommit option and set transaction commit flag accordingly
  if (!gArgs.GetBoolArg("-autocommit", true)) {
    PrintToLog("Process was started with --autocommit set to false. "
	       "Created Trade Layer transactions will not be committed to wallet or broadcast.\n");
    autoCommit = false;
  }

  // check for --startclean option and delete MP_ folders if present
  bool startClean = false;
  if (gArgs.GetBoolArg("-startclean", false)) {
    PrintToLog("Process was started with --startclean option, attempting to clear persistence files..\n");
    try {
      fs::path persistPath = GetDataDir() / "OCL_persist";
      fs::path txlistPath = GetDataDir() / "OCL_txlist";
      fs::path spPath = GetDataDir() / "OCL_spinfo";
      fs::path cdPath = GetDataDir() / "OCL_cdinfo";
      fs::path tlTXDBPath = GetDataDir() / "OCL_TXDB";
      fs::path tradeList = GetDataDir() / "OCL_tradelist";
      if (fs::exists(persistPath)) fs::remove_all(persistPath);
      if (fs::exists(txlistPath)) fs::remove_all(txlistPath);
      if (fs::exists(spPath)) fs::remove_all(spPath);
      if (fs::exists(cdPath)) fs::remove_all(cdPath);
      if (fs::exists(tlTXDBPath)) fs::remove_all(tlTXDBPath);
      if (fs::exists(tradeList)) fs::remove_all(tradeList);
      PrintToLog("Success clearing persistence files in datadir %s\n", GetDataDir().string());
      startClean = true;
    } catch (const fs::filesystem_error& e) {
      PrintToLog("Failed to delete persistence folders: %s\n", e.what());
    }
  }

  p_txlistdb = new CMPTxList(GetDataDir() / "OCL_txlist", fReindex);
  _my_sps = new CMPSPInfo(GetDataDir() / "OCL_spinfo", fReindex);
  _my_cds = new CDInfo(GetDataDir() / "OCL_cdinfo", fReindex);
  p_TradeTXDB = new CtlTransactionDB(GetDataDir() / "OCL_TXDB", fReindex);
  t_tradelistdb = new CMPTradeList(GetDataDir()/"OCL_tradelist", fReindex);
  MPPersistencePath = GetDataDir() / "OCL_persist";
  TryCreateDirectory(MPPersistencePath);

  bool wrongDBVersion = (p_txlistdb->getDBVersion() != DB_VERSION);

  ++mastercoreInitialized;

  PrintToLog("%s(): mastercoreInitialized: %d\n",__func__, mastercoreInitialized);
   
  nWaterlineBlock = load_most_relevant_state();
     
  // Initialize after loading state as the fund relies on the register
  g_fund = MakeUnique<InsuranceFund>();

  bool noPreviousState = (nWaterlineBlock <= 0);

  if (startClean) {
    assert(p_txlistdb->setDBVersion() == DB_VERSION); // new set of databases, set DB version
  } else if (wrongDBVersion) {
    nWaterlineBlock = -1; // force a clear_all_state and parse from start
  }

  if (nWaterlineBlock > 0) {
    PrintToLog("Loading persistent state: OK [block %d]\n", nWaterlineBlock);
  } else {
    std::string strReason = "unknown";
    if (wrongDBVersion) strReason = "client version changed";
    if (noPreviousState) strReason = "no usable previous state found";
    if (startClean) strReason = "-startclean parameter used";
    PrintToLog("Loading persistent state: NONE (%s)\n", strReason);
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
  // p_txlistdb->LoadAlerts(nWaterlineBlock);

  // initial scan
  PrintToLog("%s(): Initial scan with nWaterlineBlock: %d\n",__func__, nWaterlineBlock);
  msc_initial_scan(nWaterlineBlock);

  PrintToLog("Trade Layer initialization completed\n");

  return 0;
}

/**
 * Global handler to shut down Trade Layer.
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
        p_txlistdb = nullptr;
    }

    if (t_tradelistdb) {
        delete t_tradelistdb;
        t_tradelistdb = nullptr;
    }

    if (_my_sps) {
        delete _my_sps;
        _my_sps = nullptr;
    }

    if (_my_cds) {
        delete _my_cds;
        _my_cds = nullptr;
    }

    if (p_TradeTXDB) {
        delete p_TradeTXDB;
        p_TradeTXDB = nullptr;
    }

    mastercoreInitialized = 0;

    PrintToLog("\nTrade Layer shutdown completed\n");
    PrintToLog("Shutdown time: %s\n", FormatISO8601Date(GetTime()));

    return 0;
}


/**
 * Calling for Settement (if any)
 *
 * @return True, if everything is ok
 */
bool CallingSettlement()
{
    int nBlockNow = GetHeight();

    /*uint32_t nextSPID = _my_sps->peekNextSPID(1);

    for (uint32_t propertyId = 1; propertyId < nextSPID; propertyId++)
    {

        int32_t propertyId = 5;

        // if(!mastercore::isPropertyContract(propertyId))
        //     continue;

        if (nBlockNow%BlockS == 0 && nBlockNow != 0 && path_elef.size() != 0 && lastBlockg != nBlockNow)
        {

            if(msc_calling_settlement) PrintToLog("\nSettlement every 8 hours here. nBlockNow = %d\n", nBlockNow);
            pt_ndatabase = new MatrixTLS(path_elef.size(), n_cols); MatrixTLS &ndatabase = *pt_ndatabase;
            MatrixTLS M_file(path_elef.size(), n_cols);
            fillingMatrix(M_file, ndatabase, path_elef);
            n_rows = size(M_file, 0);
            if(msc_calling_settlement) PrintToLog("Matrix for Settlement: dim = (%d, %d)\n\n", n_rows, n_cols);

            // TWAP vector
            if(msc_calling_settlement) PrintToLog("\nTWAP Prices = \n");

            uint64_t num_cdex = accumulate(cdextwap_vec[propertyId].begin(), cdextwap_vec[propertyId].end(), 0.0);

            rational_t twap_priceRatCDEx(num_cdex/COIN, cdextwap_vec[propertyId].size());
            int64_t twap_priceCDEx = mastercore::RationalToInt64(twap_priceRatCDEx);
            if(msc_debug_handler_tx) PrintToLog("\nTvwap Price CDEx = %s\n", FormatDivisibleMP(twap_priceCDEx));

            CMPSPInfo::Entry sp;
            if (!_my_sps->getSP(propertyId, sp))
                return false;

            uint64_t property_num = sp.numerator;
            uint64_t property_den = sp.denominator;

            uint64_t num_mdex=accumulate(mdextwap_vec[property_num][property_den].begin(),mdextwap_vec[property_num][property_den].end(),0.0);


            if(msc_debug_handler_tx) PrintToLog("\nTWAP Prices = \n");
            // struct FutureContractObject *pfuture = getFutureContractObject("ALL F18");
            // uint32_t property_traded = pfuture->fco_propertyId;

            rational_t twap_priceRatMDEx(num_mdex/COIN, mdextwap_vec[property_num][property_den].size());
            int64_t twap_priceMDEx = mastercore::RationalToInt64(twap_priceRatMDEx);
            if(msc_calling_settlement) PrintToLog("\nTvwap Price MDEx = %s\n", FormatDivisibleMP(twap_priceMDEx));


            // Interest formula:

            // futures don't use this formula
            // if (sp.prop_type == ALL_PROPERTY_TYPE_NATIVE_CONTRACT  || sp.prop_type == ALL_PROPERTY_TYPE_ORACLE_CONTRACT)
            //     continue;

            int64_t twap_price = 0;

            switch(sp.prop_type){
                case ALL_PROPERTY_TYPE_PERPETUAL_ORACLE:
                    twap_price = getOracleTwap(propertyId, oBlocks);
                    break;
                case ALL_PROPERTY_TYPE_PERPETUAL_CONTRACTS:
                    twap_price = twap_priceMDEx;
                    break;
            }

            int64_t interest = clamp_function(abs(twap_priceCDEx-twap_price), 0.05);
            if(msc_calling_settlement) PrintToLog("Interes to Pay = %s", FormatDivisibleMP(interest));


            if(msc_calling_settlement) PrintToLog("\nCalling the Settlement Algorithm:\n\n");

            //NOTE: We need num and den for contract as a property of itself in sp.h
            // settlement_algorithm_fifo(M_file, interest, twap_priceCDEx, propertyId, sp.collateral_currency, sp.numerator, sp.denominator, sp.inverse_quoted);
        }
    }*/

    /***********************************************************************/
/** Calling The Settlement Algorithm **/

if (nBlockNow%BlockS == 0 && nBlockNow != 0 && path_elef.size() != 0 && lastBlockg != nBlockNow)
{

     PrintToLog("\nSETTLEMENT : every 8 hours here. nBlockNow = %d\n", nBlockNow);
     pt_ndatabase = new MatrixTLS(path_elef.size(), n_cols); MatrixTLS &ndatabase = *pt_ndatabase;
     MatrixTLS M_file(path_elef.size(), n_cols);
     fillingMatrix(M_file, ndatabase, path_elef);
     n_rows = size(M_file, 0);
     PrintToLog("Matrix for Settlement: dim = (%d, %d)\n\n", n_rows, n_cols);

      /*****************************************************************************/
      cout << "\n\n";
      PrintToLog("\nCalling the Settlement Algorithm:\n\n");
      int64_t twap_priceCDEx  = 0;
      int64_t interest = 0;

      //TODO: we need to add here insurance logic
      settlement_algorithm_fifo(M_file, interest, twap_priceCDEx);

      /**********************************************************************/
      /** Unallocating Dynamic Memory **/

      //path_elef.clear();
      market_priceMap.clear();
      VWAPMap.clear();
      VWAPMapSubVector.clear();
      numVWAPVector.clear();
      denVWAPVector.clear();
      mapContractAmountTimesPrice.clear();
      mapContractVolume.clear();
      VWAPMapContracts.clear();
      cdextwap_vec.clear();

   }

   return true;
}


/**
 * Calling for Expiration (if any)
 *
 * @return True, if everything is ok
 */
bool CallingExpiration(CBlockIndex const * pBlockIndex)
{
  int expirationBlock = 0, tradeBlock = 0, checkExpiration = 0;

  uint32_t nextCDID = _my_cds->peekNextContractID();

  // checking expiration block for each contract
  for (uint32_t contractId = 1; contractId < nextCDID; contractId++)
  {
      CDInfo::Entry cd;
      if (_my_cds->getCD(contractId, cd) && ! cd.expirated)
      {
	        expirationBlock = static_cast<int>(cd.blocks_until_expiration);
	        tradeBlock = static_cast<int>(pBlockIndex->nHeight);
          lastBlockg = static_cast<int>(pBlockIndex->nHeight);


          int deadline = cd.init_block + expirationBlock;

          // if(msc_debug_handler_tx) PrintToLog("%s(): deadline: %d, lastBlockg : %d\n",__func__,deadline,lastBlockg);

          if ( tradeBlock != 0 && deadline != 0 ) checkExpiration = tradeBlock == deadline ? 1 : 0;

          if (checkExpiration)
          {
              cd.expirated = true; // into entry register
              // if(msc_debug_handler_tx) PrintToLog("%s(): EXPIRATED!!!!!!!!!!!!!!!!!!!!!!!!!!!\n",__func__);

              idx_expiration += 1;
              if ( idx_expiration == 2 )
              {
                  expirationAchieve = 1;

              } else expirationAchieve = 0;
          } else expirationAchieve = 0;
      }
   }

   return true;
}

double getAccumVesting(const int64_t LTC_Volume)
{
    const double amount = (double) LTC_Volume / COIN;
    // accumVesting fraction = (Log10(Cum_LTC_Volume)-4)/4; 100% vested at 100,000,000  LTCs volume
    const double result = ((std::log10(amount) - 4) / 4);
    return ((result < 1.0) ? result : 1.0);

}

bool VestingTokens(int block)
{
    bool deactivation{false};

    if(msc_debug_vesting) PrintToLog("%s() block : %d\n",__func__, block);

    if (vestingAddresses.empty())
    {
        if(msc_debug_vesting) PrintToLog("%s(): there's no vesting address registered\n",__func__);
        return false;
    }

    // NOTE : this is used to simplify the testing on regtest
    const int64_t LTC_Volume = (RegTest()) ? globalVolumeALL_LTC * 100 : globalVolumeALL_LTC;

    if(msc_debug_vesting) PrintToLog("%s(): globalVolumeALL_LTC: %d \n",__func__, LTC_Volume);

    if (LTC_Volume <= INF_VOL_LIMIT)
    {
        if(msc_debug_vesting) PrintToLog("%s(): 0 percent of vesting (LTC) less than 10,000 LTC volume: %d\n",__func__, LTC_Volume);
        return false;
    }


    if(SUP_VOL_LIMIT <= LTC_Volume){
        if(msc_debug_vesting) PrintToLog("%s(): Vesting Tokens completed at 100,000,000 LTC volume: %d\n",__func__, LTC_Volume);
        deactivation = true;
    }

    CMPSPInfo::Entry sp;
    if (!_my_sps->getSP(VT, sp)) {
       return false; // property ID does not exist
    }

    // accumulated vesting fraction
    const double accumVesting = getAccumVesting(LTC_Volume);

    if(msc_debug_vesting) PrintToLog("%s(): accumVesting: %d, last_vesting: %d\n",__func__, accumVesting, sp.last_vesting);

    if (accumVesting == sp.last_vesting)
    {
        if(msc_debug_vesting) PrintToLog("%s(): 0 percent vesting in this block: %d\n",__func__, block);
        return false;
    }

    if(msc_debug_vesting) {
        PrintToLog("%s(): lastVesting = %f, accumVesting: %f\n",__func__, sp.last_vesting, accumVesting);
        PrintToLog("%s(): accum vesting on this block = %f, block ; %d, x_Axis: %d \n",__func__, accumVesting, block, LTC_Volume);
    }

    for (auto &addr : vestingAddresses)
    {
        if(msc_debug_vesting) PrintToLog("Address = %s\n", addr);

        const int64_t vestingBalance = getMPbalance(addr, TL_PROPERTY_VESTING, BALANCE);
        const int64_t unVestedALLBal = getMPbalance(addr, ALL, UNVESTED);
        const int64_t vestedALLBal = getMPbalance(addr, ALL, BALANCE);

        if(msc_debug_vesting) {
            PrintToLog("ALLs UNVESTED in address = %d\n", unVestedALLBal);
            PrintToLog("ALLs BALANCE in address = %d\n", vestedALLBal);
            PrintToLog("Vesting tokens in address = %d\n", vestingBalance);
        }

        if (0 < vestingBalance && 0 < unVestedALLBal)
        {
            const int64_t iAccumVesting = mastercore::DoubleToInt64(accumVesting);
            const arith_uint256 uAccumVesting  = mastercore::ConvertTo256(iAccumVesting);

            const arith_uint256 uVestingBalance = mastercore::ConvertTo256(vestingBalance);

            // accum % of vesting from vesting to ALLs
            const arith_uint256 iAmount = DivideAndRoundUp(uAccumVesting * uVestingBalance , COIN);
            int64_t nAmount = mastercore::ConvertTo64(iAmount);

            if(deactivation) nAmount = unVestedALLBal;

            // adding the difference to tally
            const int64_t difference = nAmount - vestedALLBal;


            if(msc_debug_vesting) {
                PrintToLog("%s(): nAmount = (iAccumVesting * vestingBalance) / COIN \n",__func__);
                PrintToLog("%s(): iAccumVesting = %d\n",__func__, iAccumVesting);
                PrintToLog("%s(): vestingBalance = %d\n",__func__, vestingBalance);
                PrintToLog("%s(): nAmount = %d, difference = %d\n",__func__, nAmount, difference);
            }

            if (difference > 0 && unVestedALLBal >= difference)
            {
                assert(update_tally_map(addr, ALL, -difference, UNVESTED));
                assert(update_tally_map(addr, ALL, difference, BALANCE));

            }

        }
    }

    // updating vesting info
    sp.last_vesting = accumVesting;
    sp.last_vesting_block = block;

    assert(_my_sps->updateSP(VT, sp));


    if(msc_debug_handler_tx)
    {
        auto it = vestingAddresses.begin();
        PrintToLog("ALLs UNVESTED = %d\n", getMPbalance(*it, TL_PROPERTY_ALL, UNVESTED));
        PrintToLog("ALLs BALANCE = %d\n", getMPbalance(*it, TL_PROPERTY_ALL, BALANCE));
    }


    // if it reachs the LTC volume
    if (deactivation) DeactivateFeature(FEATURE_VESTING, block);

    return true;
}


/**
 * This handler is called for every new transaction that comes in (actually in block parsing loop).
 *
 * @return True, if the transaction was a valid Trade Layer transaction
 */
bool mastercore_handler_tx(const CTransaction& tx, int nBlock, unsigned int idx, const CBlockIndex* pBlockIndex, const std::shared_ptr<std::map<COutPoint, Coin>> removedCoins)
{

    if (!mastercoreInitialized) {
       mastercore_init();
    }


    {
        LOCK(cs_tally);
        // clear pending, if any
        // NOTE1: Every incoming TX is checked, not just MP-ones because:
        // if for some reason the incoming TX doesn't pass our parser validation steps successfuly, I'd still want to clear pending amounts for that TX.
        // NOTE2: Plus I wanna clear the amount before that TX is parsed by our protocol, in case we ever consider pending amounts in internal calculations.
        PendingDelete(tx.GetHash());

        // we do not care about parsing blocks prior to our waterline (empty blockchain defense)
        if (nBlock < nWaterlineBlock) return false;

    }

    int64_t nBlockTime = pBlockIndex->GetBlockTime();

    CMPTransaction mp_obj;
    mp_obj.unlockLogic();

    bool fFoundTx = false;
    int pop_ret;
    {
       LOCK2(cs_main, cs_tally);
       pop_ret = parseTransaction(false, tx, nBlock, idx, mp_obj, nBlockTime, removedCoins);

    }

    if (0 == pop_ret)
    {
        assert(mp_obj.getEncodingClass() != NO_MARKER);
        assert(mp_obj.getSender().empty() == false);

        int interp_ret = mp_obj.interpretPacket();

        if(msc_debug_handler_tx) PrintToLog("%s(): interp_ret: %d\n",__func__, interp_ret);

        // if interpretPacket returns 1, that means we have an instant trade between LTCs and tokens.
        if (interp_ret == 1)
        {
            HandleLtcInstantTrade(tx, nBlock, mp_obj.getIndexInBlock(), mp_obj.getSender(), mp_obj.getSpecial(), mp_obj.getReceiver(), mp_obj.getProperty(), mp_obj.getAmountForSale(), mp_obj.getPrice());

        } else if (interp_ret == 2) {
            HandleDExPayments(tx, nBlock, mp_obj.getSender());

        }

        // Only structurally valid transactions get recorded in levelDB
        // PKT_ERROR - 2 = interpret_Transaction failed, structurally invalid payload
        if (interp_ret != PKT_ERROR - 2)
        {
            LOCK(cs_tally);
            bool bValid = (0 <= interp_ret);
            p_txlistdb->recordTX(tx.GetHash(), bValid, nBlock, mp_obj.getType(), mp_obj.getNewAmount(), interp_ret);
            p_TradeTXDB->RecordTransaction(tx.GetHash(), idx);

        }

        fFoundTx |= (interp_ret == 0);
    }

    LOCK(cs_tally);
    if (fFoundTx && msc_debug_consensus_hash_every_transaction) {
        const uint256 consensusHash = GetConsensusHash();
        if(msc_debug_handler_tx) PrintToLog("Consensus hash for transaction %s: %s\n", tx.GetHash().GetHex(), consensusHash.GetHex());
    }

    return fFoundTx;
}

bool nodeReward::isAddressIncluded(const std::string& address)
{
    auto it = nodeRewardsAddrs.find(address);
    return (it != nodeRewardsAddrs.end()) ? true : false;
}

void nodeReward::addWinner(const std::string& address, int64_t amount)
{
    winners[address] = amount;
}


void nodeReward::saveNodeReward(ofstream &file, CHash256& hasher)
{
    const std::string lineOut = strprintf("%d+%d", p_Reward, p_lastBlock);
    hasher.Write((unsigned char*)lineOut.c_str(), lineOut.length());
    file << lineOut << endl;

    for(const auto& addr : nodeRewardsAddrs)
    {
        const std::string& address = addr.first;
        bool status = addr.second;
        const std::string lineOut = strprintf("%s:%s", address, status ? "true":"false");

        // add the line to the hash
        hasher.Write((unsigned char*)lineOut.c_str(), lineOut.length());

        // write the line
        file << lineOut << endl;

    }

    // saving last winners
    for(const auto& addr : winners)
    {
        std::string lineOut;

        lineOut = strprintf("%s:%d", addr.first, addr.second);

        // add the line to the hash
        hasher.Write((unsigned char*)lineOut.c_str(), lineOut.length());

        // write the line
        file << lineOut << endl;
    }

}

bool nodeReward::nextReward(const int& nHeight)
{
   bool result{false};

   const CConsensusParams &params = ConsensusParams();

   // logic in function of blockheight
   const double factor = (nHeight - params.MSC_NODE_REWARD_BLOCK <= params.INFLEXION_BLOCK) ? FBASE  : SBASE;

   double f_nextReward = BASE_REWARD * pow(factor, nHeight - params.MSC_NODE_REWARD_BLOCK);

   p_Reward = (int64_t) f_nextReward;

   if (p_Reward < MIN_REWARD) p_Reward = MIN_REWARD;

   // PrintToLog("%s():p_Reward after: %d, f_nextReward: %d\n", __func__, p_Reward, f_nextReward);

   return !result;
}

void nodeReward::sendNodeReward(const std::string& consensusHash, const int& nHeight, bool fTest)
{
    int count = 0;

    for(auto& addr : nodeRewardsAddrs)
    {
        // if this address is a winner and it's claiming reward
        if(isWinnerAddress(consensusHash, addr.first, fTest) && addr.second) {
            // counting winners on this block
            count++;
            const int64_t rewardSize = p_Reward / count;
            // PrintToLog("%s(): winner: %s, reward: %d, count: %d\n",__func__, addr.first, rewardSize, count);
            // we need to save the winners and the prize here!
            winners[addr.first] = rewardSize;

        }


    }

    // using 10 blocks as block limit
    const int result = nHeight % BLOCK_LIMIT;
    // after that we share the reward with all winners
    if(0 == result)
    {
        for(const auto& w : winners)
        {
            auto it = nodeRewardsAddrs.find(w.first);
            // if address is claiming reward, we update tally
            if (it != nodeRewardsAddrs.end() && it->second){
                update_tally_map(w.first, ALL, w.second, BALANCE);
                // PrintToLog("%s(): address: %s, reward: %d\n",__func__, w.first, w.second);
                // cleaning claiming status for this block
                if (!fTest) it->second = false;
            }

        }

        // adding block info
        p_lastBlock = nHeight;

        clearWinners();

    }

    nextReward(nHeight);


}

bool nodeReward::isWinnerAddress(const std::string& consensusHash, const std::string& address, bool fTest)
{
    const int lastNterms = (fTest) ? 2 : 3;

    std::vector<unsigned char> vch;

    DecodeBase58(address, vch);

    const std::string vchStr = HexStr(vch);
    const std::string LastThreeCharAddr = vchStr.substr(vchStr.size() - lastNterms);
    const std::string lastThreeCharConsensus = consensusHash.substr(consensusHash.size() - lastNterms);

    // PrintToLog("%s(): lastThreeCharConsensus: %s, LastThreeCharAddr (decoded): %s\n",__func__, lastThreeCharConsensus, LastThreeCharAddr);

    return (LastThreeCharAddr == lastThreeCharConsensus) ? true : false;

}

void nodeReward::updateAddressStatus(const std::string& address, bool newStatus)
{
    nodeRewardsAddrs[address] = newStatus;
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
 * Determines, whether it is valid to use a Class D transaction for a given payload size.
 *
 * @param nDataSize The length of the payload
 * @return True, if Class D is enabled and the payload is small enough
 */
bool mastercore::UseEncodingClassD(size_t nDataSize)
{
    size_t nTotalSize = nDataSize + GetTLMarker().size(); // Marker "tl"
    bool fDataEnabled = gArgs.GetBoolArg("-datacarrier", true);
    int nBlockNow = GetHeight();
    if (!IsAllowedOutputType(TX_NULL_DATA, nBlockNow)) {
        fDataEnabled = false;
    }

    return (nTotalSize <= nMaxDatacarrierBytes && fDataEnabled);
}

// This function requests the wallet create an Trade Layer transaction using the supplied parameters and payload
int mastercore::WalletTxBuilder(const std::string& senderAddress, const std::string& receiverAddress, int64_t referenceAmount,
				const std::vector<unsigned char>& data, uint256& txid, std::string& rawHex, bool commit,
				unsigned int minInputs)
{
    return WalletTxBuilderEx(senderAddress, {receiverAddress}, referenceAmount, data, txid, rawHex, commit, minInputs);
}

// This function requests the wallet create an Trade Layer transaction using the supplied parameters and payload
int mastercore::WalletTxBuilderEx(const std::string& senderAddress, const std::vector<std::string>& receiverAddress, int64_t referenceAmount,
				const std::vector<unsigned char>& data, uint256& txid, std::string& rawHex, bool commit,
				unsigned int minInputs)
{
#ifdef ENABLE_WALLET

  CWalletRef pwalletMain = nullptr;
  if (vpwallets.size() > 0){
    pwalletMain = vpwallets[0];
  }

  if (pwalletMain == nullptr) return MP_ERR_WALLET_ACCESS;

  if (nMaxDatacarrierBytes < (data.size()+GetTLMarker().size())) return MP_ERR_PAYLOAD_TOO_BIG;

  //TODO verify datacarrier is enabled at startup, otherwise won't be able to send transactions

  // Prepare the transaction - first setup some vars
  CCoinControl coinControl;
  coinControl.fAllowOtherInputs = true;
  CWalletTx wtxNew;
  std::vector<std::pair<CScript, int64_t> > vecSend;
  CReserveKey reserveKey(pwalletMain);

  // Next, we set the change address to the sender
  coinControl.destChange = DecodeDestination(senderAddress);

  CAmount nFeeRequired{GetMinimumFee(1000, coinControl, mempool, ::feeEstimator, nullptr)};

  // Amount required for outputs
  CAmount outputAmount{0};

  // Encode the data outputs
  if(!TradeLayer_Encode_ClassD(data,vecSend)) { return MP_ENCODING_ERROR; }

  // Then add a paytopubkeyhash output for the recipient (if needed) - note we do this last as we want this to be the highest vout
  for (const auto& addr : receiverAddress)
  {
    CScript scriptPubKey = GetScriptForDestination(DecodeDestination(addr));
    CAmount ramount = 0 < referenceAmount ? referenceAmount : GetDustThld(scriptPubKey);
    outputAmount += ramount;
    vecSend.push_back(std::make_pair(scriptPubKey, ramount));
  }

  std::vector<CRecipient> vecRecipients;
  for (size_t i = 0; i < vecSend.size(); ++i)
  {
     const std::pair<CScript, int64_t>& vec = vecSend[i];
     CRecipient recipient = {vec.first, CAmount(vec.second), false};
     vecRecipients.push_back(recipient);
  }

  CAmount nFeeRet{0};
  int nChangePosInOut = -1;
  std::string strFailReason;

  // Select the inputs
  if (0 > SelectCoins(senderAddress, coinControl, outputAmount + nFeeRequired, minInputs)) { return MP_INPUTS_INVALID; }

  // Now we have what we need to pass to the wallet to create the transaction, perform some checks first
  if (!coinControl.HasSelected()) return MP_ERR_INPUTSELECT_FAIL;

  // Ask the wallet to create the transaction (note mining fee determined by Litecoin Core params)
  if (!pwalletMain->CreateTransaction(vecRecipients, wtxNew, reserveKey, nFeeRet, nChangePosInOut, strFailReason, coinControl, true))
  {
      return MP_ERR_CREATE_TX;
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

void CtlTransactionDB::RecordTransaction(const uint256& txid, uint32_t posInBlock)
{
     assert(pdb);

     const std::string key = txid.ToString();
     const std::string value = strprintf("%d", posInBlock);
     Status status = pdb->Put(writeoptions, key, value);
     ++nWritten;
}

uint32_t CtlTransactionDB::FetchTransactionPosition(const uint256& txid)
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


void CMPTxList::recordMetaDExCancelTX(const uint256& txidMaster, const uint256& txidSub, bool fValid, int nBlock, unsigned int propertyId, uint64_t nValue)
{
    if (!pdb) return;

    // Prep - setup vars
    unsigned int type = 99992104;
    unsigned int refNumber = 1;
    uint64_t existingAffectedTXCount = 0;
    std::string txidMasterStr = txidMaster.ToString() + "-C";

    // Step 1 - Check TXList to see if this cancel TXID exists
    // Step 2a - If doesn't exist leave number of affected txs & ref set to 1
    // Step 2b - If does exist add +1 to existing ref and set this ref as new number of affected
    std::vector<std::string> vstr;
    std::string strValue;
    leveldb::Status status = pdb->Get(readoptions, txidMasterStr, &strValue);
    if (status.ok()) {
        // parse the string returned
        boost::split(vstr, strValue, boost::is_any_of(":"), boost::token_compress_on);

        // obtain the existing affected tx count
        if (4 <= vstr.size()) {
            existingAffectedTXCount = atoi(vstr[3]);
            refNumber = existingAffectedTXCount + 1;
        }
    }

    // Step 3 - Create new/update master record for cancel tx in TXList
    const std::string key = txidMasterStr;
    const std::string value = strprintf("%u:%d:%u:%lu", fValid ? 1 : 0, nBlock, type, refNumber);
    PrintToLog("METADEXCANCELDEBUG : Writing master record %s(%s, valid=%s, block= %d, type= %d, number of affected transactions= %d)\n", __func__, txidMaster.ToString(), fValid ? "YES" : "NO", nBlock, type, refNumber);
    status = pdb->Put(writeoptions, key, value);

    // Step 4 - Write sub-record with cancel details
    const std::string txidStr = txidMaster.ToString() + "-C";
    const std::string subKey = STR_REF_SUBKEY_TXID_REF_COMBO(txidStr, refNumber);
    const std::string subValue = strprintf("%s:%d:%lu", txidSub.ToString(), propertyId, nValue);
    PrintToLog("METADEXCANCELDEBUG : Writing sub-record %s with value %s\n", subKey, subValue);
    status = pdb->Put(writeoptions, subKey, subValue);
    if (msc_debug_txdb) PrintToLog("%s(): store: %s=%s, status: %s\n", __func__, subKey, subValue, status.ToString());
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
         if (5 != vstr.size()) continue; // unexpected number of tokens
         if (atoi(vstr[3]) != TL_MESSAGE_TYPE_ACTIVATION || atoi(vstr[0]) != 1) continue; // we only care about valid activations
         uint256 txid = uint256S(it->key().ToString());
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

         if (blockHash.IsNull() || (nullptr == GetBlockIndex(blockHash))) {
             PrintToLog("ERROR: While loading activation transaction %s: failed to retrieve block hash.\n", hash.GetHex());
             continue;
         }

         CBlockIndex* pBlockIndex = GetBlockIndex(blockHash);
         if (nullptr == pBlockIndex) {
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

         if (TL_MESSAGE_TYPE_ACTIVATION != mp_obj.getType()) {
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
         if (atoi(vstr[2]) != TL_MESSAGE_TYPE_ALERT || atoi(vstr[0]) != 1) continue; // not a valid alert
         uint256 txid = uint256S(it->key().ToString());
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
         if (TL_MESSAGE_TYPE_ALERT != mp_obj.getType()) {
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
         if (pBlockIndex != nullptr) {
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
     const std::string strKey = strprintf("%s-%d", txid.ToString(), subRecordNumber);
     const std::string strValue = strprintf("%d:%d", propertyId, nValue);

     leveldb::Status status = pdb->Put(writeoptions, strKey, strValue);
     ++nWritten;
    if (msc_debug_txdb) PrintToLog("%s(): store: %s=%s, status: %s\n", __func__, strKey, strValue, status.ToString());
}

void CMPTxList::recordTX(const uint256 &txid, bool fValid, int nBlock, unsigned int type, uint64_t nValue, int interp_ret)
{
    if (!pdb) return;

    // overwrite detection, we should never be overwriting a tx, as that means we have redone something a second time
    // reorgs delete all txs from levelDB above reorg_chain_height
    if (p_txlistdb->exists(txid)) PrintToLog("LEVELDB TX OVERWRITE DETECTION - %s\n", txid.ToString());

    const string key = txid.ToString();
    const string value = strprintf("%d:%d:%d:%u:%d", fValid ? 1:0, fValid ? 0:interp_ret, nBlock, type, nValue);
    Status status;

    PrintToLog("%s(%s, valid=%s, reason=%d, block= %d, type= %d, value= %lu)\n",
       __func__, txid.ToString(), fValid ? "YES":"NO", fValid ? 0 : interp_ret, nBlock, type, nValue);

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

    std::string strValue;
    Status status = pdb->Get(readoptions, txid.ToString(), &strValue);

    return ((!status.ok() && status.IsNotFound()) ? false : true);
 }

bool CMPTxList::getTX(const uint256 &txid, string &value)
{
    Status status = pdb->Get(readoptions, txid.ToString(), &value);

    ++nRead;

    return ((status.ok()) ? true : false);
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
        PrintToLog("entry #%8d= %s:%s\n", count, skey.ToString(), svalue.ToString());
    }

    delete it;
}

void CMPTxList::recordPaymentTX(const uint256 &txid, bool fValid, int nBlock, unsigned int vout, unsigned int propertyId, uint64_t nValue, uint64_t amountPaid, string buyer, string seller)
{
    if (!pdb) return;

    // Prep - setup vars
    const unsigned int type = 99999999;
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

    if(msc_debug_record_payment_tx) PrintToLog("DEXPAYDEBUG : Writing master record %s(%s, valid=%s, block= %d, type= %d, number of payments= %lu)\n", __func__, txid.ToString(), fValid ? "YES":"NO", nBlock, type, numberOfPayments);

    if (pdb)
    {
        status = pdb->Put(writeoptions, key, value);
        if(msc_debug_record_payment_tx) PrintToLog("DEXPAYDEBUG : %s(): %s, line %d, file: %s\n", __func__, status.ToString(), __LINE__, __FILE__);
    }

    // Step 4 - Write sub-record with payment details
    const string txidStr = txid.ToString();
    const string subKey = STR_PAYMENT_SUBKEY_TXID_PAYMENT_COMBO(txidStr, paymentNumber);
    const string subValue = strprintf("%d:%d:%s:%s:%d:%lu:%lu", nBlock, vout, buyer, seller, propertyId, nValue, amountPaid);
    Status subStatus;

    if(msc_debug_record_payment_tx) PrintToLog("DEXPAYDEBUG : Writing sub-record %s with value %s\n", subKey, subValue);

    if (pdb)
    {
        subStatus = pdb->Put(writeoptions, subKey, subValue);
        if(msc_debug_record_payment_tx) PrintToLog("DEXPAYDEBUG : %s(): %s, line %d, file: %s\n", __func__, subStatus.ToString(), __LINE__, __FILE__);
     }

 }

// figure out if there was at least 1 Master Protocol transaction within the block range, or a block if starting equals ending
// block numbers are inclusive
// pass in bDeleteFound = true to erase each entry found within the block range
 bool CMPTxList::isMPinBlockRange(int starting_block, int ending_block, bool bDeleteFound)
{
    int block;
    unsigned int n_found = 0;
    unsigned int count = 0;
    leveldb::Slice skey, svalue;
    std::vector<std::string> vstr;

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
                if(msc_debug_is_mpin_block_range) PrintToLog("%s() DELETING: %s=%s\n", __FUNCTION__, skey.ToString(), svalue.ToString());
                if (bDeleteFound) pdb->Delete(writeoptions, skey);
            }
        }
    }

    if(msc_debug_is_mpin_block_range) PrintToLog("%s(%d, %d); n_found= %d\n", __FUNCTION__, starting_block, ending_block, n_found);
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
        const string strValue = svalue.ToString();

        std::vector<std::string> vstr;
        boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);

        if (vstr.size() != 17) continue;

        const std::string& address1 = vstr[0];
        const std::string& address2 = vstr[1];

        if(msc_debug_get_transaction_address) PrintToLog("%s: address1 = %s,\t address2 =%s", __func__, address1, address2);

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
bool mastercore::getValidMPTX(const uint256 &txid, std::string *reason, int *block, unsigned int *type, uint64_t *nAmended)
{
    string result;
    int validity = 0;

    if (!p_txlistdb) return false;

    if (!p_txlistdb->getTX(txid, result))
    {
        return false;
    }

    // parse the string returned, find the validity flag/bit & other parameters
    std::vector<std::string> vstr;
    boost::split(vstr, result, boost::is_any_of(":"), token_compress_on);

    if (msc_debug_txdb) PrintToLog("%s() size=%lu : %s\n", __FUNCTION__, vstr.size(), result);

    if (1 <= vstr.size()) validity = atoi(vstr[0]);

    if (reason)
    {
        if (2 <= vstr.size())
        {
            const int error_code = atoi(vstr[1]);
            *reason = error_str(error_code);
        } else {
            *reason = "-";
        }
    }

    if (block)
    {
        if (3 <= vstr.size()) *block = atoi(vstr[2]);
        else *block = 0;
    }

    if (type)
    {
        if (4 <= vstr.size()) *type = atoi(vstr[3]);
        else *type = 0;
    }

    if (nAmended)
    {
        if (5 <= vstr.size()) *nAmended = boost::lexical_cast<boost::uint64_t>(vstr[4]);
        else nAmended = 0;
    }

    if (msc_debug_txdb) p_txlistdb->printStats();

    if (0 == validity) return false;

    return true;
}

int mastercore_handler_block_begin(int nBlockPrev, CBlockIndex const * pBlockIndex)
{
    LOCK(cs_tally);

    const int& nHeight = pBlockIndex->nHeight;

    bool bRecoveryMode{false};
    {
        LOCK(cs_tally);

        if (reorgRecoveryMode > 0) {
            reorgRecoveryMode = 0; // clear reorgRecovery here as this is likely re-entrant
            bRecoveryMode = true;
        }
    }

    if (bRecoveryMode) {
        // NOTE: The blockNum parameter is inclusive, so deleteAboveBlock(1000) will delete records in block 1000 and above.
        p_txlistdb->isMPinBlockRange(nHeight, reorgRecoveryMaxHeight, true);
        reorgRecoveryMaxHeight = 0;
        nWaterlineBlock = ConsensusParams().GENESIS_BLOCK - 1;

        const int best_state_block = load_most_relevant_state();

        if (best_state_block < 0)
        {
            // unable to recover easily, remove stale stale state bits and reparse from the beginning.
            clear_all_state();
        } else {
            nWaterlineBlock = best_state_block;
        }

        // clear the global wallet property list, perform a forced wallet update and tell the UI that state is no longer valid, and UI views need to be reinit
        global_wallet_property_list.clear();
        CheckWalletUpdate(true);
        //uiInterface.TLStateInvalidated();

        if (nWaterlineBlock < nBlockPrev)
        {
            // scan from the block after the best active block to catch up to the active chain
            msc_initial_scan(nWaterlineBlock + 1);
        }
    }

    // handle any features that go live with this block
    CheckLiveActivations(nHeight);

    const CConsensusParams &params = ConsensusParams();
    /** Creating Vesting Tokens **/
    if (nHeight == params.MSC_VESTING_CREATION_BLOCK) creatingVestingTokens(nHeight);

    /** Vesting Tokens to Balance **/
    if (nHeight > params.MSC_VESTING_BLOCK) VestingTokens(nHeight);

    /** Channels **/
    if (nHeight > params.MSC_TRADECHANNEL_TOKENS_BLOCK)
    {
        makeWithdrawals(nHeight);
    }

    /** Node rewards **/
    if (nHeight > params.MSC_NODE_REWARD_BLOCK){
        nR.clearWinners();
    }

    if (nHeight > params.MSC_PEGGED_CURRENCY_BLOCK && nHeight % 24 == 0) {
        addInterestPegged(nHeight);
    }

    // marginMain(pBlockIndex->nHeight);
    // addInterestPegged(nBlockPrev,pBlockIndex);

    /****************************************************************************/
    // Calling The Settlement Algorithm
    // NOTE: now we are checking all contracts
    // CallingSettlement();

    /*****************************************************************************/
    // feeCacheBuy:
    //   1) search caché in order to see the properties ids and the amounts.
    //   2) search for each prop id, exchange for ALLs with available orders in orderbook
    //   3) check the ALLs in cache.
    //mastercore::feeCacheBuy();

    /*****************************************************************************/
    //CallingExpiration(pBlockIndex);

   return 0;
}

// called once per block, after the block has been processed
// TODO: consolidate into *handler_block_begin() << need to adjust Accept expiry check.............
// it performs cleanup and other functions
int mastercore_handler_block_end(int nBlockNow, CBlockIndex const * pBlockIndex,
        unsigned int countMP)
{
    const CConsensusParams &params = ConsensusParams();

    int nMastercoreInit;
    {
        LOCK(cs_tally);
        nMastercoreInit = mastercoreInitialized;
    }

    if (!nMastercoreInit) {
        mastercore_init();
    }


    bool checkpointValid;
    {
       LOCK(cs_tally);

       /** Node rewards **/
       if (nBlockNow > params.MSC_NODE_REWARD_BLOCK)
       {
           // last blockhash
           const CBlockIndex* pblockindex = chainActive[nBlockNow - 1];
           const std::string& blockhash = pblockindex->GetBlockHash().GetHex();
           nR.sendNodeReward(blockhash, nBlockNow, RegTest());
       }

       /** Contracts **/
       if (nBlockNow > params.MSC_CONTRACTDEX_BLOCK)
       {
           LiquidationEngine(nBlockNow);
           // bS.makeSettlement();
       }

       // deleting Expired DEx accepts
       const unsigned int how_many_erased = eraseExpiredAccepts(nBlockNow);

       if (how_many_erased) {
          PrintToLog("%s(%d); erased %u accepts this block, line %d, file: %s\n",
            __func__, how_many_erased, nBlockNow, __LINE__, __FILE__);
       }

       // check the alert status, do we need to do anything else here?
       CheckExpiredAlerts(nBlockNow, pBlockIndex->GetBlockTime());

       // transactions were found in the block, signal the UI accordingly
       if (countMP > 0) CheckWalletUpdate(true);

       // calculate and print a consensus hash if required
       if (msc_debug_consensus_hash_every_block) {
           const uint256 consensusHash = GetConsensusHash();
           PrintToLog("Consensus hash for block %d: %s\n", nBlockNow, consensusHash.GetHex());
       }

       // request checkpoint verification
       checkpointValid = VerifyCheckpoint(nBlockNow, pBlockIndex->GetBlockHash());
       if (!checkpointValid) {
           // failed checkpoint, can't be trusted to provide valid data - shutdown client
           const std::string& msg = strprintf(
                   "Shutting down due to failed checkpoint for block %d (hash %s). "
                   "Please restart with -startclean flag and if this doesn't work, please reach out to the support.\n",
                   nBlockNow, pBlockIndex->GetBlockHash().GetHex());
           PrintToLog(msg);
           if (!gArgs.GetBoolArg("-overrideforcedshutdown", false)) {
               fs::path persistPath = GetDataDir() / "OCL_persist";
               if (fs::exists(persistPath)) fs::remove_all(persistPath); // prevent the node being restarted without a reparse after forced shutdown
               DoAbortNode(msg, msg);
           }
       }

     // check that pending transactions are still in the mempool
     PendingCheck();

     }

     LOCK2(cs_main, cs_tally);
     if (checkpointValid){
         // save out the state after this block
         if (writePersistence(nBlockNow) && nBlockNow >= params.GENESIS_BLOCK) {
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
        const std::string strKey = it->key().ToString();
        const std::string strValue = it->value().ToString();
        std::vector<std::string> vecValues;
        if (strKey.size() != 64) continue; // only interested in trades
        const uint256 txid = uint256S(strKey);
        const size_t addressMatch = strValue.find(address);
        if (addressMatch == std::string::npos) continue;
        boost::split(vecValues, strValue, boost::is_any_of(":"), token_compress_on);
        if (vecValues.size() != 5) {
            // PrintToLog("TRADEDB error - unexpected number of tokens in value (%s)\n", strValue);
            continue;
        }
        const uint32_t propertyIdForSale = boost::lexical_cast<uint32_t>(vecValues[1]);
        const uint32_t propertyIdDesired = boost::lexical_cast<uint32_t>(vecValues[2]);
        const int64_t blockNum = boost::lexical_cast<uint32_t>(vecValues[3]);
        const int64_t txIndex = boost::lexical_cast<uint32_t>(vecValues[4]);
        if (propertyIdFilter != 0 && propertyIdFilter != propertyIdForSale && propertyIdFilter != propertyIdDesired) continue;
        std::string sortKey = strprintf("%06d%010d", blockNum, txIndex);
        mapTrades.insert(std::make_pair(sortKey, txid));
    }
    delete it;
    for (const auto &m : mapTrades) {
        vecTransactions.push_back(m.second);
    }
}

static bool CompareTradePair(const std::pair<int64_t, UniValue>& firstJSONObj, const std::pair<int64_t, UniValue>& secondJSONObj)
{
    return firstJSONObj.first > secondJSONObj.first;
}

// obtains an array of matching trades with pricing and volume details for a pair sorted by blocknumber
void CMPTradeList::getTradesForPair(uint32_t propertyIdSideA, uint32_t propertyIdSideB, UniValue& responseArray, uint64_t count)
{
    if (!pdb) return;
    leveldb::Iterator* it = NewIterator();
    std::vector<std::pair<int64_t, UniValue> > vecResponse;
    bool propertyIdSideAIsDivisible = isPropertyDivisible(propertyIdSideA);
    bool propertyIdSideBIsDivisible = isPropertyDivisible(propertyIdSideB);
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        const std::string strKey = it->key().ToString();
        const std::string strValue = it->value().ToString();
        std::vector<std::string> vecKeys;
        std::vector<std::string> vecValues;
        uint256 sellerTxid, matchingTxid;
        std::string sellerAddress, matchingAddress;
        int64_t amountReceived = 0, amountSold = 0;
        if (strKey.size() != 129) continue; // only interested in matches
        boost::split(vecKeys, strKey, boost::is_any_of("+"), boost::token_compress_on);
        boost::split(vecValues, strValue, boost::is_any_of(":"), boost::token_compress_on);
        if (vecKeys.size() != 2 || vecValues.size() != 8) {
            // PrintToLog("TRADEDB error - unexpected number of tokens (%s:%s)\n", strKey, strValue);
            continue;
        }

        const uint32_t tradePropertyIdSideA = boost::lexical_cast<uint32_t>(vecValues[2]);
        const uint32_t tradePropertyIdSideB = boost::lexical_cast<uint32_t>(vecValues[3]);

        if (tradePropertyIdSideA == propertyIdSideA && tradePropertyIdSideB == propertyIdSideB) {
            sellerTxid.SetHex(vecKeys[1]);
            sellerAddress = vecValues[1];
            amountSold = boost::lexical_cast<int64_t>(vecValues[4]);
            matchingTxid.SetHex(vecKeys[0]);
            matchingAddress = vecValues[0];
            amountReceived = boost::lexical_cast<int64_t>(vecValues[5]);
        } else if (tradePropertyIdSideB == propertyIdSideA && tradePropertyIdSideA == propertyIdSideB) {
            sellerTxid.SetHex(vecKeys[0]);
            sellerAddress = vecValues[0];
            amountSold = boost::lexical_cast<int64_t>(vecValues[5]);
            matchingTxid.SetHex(vecKeys[1]);
            matchingAddress = vecValues[1];
            amountReceived = boost::lexical_cast<int64_t>(vecValues[4]);
        } else {
            continue;
        }

        rational_t unitPrice(amountReceived, amountSold);
        rational_t inversePrice(amountSold, amountReceived);
        if (!propertyIdSideAIsDivisible) unitPrice = unitPrice / COIN;
        if (!propertyIdSideBIsDivisible) inversePrice = inversePrice / COIN;
        const std::string unitPriceStr = xToString(unitPrice); // TODO: not here!
        const std::string inversePriceStr = xToString(inversePrice);

        const int64_t blockNum = boost::lexical_cast<int64_t>(vecValues[6]);

        UniValue trade(UniValue::VOBJ);
        trade.pushKV("block", blockNum);
        trade.pushKV("unitprice", unitPriceStr);
        trade.pushKV("inverseprice", inversePriceStr);
        trade.pushKV("sellertxid", sellerTxid.GetHex());
        trade.pushKV("selleraddress", sellerAddress);
        if (propertyIdSideAIsDivisible) {
            trade.pushKV("amountsold", FormatDivisibleMP(amountSold));
        } else {
            trade.pushKV("amountsold", FormatIndivisibleMP(amountSold));
        }
        if (propertyIdSideBIsDivisible) {
            trade.pushKV("amountreceived", FormatDivisibleMP(amountReceived));
        } else {
            trade.pushKV("amountreceived", FormatIndivisibleMP(amountReceived));
        }
        trade.pushKV("matchingtxid", matchingTxid.GetHex());
        trade.pushKV("matchingaddress", matchingAddress);
        vecResponse.push_back(std::make_pair(blockNum, trade));
    }

    // sort the response most recent first before adding to the array
    std::sort(vecResponse.begin(), vecResponse.end(), CompareTradePair);

    uint64_t elements = 0;
    std::vector<std::pair<int64_t, UniValue>> aux;
    for_each(vecResponse.begin(), vecResponse.end(), [&aux, &elements, &count] (const std::pair<int64_t, UniValue> p) { if(elements < count) aux.push_back(p); elements++; });

    //reversing vector and pushing back
    for (std::vector<std::pair<int64_t, UniValue>>::reverse_iterator it = aux.rbegin(); it != aux.rend(); it++){
        responseArray.push_back(it->second);
    }

    delete it;
}

// obtains an array of trades in DEx
void CMPTxList::getDExTrades(const std::string& address, uint32_t propertyId, UniValue& responseArray, uint64_t count)
{
  if (!pdb) return;
  std::vector<std::pair<int64_t, UniValue> > vecResponse;
  leveldb::Iterator* it = NewIterator();
  for(it->SeekToFirst(); it->Valid(); it->Next()) {
      const std::string strKey = it->key().ToString();
      const std::string strValue = it->value().ToString();
      std::vector<std::string> vecValues;
      if (strKey.size() != 66) continue;
      std::string strtxid = strKey.substr (0,64);
      const size_t addressMatch = strValue.find(address);
      if (addressMatch == std::string::npos) continue;
      boost::split(vecValues, strValue, boost::is_any_of(":"), token_compress_on);
      if (vecValues.size() != 7) {
          // PrintToLog("TRADEDB error - unexpected number of tokens in value (%s)\n", strValue);
          continue;
      }

      const uint32_t prop = boost::lexical_cast<uint32_t>(vecValues[4]);
      if (propertyId != 0 && propertyId != prop) continue;
      const int blockNum = boost::lexical_cast<int>(vecValues[0]);
      const std::string& buyer = vecValues[2];
      const std::string& seller = vecValues[3];
      uint64_t amount = boost::lexical_cast<uint64_t>(vecValues[5]);
      uint64_t amountPaid = boost::lexical_cast<uint64_t>(vecValues[6]);

      // calculate unit price and updated amount of litecoin desired
      arith_uint256 aUnitPrice = 0;
      if (amountPaid > 0) {
          aUnitPrice = (ConvertTo256(COIN) * (ConvertTo256(amountPaid)) / ConvertTo256(amount)) ; // divide by zero protection
      }

      const int64_t unitPrice = (isPropertyDivisible(propertyId)) ? ConvertTo64(aUnitPrice) : ConvertTo64(aUnitPrice) / COIN;

      UniValue trade(UniValue::VOBJ);
      trade.pushKV("block", blockNum);
      trade.pushKV("buyertxid", strtxid);
      trade.pushKV("selleraddress", seller);
      trade.pushKV("buyeraddress", buyer);
      trade.pushKV("propertyid", (uint64_t) propertyId);
      trade.pushKV("amountbuyed", FormatMP(propertyId, amount));
      trade.pushKV("amountpaid", FormatDivisibleMP(amountPaid));
      trade.pushKV("unitprice", FormatDivisibleMP(unitPrice));

      vecResponse.push_back(std::make_pair(blockNum, trade));
  }

  // sort the response most recent first before adding to the array
  std::sort(vecResponse.begin(), vecResponse.end(), CompareTradePair);

  uint64_t elements = 0;
  std::vector<std::pair<int64_t, UniValue>> aux;
  for_each(vecResponse.begin(), vecResponse.end(), [&aux, &elements, &count] (const std::pair<int64_t, UniValue> p) { if(elements < count) aux.push_back(p); elements++; });

  //reversing vector and pushing back
  for (std::vector<std::pair<int64_t, UniValue>>::reverse_iterator it = aux.rbegin(); it != aux.rend(); it++){
      responseArray.push_back(it->second);
  }

  delete it;
}


// token/ LTC trades on channels
void CMPTxList::getChannelTrades(const std::string& address, const std::string& channel, uint32_t propertyId, UniValue& responseArray, uint64_t count)
{
  if (!pdb) return;
  leveldb::Iterator* it = NewIterator();
  std::vector<std::pair<int64_t, UniValue> > vecResponse;
  for(it->SeekToFirst(); it->Valid(); it->Next()) {
      const std::string strKey = it->key().ToString();
      const std::string strValue = it->value().ToString();
      std::vector<std::string> vecValues;
      if (strKey.size() < 64) continue;
      int pos = strKey.find("+");
      const std::string strtxid = strKey.substr(pos + 1);
      boost::split(vecValues, strValue, boost::is_any_of(":"), token_compress_on);
      if (vecValues.size() != 9) {
          // PrintToLog("TRADEDB error - unexpected number of tokens in value (%s)\n", strValue);
          continue;
      }

      const int type  = boost::lexical_cast<int>(vecValues[8]);
      if(type != MSC_TYPE_INSTANT_LTC_TRADE) continue;

      const uint32_t prop = boost::lexical_cast<uint32_t>(vecValues[3]);
      if (propertyId != 0 && propertyId != prop) continue;

      const std::string& chn = vecValues[0];

      if (channel != chn) continue;

      const std::string& seller = vecValues[1];
      const std::string& buyer = vecValues[2];

      if (address != seller && address != buyer) continue;

      const uint64_t amount = boost::lexical_cast<uint64_t>(vecValues[4]);
      const uint64_t amountPaid = boost::lexical_cast<uint64_t>(vecValues[5]);
      const int blockNum = boost::lexical_cast<int>(vecValues[6]);

      arith_uint256 aUnitPrice = 0;
      if (amountPaid > 0) {
          aUnitPrice = (ConvertTo256(COIN) * (ConvertTo256(amountPaid)) / ConvertTo256(amount)) ; // divide by zero protection
      }

      const int64_t unitPrice = (isPropertyDivisible(propertyId)) ? ConvertTo64(aUnitPrice) : ConvertTo64(aUnitPrice) / COIN;

      UniValue trade(UniValue::VOBJ);
      trade.pushKV("block", blockNum);
      trade.pushKV("buyertxid", strtxid);
      trade.pushKV("selleraddress", seller);
      trade.pushKV("buyeraddress", buyer);
      trade.pushKV("channeladdress", channel);
      trade.pushKV("propertyid", (uint64_t) propertyId);
      trade.pushKV("amountbuyed", FormatMP(propertyId, amount));
      trade.pushKV("amountpaid", FormatDivisibleMP(amountPaid));
      trade.pushKV("unitprice", FormatDivisibleMP(unitPrice));

      vecResponse.push_back(std::make_pair(blockNum, trade));
  }

  // sort the response most recent first before adding to the array
  std::sort(vecResponse.begin(), vecResponse.end(), CompareTradePair);

  uint64_t elements = 0;
  std::vector<std::pair<int64_t, UniValue>> aux;
  for_each(vecResponse.begin(), vecResponse.end(), [&aux, &elements, &count] (const std::pair<int64_t, UniValue> p) { if(elements < count) aux.push_back(p); elements++; });

  //reversing vector and pushing back
  for (std::vector<std::pair<int64_t, UniValue>>::reverse_iterator it = aux.rbegin(); it != aux.rend(); it++){
      responseArray.push_back(it->second);
  }

  delete it;
}


void CMPTradeList::getTokenChannelTrades(const std::string& address, const std::string& channel, uint32_t propertyId, UniValue& responseArray, uint64_t count)
{
  if (!pdb) return;
  leveldb::Iterator* it = NewIterator();
  std::vector<std::pair<int64_t, UniValue> > vecResponse;
  for(it->SeekToFirst(); it->Valid(); it->Next()) {
      const std::string strKey = it->key().ToString();
      const std::string strValue = it->value().ToString();
      std::vector<std::string> vecValues;
      if (strKey.size() < 64) continue;

      int pos = strKey.find("+");
      const std::string strtxid = strKey.substr(pos + 1);

      boost::split(vecValues, strValue, boost::is_any_of(":"), token_compress_on);
      if (vecValues.size() != 10) {
          // PrintToLog("TRADEDB error - unexpected number of tokens in value (%s)\n", strValue);
          continue;
      }

      const std::string& type  = vecValues[9];

      // NOTE: improve this type string here (better: int)
      if(type != TYPE_INSTANT_TRADE) continue;

      const uint32_t prop1 = boost::lexical_cast<uint32_t>(vecValues[3]);
      const uint32_t prop2 = boost::lexical_cast<uint32_t>(vecValues[5]);
      if (propertyId != 0 && propertyId != prop1 && propertyId != prop2) continue;

      const std::string& channel = vecValues[0];
      const std::string& buyer = vecValues[1];
      const std::string& seller = vecValues[2];
      const uint64_t amountForSale = boost::lexical_cast<uint64_t>(vecValues[4]);
      const uint64_t amountDesired = boost::lexical_cast<uint64_t>(vecValues[6]);
      const int blockNum = boost::lexical_cast<int>(vecValues[7]);

      rational_t unitPrice(amountDesired, amountForSale);
      if (!prop1) unitPrice = unitPrice / COIN;
      const std::string unitPriceStr = xToString(unitPrice);

      UniValue trade(UniValue::VOBJ);
      trade.pushKV("block", blockNum);
      trade.pushKV("buyertxid", strtxid);
      trade.pushKV("selleraddress", seller);
      trade.pushKV("buyeraddress", buyer);
      trade.pushKV("channeladdress", channel);
      trade.pushKV("propertyid", (uint64_t) prop1);
      trade.pushKV("amountforsale", FormatMP(prop1, amountForSale));
      trade.pushKV("propertydesired", (uint64_t) prop2);
      trade.pushKV("amountdesired", FormatMP(prop2, amountDesired));
      trade.pushKV("unitprice", unitPriceStr);

      vecResponse.push_back(std::make_pair(blockNum, trade));
  }

  // sort the response most recent first before adding to the array
  std::sort(vecResponse.begin(), vecResponse.end(), CompareTradePair);

  uint64_t elements = 0;
  std::vector<std::pair<int64_t, UniValue>> aux;
  for_each(vecResponse.begin(), vecResponse.end(), [&aux, &elements, &count] (const std::pair<int64_t, UniValue> p) { if(elements < count) aux.push_back(p); elements++; });

  //reversing vector and pushing back
  for (std::vector<std::pair<int64_t, UniValue>>::reverse_iterator it = aux.rbegin(); it != aux.rend(); it++){
      responseArray.push_back(it->second);
  }

  delete it;
}

void CMPTradeList::getChannelTradesForPair(const std::string& channel, uint32_t propertyIdA, uint32_t propertyIdB, UniValue& responseArray, uint64_t count)
{
  if (!pdb) return;
  leveldb::Iterator* it = NewIterator();
  std::vector<std::pair<int64_t, UniValue> > vecResponse;
  for(it->SeekToFirst(); it->Valid(); it->Next()) {
      const std::string strKey = it->key().ToString();
      const std::string strValue = it->value().ToString();
      std::vector<std::string> vecValues;
      if (strKey.size() < 64) continue;

      int pos = strKey.find("+");
      const std::string strtxid = strKey.substr(pos + 1);

      boost::split(vecValues, strValue, boost::is_any_of(":"), token_compress_on);
      if (vecValues.size() != 10) {
          // PrintToLog("TRADEDB error - unexpected number of tokens in value (%s)\n", strValue);
          continue;
      }

      const std::string& type  = vecValues[9];

      // NOTE: improve this type string here (better: int)
      if(type != TYPE_INSTANT_TRADE) continue;

      const uint32_t prop1 = boost::lexical_cast<uint32_t>(vecValues[3]);
      const uint32_t prop2 = boost::lexical_cast<uint32_t>(vecValues[5]);

      //checking first property
      if (propertyIdA != 0 && propertyIdA != prop1 && propertyIdA != prop2) continue;
      //checking second property
      if (propertyIdB != 0 && propertyIdB != prop1 && propertyIdB != prop2) continue;


      const std::string& channel = vecValues[0];
      const std::string& buyer = vecValues[1];
      const std::string& seller = vecValues[2];
      const uint64_t amountForSale = boost::lexical_cast<uint64_t>(vecValues[4]);
      const uint64_t amountDesired = boost::lexical_cast<uint64_t>(vecValues[6]);
      const int blockNum = boost::lexical_cast<int>(vecValues[7]);

      rational_t unitPrice(amountDesired, amountForSale);
      if (!propertyIdA) unitPrice = unitPrice / COIN;
      const std::string unitPriceStr = xToString(unitPrice);

      UniValue trade(UniValue::VOBJ);
      trade.pushKV("block", blockNum);
      trade.pushKV("buyertxid", strtxid);
      trade.pushKV("selleraddress", seller);
      trade.pushKV("buyeraddress", buyer);
      trade.pushKV("channeladdress", channel);
      trade.pushKV("propertyid", (uint64_t) prop1);
      trade.pushKV("amountforsale", FormatMP(prop1, amountForSale));
      trade.pushKV("propertydesired", (uint64_t) prop2);
      trade.pushKV("amountdesired", FormatMP(prop2, amountDesired));
      trade.pushKV("unitprice", unitPriceStr);

      vecResponse.push_back(std::make_pair(blockNum, trade));
  }

  // sort the response most recent first before adding to the array
  std::sort(vecResponse.begin(), vecResponse.end(), CompareTradePair);

  uint64_t elements = 0;
  std::vector<std::pair<int64_t, UniValue>> aux;
  for_each(vecResponse.begin(), vecResponse.end(), [&aux, &elements, &count] (const std::pair<int64_t, UniValue> p) { if(elements < count) aux.push_back(p); elements++; });

  //reversing vector and pushing back
  for (std::vector<std::pair<int64_t, UniValue>>::reverse_iterator it = aux.rbegin(); it != aux.rend(); it++){
      responseArray.push_back(it->second);
  }

  delete it;
}

void CMPTradeList::recordNewTrade(const uint256& txid, const std::string& address, uint32_t propertyIdForSale, uint32_t propertyIdDesired, int blockNum, int blockIndex)
{
    if (!pdb) return;
    const std::string strValue = strprintf("%s:%d:%d:%d:%d", address, propertyIdForSale, propertyIdDesired, blockNum, blockIndex);
    Status status = pdb->Put(writeoptions, txid.ToString(), strValue);
    ++nWritten;
    if (msc_debug_tradedb) PrintToLog("%s(): %s\n", __FUNCTION__, status.ToString());
}

void CMPTradeList::recordNewTrade(const uint256& txid, const std::string& address, uint32_t propertyIdForSale, uint32_t propertyIdDesired, int blockNum, int blockIndex, int64_t reserva)
{
    if (!pdb) return;
    const std::string strValue = strprintf("%s:%d:%d:%d:%d:%d", address, propertyIdForSale, propertyIdDesired, blockNum, blockIndex,reserva);
    Status status = pdb->Put(writeoptions, txid.ToString(), strValue);
    ++nWritten;
    if (msc_debug_tradedb) PrintToLog("%s(): %s\n", __FUNCTION__, status.ToString());
}

void CMPTradeList::recordNewChannel(const std::string& channelAddress, const std::string& frAddr, const std::string& secAddr, int blockIndex)
{
    if (!pdb) return;
    const std::string strValue = strprintf("%s:%s:%s:%s", frAddr, secAddr, ACTIVE_CHANNEL, TYPE_CREATE_CHANNEL);
    Status status = pdb->Put(writeoptions, channelAddress, strValue);
    ++nWritten;

    if (msc_debug_tradedb) PrintToLog("%s(): %s\n", __FUNCTION__, status.ToString());
    PrintToLog("%s(): %s\n", __func__, status.ToString());
}

void CMPTradeList::recordNewInstantTrade(const uint256& txid, const std::string& channelAddr, const std::string& first, const std::string& second, uint32_t propertyIdForSale, uint64_t amount_forsale, uint32_t propertyIdDesired, uint64_t amount_desired,int blockNum, int blockIndex)
{
    if (!pdb) return;
    const std::string strValue = strprintf("%s:%s:%s:%d:%d:%d:%d:%d:%d:%s", channelAddr, first, second, propertyIdForSale, amount_forsale, propertyIdDesired, amount_desired, blockNum, blockIndex, TYPE_INSTANT_TRADE);
    const string key = to_string(blockNum) + "+" + txid.ToString(); // order by blockNum
    Status status = pdb->Put(writeoptions, key, strValue);
    ++nWritten;
    if (msc_debug_tradedb) PrintToLog("%s(): %s\n", __func__, status.ToString());
}

void CMPTxList::recordNewInstantLTCTrade(const uint256& txid, const std::string& channelAddr, const std::string& seller, const std::string& buyer, uint32_t propertyIdForSale, uint64_t amount_purchased, uint64_t price, int blockNum, int blockIndex)
{
    if (!pdb) return;
    const std::string strValue = strprintf("%s:%s:%s:%d:%d:%d:%d:%d:%s", channelAddr, seller, buyer, propertyIdForSale, amount_purchased, price, blockNum, blockIndex, MSC_TYPE_INSTANT_LTC_TRADE);
    const string key = to_string(blockNum) + "+" + txid.ToString(); // order by blockNum
    Status status = pdb->Put(writeoptions, key, strValue);
    ++nWritten;
    if (msc_debug_tradedb) PrintToLog("%s(): %s\n", __func__, status.ToString());
}

void CMPTradeList::recordNewIdRegister(const uint256& txid, const std::string& address, const std::string& name, const std::string& website, int blockNum, int blockIndex)
{
    if (!pdb) return;
    const int nextId = t_tradelistdb->getNextId();
    const std::string strValue = strprintf("%s:%s:%s:%d:%d:%d:%s:%s", address, name, website, blockNum, blockIndex, nextId, txid.ToString(), TYPE_NEW_ID_REGISTER);
    const string key = to_string(blockNum) + "+" + txid.ToString(); // order by blockNum
    Status status = pdb->Put(writeoptions, key, strValue);

    ++nWritten;
    PrintToLog("%s: %s\n", __FUNCTION__, status.ToString());
}


void CMPTradeList::recordNewAttestation(const uint256& txid, const std::string& sender, const std::string& receiver, int blockNum, int blockIndex, int kyc_id)
{
    if (!pdb) return;
    const std::string strValue = strprintf("%s:%s:%d:%d:%d:%s:%s", sender, receiver, blockNum, blockIndex, kyc_id, txid.ToString(), TYPE_ATTESTATION);
    const string key = to_string(blockNum) + "+" + txid.ToString(); // order by blockNum
    Status status = pdb->Put(writeoptions, key, strValue);

    ++nWritten;
    PrintToLog("%s: %s\n", __FUNCTION__, status.ToString());
}

bool CMPTradeList::updateIdRegister(const uint256& txid, const std::string& address,  const std::string& newAddr, int blockNum, int blockIndex)
{
    bool update = false;
    if (!pdb) return update;
    std::string strKey, newKey, newValue;
    std::vector<std::string> vstr;

    leveldb::Iterator* it = NewIterator(); // Allocation proccess

    for(it->SeekToLast(); it->Valid(); it->Prev())
    {
        // search key to see if this is a matching trade
        strKey = it->key().ToString();
        const std::string& strValue = it->value().ToString();

        // ensure correct amount of tokens in value string
        boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);
        if (vstr.size() != 12)
        {
            // PrintToLog("TRADEDB error - unexpected number of tokens in value (%s)\n", strValue);
            continue;
        }

        const std::string& type = vstr[10];

        if(msc_debug_update_id_register) PrintToLog("%s: type: %s\n",__func__,type);

        if( type != TYPE_NEW_ID_REGISTER)
          continue;

        if(msc_debug_update_id_register) PrintToLog("%s: strKey: %s\n", __func__, strKey);

        if(address != strKey)
            continue;

        const std::string& website = vstr[0];
        const std::string& name = vstr[1];
        const std::string& tokens = vstr[2];
        const std::string& ltc = vstr[3];
        const std::string& natives = vstr[4];
        const std::string& oracles = vstr[5];
        const std::string& nextId = vstr[8];

        newValue = strprintf("%s:%s:%s:%s:%s:%s:%d:%d:%s:%s:%s", website, name, tokens, ltc, natives, oracles, blockNum, blockIndex, nextId,txid.ToString(), TYPE_NEW_ID_REGISTER);

        update = true;
        break;

    }

    // clean up
    delete it;

    if(update)
    {
        Status status1 = pdb->Delete(writeoptions, strKey);
        Status status2 = pdb->Put(writeoptions, newAddr, newValue);
        ++nWritten;

        if(msc_debug_update_id_register)
        {
            PrintToLog("%s: %s\n", __FUNCTION__, status1.ToString());
            PrintToLog("%s: %s\n", __FUNCTION__, status2.ToString());
        }

        return (status1.ok() && status2.ok());

    }

    return update;

}

bool CMPTradeList::checkKYCRegister(const std::string& address, int& kyc_id)
{
    bool status = false;
    if (!pdb) return status;
    std::string strKey, newKey, newValue;
    std::vector<std::string> vstr;

    leveldb::Iterator* it = NewIterator(); // Allocation proccess

    for(it->SeekToLast(); it->Valid(); it->Prev())
    {
        // search key to see if this is a matching trade
        const std::string& strKey = it->key().ToString();
        const std::string& strValue = it->value().ToString();

        // ensure correct amount of tokens in value string
        boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);
        if (vstr.size() != 8)
        {
            // PrintToLog("TRADEDB error - unexpected number of tokens in value (%s)\n", strValue);
            continue;
        }

        const std::string& type = vstr[7];

        if(msc_debug_check_kyc_register) PrintToLog("%s: type: %s\n", __func__, type);

        if(type != TYPE_NEW_ID_REGISTER)
            continue;

        const std::string& regAddr = vstr[0];

        if(msc_debug_check_kyc_register) PrintToLog("%s: regAddr: %s, address: %s\n", __func__, regAddr, address);


        if(address != regAddr) continue;

        status = true;

        if(msc_debug_check_kyc_register) PrintToLog("%s: Address Found! %s\n", __func__, address);

        // returning the kyc_id
        kyc_id = boost::lexical_cast<int>(vstr[5]);

        if(msc_debug_check_kyc_register) PrintToLog("%s: kyc_id %s\n", __func__, kyc_id);

        break;

    }

    // clean up
    delete it;

    return status;
}

bool CMPTradeList::checkAttestationReg(const std::string& address, int& kyc_id)
{
    bool status = false;
    if (!pdb) return status;
    std::string newKey, newValue;
    std::vector<std::string> vstr;

    leveldb::Iterator* it = NewIterator(); // Allocation proccess

    for(it->SeekToLast(); it->Valid(); it->Prev())
    {
        // search key to see if this is a matching trade
        const std::string& strKey = it->key().ToString();
        const std::string& strValue = it->value().ToString();

        if(msc_debug_check_attestation_reg) PrintToLog("%s(): strValue: %s\n", __func__, strValue);

        // ensure correct amount of tokens in value string
        boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);

        if (vstr.size() != 7)
            continue;

        const std::string& type = vstr[6];

        if(msc_debug_check_attestation_reg) PrintToLog("%s(): type: %s\n", __func__, type);

        if(type != TYPE_ATTESTATION)
            continue;

        const std::string& regAddr = vstr[1];

        if(msc_debug_check_attestation_reg) PrintToLog("%s(): regAddr: %s, address: %s\n", __func__, regAddr, address);

        if(address != regAddr) continue;

        status = true;

        if(msc_debug_check_attestation_reg) PrintToLog("%s(): Address Found! %s\n", __func__, address);

        // returning the kyc_id
        kyc_id = boost::lexical_cast<int>(vstr[4]);

        if(msc_debug_check_attestation_reg) PrintToLog("%s(): kyc_id %d\n", __func__,kyc_id);

        break;

    }

    // clean up
    delete it;

    return status;
}

bool CMPTradeList::deleteAttestationReg(const std::string& sender,  const std::string& receiver)
{
    bool found = false;
    if (!pdb) return found;
    std::string strKey;
    std::vector<std::string> vstr;

    leveldb::Iterator* it = NewIterator(); // Allocation proccess

    for(it->SeekToLast(); it->Valid(); it->Prev())
    {
        // search key to see if this is a matching trade
        strKey = it->key().ToString();
        const std::string& strValue = it->value().ToString();

        // ensure correct amount of tokens in value string
        boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);

        const std::string& regAddr = vstr[0];
        const std::string& type = vstr[6];

        if (vstr.size() != 7 || type != TYPE_ATTESTATION || sender != regAddr)
            continue;

        const std::string& regRec = vstr[1];

        if(receiver != regRec)
            continue;

        found = true;
        break;

    }

    // clean up
    delete it;

    if (found) {
        Status status1 = pdb->Delete(writeoptions, strKey);

        if(msc_debug_delete_att_register)
        {
          PrintToLog("%s: %s\n", __FUNCTION__, status1.ToString());
        }

        ++nWritten;
    }

    return found;
}

bool CMPTradeList::kycLoop(UniValue& response)
{
    bool status = false;
    if (!pdb) return status;
    std::string newKey, newValue;
    std::vector<std::string> vstr;

    leveldb::Iterator* it = NewIterator(); // Allocation proccess

    for(it->SeekToLast(); it->Valid(); it->Prev())
    {
        // search key to see if this is a matching trade
        const std::string& strKey = it->key().ToString();
        const std::string& strValue = it->value().ToString();

        // ensure correct amount of tokens in value string
        boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);
        if (vstr.size() != 8)
        {
            // PrintToLog("TRADEDB error - unexpected number of tokens in value (%s)\n", strValue);
            continue;
        }

        const std::string& type = vstr[7];

        if( type != TYPE_NEW_ID_REGISTER)
          continue;

        const std::string& regAddr = vstr[0];
        const std::string& name = vstr[1];
        const std::string& website = vstr[2];
        int blockNum = boost::lexical_cast<int>(vstr[3]);

        // returning the kyc_id
        int kyc_id = boost::lexical_cast<int>(vstr[5]);

        UniValue propertyObj(UniValue::VOBJ);
        propertyObj.push_back(Pair("address", regAddr));
        propertyObj.push_back(Pair("name", name));
        propertyObj.push_back(Pair("website", website));
        propertyObj.push_back(Pair("block", (uint64_t) blockNum));
        propertyObj.push_back(Pair("kyc id", (uint64_t) kyc_id));

        response.push_back(propertyObj);
        status = true;
    }

    // clean up
    delete it;

    return status;
}

bool CMPTradeList::kycConsensusHash(CSHA256& hasher)
{
    if (!pdb) {
       PrintToLog("%s(): Error loading KYC for consensus hashing, hash should not be trusted!\n",__func__);
       return false;
    }

    leveldb::Iterator* it = NewIterator();
    for(it->SeekToLast(); it->Valid(); it->Prev())
    {
        // search key to see if this is a matching trade
        const std::string& strKey = it->key().ToString();
        const std::string& strValue = it->value().ToString();
        std::vector<std::string> vstr;

        // ensure correct amount of tokens in value string
        boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);
        if (vstr.size() != 8) continue;

        const std::string& type = vstr[7];
        if( type != TYPE_NEW_ID_REGISTER) continue;

        std::string dataStr = kycGenerateConsensusString(vstr);

        if (msc_debug_consensus_hash) PrintToLog("Adding KYC entry to consensus hash: %s\n", dataStr);
        hasher.Write((unsigned char*)dataStr.c_str(), dataStr.length());

    }

    // clean up
    delete it;

    return true;
}

bool CMPTradeList::attLoop(UniValue& response)
{
    bool status = false;
    if (!pdb) return status;
    std::string newKey, newValue;
    std::vector<std::string> vstr;

    leveldb::Iterator* it = NewIterator(); // Allocation proccess

    for(it->SeekToLast(); it->Valid(); it->Prev())
    {
        // search key to see if this is a matching trade
        const std::string& strKey = it->key().ToString();
        const std::string& strValue = it->value().ToString();

        // ensure correct amount of tokens in value string
        boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);

        if (vstr.size() != 7)
            continue;

        std::string type = vstr[6];

        if(type != TYPE_ATTESTATION)
            continue;

        const std::string& sender = vstr[0];
        const std::string& receiver = vstr[1];
        uint64_t blockNum = boost::lexical_cast<uint64_t>(vstr[2]);
        uint64_t kyc_id = boost::lexical_cast<uint64_t>(vstr[4]);

        UniValue propertyObj(UniValue::VOBJ);

        propertyObj.push_back(Pair("att sender", sender));
        propertyObj.push_back(Pair("att receiver", receiver));
        propertyObj.push_back(Pair("kyc_id", kyc_id));
        propertyObj.push_back(Pair("block", blockNum));

        response.push_back(propertyObj);

    }

    // clean up
    delete it;

    return status;
}

bool CMPTradeList::attConsensusHash(CSHA256& hasher)
{
    if (!pdb) {
       PrintToLog("%s(): Error loading Attestation for consensus hashing, hash should not be trusted!\n",__func__);
       return false;
    }

    leveldb::Iterator* it = NewIterator();
    for(it->SeekToLast(); it->Valid(); it->Prev())
    {
        // search key to see if this is a matching trade
        const std::string& strKey = it->key().ToString();
        const std::string& strValue = it->value().ToString();
        std::vector<std::string> vstr;
        // ensure correct amount of tokens in value string
        boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);
        if (vstr.size() != 7)
            continue;

        std::string type = vstr[6];

        if(type != TYPE_ATTESTATION)
            continue;

        const std::string dataStr = attGenerateConsensusString(vstr);

        if (msc_debug_consensus_hash) PrintToLog("Adding Attestation entry to consensus hash: %s\n", dataStr);
        hasher.Write((unsigned char*)dataStr.c_str(), dataStr.length());
    }

    // clean up
    delete it;

    return true;
}

bool CMPTradeList::kycPropertyMatch(uint32_t propertyId, int kyc_id)
{
    CMPSPInfo::Entry sp;
    if (!_my_sps->getSP(propertyId, sp)) {
       return false; // property ID does not exist
    }

    // looking for the kyc id in sp register
    const std::vector<int64_t>& vecKyc = sp.kyc;

    if (vecKyc.empty()){
        PrintToLog("%s(): empty vector\n",__func__);
    }

    auto it = find_if(vecKyc.begin(), vecKyc.end(), [&kyc_id] (const int64_t& k) {return (kyc_id == k);});

    return (it != vecKyc.end());
}

bool CMPTradeList::kycContractMatch(uint32_t contractId, int kyc_id)
{
    CDInfo::Entry sp;
    if (!_my_cds->getCD(contractId, sp)) {
       PrintToLog("%s(): contract ID  (%d)does not exist\n",__func__, contractId);
       return false; // contract ID does not exist
    }

    // looking for the kyc id in sp register
    const std::vector<int64_t>& vecKyc = sp.kyc;

    if (vecKyc.empty()){
        PrintToLog("%s(): empty vector\n",__func__);
    }

    auto it = find_if(vecKyc.begin(), vecKyc.end(), [&kyc_id] (const int64_t& k) {return (kyc_id == k);});

    return (it != vecKyc.end());
}


void CMPTradeList::recordNewInstContTrade(const uint256& txid, const std::string& firstAddr, const std::string& secondAddr, uint32_t property, uint64_t amount_forsale, uint64_t price ,int blockNum, int blockIndex)
{
    if (!pdb) return;
    const std::string strValue = strprintf("%s:%s:%d:%d:%d:%d:%d:%s", firstAddr, secondAddr, property, amount_forsale, price, blockNum, blockIndex, TYPE_CONTRACT_INSTANT_TRADE);
    Status status = pdb->Put(writeoptions, txid.ToString(), strValue);
    ++nWritten;
    if (msc_debug_tradedb) PrintToLog("%s(): %s\n", __FUNCTION__, status.ToString());
}

void CMPTradeList::recordNewCommit(const uint256& txid, const std::string& channelAddress, const std::string& sender, uint32_t propertyId, uint64_t amountCommited, int blockNum, int blockIndex)
{
    if (!pdb) return;
    const std::string strValue = strprintf("%s:%s:%d:%d:%d:%d:%s", channelAddress, sender, propertyId, amountCommited, blockNum, blockIndex, TYPE_COMMIT);
    Status status = pdb->Put(writeoptions, txid.ToString(), strValue);
    ++nWritten;
    if (msc_debug_tradedb) PrintToLog("%s(): %s\n", __func__, status.ToString());
}

void CMPTradeList::recordNewWithdrawal(const uint256& txid, const std::string& channelAddress, const std::string& sender, uint32_t propertyId, uint64_t amountToWithdrawal, int blockNum, int blockIndex)
{
    if (!pdb) return;
    const std::string strValue = strprintf("%s:%s:%d:%d:%d:%d:%s:%d", channelAddress, sender, propertyId, amountToWithdrawal, blockNum, blockIndex,TYPE_WITHDRAWAL, ACTIVE_WITHDRAWAL);
    Status status = pdb->Put(writeoptions, txid.ToString(), strValue);
    ++nWritten;
    if (msc_debug_tradedb) PrintToLog("%s(): %s\n", __FUNCTION__, status.ToString());
}

void CMPTradeList::recordNewTransfer(const uint256& txid, const std::string& sender, const std::string& receiver, int blockNum, int blockIndex)
{
    if (!pdb) return;
    const std::string strValue = strprintf("%s:%s:%d:%d:%s", sender, receiver, blockNum, blockIndex, TYPE_TRANSFER);
    Status status = pdb->Put(writeoptions, txid.ToString(), strValue);
    ++nWritten;
    if (msc_debug_tradedb) PrintToLog("%s(): %s\n", __FUNCTION__, status.ToString());
}

void CMPTradeList::recordMatchedTrade(const uint256 txid1, const uint256 txid2, string address1, string address2, unsigned int prop1, unsigned int prop2, uint64_t amount1, uint64_t amount2, int blockNum, int64_t fee)
{
    if (!pdb) return;
    const string key = txid1.ToString() + "+" + txid2.ToString();
    const string value = strprintf("%s:%s:%u:%u:%lu:%lu:%d:%d", address1, address2, prop1, prop2, amount1, amount2, blockNum, fee);

    int64_t volumeALL64_t = 0;
    /********************************************************************/
    if (prop1 == TL_PROPERTY_ALL)
    {
        if (msc_debug_tradedb) PrintToLog("factorALLtoLTC =%s, amount1 = %s: CMPMetaDEx\n", FormatDivisibleMP(factorALLtoLTC), FormatDivisibleMP(amount1));
        arith_uint256 volumeALL256_t = mastercore::ConvertTo256(factorALLtoLTC)*mastercore::ConvertTo256(amount1)/COIN;
        if (msc_debug_tradedb) PrintToLog("ALLs involved in the traded 256 Bits ~ %s ALL\n", volumeALL256_t.ToString());
        volumeALL64_t = mastercore::ConvertTo64(volumeALL256_t);
        if (msc_debug_tradedb) PrintToLog("ALLs involved in the traded 64 Bits ~ %s ALL\n", FormatDivisibleMP(volumeALL64_t));
      }
      else if (prop2 == TL_PROPERTY_ALL)
      {
          if (msc_debug_tradedb) PrintToLog("factorALLtoLTC =%s, amount1 = %s: CMPMetaDEx\n", FormatDivisibleMP(factorALLtoLTC), FormatDivisibleMP(amount2));
          arith_uint256 volumeALL256_t = mastercore::ConvertTo256(factorALLtoLTC)*mastercore::ConvertTo256(amount2)/COIN;
          if (msc_debug_tradedb) PrintToLog("ALLs involved in the traded 256 Bits ~ %s ALL\n", volumeALL256_t.ToString());
          volumeALL64_t = mastercore::ConvertTo64(volumeALL256_t);
          if (msc_debug_tradedb) PrintToLog("ALLs involved in the traded 64 Bits ~ %s ALL\n", FormatDivisibleMP(volumeALL64_t));
      }

      if (msc_debug_tradedb)
      {
          PrintToLog("Number of Traded Contracts ~ %s LTC\n", FormatDivisibleMP(volumeALL64_t));
          PrintToLog("\nGlobal LTC Volume No Updated: CMPMetaDEx = %s \n", FormatDivisibleMP(globalVolumeALL_LTC));
      }

      globalVolumeALL_LTC += volumeALL64_t;
      if (msc_debug_tradedb) PrintToLog("\nGlobal LTC Volume Updated: CMPMetaDEx = %s\n", FormatDivisibleMP(globalVolumeALL_LTC));

      /****************************************************/
      /** Building TWAP vector MDEx **/
      
      uint32_t property_all = getTokenDataByName("ALL").data_propertyId;
      uint32_t property_usd = getTokenDataByName("dUSD").data_propertyId;

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
          if (msc_debug_tradedb) PrintToLog("%s(): %s\n", __FUNCTION__, status.ToString());
      }
}

void CMPTradeList::recordMatchedTrade(const uint256 txid1, const uint256 txid2, string address1, string address2, uint64_t effective_price, uint64_t amount_maker, uint64_t amount_taker, int blockNum1, int blockNum2, uint32_t property_traded, string tradeStatus, int64_t lives_s0, int64_t lives_s1, int64_t lives_s2, int64_t lives_s3, int64_t lives_b0, int64_t lives_b1, int64_t lives_b2, int64_t lives_b3, string s_maker0, string s_taker0, string s_maker1, string s_taker1, string s_maker2, string s_taker2, string s_maker3, string s_taker3, int64_t nCouldBuy0, int64_t nCouldBuy1, int64_t nCouldBuy2, int64_t nCouldBuy3,uint64_t amountpnew, uint64_t amountpold)
{
  if (!pdb) return;
  std::map<std::string, std::string> edgeEle;
  std::map<std::string, double>::iterator it_addrs_upnlm;
  std::map<uint32_t, std::map<std::string, double>>::iterator it_addrs_upnlc;
  std::vector<std::map<std::string, std::string>>::iterator it_path_ele;
  std::vector<std::map<std::string, std::string>>::reverse_iterator reit_path_ele;
  //std::vector<std::map<std::string, std::string>> path_eleh;
  // bool savedata_bool = false;
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
  // fileSixth.open ("graphInfoSixth.txt", std::fstream::in | std::fstream::out | std::fstream::app);
  // if ( status_bool1 || status_bool2 )
  //   {
  //     if ( s_maker3 == "EmptyStr" && s_taker3 == "EmptyStr" ) savedata_bool = true;
  //     saveDataGraphs(fileSixth, line1, line2, line3, savedata_bool);
  //   }
  // else saveDataGraphs(fileSixth, line0);
  // fileSixth.close();

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

  // unsigned int contractId = static_cast<unsigned int>(property_traded);
  CDInfo::Entry cd;
  assert(_my_cds->getCD(property_traded, cd));
  uint32_t NotionalSize = cd.notional_size;

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

  // int64_t volumeToCompare = 0;
  // bool perpetualBool = callingPerpetualSettlement(globalPNLALL_DUSD, globalVolumeALL_DUSD, volumeToCompare);
  // if (perpetualBool) PrintToLog("Perpetual Settlement Online");

  // PrintToLog("\nglobalPNLALL_DUSD = %d, globalVolumeALL_DUSD = %d, contractId = %d\n", globalPNLALL_DUSD, globalVolumeALL_DUSD, contractId);

  // std::fstream fileglobalPNLALL_DUSD;
  // fileglobalPNLALL_DUSD.open ("globalPNLALL_DUSD.txt", std::fstream::in | std::fstream::out | std::fstream::app);
  // if ( contractId == 5 ) // just fot testing
  //   saveDataGraphs(fileglobalPNLALL_DUSD, std::to_string(globalPNLALL_DUSD));
  // fileglobalPNLALL_DUSD.close();

  // std::fstream fileglobalVolumeALL_DUSD;
  // fileglobalVolumeALL_DUSD.open ("globalVolumeALL_DUSD.txt", std::fstream::in | std::fstream::out | std::fstream::app);
  // if ( contractId == 5 )
  //   saveDataGraphs(fileglobalVolumeALL_DUSD, std::to_string(FormatShortIntegerMP(globalVolumeALL_DUSD)));
  // fileglobalVolumeALL_DUSD.close();

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
    } else if ( globalVolumeALL_DUSD > volumeToCompare ){
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
				   address1, s_status1, FormatContractShortMP(lives_maker * COIN),
				   address2, s_status2, FormatContractShortMP(lives_taker * COIN),
				   FormatContractShortMP(nCouldBuy * COIN), FormatContractShortMP(effective_price));

  return lineOut;
}

void buildingEdge(std::map<std::string, std::string> &edgeEle, std::string addrs_src, std::string addrs_trk, std::string status_src, std::string status_trk, int64_t lives_src, int64_t lives_trk, int64_t amount_path, int64_t matched_price, int idx_q, int ghost_edge)
{
  edgeEle["addrs_src"]     = addrs_src;
  edgeEle["addrs_trk"]     = addrs_trk;
  edgeEle["status_src"]    = status_src;
  edgeEle["status_trk"]    = status_trk;
  edgeEle["lives_src"]     = std::to_string(FormatShortIntegerMP(lives_src * COIN));
  edgeEle["lives_trk"]     = std::to_string(FormatShortIntegerMP(lives_trk * COIN));
  edgeEle["amount_trd"]    = std::to_string(FormatShortIntegerMP(amount_path * COIN));
  edgeEle["matched_price"] = std::to_string(FormatContractShortMP(matched_price));
  edgeEle["edge_row"]      = std::to_string(idx_q);
  edgeEle["ghost_edge"]    = std::to_string(ghost_edge);
}

void printing_edges_database(std::map<std::string, std::string> &path_ele)
{
    PrintToLog("{ addrs_src : %s , status_src : %s, lives_src : %d, addrs_trk : %s , status_trk : %s, lives_trk : %d, amount_trd : %d, matched_price : %d, edge_row : %d, ghost_edge : %d }\n", path_ele["addrs_src"], path_ele["status_src"], path_ele["lives_src"], path_ele["addrs_trk"], path_ele["status_trk"], path_ele["lives_trk"], path_ele["amount_trd"], path_ele["matched_price"], path_ele["edge_row"], path_ele["ghost_edge"]);
}

bool CMPTradeList::getMatchingTrades(uint32_t propertyId, UniValue& tradeArray)
{
    if (!pdb) return false;
    int count = 0;
    std::vector<std::string> vstr;
    leveldb::Iterator* it = NewIterator(); // Allocation proccess

    for(it->SeekToLast(); it->Valid(); it->Prev())
    {
        // search key to see if this is a matching trade
        const std::string& strKey = it->key().ToString();
        const std::string& strValue = it->value().ToString();

        // ensure correct amount of tokens in value string
        boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);
        if (vstr.size() != 17) {
            //PrintToLog("TRADEDB error - unexpected number of tokens in value (%s)\n", strValue);
            continue;
        }

        // decode the details from the value string
        const uint32_t prop1 = boost::lexical_cast<uint32_t>(vstr[11]);
        const std::string& address1 = vstr[0];
        const std::string& address2 = vstr[1];
        int64_t amount1 = boost::lexical_cast<int64_t>(vstr[15]);
        int64_t amount2 = boost::lexical_cast<int64_t>(vstr[16]);
        const int block = boost::lexical_cast<int>(vstr[6]);
        const int64_t price = boost::lexical_cast<int64_t>(vstr[2]);
        const int64_t amount_traded = boost::lexical_cast<int64_t>(vstr[14]);
        const std::string& txidmaker = vstr[12];
        const std::string& txidtaker = vstr[13];

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
    leveldb::Iterator* it = NewIterator(); // Allocation proccess

    for(it->SeekToLast(); it->Valid(); it->Prev()) {
        // search key to see if this is a matching trade
        const std::string& strKey = it->key().ToString();
        const std::string& strValue = it->value().ToString();

        // ensure correct amount of tokens in value string
        boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);
        if (vstr.size() != 17)
            continue;

        // decode the details from the value string
        // const string value = strprintf("%s:%s:%lu:%lu:%lu:%d:%d:%s:%s:%d:%d:%d:%s:%s:%d", address1, address2, effective_price, amount_maker, amount_taker, blockNum1,
        // blockNum2, s_maker0, s_taker0, lives_s0, lives_b0, property_traded, txid1.ToString(), txid2.ToString(), nCouldBuy0);
        const uint32_t prop1 = boost::lexical_cast<uint32_t>(vstr[11]);
        const std::string& address1 = vstr[0];
        const std::string& address2 = vstr[1];
        const int64_t amount1 = boost::lexical_cast<int64_t>(vstr[15]);
        const int64_t amount2 = boost::lexical_cast<int64_t>(vstr[16]);
        int block = boost::lexical_cast<int>(vstr[6]);
        const int64_t price = boost::lexical_cast<int64_t>(vstr[2]);
        const int64_t amount_traded = boost::lexical_cast<int64_t>(vstr[14]);
        const std::string& txidmaker = vstr[12];
        const std::string& txidtaker = vstr[13];

        // populate trade object and add to the trade array, correcting for orientation of trade

        if (prop1 != propertyId) {
            continue;
        }

        UniValue trade(UniValue::VOBJ);

        if (tradeArray.size() <= 10000)
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

bool CMPTradeList::getCreatedPegged(uint32_t propertyId, UniValue& tradeArray)
{
    if (!pdb) return false;

    int count = 0;
    std::vector<std::string> vstr;
    leveldb::Iterator* it = NewIterator(); // Allocation proccess

    for(it->SeekToFirst(); it->Valid(); it->Next()) {
        // search key to see if this is a matching trade
        const std::string& strKey = it->key().ToString();
        const std::string& strValue = it->value().ToString();

        // ensure correct amount of tokens in value string
        boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);
        if (vstr.size() != 5) {
            // PrintToLog("TRADEDB error - unexpected number of tokens in value (%s)\n", strValue);
            continue;
        }

        // decode the details from the value string
        const uint32_t propId = boost::lexical_cast<uint32_t>(vstr[1]);

        if (propId != propertyId) {
           continue;
        }

        const std::string& address = vstr[0];
        const int64_t height = boost::lexical_cast<int64_t>(vstr[2]);
        const int64_t amount = (boost::lexical_cast<int64_t>(vstr[3]) / COIN);
        const std::string& series = vstr[4];

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


// bool mastercore::marginMain(int Block)
// {
//   //checking in map for address and the UPNL.
//     if(msc_debug_margin_main) PrintToLog("%s: Block in marginMain: %d\n", __func__, Block);
//     LOCK(cs_tally);
//     uint32_t nextSPID = _my_sps->peekNextSPID();
//     for (uint32_t contractId = 1; contractId < nextSPID; contractId++)
//     {
//         CMPSPInfo::Entry sp;
//         if (_my_sps->getSP(contractId, sp))
//         {
//             if(msc_debug_margin_main) PrintToLog("%s: Property Id: %d\n", __func__, contractId);
//             if (!sp.isContract())
//             {
//                 if(msc_debug_margin_main) PrintToLog("%s: Property is not future contract\n",__func__);
//                 continue;
//             }
//         }
//
//         uint32_t collateralCurrency = sp.collateral_currency;
//         //int64_t notionalSize = static_cast<int64_t>(sp.notional_size);
//
//         // checking the upnl map
//         std::map<uint32_t, std::map<std::string, double>>::iterator it = addrs_upnlc.find(contractId);
//         std::map<std::string, double> upnls = it->second;
//
//         //  if upnls is < 0, we need to cancel orders or liquidate contracts.
//         for(std::map<std::string, double>::iterator it2 = upnls.begin(); it2 != upnls.end(); ++it2)
// 	      {
//             const std::string address = it2->first;
//
//             int64_t upnl = static_cast<int64_t>(it2->second * COIN);
//             if(msc_debug_margin_main) PrintToLog("%s: upnl: %d", __func__, upnl);
//             // if upnl is positive, keep searching
//             if (upnl >= 0)
//                 continue;
//
//             if(msc_debug_margin_main)
//             {
//                 PrintToLog("%s: upnl: %d", __func__, upnl);
//                 PrintToLog("%s: sum_check_upnl: %d",__func__, sum_check_upnl(address));
//             }
//             // if sum of upnl is bigger than this upnl, skip address.
//             if (sum_check_upnl(address) > upnl)
//                 continue;
//
//             // checking position margin
//             int64_t posMargin = pos_margin(contractId, address, sp.margin_requirement);
//
//             // if there's no position, something is wrong!
//             if (posMargin < 0)
//                 continue;
//
//             // checking the initMargin (init_margin = position_margin + active_orders_margin)
//             std::string channelAddr;
//             int64_t initMargin;
//
//             initMargin = getMPbalance(address,collateralCurrency,CONTRACTDEX_RESERVE);
//
//             rational_t percent = rational_t(-upnl,initMargin);
//
//             int64_t ordersMargin = initMargin - posMargin;
//
//             if(msc_debug_margin_main)
//             {
//                 PrintToLog("\n--------------------------------------------------\n");
//                 PrintToLog("\n%s: initMargin= %d\n", __func__, initMargin);
//                 PrintToLog("\n%s: positionMargin= %d\n", __func__, posMargin);
//                 PrintToLog("\n%s: ordersMargin= %d\n", __func__, ordersMargin);
//                 PrintToLog("%s: upnl= %d\n", __func__, upnl);
//                 PrintToLog("%s: factor= %d\n", __func__, factor);
//                 // PrintToLog("%s: proportion upnl/initMargin= %d\n", __func__, xToString(percent));
//                 PrintToLog("\n--------------------------------------------------\n");
//             }
//             // if the upnl loss is more than 80% of the initial Margin
//             if (factor <= percent)
//             {
//                 const uint256 txid;
//                 if(msc_debug_margin_main)
//                 {
//                     // PrintToLog("%s: factor <= percent : %d <= %d\n",__func__, xToString(factor), xToString(percent));
//                     PrintToLog("%s: margin call!\n", __func__);
//                 }
//
//                 ContractDex_CLOSE_POSITION(txid, Block, address, contractId, collateralCurrency);
//                 continue;
//
//             // if the upnl loss is more than 20% and minus 80% of the Margin
//             } else if (factor2 <= percent) {
//                 if(msc_debug_margin_main)
//                 {
//                     PrintToLog("%s: CALLING CANCEL IN ORDER\n", __func__);
//                     // PrintToLog("%s: factor2 <= percent : %s <= %s\n", __func__, xToString(factor2),xToString(percent));
//                 }
//
//                 int64_t fbalance, diff;
//                 int64_t margin = getMPbalance(address,collateralCurrency,CONTRACTDEX_RESERVE);
//                 int64_t ibalance = getMPbalance(address,collateralCurrency, BALANCE);
//                 int64_t left = - 0.2 * margin - upnl;
//
//                 bool orders = false;
//
//                 do
//                 {
//                       if(msc_debug_margin_main) PrintToLog("%s: margin before cancel: %s\n", __func__, margin);
//                       if(ContractDex_CANCEL_IN_ORDER(address, contractId) == 1)
//                           orders = true;
//                       fbalance = getMPbalance(address,collateralCurrency, BALANCE);
//                       diff = fbalance - ibalance;
//
//                       if(msc_debug_margin_main)
//                       {
//                           PrintToLog("%s: ibalance: %s\n", __func__, ibalance);
//                           PrintToLog("%s: fbalance: %s\n", __func__, fbalance);
//                           PrintToLog("%s: diff: %d\n", __func__, diff);
//                           PrintToLog("%s: left: %d\n", __func__, left);
//                       }
//
//                       if ( left <= diff && msc_debug_margin_main) {
//                           PrintToLog("%s: left <= diff !\n", __func__);
//                       }
//
//                       if (orders) {
//                           PrintToLog("%s: orders=true !\n", __func__);
//                       } else
//                          PrintToLog("%s: orders=false\n", __func__);
//
//                 } while(diff < left && !orders);
//
//                 // if left is negative, the margin is above the first limit (more than 80% maintMargin)
//                 if (0 < left)
//                 {
//                     if(msc_debug_margin_main)
//                     {
//                         PrintToLog("%s: orders can't cover, we have to check the balance to refill margin\n", __func__);
//                         PrintToLog("%s: left: %d\n", __func__, left);
//                     }
//                     //we have to see if we can cover this with the balance
//                     int64_t balance = getMPbalance(address,collateralCurrency,BALANCE);
//
//                     if(balance >= left) // recover to 80% of maintMargin
//                     {
//                         if(msc_debug_margin_main) PrintToLog("\n%s: balance >= left\n", __func__);
//                         update_tally_map(address, collateralCurrency, -left, BALANCE);
//                         update_tally_map(address, collateralCurrency, left, CONTRACTDEX_RESERVE);
//                         continue;
//
//                     } else { // not enough money in balance to recover margin, so we use position
//
//                          if(msc_debug_margin_main) PrintToLog("%s: not enough money in balance to recover margin, so we use position\n", __func__);
//                          if (balance > 0)
//                          {
//                              update_tally_map(address, collateralCurrency, -balance, BALANCE);
//                              update_tally_map(address, collateralCurrency, balance, CONTRACTDEX_RESERVE);
//                          }
//
//                          const uint256 txid;
//                          unsigned int idx = 0;
//                          int64_t fcontracts = 0;
//                          uint8_t option = 0;
//
//                          const int64_t longs = getMPbalance(address,contractId, CONTRACT_BALANCE);
//                          const int64_t shorts = getMPbalance(address,contractId, CONTRACT_BALANCE);
//
//                          if(msc_debug_margin_main) PrintToLog("%s: longs: %d,shorts: %d \n", __func__, longs,shorts);
//
//                          if(longs > 0 && shorts == 0) {
//                               option = buy;
//                              fcontracts = longs;
//                          } else {
//                              option = sell;
//                              fcontracts = shorts;
//                          }
//
//                          if(msc_debug_margin_main) PrintToLog("%s(): upnl: %d, posMargin: %d\n", __func__, upnl,posMargin);
//
//                          arith_uint256 contracts = DivideAndRoundUp(ConvertTo256(posMargin) + ConvertTo256(-upnl), ConvertTo256(sp.margin_requirement));
//                          int64_t icontracts = ConvertTo64(contracts);
//
//                          if(msc_debug_margin_main)
//                          {
//                              PrintToLog("%s: icontracts: %d\n", __func__, icontracts);
//                              PrintToLog("%s: fcontracts before: %d\n", __func__, fcontracts);
//                          }
//
//                          if (icontracts > fcontracts)
//                              icontracts = fcontracts;
//
//                          if(msc_debug_margin_main) PrintToLog("%s: fcontracts after: %d\n", __func__, fcontracts);
//
//                          ContractDex_ADD_MARKET_PRICE(address, contractId, icontracts, Block, txid, idx, option, 0);
//
//
//                     }
//
//                 }
//
//             } else {
//                 if(msc_debug_margin_main) PrintToLog("%s: the upnl loss is LESS than 20% of the margin, nothing happen\n", __func__);
//
//             }
//         }
//     }
//
//     return true;
//
// }

int64_t getMinMargin(const uint32_t contractId, const int64_t& position, uint64_t marginRequirement)
{
      const int64_t margin = (int64_t) (position *  marginRequirement) / 2; // 50%

      if(msc_debug_liquidation_enginee) PrintToLog("%s(): position: %d, margin requirement: %d, total margin required: %d\n",__func__, position, marginRequirement, margin);

      return margin;
}

//check position for a given address of this contractId
bool checkContractPositions(int Block, const std::string &address, const uint32_t contractId, const CDInfo::Entry& sp, const Register& reg)
{
    if(msc_debug_liquidation_enginee)
    {
        PrintToLog("%s(): inside checkContractPositions \n",__func__);
    }

    int64_t position =  reg.getRecord(contractId, CONTRACT_POSITION);

    // we need an active position
    if (0 == position) return false;

    const int64_t reserve = getMPbalance(address, sp.collateral_currency, CONTRACTDEX_RESERVE);

    if(msc_debug_liquidation_enginee)
    {
        PrintToLog("%s(): contractdex reserve for address (%s): %d\n",__func__, address, reserve);
    }

    const int64_t upnl = reg.getUPNL(contractId, sp.notional_size, sp.isOracle(), sp.isInverseQuoted());

    // absolute value needed
    position = abs(position);

    const int64_t min_margin = getMinMargin(contractId, position, sp.margin_requirement); //(min margin: 50% requirements for position)

    const int64_t sum = reserve + upnl;

    if(msc_debug_liquidation_enginee)
    {
        PrintToLog("%s(): min_margin: %d, upnl: %d, sum: %d, reserve: %d\n",__func__, min_margin, upnl, sum, reserve);
    }

    // if sum is less than min margin, liquidate the half position
    if(sum < min_margin)
    {
        if(msc_debug_liquidation_enginee) PrintToLog("%s(): sum < min_margin\n",__func__);

        // even or odd
        int64_t icontracts = ((position % 2) == 0) ?  (position / 2) : (position - 1) / 2;

        // if position has just 1 contract, we just close it.
        if (0 == icontracts) icontracts++;

        // NOTE: to avoid this, we need to update the ContractDex constructor
        uint256 txid;
        unsigned int idx = 0;

        // trying to liquidate 50% of position
        if(msc_debug_liquidation_enginee) PrintToLog("%s(): here trying to liquidate 50 percent of position, contractId: %d, icontracts : %d, Block: %d\n",__func__,  contractId, icontracts, Block);

        // if whe don't have pending liquidation orders here! then we add the order
        ContractDex_ADD_MARKET_PRICE(address, contractId, icontracts, Block, txid, idx, (position > 0) ? sell : buy, 0, true);

        // TODO: add here a position checking in order to see if we are above the margin limits.
    } else  {
        if(msc_debug_liquidation_enginee) PrintToLog("%s(): sum >= min_margin, we are Ok\n",__func__);
    }


    return true;

}


bool mastercore::LiquidationEngine(int Block)
{

    const uint32_t nextCDID = _my_cds->peekNextContractID();

    if(msc_debug_liquidation_enginee) PrintToLog("%s(): inside LiquidationEngine, nextCDID: %d\n",__func__, nextCDID);

    for (uint32_t contractId = 1; contractId < nextCDID; contractId++)
    {
         CDInfo::Entry sp;
         if (!_my_cds->getCD(contractId, sp))
         {
             continue;
         }

         if(msc_debug_liquidation_enginee) PrintToLog("%s(): contractId: %d\n",__func__, contractId);

         // we check each position for this contract Id
         LOCK(cs_register);

         for_each(mp_register_map.begin(),mp_register_map.end(), [contractId, Block, &sp](const std::unordered_map<std::string, Register>::value_type& unmap){ checkContractPositions(Block, unmap.first, contractId, sp, unmap.second);});

   }

   return true;

}

/* margin needed for a given position */
int64_t mastercore::pos_margin(uint32_t contractId, const std::string& address, uint64_t margin_requirement)
{
        arith_uint256 maintMargin;

        LOCK(cs_register);
        CDInfo::Entry sp;

        if(!_my_cds->getCD(contractId, sp))
        {
            if(msc_debug_pos_margin) PrintToLog("%s(): future contract not found\n", __func__);
            return -1;
        }

        int64_t longs = getMPbalance(address,contractId, CONTRACT_BALANCE);
        int64_t shorts = getMPbalance(address,contractId, CONTRACT_BALANCE);

        if(msc_debug_pos_margin)
        {
            PrintToLog("%s: longs: %d, shorts: %d\n", __func__, longs,shorts);
            PrintToLog("%s: margin requirement: %d\n", __func__, margin_requirement);
        }

        if (longs > 0 && shorts == 0)
        {
            maintMargin = (ConvertTo256(longs) * ConvertTo256(margin_requirement)) / ConvertTo256(COIN);
        } else if (shorts > 0 && longs == 0){
            maintMargin = (ConvertTo256(shorts) * ConvertTo256(margin_requirement)) / ConvertTo256(COIN);
        } else {
            if(msc_debug_pos_margin) PrintToLog("%s(): there's no position avalaible\n", __func__);
            return -2;
        }

        int64_t maint_margin = ConvertTo64(maintMargin);
        if(msc_debug_pos_margin) PrintToLog("%s(): maint margin: %d\n", __func__, maint_margin);
        return maint_margin;
}


bool mastercore::makeWithdrawals(int Block)
{
    for(auto it = withdrawal_Map.begin(); it != withdrawal_Map.end(); ++it)
    {
        const std::string& channelAddress = it->first;
        vector<withdrawalAccepted> &accepted = it->second;

        for (auto itt = accepted.begin() ; itt != accepted.end(); )
        {
            const withdrawalAccepted& wthd = *itt;
            const int deadline = wthd.deadline_block;

            if (Block < deadline){
                ++itt;
                continue;
            }

            const std::string& address = wthd.address;
            const uint32_t propertyId = wthd.propertyId;
            const int64_t amount = static_cast<int64_t>(wthd.amount);

            //checking channel
            auto it = channels_Map.find(channelAddress);
            assert(it != channels_Map.end());
            Channel &chn = it->second;

            if(!chn.updateChannelBal(address, propertyId, -amount))
            {
                if(msc_debug_make_withdrawal) PrintToLog("%s(): withdrawal is not possible\n",__func__);
                itt = accepted.erase(itt++);
                continue;
            }

            if(msc_debug_make_withdrawal) PrintToLog("%s(): withdrawal: actual block: %d, deadline: %d, address: %s, propertyId: %d, amount: %d, txid: %s \n", __func__, Block, deadline, address, propertyId, amount, wthd.txid.ToString());

            // updating tally map
            assert(update_tally_map(address, propertyId, amount, BALANCE));

            // deleting element from vector
            itt = accepted.erase(itt++);


        }

    }

    return true;

}

bool mastercore::checkWithdrawal(const std::string& txid, const std::string& channelAddress)
{
    bool found = false;
    auto it =  withdrawal_Map.find(channelAddress);
    if(it != withdrawal_Map.end())
    {
        const auto &wVec = it->second;
        auto itt = std::find_if (wVec.begin(), wVec.end(), [&txid](const withdrawalAccepted& w){ return (txid == w.txid.ToString());});
        if (itt != wVec.end()){
            found = true;
        }
    }

    return found;

}

/* True if channel contain some pending withdrawal*/
bool isChannelWithdraw(const std::string& address)
{
    auto it =  withdrawal_Map.find(address);
    if(it == withdrawal_Map.end()) {
        return false;
    }

    vector<withdrawalAccepted> &accepted = it->second;

    return ((accepted.empty()) ? false : true);

}

bool mastercore::closeChannel(const std::string& sender, const std::string& channelAddr)
{
    bool fClosed = false;

    auto it = channels_Map.find(channelAddr);
    if (it != channels_Map.end())
    {
        PrintToLog("%s(): channel Found! (%s)\n",__func__,channelAddr);

        Channel &chn = it->second;

        if(!chn.isPartOfChannel(sender)) {
            return fClosed;
        }

        // no pending withdrawals
        if(isChannelWithdraw(channelAddr)) {
            PrintToLog("%s(): Pending withdrawal in Channel %s\n",__func__, channelAddr);
            return fClosed;
        }

        const uint32_t nextSPID = _my_sps->peekNextSPID();

        // retrieving funds from channel
        for (uint32_t propertyId = 1; propertyId < nextSPID; propertyId++)
        {
            CMPSPInfo::Entry sp;
            if (!_my_sps->getSP(propertyId, sp)) continue;

            const int64_t first_rem = chn.getRemaining(false, propertyId);
            const int64_t second_rem = chn.getRemaining(true, propertyId);

            if (first_rem > 0)
            {
                assert(chn.updateChannelBal(chn.getFirst(), propertyId, -first_rem));
                assert(update_tally_map(chn.getFirst(), propertyId, first_rem, BALANCE));
            }

            if (second_rem > 0)
            {
                assert(chn.updateChannelBal(chn.getSecond(), propertyId, -second_rem));
                assert(update_tally_map(chn.getSecond(), propertyId, second_rem, BALANCE));
            }

        }

        // adding info in db
        t_tradelistdb->setChannelClosed(channelAddr);

        // deleting channel from withdrawals
        auto itt = withdrawal_Map.find(channelAddr);
        if (itt != withdrawal_Map.end()){
            withdrawal_Map.erase(itt);
        }

        // deleting channel from Map
        channels_Map.erase(it);


        return (!fClosed);
    }

    return fClosed;
}


// true is both address are part of same channel
// bool mastercore::addressesInChannel(const std::string& fAddr, const std::string& sAddr)
// {
//     bool found = false;
//
//     for (const auto& chn : channels_Map)
//     {
//         if ((chn.getFirst() == fAddr && chn.getSecond() == sAddr) || (chn.getFirst() == sAddr && chn.getSecond() == fAddr)){
//             found = true;
//             break;
//         }
//     }
//
//     return found;
// }

uint64_t int64ToUint64(int64_t value)
{
    uint64_t uvalue = value;

    uvalue += INT64_MAX;
    uvalue += 1;

    return uvalue;
}

/**
 * @retrieve commits for a channel
 */
bool CMPTradeList::getAllCommits(const std::string& senderAddress, UniValue& tradeArray)
{
    int count = 0;
    if (!pdb) return count;
    std::vector<std::string> vstr;

    leveldb::Iterator* it = NewIterator(); // Allocation proccess

    for(it->SeekToLast(); it->Valid(); it->Prev()) {
        // search key to see if this is a matching trade
        const std::string& strKey = it->key().ToString();
        const std::string& strValue = it->value().ToString();

        // ensure correct amount of tokens in value string
        boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);
        if (vstr.size() != 7) {
            //PrintToLog("TRADEDB error - unexpected number of tokens in value (%s)\n", strValue);
            continue;
        }

        std::string type = vstr[6];

        if(type != TYPE_COMMIT)
            continue;

        const std::string& sender = vstr[1];

        if(sender != senderAddress)
            continue;

        const std::string& channel = vstr[0];
        const uint32_t propertyId = boost::lexical_cast<uint32_t>(vstr[2]);
        const int64_t amount = boost::lexical_cast<int64_t>(vstr[3]);
        const int blockNum = boost::lexical_cast<int>(vstr[4]);
        const int blockIndex = boost::lexical_cast<int>(vstr[5]);

        // populate trade object and add to the trade array, correcting for orientation of trade
        UniValue trade(UniValue::VOBJ);
        if (tradeArray.size() <= 100)
        {
            trade.push_back(Pair("sender", sender));
            trade.push_back(Pair("channel", channel));
            trade.push_back(Pair("propertyId",FormatByType(propertyId,1)));
            trade.push_back(Pair("amount", FormatByType(amount,2)));
            trade.push_back(Pair("block",blockNum));
            trade.push_back(Pair("block_index",blockIndex));
            tradeArray.push_back(trade);
            ++count;
        }
    }
    // clean up
    delete it;

    return ((count) ? true : false);
 }

/**
 * @retrieve  All commits (minus withdrawal and trades) for a given address into specific channel
 */
int64_t Channel::getRemaining(const std::string& address, uint32_t propertyId) const
{
    int64_t remaining = 0;
    auto it = balances.find(address);
    if (it != balances.end())
    {
        const auto &pMap = it->second;
        auto itt = pMap.find(propertyId);
        remaining = (itt != pMap.end()) ? itt->second : 0;
    }

    return remaining;
}

int64_t Channel::getRemaining(bool flag, uint32_t propertyId) const
{
    int64_t remaining = 0;
    const std::string& address = (flag) ? second : first;
    auto it = balances.find(address);
    if (it != balances.end())
    {
        const auto &pMap = it->second;
        auto itt = pMap.find(propertyId);
        remaining = (itt != pMap.end()) ? itt->second : 0;
    }

    return remaining;
}

  bool Channel::isPartOfChannel(const std::string& address) const
  {
       return (first == address || second == address);
  }
/**
 * @add or subtract tokens in trade channel, for a given address
 */
bool Channel::updateChannelBal(const std::string& address, uint32_t propertyId, int64_t amount)
{
    if (amount == 0){
        PrintToLog("%s(): nothing to update!\n", __func__);
        return false;
    }

    int64_t amount_remaining = 0;
    auto it = balances.find(address);
    if (it != balances.end())
    {
        const auto &pMap = it->second;
        auto itt = pMap.find(propertyId);
        if (itt != pMap.end()){
            amount_remaining = itt->second;
        }

    }

    if (isOverflow(amount_remaining, amount)) {
        PrintToLog("%s(): ERROR: arithmetic overflow [%d + %d]\n", __func__, amount_remaining, amount);
        return false;
    }

    if ((amount_remaining + amount) < 0)
    {
        PrintToLog("%s(): insufficient funds! (amount_remaining: %d, amount: %d)\n", __func__, amount_remaining, amount);
        return false;

    }

    amount_remaining += amount;
    setBalance(address, propertyId, amount_remaining);

    return true;

}

void Channel::setBalance(const std::string& sender, uint32_t propertyId, uint64_t amount)
{
    balances[sender][propertyId] = amount;
}

uint64_t CMPTradeList::addClosedWithrawals(const std::string& channelAddr, const std::string& receiver, uint32_t propertyId)
{
   if (!pdb) return 0;

   uint64_t totalAmount = 0;
   std::vector<std::string> vstr;

   leveldb::Iterator* it = NewIterator(); // Allocation proccess

   for(it->SeekToLast(); it->Valid(); it->Prev()) {
       // search key to see if this is a matching trade
       const std::string& strKey = it->key().ToString();
       const std::string& strValue = it->value().ToString();

       // ensure correct amount of tokens in value string
       boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);
       if (vstr.size() != 5) {
           //PrintToLog("TRADEDB error - unexpected number of tokens in value (%s)\n", strValue);
           continue;
       }

       //channelAddress, receiver, propertyId, amount, Block
       const std::string& cAddress = vstr[0];

       if(channelAddr != cAddress)
           continue;

       const std::string& rec = vstr[1];

       if(receiver != rec)
           continue;

       const uint32_t prop = boost::lexical_cast<uint32_t>(vstr[2]);

       if(propertyId != prop)
           continue;

       totalAmount += boost::lexical_cast<uint64_t>(vstr[3]);
   }

   // clean up
   delete it; // Desallocation proccess

   return totalAmount;

}

/**
 *  Does the channel address exist and it is active?
 */
bool mastercore::checkChannelAddress(const std::string& channelAddress)
{
    auto it = channels_Map.find(channelAddress);
    return ((it == channels_Map.end()) ? false : true);
}

/**
 *  Does the address is related to some active channel?
 */
bool CMPTradeList::checkChannelRelation(const std::string& address, std::string& channelAddr)
{
    auto it = find_if(channels_Map.begin(), channels_Map.end(), [&address] (const std::pair<std::string,Channel>& sChn) { return (sChn.second.getFirst() == address || sChn.second.getSecond() == address);});

    if (it == channels_Map.end())
    {
        PrintToLog("%s(): Channel not Found here!\n",__func__);
        return false;
    }

    const Channel& cfound = it->second;
    channelAddr = cfound.getMultisig();

    return true;
}

/**
* @return all info about Channel
*/
bool CMPTradeList::getChannelInfo(const std::string& channelAddress, UniValue& tradeArray)
{
    bool status = false;
    // checking actives
    auto it = channels_Map.find(channelAddress);
    if(it != channels_Map.end())
    {
        const Channel &chn = it->second;
        tradeArray.push_back(Pair("multisig address", chn.getMultisig()));
        tradeArray.push_back(Pair("first address", chn.getFirst()));
        tradeArray.push_back(Pair("second address", chn.getSecond()));
        tradeArray.push_back(Pair("status","active"));
        return (!status);
    }

    //checking in db for closed channels
    if (!pdb) return status;
    std::vector<std::string> vstr;
    std::string strValue;
    Status status1 = pdb->Get(readoptions, channelAddress, &strValue);

    if(!status1.ok()){
        PrintToLog("%s(): db error - channel not found\n", __func__);
        return false;
    }

    // ensure correct amount of tokens in value string
    boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);
    if (vstr.size() != 4) {
        PrintToLog("%s(): db error - incorrect size of register: (%s)\n", __func__, strValue);
    }

    const std::string& frAddr = vstr[0];
    const std::string& secAddr = vstr[1];
    const std::string& chanStatus = vstr[2];

    tradeArray.push_back(Pair("multisig address", channelAddress));
    tradeArray.push_back(Pair("first address", frAddr));
    tradeArray.push_back(Pair("second address", secAddr));
    tradeArray.push_back(Pair("status", chanStatus));

    return status1.ok();

}

 /**
  * @retrieve withdrawal for a given address in the channel
  */
bool CMPTradeList::getAllWithdrawals(const std::string& senderAddress, UniValue& tradeArray)
{
    if (!pdb) return false;
    int count = 0;
    std::vector<std::string> vstr;
    leveldb::Iterator* it = NewIterator(); // Allocation proccess

    for(it->SeekToLast(); it->Valid(); it->Prev())
    {
        // search key to see if this is a matching trade
        const std::string& txid = it->key().ToString();
        const std::string& strValue = it->value().ToString();

        // ensure correct amount of tokens in value string
        boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);
        if (vstr.size() != 8) {
            //PrintToLog("TRADEDB error - unexpected number of tokens in value (%s)\n", strValue);
            continue;
        }

        const std::string& type = vstr[6];

        if(type != TYPE_WITHDRAWAL)
            continue;

        const std::string& sender = vstr[1];

        if(sender != senderAddress)
            continue;

        const std::string& cAddress = vstr[0];
        const uint32_t propertyId = boost::lexical_cast<uint32_t>(vstr[2]);
        const int64_t withdrawalAmount = boost::lexical_cast<int64_t>(vstr[3]);
        const int blockNum = boost::lexical_cast<int>(vstr[4]);
        const int tx_index = boost::lexical_cast<int>(vstr[5]);
        const int st = boost::lexical_cast<int>(vstr[7]);

        const std::string status = (st == ACTIVE_WITHDRAWAL && checkWithdrawal(txid, cAddress)) ? "pending": "completed";

        // populate trade object and add to the trade array, correcting for orientation of trade
        UniValue trade(UniValue::VOBJ);
        if (tradeArray.size() <= 1000)
        {
            trade.push_back(Pair("channel_address", cAddress));
            trade.push_back(Pair("sender", sender));
            trade.push_back(Pair("withdrawal_amount", FormatByType(withdrawalAmount,2)));
            trade.push_back(Pair("property_id", FormatByType(propertyId,1)));
            trade.push_back(Pair("request_block", blockNum));
            trade.push_back(Pair("tx_index", tx_index));
            trade.push_back(Pair("status:", status));
            trade.push_back(Pair("tx_id", txid));
            tradeArray.push_back(trade);
            ++count;
        }
    }
    // clean up
    delete it; // Desallocation proccess
    if (count) { return true; } else { return false; }
}



/**
 * @retrieve if there's some withdrawal pending for a given address
 */
bool CMPTradeList::updateWithdrawal(const std::string& senderAddress, const std::string& channelAddress)
{
   if (!pdb) return false;
   bool found = false;
   std::string newValue;
   std::vector<std::string> vstr;
   std::string strKey;
   leveldb::Iterator* it = NewIterator(); // Allocation proccess

   for(it->SeekToLast(); it->Valid(); it->Prev())
   {
       // search key to see if this is a matching trade
       strKey = it->key().ToString();
       const std::string& strValue = it->value().ToString();

       // ensure correct amount of tokens in value string
       boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);
       if (vstr.size() != 8) {
           //PrintToLog("TRADEDB error - unexpected number of tokens in value (%s)\n", strValue);
           continue;
       }

       const std::string& type = vstr[6];

       if(type != TYPE_WITHDRAWAL)
           continue;

       const std::string& sender = vstr[1];

       if(sender != senderAddress)
           continue;

      const std::string& cAddress = vstr[0];

      if(cAddress != channelAddress)
          continue;

       const uint32_t propertyId = boost::lexical_cast<uint32_t>(vstr[2]);
       const int64_t withdrawalAmount = boost::lexical_cast<int64_t>(vstr[3]);
       const int blockNum = boost::lexical_cast<int>(vstr[4]);
       const int txid = boost::lexical_cast<int>(vstr[5]);

       newValue = strprintf("%s:%s:%d:%d:%d:%d:%s:%d", channelAddress, senderAddress, propertyId, withdrawalAmount, blockNum, txid, TYPE_WITHDRAWAL, COMPLETE_WITHDRAWAL);

       found = true;
       break;

   }
   // clean up
   delete it; // Desallocation proccess

   if (found) {
      Status status = pdb->Put(writeoptions, strKey, newValue);
      ++nWritten;

      return status.ok();
   }

   return found;

}
/**
* @return next id for kyc
*/
int CMPTradeList::getNextId()
{
    if (!pdb) return -1;

    int count = 0;
    std::vector<std::string> vstr;

    leveldb::Iterator* it = NewIterator(); // Allocation proccess

    for(it->SeekToLast(); it->Valid(); it->Prev()) {

        // search key to see if this is a matching trade
        const std::string& strKey = it->key().ToString();
        const std::string& strValue = it->value().ToString();

        // ensure correct amount of tokens in value string
        boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);
        if (vstr.size() != 8) {
            PrintToLog("TRADEDB error - unexpected number of tokens in value (%s)\n", strValue);
            continue;
        }

        const std::string& type = vstr[7];

        if(type != TYPE_NEW_ID_REGISTER)
            continue;

        count++;
    }

    // clean up
    delete it;

    count++;

    return count;

}

inline int64_t setPosition(int64_t positive, int64_t negative)
{
    if (positive > 0 && negative == 0)
        return positive;
    else if (positive == 0 && negative > 0)
        return -negative;
    else
        return 0;
}

std::string mastercore::updateStatus(int64_t oldPos, int64_t newPos)
{
    bool longSide = false; // old and new are long
    bool shortSide = false; // old and new are short

    int signOld = boost::math::sign(oldPos);
    int signNew = boost::math::sign(newPos);

    if(oldPos == newPos)
        return "None";

    if (signNew == 0)
        return ((signOld == 1) ? "LongPosNetted" : "ShortPosNetted");

    else if(signOld == 0)
        return ((signNew == 1) ? "OpenLongPosition" : "OpenShortPosition");


    if (signNew == 1 && signOld == -1){
        return "OpenLongPosByShortPosNetted";

    } else if (signNew == -1 && signOld == 1)
        return "OpenShortPosByLongPosNetted";

    if (signOld == -1 && signNew == -1)
        shortSide = true;

    else if (signOld == 1 && signNew == 1)
        longSide = true;

    if(newPos > oldPos) {
        if (shortSide)
            return"ShortPosNettedPartly";
        else
            return "LongPosIncreased";
    } else {
        if(longSide)
            return "LongPosNettedPartly";
        else
            return "ShortPosIncreased";
    }

    return "None";

}

bool mastercore::ContInst_Fees(const std::string& firstAddr, const std::string& secondAddr,const std::string& channelAddr, int64_t amountToReserve, uint16_t type, uint32_t colateral)
{
    arith_uint256 fee;

    if(msc_debug_contract_inst_fee) PrintToLog("%s(): firstAddr: %d, secondAddr: %d, amountToReserve: %d, colateral: %d\n", __func__, firstAddr, secondAddr, amountToReserve, colateral);

    switch (type)
    {
      case ALL_PROPERTY_TYPE_ORACLE_CONTRACT:
          // 1% basis point minus for firstAddr, 0.5% basis point minus for secondAddr
          fee = ConvertTo256(amountToReserve) / (ConvertTo256(100) * ConvertTo256(BASISPOINT));
          break;

      case ALL_PROPERTY_TYPE_NATIVE_CONTRACT:
          // 1.25% basis point minus each
          fee = (ConvertTo256(amountToReserve) * ConvertTo256(5)) / (ConvertTo256(4000) * ConvertTo256(COIN) * ConvertTo256(BASISPOINT));
          break;
    }

    const int64_t uFee = ConvertTo64(fee);
    const int64_t totalAmount = amountToReserve - uFee;

    if(msc_debug_contract_inst_fee) PrintToLog("%s: uFee: %d, amountToReserve: %d, totalAmount : %d\n",__func__, uFee, amountToReserve, totalAmount);

    // update_tally_map(channelAddr, colateral, -2 * uFee, CHANNEL_RESERVE);

    // % to native feecache
    g_fees->native_fees[colateral] += uFee;

    // % to oracle feecache
    g_fees->oracle_fees[colateral] += uFee;

    return true;
}


bool mastercore::Instant_x_Trade(const uint256& txid, uint8_t tradingAction, const std::string& channelAddr, const std::string& firstAddr, const std::string& secondAddr, uint32_t contractId, int64_t amount_forsale, uint64_t price, uint32_t collateral, uint16_t type, int& block, int tx_idx)
{

    const int64_t firstPoss = getContractRecord(firstAddr, contractId, CONTRACT_POSITION);
    const int64_t secondPoss = getContractRecord(secondAddr, contractId, CONTRACT_POSITION);

    const int64_t amount = (tradingAction == buy) ? -amount_forsale : amount_forsale;
    assert(update_register_map(firstAddr, contractId,  amount, CONTRACT_POSITION));
    assert(update_register_map(secondAddr, contractId, -amount, CONTRACT_POSITION));

    const int64_t newFirstPoss = getContractRecord(firstAddr, contractId, CONTRACT_POSITION);
    const int64_t newSecondPoss = getContractRecord(secondAddr, contractId, CONTRACT_POSITION);

    std::string Status_maker0 = mastercore::updateStatus(firstPoss, newFirstPoss);
    std::string Status_taker0 = mastercore::updateStatus(secondPoss, newSecondPoss);

    if(msc_debug_instant_x_trade)
    {
        PrintToLog("%s: old first position: %d, new first position: %d \n", __func__, firstPoss, newFirstPoss);
        PrintToLog("%s: old second position: %d, new second position: %d \n", __func__, secondPoss, newSecondPoss);
        PrintToLog("%s: Status_marker0: %s, Status_taker0: %s \n",__func__,Status_maker0, Status_taker0);
        PrintToLog("%s: amount_forsale: %d\n", __func__, amount_forsale);
    }

    int64_t newPosMaker = 0;
    int64_t newPosTaker = 0;

    if(msc_debug_instant_x_trade)
    {
        PrintToLog("%s: newPosMaker: %d\n", __func__, newPosMaker);
        PrintToLog("%s: newPosTaker: %d\n", __func__, newPosTaker);
    }

    /**
    * Adding LTC volume traded in contracts.
    *
    */
    mastercore::ContractDex_ADD_LTCVolume(amount_forsale, contractId);

    mastercore::ContInst_Fees(firstAddr, secondAddr, channelAddr, amount_forsale, type, collateral);

    t_tradelistdb->recordMatchedTrade(txid,
           txid,
           firstAddr,
           secondAddr,
           price,
           amount_forsale,
           amount_forsale,
           block,
           block,
           contractId,
           "Matched",
           newPosMaker,
           0,
           0,
           0,
           newPosTaker,
           0,
           0,
           0,
           Status_maker0,
           Status_taker0,
           "EmptyStr",
           "EmptyStr",
           "EmptyStr",
           "EmptyStr",
           "EmptyStr",
           "EmptyStr",
           amount_forsale,
           0,
           0,
           0,
           amount_forsale,
           amount_forsale);

    return true;
}

void mastercore::iterVolume(int64_t& amount, uint32_t propertyId, const int& fblock, const int& sblock, const std::map<int, std::map<uint32_t,int64_t>>& aMap)
{
    for(const auto &m : aMap)
    {
        const int& blk = m.first;
        if(blk < fblock && fblock != 0){
            continue;
        } else if(sblock < blk){
            break;
        }

        const auto &blockMap = m.second;
        auto itt = blockMap.find(propertyId);
        if (itt != blockMap.end()){
            const int64_t& newAmount = itt->second;
            // overflows?
            assert(!isOverflow(amount, newAmount));
            amount += newAmount;
        }

    }

}


/** DEx and Instant trade LTC volumes **/
int64_t mastercore::LtcVolumen(uint32_t propertyId, const int& fblock, const int& sblock)
{
    int64_t amount = 0;

    iterVolume(amount, propertyId, fblock, sblock, MapLTCVolume);

    return amount;
}

/** MetaDEx Tokens volume **/
int64_t mastercore::MdexVolumen(uint32_t property, const int& fblock, const int& sblock)
{
    int64_t amount = 0;

    iterVolume(amount, property, fblock, sblock, metavolume);

    return amount;
}

/** DEx Tokens volume **/
int64_t mastercore::DexVolumen(uint32_t property, const int& fblock, const int& sblock)
{
    int64_t amount = 0;

    iterVolume(amount, property, fblock, sblock, MapTokenVolume);

    return amount;
}

/** last 24 hours volume (set tokens = true for token volume) for a given propertyId **/
int64_t mastercore::lastVolume(uint32_t propertyId, bool tokens)
{
    // 24 hours back in time
    int bBlock = GetHeight() - dayblocks;
    const int lBlock = 999999999;

    if (bBlock < 0) {
        PrintToLog("%s(): blockHeight is less than 576 block\n",__func__);
        bBlock = 0;
    }

    int64_t totalAmount = 0;

    // DEx and Instant trade LTC volumes
    if (!tokens){
        iterVolume(totalAmount, propertyId, bBlock, lBlock, MapLTCVolume);
        PrintToLog("%s(): LTC volume: %d\n",__func__, totalAmount);
        return totalAmount;
    }

    // DEx token volume
    iterVolume(totalAmount, propertyId, bBlock, lBlock, MapTokenVolume);

    // MetaDEx token volume
    iterVolume(totalAmount, propertyId, bBlock, lBlock, metavolume);

    return totalAmount;

}

int64_t mastercore::getOracleTwap(uint32_t contractId, int nBlocks)
{
     int64_t sum = 0;
     auto it = oraclePrices.find(contractId);

     if (it != oraclePrices.end())
     {
         int count = 0;
         arith_uint256 aSum = 0;
         const auto &orMap = it->second;

         for(auto rit = orMap.rbegin(); rit != orMap.rend(); ++rit)
         {
             if(msc_debug_oracle_twap) PrintToLog("%s(): actual block: %d\n", __func__, rit->first);

             if (count >= nBlocks) break;

             const oracledata& ord = rit->second;
             aSum += (ConvertTo256(ord.high) + ConvertTo256(ord.low) + ConvertTo256(ord.close)) / ConvertTo256(3) ;
             count++;

             if(msc_debug_oracle_twap) PrintToLog("%s(): count: %d\n", __func__, count);
         }

         aSum /= ConvertTo256(nBlocks);
         sum = ConvertTo64(aSum);
         if(msc_debug_oracle_twap) PrintToLog("%s(): sum: %d\n", __func__, sum);
     }

     return sum;
}


int64_t mastercore::getContractTradesVWAP(uint32_t contractId, int nBlocks)
{
     int64_t sum = 0;
     auto it = cdexlastprice.find(contractId);

     if (it != cdexlastprice.end())
     {
         const auto& blockMap = it->second;
         for(auto itt = blockMap.rbegin() ; itt != blockMap.rend(), nBlocks > 0; itt++){
             const auto& v = itt->second;
             sum += std::accumulate(v.begin(), v.end(), 0);
             nBlocks--;
         }

     }

     PrintToLog("%s(): sum: %d\n", __func__, sum);

     return sum;
}


bool mastercore::sanityChecks(const std::string& sender, int& aBlock)
{
    if (getVestingAdmin() == sender){
        return true;
    }

    const CConsensusParams &params = ConsensusParams();
    if(msc_debug_sanity_checks) PrintToLog("%s(): vestingActivationBlock: %d\n", __func__, params.MSC_VESTING_BLOCK);

    const int timeFrame = aBlock - params.MSC_VESTING_BLOCK;

    if(msc_debug_sanity_checks) PrintToLog("%s(): timeFrame: %d\n", __func__, timeFrame);

    // is this the first transaction from address in the list?
    auto it = find_if(vestingAddresses.begin(), vestingAddresses.end(), [&sender, &timeFrame, &params] (const std::string address) { return (sender == address);});

    return (it != vestingAddresses.end());

}

bool mastercore::feeCacheBuy()
{
    bool result = false;

    if(g_fees->oracle_fees.empty())
    {
        if (msc_debug_fee_cache_buy) PrintToLog("%s(): cachefees_oracles is empty\n",__func__);
        return result;
    }


    for(auto &ca : g_fees->oracle_fees)
    {
        const uint32_t& propertyId = ca.first;

        if (propertyId == ALL)
            continue;

        uint64_t amount = static_cast<uint64_t>(ca.second);

        if (msc_debug_fee_cache_buy) PrintToLog("%s(): amount before trading (in cache): %d\n",__func__, amount);


        if(mastercore::MetaDEx_Search_ALL(amount, propertyId))
        {
            ca.second = amount;
            if (msc_debug_fee_cache_buy) PrintToLog("%s(): amount after trading (in cache): %d\n",__func__, amount);
            return true;

        } else {
            if (msc_debug_fee_cache_buy) PrintToLog("%s(): mDEx without ALL offers\n",__func__);

        }

    }

    return result;

}

/**
 *  Updating last exchange block in channels
 */
// bool Channel::updateLastExBlock(int nBlock)
// {
//     bool result = false;
//     auto it = channels_Map.find(multisig);
//     if (it == channels_Map.end())
//     {
//         if(msc_debug_update_last_block) PrintToLog("%s(): trade channel not found!; (channel: %s)\n",__func__, multisig);
//         return result;
//
//     }
//
//     const int difference = nBlock - getLastBlock();
//
//     assert(difference >= 0);
//
//     if(msc_debug_update_last_block) PrintToLog("%s: expiry height after update: %d\n",__func__, getExpiry());
//
//     if (difference < dayblocks)
//     {
//         // updating expiry_height
//         setLastBlock(difference);
//     }
//
//     return true;
// }

// long: true, short: false
bool getPosition(const std::string& status)
{
    auto it = std::find_if(longActions.begin(), longActions.end(), [status] (const std::string& s) { return (s == status); } );
    return ((it != longActions.end()) ? true : false);
}

void calculateUPNL(std::vector<double>& sum_upnl, uint64_t entry_price, uint64_t amount, uint64_t exit_price, bool position, bool inverse_quoted)
{
    double UPNL = 0;
    (inverse_quoted) ? UPNL = (double)amount * COIN * (1/(double)entry_price-1/(double)exit_price) : UPNL = (double) amount * COIN *(exit_price - entry_price);
    if(!position) UPNL *= -1;
    sum_upnl.push_back(UPNL);
}

void CMPTradeList::getUpnInfo(const std::string& address, uint32_t contractId, UniValue& response, bool showVerbose)
{
    if (!pdb) return;
    CDInfo::Entry sp;
    assert(_my_cds->getCD(contractId, sp));

    //twap of last 3 blocks
    const uint64_t exitPrice = getOracleTwap(contractId, OBLOCKS);

    int count = 0;
    std::vector<double> sumUpnl;
    leveldb::Iterator* it = NewIterator();

    for(it->SeekToFirst(); it->Valid(); it->Next())
    {
        const std::string& strKey = it->key().ToString();
        const std::string& strValue = it->value().ToString();
        std::vector<std::string> vecValues;

        boost::split(vecValues, strValue, boost::is_any_of(":"), token_compress_on);
        if (vecValues.size() != 17) {
            // PrintToLog("TRADEDB error - unexpected number of tokens in value (%s)\n", strValue);
            continue;
        }

        const std::string& address1 = vecValues[0];
        const std::string& address2 = vecValues[1];

        bool first = (address1 != address);
        bool second = (address2 != address);

        if(first && second){
            continue;
        }

        count++;

        if(msc_debug_get_upn_info) PrintToLog("%s(): searching for address: %s\n",__func__, address);

        const std::string& status1  = vecValues[7];
        const std::string& status2  = vecValues[8];

        PrintToLog("%s(): status1: %s, status2: %s\n",__func__, status1, status2);

        // taking the matched address and the status of that trade (short or long)
        std::pair <std::string, bool> args;
        (first) ? (args.first = address1, args.second = getPosition(status2)) : (args.first = address2, args.second = getPosition(status1));


        if (msc_debug_get_upn_info)
        {
            PrintToLog("%s(): first: %d\n",__func__, (first) ? 1 : 0);
            PrintToLog("%s(): strValue: %s\n",__func__, strValue);
            PrintToLog("%s(): position bool: %d\n",__func__, args.second);
            PrintToLog("%s(): address matched: %d\n",__func__, args.first);
        }

        const uint64_t price = boost::lexical_cast<uint64_t>(vecValues[2]);
        const uint64_t amount = boost::lexical_cast<uint64_t>(vecValues[14]);
        const uint64_t blockNum = boost::lexical_cast<uint64_t>(vecValues[6]);

        // partial upnl
        calculateUPNL(sumUpnl, price, amount, exitPrice, args.second, sp.isInverseQuoted());

        if(showVerbose)
        {
            UniValue registerObj(UniValue::VOBJ);
            registerObj.push_back(Pair("address matched", args.first));
            registerObj.push_back(Pair("entry price", FormatDivisibleMP(price)));
            registerObj.push_back(Pair("amount", amount));
            registerObj.push_back(Pair("block", blockNum));
            response.push_back(registerObj);
        }
    }

    delete it;

    const int64_t totalUpnl = accumulate(sumUpnl.begin(), sumUpnl.end(), 0.0) * COIN;
    UniValue upnlObj(UniValue::VOBJ);
    upnlObj.push_back(Pair("upnl", FormatDivisibleMP(totalUpnl, true)));
    response.push_back(upnlObj);

}

void mastercore::createChannel(const std::string& sender, const std::string& receiver,  uint32_t propertyId, uint64_t amount_commited, int block, int tx_id)
{
    Channel chn(receiver, sender, CHANNEL_PENDING, block);
    chn.setBalance(sender, propertyId, amount_commited);
    channels_Map[receiver] = chn;
    if(msc_create_channel) PrintToLog("%s(): checking channel elements : channel address: %s, first address: %d, second address: %d\n",__func__, chn.getMultisig(), chn.getFirst(), chn.getSecond());

    t_tradelistdb->recordNewChannel(chn.getMultisig(), chn.getFirst(), chn.getSecond(), tx_id);

}

/**
 *  Set channel status as closed
 */
bool CMPTradeList::setChannelClosed(const std::string& channelAddr)
{
    if (!pdb) return false;
    std::vector<std::string> vstr;
    std::string newValue, strValue;
    Status status = pdb->Get(readoptions, channelAddr, &strValue);

    if(!status.ok()){
        PrintToLog("%s(): db error - channel not found\n", __func__);
        return false;
    }

    // ensure correct amount of tokens in value string
    boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);

    if (vstr.size() != 4) {
        PrintToLog("%s(): db error - incorrect size of register: (%s)\n", __func__, strValue);
        return false;
    }

    const std::string& frAddr = vstr[0];
    const std::string& secAddr = vstr[1];

    newValue = strprintf("%s:%s:%s:%s",frAddr, secAddr, CLOSED_CHANNEL, TYPE_CREATE_CHANNEL);

    Status status1 = pdb->Put(writeoptions, channelAddr, newValue);
    ++nWritten;

    return (status.ok() && status1.ok());

}

/**
 *  If channel doesn't have second address, we try to add it
 */
bool CMPTradeList::tryAddSecond(const std::string& candidate, const std::string& channelAddr, uint32_t propertyId, uint64_t amount_commited)
{
    auto it = channels_Map.find(channelAddr);
    Channel& chn = it->second;
    Status status, status1;

    // updating db if address is a new one
    if(chn.getSecond() == CHANNEL_PENDING && chn.getFirst() != candidate)
    {
        chn.setSecond(candidate);

        // updating db register
        if (!pdb) return false;
        std::vector<std::string> vstr;
        std::string strValue, newValue;
        status = pdb->Get(readoptions, channelAddr, &strValue);

        if(!status.ok()){
            if(msc_debug_try_add_second) PrintToLog("%s(): db error - channel not found\n", __func__);
            return false;
        }

        // ensure correct amount of tokens in value string
        boost::split(vstr, strValue, boost::is_any_of(":"), token_compress_on);

        if (vstr.size() != 4) {
            if(msc_debug_try_add_second) PrintToLog("%s(): db error - incorrect size of register: (%s)\n", __func__, strValue);
            return false;
        }

        const std::string& frAddr = vstr[0];
        // const std::string& scAddr = vstr[1];

        assert(frAddr == chn.getFirst());

        newValue = strprintf("%s:%s:%s:%s",frAddr, candidate, ACTIVE_CHANNEL, TYPE_CREATE_CHANNEL);

        status1 = pdb->Put(writeoptions, channelAddr, newValue);
        ++nWritten;

    }

    // updating in memory
    chn.updateChannelBal(candidate, propertyId, amount_commited);

    return (status.ok() && status1.ok());

}


bool mastercore::channelSanityChecks(const std::string& sender, const std::string& receiver, uint32_t propertyId, uint64_t amount_commited, int block, int tx_idx)
{
    if(!checkChannelAddress(receiver))
    {
        createChannel(sender, receiver, propertyId, amount_commited, block, tx_idx);
        return true;
    }

    return (t_tradelistdb->tryAddSecond(sender, receiver, propertyId, amount_commited));

}

int64_t mastercore::getVWap(uint32_t propertyId, int aBlock, const std::map<uint32_t,std::map<int,std::vector<std::pair<int64_t,int64_t>>>>& aMap)
{
    int64_t volume = 0;
    arith_uint256 nvwap = 0;
    const int rollback = aBlock - 12;
    auto it = aMap.find(propertyId);
    if (it != aMap.end())
    {
        auto &vmap = it->second;
        auto itt = (rollback > 0) ? find_if(vmap.begin(), vmap.end(), [&rollback] (const std::pair<int,std::vector<std::pair<int64_t,int64_t>>>& int_arith_pair) { return (int_arith_pair.first >= rollback);}) : vmap.begin();
        if (itt != vmap.end())
        {
            for ( ; itt != vmap.end(); ++itt)
            {
                auto v = itt->second;
                for_each(v.begin(),v.end(), [&nvwap](const std::pair<int64_t,int64_t>& num){ nvwap += ConvertTo256(num.first * (num.second / COIN));});
            }
        }

    }

    // calculating the volume
    iterVolume(volume, propertyId, rollback, aBlock, MapLTCVolume);

    if (volume == 0) PrintToLog("%s():volume here is 0\n",__func__);

    return ((volume > 0) ? (COIN *(ConvertTo64(nvwap) / volume)) : 0);

}

int64_t mastercore::increaseLTCVolume(uint32_t propertyId, uint32_t propertyDesired, int aBlock)
{
    int64_t total = 0, propertyAmount = 0, propertyDesiredAmount = 0;
    const int rollback = aBlock - 1000;
    iterVolume(propertyAmount, propertyId, rollback, aBlock, MapLTCVolume);
    iterVolume(propertyDesiredAmount, propertyDesired, rollback, aBlock, MapLTCVolume);

    if (1000 * COIN <=  propertyAmount && 1000 * COIN <=  propertyDesiredAmount)
    {
        // look up the 12-block VWAP of the denominator vs. LTC
        const int64_t vwap = getVWap(propertyDesired, aBlock, tokenvwap);
        const arith_uint256 aTotal = (ConvertTo256(propertyDesiredAmount) * ConvertTo256(vwap)) / ConvertTo256(COIN);
        total = ConvertTo64(aTotal);

        // increment cumulative LTC volume by tokens traded * the 12-block VWAP
        if (total > 0) MapLTCVolume[aBlock][propertyDesired] += total;

    }

    return total;

}

bool mastercore::Token_LTC_Fees(int64_t& buyer_amountGot, uint32_t propertyId)
{

    if (!isPropertyDivisible(propertyId)){
        PrintToLog("%s(): indivisible token, no fees here\n",__func__);
        return false;
    }

    // 0.005 %
    const arith_uint256 uNumerator = ConvertTo256(buyer_amountGot);
    const arith_uint256 uDenominator = arith_uint256(BASISPOINT) * arith_uint256(BASISPOINT) *  arith_uint256(2);
    const arith_uint256 uCacheFee = DivideAndRoundUp(uNumerator, uDenominator);
    int64_t cacheFee = ConvertTo64(uCacheFee);

    // taking fee
    if(cacheFee > buyer_amountGot) {
        cacheFee = buyer_amountGot;
    }

    PrintToLog("%s(): cacheFee = %d, buyer_amountGot (before) = %d\n",__func__, cacheFee, buyer_amountGot);

    buyer_amountGot -= cacheFee;

    PrintToLog("%s(): cacheFee = %d, buyer_amountGot (after) = %d\n",__func__, cacheFee, buyer_amountGot);

    if(cacheFee > 0)
    {
         g_fees->native_fees[propertyId] += cacheFee;
         return true;
    }

    return false;

}

void blocksettlement::makeSettlement()
{
    const uint32_t nextCDID = _my_cds->peekNextContractID();

    // checking expiration block for each contract
    for (uint32_t contractId = 1; contractId < nextCDID; contractId++)
    {
         CDInfo::Entry cd;
         if (!_my_cds->getCD(contractId, cd)) {
             continue; // contract does not exist
         }

         const int64_t loss = getTotalLoss(contractId, cd.notional_size);
         PrintToLog("%s(): total loss: %d\n",__func__, loss);

         // if there's no liquidation orders
         if(0 == loss) {
             realize_pnl(contractId, cd.notional_size, cd.isOracle(), cd.isInverseQuoted());
         } else if (loss > 0) {
              //const int64_t difference = getInsurance(cd.collateral_currency) - loss;
              const int64_t difference = get_fees_balance(g_fees->spot_fees, cd.collateral_currency) - loss;

              if (0 > difference) {
                  // we need socialization of loss
                  lossSocialization(contractId, -difference);
              }

              g_fund->AccrueFees(cd.collateral_currency, loss);
          }
     }
}

int64_t blocksettlement::getTotalLoss(const uint32_t& contractId, const uint32_t& notionalSize)
{
    bool sign = false;
    int64_t vwap = 0;
    int64_t volume = 0;
    int64_t systemicLoss = 0;
    int64_t bankruptcyVWAP = 0;

    if (!mastercore::ContractDex_LIQUIDATION_VOLUME(contractId, volume, vwap, bankruptcyVWAP, sign)) {
        return 0;
    }

    const int64_t oracleTwap = mastercore::getOracleTwap(contractId, OBLOCKS);

    //! Systemic Loss in a Block = the volume-weighted avg. price of bankruptcy (for each position we need the bankruptcy price, and the volume of liquidation) for unfilled liquidations
    // * the sign of the liquidated contracts * their notional value (this depends of contract denomination) * the # of contracts (number of contract in liquidation) * (Liq. VWAP - Mark Price)
    // Mark Price: (oracle: TWAP of oracle, native: spot price)
    systemicLoss = ((bankruptcyVWAP * notionalSize) / COIN) * ((volume * vwap * oracleTwap) / COIN);

    // return totalLoss;
    return systemicLoss;

}

void blocksettlement::lossSocialization(const uint32_t& contractId, int64_t fullAmount)
{
    int count = 0;
    LOCK(cs_register);
    std::vector<std::string> zeroposition;

    for(const auto& p : mp_register_map)
    {
        const Register& reg = p.second;
        const int64_t position = reg.getRecord(contractId, CONTRACT_POSITION);

        // not counting these addresses
        if (0 == position) {
            zeroposition.push_back(p.first);
        } else {
            count++;
        }
    }

    if (count == 0) {
        return;
    }

    const int64_t fraction = fullAmount / count;

    for(auto& q : mp_register_map)
    {
        auto it = find (zeroposition.begin(), zeroposition.end(), q.first);
        if (it == zeroposition.end()) {
            Register& reg = q.second;
            reg.updateRecord(contractId, -fraction, MARGIN);
        }

    }

}

/**
 * @return The marker for class D transactions.
 */
const std::vector<uint8_t> GetTLMarker()
{
    std::vector<uint8_t> marker{0x74, 0x74};  /* 'tt' hex-encoded */
    return marker;
}