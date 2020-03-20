#include "tradelayer/mdex.h"
#include "tradelayer/errors.h"
#include "tradelayer/log.h"
#include "tradelayer/tradelayer.h"
#include "tradelayer/rules.h"
#include "tradelayer/sp.h"
#include "tradelayer/tx.h"
#include "tradelayer/uint256_extensions.h"
#include "arith_uint256.h"
#include "chain.h"
#include "tinyformat.h"
#include "uint256.h"
#include "tradelayer/tradelayer_matrices.h"
#include "tradelayer/externfns.h"
#include "tradelayer/operators_algo_clearing.h"
#include "validation.h"
#include "utilsbitcoin.h"
#include <univalue.h>
#include <boost/lexical_cast.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/rational.hpp>
#include <openssl/sha.h>
#include <assert.h>
#include <stdint.h>
#include <iostream>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <string>

typedef boost::multiprecision::cpp_dec_float_100 dec_float;
typedef boost::multiprecision::checked_int128_t int128_t;

using namespace mastercore;

//! Number of digits of unit price
#define DISPLAY_PRECISION_LEN  50

//! Global map for price and order data
md_PropertiesMap mastercore::metadex;

chn_PropertiesMap mastercore::chndex;

extern volatile uint64_t marketPrice;
extern volatile int idx_q;
extern int64_t factorE;
extern uint64_t marketP[NPTYPES];
extern int expirationAchieve;
extern std::map<uint32_t, std::map<uint32_t, int64_t>> market_priceMap;
extern std::map<uint32_t, std::map<uint32_t, int64_t>> numVWAPMap;
extern std::map<uint32_t, std::map<uint32_t, int64_t>> denVWAPMap;
extern std::map<uint32_t, std::map<uint32_t, int64_t>> VWAPMap;
extern std::map<uint32_t, std::map<uint32_t, std::vector<int64_t>>> numVWAPVector;
extern std::map<uint32_t, std::map<uint32_t, std::vector<int64_t>>> denVWAPVector;
extern std::map<uint32_t, std::map<uint32_t, int64_t>> VWAPMapSubVector;
extern std::map<uint32_t, std::vector<int64_t>> mapContractAmountTimesPrice;
extern std::map<uint32_t, std::vector<int64_t>> mapContractVolume;
extern std::map<int, std::map<std::pair<uint32_t, uint32_t>, int64_t>> MapMetaVolume;
extern std::map<uint32_t, int64_t> VWAPMapContracts;
extern std::map<uint32_t, int64_t> cachefees;
extern int n_cols;
extern int n_rows;
extern MatrixTLS *pt_ndatabase;
extern int64_t globalNumPrice;
extern int64_t globalDenPrice;
extern int lastBlockg;
extern int volumeToVWAP;
extern int BlockS;

md_PricesMap* mastercore::get_Prices(uint32_t prop)
{
    md_PropertiesMap::iterator it = metadex.find(prop);

    if (it != metadex.end()) return &(it->second);

    return (md_PricesMap*) NULL;
}

chn_PricesMap* mastercore::get_chnPrices(uint32_t prop)
{
    chn_PropertiesMap::iterator it = chndex.find(prop);

    if (it != chndex.end()) return &(it->second);

    return (chn_PricesMap*) NULL;
}

md_Set* mastercore::get_Indexes(md_PricesMap* p, rational_t price)
{
    md_PricesMap::iterator it = p->find(price);

    if (it != p->end()) return &(it->second);

    return (md_Set*) NULL;
}

chn_Set* mastercore::get_chnIndexes(chn_PricesMap* p, rational_t price)
{
    chn_PricesMap::iterator it = p->find(price);

    if (it != p->end()) return &(it->second);

    return (chn_Set*) NULL;
}

cd_PropertiesMap mastercore::contractdex;

cd_PricesMap *mastercore::get_PricesCd(uint32_t prop)
{
    cd_PropertiesMap::iterator it = contractdex.find(prop);

    if (it != contractdex.end()) return &(it->second);

    return (cd_PricesMap*) NULL;
}

cd_Set *mastercore::get_IndexesCd(cd_PricesMap *p, uint64_t price)
{
    cd_PricesMap::iterator it = p->find(price);

    if (it != p->end()) return &(it->second);

    return (cd_Set*) NULL;
}

void mastercore::LoopBiDirectional(cd_PricesMap* const ppriceMap, uint8_t trdAction, MatchReturnType &NewReturn, CMPContractDex* const pnew, const uint32_t propertyForSale)
{
  cd_PricesMap::iterator it_fwdPrices;
  cd_PricesMap::reverse_iterator it_bwdPrices;

  if ( trdAction == BUY )
    {
      for (it_fwdPrices = ppriceMap->begin(); it_fwdPrices != ppriceMap->end(); ++it_fwdPrices)
	{
	  const uint64_t sellerPrice = it_fwdPrices->first;
	  if ( pnew->getEffectivePrice() < sellerPrice )
	    {
	      continue;
	    }
	  x_TradeBidirectional(it_fwdPrices, it_bwdPrices, trdAction, pnew, sellerPrice, propertyForSale, NewReturn);
	}
    }
  else
    {
      for (it_bwdPrices = ppriceMap->rbegin(); it_bwdPrices != ppriceMap->rend(); ++it_bwdPrices)
	{
	  const uint64_t sellerPrice = it_bwdPrices->first;

	  if ( pnew->getEffectivePrice() > sellerPrice )
	    {
	      continue;
	    }
	  x_TradeBidirectional(it_fwdPrices, it_bwdPrices, trdAction, pnew, sellerPrice, propertyForSale, NewReturn);
	}
    }
}

void mastercore::x_TradeBidirectional(typename cd_PricesMap::iterator &it_fwdPrices, typename cd_PricesMap::reverse_iterator &it_bwdPrices, uint8_t trdAction, CMPContractDex* const pnew, const uint64_t sellerPrice, const uint32_t propertyForSale, MatchReturnType &NewReturn)
{
  cd_Set* const pofferSet = trdAction == BUY ? &(it_fwdPrices->second) : &(it_bwdPrices->second);

  /** At good (single) price level and property iterate over offers looking at all parameters to find the match */
  cd_Set::iterator offerIt = pofferSet->begin();

  while ( offerIt != pofferSet->end() )  /** Specific price, check all properties */
    {
      const CMPContractDex* const pold = &(*offerIt);

      assert(pold->getEffectivePrice() == sellerPrice);

      std::string tradeStatus = pold->getEffectivePrice() == sellerPrice ? "Matched" : "NoMatched";

      /** Match Conditions */
      bool boolProperty  = pold->getProperty() != propertyForSale;
      bool boolTrdAction = pold->getTradingAction() == pnew->getTradingAction();
      bool boolAddresses = pold->getAddr() != pnew->getAddr();

      if ( findTrueValue(boolProperty, boolTrdAction, !boolAddresses) )
    	{
	  ++offerIt;
	  continue;
	}

      idx_q += 1;
      // const int idx_qp = idx_q;

      /********************************************************/
      /** Preconditions */
      assert(pold->getProperty() == pnew->getProperty());

      if(msc_debug_x_trade_bidirectional)
      {
          PrintToLog("________________________________________________________\n");
          PrintToLog("Inside x_trade:\n");
          PrintToLog("Checking effective prices and trading actions:\n");
          PrintToLog("Effective price pold: %d\n", FormatContractShortMP(pold->getEffectivePrice()) );
          PrintToLog("Effective price pnew: %d\n", FormatContractShortMP(pnew->getEffectivePrice()) );
          PrintToLog("Amount for sale pold: %d\n", pold->getAmountForSale() );
          PrintToLog("Amount for sale pnew: %d\n", pnew->getAmountForSale() );
          PrintToLog("Trading action pold: %d\n", pold->getTradingAction() );
          PrintToLog("Trading action pnew: %d\n", pnew->getTradingAction() );
          PrintToLog("Trade Status: %s\n", tradeStatus);
          PrintToLog("propertyForSale = %d", propertyForSale);
          PrintToLog("\nlastBlockg = %s\n", lastBlockg);
      }

      /********************************************************/
      uint32_t property_traded = pold->getProperty();
      uint64_t amountpnew = pnew->getAmountForSale();
      uint64_t amountpold = pold->getAmountForSale();

      int64_t poldPositiveBalanceB = getMPbalance(pold->getAddr(), property_traded, POSITIVE_BALANCE);
      int64_t pnewPositiveBalanceB = getMPbalance(pnew->getAddr(), property_traded, POSITIVE_BALANCE);
      int64_t poldNegativeBalanceB = getMPbalance(pold->getAddr(), property_traded, NEGATIVE_BALANCE);
      int64_t pnewNegativeBalanceB = getMPbalance(pnew->getAddr(), property_traded, NEGATIVE_BALANCE);

      if(msc_debug_x_trade_bidirectional)
      {
          PrintToLog("poldPositiveBalanceB: %d, poldNegativeBalanceB: %d\n", poldPositiveBalanceB, poldNegativeBalanceB);
          PrintToLog("pnewPositiveBalanceB: %d, pnewNegativeBalanceB: %d\n", pnewPositiveBalanceB, pnewNegativeBalanceB);
      }

      int64_t possitive_sell = (pold->getTradingAction() == SELL) ? poldPositiveBalanceB : pnewPositiveBalanceB;
      int64_t negative_sell  = (pold->getTradingAction() == SELL) ? poldNegativeBalanceB : pnewNegativeBalanceB;
      int64_t possitive_buy  = (pold->getTradingAction() == SELL) ? pnewPositiveBalanceB : poldPositiveBalanceB;
      int64_t negative_buy   = (pold->getTradingAction() == SELL) ? pnewNegativeBalanceB : poldNegativeBalanceB;

      int64_t seller_amount  = (pold->getTradingAction() == SELL) ? pold->getAmountForSale() : pnew->getAmountForSale();
      int64_t buyer_amount   = (pold->getTradingAction() == SELL) ? pnew->getAmountForSale() : pold->getAmountForSale();
      std::string seller_address = (pold->getTradingAction() == SELL) ? pold->getAddr() : pnew->getAddr();
      std::string buyer_address  = (pold->getTradingAction() == SELL) ? pnew->getAddr() : pold->getAddr();

      int64_t nCouldBuy = buyer_amount < seller_amount ? buyer_amount : seller_amount;

      if (nCouldBuy == 0)
      	{
      	  ++offerIt;
      	  continue;
      	}

      /*************************************************************************************************/
      /** Computing VWAP Price**/

      CMPSPInfo::Entry sp;
      assert(_my_sps->getSP(property_traded, sp));

      uint32_t NotionalSize = sp.notional_size;

      arith_uint256 Volume256_t = mastercore::ConvertTo256(NotionalSize)*mastercore::ConvertTo256(nCouldBuy)/COIN;
      int64_t Volume64_t = mastercore::ConvertTo64(Volume256_t);

      if(msc_debug_x_trade_bidirectional) PrintToLog("\nNotionalSize = %s\t nCouldBuy = %s\t Volume64_t = %s\n",
		  FormatDivisibleMP(NotionalSize), FormatDivisibleMP(nCouldBuy), FormatDivisibleMP(Volume64_t));

      arith_uint256 numVWAP256_t = mastercore::ConvertTo256(sellerPrice)*mastercore::ConvertTo256(Volume64_t)/COIN;
      int64_t numVWAP64_t = mastercore::ConvertTo64(numVWAP256_t);

      threading(property_traded, numVWAP64_t, "cdex_price");
      threading(property_traded, Volume64_t, "cdex_volume");

      std::vector<int64_t> numVWAPpriceContract(mapContractAmountTimesPrice[property_traded].end()-
						std::min(int(mapContractAmountTimesPrice[property_traded].size()), volumeToVWAP),
						mapContractAmountTimesPrice[property_traded].end());
      std::vector<int64_t> denVWAPpriceContract(mapContractVolume[property_traded].end()-
						std::min(int(mapContractVolume[property_traded].size()), volumeToVWAP),
						mapContractVolume[property_traded].end());
      int64_t numVWAPriceh = 0, denVWAPriceh = 0;

      int vwaplength = denVWAPpriceContract.size();
      for (int i = 0; i < vwaplength; i++)
	{
	  numVWAPriceh += numVWAPpriceContract[i];
	  denVWAPriceh += denVWAPpriceContract[i];
	}

      rational_t vwapPricehRat(numVWAPriceh, denVWAPriceh);
      int64_t vwapPriceh64_t = mastercore::RationalToInt64(vwapPricehRat);
      threading(property_traded, vwapPriceh64_t, "cdex_vwap");

      /********************************************************/
      int64_t difference_s = 0, difference_b = 0;
      if (boolAddresses)
	{
	  if ( possitive_sell != 0 )
	    {
	      difference_s = possitive_sell - nCouldBuy;
	      if (difference_s >= 0)
		assert(update_tally_map(seller_address, property_traded, -nCouldBuy, POSITIVE_BALANCE));
	      else
		{
		  assert(update_tally_map(seller_address, property_traded, -possitive_sell, POSITIVE_BALANCE));
		  assert(update_tally_map(seller_address, property_traded, -difference_s, NEGATIVE_BALANCE));
		}
	    }
	  else if ( negative_sell != 0 || negative_sell == 0 || possitive_sell == 0 )
	    assert(update_tally_map(seller_address, property_traded, nCouldBuy, NEGATIVE_BALANCE));

	  if ( negative_buy != 0 )
	    {
	      difference_b = negative_buy - nCouldBuy;
	      if (difference_b >= 0)
		assert(update_tally_map(buyer_address, property_traded, -nCouldBuy, NEGATIVE_BALANCE));
	      else
		{
		  assert(update_tally_map(buyer_address, property_traded, -negative_buy, NEGATIVE_BALANCE));
		  assert(update_tally_map(buyer_address, property_traded, -difference_b, POSITIVE_BALANCE));
		}
	    }
	  else if ( possitive_buy != 0 || possitive_buy == 0 || negative_buy == 0 )
	    assert(update_tally_map(buyer_address, property_traded, nCouldBuy, POSITIVE_BALANCE));
	}
      /********************************************************/
      int64_t poldPositiveBalanceL = getMPbalance(pold->getAddr(), property_traded, POSITIVE_BALANCE);
      int64_t pnewPositiveBalanceL = getMPbalance(pnew->getAddr(), property_traded, POSITIVE_BALANCE);
      int64_t poldNegativeBalanceL = getMPbalance(pold->getAddr(), property_traded, NEGATIVE_BALANCE);
      int64_t pnewNegativeBalanceL = getMPbalance(pnew->getAddr(), property_traded, NEGATIVE_BALANCE);

      std::string Status_s = "Empty";
      std::string Status_b = "Empty";

      NewReturn = TRADED;
      CMPContractDex contract_replacement = *pold;
      int64_t creplNegativeBalance = getMPbalance(contract_replacement.getAddr(), property_traded, NEGATIVE_BALANCE);
      int64_t creplPositiveBalance = getMPbalance(contract_replacement.getAddr(), property_traded, POSITIVE_BALANCE);

      if(msc_debug_x_trade_bidirectional)
      {
          PrintToLog("poldPositiveBalance: %d, poldNegativeBalance: %d\n", poldPositiveBalanceL, poldNegativeBalanceL);
          PrintToLog("pnewPositiveBalance: %d, pnewNegativeBalance: %d\n", pnewPositiveBalanceL, pnewNegativeBalanceL);
          PrintToLog("creplPositiveBalance: %d, creplNegativeBalance: %d\n", creplPositiveBalance, creplNegativeBalance);
      }

      int64_t remaining = seller_amount >= buyer_amount ? seller_amount - buyer_amount : buyer_amount - seller_amount;

      if ( (seller_amount > buyer_amount && pold->getTradingAction() == SELL) || (seller_amount < buyer_amount && pold->getTradingAction() == BUY))
      	{
      	  contract_replacement.setAmountForsale(remaining, "moreinseller");
      	  pnew->setAmountForsale(0, "no_remaining");
      	  NewReturn = TRADED_MOREINSELLER;
      	}
      else if ( (seller_amount < buyer_amount && pold->getTradingAction() == SELL) || (seller_amount > buyer_amount && pold->getTradingAction() == BUY))
      	{
      	  contract_replacement.setAmountForsale(0, "no_remaining");
      	  pnew->setAmountForsale(remaining, "moreinbuyer");
      	  NewReturn = TRADED_MOREINBUYER;
      	}
      else if (seller_amount == buyer_amount)
      	{
      	  pnew->setAmountForsale(0, "no_remaining");
      	  contract_replacement.setAmountForsale(0, "no_remaining");
      	  NewReturn = TRADED;
      	}
      /********************************************************/
      int64_t countClosedSeller = 0, countClosedBuyer  = 0;
      if ( possitive_sell > 0 && negative_sell == 0 )
      	{
      	  if ( pold->getTradingAction() == SELL )
      	    {
      	      Status_s = possitive_sell > creplPositiveBalance && creplPositiveBalance != 0 ? "LongPosNettedPartly" : ( creplPositiveBalance == 0 && creplNegativeBalance == 0 ? "LongPosNetted" : ( creplPositiveBalance == 0 && creplNegativeBalance > 0 ? "OpenShortPosByLongPosNetted" : "LongPosIncreased") );
      	      countClosedSeller = creplPositiveBalance == 0 ? possitive_sell : abs( possitive_sell - creplPositiveBalance );
      	    }
      	  else
      	    {
      	      Status_s = possitive_sell > pnewPositiveBalanceL && pnewPositiveBalanceL != 0 ? "LongPosNettedPartly" : ( pnewPositiveBalanceL == 0 && pnewNegativeBalanceL == 0 ? "LongPosNetted" : ( pnewPositiveBalanceL == 0 && pnewNegativeBalanceL > 0 ? "OpenShortPosByLongPosNetted": "LongPosIncreased") );
      	      countClosedSeller = pnewPositiveBalanceL == 0 ? possitive_sell : abs( possitive_sell - pnewPositiveBalanceL );
      	    }
      	}
      else if ( negative_sell > 0 && possitive_sell == 0 )
      	{
      	  if ( pold->getTradingAction() == SELL )
      	    {
      	      Status_s = negative_sell > creplNegativeBalance && creplNegativeBalance != 0 ? "ShortPosNettedPartly" : ( creplNegativeBalance == 0 && creplPositiveBalance == 0 ? "ShortPosNetted" : ( creplNegativeBalance == 0 && creplPositiveBalance > 0 ? "OpenLongPosByShortPosNetted" : "ShortPosIncreased") );
      	      countClosedSeller = creplNegativeBalance == 0 ? negative_sell : abs( negative_sell - creplNegativeBalance );
      	    }
      	  else
      	    {
      	      Status_s = negative_sell > pnewNegativeBalanceL && pnewNegativeBalanceL != 0 ? "ShortPosNettedPartly" : ( pnewNegativeBalanceL == 0 && pnewPositiveBalanceL == 0 ? "ShortPosNetted" : ( pnewNegativeBalanceL == 0 && pnewPositiveBalanceL > 0 ? "OpenLongPosByShortPosNetted" : "ShortPosIncreased") );
      	      countClosedSeller = pnewNegativeBalanceL == 0 ? negative_sell : abs( negative_sell - pnewNegativeBalanceL );
      	    }
      	}
      else if ( negative_sell == 0 && possitive_sell == 0 )
      	{
      	  if ( pold->getTradingAction() == SELL )
      	    Status_s = creplPositiveBalance > 0 ? "OpenLongPosition" : "OpenShortPosition";
      	  else
      	    Status_s = pnewPositiveBalanceL  > 0 ? "OpenLongPosition" : "OpenShortPosition";
      	  countClosedSeller = 0;
      	}
      /********************************************************/
      if ( possitive_buy > 0 && negative_buy == 0 )
      	{
      	  if ( pold->getTradingAction() == BUY )
      	    {
      	      Status_b = possitive_buy > creplPositiveBalance && creplPositiveBalance != 0 ? "LongPosNettedPartly" : ( creplPositiveBalance == 0 && creplNegativeBalance == 0 ? "LongPosNetted" : ( creplPositiveBalance == 0 && creplNegativeBalance > 0  ? "OpenShortPosByLongPosNetted" : "LongPosIncreased") );
      	      countClosedBuyer = creplPositiveBalance == 0 ? possitive_buy : abs( possitive_buy - creplPositiveBalance );
      	    }
      	  else
      	    {
      	      Status_b = possitive_buy > pnewPositiveBalanceL && pnewPositiveBalanceL != 0 ? "LongPosNettedPartly" : ( pnewPositiveBalanceL == 0 && pnewNegativeBalanceL == 0 ? "LongPosNetted" : ( pnewPositiveBalanceL == 0 && pnewNegativeBalanceL > 0 ? "OpenShortPosByLongPosNetted" : "LongPosIncreased") );
      	      countClosedBuyer = pnewPositiveBalanceL == 0 ? possitive_buy : abs( possitive_buy - pnewPositiveBalanceL );
      	    }
      	}
      else if ( negative_buy > 0 && possitive_buy == 0 )
      	{
      	  if ( pold->getTradingAction() == BUY )
      	    {
      	      Status_b = negative_buy > creplNegativeBalance && creplNegativeBalance != 0 ? "ShortPosNettedPartly" : ( creplNegativeBalance == 0 && creplPositiveBalance == 0 ? "ShortPosNetted" : ( creplNegativeBalance == 0 && creplPositiveBalance > 0 ? "OpenLongPosByShortPosNetted" : "ShortPosIncreased" ) );
      	      countClosedBuyer = creplNegativeBalance == 0 ? negative_buy : abs( negative_buy - creplNegativeBalance );
      	    }
      	  else
      	    {
      	      Status_b = negative_buy > pnewNegativeBalanceL && pnewNegativeBalanceL != 0 ? "ShortPosNettedPartly" : ( pnewNegativeBalanceL == 0 && pnewPositiveBalanceL == 0 ? "ShortPosNetted" : ( pnewNegativeBalanceL == 0 && pnewPositiveBalanceL > 0 ? "OpenLongPosByShortPosNetted" : "ShortPosIncreased") );
      	      countClosedBuyer = pnewNegativeBalanceL == 0 ? negative_buy : abs( negative_buy - pnewNegativeBalanceL );
      	    }
      	}
      else if ( negative_buy == 0 && possitive_buy == 0 )
      	{
      	  if ( pold->getTradingAction() == BUY )
      	    Status_b = creplPositiveBalance > 0 ? "OpenLongPosition" : "OpenShortPosition";
      	  else
      	    Status_b = pnewPositiveBalanceL > 0 ? "OpenLongPosition" : "OpenShortPosition";
      	  countClosedBuyer = 0;
      	}
      /********************************************************/
      int64_t lives_maker = 0, lives_taker = 0;

      if( creplPositiveBalance > 0 && creplNegativeBalance == 0 )
      	lives_maker = creplPositiveBalance;
      else if( creplNegativeBalance > 0 && creplPositiveBalance == 0 )
      	lives_maker = creplNegativeBalance;

      if( pnewPositiveBalanceL && pnewNegativeBalanceL == 0 )
      	lives_taker = pnewPositiveBalanceL;
      else if( pnewNegativeBalanceL > 0 && pnewPositiveBalanceL == 0 )
      	lives_taker = pnewNegativeBalanceL;

      if ( countClosedSeller < 0 ) countClosedSeller = 0;
      if ( countClosedBuyer  < 0 ) countClosedBuyer  = 0;
      /********************************************************/
      std::string Status_maker = "", Status_taker = "";
      if (pold->getAddr() == seller_address)
      	{
      	  Status_maker = Status_s;
      	  Status_taker = Status_b;
      	}
      else
      	{
      	  Status_maker = Status_b;
      	  Status_taker = Status_s;
      	}

      if(msc_debug_x_trade_bidirectional) PrintToLog("Status_maker = %d, Status_taker = %d\n", Status_maker, Status_taker);

      std::string Status_s0 = "EmptyStr", Status_s1 = "EmptyStr", Status_s2 = "EmptyStr", Status_s3 = "EmptyStr";
      std::string Status_b0 = "EmptyStr", Status_b1 = "EmptyStr", Status_b2 = "EmptyStr", Status_b3 = "EmptyStr";

      int64_t lives_maker0 = 0, lives_maker1 = 0, lives_maker2 = 0, lives_maker3 = 0;
      int64_t lives_taker0 = 0, lives_taker1 = 0, lives_taker2 = 0, lives_taker3 = 0;
      int64_t nCouldBuy0 = 0, nCouldBuy1 = 0, nCouldBuy2 = 0, nCouldBuy3 = 0;

      lives_maker0 = lives_maker;
      lives_taker0 = lives_taker;
      nCouldBuy0 = nCouldBuy;
      /********************************************************/
      if ( pold->getTradingAction() == SELL )
	{
	  // If maker Sell and Open Short by Long Netted: status_sj -> makers
	  if ( Status_maker == "OpenShortPosByLongPosNetted" )
	    {
	      if ( Status_taker == "OpenLongPosByShortPosNetted" )
		{
		  if ( possitive_sell > negative_buy )
		    {
		      Status_s1  = "LongPosNettedPartly";
		      lives_maker1   = possitive_sell - negative_buy;
		      Status_b1  = "ShortPosNetted";
		      lives_taker1   = 0;
		      nCouldBuy1 = negative_buy;

		      Status_s2  = "LongPosNetted";
		      lives_maker2   = 0;
		      Status_b2  = "OpenLongPosition";
		      lives_taker2   = lives_maker1;
		      nCouldBuy2 = lives_maker1;

		      Status_s3  = "OpenShortPosition";
		      lives_maker3   = nCouldBuy - possitive_sell;
		      Status_b3  = "LongPosIncreased";
		      lives_taker3   = lives_taker2 + lives_maker3;
		      nCouldBuy3 = lives_maker3;

		    }
		  else if ( possitive_sell < negative_buy )
		    {
		      Status_s1  = "LongPosNetted";
		      lives_maker1   = 0;
		      Status_b1  = "ShortPosNettedPartly";
		      lives_taker1   = negative_buy - possitive_sell;
		      nCouldBuy1 = possitive_sell;

		      Status_s2  = "OpenShortPosition";
		      lives_maker2   = negative_buy - possitive_sell;
		      Status_b2  = "ShortPosNetted";
		      lives_taker2   = 0;
		      nCouldBuy2 = lives_maker2;

		      Status_b3  = "OpenLongPosition";
		      lives_taker3   = nCouldBuy - negative_buy;
		      Status_s3  = "ShortPosIncreased";
		      lives_maker3   = lives_maker2 + lives_taker3;
		      nCouldBuy3 = lives_taker3;

		    }
		  else if ( possitive_sell == negative_buy )
		    {
		      Status_s1  = "LongPosNetted";
		      lives_maker1   = 0;
		      Status_b1  = "ShortPosNetted";
		      lives_taker1   = 0;
		      nCouldBuy1 = possitive_sell;

		      Status_s2  = "OpenShortPosition";
		      lives_maker2   = nCouldBuy - possitive_sell;
		      Status_b2  = "OpenLongPosition";
		      lives_taker2   = lives_maker2;
		      nCouldBuy2 = lives_maker2;
		    }
		}
	      else if ( Status_taker == "ShortPosNettedPartly" )
		{
		  Status_s1  = "LongPosNetted";
		  lives_maker1   = 0;
		  Status_b1  = "ShortPosNettedPartly";
		  lives_taker1   = negative_buy - possitive_sell;
		  nCouldBuy1 = possitive_sell;

		  Status_s2  = "OpenShortPosition";
		  lives_maker2   = nCouldBuy - possitive_sell;
		  Status_b2  = "ShortPosNettedPartly";
		  lives_taker2   = lives_taker1 - lives_maker2;
		  nCouldBuy2 = lives_maker2;

		}
	      else if ( Status_taker == "ShortPosNetted" )
		{
		  Status_s1  = "LongPosNetted";
		  lives_maker1   = 0;
		  Status_b1  = "ShortPosNettedPartly";
		  lives_taker1   = negative_buy - possitive_sell;
		  nCouldBuy1 = possitive_sell;

		  Status_s2  = "OpenShortPosition";
		  lives_maker2   = nCouldBuy - possitive_sell;
		  Status_b2  = "ShortPosNetted";
		  lives_taker2   = 0;
		  nCouldBuy2 = lives_maker2;

		}
	      else if ( Status_taker == "OpenLongPosition" )
		{
		  Status_s1  = "LongPosNetted";
		  lives_maker1   = 0;
		  Status_b1  = "OpenLongPosition";
		  lives_taker1   = possitive_sell;
		  nCouldBuy1 = possitive_sell;

		  Status_s2  = "OpenShortPosition";
		  lives_maker2   = nCouldBuy - possitive_sell;
		  Status_b2  = "LongPosIncreased";
		  lives_taker2   = lives_taker1 + lives_maker2;
		  nCouldBuy2 = lives_maker2;

		}
	      else if ( Status_taker == "LongPosIncreased" )
		{
		  Status_s1  = "LongPosNetted";
		  lives_maker1   = 0;
		  Status_b1  = "LongPosIncreased";
		  lives_taker1   = possitive_buy + possitive_sell;
		  nCouldBuy1 = possitive_sell;

		  Status_s2  = "OpenShortPosition";
		  lives_maker2   = nCouldBuy - possitive_sell;
		  Status_b2  = "LongPosIncreased";
		  lives_taker2   = lives_taker1 + lives_maker2;
		  nCouldBuy2 = lives_maker2;
		}
	    }
	  // Checked
	}
      else
	{
	  // If maker Buy and Open Long by Short Netted: status_bj -> makers
	  if ( Status_maker == "OpenLongPosByShortPosNetted" )
	    {
	      if ( Status_taker == "OpenShortPosByLongPosNetted" )
		{
		  if ( negative_buy < possitive_sell )
		    {
		      Status_b1  = "ShortPosNetted";
		      lives_maker1   = 0;
		      Status_s1  = "LongPosNettedPartly";
		      lives_taker1   = possitive_sell - negative_buy;
		      nCouldBuy1 = negative_buy;

		      Status_b2  = "OpenLongPosition";
		      lives_maker2   = lives_taker1;
		      Status_s2  = "LongPosNetted";
		      lives_taker2   = 0;
		      nCouldBuy2 = lives_taker1;

		      Status_b3  = "LongPosIncreased";
		      lives_maker3   = lives_maker2 + nCouldBuy - possitive_sell;
		      Status_s3  = "OpenShortPosition";
		      lives_taker3   = nCouldBuy - possitive_sell;
		      nCouldBuy3 = lives_taker3;

		    }
		  else if ( negative_buy > possitive_sell )
		    {
		      Status_b1  = "ShortPosNettedPartly";
		      lives_maker1   = negative_buy - possitive_sell;
		      Status_s1  = "LongPosNetted";
		      lives_taker1   = 0;
		      nCouldBuy1 = possitive_sell;

		      Status_b2  = "ShortPosNetted";
		      lives_maker2   = 0;
		      Status_s2  = "OpenShortPosition";
		      lives_taker2   = lives_maker1;
		      nCouldBuy2 = lives_maker1;

		      Status_b3  = "OpenLongPosition";
		      lives_maker3   = nCouldBuy - negative_buy;
		      Status_s3  = "ShortPosIncreased";
		      lives_taker3   = lives_taker2 + lives_maker3;
		      nCouldBuy3 = lives_maker3;

		    }
		  else if ( negative_buy == possitive_sell )
		    {
		      Status_b1  = "ShortPosNetted";
		      lives_maker1   = 0;
		      Status_s1  = "LongPosNetted";
		      lives_taker1   = 0;
		      nCouldBuy1 = possitive_sell;

		      Status_b2  = "OpenLongPosition";
		      lives_maker2   = nCouldBuy - possitive_sell;
		      Status_s2  = "OpenShortPosition";
		      lives_taker2   = lives_maker2;
		      nCouldBuy2 = lives_maker2;
		    }
		}
	      else if ( Status_taker == "LongPosNettedPartly" )
		{
		  Status_b1  = "ShortPosNetted";
		  lives_maker1   = 0;
		  Status_s1  = "LongPosNettedPartly";
		  lives_taker1 = possitive_sell - negative_buy;
		  nCouldBuy1 = negative_buy;

		  Status_b2  = "OpenLongPosition";
		  lives_maker2  = nCouldBuy - negative_buy;
		  Status_s2  = "LongPosNettedPartly";
		  lives_taker2  = lives_taker1 - lives_maker2;
		  nCouldBuy2 = lives_maker2;

		}
	      else if ( Status_taker == "LongPosNetted" )
		{
		  Status_b1  = "ShortPosNetted";
		  lives_maker1   = 0;
		  Status_s1  = "LongPosNettedPartly";
		  lives_taker1   = possitive_sell - negative_buy;
		  nCouldBuy1 = negative_buy;

		  Status_b2  = "OpenLongPosition";
		  lives_maker2   = nCouldBuy - negative_buy;
		  Status_s2  = "LongPosNetted";
		  lives_taker2   = 0;
		  nCouldBuy2 = lives_maker2;

		}
	      else if ( Status_taker == "OpenShortPosition" )
		{
		  Status_b1  = "ShortPosNetted";
		  lives_maker1   = 0;
		  Status_s1  = "OpenShortPosition";
		  lives_taker1   = negative_buy;
		  nCouldBuy1 = negative_buy;

		  Status_b2  = "OpenLongPosition";
		  lives_maker2   = nCouldBuy - negative_buy;
		  Status_s2  = "ShortPosIncreased";
		  lives_taker2   = lives_taker1 + lives_maker2;
		  nCouldBuy2 = lives_maker2;

		}
	      else if ( Status_taker == "ShortPosIncreased" )
		{
		  Status_b1  = "ShortPosNetted";
		  lives_maker1   = 0;
		  Status_s1  = "ShortPosIncreased";
		  lives_taker1   = negative_sell + negative_buy;
		  nCouldBuy1 = negative_buy;

		  Status_b2  = "OpenLongPosition";
		  lives_maker2   = nCouldBuy - negative_buy;
		  Status_s2  = "ShortPosIncreased";
		  lives_taker2   = lives_taker1 + lives_maker2;
		  nCouldBuy2 = lives_maker2;
		}
	    }
	  // Checked
	}
      /********************************************************/
      if ( pold->getTradingAction() == BUY )
	{
	  // If taker Sell and Open Short by Long Netted: status_sj -> taker
	  if ( Status_taker == "OpenShortPosByLongPosNetted" )
	    {
	      if ( Status_maker == "OpenLongPosByShortPosNetted" )
		{
		  if ( possitive_sell > negative_buy )
		    {
		      Status_s1  = "LongPosNettedPartly";
		      lives_taker1   = possitive_sell - negative_buy;
		      Status_b1  = "ShortPosNetted";
		      lives_maker1   = 0;
		      nCouldBuy1 = negative_buy;

		      Status_s2  = "LongPosNetted";
		      lives_taker2   = 0;
		      Status_b2  = "OpenLongPosition";
		      lives_maker2   = lives_taker1;
		      nCouldBuy2 = lives_taker1;

		      Status_s3  = "OpenShortPosition";
		      lives_taker3   = nCouldBuy - possitive_sell;
		      Status_b3  = "LongPosIncreased";
		      lives_maker3   = lives_maker2 + lives_taker3;
		      nCouldBuy3 = lives_taker3;

		    }
		  else if ( possitive_sell < negative_buy )
		    {
		      Status_s1  = "LongPosNetted";
		      lives_taker1   = 0;
		      Status_b1  = "ShortPosNettedPartly";
		      lives_maker1   = negative_buy - possitive_sell ;
		      nCouldBuy1 = possitive_sell;

		      Status_s2  = "OpenShortPosition";
		      lives_taker2   = lives_maker1;
		      Status_b2  = "ShortPosNetted";
		      lives_maker2   = 0;
		      nCouldBuy2 = lives_taker2;

		      Status_b3  = "OpenLongPosition";
		      lives_maker3   = nCouldBuy - negative_buy;
		      Status_s3  = "ShortPosIncreased";
		      lives_taker3   = lives_taker2 + lives_maker3;
		      nCouldBuy3 = lives_maker3;

		    }
		  else if ( possitive_sell == negative_buy )
		    {
		      Status_s1  = "LongPosNetted";
		      lives_taker1   = 0;
		      Status_b1  = "ShortPosNetted";
		      lives_maker1   = 0;
		      nCouldBuy1 = possitive_sell;

		      Status_s2  = "OpenShortPosition";
		      lives_taker2   = nCouldBuy - possitive_sell;
		      Status_b2  = "OpenLongPosition";
		      lives_maker2   = lives_taker2;
		      nCouldBuy2 = lives_taker2;
		    }
		}
	      else if ( Status_maker == "ShortPosNettedPartly" )
		{
		  Status_s1  = "LongPosNetted";
		  lives_taker1   = 0;
		  Status_b1  = "ShortPosNettedPartly";
		  lives_maker1   = negative_buy - possitive_sell;
		  nCouldBuy1 = possitive_sell;

		  Status_s2  = "OpenShortPosition";
		  lives_taker2   = nCouldBuy - possitive_sell;
		  Status_b2  = "ShortPosNettedPartly";
		  lives_maker2   = lives_maker1 - lives_taker2;
		  nCouldBuy2 = lives_taker2;

		}
	      else if ( Status_maker == "ShortPosNetted" )
		{
		  Status_s1  = "LongPosNetted";
		  lives_taker1   = 0;
		  Status_b1  = "ShortPosNettedPartly";
		  lives_maker1   = negative_buy - possitive_sell;
		  nCouldBuy1 = possitive_sell;

		  Status_s2  = "OpenShortPosition";
		  lives_taker2   = nCouldBuy - possitive_sell;
		  Status_b2  = "ShortPosNetted";
		  lives_maker2   = 0;
		  nCouldBuy2 = lives_taker2;

		}
	      else if ( Status_maker == "OpenLongPosition" )
		{
		  Status_s1  = "LongPosNetted";
		  lives_taker1   = 0;
		  Status_b1  = "OpenLongPosition";
		  lives_maker1   = possitive_sell;
		  nCouldBuy1 = possitive_sell;

		  Status_s2  = "OpenShortPosition";
		  lives_taker2   = nCouldBuy - possitive_sell;
		  Status_b2  = "LongPosIncreased";
		  lives_maker2   = lives_maker1 + lives_taker2;
		  nCouldBuy2 = lives_taker2;

		}
	      else if ( Status_maker == "LongPosIncreased" )
		{
		  Status_s1  = "LongPosNetted";
		  lives_taker1   = 0;
		  Status_b1  = "LongPosIncreased";
		  lives_maker1   = possitive_buy + possitive_sell;
		  nCouldBuy1 = possitive_sell;

		  Status_s2  = "OpenShortPosition";
		  lives_taker2   = nCouldBuy - possitive_sell;
		  Status_b2  = "LongPosIncreased";
		  lives_maker2   = lives_maker1 + lives_taker2;
		  nCouldBuy2 = lives_taker2;
		}
	    }
	  // Checked
	}
      else
	{
	  // If taker Buy and Open Long by Short Netted: status_bj -> taker
	  if ( Status_taker == "OpenLongPosByShortPosNetted" )
	    {
	      if ( Status_maker == "OpenShortPosByLongPosNetted" )
		{
		  if ( negative_buy < possitive_sell )
		    {
		      Status_b1  = "ShortPosNetted";
		      lives_taker1   = 0;
		      Status_s1  = "LongPosNettedPartly";
		      lives_maker1   = possitive_sell - negative_buy;
		      nCouldBuy1 = negative_buy;

		      Status_b2  = "OpenLongPosition";
		      lives_taker2   = lives_maker1;
		      Status_s2  = "LongPosNetted";
		      lives_maker2   = 0;
		      nCouldBuy2 = lives_maker1;

		      Status_b3  = "LongPosIncreased";
		      lives_taker3   = lives_taker2 + nCouldBuy - possitive_sell;
		      Status_s3  = "OpenShortPosition";
		      lives_maker3   = nCouldBuy - possitive_sell;
		      nCouldBuy3 = lives_maker3;

		    }
		  else if ( negative_buy > possitive_sell )
		    {
		      Status_b1  = "ShortPosNettedPartly";
		      lives_taker1   = negative_buy - possitive_sell;
		      Status_s1  = "LongPosNetted";
		      lives_maker1   = 0;
		      nCouldBuy1 = lives_taker1;

		      Status_b2  = "ShortPosNetted";
		      lives_taker2   = 0;
		      Status_s2  = "OpenShortPosition";
		      lives_maker2   = negative_buy - possitive_sell;
		      nCouldBuy2 = negative_buy - possitive_sell;

		      Status_b3  = "OpenLongPosition";
		      lives_taker3   = nCouldBuy - negative_buy;
		      Status_s3  = "ShortPosIncreased";
		      lives_maker3   = lives_maker2 + lives_taker3;
		      nCouldBuy3 = lives_taker3;

		    }
		  else if ( negative_buy == possitive_sell )
		    {
		      Status_b1  = "ShortPosNetted";
		      lives_taker1   = 0;
		      Status_s1  = "LongPosNetted";
		      lives_maker1   = 0;
		      nCouldBuy1 = possitive_sell;

		      Status_b2  = "OpenLongPosition";
		      lives_taker2   = nCouldBuy - possitive_sell;
		      Status_s2  = "OpenShortPosition";
		      lives_maker2   = lives_taker2;
		      nCouldBuy2 = lives_taker2;
		    }
		}
	      else if ( Status_maker == "LongPosNettedPartly" )
		{
		  Status_b1  = "ShortPosNetted";
		  lives_taker1   = 0;
		  Status_s1  = "LongPosNettedPartly";
		  lives_maker1   = possitive_sell - negative_buy;
		  nCouldBuy1 = negative_buy;

		  Status_b2  = "OpenLongPosition";
		  lives_taker2   = nCouldBuy - negative_buy;
		  Status_s2  = "LongPosNettedPartly";
		  lives_maker2   = lives_maker1 - lives_taker2;
		  nCouldBuy2 = lives_taker2;

		}
	      else if ( Status_maker == "LongPosNetted" )
		{
		  Status_b1  = "ShortPosNetted";
		  lives_taker1   = 0;
		  Status_s1  = "LongPosNettedPartly";
		  lives_maker1   = possitive_sell - negative_buy;
		  nCouldBuy1 = negative_buy;

		  Status_b2  = "OpenLongPosition";
		  lives_taker2   = nCouldBuy - negative_buy;
		  Status_s2  = "LongPosNetted";
		  lives_maker2   = 0;
		  nCouldBuy2 = lives_taker2;

		}
	      else if ( Status_maker == "OpenShortPosition" )
		{
		  Status_b1  = "ShortPosNetted";
		  lives_taker1   = 0;
		  Status_s1  = "OpenShortPosition";
		  lives_maker1   = negative_buy;
		  nCouldBuy1 = negative_buy;

		  Status_b2  = "OpenLongPosition";
		  lives_taker2   = nCouldBuy - negative_buy;
		  Status_s2  = "ShortPosIncreased";
		  lives_maker2   = lives_maker1 + lives_taker2;
		  nCouldBuy2 = lives_taker2;

		}
	      else if ( Status_maker == "ShortPosIncreased" )
		{
		  Status_b1  = "ShortPosNetted";
		  lives_taker1   = 0;
		  Status_s1  = "ShortPosIncreased";
		  lives_maker1   = negative_sell + negative_buy;
		  nCouldBuy1 = negative_buy;

		  Status_b2  = "OpenLongPosition";
		  lives_taker2   = nCouldBuy - negative_buy;
		  Status_s2  = "ShortPosIncreased";
		  lives_maker2   = lives_maker1 + lives_taker2;
		  nCouldBuy2 = lives_taker2;
		}
	    }
	  // Checked
	}
      /********************************************************************/
      std::string Status_maker0="EmptyStr", Status_maker1="EmptyStr", Status_maker2="EmptyStr", Status_maker3="EmptyStr";
      std::string Status_taker0="EmptyStr", Status_taker1="EmptyStr", Status_taker2="EmptyStr", Status_taker3="EmptyStr";

      std::vector<std::string> v_status;
      std::vector<int64_t> v_livesc;
      std::vector<int64_t> v_ncouldbuy;

      v_ncouldbuy.push_back(nCouldBuy0);
      v_ncouldbuy.push_back(nCouldBuy1);
      v_ncouldbuy.push_back(nCouldBuy2);
      v_ncouldbuy.push_back(nCouldBuy3);

      v_livesc.push_back(lives_maker0);
      v_livesc.push_back(lives_taker0);
      v_livesc.push_back(lives_maker1);
      v_livesc.push_back(lives_taker1);
      v_livesc.push_back(lives_maker2);
      v_livesc.push_back(lives_taker2);
      v_livesc.push_back(lives_maker3);
      v_livesc.push_back(lives_taker3);

      if ( pold->getAddr() == seller_address )
	{
	  v_status.push_back(Status_s);
	  v_status.push_back(Status_b);
	  v_status.push_back(Status_s1);
	  v_status.push_back(Status_b1);
	  v_status.push_back(Status_s2);
	  v_status.push_back(Status_b2);
	  v_status.push_back(Status_s3);
	  v_status.push_back(Status_b3);

	}
      else
	{
	  v_status.push_back(Status_b);
	  v_status.push_back(Status_s);
	  v_status.push_back(Status_b1);
	  v_status.push_back(Status_s1);
	  v_status.push_back(Status_b2);
	  v_status.push_back(Status_s2);
	  v_status.push_back(Status_b3);
	  v_status.push_back(Status_s3);
	}

      Status_maker0 = Status_maker;
      Status_taker0 = Status_taker;

      if ( pold->getAddr() == seller_address )
	{
	  Status_maker1 = Status_s1;
	  Status_taker1 = Status_b1;
	  Status_maker2 = Status_s2;
	  Status_taker2 = Status_b2;
	  Status_maker3 = Status_s3;
	  Status_taker3 = Status_b3;
	}
      else
	{
	  Status_maker1 = Status_b1;
	  Status_taker1 = Status_s1;
	  Status_maker2 = Status_b2;
	  Status_taker2 = Status_s2;
	  Status_maker3 = Status_b3;
	  Status_taker3 = Status_s3;
	}

  /**
   * Fees calculations for maker and taker.
   *
   *
   */
    // NOTE: Check this function later (got issues!)
    mastercore::ContractDex_Fees(pnew->getAddr(),pold->getAddr(), nCouldBuy, property_traded);

    if(msc_debug_x_trade_bidirectional)
    {
        PrintToLog("Checking all parameters inside recordMatchedTrade:\n");
        PrintToLog("txmaker: %s, txtaker: %s, makeraddress: %s, takeraddress: %s, price: %d, maker_crgafs: %d\n", pold->getHash().ToString(), pnew->getHash().ToString(), pold->getAddr(), pnew->getAddr(), pold->getEffectivePrice(),contract_replacement.getAmountForSale());
        PrintToLog("takergetAmounForSale: %d, makerblock: %d, takerblock: %d, property: %d, tradestatus: %s\n", pnew->getAmountForSale(), pold->getBlock(), pnew->getBlock(), property_traded, tradeStatus);
        PrintToLog("lives_maker0: %d, lives_maker1: %d, lives_maker2: %d, lives_maker3: %d, lives_taker0: %d, lives_taker1: %d, lives_taker2: %d, lives_taker3:%d\n",lives_maker0, lives_maker1, lives_maker2, lives_maker3, lives_taker0, lives_taker1, lives_taker2, lives_taker3);
        PrintToLog("Status_maker0: %d, Status_taker0: %d, Status_maker1: %d, Status_taker1: %d, Status_maker2: %d, Status_taker2: %d, Status_maker3: %d, Status_taker3:%d\n",Status_maker0, Status_taker0, Status_maker1, Status_taker1, Status_maker2, Status_taker2, Status_maker3, Status_taker3);
        PrintToLog("nCouldBuy0: %d, nCouldBuy1: %d, nCouldBuy2: %d, nCouldBuy3: %d, amountpnew: %d, amountpold: %d\n",nCouldBuy0, nCouldBuy1, nCouldBuy2, nCouldBuy3, amountpnew, amountpold);
    }
   /********************************************************/
   t_tradelistdb->recordMatchedTrade(pold->getHash(),
					pnew->getHash(),
					pold->getAddr(),
					pnew->getAddr(),
					pold->getEffectivePrice(),
					contract_replacement.getAmountForSale(),
					pnew->getAmountForSale(),
					pold->getBlock(),
					pnew->getBlock(),
					property_traded,
					tradeStatus,
					lives_maker0,
					lives_maker1,
					lives_maker2,
					lives_maker3,
					lives_taker0,
					lives_taker1,
					lives_taker2,
					lives_taker3,
					Status_maker0,
					Status_taker0,
					Status_maker1,
					Status_taker1,
					Status_maker2,
					Status_taker2,
					Status_maker3,
					Status_taker3,
					nCouldBuy0,
					nCouldBuy1,
					nCouldBuy2,
					nCouldBuy3,
					amountpnew,
					amountpold);
          /********************************************************/
          int index = static_cast<unsigned int>(property_traded);

          marketP[index] = pold->getEffectivePrice();
          // uint64_t marketPriceNow = marketP[index];
          if(msc_debug_x_trade_bidirectional) PrintToLog("%s: marketP[index] = %d\n",__func__, marketP[index]);
          // t_tradelistdb->recordForUPNL(pnew->getHash(),pnew->getAddr(),property_traded,pold->getEffectivePrice());

          if(msc_debug_x_trade_bidirectional) PrintToLog("++ erased old: %s\n", offerIt->ToString());
          pofferSet->erase(offerIt++);

          if (0 < remaining)
	            pofferSet->insert(contract_replacement);
      }
}

static const std::string getTradeReturnType(MatchReturnType ret)
{
  switch (ret)
    {
    case NOTHING: return "NOTHING";
    case TRADED: return "TRADED";
    case TRADED_MOREINSELLER: return "TRADED_MOREINSELLER";
    case TRADED_MOREINBUYER: return "TRADED_MOREINBUYER";
    case ADDED: return "ADDED";
    case CANCELLED: return "CANCELLED";
    default: return "* unknown *";
    }
}

bool mastercore::ContractDex_Fees(std::string addressTaker,std::string addressMaker, int64_t nCouldBuy,uint32_t contractId)
{
    int64_t takerFee, makerFee, cacheFee;

    CMPSPInfo::Entry sp;
    if (!_my_sps->getSP(contractId, sp))
       return false;

    int64_t marginRe = static_cast<int64_t>(sp.margin_requirement);

    if (msc_debug_contractdex_fees) PrintToLog("%s: addressTaker: %d, addressMaker: %d, nCouldBuy: %d, contractIds: %d\n",__func__,addressTaker, addressMaker, nCouldBuy, contractId);


    if (sp.prop_type == ALL_PROPERTY_TYPE_ORACLE_CONTRACT)
    {
        arith_uint256 uTakerFee = (ConvertTo256(nCouldBuy) * ConvertTo256(marginRe) * ConvertTo256(25)) / (ConvertTo256(1000) * ConvertTo256(COIN) *ConvertTo256(BASISPOINT));  // 2.5% basis point
        arith_uint256 uMakerFee = (ConvertTo256(nCouldBuy) * ConvertTo256(marginRe)) / (ConvertTo256(100) * ConvertTo256(COIN) * ConvertTo256(BASISPOINT));  // 1% basis point

        cacheFee = ConvertTo64(uMakerFee / ConvertTo256(2));
        takerFee = ConvertTo64(uTakerFee);
        makerFee = ConvertTo64(uMakerFee);

        if (msc_debug_contractdex_fees) PrintToLog("%s: oracles cacheFee: %d, oracles takerFee: %d, oracles makerFee: %d\n",__func__,cacheFee, takerFee, makerFee);

        // 0.5% basis point to oracle maintaineer
        update_tally_map(sp.issuer,sp.collateral_currency, cacheFee,BALANCE);

        if (sp.collateral_currency == 4) //ALLS
        {
          // 0.5% basis point to feecache
          cachefees[sp.collateral_currency] += cacheFee;

        }else {
            // Create the metadex object with specific params

            // uint256 txid;
            // int block = 0;
            // unsigned int idx = 0;

            // CMPSPInfo::Entry spp;
            // _my_sps->getSP(sp.collateral_currency, spp);
            // if (msc_debug_contractdex_fees) PrintToLog("name of collateral currency:%s \n",spp.name);
            //
            // int64_t VWAPMetaDExPrice = mastercore::getVWAPPriceByPair(spp.name, "ALL");
            // if (msc_debug_contractdex_fees) PrintToLog("\nVWAPMetaDExPrice = %s\n", FormatDivisibleMP(VWAPMetaDExPrice));
            // int64_t amount_desired = 1; // we need to ajust this to market prices
            // CMPMetaDEx new_mdex(addressTaker, block, sp.collateral_currency, cacheFee, 1, amount_desired, txid, idx, CMPTransaction::ADD);
            // mastercore::MetaDEx_INSERT(new_mdex);

        }

    } else {      //natives

          arith_uint256 uTakerFee = (ConvertTo256(nCouldBuy) * ConvertTo256(marginRe)) / (ConvertTo256(100) * ConvertTo256(COIN) * ConvertTo256(BASISPOINT));
          arith_uint256 uMakerFee = (ConvertTo256(nCouldBuy) * ConvertTo256(marginRe) * ConvertTo256(5)) / (ConvertTo256(1000) * ConvertTo256(COIN) * ConvertTo256(BASISPOINT));

          takerFee = ConvertTo64(uTakerFee);
          makerFee = ConvertTo64(uMakerFee);

          if (msc_debug_contractdex_fees) PrintToLog("%s: natives takerFee: %d, natives makerFee: %d\n",__func__,takerFee, makerFee);

    }

    // -% to taker, +% to maker
    update_tally_map(addressTaker,sp.collateral_currency,-takerFee,BALANCE);
    update_tally_map(addressMaker,sp.collateral_currency, makerFee,BALANCE);

    //sum check
    assert(takerFee == makerFee + 3*cacheFee); // 2.5% = 1% + 3*0.5%

    return true;

}

bool mastercore::MetaDEx_Fees(const CMPMetaDEx *pnew,const CMPMetaDEx *pold, int64_t buyer_amountGot)
{
    if(pnew->getBlock() == 0 && pnew->getHash() == uint256() && pnew->getIdx() == 0) {
       if(msc_debug_metadex_fees) PrintToLog("%s: Buy from  ContractDex_Fees \n",__func__);
       return false;
    }

    arith_uint256 uTakerFee = (ConvertTo256(buyer_amountGot) * ConvertTo256(5)) / (ConvertTo256(100) * ConvertTo256(BASISPOINT));
    arith_uint256 uMakerFee = (ConvertTo256(buyer_amountGot) * ConvertTo256(4)) / (ConvertTo256(100) * ConvertTo256(BASISPOINT));
    arith_uint256 uCacheFee = ConvertTo256(buyer_amountGot) / (ConvertTo256(100) * ConvertTo256(BASISPOINT));

    int64_t takerFee = ConvertTo64(uTakerFee);
    int64_t makerFee = ConvertTo64(uMakerFee);
    int64_t cacheFee = ConvertTo64(uCacheFee);

    if(msc_debug_metadex_fees) PrintToLog("%s: buyer_amountGot: %d, cacheFee: %d, takerFee: %d, makerFee: %d\n",__func__, buyer_amountGot, cacheFee, takerFee, makerFee);
    //sum check
    assert(takerFee == makerFee + cacheFee);

    // checking the same property for both
    assert(pnew->getDesProperty() == pold->getProperty());
    if(msc_debug_metadex_fees) PrintToLog("%s: propertyId: %d\n",__func__,pold->getProperty());

    // -% to taker, +% to maker
    update_tally_map(pnew->getAddr(), pnew->getDesProperty(), -takerFee,BALANCE);
    update_tally_map(pold->getAddr(), pold->getProperty(), makerFee,BALANCE);


    // to feecache
    cachefees[pnew->getProperty()] += cacheFee;

    return true;

}

// Used by rangeInt64, xToInt64
static bool rangeInt64(const int128_t& value)
{
  return (std::numeric_limits<int64_t>::min() <= value && value <= std::numeric_limits<int64_t>::max());
}

// Used by xToString
static bool rangeInt64(const rational_t& value)
{
  return (rangeInt64(value.numerator()) && rangeInt64(value.denominator()));
}

// Used by CMPMetaDEx::displayUnitPrice
static int64_t xToRoundUpInt64(const rational_t& value)
{
  // for integer rounding up: ceil(num / denom) => 1 + (num - 1) / denom
  int128_t result = int128_t(1) + (value.numerator() - int128_t(1)) / value.denominator();

  assert(rangeInt64(result));

  return result.convert_to<int64_t>();
}

std::string xToString(const dec_float& value)
{
  return value.str(DISPLAY_PRECISION_LEN, std::ios_base::fixed);
}

std::string xToString(const int128_t& value)
{
  return strprintf("%s", boost::lexical_cast<std::string>(value));
}

std::string xToString(const rational_t& value)
{
  if (rangeInt64(value))
    {
      int64_t num = value.numerator().convert_to<int64_t>();
      int64_t denom = value.denominator().convert_to<int64_t>();
      dec_float x = dec_float(num) / dec_float(denom);
      return xToString(x);
    }
  else
    return strprintf("%s / %s", xToString(value.numerator()), xToString(value.denominator()));
}

std::string xToString(const uint64_t &price)
{
  return strprintf("%s", boost::lexical_cast<std::string>(price));
}

std::string xToString(const int64_t &price)
{
  return strprintf("%s", boost::lexical_cast<std::string>(price));
}

std::string xToString(const uint32_t &value)
{
  return strprintf("%s", boost::lexical_cast<std::string>(value));
}

ui128 multiply_int64_t(int64_t &m, int64_t &n)
{
  ui128 product;
  multiply(product, m, n);
  return product;
}

ui128 multiply_uint64_t(uint64_t &m, uint64_t &n)
{
  ui128 product;
  multiply(product, m, n);
  return product;
}

/*The metadex of tokens*/

// find the best match on the market
// NOTE: sometimes I refer to the older order as seller & the newer order as buyer, in this trade
// INPUT: property, desprop, desprice = of the new order being inserted; the new object being processed
// RETURN:

MatchReturnType x_Trade(CMPMetaDEx* const pnew)
{
  const uint32_t propertyForSale = pnew->getProperty();
  const uint32_t propertyDesired = pnew->getDesProperty();
  MatchReturnType NewReturn = NOTHING;
  bool bBuyerSatisfied = false;
  extern int volumeToVWAP;

  if (msc_debug_metadex1) PrintToLog("%s(%s: prop=%d, desprop=%d, desprice= %s);newo: %s\n",
      __FUNCTION__, pnew->getAddr(), propertyForSale, propertyDesired, xToString(pnew->inversePrice()), pnew->ToString());

  md_PricesMap* const ppriceMap = get_Prices(propertyDesired);

  // Nothing for the desired property exists in the market !!
  if (!ppriceMap) {
    PrintToLog("%s()=%d:%s FOUND ON THE MARKET\n", __FUNCTION__, NewReturn, getTradeReturnType(NewReturn));
    return NewReturn;
  }

  // Within the desired property map (given one property) iterate over the items looking at prices
  for (md_PricesMap::iterator priceIt = ppriceMap->begin(); priceIt != ppriceMap->end(); ++priceIt) { // check all prices

    const rational_t sellersPrice = priceIt->first;

    if (msc_debug_metadex2) PrintToLog("comparing prices: desprice %s needs to be GREATER THAN OR EQUAL TO %s\n",
        xToString(pnew->inversePrice()), xToString(sellersPrice));

    // Is the desired price check satisfied? The buyer's inverse price must be larger than that of the seller.
    if (pnew->inversePrice() < sellersPrice) {
      continue;
    }

    md_Set* const pofferSet = &(priceIt->second);

    // at good (single) price level and property iterate over offers looking at all parameters to find the match
    md_Set::iterator offerIt = pofferSet->begin();
    while (offerIt != pofferSet->end())
      {
	// specific price, check all properties
	const CMPMetaDEx* const pold = &(*offerIt);
	assert(pold->unitPrice() == sellersPrice);

	if (msc_debug_metadex1) PrintToLog("Looking at existing: %s (its prop= %d, its des prop= %d) = %s\n",
	    xToString(sellersPrice), pold->getProperty(), pold->getDesProperty(), pold->ToString());

	// does the desired property match?
	if (pold->getDesProperty() != propertyForSale) {
	  ++offerIt;
	  continue;
	}

	if (msc_debug_metadex1) PrintToLog("MATCH FOUND, Trade: %s = %s\n", xToString(sellersPrice), pold->ToString());

	// match found, execute trade now!
	const int64_t seller_amountForSale = pold->getAmountRemaining();
	const int64_t buyer_amountOffered = pnew->getAmountRemaining();

	if (msc_debug_metadex1) PrintToLog("$$ trading using price: %s; seller: forsale=%d, desired=%d, remaining=%d, buyer amount offered=%d\n",
	    xToString(sellersPrice), pold->getAmountForSale(), pold->getAmountDesired(), pold->getAmountRemaining(), pnew->getAmountRemaining());
	if (msc_debug_metadex1) PrintToLog("$$ old: %s\n", pold->ToString());
	if (msc_debug_metadex1) PrintToLog("$$ new: %s\n", pnew->ToString());

	///////////////////////////

	// preconditions
	assert(0 < pold->getAmountRemaining());
	assert(0 < pnew->getAmountRemaining());
	assert(pnew->getProperty() != pnew->getDesProperty());
	assert(pnew->getProperty() == pold->getDesProperty());
	assert(pold->getProperty() == pnew->getDesProperty());
	assert(pold->unitPrice() <= pnew->inversePrice());
	assert(pnew->unitPrice() <= pold->inversePrice());

	//globalNumPrice = pold->getAmountDesired();
	//globalDenPrice = pold->getAmountForSale();
	globalNumPrice = 1;
	globalDenPrice = 1;
	/*Lets gonna take the pnew->unitPrice() as the ALL unit price*/
	/*unitPrice = 1 ALL on dUSD*/
	///////////////////////////

	// First determine how many representable (indivisible) tokens Alice can
	// purchase from Bob, using Bob's unit price
	// This implies rounding down, since rounding up is impossible, and would
	// require more tokens than Alice has
	arith_uint256 iCouldBuy = (ConvertTo256(pnew->getAmountRemaining()) * ConvertTo256(pold->getAmountForSale())) / ConvertTo256(pold->getAmountDesired());

	int64_t nCouldBuy = 0;
	if (iCouldBuy < ConvertTo256(pold->getAmountRemaining())) {
	  nCouldBuy = ConvertTo64(iCouldBuy);
	} else {
	  nCouldBuy = pold->getAmountRemaining();
	}

	if (nCouldBuy == 0) {
	  if (msc_debug_metadex1) PrintToLog(
	          "-- buyer has not enough tokens for sale to purchase one unit!\n");
	  ++offerIt;
	  continue;
	}

	// If the amount Alice would have to pay to buy Bob's tokens at his price
	// is fractional, always round UP the amount Alice has to pay
	// This will always be better for Bob. Rounding in the other direction
	// will always be impossible, because ot would violate Bob's accepted price
	arith_uint256 iWouldPay = DivideAndRoundUp((ConvertTo256(nCouldBuy) * ConvertTo256(pold->getAmountDesired())), ConvertTo256(pold->getAmountForSale()));
	int64_t nWouldPay = ConvertTo64(iWouldPay);

	// If the resulting adjusted unit price is higher than Alice' price, the
	// orders shall not execute, and no representable fill is made
	const rational_t xEffectivePrice(nWouldPay, nCouldBuy);

	if (xEffectivePrice > pnew->inversePrice())
	  {
	    if (msc_debug_metadex1) PrintToLog(
	            "-- effective price is too expensive: %s\n", xToString(xEffectivePrice));
	    ++offerIt;
	    continue;
	  }

	const int64_t buyer_amountGot = nCouldBuy;
	const int64_t seller_amountGot = nWouldPay;
	const int64_t buyer_amountLeft = pnew->getAmountRemaining() - seller_amountGot;
	const int64_t seller_amountLeft = pold->getAmountRemaining() - buyer_amountGot;

	if (msc_debug_metadex1) PrintToLog("$$ buyer_got= %d, seller_got= %d, seller_left_for_sale= %d, buyer_still_for_sale= %d\n",
	    buyer_amountGot, seller_amountGot, seller_amountLeft, buyer_amountLeft);


	// postconditions
	assert(xEffectivePrice >= pold->unitPrice());
	assert(xEffectivePrice <= pnew->inversePrice());
	assert(0 <= seller_amountLeft);
	assert(0 <= buyer_amountLeft);
	assert(seller_amountForSale == seller_amountLeft + buyer_amountGot);
	assert(buyer_amountOffered == buyer_amountLeft + seller_amountGot);

	/***********************************************************************************************/
	/** Finding Market Prices TOKEN1/TOKEN2 or TOKEN2/TOKEN1 **/
  if(msc_debug_metadex2)
  {
	    PrintToLog("\n********************************************************************************\n");
	    PrintToLog("\npnew->getProperty() = %d,\t pnew->getDesProperty() = %d\n", pnew->getProperty(), pnew->getDesProperty());
  }

	int64_t pnew_forsale = pnew->getAmountForSale();
	int64_t pnew_desired = pnew->getAmountDesired();

	int64_t pold_forsale = pold->getAmountForSale();
	int64_t pold_desired = pold->getAmountDesired();

	rational_t market_priceratToken1_Token2(pold_desired, pold_forsale);
	int64_t market_priceToken1_Token2 = mastercore::RationalToInt64(market_priceratToken1_Token2);
	market_priceMap[pold->getProperty()][pold->getDesProperty()] = market_priceToken1_Token2;

	rational_t market_priceratToken2_Token1(pnew_desired, pnew_forsale);
	int64_t market_priceToken2_Token1 = mastercore::RationalToInt64(market_priceratToken2_Token1);
	market_priceMap[pnew->getProperty()][pnew->getDesProperty()] = market_priceToken2_Token1;

  if(msc_debug_metadex2)
  {
	    PrintToLog("\npold_forsale = %s,\t pold_desired = %s\n",
		       FormatDivisibleMP(pold_forsale), FormatDivisibleMP(pold_desired));
	    PrintToLog("\npnew_forsale = %s,\t pnew_desired = %s\n",
		       FormatDivisibleMP(pnew_forsale), FormatDivisibleMP(pnew_desired));
  }

	/*************************************************************************************************/
	/** Finding VWAP Price **/

  if(msc_debug_metadex2)
  {
	     PrintToLog("\n********************************************************************************\n");
	     PrintToLog("\nbuyer_amountGot = %s,\t seller_amountGot = %s\n", FormatDivisibleMP(buyer_amountGot),
		        FormatDivisibleMP(seller_amountGot));
  }

	arith_uint256 numVWAPMapToken1_Token2_256t =
	  mastercore::ConvertTo256(market_priceToken1_Token2)*mastercore::ConvertTo256(buyer_amountGot)/COIN;
	int64_t numVWAPMapToken1_Token2_64t = mastercore::ConvertTo64(numVWAPMapToken1_Token2_256t);

	arith_uint256 numVWAPMapToken2_Token1_256t =
	  mastercore::ConvertTo256(market_priceToken2_Token1)*mastercore::ConvertTo256(seller_amountGot)/COIN;
	int64_t numVWAPMapToken2_Token1_64t = mastercore::ConvertTo64(numVWAPMapToken2_Token1_256t);

	numVWAPVector[pold->getProperty()][pold->getDesProperty()].push_back(numVWAPMapToken1_Token2_64t);
	denVWAPVector[pold->getProperty()][pold->getDesProperty()].push_back(buyer_amountGot);

	numVWAPVector[pnew->getProperty()][pnew->getDesProperty()].push_back(numVWAPMapToken2_Token1_64t);
	denVWAPVector[pnew->getProperty()][pnew->getDesProperty()].push_back(seller_amountGot);

	if(msc_debug_metadex2)
  {
      PrintToLog("numVWAPVector[pold->getProperty()][pold->getDesProperty()].size() = %d",
		       numVWAPVector[pold->getProperty()][pold->getDesProperty()].size());
	    PrintToLog("numVWAPVector[pnew->getProperty()][pnew->getDesProperty()].size() = %d",
		       numVWAPVector[pnew->getProperty()][pnew->getDesProperty()].size());
  }

	std::vector<int64_t> numVWAPpoldv(numVWAPVector[pold->getProperty()][pold->getDesProperty()].end()-
					  std::min(int(numVWAPVector[pold->getProperty()][pold->getDesProperty()].size()),
						   volumeToVWAP),
					  numVWAPVector[pold->getProperty()][pold->getDesProperty()].end());
	std::vector<int64_t> denVWAPpoldv(denVWAPVector[pold->getProperty()][pold->getDesProperty()].end()-
					  std::min(int(denVWAPVector[pold->getProperty()][pold->getDesProperty()].size()),
						   volumeToVWAP),
					  denVWAPVector[pold->getProperty()][pold->getDesProperty()].end());

	std::vector<int64_t> numVWAPpnewv(numVWAPVector[pnew->getProperty()][pnew->getDesProperty()].end()-
					  std::min(int(numVWAPVector[pnew->getProperty()][pnew->getDesProperty()].size()),
						   volumeToVWAP),
					  numVWAPVector[pnew->getProperty()][pnew->getDesProperty()].end());
	std::vector<int64_t> denVWAPpnewv(denVWAPVector[pnew->getProperty()][pnew->getDesProperty()].end()-
					  std::min(int(denVWAPVector[pnew->getProperty()][pnew->getDesProperty()].size()),
						   volumeToVWAP),
					  denVWAPVector[pnew->getProperty()][pnew->getDesProperty()].end());

	int64_t numVWAPpold = 0, denVWAPpold = 0;
	int64_t numVWAPpnew = 0, denVWAPpnew = 0;

	vwap_num_den(numVWAPpoldv, denVWAPpoldv, numVWAPpold, denVWAPpold);
	vwap_num_den(numVWAPpnewv, denVWAPpnewv, numVWAPpnew, denVWAPpnew);

	rational_t vwapPriceToken1_Token2RatV(numVWAPpold, denVWAPpold);
	int64_t vwapPriceToken1_Token2Int64V = mastercore::RationalToInt64(vwapPriceToken1_Token2RatV);

	rational_t vwapPriceToken2_Token1RatV(numVWAPpnew, denVWAPpnew);
	int64_t vwapPriceToken2_Token1Int64V = mastercore::RationalToInt64(vwapPriceToken2_Token1RatV);

	VWAPMapSubVector[pold->getProperty()][pold->getDesProperty()]=vwapPriceToken1_Token2Int64V;
	VWAPMapSubVector[pnew->getProperty()][pnew->getDesProperty()]=vwapPriceToken2_Token1Int64V;

  if(msc_debug_metadex2)
  {
	    PrintToLog("\nVWAPMapSubVector[pold->getProperty()][pold->getDesProperty()] = %s\n",
		       FormatDivisibleMP(VWAPMapSubVector[pold->getProperty()][pold->getDesProperty()]));
	    PrintToLog("\nVWAPMapSubVector[pnew->getProperty()][pnew->getDesProperty()] = %s\n",
		       FormatDivisibleMP(VWAPMapSubVector[pnew->getProperty()][pnew->getDesProperty()]));
	    PrintToLog("\n********************************************************************************\n");
  }

  /***********************************************************************************************/
  // Adding volume into Map
  (pnew->getProperty() < pnew->getDesProperty()) ? MapMetaVolume[pnew->getBlock()][std::make_pair(pnew->getProperty(),pnew->getDesProperty())] = buyer_amountGot : MapMetaVolume[pnew->getBlock()][std::make_pair(pnew->getDesProperty(),pnew->getProperty())] = seller_amountGot;
  PrintToLog("%s(): Adding volume into map: Property: %d, Desproperty : %d, buyeramountgot : %d\n", __func__, pnew->getProperty(), pnew->getDesProperty(), buyer_amountGot);

	/***********************************************************************************************/
	int64_t buyer_amountGotAfterFee = buyer_amountGot;
	int64_t tradingFee = 0;

	// transfer the payment property from buyer to seller
	assert(update_tally_map(pnew->getAddr(), pnew->getProperty(), -seller_amountGot, BALANCE));
	assert(update_tally_map(pold->getAddr(), pold->getDesProperty(), seller_amountGot, BALANCE));

	// transfer the market (the one being sold) property from seller to buyer
	assert(update_tally_map(pold->getAddr(), pold->getProperty(), -buyer_amountGot, METADEX_RESERVE));
	assert(update_tally_map(pnew->getAddr(), pnew->getDesProperty(), buyer_amountGot, BALANCE));

	/**
	 * Fees calculations for maker and taker.
	 *
	 *
	 */
	mastercore::MetaDEx_Fees(pnew, pold, buyer_amountGot);

	NewReturn = TRADED;

	CMPMetaDEx seller_replacement = *pold; // < can be moved into last if block
	seller_replacement.setAmountRemaining(seller_amountLeft, "seller_replacement");

	pnew->setAmountRemaining(buyer_amountLeft, "buyer");

	if (0 < buyer_amountLeft)
	  NewReturn = TRADED_MOREINBUYER;

	if (0 == buyer_amountLeft)
	  bBuyerSatisfied = true;

	if (0 < seller_amountLeft)
	  NewReturn = TRADED_MOREINSELLER;

	if (msc_debug_metadex3) PrintToLog("==== TRADED !!! %u=%s\n", NewReturn, getTradeReturnType(NewReturn));
	// record the trade in MPTradeList
	t_tradelistdb->recordMatchedTrade(pold->getHash(), pnew->getHash(), // < might just pass pold, pnew
					  pold->getAddr(), pnew->getAddr(), pold->getDesProperty(), pnew->getDesProperty(), seller_amountGot, buyer_amountGotAfterFee, pnew->getBlock(), tradingFee);

	if (msc_debug_metadex3) PrintToLog("++ erased old: %s\n", offerIt->ToString());
	// erase the old seller element
	pofferSet->erase(offerIt++);

	// insert the updated one in place of the old
	if (0 < seller_replacement.getAmountRemaining())
	  {
	    PrintToLog("++ inserting seller_replacement: %s\n", seller_replacement.ToString());
	    pofferSet->insert(seller_replacement);
	  }

	if (bBuyerSatisfied)
	  {
	    assert(buyer_amountLeft == 0);
	    break;
	  }
      } // specific price, check all properties

    if (bBuyerSatisfied) break;
  } // check all prices

  if(msc_debug_metadex3) PrintToLog("%s()=%d:%s\n", __FUNCTION__, NewReturn, getTradeReturnType(NewReturn));

  return NewReturn;
}



///////////////////////////////////////
/** New things for Contracts */
MatchReturnType x_Trade(CMPContractDex* const pnew)
{
  const uint32_t propertyForSale = pnew->getProperty();
  uint8_t trdAction = pnew->getTradingAction();
  MatchReturnType NewReturn = NOTHING;

  cd_PricesMap* const ppriceMap = get_PricesCd(propertyForSale);

  if (!ppriceMap)
    {
      PrintToLog("%s()=%d:%s NOT FOUND ON THE MARKET\n", __FUNCTION__, NewReturn, getTradeReturnType(NewReturn));
      return NewReturn;
    }

  LoopBiDirectional(ppriceMap, trdAction, NewReturn, pnew, propertyForSale);

  return NewReturn;
}

int64_t mastercore::getVWAPPriceByPair(std::string num, std::string den)
{
  LOCK(cs_tally);
  uint32_t nextSPID = _my_sps->peekNextSPID();

  uint32_t numId = 0;
  uint32_t denId = 0;

  for (uint32_t propertyId = 1; propertyId < nextSPID; propertyId++)
    {
      CMPSPInfo::Entry sp;
      if (_my_sps->getSP(propertyId, sp))
	{
	  if ( sp.name == num )
	    {
	      PrintToLog("\npropertyId num: %d\t sp.name = %s\n", propertyId, sp.name);
	      numId = propertyId;
	    }
	  if ( sp.name == den )
	    {
	      PrintToLog("\npropertyId den: %d\t sp.name = %s\n", propertyId, sp.name);
	      denId = propertyId;
	    }
	}
    }
  PrintToLog("\nVWAPMapSubVector[nameId][denId] = %d\n", FormatDivisibleMP(VWAPMapSubVector[numId][denId]));
  return VWAPMapSubVector[numId][denId];
}

int64_t mastercore::getVWAPPriceContracts(std::string namec)
{
  LOCK(cs_tally);
  uint32_t nextSPID = _my_sps->peekNextSPID();

  uint32_t nameId = 0;
  for (uint32_t propertyId = 1; propertyId < nextSPID; propertyId++)
    {
      CMPSPInfo::Entry sp;
      if (_my_sps->getSP(propertyId, sp))
	{
	  if ( sp.name == namec )
	    {
	      PrintToLog("\npropertyId num: %d\n", propertyId);
	      nameId = propertyId;
	    }
	}
    }
  PrintToLog("\nVWAPMapContracts[nameId] = %d\n", FormatDivisibleMP(VWAPMapContracts[nameId]));
  return VWAPMapContracts[nameId];
}

/**
 * Used for display of unit prices to 8 decimal places at UI layer.
 *
 * Automatically returns unit or inverse price as needed.
 */
std::string CMPMetaDEx::displayUnitPrice() const
{
     rational_t tmpDisplayPrice;
     if (getDesProperty() == TL_PROPERTY_ALL || getDesProperty() == TL_PROPERTY_TALL) {
         tmpDisplayPrice = unitPrice();
         if (isPropertyDivisible(getProperty())) tmpDisplayPrice = tmpDisplayPrice * COIN;
     } else {
         tmpDisplayPrice = inversePrice();
         if (isPropertyDivisible(getDesProperty())) tmpDisplayPrice = tmpDisplayPrice * COIN;
     }

     // offers with unit prices under 0.00000001 will be excluded from UI layer - TODO: find a better way to identify sub 0.00000001 prices
     std::string tmpDisplayPriceStr = xToString(tmpDisplayPrice);
     if (!tmpDisplayPriceStr.empty()) { if (tmpDisplayPriceStr.substr(0,1) == "0") return "0.00000000"; }

     // we must always round up here - for example if the actual price required is 0.3333333344444
     // round: 0.33333333 - price is insufficient and thus won't result in a trade
     // round: 0.33333334 - price will be sufficient to result in a trade
     std::string displayValue = FormatDivisibleMP(xToRoundUpInt64(tmpDisplayPrice));
     return displayValue;
}

std::string CMPMetaDEx::displayFullUnitPrice() const
{
  rational_t tempUnitPrice = unitPrice();

  /* Matching types require no action (divisible/divisible or indivisible/indivisible)
     Non-matching types require adjustment for display purposes
     divisible/indivisible   : *COIN
     indivisible/divisible   : /COIN
  */
  if ( isPropertyDivisible(getProperty()) && !isPropertyDivisible(getDesProperty()) ) tempUnitPrice = tempUnitPrice*COIN;
  if ( !isPropertyDivisible(getProperty()) && isPropertyDivisible(getDesProperty()) ) tempUnitPrice = tempUnitPrice/COIN;

  std::string unitPriceStr = xToString(tempUnitPrice);
  return unitPriceStr;
}

std::string CMPContractDex::displayFullContractPrice() const
{
    uint64_t priceForsale = getEffectivePrice();
    uint64_t amountForsale = getAmountForSale();

    int128_t fullprice;
    if ( isPropertyContract(getProperty()) ) multiply(fullprice, priceForsale, amountForsale);

    std::string priceForsaleStr = xToString(fullprice);
    return priceForsaleStr;
}

rational_t CMPMetaDEx::unitPrice() const
{
  rational_t effectivePrice;
  if (amount_forsale) effectivePrice = rational_t(amount_desired, amount_forsale);
  return effectivePrice;
}

rational_t CMPMetaDEx::inversePrice() const
{
  rational_t inversePrice;
  if (amount_desired) inversePrice = rational_t(amount_forsale, amount_desired);
  return inversePrice;
}


int64_t CMPMetaDEx::getAmountToFill() const
{
    // round up to ensure that the amount we present will actually result in buying all available tokens
    arith_uint256 iAmountNeededToFill = DivideAndRoundUp((ConvertTo256(amount_remaining) * ConvertTo256(amount_desired)), ConvertTo256(amount_forsale));
    int64_t nAmountNeededToFill = ConvertTo64(iAmountNeededToFill);
    return nAmountNeededToFill;
}

int64_t CMPMetaDEx::getBlockTime() const
{
    CBlockIndex* pblockindex = chainActive[block];
    return pblockindex->GetBlockTime();
}

void CMPMetaDEx::setAmountRemaining(int64_t amount, const std::string& label)
{
    amount_remaining = amount;
    PrintToLog("update remaining amount still up for sale (%ld %s):%s\n", amount, label, ToString());
}


///////////////////////////////////////////
void CMPMetaDEx::setAmountForsale(int64_t amount, const std::string& label)
{
    amount_forsale = amount;
    PrintToLog("update remaining amount still up for sale (%ld %s):%s\n", amount, label, ToString());
}

void CMPContractDex::setPrice(int64_t price)
{
    effective_price = price;
    // PrintToLog("update price still up for sale (%ld):%s\n", price, ToString());
}
///////////////////////////////////////////

std::string CMPMetaDEx::ToString() const
{
    return strprintf("%s:%34s in %d/%03u, txid: %s , trade #%u %s for #%u %s",
        xToString(unitPrice()), addr, block, idx, txid.ToString().substr(0, 10),
        property, FormatMP(property, amount_forsale), desired_property, FormatMP(desired_property, amount_desired));
}

std::string CMPContractDex::ToString() const
{
    return strprintf("%s:%34s in %d/%03u, txid: %s , trade #%u %s for #%u %s",
        xToString(getEffectivePrice()), getAddr(), getBlock(), getIdx(), getHash().ToString().substr(0, 10),
        getProperty(), FormatMP(getProperty(), getAmountForSale()));
}

void CMPMetaDEx::saveOffer(std::ofstream& file, SHA256_CTX* shaCtx) const
{
    std::string lineOut = strprintf("%s,%d,%d,%d,%d,%d,%d,%d,%s,%d",
        addr,
        block,
        amount_forsale,
        property,
        amount_desired,
        desired_property,
        subaction,
        idx,
        txid.ToString(),
        amount_remaining
    );

    // add the line to the hash
    SHA256_Update(shaCtx, lineOut.c_str(), lineOut.length());

    // write the line
    file << lineOut << std::endl;
}

void CMPContractDex::saveOffer(std::ofstream& file, SHA256_CTX* shaCtx) const
{
    std::string lineOut = strprintf("%s,%d,%d,%d,%d,%d,%d,%d,%s,%d,%d,%d",
        getAddr(),
        getBlock(),
        getAmountForSale(),
        getProperty(),
        getAmountDesired(),
        getDesProperty(),
        getAction(),
        getIdx(),
        getHash().ToString(),
        getAmountRemaining(),
        effective_price,
        trading_action
    );

    // add the line to the hash
    SHA256_Update(shaCtx, lineOut.c_str(), lineOut.length());

    // write the line
    file << lineOut << std::endl;
}

void saveDataGraphs(std::fstream &file, std::string lineOutSixth1, std::string lineOutSixth2, std::string lineOutSixth3, bool savedata_bool)
{
  std::string lineSixth1 = lineOutSixth1;
  std::string lineSixth2 = lineOutSixth2;
  std::string lineSixth3 = lineOutSixth3;

  if ( savedata_bool )
    {
      file << lineSixth1 << "\n";
      file << lineSixth2 << std::endl;
    }
  else
    {
      file << lineSixth1 << "\n";
      file << lineSixth2 << "\n";
      file << lineSixth3 << std::endl;
    }
}

void saveDataGraphs(std::fstream &file, std::string lineOut)
{
  std::string line = lineOut;
  file << line << std::endl;
}

bool MetaDEx_compare::operator()(const CMPMetaDEx &lhs, const CMPMetaDEx &rhs) const
{
    if (lhs.getBlock() == rhs.getBlock()) return lhs.getIdx() < rhs.getIdx();
    else return lhs.getBlock() < rhs.getBlock();
}


bool ContractDex_compare::operator()(const CMPContractDex &lhs, const CMPContractDex &rhs) const
{
    if (lhs.getBlock() == rhs.getBlock()) return lhs.getIdx() < rhs.getIdx();
    else return lhs.getBlock() < rhs.getBlock();
}

bool mastercore::MetaDEx_INSERT(const CMPMetaDEx& objMetaDEx)
{
    // Create an empty price map (to use in case price map for this property does not already exist)
    md_PricesMap temp_prices;
    // Attempt to obtain the price map for the property
    md_PricesMap *p_prices = get_Prices(objMetaDEx.getProperty());

    // Create an empty set of metadex objects (to use in case no set currently exists at this price)
    md_Set temp_indexes;
    md_Set *p_indexes = NULL;

    // Prepare for return code
    std::pair <md_Set::iterator, bool> ret;

    // Attempt to obtain a set of metadex objects for this price from the price map
    if (p_prices) p_indexes = get_Indexes(p_prices, objMetaDEx.unitPrice());
    // See if the set was populated, if not no set exists at this price level, use the empty set that we created earlier
    if (!p_indexes) p_indexes = &temp_indexes;

    // Attempt to insert the metadex object into the set
    ret = p_indexes->insert(objMetaDEx);
    if (false == ret.second) return false;

    // If a prices map did not exist for this property, set p_prices to the temp empty price map
    if (!p_prices) p_prices = &temp_prices;

    // Update the prices map with the new set at this price
    (*p_prices)[objMetaDEx.unitPrice()] = *p_indexes;

    // Set the metadex map for the property to the updated (or new if it didn't exist) price map
    metadex[objMetaDEx.getProperty()] = *p_prices;

    return true;
}

bool mastercore::ContractDex_INSERT(const CMPContractDex &objContractDex)
{
  // Create an empty price map (to use in case price map for this property does not already exist)
  cd_PricesMap temp_prices;

  // Attempt to obtain the price map for the property
  cd_PricesMap *cd_prices = get_PricesCd(objContractDex.getProperty());

  // Create an empty set of contractdex objects (to use in case no set currently exists at this price)
  cd_Set temp_indexes;
  cd_Set *p_indexes = NULL;

  // Prepare for return code
  std::pair <cd_Set::iterator, bool> ret;

  // Attempt to obtain a set of contractdex objects for this price from the price map
  if (cd_prices) p_indexes = get_IndexesCd(cd_prices, objContractDex.getEffectivePrice());

  // See if the set was populated, if not no set exists at this price level, use the empty set that we created earlier
  if (!p_indexes) p_indexes = &temp_indexes;

    // Attempt to insert the contractdex object into the set
  ret = p_indexes->insert(objContractDex);

  if (false == ret.second) return false;

  // If a prices map did not exist for this property, set p_prices to the temp empty price map
  if (!cd_prices) cd_prices = &temp_prices;

  // Update the prices map with the new Set at this price
  (*cd_prices)[objContractDex.getEffectivePrice()] = *p_indexes;

  // Set the contractdex map for the property to the updated (or new if it didn't exist) price map
  contractdex[objContractDex.getProperty()] = *cd_prices;

  return true;
}

// pretty much directly linked to the ADD TX21 command off the wire
int mastercore::MetaDEx_ADD(const std::string& sender_addr, uint32_t prop, int64_t amount, int block, uint32_t property_desired, int64_t amount_desired, const uint256& txid, unsigned int idx)
{
    int rc = METADEX_ERROR -1;
    // Create a MetaDEx object from paremeters
    CMPMetaDEx new_mdex(sender_addr, block, prop, amount, property_desired, amount_desired, txid, idx, CMPTransaction::ADD);
    if (msc_debug_metadex_add) PrintToLog("%s(); buyer obj: %s\n", __FUNCTION__, new_mdex.ToString());

    // Ensure this is not a badly priced trade (for example due to zero amounts)
    if (0 >= new_mdex.unitPrice()) return METADEX_ERROR -66;

    // Match against existing trades, remainder of the order will be put into the order book
    // if (msc_debug_metadex_add) MetaDEx_debug_print();
    x_Trade(&new_mdex);

    // Insert the remaining order into the MetaDEx maps
    if (0 < new_mdex.getAmountRemaining()) { //switch to getAmountRemaining() when ready
        if (!MetaDEx_INSERT(new_mdex)) {
            PrintToLog("%s() ERROR: ALREADY EXISTS, line %d, file: %s\n", __FUNCTION__, __LINE__, __FILE__);
            return METADEX_ERROR -70;
        } else {
            // move tokens into reserve
            assert(update_tally_map(sender_addr, prop, -new_mdex.getAmountRemaining(), BALANCE));
            assert(update_tally_map(sender_addr, prop, new_mdex.getAmountRemaining(), METADEX_RESERVE));

            if (msc_debug_metadex_add) PrintToLog("==== INSERTED: %s= %s\n", xToString(new_mdex.unitPrice()), new_mdex.ToString());
            // if (msc_debug_metadex_add) MetaDEx_debug_print();
        }
    }
    rc = 0;
    return rc;
}

int mastercore::ContractDex_ADD(const std::string& sender_addr, uint32_t prop, int64_t amount, int block, const uint256& txid, unsigned int idx, uint64_t effective_price, uint8_t trading_action, int64_t amount_to_reserve)
{
    int rc = METADEX_ERROR -1;

    /*Remember: Here CMPTransaction::ADD is the subaction coming from CMPMetaDEx*/
    CMPContractDex new_cdex(sender_addr, block, prop, amount, 0, 0, txid, idx, CMPTransaction::ADD, effective_price, trading_action);

    if (msc_debug_contractdex_add) PrintToLog("%s(); buyer obj: %s\n", __FUNCTION__, new_cdex.ToString());
    //  Ensure this is not a badly priced trade (for example due to zero amounts)
    if (0 >= new_cdex.getEffectivePrice()) return METADEX_ERROR -66;

    x_Trade(&new_cdex);

    // Insert the remaining order into the ContractDex maps
    if (0 < new_cdex.getAmountForSale())
    {
        //switch to getAmounForSale() when ready
        if (!ContractDex_INSERT(new_cdex))
        {
            if (msc_debug_contractdex_add) PrintToLog("%s() ERROR: ALREADY EXISTS, line %d, file: %s\n", __FUNCTION__, __LINE__, __FILE__);
            return METADEX_ERROR -70;  // TODO: create new numbers for our errors.
        } else {
            if (msc_debug_contractdex_add)
            {
                PrintToLog("\nInserted in the orderbook!!\n");
                PrintToLog("==== INSERTED: %s= %s\n", xToString(new_cdex.getEffectivePrice()), new_cdex.ToString());
            }
        }
     }

     rc = 0;
     return rc;
}

int mastercore::ContractDex_ADD_MARKET_PRICE(const std::string& sender_addr, uint32_t contractId, int64_t amount, int block, const uint256& txid, unsigned int idx, uint8_t trading_action, int64_t amount_to_reserve)
{
    int rc = METADEX_ERROR -1;

    if(trading_action != BUY && trading_action != SELL)
        return -1;

    uint64_t ask = edgeOrderbook(contractId, trading_action);

    CMPContractDex new_cdex(sender_addr, block, contractId, amount, 0, 0, txid, idx, CMPTransaction::ADD, ask, trading_action);

    if(msc_debug_contract_add_market) PrintToLog("effective price of new_cdex /buy/: %d\n",new_cdex.getEffectivePrice());
    if (0 >= new_cdex.getEffectivePrice()) return METADEX_ERROR -66;


    while(true)
    {
        x_Trade(&new_cdex);
        if (new_cdex.getAmountForSale() == 0)
	         break;

        uint64_t price = edgeOrderbook(contractId,trading_action);
        new_cdex.setPrice(price);
    }

    return rc;
}

int mastercore::ContractDex_CANCEL_EVERYTHING(const uint256& txid, unsigned int block, const std::string& sender_addr, uint32_t contractId)
{
    int rc = METADEX_ERROR -40;
    bool bValid = false;

    for (cd_PropertiesMap::iterator my_it = contractdex.begin(); my_it != contractdex.end(); ++my_it)
    {
        unsigned int prop = my_it->first;

        if (msc_debug_contract_cancel_every) PrintToLog(" ## property: %u\n", prop);
        cd_PricesMap &prices = my_it->second;

        for (cd_PricesMap::iterator it = prices.begin(); it != prices.end(); ++it)
        {
            uint64_t price = it->first;
            cd_Set &indexes = it->second;

            if (msc_debug_contract_cancel_every) PrintToLog("  # Price Level: %s\n", xToString(price));

            for (cd_Set::iterator it = indexes.begin(); it != indexes.end();)
            {
	              if (msc_debug_contract_cancel_every) PrintToLog("%s= %s\n", xToString(price), it->ToString());

	              if (it->getAddr() != sender_addr || it->getProperty() != contractId || it->getAmountForSale() == 0)
                {
	                  ++it;
	                  continue;
	              }

	              rc = 0;
	              if (msc_debug_contract_cancel_every) PrintToLog("%s(): REMOVING %s\n", __FUNCTION__, it->ToString());

	              CMPSPInfo::Entry sp;
	              assert(_my_sps->getSP(it->getProperty(), sp));
	              uint32_t collateralCurrency = sp.collateral_currency;
	              int64_t marginRe = static_cast<int64_t>(sp.margin_requirement);

	              string addr = it->getAddr();
	              int64_t amountForSale = it->getAmountForSale();

	              rational_t conv = notionalChange(contractId);
	              int64_t num = conv.numerator().convert_to<int64_t>();
	              int64_t den = conv.denominator().convert_to<int64_t>();
	              int64_t balance = getMPbalance(addr,collateralCurrency,BALANCE);

	              arith_uint256 amountMargin = (ConvertTo256(amountForSale) * ConvertTo256(marginRe) * ConvertTo256(num) / (ConvertTo256(den) * ConvertTo256(factorE)));
	              int64_t redeemed = ConvertTo64(amountMargin);

                // if (msc_debug_contract_cancel_every)
                // {
    	              PrintToLog("collateral currency id of contract : %d\n",collateralCurrency);
    	              PrintToLog("margin requirement of contract : %d\n",marginRe);
    	              PrintToLog("amountForSale: %d\n",amountForSale);
    	              PrintToLog("Address: %d\n",addr);
    	              PrintToLog("--------------------------------------------\n");
                // }
	              // move from reserve to balance the collateral
	              if (balance > redeemed && balance > 0 && redeemed > 0)
			          {
	                  assert(update_tally_map(addr, collateralCurrency, redeemed, BALANCE));
	                  assert(update_tally_map(addr, collateralCurrency, -redeemed, CONTRACTDEX_MARGIN));
			          }

	              bValid = true;
	              // p_txlistdb->recordContractDexCancelTX(txid, it->getHash(), bValid, block, it->getProperty(), it->getAmountForSale
	              indexes.erase(it++);
            }
        }
    }
    if (!bValid && msc_debug_contract_cancel_every)
      PrintToLog("You don't have active orders\n");

    return rc;
}

int mastercore::ContractDex_CANCEL_FOR_BLOCK(const uint256& txid,  int block,unsigned int idx, const std::string& sender_addr)
{
    int rc = METADEX_ERROR -40;
    bool bValid = false;
    for (cd_PropertiesMap::iterator my_it = contractdex.begin(); my_it != contractdex.end(); ++my_it)
    {
        cd_PricesMap &prices = my_it->second;

        for (cd_PricesMap::iterator it = prices.begin(); it != prices.end(); ++it)
        {
            // uint64_t price = it->first;
            cd_Set &indexes = it->second;

            for (cd_Set::iterator it = indexes.begin(); it != indexes.end();)
            {
	              string addr = it->getAddr();
 	              if (addr != sender_addr || it->getBlock()!= block || it->getIdx()!= idx)
                {
	                  ++it;
	                  continue;
	              }

	              CMPSPInfo::Entry sp;
	              uint32_t contractId = it->getProperty();
	              assert(_my_sps->getSP(contractId, sp));
	              uint32_t collateralCurrency = sp.collateral_currency;
	              uint32_t marginRe = sp.margin_requirement;

	              int64_t balance = getMPbalance(addr,collateralCurrency,BALANCE);
	              int64_t amountForSale = it->getAmountForSale();

	              rational_t conv = notionalChange(it->getProperty());
	              int64_t num = conv.numerator().convert_to<int64_t>();
	              int64_t den = conv.denominator().convert_to<int64_t>();

	              arith_uint256 amountMargin = (ConvertTo256(amountForSale) * ConvertTo256(marginRe) * ConvertTo256(num) / (ConvertTo256(den) * ConvertTo256(factorE)));
	              int64_t redeemed = ConvertTo64(amountMargin);

                if(msc_debug_contract_cancel_forblock)
                {
    	              PrintToLog("collateral currency id of contract : %d\n", collateralCurrency);
    	              PrintToLog("margin requirement of contract : %d\n", marginRe);
    	              PrintToLog("amountForSale: %d\n", amountForSale);
    	              PrintToLog("Address: %d\n", addr);
                }

	              std::string sgetback = FormatDivisibleMP(redeemed, false);


	              if(msc_debug_contract_cancel_forblock) PrintToLog("amount returned to balance: %d\n", redeemed);


	              // move from reserve to balance the collateral
	              if (balance > redeemed && balance > 0 && redeemed > 0)
			          {
			              assert(update_tally_map(addr, collateralCurrency, redeemed, BALANCE));
	                  assert(update_tally_map(addr, collateralCurrency,  -redeemed, CONTRACTDEX_RESERVE));
	              }

	              // record the cancellation
	              bValid = true;
	              // p_txlistdb->recordContractDexCancelTX(txid, it->getHash(), bValid, block, it->getProperty(), it->getAmountForSale
	              indexes.erase(it++);

	              rc = 0;
            }
       }
  }
  if (!bValid && msc_debug_contract_cancel_forblock){
    PrintToLog("Incorrect block or idx\n");
  }

  return rc;
}

bool mastercore::ContractDex_CHECK_ORDERS(const std::string& sender_addr, uint32_t contractId)
{
    bool bValid = false;
    for (cd_PropertiesMap::iterator my_it = contractdex.begin(); my_it != contractdex.end(); ++my_it)
    {
        cd_PricesMap &prices = my_it->second;

        for (cd_PricesMap::iterator it = prices.begin(); it != prices.end(); ++it)
        {
            // uint64_t price = it->first;
            cd_Set &indexes = it->second;

            for (cd_Set::iterator it = indexes.begin(); it != indexes.end();)
            {
	              string addr = it->getAddr();
                uint32_t contract = it->getProperty();
 	              if (addr != sender_addr || contract != contractId)
                {
	                  ++it;
	                  continue;
	              }

	              bValid = true;
                break;
            }
        }
    }

    return bValid;
}

int mastercore::ContractDex_CANCEL_IN_ORDER(const std::string& sender_addr, uint32_t contractId)
{
    int rc = METADEX_ERROR -40;
    bool bValid = false;

    CMPSPInfo::Entry sp;
    if(!_my_sps->getSP(contractId, sp))
        return rc;

    uint32_t collateralCurrency = sp.collateral_currency;
    int64_t marginRe = static_cast<int64_t>(sp.margin_requirement);

    for (cd_PropertiesMap::iterator my_it = contractdex.begin(); my_it != contractdex.end(); ++my_it) {
        unsigned int prop = my_it->first;

        if(msc_debug_contract_cancel_inorder) PrintToLog(" ## property: %u\n", prop);
        cd_PricesMap &prices = my_it->second;

        for (cd_PricesMap::iterator it = prices.begin(); it != prices.end(); ++it) {
            uint64_t price = it->first;
            cd_Set &indexes = it->second;

            for (cd_Set::iterator it = indexes.begin(); it != indexes.end();) {

                if(msc_debug_contract_cancel_inorder)
                {
                    PrintToLog("%s= %s\n", xToString(price), it->ToString());
                    PrintToLog("address: %d\n",it->getAddr());
                    PrintToLog("propertyid: %d\n",it->getProperty());
                    PrintToLog("amount for sale: %d\n",it->getAmountForSale());
                }

                if (it->getAddr() != sender_addr || it->getProperty() != contractId || it->getAmountForSale() == 0) {
                    ++it;
                    continue;
                }

                string addr = it->getAddr();
                int64_t amountForSale = it->getAmountForSale();

                // rational_t conv = notionalChange(contractId);
                rational_t conv = rational_t(1,1);
                int64_t num = conv.numerator().convert_to<int64_t>();
                int64_t den = conv.denominator().convert_to<int64_t>();
                int64_t balance = getMPbalance(addr,collateralCurrency,BALANCE);

                if(msc_debug_contract_cancel_inorder)
                {
                    PrintToLog("collateral currency id of contract : %d\n",collateralCurrency);
                    PrintToLog("margin requirement of contract : %d\n",marginRe);
                    PrintToLog("amountForSale: %d\n",amountForSale);
                    PrintToLog("Address: %d\n",addr);
                }
                // arith_uint256 amountMargin = ConvertTo256(amountForSale) * ConvertTo256(marginRe) * ConvertTo256(num) / ConvertTo256(den);
                arith_uint256 amountMargin = ConvertTo256(amountForSale) * ConvertTo256(num) / ConvertTo256(den);
                int64_t redeemed = ConvertTo64(amountMargin);
                if(msc_debug_contract_cancel_inorder) PrintToLog("redeemed: %d\n",redeemed);

                // move from reserve to balance the collateral
                if (balance > redeemed && balance > 0 && redeemed > 0) {
                    assert(update_tally_map(addr, collateralCurrency, redeemed, BALANCE));
                    assert(update_tally_map(addr, collateralCurrency, -redeemed, CONTRACTDEX_MARGIN));
                // // record the cancellation
                }

                bValid = true;
                if(msc_debug_contract_cancel_inorder) PrintToLog("CANCEL IN ORDER: order found!\n");
                // p_txlistdb->recordContractDexCancelTX(txid, it->getHash(), bValid, block, it->getProperty(), it->getAmountForSale
                indexes.erase(it++);
                rc = 0;
                return rc;
            }

        }
    }

    if (!bValid && msc_debug_contract_cancel_inorder)
    {
       PrintToLog("CANCEL IN ORDER: You don't have active orders\n");
       rc = 1;
    }

    return rc;
}

int mastercore::ContractDex_ADD_ORDERBOOK_EDGE(const std::string& sender_addr, uint32_t contractId, int64_t amount, int block, const uint256& txid, unsigned int idx, uint8_t trading_action, int64_t amount_to_reserve)
{
    int rc = METADEX_ERROR -1;
    uint64_t price;

    (trading_action == BUY) ? price = edgeOrderbook(contractId, SELL) : price = edgeOrderbook(contractId, BUY);
    if (msc_debug_add_orderbook_edge) PrintToLog("price of edge: %d\n",price);
    if (price > 0)
    {
        CMPContractDex new_cdex(sender_addr, block, contractId, amount, 0, 0, txid, idx, CMPTransaction::ADD, price, trading_action);
        if (!ContractDex_INSERT(new_cdex))
        {
            if (msc_debug_add_orderbook_edge) PrintToLog("%s() ERROR: ALREADY EXISTS, line %d, file: %s\n", __FUNCTION__, __LINE__, __FILE__);
            return METADEX_ERROR -70;  // TODO: create new numbers for our errors.
        } else {
            if(msc_debug_add_orderbook_edge) PrintToLog("\nInserted in the orderbook!!\n");
        }

    } else{
        if(msc_debug_add_orderbook_edge) PrintToLog("\nNo orders in ask or bid\n");
        return METADEX_ERROR -60;
    }

    rc = 0;
    return rc;
}

/*NEW FUNCTION*/
int mastercore::ContractDex_CLOSE_POSITION(const uint256& txid, unsigned int block, const std::string& sender_addr, uint32_t contractId, uint32_t collateralCurrency)
{
    int64_t shortPosition = getMPbalance(sender_addr,contractId, NEGATIVE_BALANCE);
    int64_t longPosition = getMPbalance(sender_addr,contractId, POSITIVE_BALANCE);

    if (msc_debug_close_position)
    {
        PrintToLog("shortPosition before: %d\n",shortPosition);
        PrintToLog("longPosition before: %d\n",longPosition);
    }

    LOCK(cs_tally);

    // Clearing the position
    unsigned int idx=0;
    if (shortPosition > 0 && longPosition == 0)
    {
        if(msc_debug_close_position) PrintToLog("Short Position closing...\n");
        ContractDex_ADD_MARKET_PRICE(sender_addr,contractId, shortPosition, block, txid, idx,BUY, 0);
    } else if (longPosition > 0 && shortPosition == 0){
        if(msc_debug_close_position) PrintToLog("Long Position closing...\n");
        ContractDex_ADD_MARKET_PRICE(sender_addr,contractId, longPosition, block, txid, idx, SELL, 0);
    }

    // cleaning liquidation price
    int64_t liqPrice = getMPbalance(sender_addr,contractId, LIQUIDATION_PRICE);
    if (liqPrice > 0){
        update_tally_map(sender_addr, contractId, -liqPrice, LIQUIDATION_PRICE);
    }

    int64_t shortPositionAf = getMPbalance(sender_addr,contractId, NEGATIVE_BALANCE);
    int64_t longPositionAf= getMPbalance(sender_addr,contractId, POSITIVE_BALANCE);

    if(msc_debug_close_position) PrintToLog("%s: shortPosition Now: %d, longPosition Now: %d\n",__func__, shortPositionAf, longPositionAf);

    if (shortPositionAf == 0 && longPositionAf == 0){
        if(msc_debug_close_position) PrintToLog("POSITION CLOSED!!!\n");
    } else {
        if(msc_debug_close_position) PrintToLog("ERROR: Position partialy Closed\n");
    }

    return 0;
}

int64_t mastercore::getPairMarketPrice(std::string num, std::string den)
{
  LOCK(cs_tally);
  uint32_t nextSPID = _my_sps->peekNextSPID();

  uint32_t numId = 0;
  uint32_t denId = 0;

  for (uint32_t propertyId = 1; propertyId < nextSPID; propertyId++)
    {
      CMPSPInfo::Entry sp;
      if (_my_sps->getSP(propertyId, sp))
       	{
	  if ( sp.name == num )
	    {
	      if(msc_debug_get_pair_market_price) PrintToLog("\npropertyId num: %d\n", propertyId);
	      numId = propertyId;
	    }
	  if ( sp.name == den )
	    {
	      if(msc_debug_get_pair_market_price) PrintToLog("\npropertyId den: %d\n", propertyId);
	      denId = propertyId;
	    }
	}
    }

  return market_priceMap[numId][denId];
}

uint64_t mastercore::edgeOrderbook(uint32_t contractId, uint8_t tradingAction)
{
    uint64_t price = 0;
    uint64_t result = 0;
    std::vector<uint64_t> vecContractDexPrices;

    cd_PricesMap* const ppriceMap = get_PricesCd(contractId); // checking the ask price of contract A
    for (cd_PricesMap::iterator it = ppriceMap->begin(); it != ppriceMap->end(); ++it) {
        const cd_Set& indexes = it->second;
        for (cd_Set::const_iterator it = indexes.begin(); it != indexes.end(); ++it) {
            const CMPContractDex& obj = *it;
            if (obj.getTradingAction() == tradingAction || obj.getAmountForSale() <= 0) continue;
            price = obj.getEffectivePrice();
            if(msc_debug_sp) PrintToLog("%s(): choosen price: %d\n",__func__, price);
            vecContractDexPrices.push_back(price);
        }
    }

    if (tradingAction == BUY && !vecContractDexPrices.empty()){
       result = vecContractDexPrices.front();
    } else if (tradingAction == SELL && !vecContractDexPrices.empty()){
       result = vecContractDexPrices.back();
    }

    return static_cast<uint64_t>(result);
}

bool mastercore::MetaDEx_Search_ALL(uint64_t& amount, uint32_t propertyOffered)
{
    bool bBuyerSatisfied = false;

    for (md_PropertiesMap::iterator my_it = metadex.begin(); my_it != metadex.end(); ++my_it)
    {

        // if (msc_debug_contract_cancel_every) PrintToLog(" ## property: %u\n", prop);
        md_PricesMap &prices = my_it->second;

        for (md_PricesMap::iterator itt = prices.begin(); itt != prices.end(); ++itt)
        {
            const rational_t price = itt->first;
            md_Set &indexes = itt->second;

            // if (msc_debug_search_all) PrintToLog("  # Price Level: %s\n", xToString(price));

            for (md_Set::iterator it = indexes.begin(); it != indexes.end();)
            {

                // for testing ALL is 4
	              if (it->getProperty() != ALL && it->getDesProperty() != propertyOffered)
                {
	                  ++it;
	                  continue;
	              }

	              if (msc_debug_search_all) PrintToLog("%s(): ALLS FOUND! %s\n", __func__, it->ToString());

                if (msc_debug_search_all) PrintToLog("%s(): amount of ALL: %d, desproperty: %d; amount of desproperty: %d\n", __func__, it->getAmountForSale(), it->getDesProperty(), it->getAmountDesired());


                arith_uint256 iCouldBuy = (ConvertTo256(amount) * ConvertTo256(it->getAmountForSale())) / ConvertTo256(it->getAmountDesired());

                int64_t nCouldBuy = 0;

                if (iCouldBuy < ConvertTo256(it->getAmountRemaining())) {
                  nCouldBuy = ConvertTo64(iCouldBuy);
                  PrintToLog("%s(): nCouldBuy < it->getAmountRemaining()\n", __func__);
                } else {
                  nCouldBuy = it->getAmountRemaining();
                }

                if (msc_debug_search_all) PrintToLog("%s(): nCouldBuy: %d\n", __func__, nCouldBuy);

                if (nCouldBuy == 0)
                {
                    PrintToLog("-- buyer has not enough tokens for sale to purchase one unit!\n");
                    ++it;
                    continue;
                }

                // taking ALLs from seller
                assert(update_tally_map(it->getAddr(), it->getProperty(), -nCouldBuy, METADEX_RESERVE));

                // discounting the amount
                amount -= static_cast<uint64_t>(nCouldBuy);

                const int64_t seller_amountLeft = it->getAmountForSale() - nCouldBuy;

                CMPMetaDEx seller_replacement = *it;
                seller_replacement.setAmountRemaining(seller_amountLeft, "seller_replacement");

                // erase the old seller element
                indexes.erase(it++);

                // insert the updated one in place of the old
                if (0 < seller_replacement.getAmountRemaining())
                {
                    PrintToLog("++ inserting seller_replacement: %s\n", seller_replacement.ToString());
                    indexes.insert(seller_replacement);
                }

            }

            if (0 == amount)
            {
                bBuyerSatisfied = true;
                break;
            }

        }

        if (0 == amount)
        {
            bBuyerSatisfied = true;
            break;
        }
    }

    return bBuyerSatisfied;
}
