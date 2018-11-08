#ifndef OMNICORE_OMNICORE_H
#define OMNICORE_OMNICORE_H

class CBitcoinAddress;
class CBlockIndex;
class CCoinsView;
class CCoinsViewCache;
class CTransaction;

#include "omnicore/log.h"
#include "omnicore/persistence.h"
#include "omnicore/tally.h"

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

// Omni Layer Transaction Class
#define NO_MARKER    0
#define OMNI_CLASS_A 1
#define OMNI_CLASS_B 2
#define OMNI_CLASS_C 3 // uncompressed OP_RETURN
#define OMNI_CLASS_D 4 // compressed OP_RETURN

// Omni Layer Transaction (Packet) Version
#define MP_TX_PKT_V0  0
#define MP_TX_PKT_V1  1

// Alls
#define ALL_PROPERTY_MSC   3

#define MIN_PAYLOAD_SIZE     5

#define MAX_CLASS_D_SEARCH_BYTES   200


// Transaction types, from the spec
enum TransactionType {
  MSC_TYPE_SIMPLE_SEND                =  0,
  MSC_TYPE_RESTRICTED_SEND            =  2,
  MSC_TYPE_SEND_ALL                   =  4,
  MSC_TYPE_SAVINGS_MARK               = 10,
  MSC_TYPE_SAVINGS_COMPROMISED        = 11,
  MSC_TYPE_CREATE_PROPERTY_FIXED      = 50,
  MSC_TYPE_CREATE_PROPERTY_VARIABLE   = 51,
  MSC_TYPE_CLOSE_CROWDSALE            = 53,
  MSC_TYPE_CREATE_PROPERTY_MANUAL     = 54,
  MSC_TYPE_GRANT_PROPERTY_TOKENS      = 55,
  MSC_TYPE_REVOKE_PROPERTY_TOKENS     = 56,
  MSC_TYPE_CHANGE_ISSUER_ADDRESS      = 70,
  OMNICORE_MESSAGE_TYPE_DEACTIVATION  = 65533,
  OMNICORE_MESSAGE_TYPE_ACTIVATION    = 65534,
  OMNICORE_MESSAGE_TYPE_ALERT         = 65535,

  ////////////////////////////////////
  /** New things for Contract */
  MSC_TYPE_TRADE_OFFER                = 20,
  MSC_TYPE_DEX_BUY_OFFER              = 21,
  MSC_TYPE_ACCEPT_OFFER_BTC           = 22,
  MSC_TYPE_METADEX_TRADE              = 25,
  MSC_TYPE_CONTRACTDEX_TRADE          = 29,
  MSC_TYPE_CONTRACTDEX_CANCEL_PRICE   = 30,
  MSC_TYPE_CONTRACTDEX_CANCEL_ECOSYSTEM   = 32,
  MSC_TYPE_CONTRACTDEX_CLOSE_POSITION   = 33,
  MSC_TYPE_CONTRACTDEX_CANCEL_ORDERS_BY_BLOCK = 34,
  /** !Here we changed "MSC_TYPE_OFFER_ACCEPT_A_BET = 40" */
  MSC_TYPE_CREATE_CONTRACT            = 40,
  MSC_TYPE_PEGGED_CURRENCY            = 100,
  MSC_TYPE_REDEMPTION_PEGGED          = 101,
  MSC_TYPE_SEND_PEGGED_CURRENCY       = 102,


  ////////////////////////////////////

};

#define MSC_PROPERTY_TYPE_INDIVISIBLE             1
#define MSC_PROPERTY_TYPE_DIVISIBLE               2

enum FILETYPES {
  FILETYPE_BALANCES = 0,
  FILETYPE_GLOBALS,
  FILETYPE_CROWDSALES,
  FILETYPE_CDEXORDERS,
  FILETYPE_MARKETPRICES,
  FILETYPE_MDEXORDERS,
  FILETYPE_OFFERS,
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

#define OMNI_PROPERTY_BTC             0
#define OMNI_PROPERTY_MSC             1
#define OMNI_PROPERTY_TMSC            2
//////////////////////////////////////
/** New things for Contracts */
#define BUY   1
#define SELL  2
#define ACTIONINVALID  3

#define CONTRACT_ALL        3
#define CONTRACT_ALL_DUSD   4
#define CONTRACT_ALL_LTC    5
#define CONTRACT_LTC_DJPY   6
#define CONTRACT_LTC_DUSD   7
#define CONTRACT_LTC_DEUR   8
#define CONTRACT_sLTC_ALL   9

// Currency in existance (options for createcontract)
uint32_t const TL_dUSD  = 1;
uint32_t const TL_dEUR  = 2;
uint32_t const TL_dYEN  = 3;
uint32_t const TL_ALL   = 4;
uint32_t const TL_sLTC  = 5;
uint32_t const TL_LTC   = 6;

/** New for future contracts port */
#define MSC_PROPERTY_TYPE_CONTRACT    3

// forward declarations
std::string FormatDivisibleMP(int64_t amount, bool fSign = false);
std::string FormatDivisibleShortMP(int64_t amount);
std::string FormatMP(uint32_t propertyId, int64_t amount, bool fSign = false);
std::string FormatShortMP(uint32_t propertyId, int64_t amount);
std::string FormatByType(int64_t amount, uint16_t propertyType);
std::string FormatByDivisibility(int64_t amount, bool divisible);
double FormatContractShortMP(int64_t n);
long int FormatShortIntegerMP(int64_t n);
/** Returns the Exodus address. */
const std::string ExodusAddress();

/** Returns the marker for transactions. */
const std::vector<unsigned char> GetOmMarker();

//! Used to indicate, whether to automatically commit created transactions
extern bool autoCommit;

//! Global lock for state objects
extern CCriticalSection cs_tally;

/** LevelDB based storage for storing Omni transaction data.  This will become the new master database, holding serialized Omni transactions.
 *  Note, intention is to consolidate and clean up data storage
 */
class COmniTransactionDB : public CDBBase
{
public:
    COmniTransactionDB(const boost::filesystem::path& path, bool fWipe)
    {
        leveldb::Status status = Open(path, fWipe);
        PrintToConsole("Loading master transactions database: %s\n", status.ToString());
    }

    virtual ~COmniTransactionDB()
    {
        // if (msc_debug_persistence) PrintToLog("COmniTransactionDB closed\n");
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
        // if (msc_debug_persistence) PrintToLog("CMPTxList closed\n");
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

    /////////////////////////////////////////////
    /** New things for Contracts */
    void recordContractDexCancelTX(const uint256 &txidMaster, const uint256 &txidSub, bool fValid, int nBlock, unsigned int propertyId, uint64_t nValue);
    /////////////////////////////////////////////

    uint256 findMetaDExCancel(const uint256 txid);
    /** Returns the number of sub records. */
    int getNumberOfMetaDExCancels(const uint256 txid);

    //////////////////////////////////////
    /** New things for Contracts */
    int getNumberOfContractDexCancels(const uint256 txid);
    //////////////////////////////////////

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
        // if (msc_debug_persistence) PrintToLog("CMPTradeList closed\n");
    }

    void recordMatchedTrade(const uint256 txid1, const uint256 txid2, string address1, string address2, unsigned int prop1, unsigned int prop2, uint64_t amount1, uint64_t amount2, int blockNum, int64_t fee);

    /////////////////////////////////
    /** New things for Contract */
    void recordMatchedTrade(const uint256 txid1, const uint256 txid2, string address1, string address2, uint64_t effective_price, uint64_t amount_maker, uint64_t amount_taker, int blockNum1, int blockNum2, uint32_t property_traded, string tradeStatus, int64_t lives_s0, int64_t lives_s1, int64_t lives_s2, int64_t lives_s3, int64_t lives_b0, int64_t lives_b1, int64_t lives_b2, int64_t lives_b3, string s_maker0, string s_taker0, string s_maker1, string s_taker1, string s_maker2, string s_taker2, string s_maker3, string s_taker3, int64_t nCouldBuy0, int64_t nCouldBuy1, int64_t nCouldBuy2, int64_t nCouldBuy3);
    void recordForUPNL(const uint256 txid, string address, uint32_t property_traded, int64_t effectivePrice);
    // void recordMatchedTrade(const uint256 txid1, const uint256 txid2, string address1, string address2, unsigned int prop1, unsigned int prop2, uint64_t amount1, uint64_t amount2, int blockNum, int64_t fee, string t_status, std::vector<uint256> &vecTxid);
    /////////////////////////////////

    void recordNewTrade(const uint256& txid, const std::string& address, uint32_t propertyIdForSale, uint32_t propertyIdDesired, int blockNum, int blockIndex);
    void recordNewTrade(const uint256& txid, const std::string& address, uint32_t propertyIdForSale, uint32_t propertyIdDesired, int blockNum, int blockIndex, int64_t reserva);
    int deleteAboveBlock(int blockNum);
    bool exists(const uint256 &txid);
    void printStats();
    void printAll();
    bool getMatchingTrades(const uint256& txid, uint32_t propertyId, UniValue& tradeArray, int64_t& totalSold, int64_t& totalBought);

    ///////////////////////////////////////
    /** New things for Contract */
    bool getMatchingTrades(uint32_t propertyId, UniValue& tradeArray);
    double getPNL(string address, int64_t contractsClosed, int64_t price, uint32_t property, uint32_t marginRequirementContract, uint32_t notionalSize, std::string Status);
    void marginLogic(uint32_t property);
    double getUPNL(string address, uint32_t contractId);
    int64_t getTradeAllsByTxId(uint256& txid);
    /** Used to notify that the number of tokens for a property has changed. */
    void NotifyPeggedCurrency(const uint256& txid, string address, uint32_t propertyId, uint64_t amount, std::string series);
    bool getCreatedPegged(uint32_t propertyId, UniValue& tradeArray);
    //////////////////////////////////////

    bool getMatchingTrades(const uint256& txid);
    void getTradesForAddress(std::string address, std::vector<uint256>& vecTransactions, uint32_t propertyIdFilter = 0);
    void getTradesForPair(uint32_t propertyIdSideA, uint32_t propertyIdSideB, UniValue& response, uint64_t count);
    int getMPTradeCountTotal();
};


//! Available balances of wallet properties
extern std::map<uint32_t, int64_t> global_balance_money;
//! Vector containing a list of properties relative to the wallet
extern std::set<uint32_t> global_wallet_property_list;

int64_t getMPbalance(const std::string& address, uint32_t propertyId, TallyType ttype);
int64_t getUserAvailableMPbalance(const std::string& address, uint32_t propertyId);

/** Global handler to initialize Omni Core. */
int omnicorelite_init();

/** Global handler to shut down Omni Core. */
int omnicorelite_shutdown();

/** Global handler to total wallet balances. */
void CheckWalletUpdate(bool forceUpdate = false);

void NotifyTotalTokensChanged(uint32_t propertyId);

void buildingEdge(std::map<std::string, std::string> &edgeEle, std::string addrs_src, std::string addrs_trk, std::string status_src, std::string status_trk, int64_t lives_src, int64_t lives_trk, int64_t amount_path, int64_t matched_price, int idx_q, int ghost_edge);
void printing_edges_database(std::map<std::string, std::string> &path_ele);
const string gettingLineOut(std::string address1, std::string s_status1, int64_t lives_maker, std::string address2, std::string s_status2, int64_t lives_taker, int64_t nCouldBuy, uint64_t effective_price);
void loopForEntryPrice(std::vector<std::map<std::string, std::string>> path_ele, unsigned int path_length, std::string address1, std::string address2, double &UPNL1, double &UPNL2, uint64_t exit_price);
bool callingPerpetualSettlement(double globalPNLALL_DUSD, int64_t globalVolumeALL_DUSD, int64_t volumeToCompare);
double PNL_function(uint64_t entry_price, uint64_t exit_price, int64_t amount_trd, std::string netted_status);
void fillingMatrix(MatrixTLS &M_file, MatrixTLS &ndatabase, std::vector<std::map<std::string, std::string>> &path_ele);
int mastercore_handler_disc_begin(int nBlockNow, CBlockIndex const *pBlockIndex);
int mastercore_handler_disc_end(int nBlockNow, CBlockIndex const *pBlockIndex);
int mastercore_handler_block_begin(int nBlockNow, CBlockIndex const *pBlockIndex);
int mastercore_handler_block_end(int nBlockNow, CBlockIndex const *pBlockIndex, unsigned int);
bool mastercore_handler_tx(const CTransaction& tx, int nBlock, unsigned int idx, const CBlockIndex *pBlockIndex);
int mastercore_save_state( CBlockIndex const *pBlockIndex );

namespace mastercore
{
extern std::unordered_map<std::string, CMPTally> mp_tally_map;
/*New things for contracts*///////////////////////////////////
// extern std::unordered_map<std::string, CDexTally> cd_tally_map;
//////////////////////////////////////////////////////////////
extern CMPTxList *p_txlistdb;
extern COmniTransactionDB *p_OmniTXDB;
extern CMPTradeList *t_tradelistdb;

// TODO: move, rename
extern CCoinsView viewDummy;
extern CCoinsViewCache view;
//! Guards coins view cache
extern CCriticalSection cs_tx_cache;

std::string strMPProperty(uint32_t propertyId);

/* New things for contracts *///////////////////////////////////////////////////
rational_t notionalChange(uint32_t contractId);
// bool marginCheck(const std::string address);
bool marginNeeded(const std::string address, int64_t amountTraded, uint32_t contractId);
bool marginBack(const std::string address, int64_t amountTraded, uint32_t contractId);
////////////////////////////////////////////////////////////////////////////////

bool isMPinBlockRange(int starting_block, int ending_block, bool bDeleteFound);

/////////////////////////////////////////
/*New property type No 3 Contract*/
std::string FormatContractMP(int64_t n);
/////////////////////////////////////////
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
}


#endif // OMNICORE_OMNICORE_H
