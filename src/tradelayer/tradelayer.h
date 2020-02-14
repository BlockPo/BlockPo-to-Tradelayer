#ifndef TRADELAYER_TL_H
#define TRADELAYER_TL_H

class CBitcoinAddress;
class CBlockIndex;
class CCoinsView;
class CCoinsViewCache;
class CTransaction;

#include "tradelayer/log.h"
#include "tradelayer/persistence.h"
#include "tradelayer/tally.h"

#include "arith_uint256.h"
#include "sync.h"
#include "uint256.h"
#include "util.h"

#include <univalue.h>

#include <boost/lexical_cast.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/rational.hpp>
#include <boost/filesystem/path.hpp>

#include "leveldb/status.h"

#include <stdint.h>

#include <map>
#include <string>
#include <vector>
#include <set>
#include <unordered_map>
#include "tradelayer_matrices.h"

using std::string;

typedef boost::rational<boost::multiprecision::checked_int128_t> rational_t;

int const MAX_STATE_HISTORY = 50;

#define TEST_ECO_PROPERTY_1 (0x80000003UL)

// increment this value to force a refresh of the state (similar to --startclean)
#define DB_VERSION 1

// could probably also use: int64_t maxInt64 = std::numeric_limits<int64_t>::max();
// maximum numeric values from the spec:
#define MAX_INT_8_BYTES (9223372036854775807UL)

// maximum size of string fields
#define SP_STRING_FIELD_LEN 256

// Trade Layer Transaction Class
#define NO_MARKER  0
#define TL_CLASS_A 1
#define TL_CLASS_B 2
#define TL_CLASS_C 3 // uncompressed OP_RETURN
#define TL_CLASS_D 4 // compressed OP_RETURN

// Trade Layer Transaction (Packet) Version
#define MP_TX_PKT_V0  0
#define MP_TX_PKT_V1  1

// Alls
#define ALL_PROPERTY_MSC             3
#define MIN_PAYLOAD_SIZE             5
#define MAX_CLASS_D_SEARCH_BYTES   200

#define COIN256   10000000000000000

// basis point factor
#define int64_t BASISPOINT 100

// Oracle twaps blocks
#define oBlocks 9

// Transaction types, from the spec
enum TransactionType {
  MSC_TYPE_SIMPLE_SEND                =  0,
  MSC_TYPE_RESTRICTED_SEND            =  2,
  MSC_TYPE_SEND_ALL                   =  4,
  MSC_TYPE_SEND_VESTING               =  5,
  MSC_TYPE_SAVINGS_MARK               = 10,
  MSC_TYPE_SAVINGS_COMPROMISED        = 11,
  MSC_TYPE_CREATE_PROPERTY_FIXED      = 50,
  MSC_TYPE_CREATE_PROPERTY_VARIABLE   = 51,
  MSC_TYPE_CLOSE_CROWDSALE            = 53,
  MSC_TYPE_CREATE_PROPERTY_MANUAL     = 54,
  MSC_TYPE_GRANT_PROPERTY_TOKENS      = 55,
  MSC_TYPE_REVOKE_PROPERTY_TOKENS     = 56,
  MSC_TYPE_CHANGE_ISSUER_ADDRESS      = 70,
  TL_MESSAGE_TYPE_DEACTIVATION        = 65533,
  TL_MESSAGE_TYPE_ACTIVATION          = 65534,
  TL_MESSAGE_TYPE_ALERT               = 65535,

  MSC_TYPE_TRADE_OFFER                         = 20,
  MSC_TYPE_DEX_BUY_OFFER                       = 21,
  MSC_TYPE_ACCEPT_OFFER_BTC                    = 22,
  MSC_TYPE_METADEX_TRADE                       = 25,
  MSC_TYPE_CONTRACTDEX_TRADE                   = 29,
  MSC_TYPE_CONTRACTDEX_CANCEL_PRICE            = 30,
  MSC_TYPE_CONTRACTDEX_CANCEL_ECOSYSTEM        = 32,
  MSC_TYPE_CONTRACTDEX_CLOSE_POSITION          = 33,
  MSC_TYPE_CONTRACTDEX_CANCEL_ORDERS_BY_BLOCK  = 34,
  MSC_TYPE_CREATE_CONTRACT                     = 40,
  MSC_TYPE_PEGGED_CURRENCY                    = 100,
  MSC_TYPE_REDEMPTION_PEGGED                  = 101,
  MSC_TYPE_SEND_PEGGED_CURRENCY               = 102,
  MSC_TYPE_CREATE_ORACLE_CONTRACT             = 103,
  MSC_TYPE_CHANGE_ORACLE_REF                  = 104,
  MSC_TYPE_SET_ORACLE                         = 105,
  MSC_TYPE_ORACLE_BACKUP                      = 106,
  MSC_TYPE_CLOSE_ORACLE                       = 107,
  MSC_TYPE_COMMIT_CHANNEL                     = 108,
  MSC_TYPE_WITHDRAWAL_FROM_CHANNEL            = 109,
  MSC_TYPE_INSTANT_TRADE                      = 110,
  MSC_TYPE_PNL_UPDATE                         = 111,
  MSC_TYPE_TRANSFER                           = 112,
  MSC_TYPE_CREATE_CHANNEL                     = 113,
  MSC_TYPE_CONTRACT_INSTANT                   = 114,
  MSC_TYPE_NEW_ID_REGISTRATION                = 115,
  MSC_TYPE_UPDATE_ID_REGISTRATION             = 116,
  MSC_TYPE_DEX_PAYMENT                        = 117

};

#define ALL_PROPERTY_TYPE_INDIVISIBLE                 1
#define ALL_PROPERTY_TYPE_DIVISIBLE                   2
#define ALL_PROPERTY_TYPE_NATIVE_CONTRACT             3
#define ALL_PROPERTY_TYPE_VESTING                     4
#define ALL_PROPERTY_TYPE_PEGGEDS                     5
#define ALL_PROPERTY_TYPE_ORACLE_CONTRACT             6
#define ALL_PROPERTY_TYPE_PERPETUAL_ORACLE            7
#define ALL_PROPERTY_TYPE_PERPETUAL_CONTRACTS         8

enum FILETYPES {
  FILETYPE_BALANCES = 0,
  FILETYPE_GLOBALS,
  FILETYPE_CROWDSALES,
  FILETYPE_CDEXORDERS,
  FILETYPE_MARKETPRICES,
  FILETYPE_MDEXORDERS,
  FILETYPE_OFFERS,
  FILETYPE_CACHEFEES,
  FILETYPE_WITHDRAWALS,
  FILETYPE_ACTIVE_CHANNELS,
  FILETYPE_DEX_VOLUME,
  FILETYPE_MDEX_VOLUME,
  NUM_FILETYPES
};

#define PKT_RETURNED_OBJECT    (1000)

#define PKT_ERROR             ( -9000)
// Smart Properties
#define PKT_ERROR_SP          (-40000)
#define PKT_ERROR_CROWD       (-45000)
// Send To Owners
#define PKT_ERROR_SEND        (-60000)
#define PKT_ERROR_TOKENS      (-82000)
#define PKT_ERROR_SEND_ALL    (-83000)
#define PKT_ERROR_METADEX     (-80000)
#define METADEX_ERROR         (-81000)

#define PKT_ERROR             ( -9000)
#define DEX_ERROR_SELLOFFER   (-10000)
#define DEX_ERROR_ACCEPT      (-20000)
#define DEX_ERROR_PAYMENT     (-30000)
#define PKT_ERROR_TRADEOFFER  (-70000)

#define PKT_ERROR_KYC            (-90000)
#define PKT_ERROR_CONTRACTDEX    (-100000)
#define PKT_ERROR_ORACLE         (-110000)
#define PKT_ERROR_CHANNELS       (-120000)

#define TL_PROPERTY_BTC             0
#define TL_PROPERTY_ALL             1
#define TL_PROPERTY_TALL            2
#define TL_PROPERTY_VESTING         3
#define TL_PROPERTY_ALL_ISSUANCE    6
#define TOTAL_AMOUNT_VESTING_TOKENS   1500000*COIN

#define BUY            1
#define SELL           2
#define ACTIONINVALID  3


//Main Cardinal Definitions
#define LTC        0
#define ALL        1
#define sLTC       2
#define dUSD       3
#define dEUR       4
#define dJPY       5
#define dCNY       6
#define ALL_LTC    7
#define LTC_USD    8
#define LTC_EUR    9
#define JPY       10
#define CNY       11

// #define CONTRACT_ALL        3
// #define CONTRACT_ALL_DUSD   4
// #define CONTRACT_ALL_LTC    5
// #define CONTRACT_LTC_DJPY   6
// #define CONTRACT_LTC_DUSD   7
// #define CONTRACT_LTC_DEUR   8
// #define CONTRACT_sLTC_ALL   9

// channels definitions
#define TYPE_COMMIT                     "commit"
#define TYPE_WITHDRAWAL                 "withdrawal"
#define TYPE_INSTANT_TRADE              "instant_trade"
#define TYPE_TRANSFER                   "transfer"
#define TYPE_CONTRACT_INSTANT_TRADE     "contract_instat_trade"
#define TYPE_CREATE_CHANNEL             "create channel"
#define TYPE_NEW_ID_REGISTER            "new id register"

// Currency in existance (options for createcontract)
uint32_t const TL_dUSD  = 1;
uint32_t const TL_dEUR  = 2;
uint32_t const TL_dYEN  = 3;
uint32_t const TL_ALL   = 4;
uint32_t const TL_sLTC  = 5;
uint32_t const TL_LTC   = 6;

/*24 horus to blocks*/
const int dayblocks = 576;

// limits for margin dynamic
const rational_t factor = rational_t(80,100);  // critical limit
const rational_t factor2 = rational_t(20,100); // normal limits

// define 1 year in blocks:
#define ONE_YEAR 210240


// forward declarations
std::string FormatDivisibleMP(int64_t amount, bool fSign = false);
std::string FormatDivisibleShortMP(int64_t amount);
std::string FormatMP(uint32_t propertyId, int64_t amount, bool fSign = false);
std::string FormatShortMP(uint32_t propertyId, int64_t amount);
std::string FormatByType(int64_t amount, uint16_t propertyType);
std::string FormatByDivisibility(int64_t amount, bool divisible);
double FormatContractShortMP(int64_t n);
long int FormatShortIntegerMP(int64_t n);
uint64_t int64ToUint64(int64_t value);
std::string FormatDivisibleZeroClean(int64_t n);

/** Returns the marker for transactions. */
const std::vector<unsigned char> GetTLMarker();

//! Used to indicate, whether to automatically commit created transactions
extern bool autoCommit;

//! Global lock for state objects
extern CCriticalSection cs_tally;

/** LevelDB based storage for storing Trade Layer transaction data.  This will become the new master database, holding serialized Trade Layer transactions.
 *  Note, intention is to consolidate and clean up data storage
 */

 struct channel
 {
   std::string multisig;
   std::string first;
   std::string second;
   int expiry_height;
   int last_exchange_block;

   channel() : multisig(""), first(""), second(""), expiry_height(0), last_exchange_block(0) {}
 };

class CtlTransactionDB : public CDBBase
{
public:
    CtlTransactionDB(const boost::filesystem::path& path, bool fWipe)
    {
        leveldb::Status status = Open(path, fWipe);
        PrintToConsole("Loading master transactions database: %s\n", status.ToString());
    }

    virtual ~CtlTransactionDB()
    {
        if (msc_debug_persistence) PrintToLog("CtlTransactionDB closed\n");
    }

    /* These functions would be expanded upon to store a serialized version of the transaction and associated state data
     *
     * void RecordTransaction(const uint256& txid, uint32_t posInBlock, various, other, data);
     * int FetchTransactionPosition(const uint256& txid);
     * bool FetchTransactionValidity(const uint256& txid);
     *
     * and so on...
     */
    void RecordTransaction(const uint256& txid, uint32_t posInBlock);
    uint32_t FetchTransactionPosition(const uint256& txid);
};

/** LevelDB based storage for transactions, with txid as key and validity bit, and other data as value.
 */
class CMPTxList : public CDBBase
{
public:
    CMPTxList(const boost::filesystem::path& path, bool fWipe)
    {
        leveldb::Status status = Open(path, fWipe);
        PrintToConsole("Loading tx meta-info database: %s\n", status.ToString());
    }

    virtual ~CMPTxList()
      {
        if (msc_debug_persistence) PrintToLog("CMPTxList closed\n");
      }

    void recordTX(const uint256 &txid, bool fValid, int nBlock, unsigned int type, uint64_t nValue);
    /** Records a "send all" sub record. */
    void recordSendAllSubRecord(const uint256& txid, int subRecordNumber, uint32_t propertyId, int64_t nvalue);

    string getKeyValue(string key);
    /** Returns the number of sub records. */
    int getNumberOfSubRecords(const uint256& txid);
    /** Retrieves details about a "send all" record. */
    bool getSendAllDetails(const uint256& txid, int subSend, uint32_t& propertyId, int64_t& amount);
    int getMPTransactionCountTotal();
    int getMPTransactionCountBlock(int block);

    int getDBVersion();
    int setDBVersion();

    bool exists(const uint256 &txid);
    bool getTX(const uint256 &txid, string &value);

    std::set<int> GetSeedBlocks(int startHeight, int endHeight);
    void LoadAlerts(int blockHeight);
    void LoadActivations(int blockHeight);

    void printStats();
    void printAll();

    bool isMPinBlockRange(int, int, bool);

    void recordPaymentTX(const uint256 &txid, bool fValid, int nBlock, unsigned int vout, unsigned int propertyId, uint64_t nValue, string buyer, string seller);
    void recordMetaDExCancelTX(const uint256 &txidMaster, const uint256 &txidSub, bool fValid, int nBlock, unsigned int propertyId, uint64_t nValue);
    void recordContractDexCancelTX(const uint256 &txidMaster, const uint256 &txidSub, bool fValid, int nBlock, unsigned int propertyId, uint64_t nValue);

    uint256 findMetaDExCancel(const uint256 txid);

    /** Returns the number of sub records. */
    int getNumberOfMetaDExCancels(const uint256 txid);
    int getNumberOfContractDexCancels(const uint256 txid);
    void getMPTransactionAddresses(std::vector<std::string> &vaddrs);
};

/** LevelDB based storage for the trade history. Trades are listed with key "txid1+txid2".
 */

class CMPTradeList : public CDBBase
{
 public:

  CMPTradeList(const boost::filesystem::path& path, bool fWipe)
    {
      leveldb::Status status = Open(path, fWipe);
      PrintToConsole("Loading trades database: %s\n", status.ToString());
    }

  virtual ~CMPTradeList()
    {
      if (msc_debug_persistence) PrintToLog("CMPTradeList closed\n");
    }

  void recordMatchedTrade(const uint256 txid1, const uint256 txid2, string address1, string address2, unsigned int prop1, unsigned int prop2, uint64_t amount1, uint64_t amount2, int blockNum, int64_t fee);

  void recordMatchedTrade(const uint256 txid1, const uint256 txid2, string address1, string address2, uint64_t effective_price, uint64_t amount_maker, uint64_t amount_taker, int blockNum1, int blockNum2, uint32_t property_traded, string tradeStatus, int64_t lives_s0, int64_t lives_s1, int64_t lives_s2, int64_t lives_s3, int64_t lives_b0, int64_t lives_b1, int64_t lives_b2, int64_t lives_b3, string s_maker0, string s_taker0, string s_maker1, string s_taker1, string s_maker2, string s_taker2, string s_maker3, string s_taker3, int64_t nCouldBuy0, int64_t nCouldBuy1, int64_t nCouldBuy2, int64_t nCouldBuy3, uint64_t amountpnew, uint64_t amountpold);
  void recordForUPNL(const uint256 txid, string address, uint32_t property_traded, int64_t effectivePrice);
  void recordNewTrade(const uint256& txid, const std::string& address, uint32_t propertyIdForSale, uint32_t propertyIdDesired, int blockNum, int blockIndex);
  void recordNewTrade(const uint256& txid, const std::string& address, uint32_t propertyIdForSale, uint32_t propertyIdDesired, int blockNum, int blockIndex, int64_t reserva);

  //Multisig channels
  void recordNewCommit(const uint256& txid, const std::string& channelAddress, const std::string& sender, uint32_t propertyId, uint64_t amountCommited, int blockNum, int blockIndex);
  void recordNewWithdrawal(const uint256& txid, const std::string& channelAddress, const std::string& sender, uint32_t propertyId, uint64_t amountToWithdrawal, int blockNum, int blockIndex);
  void recordNewChannel(const std::string& channelAddress, const std::string& frAddr, const std::string& secAddr, int blockNum, int blockIndex);
  void recordNewInstantTrade(const uint256& txid, const std::string& sender, const std::string& receiver, uint32_t propertyIdForSale, uint64_t amount_forsale, uint32_t propertyIdDesired, uint64_t amount_desired, int blockNum, int blockIndex);
  void recordNewTransfer(const uint256& txid, const std::string& sender, const std::string& receiver, uint32_t propertyId, uint64_t amount, int blockNum, int blockIndex);
  void recordNewInstContTrade(const uint256& txid, const std::string& firstAddr, const std::string& secondAddr, uint32_t property, uint64_t amount_forsale, uint64_t price ,int blockNum, int blockIndex);
  void recordNewIdRegister(const uint256& txid, const std::string& address, const std::string& website, const std::string& name, uint8_t tokens, uint8_t ltc, uint8_t natives, uint8_t oracles, int blockNum, int blockIndex);

  bool getAllCommits(const std::string& senderAddress, UniValue& tradeArray);
  bool getAllWithdrawals(const std::string& senderAddress, UniValue& tradeArray);
  bool getChannelInfo(const std::string& channelAddress, UniValue& tradeArray);
  bool checkChannelAddress(const std::string& channelAddress);
  channel getChannelAddresses(const std::string& channelAddress);
  bool checkChannelRelation(const std::string& address, std::string& channelAddr);
  uint64_t getRemaining(const std::string& channelAddress, const std::string& senderAddress, uint32_t propertyId);

  //KYC
  bool updateIdRegister(const uint256& txid, const std::string& address, const std::string& newAddr, int blockNum, int blockIndex);
  bool checkKYCRegister(const std::string& address, int registered);

  int deleteAboveBlock(int blockNum);
  bool exists(const uint256 &txid);
  void printStats();
  void printAll();
  bool getMatchingTrades(const uint256& txid, uint32_t propertyId, UniValue& tradeArray, int64_t& totalSold, int64_t& totalBought);

  bool getMatchingTrades(uint32_t propertyId, UniValue& tradeArray);
  bool getMatchingTradesUnfiltered(uint32_t propertyId, UniValue& tradeArray);
  double getPNL(string address, int64_t contractsClosed, int64_t price, uint32_t property, uint32_t marginRequirementContract, uint32_t notionalSize, std::string Status);
  double getUPNL(string address, uint32_t contractId);
  int64_t getTradeAllsByTxId(uint256& txid);

  /** Used to notify that the number of tokens for a property has changed. */
  void NotifyPeggedCurrency(const uint256& txid, string address, uint32_t propertyId, uint64_t amount, std::string series);
  bool getCreatedPegged(uint32_t propertyId, UniValue& tradeArray);
  bool checkRegister(const std::string& address, int registered);

  bool getMatchingTrades(const uint256& txid);
  void getTradesForAddress(std::string address, std::vector<uint256>& vecTransactions, uint32_t propertyIdFilter = 0);
  void getTradesForPair(uint32_t propertyIdSideA, uint32_t propertyIdSideB, UniValue& response, uint64_t count);
  int getMPTradeCountTotal();
  int getNextId();
};

class CMPSettlementMatchList : public CDBBase
{
 public:
  CMPSettlementMatchList(const boost::filesystem::path& path, bool fWipe)
    {
      leveldb::Status status = Open(path, fWipe);
      PrintToConsole("Loading settlement match info database: %s\n", status.ToString());
    }

  virtual ~CMPSettlementMatchList() { }
};

//! Available balances of wallet properties
extern std::map<uint32_t, int64_t> global_balance_money;

//! Vector containing a list of properties relative to the wallet
extern std::set<uint32_t> global_wallet_property_list;

int64_t getMPbalance(const std::string& address, uint32_t propertyId, TallyType ttype);
int64_t getUserAvailableMPbalance(const std::string& address, uint32_t propertyId);
int64_t getUserReserveMPbalance(const std::string& address, uint32_t propertyId);

/** Global handler to total wallet balances. */
void CheckWalletUpdate(bool forceUpdate = false);

void NotifyTotalTokensChanged(uint32_t propertyId);

void buildingEdge(std::map<std::string, std::string> &edgeEle, std::string addrs_src, std::string addrs_trk, std::string status_src, std::string status_trk, int64_t lives_src, int64_t lives_trk, int64_t amount_path, int64_t matched_price, int idx_q, int ghost_edge);
void printing_edges_database(std::map<std::string, std::string> &path_ele);
const string gettingLineOut(std::string address1, std::string s_status1, int64_t lives_maker, std::string address2, std::string s_status2, int64_t lives_taker, int64_t nCouldBuy, uint64_t effective_price);
void loopForUPNL(std::vector<std::map<std::string, std::string>> path_ele, std::vector<std::map<std::string, std::string>> path_eleh, unsigned int path_length, std::string address1, std::string address2, std::string status1, std::string status2, double &UPNL1, double &UPNL2, uint64_t exit_price, int64_t nCouldBuy0);
void loopforEntryPrice(std::vector<std::map<std::string, std::string>> path_ele, std::vector<std::map<std::string, std::string>> path_eleh, std::string addrs_upnl, std::string status_match, double &entry_price, int &idx_price, uint64_t entry_price_num, unsigned int limSup, double exit_priceh, uint64_t &amount, std::string &status);
bool callingPerpetualSettlement(double globalPNLALL_DUSD, int64_t globalVolumeALL_DUSD, int64_t volumeToCompare);
double PNL_function(double entry_price, double exit_price, int64_t amount_trd, std::string netted_status);
void fillingMatrix(MatrixTLS &M_file, MatrixTLS &ndatabase, std::vector<std::map<std::string, std::string>> &path_ele);
int mastercore_handler_disc_begin(int nBlockNow, CBlockIndex const *pBlockIndex);
int mastercore_handler_disc_end(int nBlockNow, CBlockIndex const *pBlockIndex);
int mastercore_handler_block_begin(int nBlockNow, CBlockIndex const *pBlockIndex);
int mastercore_handler_block_end(int nBlockNow, CBlockIndex const *pBlockIndex, unsigned int);
bool mastercore_handler_tx(const CTransaction& tx, int nBlock, unsigned int idx, const CBlockIndex *pBlockIndex);
int mastercore_save_state( CBlockIndex const *pBlockIndex );
void creatingVestingTokens(int block);
void lookingin_globalvector_pastlivesperpetuals(std::vector<std::map<std::string, std::string>> &lives_g, MatrixTLS M_file, std::vector<std::string> addrs_vg, std::vector<std::map<std::string, std::string>> &lives_h);
void lookingaddrs_inside_M_file(std::string addrs, MatrixTLS M_file, std::vector<std::map<std::string, std::string>> &lives_g, std::vector<std::map<std::string, std::string>> &lives_h);
int finding_idxforaddress(std::string addrs, std::vector<std::map<std::string, std::string>> lives);
void Filling_Twap_Vec(std::map<uint32_t, std::vector<uint64_t>> &twap_ele, std::map<uint32_t, std::vector<uint64_t>> &twap_vec,
		      uint32_t property_traded, uint32_t property_desired, uint64_t effective_price);
void Filling_Twap_Vec(std::map<uint32_t, std::map<uint32_t, std::vector<uint64_t>>> &twap_ele,
		      std::map<uint32_t, std::map<uint32_t, std::vector<uint64_t>>> &twap_vec,
		      uint32_t property_traded, uint32_t property_desired, uint64_t effective_price);
inline int64_t clamp_function(int64_t diff, int64_t nclamp);
bool TxValidNodeReward(std::string ConsensusHash, std::string Tx);

namespace mastercore
{
  extern std::unordered_map<std::string, CMPTally> mp_tally_map;
  extern CMPTxList *p_txlistdb;
  extern CtlTransactionDB *p_TradeTXDB;
  extern CMPTradeList *t_tradelistdb;

  // TODO: move, rename
  extern CCoinsView viewDummy;
  extern CCoinsViewCache view;

  //! Guards coins view cache
  extern CCriticalSection cs_tx_cache;

  std::string strMPProperty(uint32_t propertyId);

  rational_t notionalChange(uint32_t contractId);

  bool isMPinBlockRange(int starting_block, int ending_block, bool bDeleteFound);

  std::string FormatContractMP(int64_t n);

  std::string FormatIndivisibleMP(int64_t n);

  int WalletTxBuilder(const std::string& senderAddress, const std::string& receiverAddress, int64_t referenceAmount,
		      const std::vector<unsigned char>& data, uint256& txid, std::string& rawHex, bool commit, unsigned int minInputs = 1);

  bool isTestEcosystemProperty(uint32_t propertyId);
  bool isMainEcosystemProperty(uint32_t propertyId);

  uint32_t GetNextPropertyId(bool maineco); // maybe move into sp

  CMPTally* getTally(const std::string& address);

  int64_t getTotalTokens(uint32_t propertyId, int64_t* n_owners_total = NULL);

  std::string strTransactionType(uint16_t txType);

  /** Returns the encoding class, used to embed a payload. */
  int GetEncodingClass(const CTransaction& tx, int nBlock);

  /** Determines, whether it is valid to use a Class C transaction for a given payload size. */
  bool UseEncodingClassC(size_t nDataSize);

  bool getValidMPTX(const uint256 &txid, int *block = NULL, unsigned int *type = NULL, uint64_t *nAmended = NULL);

  bool update_tally_map(const std::string& who, uint32_t propertyId, int64_t amount, TallyType ttype);

  std::string getTokenLabel(uint32_t propertyId);

  bool marginMain(int Block);

  void update_sum_upnls(); // update the sum of all upnls for all addresses.

  int64_t sum_check_upnl(std::string address); //  sum of all upnls for a given address.

  int64_t pos_margin(uint32_t contractId, std::string address, uint32_t margin_requirement); // return mainteinance margin for a given contrand and address

  bool makeWithdrawals(int Block); // make the withdrawals for multisig channels

  bool closeChannels(int Block);

  // x_Trade function for contracts on instant trade
  bool Instant_x_Trade(const uint256& txid, uint8_t tradingAction, std::string& channelAddr, std::string& firstAddr, std::string& secondAddr, uint32_t property, int64_t amount_forsale, uint64_t price, int block, int tx_idx);

  //Fees for contract instant trades
  bool ContInst_Fees(const std::string& firstAddr,const std::string& secondAddr,const std::string& channelAddr, int64_t amountToReserve,uint16_t type, uint32_t colateral);

  // Map of LTC volume
  int64_t LtcVolumen(uint32_t propertyId, int fblock, int sblock);

  //Map of MetaDEx volume
  int64_t MdexVolumen(uint32_t fproperty, uint32_t sproperty, int fblock, int sblock);

  void twapForLiquidation(uint32_t contractId, int blocks);

  int64_t getOracleTwap(uint32_t contractId, int nBlocks);

  // check for vesting
  bool SanityChecks(string receiver, int aBlock);

}

#endif // TRADELAYER_TL_H
