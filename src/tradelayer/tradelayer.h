#ifndef TRADELAYER_TL_H
#define TRADELAYER_TL_H

#include <tradelayer/log.h>
#include <tradelayer/persistence.h>
#include <tradelayer/tally.h>
#include <tradelayer/tradelayer_matrices.h>

#include <arith_uint256.h>
#include <chain.h>
#include <coins.h>
#include <hash.h>
#include <primitives/transaction.h>
#include <sync.h>
#include <uint256.h>
#include <util/system.h>

#include <leveldb/status.h>
#include <openssl/sha.h>

#include <map>
#include <set>
#include <stdint.h>
#include <string>
#include <univalue.h>
#include <unordered_map>
#include <vector>

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/rational.hpp>

using std::string;

typedef boost::rational<boost::multiprecision::checked_int128_t> rational_t;

const int MAX_STATE_HISTORY = 200;

const int STORE_EVERY_N_BLOCK = 5000;

#define MAX_PROPERTY_N (0x80000003UL)

// increment this value to force a refresh of the state (similar to --startclean)
const int DB_VERSION = 1;

// could probably also use: int64_t maxInt64 = std::numeric_limits<int64_t>::max();
// maximum numeric values from the spec:
#define MAX_INT_8_BYTES (9223372036854775807UL)

// maximum size of string fields
#define SP_STRING_FIELD_LEN 256

// Trade Layer Transaction Class
const int NO_MARKER  = 0;
const int TL_CLASS_D = 4; // compressed OP_RETURN

// Trade Layer Transaction (Packet) Version
const uint16_t  MP_TX_PKT_V0 = 0;
const uint16_t  MP_TX_PKT_V1 = 1;

// Alls
const int ALL_PROPERTY_MSC         = 3;
const int MIN_PAYLOAD_SIZE         = 5;
const int MAX_CLASS_D_SEARCH_BYTES = 200;

#define COIN256   10000000000000000

// basis point factor
const int BASISPOINT = 100;

// Oracle twaps blocks
const int OBLOCKS = 9;
const int OL_BLOCKS = 3;

// settlement variables
const int BlockS = 500; /** regtest **/

//node reward
const int64_t BASE_REWARD = 0.05 * COIN;
const int64_t MIN_REWARD = 0.01 * COIN;
const double FBASE = 1.000014979;
const double SBASE = 0.999937;
const int BLOCK_LIMIT = 10;

const double CompoundRate = 1.00002303;
const double DecayRate = 0.99998;
const double LongTailDecay = 0.99999992;
const double SatoshiH = 0.00000001;
const int NYears = 10;
const int initYear = 19;

//factor ALLs
const int64_t factorALLtoLTC = 1;

//Vesting variables
// const int nVestingAddrs = 5;
extern int lastBlockg;
const int64_t totalVesting = 1500000 * COIN;

extern int64_t globalVolumeALL_LTC;
extern int n_rows;
extern int idx_q;

// Transaction types, from the spec
enum TransactionType : unsigned int {
  MSC_TYPE_SIMPLE_SEND                =  0,
  MSC_TYPE_SEND_MANY                  =  1,
  MSC_TYPE_RESTRICTED_SEND            =  2,
  MSC_TYPE_SEND_ALL                   =  4,
  MSC_TYPE_SEND_VESTING               =  5,
  MSC_TYPE_SAVINGS_MARK               = 10,
  MSC_TYPE_SAVINGS_COMPROMISED        = 11,
  MSC_TYPE_CREATE_PROPERTY_FIXED      = 50,
  MSC_TYPE_CREATE_PROPERTY_MANUAL     = 54,
  MSC_TYPE_GRANT_PROPERTY_TOKENS      = 55,
  MSC_TYPE_REVOKE_PROPERTY_TOKENS     = 56,
  MSC_TYPE_CHANGE_ISSUER_ADDRESS      = 70,
  TL_MESSAGE_TYPE_DEACTIVATION        = 65533,
  TL_MESSAGE_TYPE_ACTIVATION          = 65534,
  TL_MESSAGE_TYPE_ALERT               = 65535,

  MSC_TYPE_DEX_SELL_OFFER                      = 20,
  MSC_TYPE_DEX_BUY_OFFER                       = 21,
  MSC_TYPE_ACCEPT_OFFER_BTC                    = 22,
  MSC_TYPE_METADEX_TRADE                       = 25,
  MSC_TYPE_METADEX_CANCEL_ALL                  = 26,
  MSC_TYPE_CONTRACTDEX_TRADE                   = 29,
  MSC_TYPE_CONTRACTDEX_CANCEL_PRICE            = 30,
  MSC_TYPE_CONTRACTDEX_CANCEL                  = 31,
  MSC_TYPE_CONTRACTDEX_CANCEL_ECOSYSTEM        = 32,
  MSC_TYPE_CONTRACTDEX_CLOSE_POSITION          = 33,
  MSC_TYPE_CONTRACTDEX_CANCEL_ORDERS_BY_BLOCK  = 34,
  MSC_TYPE_METADEX_CANCEL                      = 35,
  MSC_TYPE_METADEX_CANCEL_BY_PAIR              = 36,
  MSC_TYPE_METADEX_CANCEL_BY_PRICE             = 37,
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
  MSC_TYPE_INSTANT_LTC_TRADE                  = 113,
  MSC_TYPE_CONTRACT_INSTANT                   = 114,
  MSC_TYPE_NEW_ID_REGISTRATION                = 115,
  MSC_TYPE_UPDATE_ID_REGISTRATION             = 116,
  MSC_TYPE_DEX_PAYMENT                        = 117,
  MSC_TYPE_ATTESTATION                        = 118,
  MSC_TYPE_REVOKE_ATTESTATION                 = 119,
  MSC_TYPE_CLOSE_CHANNEL                      = 120,
  MSC_TYPE_SUBMIT_NODE_ADDRESS                = 121,
  MSC_TYPE_CLAIM_NODE_REWARD                  = 122,
  MSC_TYPE_SEND_DONATION                      = 123,

};


enum AllProperties  {
  ALL_PROPERTY_TYPE_INDIVISIBLE = 1,
  ALL_PROPERTY_TYPE_DIVISIBLE,
  ALL_PROPERTY_TYPE_NATIVE_CONTRACT,
  ALL_PROPERTY_TYPE_VESTING,
  ALL_PROPERTY_TYPE_PEGGEDS,
  ALL_PROPERTY_TYPE_ORACLE_CONTRACT,
  ALL_PROPERTY_TYPE_PERPETUAL_ORACLE,
  ALL_PROPERTY_TYPE_PERPETUAL_CONTRACTS
};

enum FILETYPES {
  FILETYPE_BALANCES = 0,
  FILETYPE_GLOBALS,
  FILETYPE_CDEXORDERS,
  FILETYPE_MDEXORDERS,
  FILETYPE_OFFERS,
  FILETYPE_ACCEPTS,
  FILETYPE_CACHEFEES,
  FILETYPE_CACHEFEES_ORACLES,
  FILETYPE_WITHDRAWALS,
  FILETYPE_ACTIVE_CHANNELS,
  FILETYPE_DEX_VOLUME,
  FILETYPE_MDEX_VOLUME,
  FILETYPE_GLOBAL_VARS,
  FILE_TYPE_VESTING_ADDRESSES,
  FILE_TYPE_LTC_VOLUME,
  FILE_TYPE_TOKEN_LTC_PRICE,
  FILE_TYPE_TOKEN_VWAP,
  FILE_TYPE_REGISTER,
  FILETYPE_CONTRACT_GLOBALS,
  FILE_TYPE_NODE_ADDRESSES,
  NUM_FILETYPES
};

const int PKT_RETURNED_OBJECT    =  1000;
const int PKT_ERROR              = -9000;
// Smart Properties
const int PKT_ERROR_SP           = -40000;
// Send To Owners
const int PKT_ERROR_SEND          = -60000;
const int PKT_ERROR_TOKENS        = -82000;
const int PKT_ERROR_SEND_ALL      = -83000;
const int PKT_ERROR_METADEX       = -80000;
const int METADEX_ERROR           = -81000;
const int CONTRACTDEX_ERROR       = -82000;
const int NODE_REWARD_ERROR       = -84000;
const int PKT_ERROR_SEND_DONATION = -85000;

const int DEX_ERROR_SELLOFFER    = -10000;
const int DEX_ERROR_ACCEPT       = -20000;
const int DEX_ERROR_PAYMENT      = -30000;
const int PKT_ERROR_TRADEOFFER   = -70000;

const int PKT_ERROR_KYC          = -90000;
const int PKT_ERROR_CONTRACTDEX  = -100000;
const int PKT_ERROR_ORACLE       = -110000;
const int PKT_ERROR_CHANNELS     = -120000;

const int TL_PROPERTY_BTC                 = 0;
const int TL_PROPERTY_ALL                 = 1;
const int TL_PROPERTY_TALL                = 2;
const int TL_PROPERTY_VESTING             = 3;
const int TL_PROPERTY_ALL_ISSUANCE        = 6;
const uint64_t TOTAL_AMOUNT_VESTING_TOKENS =  150000000000000;

enum Action : uint8_t {
  buy = 1,
  sell = 2,
  actioninvalid = 3
};

//Main Cardinal Definitions
enum Cardinal : uint32_t {
  LTC  = 0,
  ALL,
  sLTC,    //ALL + 1x short ALL_LTC_PERP
  VT,      // vesting tokens
  dEUR,
  dJPY,
  dCNY,
  dUSD,
  rLTC, //future activation goal: new bitcoin OPcode, native LTC paid in as one output, can redeem for any sized output = # of rLTC tokens redeemed.
  //rLTC (and rBTC on the Bitcoin version) is intended to be the 'final form' of the protocol in terms of native collateral choices.
  ALL_LTC_PERP,
  LTC_USD_PERP,
  LTC_EUR_PERP,
  LTC_JPY_PERP,
  LTC_CNY_PERP,
  ALL_LTC_Futures_FirstPeriod,
  ALL_LTC_Futures_SecondPeriod,
  ALL_LTC_Futures_ThirdPeriod,
  ALL_LTC_Futures_FourthPeriod,
  LTC_USD_Futures_FirstPeriod,
  LTC_USD_Futures_SecondPeriod,
  LTC_USD_Futures_ThirdPeriod,
  LTC_USD_Futures_FourthPeriod,
  LTC_USD_Futures_FifthPeriod,
  LTC_USD_Futures_SixthPeriod,
  LTC_EUR_Futures_FirstPeriod,
  LTC_EUR_Futures_SecondPeriod,
  LTC_EUR_Futures_ThirdPeriod,
  LTC_EUR_Futures_FourthPeriod,
  LTC_EUR_Futures_FifthPeriod,
  LTC_EUR_Futures_SixthPeriod,
  LTC_JPY_Futures_FirstPeriod,
  LTC_JPY_Futures_SecondPeriod,
  LTC_JPY_Futures_ThirdPeriod,
  LTC_JPY_Futures_FourthPeriod,
  LTC_JPY_Futures_FifthPeriod,
  LTC_JPY_Futures_SixthPeriod,
  LTC_CNY_Futures_FirstPeriod,
  LTC_CNY_Futures_SecondPeriod,
  LTC_CNY_Futures_ThirdPeriod,
  LTC_CNY_Futures_FourthPeriod,
  LTC_CNY_Futures_FifthPeriod,
  LTC_CNY_Futures_SixthPeriod,
  Native_Difficulty_Futures_FirstPeriod,
  Native_Difficulty_Futures_SecondPeriod,
  Native_Difficulty_Futures_ThirdPeriod,
  Native_Difficulty_Futures_FourthPeriod,  //extended
  Native_Fee_Futures_FirstPeriod,
  Native_Fee_Futures_SecondPeriod,
  Native_Fee_Futures_ThirdPeriod,
  Native_Fee_Futures_FourthPeriod,
  Native_ALL_LTC_IRS_FirstPeriod,
  Native_ALL_LTC_IRS_SecondPeriod,
  Native_LTC_USD_IRS_FirstPeriod,
  Native_LTC_USD_IRS_SecondPeriod,
  ALL_LTC_PERP_GDS,
  LTC_USD_PERP_GDS
};

// channels definitions
const std::string TYPE_COMMIT                   = "commit";
const std::string TYPE_WITHDRAWAL               = "withdrawal";
const std::string TYPE_INSTANT_TRADE            = "instant_trade";
const std::string TYPE_TRANSFER                 = "transfer";
const std::string TYPE_CONTRACT_INSTANT_TRADE   = "contract_instat_trade";
const std::string TYPE_CREATE_CHANNEL           = "create channel";
const std::string TYPE_NEW_ID_REGISTER          = "new id register";
const std::string TYPE_ATTESTATION              = "attestation";

// channel status
const std::string ACTIVE_CHANNEL                = "active";
const std::string CLOSED_CHANNEL                = "closed";

//channel second address PENDING
const std::string CHANNEL_PENDING               = "pending";

// withdrawal status
const int ACTIVE_WITHDRAWAL   = 1;
const int COMPLETE_WITHDRAWAL = 0;

/*24 horus to blocks*/
const int dayblocks = 576;

// limits for margin dynamic
const rational_t factor = rational_t(80,100);  // critical limit
const rational_t factor2 = rational_t(20,100); // normal limits

// define KYC id = 0 for self attestations
const int KYC_0 = 0;

// vesting LTC volume limits
const int64_t INF_VOL_LIMIT = 10000 * COIN;
const int64_t SUP_VOL_LIMIT = 100000000 * COIN;

// upnl calculations
const std::vector<std::string> longActions{ "ShortPosNetted", "OpenLongPosition", "OpenLongPosByShortPosNetted", "LongPosIncreased", "ShortPosNettedPartly"};

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
void addBalances(const std::map<std::string,map<uint32_t, int64_t>>& balances, std::string& lineOut);

/** Returns the marker for transactions. */
const std::vector<unsigned char> GetTLMarker();

//! Used to indicate, whether to automatically commit created transactions
extern bool autoCommit;

//! Global lock for state objects
extern CCriticalSection cs_tally;

class Channel
{
 private:
   std::string multisig;
   std::string first;
   std::string second;
   int last_exchange_block;
   //! Available balances for first  and second addresses properties
   std::map<std::string,map<uint32_t, int64_t>> balances;

 public:
   Channel() : multisig(""), first(""), second(""), last_exchange_block(0) {}
   ~Channel() {}
   Channel(const std::string& m, const std::string& f, const std::string& s, int blk) :  multisig(m),
   first(f), second(s), last_exchange_block(blk) {}

   const std::string& getMultisig() const { return multisig; }
   const std::string& getFirst() const { return first; }
   const std::string& getSecond() const { return second; }
   const std::map<std::string,map<uint32_t, int64_t>>& getBalanceMap() const { return balances; }
   int getLastBlock() const { return last_exchange_block; }
   int64_t getRemaining(const std::string& address, uint32_t propertyId) const;
   int64_t getRemaining(bool flag, uint32_t propertyId) const;
   bool isPartOfChannel(const std::string& address) const;

   void setLastBlock(int block) { last_exchange_block += block;}
   void setBalance(const std::string& sender, uint32_t propertyId, uint64_t amount);
   void setSecond(const std::string& sender) { second = sender ; }
   bool updateChannelBal(const std::string& address, uint32_t propertyId, int64_t amount);
   bool updateLastExBlock(int nBlock);

 };


class nodeReward
{
  private:
    int64_t p_Reward;
    int p_lastBlock;
    std::map<string, int64_t> winners;
    std::map<string, bool> nodeRewardsAddrs;

  public:
    nodeReward() : p_Reward(0), p_lastBlock(0) {}
    ~nodeReward() {}

    void sendNodeReward(const std::string& consensusHash, const int& nHeight, bool fTest);
    const int64_t& getNextReward() const { return p_Reward;}
    const int& getLastBlock() const { return p_lastBlock;}
    bool nextReward(const int& nHeight);
    bool isWinnerAddress(const std::string& consensusHash, const std::string& address, bool fTest);
    const std::map<string, bool>& getnodeRewardAddrs() const { return nodeRewardsAddrs; }
    void updateAddressStatus(const std::string& address, bool newStatus);
    bool isAddressIncluded(const std::string& address);

    void saveNodeReward(ofstream &file, CHash256& hasher);
    void clearNodeRewardMap() { nodeRewardsAddrs.clear(); }
    const std::map<string, int64_t>& getWinners() const { return winners; }
    void addWinner(const std::string& address, int64_t amount);
    void clearWinners() { winners.clear(); }
    void setLastBlock(int lastBlock) { p_lastBlock = lastBlock; }
    void setLastReward(int64_t reward) { p_Reward = reward; }

 };

 class blocksettlement
 {
  private:
    int last_block;
    //! Available settlement prices
    std::map<std::string,map<uint32_t, int64_t>> prices;
    //! Insurance fund to pay the losses on settlements
    std::map<uint32_t, int64_t> insurance_fund;
  public:
    blocksettlement() : last_block(0) {}
    ~blocksettlement() {}

    const std::map<std::string,map<uint32_t, int64_t>>& getSettlementPrices() const { return prices; }
    int getLastBlock() const { return last_block; }

    void makeSettlement();

    int64_t getTotalLoss(const uint32_t& contractId, const uint32_t& notionalSize);

    //! insurance funds are either in ALL which is the numerator of native insurance funds
    //(with 50% short the contracts as a hedge as well) or they are in e.g. USDC on a linear BTC/USDC contract, where some % (to be decided by contract operator when they create it) is held as a long hedge for any short blow-outs
    int64_t getInsurance(const uint32_t& propertyId) const;

    void lossSocialization(const uint32_t& contractId, int64_t fullAmount);

    //! more auxilar functions here!
    bool update_Insurance(const uint32_t& propertyId, int64_t amount);

  };

extern blocksettlement bS;

/* LevelDB based storage for  Trade Layer transaction data.  This will become the new master database, holding serialized Trade Layer transactions.
 *  Note, intention is to consolidate and clean up data storage
 */
class CtlTransactionDB : public CDBBase
{
public:
    CtlTransactionDB(const fs::path& path, bool fWipe)
    {
        leveldb::Status status = Open(path, fWipe);
        if (msc_debug_persistence) PrintToLog("Loading master transactions database: %s\n", status.ToString());
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
    CMPTxList(const fs::path& path, bool fWipe)
    {
        leveldb::Status status = Open(path, fWipe);
        if (msc_debug_persistence) PrintToLog("Loading tx meta-info database: %s\n", status.ToString());
    }

    virtual ~CMPTxList()
      {
        if (msc_debug_persistence) PrintToLog("CMPTxList closed\n");
      }

    void recordTX(const uint256 &txid, bool fValid, int nBlock, unsigned int type, uint64_t nValue, int interp_ret);
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

    void recordPaymentTX(const uint256 &txid, bool fValid, int nBlock, unsigned int vout, unsigned int propertyId, uint64_t nValue, uint64_t amountPaid, string buyer, string seller);
    void recordMetaDExCancelTX(const uint256 &txidMaster, const uint256 &txidSub, bool fValid, int nBlock, unsigned int propertyId, uint64_t nValue);
    void recordContractDexCancelTX(const uint256 &txidMaster, const uint256 &txidSub, bool fValid, int nBlock, unsigned int propertyId, uint64_t nValue);
    void recordNewInstantLTCTrade(const uint256& txid, const std::string& channelAddr, const std::string& seller, const std::string& buyer, uint32_t propertyIdForSale, uint64_t amount_purchased, uint64_t price, int blockNum, int blockIndex);

    uint256 findMetaDExCancel(const uint256 txid);

    /** Returns the number of sub records. */
    int getNumberOfMetaDExCancels(const uint256 txid);
    int getNumberOfContractDexCancels(const uint256 txid);
    void getMPTransactionAddresses(std::vector<std::string> &vaddrs);

    void getDExTrades(const std::string& address, uint32_t propertyId, UniValue& responseArray, uint64_t count);
    void getChannelTrades(const std::string& address, const std::string& channel, uint32_t propertyId, UniValue& responseArray, uint64_t count);

};

/** LevelDB based storage for the trade history. Trades are listed with key "txid1+txid2".
 */
class CMPTradeList : public CDBBase
{
 public:
  CMPTradeList(const fs::path& path, bool fWipe)
    {
      leveldb::Status status = Open(path, fWipe);
      if (msc_debug_persistence) PrintToLog("Loading trades database: %s\n", status.ToString());
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
  void recordNewChannel(const std::string& channelAddress, const std::string& frAddr, const std::string& secAddr, int blockIndex);
  void recordNewInstantTrade(const uint256& txid, const std::string& channelAddr, const std::string& first, const std::string& second, uint32_t propertyIdForSale, uint64_t amount_forsale, uint32_t propertyIdDesired, uint64_t amount_desired,int blockNum, int blockIndex);
  void recordNewTransfer(const uint256& txid, const std::string& sender, const std::string& receiver, int blockNum, int blockIndex);
  void recordNewInstContTrade(const uint256& txid, const std::string& firstAddr, const std::string& secondAddr, uint32_t property, uint64_t amount_forsale, uint64_t price ,int blockNum, int blockIndex);
  void recordNewIdRegister(const uint256& txid, const std::string& address, const std::string& name, const std::string& website, int blockNum, int blockIndex);
  void recordNewAttestation(const uint256& txid, const std::string& sender, const std::string& receiver, int blockNum, int blockIndex, int kyc_id);
  bool deleteAttestationReg(const std::string& sender,  const std::string& receiver);
  bool getAllCommits(const std::string& senderAddress, UniValue& tradeArray);
  bool getAllWithdrawals(const std::string& senderAddress, UniValue& tradeArray);
  bool getChannelInfo(const std::string& channelAddress, UniValue& tradeArray);
  bool checkChannelRelation(const std::string& address, std::string& channelAddr);
  bool tryAddSecond(const std::string& candidate, const std::string& channelAddr, uint32_t propertyId, uint64_t amount_commited);
  bool setChannelClosed(const std::string& channelAddr);
  uint64_t addWithAndCommits(const std::string& channelAddr, const std::string& senderAddr, uint32_t propertyId);
  uint64_t addTrades(const std::string& channelAddr, const std::string& senderAddr, uint32_t propertyId);
  uint64_t addClosedWithrawals(const std::string& channelAddr, const std::string& receiver, uint32_t propertyId);
  bool updateWithdrawal(const std::string& senderAddress, const std::string& channelAddress);

  //KYC
  bool updateIdRegister(const uint256& txid, const std::string& address, const std::string& newAddr, int blockNum, int blockIndex);
  bool checkKYCRegister(const std::string& address, int& kyc_id);
  bool checkAttestationReg(const std::string& address, int& kyc_id);
  bool kycPropertyMatch(uint32_t propertyId, int kyc_id);
  bool kycContractMatch(uint32_t contractId, int kyc_id);
  bool kycLoop(UniValue& response);
  bool attLoop(UniValue& response);

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
  void getUpnInfo(const std::string& address, uint32_t contractId, UniValue& response, bool showVerbose);
  bool kycConsensusHash(CSHA256& hasher);
  bool attConsensusHash(CSHA256& hasher);
  void getTokenChannelTrades(const std::string& address, const std::string& channel, uint32_t propertyId, UniValue& responseArray, uint64_t count);
  void getChannelTradesForPair(const std::string& channel, uint32_t propertyIdA, uint32_t propertyIdB, UniValue& responseArray, uint64_t count);
};

class CMPSettlementMatchList : public CDBBase
{
 public:
  CMPSettlementMatchList(const fs::path& path, bool fWipe)
    {
      leveldb::Status status = Open(path, fWipe);
      if (msc_debug_persistence) PrintToLog("Loading settlement match info database: %s\n", status.ToString());
    }

  virtual ~CMPSettlementMatchList() { }
};

//! Available balances of wallet properties
extern std::map<uint32_t, int64_t> global_balance_money;

//! Vector containing a list of properties relative to the wallet
extern std::set<uint32_t> global_wallet_property_list;

//! Map of active channels
extern std::map<std::string,Channel> channels_Map;

//! Cache fees
extern std::map<uint32_t, int64_t> cachefees;
extern std::map<uint32_t, int64_t> cachefees_oracles;

//! Vesting receiver addresses
extern std::set<std::string> vestingAddresses;

//!Contract upnls
extern std::map<std::string, int64_t> sum_upnls;

//! Last unit price for token/LTC
extern std::map<uint32_t, int64_t> lastPrice;

extern std::map<uint32_t, std::map<uint32_t, int64_t>> market_priceMap;
extern std::map<uint32_t, std::map<std::string, double>> addrs_upnlc;
extern std::map<uint32_t, int64_t> VWAPMapContracts;
extern std::map<uint32_t, std::vector<int64_t>> mapContractVolume;
extern std::map<uint32_t, std::vector<int64_t>> mapContractAmountTimesPrice;

extern std::map<uint32_t, std::map<uint32_t, int64_t>> VWAPMap;
extern std::map<uint32_t, std::map<uint32_t, int64_t>> VWAPMapSubVector;
extern std::map<uint32_t, std::map<uint32_t, std::vector<int64_t>>> numVWAPVector;
extern std::map<uint32_t, std::map<uint32_t, std::vector<int64_t>>> denVWAPVector;

extern std::vector<std::map<std::string, std::string>> path_ele;
extern std::vector<std::map<std::string, std::string>> path_elef;

extern nodeReward nR;

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
bool mastercore_handler_tx(const CTransaction& tx, int nBlock, unsigned int idx, const CBlockIndex *pBlockIndex, std::shared_ptr<std::map<COutPoint, Coin>> removedCoin);
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
double getAccumVesting(const int64_t xAxis);

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

  bool isMPinBlockRange(int starting_block, int ending_block, bool bDeleteFound);

  std::string FormatContractMP(int64_t n);

  std::string FormatIndivisibleMP(int64_t n);

  int WalletTxBuilder(const std::string& senderAddress, const std::string& receiverAddress, int64_t referenceAmount,
		      const std::vector<unsigned char>& data, uint256& txid, std::string& rawHex, bool commit, unsigned int minInputs = 1);

  int WalletTxBuilderEx(const std::string& senderAddress, const std::vector<std::string>& receiverAddresses, int64_t referenceAmount,
		      const std::vector<unsigned char>& data, uint256& txid, std::string& rawHex, bool commit, unsigned int minInputs = 1);

  uint32_t GetNextPropertyId(); // maybe move into sp

  CMPTally* getTally(const std::string& address);

  int64_t getTotalTokens(uint32_t propertyId, int64_t* n_owners_total = nullptr);

  std::string strTransactionType(unsigned int txType);

  /** Returns the encoding class, used to embed a payload. */
  int GetEncodingClass(const CTransaction& tx, int nBlock);

  /** Determines, whether it is valid to use a Class D transaction for a given payload size. */
  bool UseEncodingClassD(size_t nDataSize);

  bool getValidMPTX(const uint256 &txid, std::string *reason = nullptr, int *block = nullptr, unsigned int *type = nullptr, uint64_t *nAmended = nullptr);

  bool update_tally_map(const std::string& who, uint32_t propertyId, int64_t amount, TallyType ttype);

  std::string getTokenLabel(uint32_t propertyId);

  bool marginMain(int Block);

  void update_sum_upnls(); // update the sum of all upnls for all addresses.

  int64_t sum_check_upnl(const std::string& address); //  sum of all upnls for a given address.

  int64_t pos_margin(uint32_t contractId, const std::string& address, uint64_t margin_requirement); // return mainteinance margin for a given contrand and address

  bool makeWithdrawals(int Block); // make the withdrawals for multisig channels

  bool closeChannel(const std::string& sender, const std::string& channelAddr);

  // x_Trade function for contracts on instant trade
  bool Instant_x_Trade(const uint256& txid, uint8_t tradingAction, const std::string& channelAddr, const std::string& firstAddr, const std::string& secondAddr, uint32_t property, int64_t amount_forsale, uint64_t price, uint32_t collateral, uint16_t type, int& block, int tx_idx);

  //Fees for contract instant trades
  bool ContInst_Fees(const std::string& firstAddr,const std::string& secondAddr,const std::string& channelAddr, int64_t amountToReserve,uint16_t type, uint32_t colateral);

  // Map of LTC volume
  int64_t LtcVolumen(uint32_t propertyId, const int& fblock, const int& sblock);

  //Map of MetaDEx token volume
  int64_t MdexVolumen(uint32_t property, const int& fblock, const int& sblock);

  //Map of DEx token volume
  int64_t DexVolumen(uint32_t property, const int& fblock, const int& sblock);

  void twapForLiquidation(uint32_t contractId, int blocks);

  int64_t getOracleTwap(uint32_t contractId, int nBlocks);

  int64_t getContractTradesVWAP(uint32_t contractId, int nBlocks);

  // check for vesting
  bool sanityChecks(const std::string& sender, int& aBlock);

  // fee cache buying Alls in mDEx
  bool feeCacheBuy();

  // updating the expiration block for channels
  bool updateLastExBlock(int nBlock, const std::string& sender);

  std::string updateStatus(int64_t oldPos, int64_t newPos);

  void createChannel(const std::string& sender, const std::string& receiver, uint32_t propertyId, uint64_t amount_commited, int block, int tx_id);

  bool channelSanityChecks(const std::string& sender, const std::string& receiver, uint32_t propertyId, uint64_t amount_commited, int block, int tx_idx);

  const std::string getVestingAdmin();

  int64_t lastVolume(uint32_t propertyId, bool tokens);

  bool checkWithdrawal(const std::string& txid, const std::string& channelAddress);

  int64_t increaseLTCVolume(uint32_t propertyId, uint32_t propertyDesired, int aBlock);

  int64_t getVWap(uint32_t propertyId, int aBlock, const std::map<uint32_t,std::map<int,std::vector<std::pair<int64_t,int64_t>>>>& aMap);

  void iterVolume(int64_t& amount, uint32_t propertyId, const int& fblock, const int& sblock, const std::map<int, std::map<uint32_t,int64_t>>& aMap);

  bool Token_LTC_Fees(int64_t& buyer_amountGot, uint32_t propertyId);

  bool checkChannelAddress(const std::string& channelAddress);

  bool LiquidationEngine(int Block);

}

#endif // TRADELAYER_TL_H
