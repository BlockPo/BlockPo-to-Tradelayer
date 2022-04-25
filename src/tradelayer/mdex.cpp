#include <tradelayer/ce.h>
#include <tradelayer/mdex.h>
#include <tradelayer/errors.h>
#include <tradelayer/externfns.h>
#include <tradelayer/log.h>
#include <tradelayer/operators_algo_clearing.h>
#include <tradelayer/register.h>
#include <tradelayer/rules.h>
#include <tradelayer/sp.h>
#include <tradelayer/tradelayer.h>
#include <tradelayer/tradelayer_matrices.h>
#include <tradelayer/tx.h>
#include <tradelayer/uint256_extensions.h>
#include <tradelayer/utilsbitcoin.h>

#include <arith_uint256.h>
#include <chain.h>
#include <hash.h>
#include <tinyformat.h>
#include <uint256.h>
#include <univalue.h>
#include <validation.h>

#include <assert.h>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <stdint.h>
#include <string>

#include <boost/lexical_cast.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/rational.hpp>

typedef boost::multiprecision::cpp_dec_float_100 dec_float;
typedef boost::multiprecision::checked_int128_t int128_t;

using namespace mastercore;

//! Number of digits of unit price
#define DISPLAY_PRECISION_LEN  50
//! Global map for price and order data
md_PropertiesMap mastercore::metadex;
//! Global map for  tokens volume
std::map<int, std::map<uint32_t,int64_t>> mastercore::metavolume;
//! Global map for last contract price
std::map<uint32_t, std::map<int,std::vector<int64_t>>> mastercore::cdexlastprice;

extern MatrixTLS *pt_ndatabase;


md_PricesMap* mastercore::get_Prices(uint32_t prop)
{
    md_PropertiesMap::iterator it = metadex.find(prop);

    if (it != metadex.end()) return &(it->second);

    return static_cast<md_PricesMap*>(nullptr);
}

md_Set* mastercore::get_Indexes(md_PricesMap* p, rational_t price)
{
    md_PricesMap::iterator it = p->find(price);

    if (it != p->end()) return &(it->second);

    return static_cast<md_Set*>(nullptr);

}

cd_PropertiesMap mastercore::contractdex;

cd_PricesMap *mastercore::get_PricesCd(uint32_t prop)
{
    cd_PropertiesMap::iterator it = contractdex.find(prop);

    if (it != contractdex.end()) return &(it->second);

    return static_cast<cd_PricesMap*>(nullptr);
}

cd_Set *mastercore::get_IndexesCd(cd_PricesMap *p, uint64_t price)
{
    cd_PricesMap::iterator it = p->find(price);

    if (it != p->end()) return &(it->second);

    return static_cast<cd_Set*>(nullptr);
}

void mastercore::LoopBiDirectional(cd_PricesMap* const ppriceMap, uint8_t trdAction, MatchReturnType &NewReturn, CMPContractDex* const pnew, const uint32_t propertyForSale)
{
  cd_PricesMap::iterator it_fwdPrices;
  cd_PricesMap::reverse_iterator it_bwdPrices;

  if ( trdAction == buy )
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

// it needs more work (separate fee from margin)!
void mastercore::takeMargin(int64_t amount, uint32_t contract_traded, const CDInfo::Entry& cd, CMPContractDex *elem)
{
        // taking more margin
        const uint64_t& allreserved = elem->getAmountReserved();
        const uint32_t& colateral = cd.collateral_currency;
        const int64_t leverage = getContractRecord(elem->getAddr(), contract_traded, LEVERAGE);
        PrintToLog("%s(): leverage: %d\n",__func__, leverage);
        const arith_uint256 aMarginRequirement = ConvertTo256(cd.margin_requirement);
        const arith_uint256 aAmount = ConvertTo256(amount);
        const arith_uint256 aLeverage = ConvertTo256(leverage);

        const arith_uint256 nAmountOfMoney = DivideAndRoundUp(aAmount * aMarginRequirement, aLeverage);
        const int64_t amountOfMoney = ConvertTo64(nAmountOfMoney);

        PrintToLog("%s(): amountOfMoney: %d\n",__func__, amountOfMoney);

        // if we need more margin, we add the difference.
        // updating amount reserved for the order
        assert(update_tally_map(elem->getAddr(), colateral, -amountOfMoney, CONTRACTDEX_RESERVE));
        elem->updateAmountReserved(-amountOfMoney);

        // passing colateral to margin position
        assert(update_register_map(elem->getAddr(), contract_traded, amountOfMoney, MARGIN));

        PrintToLog("%s(): taking more margin: allreserved: %d, amountOfMoney: %d, colateral: %d\n",__func__, allreserved, amountOfMoney, colateral);

}

// update entries (amount,price)
void mastercore::updateAllEntry(int64_t oldPosition, int64_t newPosition, int64_t nCouldBuy, uint32_t contract_traded, CMPContractDex* elem, const CDInfo::Entry& cd)
{
    const int signOld = boost::math::sign(oldPosition);
    const int signNew = boost::math::sign(newPosition);

    // open position
    if (signOld == 0 && signNew != 0)
    {
        PrintToLog("%s() open position\n",__func__);
        // updating entries (amount of contracts , price)
        assert(insert_entry(elem->getAddr(), contract_traded, nCouldBuy, elem->getEffectivePrice()));
        //taking margin
        takeMargin(nCouldBuy, contract_traded, cd, elem);

        // here we need to save the bankruptcy price
        const int64_t initMargin = getMPbalance(elem->getAddr(), cd.collateral_currency, CONTRACTDEX_RESERVE);
        set_bankruptcy_price_onmap(elem->getAddr(), contract_traded, cd.notional_size, initMargin);

    // same position
    } else if (signOld == signNew) {

        // if position increasing -> we add a new entry
        if(abs(newPosition) > abs(oldPosition))
        {
            PrintToLog("%s() increasing position\n",__func__);
            // updating entries (amount of contracts , price)
            assert(insert_entry(elem->getAddr(), contract_traded, nCouldBuy, elem->getEffectivePrice()));
            //taking margin
            takeMargin(nCouldBuy, contract_traded, cd, elem);

        // decreasing -> delete some contracts in entry
        } else {
            PrintToLog("%s() decreasing position\n",__func__);
            assert(decrease_entry(elem->getAddr(), contract_traded, nCouldBuy));
        }

    // closing position
   } else if (signOld != 0 && signNew == 0) {
       //releasing margin
       PrintToLog("%s() closing position\n",__func__);
       const int64_t remainingMargin = getContractRecord(elem->getAddr(), contract_traded, MARGIN);

       // resetting the LEVERAGE
       assert(reset_leverage_register(elem->getAddr(), contract_traded));

       // passing  from margin to balance
       assert(update_register_map(elem->getAddr(), contract_traded, -remainingMargin, MARGIN));
       assert(update_tally_map(elem->getAddr(), cd.collateral_currency, remainingMargin, BALANCE));

      assert(decrease_entry(elem->getAddr(), contract_traded, nCouldBuy));

     // closing position and opening another from different side
   } else if (signOld != signNew && (signOld != 0 && signNew != 0)) {

         PrintToLog("%s() closing position and opening another from different side\n",__func__);
         assert(decrease_entry(elem->getAddr(), contract_traded, nCouldBuy, elem->getEffectivePrice()));

         const int64_t remainingMargin = getContractRecord(elem->getAddr(), contract_traded, MARGIN);

         // passing  from margin to balance
         assert(update_register_map(elem->getAddr(), contract_traded, -remainingMargin, MARGIN));
         assert(update_tally_map(elem->getAddr(), cd.collateral_currency, remainingMargin, BALANCE));

         //here we adapt the margin to the new position
         const int64_t newAmount = abs(newPosition);
         takeMargin(newAmount, contract_traded, cd, elem);

         // here we need to include the bankruptcy price
         const int64_t initMargin = getMPbalance(elem->getAddr(), cd.collateral_currency, CONTRACTDEX_RESERVE);
         set_bankruptcy_price_onmap(elem->getAddr(), contract_traded, cd.notional_size, initMargin);
   }

 }

void mastercore::x_TradeBidirectional(typename cd_PricesMap::iterator &it_fwdPrices, typename cd_PricesMap::reverse_iterator &it_bwdPrices, uint8_t trdAction, CMPContractDex* const pnew, const uint64_t sellerPrice, const uint32_t propertyForSale, MatchReturnType &NewReturn)
{
  cd_Set* const pofferSet = trdAction == buy ? &(it_fwdPrices->second) : &(it_bwdPrices->second);

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

      if (!boolAddresses && !boolProperty && !boolTrdAction) {
          PrintToLog("%s(): trading with yourself is not allowed\n",__func__);

          CDInfo::Entry cd;
          assert(_my_cds->getCD(propertyForSale, cd));
          const uint32_t collateral = cd.collateral_currency;

          const int64_t amountReserved = getMPbalance(pnew->getAddr(), collateral, CONTRACTDEX_RESERVE);

          PrintToLog("%s(): amountReserved: %d, collateral: %d\n",__func__, amountReserved, collateral);

          if (0 < amountReserved) {
            assert(update_tally_map(pnew->getAddr(), collateral, amountReserved, BALANCE));
            assert(update_tally_map(pnew->getAddr(), collateral,  -amountReserved, CONTRACTDEX_RESERVE));
          }

          pnew->setAmountForsale(0, "no_remaining");
          return;
      }


      if ( findTrueValue(boolProperty, boolTrdAction, !boolAddresses) )
    	{
          PrintToLog("%s(): findTrueValue == true\n",__func__);
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
      const uint32_t& property_traded = pold->getProperty();
      int64_t amountpnew = abs(pnew->getAmountForSale());
      int64_t amountpold = abs(pold->getAmountForSale());

      if(msc_debug_x_trade_bidirectional)
      {
          PrintToLog("amountpnew %d, amountpold: %d\n", amountpnew, amountpold);
      }

      const int64_t poldBalance = getContractRecord(pold->getAddr(), property_traded, CONTRACT_POSITION);
      const int64_t pnewBalance = getContractRecord(pnew->getAddr(), property_traded, CONTRACT_POSITION);

      int64_t poldPositiveBalanceB = 0;
      int64_t pnewPositiveBalanceB = 0;
      int64_t poldNegativeBalanceB = 0;
      int64_t pnewNegativeBalanceB = 0;

      // bringing back positive or negative position
      if (poldBalance > 0) {
          poldPositiveBalanceB = poldBalance;
      } else if (poldBalance < 0) {
          poldNegativeBalanceB = -poldBalance;
      }

      if (pnewBalance > 0) {
          pnewPositiveBalanceB = pnewBalance;
      } else if (pnewBalance < 0) {
          pnewNegativeBalanceB = -pnewBalance;
      }

      if(msc_debug_x_trade_bidirectional)
      {
          PrintToLog("poldPositiveBalanceB: %d, poldNegativeBalanceB: %d\n", poldPositiveBalanceB, poldNegativeBalanceB);
          PrintToLog("pnewPositiveBalanceB: %d, pnewNegativeBalanceB: %d\n", pnewPositiveBalanceB, pnewNegativeBalanceB);
      }

      const int64_t &possitive_sell = (pold->getTradingAction() == sell) ? poldPositiveBalanceB : pnewPositiveBalanceB;
      const int64_t &negative_sell  = (pold->getTradingAction() == sell) ? poldNegativeBalanceB : pnewNegativeBalanceB;
      const int64_t &possitive_buy  = (pold->getTradingAction() == sell) ? pnewPositiveBalanceB : poldPositiveBalanceB;
      const int64_t &negative_buy   = (pold->getTradingAction() == sell) ? pnewNegativeBalanceB : poldNegativeBalanceB;

      int64_t seller_amount  = (pold->getTradingAction() == sell) ? abs(pold->getAmountForSale()) : abs(pnew->getAmountForSale());
      int64_t buyer_amount   = (pold->getTradingAction() == sell) ? abs(pnew->getAmountForSale()) : abs(pold->getAmountForSale());

      const std::string &seller_address = (pold->getTradingAction() == sell) ? pold->getAddr() : pnew->getAddr();
      const std::string &buyer_address  = (pold->getTradingAction() == sell) ? pnew->getAddr() : pold->getAddr();

      int64_t nCouldBuy = (buyer_amount < seller_amount) ? buyer_amount : seller_amount;



      if(msc_debug_x_trade_bidirectional)
      {
          PrintToLog("seller_amount %d, buyer_amount: %d, nCouldBuy: %d\n", seller_amount, buyer_amount, nCouldBuy);
      }

      if (nCouldBuy == 0)
      {
      	  ++offerIt;
      	  continue;
      }

      /*************************************************************************************************/
      /** Computing VWAP Price**/

      CDInfo::Entry cd;
      assert(_my_cds->getCD(property_traded, cd));

      const uint32_t& NotionalSize = cd.notional_size;

      arith_uint256 Volume256_t = mastercore::ConvertTo256(NotionalSize) * mastercore::ConvertTo256(nCouldBuy)/COIN;
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

      if (boolAddresses)
	    {
          assert(update_register_map(seller_address, property_traded, -nCouldBuy, CONTRACT_POSITION));
          assert(update_register_map(buyer_address, property_traded, nCouldBuy, CONTRACT_POSITION));
      }

      // bringing back new positions
      const int64_t poldNBalance = getContractRecord(pold->getAddr(), property_traded, CONTRACT_POSITION);
      const int64_t pnewNBalance = getContractRecord(pnew->getAddr(), property_traded, CONTRACT_POSITION);

      //------------------------------------------------------------------------
      CMPContractDex contract_replacement = *pold;
      CMPContractDex *aux = &contract_replacement;

      // entries (amount, price)
      updateAllEntry(pnewBalance, pnewNBalance, nCouldBuy, property_traded, pnew, cd);
      updateAllEntry(poldBalance, poldNBalance, nCouldBuy, property_traded, aux, cd);

      // checking here if positions increase or decrease (we need this to take more margin if it's required)
      PrintToLog("%s(): abs(poldBalance): %d, abs(poldNBalance): %d\n",__func__, abs(poldBalance), abs(poldNBalance));

      int64_t poldPositiveBalanceL = 0;
      int64_t pnewPositiveBalanceL = 0;
      int64_t poldNegativeBalanceL = 0;
      int64_t pnewNegativeBalanceL = 0;

      if (poldNBalance > 0) {
          poldPositiveBalanceL = poldNBalance;
      } else if (poldNBalance < 0) {
          poldNegativeBalanceL = -poldNBalance;
      }

      if (pnewNBalance > 0) {
          pnewPositiveBalanceL = pnewNBalance;
      } else if (pnewNBalance < 0) {
          pnewNegativeBalanceL = -pnewNBalance;
      }

      std::string Status_s = "Empty";
      std::string Status_b = "Empty";

      NewReturn = TRADED;

      int64_t creplNegativeBalance = 0;
      int64_t creplPositiveBalance = 0;

      const int64_t creplBalance = getContractRecord(contract_replacement.getAddr(), property_traded, CONTRACT_POSITION);

      if (creplBalance > 0) {
          creplPositiveBalance = creplBalance;
      } else if (poldNBalance < 0) {
          creplNegativeBalance = -creplBalance;
      }

      if(msc_debug_x_trade_bidirectional)
      {
          PrintToLog("poldPositiveBalance: %d, poldNegativeBalance: %d\n", poldPositiveBalanceL, poldNegativeBalanceL);
          PrintToLog("pnewPositiveBalance: %d, pnewNegativeBalance: %d\n", pnewPositiveBalanceL, pnewNegativeBalanceL);
          PrintToLog("creplPositiveBalance: %d, creplNegativeBalance: %d\n", creplPositiveBalance, creplNegativeBalance);
      }

      int64_t remaining = (seller_amount >= buyer_amount) ? seller_amount - buyer_amount : buyer_amount - seller_amount;


      if(msc_debug_x_trade_bidirectional)
      {
          PrintToLog("remaining: %d\n", remaining);
      }

      if ( (seller_amount > buyer_amount && pold->getTradingAction() == sell) || (seller_amount < buyer_amount && pold->getTradingAction() == buy))
      	{
      	  contract_replacement.setAmountForsale(remaining, "moreinseller");
      	  pnew->setAmountForsale(0, "no_remaining");
      	  NewReturn = TRADED_MOREINSELLER;
      	}
      else if ( (seller_amount < buyer_amount && pold->getTradingAction() == sell) || (seller_amount > buyer_amount && pold->getTradingAction() == buy))
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
      	  if ( pold->getTradingAction() == sell )
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
      	  if ( pold->getTradingAction() == sell )
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
      	  if ( pold->getTradingAction() == sell )
      	    Status_s = creplPositiveBalance > 0 ? "OpenLongPosition" : "OpenShortPosition";
      	  else
      	    Status_s = pnewPositiveBalanceL  > 0 ? "OpenLongPosition" : "OpenShortPosition";
      	  countClosedSeller = 0;
      	}
      /********************************************************/
      if ( possitive_buy > 0 && negative_buy == 0 )
      	{
      	  if ( pold->getTradingAction() == buy )
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
      	  if ( pold->getTradingAction() == buy )
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
      	  if ( pold->getTradingAction() == buy )
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
      if ( pold->getTradingAction() == sell )
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
      if ( pold->getTradingAction() == buy )
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
   * Adding LTC volume traded in contracts.
   *
   */
  mastercore::ContractDex_ADD_LTCVolume(nCouldBuy, property_traded);

  /**
   * Fees calculations for maker and taker.
   *
   */
   //NOTE: it needs refinement!
  mastercore::ContractDex_Fees(pold, pnew, nCouldBuy);


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

          cdexlastprice[property_traded][pnew->getBlock()].push_back(pold->getEffectivePrice());
          // if(msc_debug_x_trade_bidirectional) PrintToLog("%s: marketPrice = %d\n",__func__, pold->getEffectivePrice());
          // t_tradelistdb->recordForUPNL(pnew->getHash(),pnew->getAddr(),property_traded,pold->getEffectivePrice());

          // if(msc_debug_x_trade_bidirectional) PrintToLog("++ erased old: %s\n", offerIt->ToString());
          pofferSet->erase(offerIt++);

          if (0 < remaining) pofferSet->insert(contract_replacement);
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

bool mastercore::ContractDex_Fees(const CMPContractDex* maker, const CMPContractDex* taker, int64_t nCouldBuy)
{
    int64_t takerFee = 0, makerFee = 0, cacheFee = 0, oracleOp = 0;
    uint32_t contractId = maker->getProperty();

    CDInfo::Entry cd;
    if (!_my_cds->getCD(contractId, cd))
       return false;

    if (msc_debug_contractdex_fees) PrintToLog("%s: addressTaker: %d, addressMaker: %d, nCouldBuy: %d, contractIds: %d\n",__func__, taker->getAddr(), maker->getAddr(), nCouldBuy, contractId);


    if (cd.prop_type == ALL_PROPERTY_TYPE_ORACLE_CONTRACT)
    {
        arith_uint256 uTakerFee = (ConvertTo256(nCouldBuy) * ConvertTo256(cd.margin_requirement) * ConvertTo256(25)) / (ConvertTo256(1000) * ConvertTo256(BASISPOINT));  // 2.5 basis point
        arith_uint256 uMakerFee = (ConvertTo256(nCouldBuy) * ConvertTo256(cd.margin_requirement)) / (ConvertTo256(100) * ConvertTo256(BASISPOINT));  // 1 basis point

        oracleOp = makerFee = ConvertTo64(uMakerFee);  // 1 basis point
        cacheFee = makerFee / 2 ;                      // 0.5 basis point
        takerFee = ConvertTo64(uTakerFee);             // 2.5 basis point

        if (msc_debug_contractdex_fees) PrintToLog("%s: oracles cacheFee: %d, oracles takerFee: %d, oracles makerFee: %d\n",__func__,cacheFee, takerFee, makerFee);

        // sumcheck: 2.5 bsp  =  1 bsp +  1 bsp + 0.5
        assert(takerFee == (makerFee + oracleOp + cacheFee));

        // 0.5 basis point to oracle maintaineer
        assert(update_tally_map(cd.issuer,cd.collateral_currency, oracleOp, BALANCE));

        if (cd.collateral_currency == 4) //ALLS
        {
            //0.5 basis point to feecache
            cachefees_oracles[cd.collateral_currency] += cacheFee;

        }else {
            // Create the metadex object with specific params

            // uint256 txid;
            // int block = 0;
            // unsigned int idx = 0;

            // CMPSPInfo::Entry spp;
            // _my_sps->getSP(cd.collateral_currency, spp);
            // if (msc_debug_contractdex_fees) PrintToLog("name of collateral currency:%s \n",spp.name);
            //
            // int64_t VWAPMetaDExPrice = mastercore::getVWAPPriceByPair(spp.name, "ALL");
            // if (msc_debug_contractdex_fees) PrintToLog("\nVWAPMetaDExPrice = %s\n", FormatDivisibleMP(VWAPMetaDExPrice));
            // int64_t amount_desired = 1; // we need to ajust this to market prices
            // CMPMetaDEx new_mdex(addressTaker, block, cd.collateral_currency, cacheFee, 1, amount_desired, txid, idx, CMPTransaction::ADD);
            // mastercore::MetaDEx_INSERT(new_mdex);

        }

    } else {      //natives

          arith_uint256 uTakerFee = (ConvertTo256(nCouldBuy) * ConvertTo256(cd.margin_requirement)) / (ConvertTo256(100) * ConvertTo256(BASISPOINT));
          arith_uint256 uMakerFee = (ConvertTo256(nCouldBuy) * ConvertTo256(cd.margin_requirement) * ConvertTo256(5)) / (ConvertTo256(1000) * ConvertTo256(BASISPOINT));

          takerFee = ConvertTo64(uTakerFee);
          cacheFee = makerFee = ConvertTo64(uMakerFee);

          // sumcheck: 1 bsp  =  0.005 bsp +  0.5 bsp
          // assert(takerFee == (makerFee + cacheFee));

          // 0.5 basis point to feecache
          cachefees[cd.collateral_currency] += cacheFee;

          const int64_t reserveMaker =  getMPbalance(maker->getAddr(), cd.collateral_currency, CONTRACTDEX_RESERVE);
          const int64_t reserveTaker =  getMPbalance(taker->getAddr(), cd.collateral_currency, CONTRACTDEX_RESERVE);


          if (msc_debug_contractdex_fees) PrintToLog("%s: natives takerFee: %d, natives makerFee: %d, cacheFee: %d, reserveMaker: %d, reserveTaker: %d\n",__func__, takerFee, makerFee, cacheFee, reserveMaker, reserveTaker);

    }

    // - to taker, + to maker ( do we need to take fee when positions are decreasing?)
    assert(update_tally_map(taker->getAddr(), cd.collateral_currency, -takerFee, CONTRACTDEX_RESERVE));
    assert(update_tally_map(maker->getAddr(), cd.collateral_currency, makerFee, BALANCE));


    return true;

}

bool mastercore::ContractDex_ADD_LTCVolume(int64_t nCouldBuy, uint32_t contractId)
{
    //trade amount * contract notional * the LTC price
    CDInfo::Entry cd;
    if (!_my_cds->getCD(contractId, cd)){
        if (msc_debug_add_contract_ltc_vol) PrintToLog("%s(): Contract (id: %d) not found in database \n",__func__, contractId);
        return false;
    }

    // get token Price in LTC
    int64_t tokenPrice = 0;

    auto it = market_priceMap.find(static_cast<uint32_t>(LTC));
    if (it != market_priceMap.end())
    {
        std::map<uint32_t, int64_t>& auxMap = it->second;
        auto itt = auxMap.find(cd.collateral_currency);
        if(itt != auxMap.end())
        {
            tokenPrice = itt->second;
        } else {
            if(msc_debug_add_contract_ltc_vol) PrintToLog("%s(): price for collateral (id: %d) in DEx not found \n",__func__, cd.collateral_currency);
            return false;
        }

    } else{
        if (msc_debug_add_contract_ltc_vol) PrintToLog("%s(): DEx without trade history \n",__func__);
        return false;
    }

    if (msc_debug_add_contract_ltc_vol) PrintToLog("%s(): nCouldBuy: %d, notional: %d, tokenPrice: %d \n",__func__, nCouldBuy, cd.notional_size, tokenPrice);
    arith_uint256 globalVolume = (ConvertTo256(nCouldBuy) * ConvertTo256(cd.notional_size) * ConvertTo256(tokenPrice)) / (ConvertTo256(COIN) * ConvertTo256(COIN));

    globalVolumeALL_LTC += ConvertTo64(globalVolume);

    if (msc_debug_add_contract_ltc_vol) PrintToLog("%s(): volume added: %d \n",__func__, ConvertTo64(globalVolume));

    return true;

}

bool mastercore::MetaDEx_Fees(const CMPMetaDEx *pnew,const CMPMetaDEx *pold, int64_t buyer_amountGot)
{
    if(pnew->getBlock() == 0 && pnew->getHash() == uint256() && pnew->getIdx() == 0)
    {
        if(msc_debug_metadex_fees) PrintToLog("%s: Buy from  ContractDex_Fees \n",__func__);
        return false;
    }

    const arith_uint256 uNumerator = ConvertTo256(buyer_amountGot);
    const arith_uint256 uDenominator = ConvertTo256(BASISPOINT) * ConvertTo256(BASISPOINT);
    const arith_uint256 uCacheFee = DivideAndRoundUp(uNumerator, uDenominator);
    const int64_t cacheFee = ConvertTo64(uCacheFee);
    const int64_t takerFee = 5 * cacheFee;
    const int64_t makerFee = 4 * cacheFee;

    if(msc_debug_metadex_fees) PrintToLog("%s: buyer_amountGot: %d, cacheFee: %d, takerFee: %d, makerFee: %d\n",__func__, buyer_amountGot, cacheFee, takerFee, makerFee);
    //sum check
    assert(takerFee == makerFee + cacheFee);

    // checking the same property for both
    assert(pnew->getDesProperty() == pold->getProperty());
    if(msc_debug_metadex_fees) PrintToLog("%s: propertyId: %d\n",__func__,pold->getProperty());

    // -% to taker, +% to maker
    if(cacheFee != 0)
    {
         assert(update_tally_map(pnew->getAddr(), pnew->getDesProperty(), -takerFee, BALANCE));
         assert(update_tally_map(pold->getAddr(), pold->getProperty(), makerFee, BALANCE));
         cachefees[pnew->getProperty()] += cacheFee;
         return true;
    }

    return false;

}

/**
 * Scans the orderbook for an specific metadex order.
 */
 int mastercore::MetaDEx_CANCEL(const uint256& txid, const std::string& sender_addr, unsigned int block, const std::string& hash)
 {
     int rc = METADEX_ERROR -40;
     bool bValid = false;

     for (md_PropertiesMap::iterator my_it = metadex.begin(); my_it != metadex.end(); ++my_it) {
         // uint32_t prop = my_it->first;

         // if(msc_debug_contract_cancel) PrintToLog(" ## property: %d\n", prop);
         md_PricesMap &prices = my_it->second;

         for (md_PricesMap::iterator itt = prices.begin(); itt != prices.end(); ++itt)
         {
             // uint64_t price = it->first;
             md_Set &indexes = itt->second;

             for (md_Set::iterator it = indexes.begin(); it != indexes.end();)
             {

                 if (it->getAddr() != sender_addr ||  it->getAmountForSale() == 0 || (it->getHash()).ToString() != hash) {
                     ++it;
                     continue;
                 }

                 // move from reserve to main
                 assert(update_tally_map(it->getAddr(), it->getProperty(), -it->getAmountRemaining(), METADEX_RESERVE));
                 assert(update_tally_map(it->getAddr(), it->getProperty(), it->getAmountRemaining(), BALANCE));


                 bValid = true;
                 if(msc_debug_contract_cancel) PrintToLog("%s(): order found!\n",__func__);
                 p_txlistdb->recordMetaDExCancelTX(txid, it->getHash(), bValid, block, it->getProperty(), it->getAmountRemaining());
                 indexes.erase(it++);
                 return 0;
             }

         }
     }

     if (!bValid && msc_debug_contract_cancel)
     {
        PrintToLog("%s(): CANCEL IN ORDER: You don't have active orders\n",__func__);
        rc = 1;
     }

     return rc;
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

/*  The metadex of tokens ***
 *  Find the best match on the market
 *  NOTE: sometimes I refer to the older order as seller & the newer order as buyer, in this trade
 *  INPUT: property, desprop, desprice = of the new order being inserted; the new object being processed
 *  RETURN:
 */
MatchReturnType x_Trade(CMPMetaDEx* const pnew)
{
    const uint32_t propertyForSale = pnew->getProperty();
    const uint32_t propertyDesired = pnew->getDesProperty();
    MatchReturnType NewReturn = NOTHING;
    bool bBuyerSatisfied = false;

    if (msc_debug_metadex1) PrintToLog("%s(%s: prop=%d, desprop=%d, desprice= %s);newo: %s\n",
        __FUNCTION__, pnew->getAddr(), propertyForSale, propertyDesired, xToString(pnew->inversePrice()), pnew->ToString());

    md_PricesMap* const ppriceMap = get_Prices(propertyDesired);

    // Nothing for the desired property exists in the market !!
    if (!ppriceMap) {
        PrintToLog("%s()=%d:%s FOUND ON THE MARKET\n", __FUNCTION__, NewReturn, getTradeReturnType(NewReturn));
        return NewReturn;
    }

    // Within the desired property map (given one property) iterate over the items looking at prices
    for (md_PricesMap::iterator priceIt = ppriceMap->begin(); priceIt != ppriceMap->end(); ++priceIt)
    { // check all prices
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
          	// globalNumPrice = 1;
          	// globalDenPrice = 1;
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

          	const int64_t& pnew_forsale = pnew->getAmountForSale();
          	const int64_t& pnew_desired = pnew->getAmountDesired();

          	const int64_t& pold_forsale = pold->getAmountForSale();
          	const int64_t& pold_desired = pold->getAmountDesired();

          	rational_t market_priceratToken1_Token2(pold_desired, pold_forsale);
          	const int64_t market_priceToken1_Token2 = mastercore::RationalToInt64(market_priceratToken1_Token2);
          	market_priceMap[pold->getProperty()][pold->getDesProperty()] = market_priceToken1_Token2;

          	rational_t market_priceratToken2_Token1(pnew_desired, pnew_forsale);
          	const int64_t market_priceToken2_Token1 = mastercore::RationalToInt64(market_priceratToken2_Token1);
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
          	const int64_t numVWAPMapToken1_Token2_64t = mastercore::ConvertTo64(numVWAPMapToken1_Token2_256t);

          	arith_uint256 numVWAPMapToken2_Token1_256t =
          	  mastercore::ConvertTo256(market_priceToken2_Token1)*mastercore::ConvertTo256(seller_amountGot)/COIN;
          	const int64_t numVWAPMapToken2_Token1_64t = mastercore::ConvertTo64(numVWAPMapToken2_Token1_256t);

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
          	const int64_t vwapPriceToken1_Token2Int64V = mastercore::RationalToInt64(vwapPriceToken1_Token2RatV);

          	rational_t vwapPriceToken2_Token1RatV(numVWAPpnew, denVWAPpnew);
          	const int64_t vwapPriceToken2_Token1Int64V = mastercore::RationalToInt64(vwapPriceToken2_Token1RatV);

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
            // Adding token volume into Map
            metavolume[pnew->getBlock()][pnew->getProperty()] = seller_amountGot;
            metavolume[pnew->getBlock()][pnew->getDesProperty()] = buyer_amountGot;

          	/***********************************************************************************************/
            // Adding volume in termos of LTC
            increaseLTCVolume(pnew->getProperty(), pnew->getDesProperty(), pnew->getBlock());
            /***********************************************************************************************/
          	const int64_t& buyer_amountGotAfterFee = buyer_amountGot;
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


int64_t mastercore::getVWAPPriceContracts(const std::string& namec)
{
    LOCK(cs_register);
    const uint32_t& nextCDID = _my_cds->peekNextContractID();

    for (uint32_t contractId = 1; contractId < nextCDID; contractId++)
    {
        CDInfo::Entry cd;
        if (_my_cds->getCD(contractId, cd) && cd.name == namec)
	      {
	          PrintToLog("\npropertyId num: %d\n", contractId);
            PrintToLog("\nVWAPMapContracts[contractId] = %d\n", FormatDivisibleMP(VWAPMapContracts[contractId]));
            return VWAPMapContracts[contractId];
	      }
    }

    return 0;
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
    multiply(fullprice, priceForsale, amountForsale);

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
    // PrintToLog("update remaining amount still up for sale (%ld %s):%s\n", amount, label, ToString());
}


void CMPMetaDEx::setAmountForsale(int64_t amount, const std::string& label)
{
    amount_forsale = amount;
    // PrintToLog("update remaining amount still up for sale (%ld %s):%s\n", amount, label, ToString());
}

void CMPContractDex::setPrice(int64_t price)
{
    effective_price = price;
    // PrintToLog("update price still up for sale (%ld):%s\n", price, ToString());
}

bool CMPContractDex::updateAmountReserved(int64_t amount)
{
    if (isOverflow(amount_reserved, amount))
    {
        PrintToLog("%s(): ERROR: arithmetic overflow [%d + %d]\n", __func__, amount_reserved, amount);
        return false;
    }

    amount_reserved += amount;

    return true;
}


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

void CMPMetaDEx::saveOffer(std::ofstream& file, CHash256& hasher) const
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
    hasher.Write((unsigned char*)lineOut.c_str(), lineOut.length());

    // write the line
    file << lineOut << std::endl;
}


void CMPContractDex::saveOffer(std::ofstream& file, CHash256& hasher) const
{
    std::string lineOut = strprintf("%s,%d,%d,%d,%d,%d,%d,%d,%s,%d,%d,%d,%d",
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
        trading_action,
        amount_reserved
    );

    // add the line to the hash
    hasher.Write((unsigned char*)lineOut.c_str(), lineOut.length());

    // write the line
    file << lineOut << std::endl;
}


// Generates a consensus string for hashing based on a MetaDEx object
std::string CMPMetaDEx::GenerateConsensusString() const
{
   return strprintf("%s|%s|%d|%d|%d|%d|%d",
		    getHash().GetHex(), getAddr(), getProperty(), getAmountForSale(),
		    getDesProperty(), getAmountDesired(), getAmountRemaining());
}

// Generates a consensus string for hashing based on a ContractDex object
std::string CMPContractDex::GenerateConsensusString() const
{
    return strprintf("%s|%s|%d|%d|%d|%d|%d|%d",
         getHash().GetHex(), getAddr(), getProperty(), getAmountForSale(),
         getAmountRemaining(), getEffectivePrice(), getTradingAction(), getAmountReserved());
}




void saveDataGraphs(std::fstream &file, std::string lineOutSixth1, std::string lineOutSixth2, std::string lineOutSixth3, bool savedata_bool)
{
    std::string lineSixth1 = lineOutSixth1;
    std::string lineSixth2 = lineOutSixth2;
    std::string lineSixth3 = lineOutSixth3;

    if (savedata_bool)
    {
          file << lineSixth1 << "\n";
          file << lineSixth2 << std::endl;
    } else {
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
    md_Set *p_indexes = nullptr;

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
    cd_Set *p_indexes = nullptr;

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
    int rc = 0;
    // Create a MetaDEx object from paremeters
    CMPMetaDEx new_mdex(sender_addr, block, prop, amount, property_desired, amount_desired, txid, idx, CMPTransaction::ADD);
    if (msc_debug_metadex_add) PrintToLog("%s(); buyer obj: %s\n", __FUNCTION__, new_mdex.ToString());

    // Ensure this is not a badly priced trade (for example due to zero amounts)
    if (0 == new_mdex.unitPrice()) return METADEX_ERROR -66;

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

    return rc;
}

int mastercore::ContractDex_ADD(const std::string& sender_addr, uint32_t prop, int64_t amount, int block, const uint256& txid, unsigned int idx, uint64_t effective_price, uint8_t trading_action, int64_t amountToReserve)
{
    int rc = 0;

    /*Remember: Here CMPTransaction::ADD is the subaction coming from CMPMetaDEx*/
    CMPContractDex new_cdex(sender_addr, block, prop, amount, 0, 0, txid, idx, CMPTransaction::ADD, effective_price, trading_action, amountToReserve, false);

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

     return rc;
}

int mastercore::ContractDex_ADD_MARKET_PRICE(const std::string& sender_addr, uint32_t contractId, int64_t amount, int block, const uint256& txid, unsigned int idx, uint8_t trading_action, int64_t amount_to_reserve, bool liquidation)
{

    int rc = METADEX_ERROR -1;
    // const int MAX = 10; // max number of orders to match at market price in the other side (depth)

    if(trading_action != buy && trading_action != sell){
        PrintToLog("%s(): ERROR: invalid trading action: %d\n",__func__, trading_action);
        return -1;
    }

    uint64_t edge = edgeOrderbook(contractId, trading_action);
    CMPContractDex new_cdex(sender_addr, block, contractId, amount, 0, 0, txid, idx, CMPTransaction::ADD, edge, trading_action, 0, liquidation);
    if(msc_debug_contract_add_market) PrintToLog("%s(): effective price of new_cdex : %d, edge price : %d, trading_action: %d\n",__func__, new_cdex.getEffectivePrice(), edge, trading_action);
    if (0 >= new_cdex.getEffectivePrice()) return METADEX_ERROR -66;

    x_Trade(&new_cdex);

    if (new_cdex.getAmountForSale() > 0)
    {
        if (!ContractDex_INSERT(new_cdex))
        {
            PrintToLog("%s() ERROR: ALREADY EXISTS, line %d, file: %s\n", __FUNCTION__, __LINE__, __FILE__);
            return CONTRACTDEX_ERROR -70;
        }
    }

    if(msc_debug_contract_add_market) PrintToLog("%s(): final amount in order : %d, contractId: %d\n",__func__, new_cdex.getAmountForSale(), contractId);

    return rc;
}

int mastercore::ContractDex_CANCEL_EVERYTHING(const uint256& txid, unsigned int block, const std::string& sender_addr, uint32_t contractId)
{
    int rc = METADEX_ERROR -40;
    bool bValid = false;

    for (cd_PropertiesMap::iterator my_it = contractdex.begin(); my_it != contractdex.end(); ++my_it)
    {
        uint32_t prop = my_it->first;

        if (msc_debug_contract_cancel_every) PrintToLog(" ## property: %d\n", prop);
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
	              if (msc_debug_contract_cancel_every) PrintToLog("%s(): REMOVING %s\n", __func__, it->ToString());

	              CDInfo::Entry cd;
	              assert(_my_cds->getCD(it->getProperty(), cd));

	              uint32_t collateralCurrency = cd.collateral_currency;

	              string addr = it->getAddr();
                int64_t redeemed = it->getAmountReserved();
	              int64_t amountForSale = it->getAmountForSale();
                int64_t amountRemaining = it->getAmountRemaining();
                // int64_t balance = getMPbalance(addr,collateralCurrency,BALANCE);

                if (msc_debug_contract_cancel_every)
                {
    	              PrintToLog("collateral currency id of contract : %d\n",collateralCurrency);
    	              PrintToLog("amountForSale: %d\n",amountForSale);
                    PrintToLog("amountRemaining: %d\n",amountRemaining);
    	              PrintToLog("Address: %s\n",addr);
    	              PrintToLog("--------------------------------------------\n");
                }


                const int64_t orderReserve = getMPbalance(addr, collateralCurrency, CONTRACTDEX_RESERVE);
                const int64_t newRedeemed = (redeemed <= orderReserve) ? redeemed : orderReserve;

	              // move from reserve to balance the collateral
	              if (0 < newRedeemed)
			          {
	                  assert(update_tally_map(addr, collateralCurrency, newRedeemed, BALANCE));
	                  assert(update_tally_map(addr, collateralCurrency, -newRedeemed, CONTRACTDEX_RESERVE));
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

	              CDInfo::Entry cd;
	              uint32_t contractId = it->getProperty();
                int64_t redeemed = it->getAmountReserved();
	              assert(_my_cds->getCD(contractId, cd));

	              uint32_t collateralCurrency = cd.collateral_currency;
	              // int64_t balance = getMPbalance(addr,collateralCurrency,BALANCE);
	              int64_t amountForSale = it->getAmountForSale();
                if(msc_debug_contract_cancel_forblock)
                {

    	              PrintToLog("collateral currency id of contract : %d\n", collateralCurrency);
    	              PrintToLog("amountForSale: %d\n", amountForSale);
    	              PrintToLog("Address: %d\n", addr);
                    // PrintToLog("balance in collateral: %d\n", balance);
                }


                const int64_t orderReserve = getMPbalance(addr, collateralCurrency, CONTRACTDEX_RESERVE);
                const int64_t newRedeemed = (redeemed <= orderReserve) ? redeemed : orderReserve;

	              // std::string sgetback = FormatDivisibleMP(redeemed, false);

	              if(msc_debug_contract_cancel_forblock) PrintToLog("amount returned to balance: %d\n", redeemed);

	              // move from reserve to balance the collateral
	              if (0 < newRedeemed)
			          {
			              assert(update_tally_map(addr, collateralCurrency, newRedeemed, BALANCE));
	                  assert(update_tally_map(addr, collateralCurrency,  -newRedeemed, CONTRACTDEX_RESERVE));
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

    CDInfo::Entry cd;
    if(!_my_cds->getCD(contractId, cd))
        return rc;

    uint32_t collateralCurrency = cd.collateral_currency;

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
                int64_t redeemed = it->getAmountReserved();
                int64_t amountForSale = it->getAmountForSale();
                // int64_t balance = getMPbalance(addr,collateralCurrency,BALANCE);

                if(msc_debug_contract_cancel_inorder)
                {
                    PrintToLog("collateral currency id of contract : %d\n",collateralCurrency);
                    PrintToLog("amountForSale: %d\n",amountForSale);
                    PrintToLog("Address: %d\n",addr);
                }

                if(msc_debug_contract_cancel_inorder) PrintToLog("redeemed: %d\n",redeemed);


                const int64_t orderReserve = getMPbalance(addr, collateralCurrency, CONTRACTDEX_RESERVE);
                const int64_t newRedeemed = (redeemed <= orderReserve) ? redeemed : orderReserve;

                // move from reserve to balance the collateral
                if (0 < newRedeemed) {
                    assert(update_tally_map(addr, collateralCurrency, newRedeemed, BALANCE));
                    assert(update_tally_map(addr, collateralCurrency, -newRedeemed, CONTRACTDEX_RESERVE));
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

int mastercore::ContractDex_ADD_ORDERBOOK_EDGE(const std::string& sender_addr, uint32_t contractId, int64_t amount, int block, const uint256& txid, unsigned int idx, uint8_t trading_action, int64_t amount_to_reserve, bool liquidation)
{
    int rc = METADEX_ERROR -1;
    uint64_t price;

    price = (trading_action == buy) ? edgeOrderbook(contractId, sell) : edgeOrderbook(contractId, buy);
    if (msc_debug_add_orderbook_edge) PrintToLog("price of edge: %d\n",price);

    if (price != 0)
    {
        CMPContractDex new_cdex(sender_addr, block, contractId, amount, 0, 0, txid, idx, CMPTransaction::ADD, price, trading_action, 0, liquidation);
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
int mastercore::ContractDex_CLOSE_POSITION(const uint256& txid, unsigned int block, const std::string& sender_addr, uint32_t contractId, uint32_t collateralCurrency, bool liquidation)
{
    int rc = -1;

    const int64_t contractBalance = getContractRecord(sender_addr,contractId, CONTRACT_POSITION);

    if(msc_debug_close_position) PrintToLog("%s(): position before: %d\n",__func__, contractBalance);

    if (contractBalance == 0) {
        return rc;
    }

    // Clearing the position
    unsigned int idx = 0;
    std::pair <int64_t, uint8_t> result;
    (contractBalance > 0) ? (result.first = contractBalance, result.second = sell) : (result.first = -contractBalance, result.second = buy);

    if(msc_debug_close_position) PrintToLog("%s(): result.first: %d, result.second: %d\n",__func__, result.first, result.second);

    rc = ContractDex_ADD_MARKET_PRICE(sender_addr, contractId, result.first, block, txid, idx, result.second, 0, liquidation);

    const int64_t positionAf = getContractRecord(sender_addr, contractId, CONTRACT_POSITION);
    if(msc_debug_close_position) PrintToLog("%s(): position after: %d, rc: %d\n",__func__, positionAf, rc);

    if (positionAf == 0)
    {
        if(msc_debug_close_position) PrintToLog("%s(): POSITION CLOSED!!!\n",__func__);
        // releasing the reserve
        // const int64_t reserve = getMPbalance(sender_addr, collateralCurrency, CONTRACTDEX_RESERVE);
        // assert(update_tally_map(sender_addr, collateralCurrency, reserve, BALANCE));
        // assert(update_tally_map(sender_addr, collateralCurrency, -reserve, CONTRACTDEX_RESERVE));

    } else{
      if(msc_debug_close_position) PrintToLog("%s(): Position partialy Closed\n", __func__);
    }


    return rc;
}

uint64_t mastercore::edgeOrderbook(uint32_t contractId, uint8_t tradingAction)
{
    uint64_t candPrice = (tradingAction == buy) ?  std::numeric_limits<uint64_t>::max() : 0;

    cd_PricesMap* const ppriceMap = get_PricesCd(contractId); // checking the ask price of contract A
    for (cd_PricesMap::iterator it = ppriceMap->begin(); it != ppriceMap->end(); ++it)
    {
        const cd_Set& indexes = it->second;
        for (cd_Set::const_iterator it = indexes.begin(); it != indexes.end(); ++it)
        {
            const CMPContractDex& obj = *it;
            if (obj.getTradingAction() == tradingAction || obj.getAmountForSale() == 0) continue;
                uint64_t price = obj.getEffectivePrice();
            if ((tradingAction == buy && price < candPrice) || (tradingAction == sell && price > candPrice))
                candPrice = price;
            if(msc_debug_sp) PrintToLog("%s(): choosen price: %d\n",__func__, price);
        }
    }

    // just for testing
    if (candPrice == 0 && RegTest()) candPrice = 1000 * COIN;


    return candPrice;
}

bool mastercore::MetaDEx_Search_ALL(uint64_t& amount, uint32_t propertyOffered)
{
    bool bBuyerSatisfied = false;

    for (md_PropertiesMap::iterator my_it = metadex.begin(); my_it != metadex.end(); ++my_it)
    {
        md_PricesMap &prices = my_it->second;

        for (md_PricesMap::iterator itt = prices.begin(); itt != prices.end(); ++itt)
        {
            md_Set &indexes = itt->second;

            for (md_Set::iterator it = indexes.begin(); it != indexes.end();)
            {

	              if (it->getProperty() != ALL || it->getDesProperty() != propertyOffered)
                {
	                  ++it;
	                  continue;
	              }

	              if (msc_debug_search_all) PrintToLog("%s(): ALLS FOUND! %s\n", __func__, it->ToString());

                if (msc_debug_search_all) PrintToLog("%s(): amount of ALL: %d, desproperty: %d; amount of desproperty: %d\n", __func__, it->getAmountForSale(), it->getDesProperty(), it->getAmountDesired());

                // amount of All to buy
                const arith_uint256 iCouldBuy = (ConvertTo256(amount) * ConvertTo256(it->getAmountForSale())) / ConvertTo256(it->getAmountDesired());

                int64_t nCouldBuy = 0;

                if (iCouldBuy < ConvertTo256(it->getAmountRemaining())) {
                  nCouldBuy = ConvertTo64(iCouldBuy);
                  PrintToLog("%s(): nCouldBuy < it->getAmountRemaining()\n", __func__);
                } else {
                  nCouldBuy = it->getAmountRemaining();
                }

                // amount of tokens to pay
                const arith_uint256 iWouldPay = DivideAndRoundUp((ConvertTo256(nCouldBuy) * ConvertTo256(it->getAmountDesired())), ConvertTo256(it->getAmountForSale()));
                int64_t nWouldPay = ConvertTo64(iWouldPay);

                if (msc_debug_search_all) PrintToLog("%s(): nWouldPay: %d\n", __func__, nWouldPay);

                if (msc_debug_search_all) PrintToLog("%s(): nCouldBuy: %d\n", __func__, nCouldBuy);

                if (nCouldBuy == 0)
                {
                    if (msc_debug_search_all) PrintToLog("-- buyer has not enough tokens for sale to purchase one unit!\n");
                    ++it;
                    continue;
                }

                // preconditions
                assert(0 < it->getAmountRemaining());
                assert(0 < amount);

                // taking ALLs from seller
                assert(update_tally_map(it->getAddr(), it->getProperty(), -nCouldBuy, METADEX_RESERVE));
                cachefees_oracles[ALL] = nCouldBuy;

                // giving the tokens from cache
                assert(update_tally_map(it->getAddr(), it->getDesProperty(), nWouldPay, BALANCE));

                const int64_t seller_amountLeft = it->getAmountForSale() - nCouldBuy;

                // postconditions
                if (msc_debug_search_all) PrintToLog("%s(): amountForSale : %d, nCouldBuy : %d, seller_amountLeft: %d\n",__func__, it->getAmountForSale(), nCouldBuy, seller_amountLeft);
                assert( it->getAmountForSale() == nCouldBuy + seller_amountLeft);

                // discounting the amount
                amount -= static_cast<uint64_t>(nCouldBuy);

                CMPMetaDEx seller_replacement = *it;
                seller_replacement.setAmountRemaining(seller_amountLeft, "seller_replacement");

                // erase the old seller element
                indexes.erase(it++);

                // insert the updated one in place of the old
                if (0 < seller_replacement.getAmountRemaining())
                {
                    if (msc_debug_search_all) PrintToLog("++ inserting seller_replacement: %s\n", seller_replacement.ToString());
                    indexes.insert(seller_replacement);
                }

            }

            if (amount == 0)
            {
                bBuyerSatisfied = true;
                break;
            }

        }

        if (amount == 0)
        {
            bBuyerSatisfied = true;
            break;
        }
    }

    return bBuyerSatisfied;
}

/**
 * Scans the orderbook and remove everything for an address.
 */
int mastercore::MetaDEx_CANCEL_EVERYTHING(const uint256& txid, unsigned int block, const std::string& sender_addr)
{
    int rc = METADEX_ERROR -40;

    for (md_PropertiesMap::iterator my_it = metadex.begin(); my_it != metadex.end(); ++my_it) {
        unsigned int prop = my_it->first;

        if (msc_debug_metadex2) PrintToLog(" ## property: %u\n", prop);
        md_PricesMap& prices = my_it->second;

        for (md_PricesMap::iterator itt = prices.begin(); itt != prices.end(); ++itt) {
            rational_t price = itt->first;
            md_Set& indexes = itt->second;

            if (msc_debug_metadex2) PrintToLog("  # Price Level: %s\n", xToString(price));

            for (md_Set::iterator it = indexes.begin(); it != indexes.end();) {
                PrintToLog("%s= %s\n", xToString(price), it->ToString());

                if (it->getAddr() != sender_addr) {
                    ++it;
                    continue;
                }

                rc = 0;
                // move from reserve to balance
                assert(update_tally_map(it->getAddr(), it->getProperty(), -it->getAmountRemaining(), METADEX_RESERVE));
                assert(update_tally_map(it->getAddr(), it->getProperty(), it->getAmountRemaining(), BALANCE));

                //record the cancellation
                bool bValid = true;
                p_txlistdb->recordMetaDExCancelTX(txid, it->getHash(), bValid, block, it->getProperty(), it->getAmountRemaining());

                indexes.erase(it++);
            }
        }
    }

    return rc;
}

int mastercore::MetaDEx_CANCEL_AT_PRICE(const uint256& txid, unsigned int block, const std::string& sender_addr, uint32_t prop, int64_t amount, uint32_t property_desired, int64_t amount_desired)
{
    int rc = METADEX_ERROR -20;
    CMPMetaDEx mdex(sender_addr, 0, prop, amount, property_desired, amount_desired, uint256(), 0, CMPTransaction::CANCEL_AT_PRICE);
    md_PricesMap* prices = get_Prices(prop);
    const CMPMetaDEx* p_mdex = nullptr;

    if (!prices) {
        PrintToLog("%s() NOTHING FOUND for %s\n", __func__, mdex.ToString());
        return rc -1;
    }

    // within the desired property map (given one property) iterate over the items
    for (md_PricesMap::iterator my_it = prices->begin(); my_it != prices->end(); ++my_it) {
        rational_t sellers_price = my_it->first;

        if (mdex.unitPrice() != sellers_price) continue;

        md_Set* indexes = &(my_it->second);

        for (md_Set::iterator iitt = indexes->begin(); iitt != indexes->end();) {
            p_mdex = &(*iitt);

            if (msc_debug_metadex3) PrintToLog("%s(): %s\n", __func__, p_mdex->ToString());

            if ((p_mdex->getDesProperty() != property_desired) || (p_mdex->getAddr() != sender_addr)) {
                ++iitt;
                continue;
            }

            rc = 0;

            PrintToLog("%s(): REMOVING %s\n", __func__, p_mdex->ToString());

            // move from reserve to main
            assert(update_tally_map(p_mdex->getAddr(), p_mdex->getProperty(), -p_mdex->getAmountRemaining(), METADEX_RESERVE));
            assert(update_tally_map(p_mdex->getAddr(), p_mdex->getProperty(), p_mdex->getAmountRemaining(), BALANCE));

            // record the cancellation
            bool bValid = true;
            p_txlistdb->recordMetaDExCancelTX(txid, p_mdex->getHash(), bValid, block, p_mdex->getProperty(), p_mdex->getAmountRemaining());

            indexes->erase(iitt++);
        }
    }

    return rc;
}

int mastercore::MetaDEx_CANCEL_ALL_FOR_PAIR(const uint256& txid, unsigned int block, const std::string& sender_addr, uint32_t prop, uint32_t property_desired)
{
    int rc = METADEX_ERROR -30;
    md_PricesMap* prices = get_Prices(prop);
    const CMPMetaDEx* p_mdex = nullptr;

    if (!prices) {
        PrintToLog("%s() NOTHING FOUND\n", __FUNCTION__);
        return rc -1;
    }

    // within the desired property map (given one property) iterate over the items
    for (md_PricesMap::iterator my_it = prices->begin(); my_it != prices->end(); ++my_it) {
        md_Set* indexes = &(my_it->second);

        for (md_Set::iterator iitt = indexes->begin(); iitt != indexes->end();) {
            p_mdex = &(*iitt);

            if (msc_debug_metadex3) PrintToLog("%s(): %s\n", __FUNCTION__, p_mdex->ToString());

            if ((p_mdex->getDesProperty() != property_desired) || (p_mdex->getAddr() != sender_addr)) {
                ++iitt;
                continue;
            }

            rc = 0;
            PrintToLog("%s(): REMOVING %s\n", __FUNCTION__, p_mdex->ToString());

            // move from reserve to balance
            assert(update_tally_map(p_mdex->getAddr(), p_mdex->getProperty(), -p_mdex->getAmountRemaining(), METADEX_RESERVE));
            assert(update_tally_map(p_mdex->getAddr(), p_mdex->getProperty(), p_mdex->getAmountRemaining(), BALANCE));

            // record the cancellation
            bool bValid = true;
            p_txlistdb->recordMetaDExCancelTX(txid, p_mdex->getHash(), bValid, block, p_mdex->getProperty(), p_mdex->getAmountRemaining());

            indexes->erase(iitt++);
        }
    }

    return rc;
}

/**
 * Scans the orderbook for an specific contract order.
 */
 int mastercore::ContractDex_CANCEL(const std::string& sender_addr, const std::string& hash)
 {
     int rc = METADEX_ERROR -40;
     bool bValid = false;

     for (cd_PropertiesMap::iterator my_it = contractdex.begin(); my_it != contractdex.end(); ++my_it) {
         uint32_t prop = my_it->first;

         if(msc_debug_contract_cancel) PrintToLog(" ## property: %d\n", prop);
         cd_PricesMap &prices = my_it->second;

         for (cd_PricesMap::iterator itt = prices.begin(); itt != prices.end(); ++itt) {
             // uint64_t price = it->first;
             cd_Set &indexes = itt->second;

             for (cd_Set::iterator it = indexes.begin(); it != indexes.end();) {

                 if(msc_debug_contract_cancel)
                 {
                     std::string getstring = (it->getHash()).ToString();
                     PrintToLog("getAddr: %s\n",it->getAddr());
                     PrintToLog("address: %s\n",sender_addr);
                     PrintToLog("propertyid: %d\n",it->getProperty());
                     PrintToLog("amount for sale: %d\n",it->getAmountForSale());
                     PrintToLog("hash: %s\n",hash);
                     PrintToLog("getHash: %s\n",getstring);
                 }

                 if (it->getAddr() != sender_addr ||  it->getAmountForSale() == 0 || (it->getHash()).ToString() != hash) {
                     ++it;
                     continue;
                 }

                 string addr = it->getAddr();
                 int64_t redeemed = it->getAmountReserved();
                 int64_t amountForSale = it->getAmountForSale();
                 int64_t amountRemaining = it->getAmountRemaining();
                 uint32_t contractId = it->getProperty();

                 CDInfo::Entry cd;
                 if(!_my_cds->getCD(contractId, cd))
                     return rc;

                 uint32_t collateralCurrency = cd.collateral_currency;

                 if(msc_debug_contract_cancel)
                 {

                     PrintToLog("collateral currency id of contract : %d\n", collateralCurrency);
                     PrintToLog("amountForSale: %d\n",amountForSale);
                     PrintToLog("amountRemaining: %d\n",amountRemaining);
                     PrintToLog("Address: %s\n",addr);
                 }

                 if(msc_debug_contract_cancel) PrintToLog("redeemed: %d\n",redeemed);

                 const int64_t orderReserve = getMPbalance(addr, collateralCurrency, CONTRACTDEX_RESERVE);
                 const int64_t newRedeemed = (redeemed <= orderReserve) ? redeemed : orderReserve;

                 // move from reserve to balance the collateral
                 if (0 < newRedeemed) {
                     assert(update_tally_map(addr, collateralCurrency, newRedeemed, BALANCE));
                     assert(update_tally_map(addr, collateralCurrency, -newRedeemed, CONTRACTDEX_RESERVE));
                 }

                 bValid = true;
                 if(msc_debug_contract_cancel) PrintToLog("%s(): order found!\n",__func__);
                 // p_txlistdb->recordContractDexCancelTX(txid, it->getHash(), bValid, block, it->getProperty(), it->getAmountForSale
                 indexes.erase(it++);
                 rc = 0;
                 return rc;
             }

         }
     }

     if (!bValid && msc_debug_contract_cancel)
     {
        PrintToLog("%s(): CANCEL IN ORDER: You don't have active orders\n",__func__);
        rc = 1;
     }

     return rc;
 }


 /**
  * Scans the orderbook for liquidation orders
  */
  bool mastercore::ContractDex_LIQUIDATION_VOLUME(uint32_t contractId, int64_t& volume, int64_t& vwap, int64_t& bankruptcyVWAP, bool& sign)
  {
      bool bValid{false};
      arith_uint256 iVWAP = 0;
      arith_uint256 iBankrupcyVWAP = 0;

      for (cd_PropertiesMap::iterator my_it = contractdex.begin(); my_it != contractdex.end(); ++my_it) {
          uint32_t prop = my_it->first;

          if (prop != contractId) continue;

          PrintToLog(" ## contractId: %d\n", prop);
          cd_PricesMap &prices = my_it->second;

          for (cd_PricesMap::iterator it = prices.begin(); it != prices.end(); ++it)
          {
              const uint64_t& price = it->first;
              PrintToLog(" ## price: %d\n", price);
              cd_Set &indexes = it->second;

              for (cd_Set::iterator itt = indexes.begin(); itt != indexes.end();)
              {
                  if (itt->isLiquidationOrder()) PrintToLog("%s(): YOU HAVE LIQUIDATION ORDERS HERE\n",__func__);

                  if (!itt->isLiquidationOrder() || 0 == itt->getAmountForSale()) {
                      ++itt;
                      continue;
                  }

                  // const std::string getstring = (itt->getHash()).ToString();
                  PrintToLog("%s(): getAddr: %s\n",__func__, itt->getAddr());
                  PrintToLog("%s(): propertyid: %d\n",__func__, itt->getProperty());
                  PrintToLog("%s(): amount for sale: %d\n",__func__, itt->getAmountForSale());
                  PrintToLog("%s(): price: %d\n",__func__, itt->getEffectivePrice());
                  // PrintToLog("%s(): getHash: %s\n",__func__, getstring);
                  volume += itt->getAmountForSale();
                  iVWAP += ConvertTo256(itt->getAmountForSale() * itt->getEffectivePrice());

                  // bankruptcyVWAP calculations
                  int64_t bankruptcyPrice = getContractRecord(itt->getAddr(), contractId, BANKRUPTCY_PRICE);
                  iBankrupcyVWAP += ConvertTo256(itt->getAmountForSale() * bankruptcyPrice);

                  const int64_t position = getContractRecord(itt->getAddr(), contractId, CONTRACT_POSITION);

                  // sign of liquidation orders
                  if (!bValid && 0 < position) {
                      sign = true;
                  }

                  // deleting small orders (later it's gonna add a big one)
                  indexes.erase(itt++);


                  bValid = true;
              }

          }
      }

      if (!bValid)
      {
         PrintToLog("%s(): You DON'T have LIQUIDATION ORDERS\n",__func__);
      } else {
           iVWAP /= ConvertTo256(volume);
           iBankrupcyVWAP /= ConvertTo256(volume);
           vwap = ConvertTo64(iVWAP);
           bankruptcyVWAP = ConvertTo64(iBankrupcyVWAP);
           PrintToLog("%s(): final vwap: %d, final volume: %d, bankruptcyVWAP: %d\n",__func__, vwap, volume, bankruptcyVWAP);
      }


      return bValid;
}

bool mastercore::checkReserve(const std::string& address, int64_t amount, uint32_t propertyId, int64_t& nBalance)
{
    arith_uint256 am = ConvertTo256(amount) + DivideAndRoundUp(ConvertTo256(amount) * ConvertTo256(5), ConvertTo256(BASISPOINT) * ConvertTo256(BASISPOINT));
    int64_t amountToReserve = ConvertTo64(am);

    nBalance = getMPbalance(address, propertyId, BALANCE);

    return (nBalance < amountToReserve || nBalance == 0) ? false : true;
}


int64_t mastercore::ContractBasisPoints(const CDInfo::Entry& cd, int64_t amount, int64_t leverage)
{
    //NOTE: add changes to inverse quoted
    // const int64_t uPrice = COIN;
    std::pair<int64_t, int64_t> factor;

    const arith_uint256 aLeverage = ConvertTo256(leverage);
    const arith_uint256 aPositionRequirement = ConvertTo256(amount) * ConvertTo256(cd.margin_requirement);

    // max = 2.5 basis point in oracles, max = 1.0 basis point in natives
    (cd.isOracle()) ? (factor.first = 100025, factor.second = 100000) : (cd.isNative()) ? (factor.first = 10001, factor.second = 10000) : (factor.first = 1, factor.second = 1);

    const arith_uint256 aFee = ConvertTo256(factor.first) * (aPositionRequirement /  ConvertTo256(factor.second));
    const arith_uint256 aRealRequirement = DivideAndRoundUp(aPositionRequirement, aLeverage);

    // total amount required
    return (ConvertTo64(aFee) + ConvertTo64(aRealRequirement));

}

bool mastercore::checkContractReserve(const std::string& address, int64_t amount, uint32_t contractId, int64_t leverage, int64_t& nBalance, int64_t& amountToReserve)
{
    CDInfo::Entry cd;
    assert(_my_cds->getCD(contractId, cd));

    amountToReserve = ContractBasisPoints(cd, amount, leverage);

    PrintToLog("%s(): amountToReserve: %d\n",__func__, amountToReserve);

    nBalance = getMPbalance(address, cd.collateral_currency, BALANCE);

    return (nBalance < amountToReserve || nBalance == 0) ? false : true;

 }

// auxiliar function
void addLives(int64_t& totalLongs, int64_t& totalShorts, int32_t contractId, const Register& reg){
    int64_t position = reg.getRecord(contractId, CONTRACT_POSITION);
    (position > 0) ? totalLongs += position : totalShorts += position;

}

 // counts the number of all contracts in every position
int64_t mastercore::getTotalLives(uint32_t contractId)
{
    int64_t totalLongs = 0;
    int64_t totalShorts = 0;

    LOCK(cs_register);

    // for each tally account, we sum just contracts (all shorts, or all longs)
    for_each(mp_register_map.begin(), mp_register_map.end(), [&totalLongs, &totalShorts, contractId](const std::pair<std::string, Register>& elem){ addLives(totalLongs, totalShorts, contractId, elem.second); });

    if(msc_debug_get_total_lives) PrintToLog("%s(): totalLongs : %d, totalShorts : %d\n",__func__, totalLongs, totalShorts);

    assert(totalLongs == -totalShorts);

    return totalLongs;
 }
