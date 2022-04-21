#ifndef TRADELAYER_MDEX_H
#define TRADELAYER_MDEX_H

#include <tradelayer/ce.h>
#include <tradelayer/tx.h>
#include <tradelayer/tradelayer_matrices.h>
#include <uint256.h>

#include <fstream>
#include <map>
#include <set>
#include <stdint.h>
#include <string>

#include <boost/lexical_cast.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/rational.hpp>

class CHash256;

typedef boost::multiprecision::uint128_t ui128;
typedef boost::rational<boost::multiprecision::checked_int128_t> rational_t;

// MetaDEx trade statuses
#define TRADE_INVALID                -1
#define TRADE_OPEN                    1
#define TRADE_OPEN_PART_FILLED        2
#define TRADE_FILLED                  3
#define TRADE_CANCELLED               4
#define TRADE_CANCELLED_PART_FILLED   5

//Twap constant
const int volumeToVWAP = 10;

const int64_t globalNumPrice = 1;
const int64_t globalDenPrice = 1;


/** Converts price to string. */
std::string xToString(const rational_t& value);

std::string xToString(const uint64_t &value);
std::string xToString(const int64_t  &price);
std::string xToString(const uint32_t &value);
void saveDataGraphs(std::fstream &file, std::string lineOutSixth1, std::string lineOutSixth2, std::string lineOutSixth3, bool savedata_bool);
void saveDataGraphs(std::fstream &file, std::string lineOut);
ui128 multiply_uint64_t(uint64_t &m, uint64_t &n);
ui128 multiply_int64_t(int64_t &m, int64_t &n);
void buyerSettingBalance(int64_t possitive_buy, int64_t negative_buy, std::string buyer_address, int64_t nCouldBuy, int64_t difference_b, uint32_t property_traded);
//! Number of digits of unit price
#define DISPLAY_PRECISION_LEN  50
#define NPTYPES  500

enum MatchReturnType
  {
    NOTHING = 0,
    TRADED = 1,
    TRADED_MOREINSELLER,
    TRADED_MOREINBUYER,
    ADDED,
    CANCELLED,
  };

MatchReturnType x_Trade(CMPMetaDEx* const pnew);

/** A trade on the distributed exchange.
 */
class CMPMetaDEx
{
 private:
  int block;
  uint256 txid;
  unsigned int idx; // index within block
  uint32_t property;
  int64_t amount_forsale;
  uint32_t desired_property;
  int64_t amount_desired;
  int64_t amount_remaining;
  uint8_t subaction;
  std::string addr;

 public:
  uint256 getHash() const { return txid; }
  uint32_t getProperty() const { return property; }
  uint32_t getDesProperty() const { return desired_property; }

  int64_t getAmountForSale() const { return amount_forsale; }
  int64_t getAmountDesired() const { return amount_desired; }
  int64_t getAmountRemaining() const { return amount_remaining; }
  int64_t getAmountToFill() const;

  void setAmountRemaining(int64_t ar, const std::string &label = "");

  void setAmountForsale(int64_t ar, const std::string &label = "");

  uint8_t getAction() const { return subaction; }

  const std::string& getAddr() const { return addr; }

  int getBlock() const { return block; }
  unsigned int getIdx() const { return idx; }

  int64_t getBlockTime() const;

 CMPMetaDEx()
   : block(0), idx(0), property(0), amount_forsale(0), desired_property(0), amount_desired(0),
    amount_remaining(0), subaction(0) {}

 CMPMetaDEx(const std::string& addr, int b, uint32_t c, int64_t nValue, uint32_t cd, int64_t ad,
	    const uint256& tx, uint32_t i, uint8_t suba)
   : block(b), txid(tx), idx(i), property(c), amount_forsale(nValue), desired_property(cd), amount_desired(ad),
    amount_remaining(nValue), subaction(suba), addr(addr) {}

 CMPMetaDEx(const std::string& addr, int b, uint32_t c, int64_t nValue, uint32_t cd, int64_t ad,
	    const uint256& tx, uint32_t i, uint8_t suba, int64_t ar)
   : block(b), txid(tx), idx(i), property(c), amount_forsale(nValue), desired_property(cd), amount_desired(ad),
    amount_remaining(ar), subaction(suba), addr(addr) {}

 CMPMetaDEx(const CMPTransaction& tx)
   : block(tx.block), txid(tx.txid), idx(tx.tx_idx), property(tx.property), amount_forsale(tx.nValue),
    desired_property(tx.desired_property), amount_desired(tx.desired_value), amount_remaining(tx.nValue),
    subaction(tx.subaction), addr(tx.sender) {}

  std::string ToString() const;

  rational_t unitPrice() const;
  rational_t inversePrice() const;

  /** Used for display of unit prices to 8 decimal places at UI layer. */
  std::string displayUnitPrice() const;
  /** Used for display of unit prices with 50 decimal places at RPC layer. */
  std::string displayFullUnitPrice() const;

  void saveOffer(std::ofstream& file, CHash256& hasher) const;

  std::string GenerateConsensusString() const;


};


class CMPContractDex : public CMPMetaDEx
{
 private:
  uint64_t effective_price;
  uint8_t trading_action;
  int64_t amount_reserved;
  bool liquidation_order;


 public:
 CMPContractDex()
   : effective_price(0), trading_action(0), amount_reserved(0), liquidation_order(false) {}

 CMPContractDex(const std::string& addr, int b, uint32_t c, int64_t nValue, uint32_t cd, int64_t ad, const uint256& tx, uint32_t i, uint8_t suba, uint64_t effp, uint8_t act, uint64_t amr, bool liquidation)
   : CMPMetaDEx(addr, b, c, nValue, cd, ad, tx, i, suba), effective_price(effp), trading_action(act), amount_reserved(amr),  liquidation_order(liquidation)  {}

  /*Remember: Needed for tradelayer.cpp "ar"*/
 CMPContractDex(const std::string& addr, int b, uint32_t c, int64_t nValue, uint32_t cd, int64_t ad, const uint256& tx, uint32_t i, uint8_t suba, int64_t ar, uint64_t effp, uint8_t act, uint64_t amr)
   : CMPMetaDEx(addr, b, c, nValue, cd, ad, tx, i, suba, ar), effective_price(effp), trading_action(act), amount_reserved(amr), liquidation_order(false) {}

 CMPContractDex(const CMPTransaction &tx)
   : CMPMetaDEx(tx), effective_price(tx.effective_price), trading_action(tx.trading_action), liquidation_order(false) {}

  virtual ~CMPContractDex(){}

  uint64_t getEffectivePrice() const { return effective_price; }
  uint8_t getTradingAction() const { return trading_action; }
  uint64_t getAmountReserved() const { return amount_reserved; }
  bool isLiquidationOrder() const { return liquidation_order; }

  std::string displayFullContractPrice() const;
  std::string ToString() const;

  void saveOffer(std::ofstream& file, CHash256& hasher) const;

  void setPrice(int64_t price);

  bool updateAmountReserved(int64_t amount);

  bool Fees(std::string addressTaker,std::string addressMaker, int64_t nCouldBuy,uint32_t contractId);

  ///////////////////////////////
  /*New things for Contracts*/
  friend MatchReturnType x_Trade(CMPContractDex* const pnew);
  ///////////////////////////////

  // Generates a consensus string for hashing based on a ContractDex object
  std::string GenerateConsensusString() const;

};

namespace mastercore
{
  struct MetaDEx_compare
  {
    bool operator()(const CMPMetaDEx& lhs, const CMPMetaDEx& rhs) const;
  };

  // ---------------
  //! Set of objects sorted by block+idx
  typedef std::set<CMPMetaDEx, MetaDEx_compare> md_Set;
  //! Map of prices; there is a set of sorted objects for each price
  typedef std::map<rational_t, md_Set> md_PricesMap;
  //! Map of properties; there is a map of prices for exchange each property
  typedef std::map<uint32_t, md_PricesMap> md_PropertiesMap;

  /**  Global map for cumulative volume by pair of properties
   *   Block, property -> put the amount of property traded.
   */
  extern std::map<int, std::map<uint32_t,int64_t>> metavolume;

  //! Global map for last contract price
  extern std::map<uint32_t, std::map<int,std::vector<int64_t>>> cdexlastprice;

  //! Global map for price and order data
  extern md_PropertiesMap metadex;

  // TODO: explore a property-pair, instead of a single priceoperty as map's key........
  md_PricesMap* get_Prices(uint32_t prop);
  md_Set* get_Indexes(md_PricesMap* p, rational_t price);

  uint64_t edgeOrderbook(uint32_t contractId, uint8_t tradingAction);

  // --------------

  struct ContractDex_compare
  {
    bool operator()(const CMPContractDex &lhs, const CMPContractDex &rhs) const;
  };

  typedef std::set<CMPContractDex, ContractDex_compare> cd_Set;
  typedef std::map<uint64_t, cd_Set> cd_PricesMap;
  typedef std::map<uint32_t, cd_PricesMap> cd_PropertiesMap;

  extern cd_PropertiesMap contractdex;

  cd_PricesMap *get_PricesCd(uint32_t prop);
  cd_Set *get_IndexesCd(cd_PricesMap *p, uint64_t price);

  void LoopBiDirectional(cd_PricesMap* const ppriceMap, uint8_t trdAction, MatchReturnType &NewReturn, CMPContractDex* const pnew, const uint32_t propertyForSale);
  void x_TradeBidirectional(typename cd_PricesMap::iterator &it_fwdPrices, typename cd_PricesMap::reverse_iterator &it_bwdPrices, uint8_t trdAction, CMPContractDex* const pnew, const uint64_t sellerPrice, const uint32_t propertyForSale, MatchReturnType &NewReturn);
  int ContractDex_ADD(const std::string& sender_addr, uint32_t prop, int64_t amount, int block, const uint256& txid, unsigned int idx, uint64_t effective_price, uint8_t trading_action, int64_t amountToReserve);
  bool ContractDex_INSERT(const CMPContractDex &objContractDex);
  void ContractDex_debug_print(bool bShowPriceLevel, bool bDisplay);
  const CMPContractDex *ContractDex_RetrieveTrade(const uint256& txid);
  bool ContractDex_isOpen(const uint256& txid, uint32_t propertyIdForSale);
  int ContractDex_getStatus(const uint256& txid, uint32_t propertyIdForSale, int64_t amountForSale, int64_t totalSold);
  std::string ContractDex_getStatusText(int tradeStatus);
  int ContractDex_SHUTDOWN();
  int ContractDex_SHUTDOWN_ALLPAIR();
  int ContractDex_CANCEL_ALL_FOR_PAIR(const uint256& txid, unsigned int block, const std::string& sender_addr, uint32_t prop, uint32_t property_desired);

  ///////////////////////////////////
  /** New things for Contracts */
  int ContractDex_CANCEL_EVERYTHING(const uint256& txid, unsigned int block, const std::string& sender_addr, uint32_t contractId);
  int ContractDex_CLOSE_POSITION(const uint256& txid, unsigned int block, const std::string& sender_addr, uint32_t contractId, uint32_t collateralCurrency, bool liquidation);
  int ContractDex_CANCEL_IN_ORDER(const std::string& sender_addr, uint32_t contractId);
  int ContractDex_CANCEL_AT_PRICE(const uint256& txid, unsigned int block, const std::string& sender_addr, uint32_t prop, int64_t amount, uint32_t property_desired, int64_t amount_desired, uint64_t effective_price, uint8_t trading_action);
  int ContractDex_ADD_ORDERBOOK_EDGE(const std::string& sender_addr, uint32_t contractId, int64_t amount, int block, const uint256& txid, unsigned int idx, uint8_t trading_action, int64_t amount_to_reserve, bool liquidation);
  int ContractDex_ADD_MARKET_PRICE(const std::string& sender_addr, uint32_t contractId, int64_t amount, int block, const uint256& txid, unsigned int idx, uint8_t trading_action, int64_t amount_to_reserve, bool liquidation);
  int ContractDex_CANCEL_FOR_BLOCK(const uint256& txid, int block,unsigned int idx, const std::string& sender_addr);
  bool ContractDex_Fees(const CMPContractDex* maker, const CMPContractDex* taker, int64_t nCouldBuy);
  bool ContractDex_CHECK_ORDERS(const std::string& sender_addr, uint32_t contractId);
  bool ContractDex_LIQUIDATION_VOLUME(uint32_t contractId, int64_t& volume, int64_t& vwap, int64_t& bankruptcyVWAP, bool& sign);
  ///////////////////////////////////

  int MetaDEx_ADD(const std::string& sender_addr, uint32_t, int64_t, int block, uint32_t property_desired, int64_t amount_desired, const uint256& txid, unsigned int idx);
  int MetaDEx_CANCEL_AT_PRICE(const uint256&, uint32_t, const std::string&, uint32_t, int64_t, uint32_t, int64_t);
  int MetaDEx_CANCEL_ALL_FOR_PAIR(const uint256&, uint32_t, const std::string&, uint32_t, uint32_t);
  int MetaDEx_CANCEL_EVERYTHING(const uint256& txid, uint32_t block, const std::string& sender_addr);
  int MetaDEx_CANCEL(const uint256& txid, const std::string& sender_addr, unsigned int block, const std::string& hash);
  int MetaDEx_CANCEL_ALL_FOR_PAIR(const uint256& txid, unsigned int block, const std::string& sender_addr, uint32_t prop, uint32_t property_desired);
  int MetaDEx_CANCEL_AT_PRICE(const uint256& txid, unsigned int block, const std::string& sender_addr, uint32_t prop, int64_t amount, uint32_t property_desired, int64_t amount_desired);
  int MetaDEx_SHUTDOWN();
  int MetaDEx_SHUTDOWN_ALLPAIR();
  bool MetaDEx_INSERT(const CMPMetaDEx& objMetaDEx);
  void MetaDEx_debug_print(bool bShowPriceLevel = false, bool bDisplay = false);
  bool MetaDEx_isOpen(const uint256& txid, uint32_t propertyIdForSale = 0);
  int MetaDEx_getStatus(const uint256& txid, uint32_t propertyIdForSale, int64_t amountForSale, int64_t totalSold = -1);
  std::string MetaDEx_getStatusText(int tradeStatus);
  int64_t getPairMarketPrice(const std::string& num, const std::string& den);
  int64_t getVWAPPriceByPair(const std::string& num, const std::string& den);
  int64_t getVWAPPriceContracts(const std::string& namec);
  bool MetaDEx_Search_ALL(uint64_t& amount, uint32_t propertyOffered);
  bool MetaDEx_Fees(const CMPMetaDEx *pnew, const CMPMetaDEx *pold, int64_t nCouldBuy);
  bool ContractDex_ADD_LTCVolume(int64_t nCouldBuy,uint32_t contractId);

  // Locates a trade in the MetaDEx maps via txid and returns the trade object
  const CMPMetaDEx* MetaDEx_RetrieveTrade(const uint256& txid);
  const CMPContractDex* ContractDex_RetrieveTrade(const uint256&);
  int ContractDex_CANCEL(const std::string& sender_addr, const std::string& hash);
  bool checkReserve(const std::string& address, int64_t amount, uint32_t propertyId, int64_t& nBalance);
  bool checkContractReserve(const std::string& address, int64_t amount, uint32_t contractId, int64_t leverage, int64_t& nBalance, int64_t& amountToReserve);
  int64_t getTotalLives(uint32_t contractId);
  int64_t ContractBasisPoints(const CDInfo::Entry& cd, int64_t amount, int64_t leverage);

  void updateAllEntry(int64_t oldPosition, int64_t newPosition, int64_t nCouldBuy, uint32_t contract_traded, CMPContractDex* elem, const CDInfo::Entry& cd);
  void takeMargin(int64_t amount, uint32_t contract_traded, const CDInfo::Entry& cd, CMPContractDex *elem);
}

#endif // TRADELAYER_MDEX_H
