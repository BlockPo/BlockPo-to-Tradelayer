#include "omnicore/mdex.h"

#include "omnicore/errors.h"
// #include "omnicore/fees.h"
#include "omnicore/log.h"
#include "omnicore/omnicore.h"
#include "omnicore/rules.h"
#include "omnicore/sp.h"
#include "omnicore/tx.h"
#include "omnicore/uint256_extensions.h"

#include "arith_uint256.h"
#include "chain.h"
#include "main.h"
#include "tinyformat.h"
#include "uint256.h"

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

md_PricesMap* mastercore::get_Prices(uint32_t prop)
{
    md_PropertiesMap::iterator it = metadex.find(prop);

    if (it != metadex.end()) return &(it->second);

    return (md_PricesMap*) NULL;
}

md_Set* mastercore::get_Indexes(md_PricesMap* p, rational_t price)
{
    md_PricesMap::iterator it = p->find(price);

    if (it != p->end()) return &(it->second);

    return (md_Set*) NULL;
}

///////////////////////////////////////////
/** New things for Contracts */
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

static const std::string getTradeReturnType(MatchReturnType ret)
{
    switch (ret) {
        case NOTHING: return "NOTHING";
        case TRADED: return "TRADED";
        case TRADED_MOREINSELLER: return "TRADED_MOREINSELLER";
        case TRADED_MOREINBUYER: return "TRADED_MOREINBUYER";
        case ADDED: return "ADDED";
        case CANCELLED: return "CANCELLED";
        default: return "* unknown *";
    }
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
    if (rangeInt64(value)) {
        int64_t num = value.numerator().convert_to<int64_t>();
        int64_t denom = value.denominator().convert_to<int64_t>();
        dec_float x = dec_float(num) / dec_float(denom);
        return xToString(x);
    } else {
        return strprintf("%s / %s", xToString(value.numerator()), xToString(value.denominator()));
    }
}

///////////////////////////////
/** New things for Contracts */
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
///////////////////////////////

// find the best match on the market
// NOTE: sometimes I refer to the older order as seller & the newer order as buyer, in this trade
// INPUT: property, desprop, desprice = of the new order being inserted; the new object being processed
// RETURN:


///////////////////////////////////////
/** New things for Contracts */
MatchReturnType x_Trade(CMPContractDex* const pnew)
{
    ///////////////////////////////
    /*New things for Contracts*/
    extern uint32_t notionalSize;
    extern uint32_t marginRequirementContract;
    extern uint32_t collateralCurrency;
    extern volatile uint64_t marketPrice;
    ///////////////////////////////

    // PrintToConsole("________________________________________\n");
    // PrintToConsole("Checking the margin requirement and notional size\n");
    // PrintToConsole("Margin requirement: %d, Notional size: %d\n", marginRequirementContract, notionalSize);
    // PrintToConsole("________________________________________\n");

    const uint32_t propertyForSale = pnew->getProperty();
    MatchReturnType NewReturn = NOTHING;
    bool bBuyerSatisfied = false;

    if (msc_debug_metadex1) PrintToLog("%s(%s: prop=%d, desprice= %s);newo: %s\n", __FUNCTION__, pnew->getAddr(),
                                       propertyForSale, xToString(pnew->getEffectivePrice()), pnew->ToString());

    cd_PricesMap* const ppriceMap = get_PricesCd(propertyForSale);

    // nothing for the desired property exists in the market, sorry!
    if (!ppriceMap) {
        PrintToLog("%s()=%d:%s NOT FOUND ON THE MARKET\n", __FUNCTION__, NewReturn, getTradeReturnType(NewReturn));
        return NewReturn;
    }

    // within the desired property map (given one property) iterate over the items looking at prices
    for (cd_PricesMap::iterator priceIt = ppriceMap->begin(); priceIt != ppriceMap->end(); ++priceIt) { // check all prices

        const uint64_t sellersPrice = priceIt->first;

        if (msc_debug_metadex2) PrintToLog("comparing prices: desprice %s needs to be GREATER THAN OR EQUAL TO %s\n",
            xToString(pnew->getEffectivePrice()), xToString(sellersPrice));

        // Is the desired price check satisfied? The buyer's inverse price must be larger than that of the seller.
        if ((pnew->getTradingAction() == BUY) && (pnew->getEffectivePrice() < sellersPrice)) {
            continue;
        }
        else if ((pnew->getTradingAction() == SELL) && (pnew->getEffectivePrice() > sellersPrice))  {
            continue;
        }

        cd_Set* const pofferSet = &(priceIt->second);
        // At good (single) price level and property iterate over offers looking at all parameters to find the match
        cd_Set::iterator offerIt = pofferSet->begin();
        while (offerIt != pofferSet->end()) { // Specific price, check all properties

            const CMPContractDex* const pold = &(*offerIt);
            assert(pold->getEffectivePrice() == sellersPrice);

            std::string tradeStatus = pold->getEffectivePrice() == sellersPrice ? "Matched" : "NoMatched";

            // Does the desired property match? Does the tradingaction match?
            if ((pold->getProperty() != propertyForSale) || (pold->getTradingAction() == pnew->getTradingAction())) {
                ++offerIt;
                continue;
            }

            //////////////////////////////////////
            // Preconditions
            assert(pold->getProperty() == pnew->getProperty());

            get_LiquidationPrice(pnew->getEffectivePrice(), pnew->getAddr(), pnew->getProperty(), pnew->getTradingAction()); // setting liquidation prices
            get_LiquidationPrice(pold->getEffectivePrice(), pold->getAddr(), pold->getProperty(), pold->getTradingAction());
            //
            // PrintToConsole("Checking effective prices and trading actions:\n");
            // PrintToConsole("Effective price pold: %d\n", pold->getEffectivePrice());
            // PrintToConsole("Effective price pnew: %d\n", pnew->getEffectivePrice());
            // PrintToConsole("Trading action pold: %d\n", pold->getTradingAction());
            // PrintToConsole("Trading action pnew: %d\n", pnew->getTradingAction());
            // PrintToConsole("________________________________________\n");
            ///////////////////////////
            int64_t possitive_sell = 0, difference_s = 0, seller_amount = 0, negative_sell = 0;
            int64_t possitive_buy  = 0, difference_b = 0, buyer_amount  = 0, negative_buy  = 0;
            uint32_t property_traded = pold->getProperty();
            std::string seller_address, buyer_address;

            possitive_sell = (pold->getTradingAction() == SELL) ? getMPbalance(pold->getAddr(), pold->getProperty(), POSSITIVE_BALANCE) : getMPbalance(pnew->getAddr(), pnew->getProperty(), POSSITIVE_BALANCE);
            negative_sell  = (pold->getTradingAction() == SELL) ? getMPbalance(pold->getAddr(), pold->getProperty(), NEGATIVE_BALANCE) : getMPbalance(pnew->getAddr(), pnew->getProperty(), NEGATIVE_BALANCE);
            possitive_buy  = (pold->getTradingAction() == SELL) ? getMPbalance(pnew->getAddr(), pnew->getProperty(), POSSITIVE_BALANCE) : getMPbalance(pold->getAddr(), pold->getProperty(), POSSITIVE_BALANCE);
            negative_buy   = (pold->getTradingAction() == SELL) ? getMPbalance(pnew->getAddr(), pnew->getProperty(), NEGATIVE_BALANCE) : getMPbalance(pold->getAddr(), pold->getProperty(), NEGATIVE_BALANCE);
            seller_amount  = (pold->getTradingAction() == SELL) ? pold->getAmountForSale() : pnew->getAmountForSale();
            buyer_amount   = (pold->getTradingAction() == SELL) ? pnew->getAmountForSale() : pold->getAmountForSale();
            seller_address = (pold->getTradingAction() == SELL) ? pold->getAddr() : pnew->getAddr();
            buyer_address  = (pold->getTradingAction() == SELL) ? pnew->getAddr() : pold->getAddr();

            ///////////////////////////
            int64_t nCouldBuy = 0;
            nCouldBuy = ( buyer_amount < seller_amount ) ? buyer_amount : seller_amount;

            if (nCouldBuy == 0) {
                // if (msc_debug_metadex1) PrintToLog(
                // "The buyer has not enough contracts for sale!\n");
                ++offerIt;
                continue;
            }
            ///////////////////////////
            if (possitive_sell > 0)
            {
               difference_s = possitive_sell - nCouldBuy;
               if (difference_s >= 0)
               {
                  assert(update_tally_map(seller_address, property_traded, -nCouldBuy, POSSITIVE_BALANCE));
               } else {
                  assert(update_tally_map(seller_address, property_traded, -possitive_sell, POSSITIVE_BALANCE));
                  assert(update_tally_map(seller_address, property_traded, -difference_s, NEGATIVE_BALANCE));
               }
            } else {
                  assert(update_tally_map(seller_address, property_traded, nCouldBuy, NEGATIVE_BALANCE));
            }

            if (possitive_buy > 0)
            {
               assert(update_tally_map(buyer_address, property_traded, nCouldBuy, POSSITIVE_BALANCE));
            } else {
               difference_b = nCouldBuy - negative_buy;
               if (difference_b > 0)
               {
                   assert(update_tally_map(buyer_address, property_traded, difference_b, POSSITIVE_BALANCE));
                   if (negative_buy > 0){
                     assert(update_tally_map(buyer_address, property_traded, -negative_buy, NEGATIVE_BALANCE));
                   }
               } else {
                   assert(update_tally_map(buyer_address, property_traded, -nCouldBuy, NEGATIVE_BALANCE));

                 }
            }
            ///////////////////////////
            std::string Status_s = "Netted";
            std::string Status_b = "Netted";

            NewReturn = TRADED;
            CMPContractDex contract_replacement = *pold;
            int64_t remaining = abs(seller_amount - buyer_amount);

            if ((seller_amount > buyer_amount && pold->getTradingAction() == SELL) || (seller_amount < buyer_amount && pold->getTradingAction() == BUY)) {
                contract_replacement.setAmountForsale(remaining, "moreinseller");
                pnew->setAmountForsale(0, "no_remaining");
                NewReturn = TRADED_MOREINSELLER;

            } else if ((seller_amount < buyer_amount && pold->getTradingAction() == SELL) || (seller_amount > buyer_amount && pold->getTradingAction() == BUY)) {
                contract_replacement.setAmountForsale(0, "no_remaining");
                pnew->setAmountForsale(remaining, "moreinbuyer");
                NewReturn = TRADED_MOREINBUYER;

            } else if (seller_amount == buyer_amount) {
                pnew->setAmountForsale(0, "no_remaining");
                contract_replacement.setAmountForsale(0, "no_remaining");
                NewReturn = TRADED;
            }
            ///////////////////////////////////////
            int64_t countClosedSeller = 0, countClosedBuyer = 0;

            if ( possitive_sell > 0 && negative_sell == 0 ) {
                if ( pold->getTradingAction() == SELL ) {
                    Status_s = (possitive_sell > getMPbalance(contract_replacement.getAddr(), contract_replacement.getProperty(), POSSITIVE_BALANCE)) ? "Long Netted" : ( getMPbalance(contract_replacement.getAddr(), contract_replacement.getProperty(), POSSITIVE_BALANCE) == 0 ? "Netted" : "None" );
                    countClosedSeller = getMPbalance(contract_replacement.getAddr(), contract_replacement.getProperty(), POSSITIVE_BALANCE) == 0 ? possitive_sell : abs( possitive_sell - getMPbalance(contract_replacement.getAddr(), contract_replacement.getProperty(), POSSITIVE_BALANCE) );
                } else {
                    Status_s = (possitive_sell > getMPbalance(pnew->getAddr(), pnew->getProperty(), POSSITIVE_BALANCE)) ? "Long Netted" : ( getMPbalance(pnew->getAddr(), pnew->getProperty(), POSSITIVE_BALANCE) == 0 ? "Netted" : "None" );
                    countClosedSeller = getMPbalance(pnew->getAddr(), pnew->getProperty(), POSSITIVE_BALANCE) == 0 ? possitive_sell : abs( possitive_sell - getMPbalance(pnew->getAddr(), pnew->getProperty(), POSSITIVE_BALANCE) );
                }
            } else if ( negative_sell > 0 && possitive_sell == 0 ) {
                if ( pold->getTradingAction() == SELL ) {
                    Status_s = (negative_sell > getMPbalance(contract_replacement.getAddr(), contract_replacement.getProperty(), NEGATIVE_BALANCE)) ? "Short Netted" : ( getMPbalance(contract_replacement.getAddr(), contract_replacement.getProperty(), NEGATIVE_BALANCE) == 0 ? "Netted" : "None" );
                    countClosedSeller = getMPbalance(contract_replacement.getAddr(), contract_replacement.getProperty(), NEGATIVE_BALANCE) == 0 ? negative_sell : abs( negative_sell - getMPbalance(contract_replacement.getAddr(), contract_replacement.getProperty(), NEGATIVE_BALANCE) );

                } else {
                    Status_s = (negative_sell > getMPbalance(pnew->getAddr(), pnew->getProperty(), NEGATIVE_BALANCE)) ? "Short Netted" : ( getMPbalance(pnew->getAddr(), pnew->getProperty(), NEGATIVE_BALANCE) == 0 ? "Netted" : "None");
                    countClosedSeller = getMPbalance(pnew->getAddr(), pnew->getProperty(), NEGATIVE_BALANCE) == 0 ? negative_sell : abs( negative_sell - getMPbalance(pnew->getAddr(), pnew->getProperty(), NEGATIVE_BALANCE) );
                }

            } else if ( negative_sell == 0 && possitive_sell == 0 ) {
                Status_s = "None";
                countClosedSeller = 0;
            }

            if ( possitive_buy > 0 && negative_buy == 0 ) {
                Status_b = (possitive_buy > getMPbalance(buyer_address, property_traded, POSSITIVE_BALANCE)) ? "Long Netted" : ( getMPbalance(buyer_address, property_traded, POSSITIVE_BALANCE) == 0 ? "Netted" : "None" );
                countClosedBuyer = getMPbalance(buyer_address, property_traded, POSSITIVE_BALANCE) == 0 ? possitive_buy : abs( possitive_buy - getMPbalance(buyer_address, property_traded, POSSITIVE_BALANCE) );

            } else if ( negative_buy > 0 && negative_buy == 0 ) {
                Status_b = (negative_buy > getMPbalance(buyer_address, property_traded, NEGATIVE_BALANCE)) ? "Short Netted" : ( getMPbalance(buyer_address, property_traded, NEGATIVE_BALANCE) == 0 ? "Netted" : "None" );
                countClosedBuyer = getMPbalance(buyer_address, property_traded, NEGATIVE_BALANCE) == 0 ? negative_buy : abs( negative_buy - getMPbalance(buyer_address, property_traded, NEGATIVE_BALANCE) );

            } else if ( negative_buy == 0 && possitive_buy == 0 ) {
                Status_b = "None";
                countClosedBuyer = 0;
            }
            //////////////////////////////////////////////
            // PrintToConsole("Checking Status for Seller and Buyer:\n");
            // PrintToConsole("Status seller: %s, Status buyer: %s\n", Status_s, Status_b);
            // PrintToConsole("________________________________________\n");
            ////////////////////////////////////////////////
            int64_t lives_maker = 0, lives_taker = 0;

            if( (getMPbalance(contract_replacement.getAddr(), contract_replacement.getProperty(), POSSITIVE_BALANCE) > 0) && ( getMPbalance(contract_replacement.getAddr(), contract_replacement.getProperty(), NEGATIVE_BALANCE) == 0 ) ) {
                lives_maker = getMPbalance(contract_replacement.getAddr(), contract_replacement.getProperty(), POSSITIVE_BALANCE);

            } else if( (getMPbalance(contract_replacement.getAddr(), contract_replacement.getProperty(), NEGATIVE_BALANCE) > 0 ) && (getMPbalance(contract_replacement.getAddr(), contract_replacement.getProperty(), POSSITIVE_BALANCE) == 0 ) ) {
                lives_maker = getMPbalance(contract_replacement.getAddr(), contract_replacement.getProperty(), NEGATIVE_BALANCE);
            }

            if( (getMPbalance(pnew->getAddr(), pnew->getProperty(), POSSITIVE_BALANCE) > 0 ) && (getMPbalance(pnew->getAddr(), pnew->getProperty(), NEGATIVE_BALANCE) == 0 ) ) {
                lives_taker = getMPbalance(pnew->getAddr(), pnew->getProperty(), POSSITIVE_BALANCE);

            } else if( (getMPbalance(pnew->getAddr(), pnew->getProperty(), NEGATIVE_BALANCE) > 0 ) && (getMPbalance(pnew->getAddr(), pnew->getProperty(), POSSITIVE_BALANCE) == 0 ) ) {
                lives_taker = getMPbalance(pnew->getAddr(), pnew->getProperty(), NEGATIVE_BALANCE);
            }

            if (countClosedSeller < 0){
                countClosedSeller = 0;
            }
            if (countClosedBuyer < 0){
               countClosedBuyer = 0;
            }
            ///////////////////////////////////////
            // PrintToConsole("Checking Market Price and Match Price:\n");
            // PrintToConsole("Market Price in Mdex: %d, countClosedBuyer: %d, Match Price: %d\n", marketPrice, countClosedBuyer, pold->getEffectivePrice());
            // PrintToConsole("________________________________________\n");

            if ( Status_s != "None") {

                uint64_t freedReserverExPNLMaker = marginRequirementContract*countClosedSeller;

				if (freedReserverExPNLMaker != 0) {
                	assert(update_tally_map(seller_address, collateralCurrency, -freedReserverExPNLMaker, CONTRACTDEX_RESERVE));
                	assert(update_tally_map(seller_address, collateralCurrency,  freedReserverExPNLMaker, BALANCE));
                }

                // int64_t basis_s = t_tradelistdb->getTradeBasis(seller_address, countClosedSeller, property_traded);
                // int64_t PNL_s = (marketPrice*countClosedSeller - basis_s)*notionalSize;

                // if ( PNL_s != 0 ) {
                //     assert(update_tally_map(seller_address, collateralCurrency, PNL_s, CONTRACTDEX_RESERVE));
                // }
                // PrintToConsole("PNL_s: %d, Basis: %d\n", PNL_s, basis_s);
            }

            if ( Status_b != "None" ) {

                uint64_t freedReserverExPNLTaker = marginRequirementContract*countClosedBuyer;

                if (freedReserverExPNLTaker != 0) {
	                assert(update_tally_map(buyer_address, collateralCurrency, -freedReserverExPNLTaker, CONTRACTDEX_RESERVE));
    	            assert(update_tally_map(buyer_address, collateralCurrency,  freedReserverExPNLTaker, BALANCE));
    	        }

                // int64_t basis_b = t_tradelistdb->getTradeBasis(buyer_address, countClosedBuyer, property_traded);
                // int64_t PNL_b = (marketPrice*countClosedBuyer - basis_b)*notionalSize;

                // if ( PNL_b != 0 ) {
                //     assert(update_tally_map(buyer_address, collateralCurrency, PNL_b, CONTRACTDEX_RESERVE));
                // }
            }
            ///////////////////////////////////////
            std::string Status_maker = "";
            std::string Status_taker = "";

            if (pold->getAddr() == seller_address){
                Status_maker = Status_s;
                Status_taker = Status_b;
            } else {
                Status_maker = Status_b;
                Status_taker = Status_s;
            }

            t_tradelistdb->recordMatchedTrade(pold->getHash(),
                                              pnew->getHash(),
                                              pold->getAddr(),
                                              pnew->getAddr(),
                                              pold->getEffectivePrice(),
                                              nCouldBuy,
                                              pnew->getAmountForSale(),
                                              pold->getBlock(),
                                              pnew->getBlock(),
                                              Status_maker,
                                              Status_taker,
                                              lives_maker,
                                              lives_taker,
                                              property_traded,
                                              tradeStatus,
                                              pold->getEffectivePrice(),
                                              pnew->getEffectivePrice()
                                              );
            ///////////////////////////////////////
            marketPrice = pold->getEffectivePrice();
            PrintToConsole("marketPrice: %d\n", FormatContractShortMP(marketPrice));

            t_tradelistdb->marginLogic(property_traded); //checking margin

            if (msc_debug_metadex1) PrintToLog("++ erased old: %s\n", offerIt->ToString());
                pofferSet->erase(offerIt++);

            if (seller_amount != buyer_amount) {
                pofferSet->insert(contract_replacement);
            }

            if (bBuyerSatisfied) {
                break;
            }
        }
        if (bBuyerSatisfied) break;
    }
    return NewReturn;
}

/////////////////////////////////////
/*New things for contracts */
void get_LiquidationPrice(int64_t effectivePrice, string address, uint32_t property, uint8_t trading_action)
{
    /** Remember: percentLiqPrice is defined in tx.cpp ContractDexTrade */
    extern double percentLiqPrice;
    double liqFactor = (trading_action == BUY) ? (1 - percentLiqPrice) : (1 + percentLiqPrice);
    double liqPrice = effectivePrice*liqFactor;

    PrintToConsole ("Effective price : %g\n", liqPrice);

    int64_t liq64 = static_cast<int64_t>(liqPrice);
    assert(update_tally_map(address, property, liq64, LIQUIDATION_PRICE));

    PrintToConsole ("Precio de liquidación: %d\n", FormatContractShortMP(liq64));
}
////////////////////////////////////////
/**
 * Used for display of unit prices to 8 decimal places at UI layer.
 *
 * Automatically returns unit or inverse price as needed.
 */
std::string CMPMetaDEx::displayUnitPrice() const
{
     rational_t tmpDisplayPrice;
     if (getDesProperty() == OMNI_PROPERTY_MSC || getDesProperty() == OMNI_PROPERTY_TMSC) {
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

/**
 * Used for display of unit prices with 50 decimal places at RPC layer.
 *
 * Note: unit price is no longer always shown in OMNI and/or inverted
 */
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

//////////////////////////////////////
/** New things for Contracts */
std::string CMPContractDex::displayFullContractPrice() const
{
    uint64_t priceForsale = getEffectivePrice();
    uint64_t amountForsale = getAmountForSale();

    int128_t fullprice;
    if ( isPropertyContract(getProperty()) ) multiply(fullprice, priceForsale, amountForsale);

    std::string priceForsaleStr = xToString(fullprice);
    return priceForsaleStr;
}
//////////////////////////////////////

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
///////////////////////////////////////////

std::string CMPMetaDEx::ToString() const
{
    return strprintf("%s:%34s in %d/%03u, txid: %s , trade #%u %s for #%u %s",
        xToString(unitPrice()), addr, block, idx, txid.ToString().substr(0, 10),
        property, FormatMP(property, amount_forsale), desired_property, FormatMP(desired_property, amount_desired));
}

////////////////////////////////////
/** New things for Contract */
std::string CMPContractDex::ToString() const
{
    return strprintf("%s:%34s in %d/%03u, txid: %s , trade #%u %s for #%u %s",
        xToString(getEffectivePrice()), getAddr(), getBlock(), getIdx(), getHash().ToString().substr(0, 10),
        getProperty(), FormatMP(getProperty(), getAmountForSale()));
}
//////////////////////////////////////

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

////////////////////////////////////
/** New things for Contract */
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

////////////////////////////////////
/** New things for Contract */
void saveDataGraphs(std::ofstream& file, std::string lineOut)
{
    std::string line = lineOut;
    file << line << std::endl;
}
////////////////////////////////////

bool MetaDEx_compare::operator()(const CMPMetaDEx &lhs, const CMPMetaDEx &rhs) const
{
    if (lhs.getBlock() == rhs.getBlock()) return lhs.getIdx() < rhs.getIdx();
    else return lhs.getBlock() < rhs.getBlock();
}

///////////////////////////////////////////
/** New things for Contracts */
bool ContractDex_compare::operator()(const CMPContractDex &lhs, const CMPContractDex &rhs) const
{
    if (lhs.getBlock() == rhs.getBlock()) return lhs.getIdx() < rhs.getIdx();
    else return lhs.getBlock() < rhs.getBlock();
}
///////////////////////////////////////////

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

///////////////////////////////////////////
/** New things for Contracts */
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
     // = *cd_prices;

    return true;
}
///////////////////////////////////////////

// pretty much directly linked to the ADD TX21 command off the wire
int mastercore::MetaDEx_ADD(const std::string& sender_addr, uint32_t prop, int64_t amount, int block, uint32_t property_desired, int64_t amount_desired, const uint256& txid, unsigned int idx)
{
    int rc = METADEX_ERROR -1;

    // Create a MetaDEx object from paremeters
    CMPMetaDEx new_mdex(sender_addr, block, prop, amount, property_desired, amount_desired, txid, idx, CMPTransaction::ADD);
    if (msc_debug_metadex1) PrintToLog("%s(); buyer obj: %s\n", __FUNCTION__, new_mdex.ToString());

    // Ensure this is not a badly priced trade (for example due to zero amounts)
    if (0 >= new_mdex.unitPrice()) return METADEX_ERROR -66;

    // Match against existing trades, remainder of the order will be put into the order book
    if (msc_debug_metadex3) MetaDEx_debug_print();
    x_Trade(&new_mdex);
    if (msc_debug_metadex3) MetaDEx_debug_print();

    // Insert the remaining order into the MetaDEx maps
    if (0 < new_mdex.getAmountRemaining()) { //switch to getAmountRemaining() when ready
        if (!MetaDEx_INSERT(new_mdex)) {
            PrintToLog("%s() ERROR: ALREADY EXISTS, line %d, file: %s\n", __FUNCTION__, __LINE__, __FILE__);
            return METADEX_ERROR -70;
        } else {
            // move tokens into reserve
            assert(update_tally_map(sender_addr, prop, -new_mdex.getAmountRemaining(), BALANCE));
            assert(update_tally_map(sender_addr, prop, new_mdex.getAmountRemaining(), METADEX_RESERVE));

            if (msc_debug_metadex1) PrintToLog("==== INSERTED: %s= %s\n", xToString(new_mdex.unitPrice()), new_mdex.ToString());
            if (msc_debug_metadex3) MetaDEx_debug_print();
        }
    }
    rc = 0;
    return rc;
}

/////////////////////////////////////////
/** New things for Contract */
int mastercore::ContractDex_ADD(const std::string& sender_addr, uint32_t prop, int64_t amount, int block, const uint256& txid, unsigned int idx, uint64_t effective_price, uint8_t trading_action, int64_t amount_to_reserve)
{
    // int rc = METADEX_ERROR -1;

    // Create a MetaDEx object from paremeters
    /*Remember: Here CMPTransaction::ADD is the subaction coming from CMPMetaDEx*/
    CMPContractDex new_cdex(sender_addr, block, prop, amount, 0, 0, txid, idx, CMPTransaction::ADD, effective_price, trading_action);
    // if (msc_debug_metadex1) PrintToLog("%s(); buyer obj: %s\n", __FUNCTION__, new_cdex.ToString());
    // Ensure this is not a badly priced trade (for example due to zero amounts)
    // if (0 >= new_cdex.getEffectivePrice()) return METADEX_ERROR -66;

    // if (new_cdex.getBlock() == 1){
      PrintToConsole("AmountForSale: %d\n",new_cdex.getAmountForSale());
      PrintToConsole("Address: %d \n",new_cdex.getAddr());
      PrintToConsole("Effective Price: %d \n",new_cdex.getEffectivePrice());
      PrintToConsole("Trading Action: %d \n",new_cdex.getTradingAction());
    // }
    // Match against existing trades, remainder of the order will be put into the order book
    // if (msc_debug_metadex3) MetaDEx_debug_print();
    // x_Trade(&new_cdex);
    // if (msc_debug_metadex3) MetaDEx_debug_print();

    // Insert the remaining order into the ContractDex maps
    // if (0 < new_cdex.getAmountForSale()) { //switch to getAmounForSale() when ready
    //     if (!ContractDex_INSERT(new_cdex)) {
    //         PrintToLog("%s() ERROR: ALREADY EXISTS, line %d, file: %s\n", __FUNCTION__, __LINE__, __FILE__);
            // return METADEX_ERROR -70;
        // } else {
        //       PrintToConsole("Inserted in the orderbook!\n");
            // if (msc_debug_metadex1) PrintToLog("==== INSERTED: %s= %s\n", xToString(new_cdex.getEffectivePrice()), new_cdex.ToString());
            // if (msc_debug_metadex3) MetaDEx_debug_print();
    //     }
    // }
    // rc = 0;
    return 0;
}

////////////////////////////////////

int mastercore::MetaDEx_CANCEL_AT_PRICE(const uint256& txid, unsigned int block, const std::string& sender_addr, uint32_t prop, int64_t amount, uint32_t property_desired, int64_t amount_desired)
{
    int rc = METADEX_ERROR -20;
    CMPMetaDEx mdex(sender_addr, 0, prop, amount, property_desired, amount_desired, uint256(), 0, CMPTransaction::CANCEL_AT_PRICE);
    md_PricesMap* prices = get_Prices(prop);
    const CMPMetaDEx* p_mdex = NULL;

    if (msc_debug_metadex1) PrintToLog("%s():%s\n", __FUNCTION__, mdex.ToString());

    if (msc_debug_metadex2) MetaDEx_debug_print();

    if (!prices) {
        PrintToLog("%s() NOTHING FOUND for %s\n", __FUNCTION__, mdex.ToString());
        return rc -1;
    }

    // within the desired property map (given one property) iterate over the items
    for (md_PricesMap::iterator my_it = prices->begin(); my_it != prices->end(); ++my_it) {
        rational_t sellers_price = my_it->first;

        if (mdex.unitPrice() != sellers_price) continue;

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

            // move from reserve to main
            assert(update_tally_map(p_mdex->getAddr(), p_mdex->getProperty(), -p_mdex->getAmountRemaining(), METADEX_RESERVE));
            assert(update_tally_map(p_mdex->getAddr(), p_mdex->getProperty(), p_mdex->getAmountRemaining(), BALANCE));

            // record the cancellation
            bool bValid = true;
            p_txlistdb->recordMetaDExCancelTX(txid, p_mdex->getHash(), bValid, block, p_mdex->getProperty(), p_mdex->getAmountRemaining());

            indexes->erase(iitt++);
        }
    }

    if (msc_debug_metadex2) MetaDEx_debug_print();

    return rc;
}

//////////////////////////////////////
/** New things for Contracts */
int mastercore::ContractDex_CANCEL_AT_PRICE(const uint256& txid, unsigned int block, const std::string& sender_addr, uint32_t prop, int64_t amount, uint32_t property_desired, int64_t amount_desired, uint64_t effective_price, uint8_t trading_action)
{
    int rc = METADEX_ERROR -20;
    CMPContractDex cdex(sender_addr, 0, prop, amount, property_desired, amount_desired, uint256(), 0, CMPTransaction::CANCEL_AT_PRICE, effective_price, trading_action);
    cd_PricesMap* prices = get_PricesCd(prop);
    const CMPContractDex* p_cdex = NULL;

    if (msc_debug_metadex1) PrintToLog("%s():%s\n", __FUNCTION__, cdex.ToString());

    if (msc_debug_metadex2) MetaDEx_debug_print();

    if (!prices) {
        PrintToLog("%s() NOTHING FOUND for %s\n", __FUNCTION__, cdex.ToString());
        return rc -1;
    }

    // within the desired property map (given one property) iterate over the items
    for (cd_PricesMap::iterator my_it = prices->begin(); my_it != prices->end(); ++my_it) {
        uint64_t sellers_price = my_it->first;

        if (cdex.getEffectivePrice() != sellers_price) continue;

        cd_Set* indexes = &(my_it->second);

        for (cd_Set::iterator iitt = indexes->begin(); iitt != indexes->end();) {
            p_cdex = &(*iitt);

            if (msc_debug_metadex3) PrintToLog("%s(): %s\n", __FUNCTION__, p_cdex->ToString());

            if ((p_cdex->getProperty() != property_desired) || (p_cdex->getAddr() != sender_addr)) {
                ++iitt;
                continue;
            }

            rc = 0;
            PrintToLog("%s(): REMOVING %s\n", __FUNCTION__, p_cdex->ToString());

            // move from reserve to main
            assert(update_tally_map(p_cdex->getAddr(), p_cdex->getProperty(), -p_cdex->getAmountForSale(), METADEX_RESERVE));
            assert(update_tally_map(p_cdex->getAddr(), p_cdex->getProperty(), p_cdex->getAmountForSale(), BALANCE));

            // record the cancellation
            bool bValid = true;
            p_txlistdb->recordContractDexCancelTX(txid, p_cdex->getHash(), bValid, block, p_cdex->getProperty(), p_cdex->getAmountForSale());

            indexes->erase(iitt++);
        }
    }

    if (msc_debug_metadex2) MetaDEx_debug_print();

    return rc;
}
//////////////////////////////////////

int mastercore::MetaDEx_CANCEL_ALL_FOR_PAIR(const uint256& txid, unsigned int block, const std::string& sender_addr, uint32_t prop, uint32_t property_desired)
{
    int rc = METADEX_ERROR -30;
    md_PricesMap* prices = get_Prices(prop);
    const CMPMetaDEx* p_mdex = NULL;

    PrintToLog("%s(%d,%d)\n", __FUNCTION__, prop, property_desired);

    if (msc_debug_metadex3) MetaDEx_debug_print();

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

            // move from reserve to main
            assert(update_tally_map(p_mdex->getAddr(), p_mdex->getProperty(), -p_mdex->getAmountRemaining(), METADEX_RESERVE));
            assert(update_tally_map(p_mdex->getAddr(), p_mdex->getProperty(), p_mdex->getAmountRemaining(), BALANCE));

            // record the cancellation
            bool bValid = true;
            p_txlistdb->recordMetaDExCancelTX(txid, p_mdex->getHash(), bValid, block, p_mdex->getProperty(), p_mdex->getAmountRemaining());

            indexes->erase(iitt++);
        }
    }

    if (msc_debug_metadex3) MetaDEx_debug_print();

    return rc;
}

////////////////////////////////
/** New things for Contracts */
int mastercore::ContractDex_CANCEL_ALL_FOR_PAIR(const uint256& txid, unsigned int block, const std::string& sender_addr, uint32_t prop, uint32_t property_desired)
{
    int rc = METADEX_ERROR -30;
    cd_PricesMap* prices = get_PricesCd(prop);
    const CMPContractDex* p_cdex = NULL;

    PrintToLog("%s(%d,%d)\n", __FUNCTION__, prop, property_desired);

    if (msc_debug_metadex3) MetaDEx_debug_print();

    if (!prices) {
        PrintToLog("%s() NOTHING FOUND\n", __FUNCTION__);
        return rc -1;
    }

    // within the desired property map (given one property) iterate over the items
    for (cd_PricesMap::iterator my_it = prices->begin(); my_it != prices->end(); ++my_it) {
        cd_Set* indexes = &(my_it->second);

        for (cd_Set::iterator iitt = indexes->begin(); iitt != indexes->end();) {
            p_cdex = &(*iitt);

            if (msc_debug_metadex3) PrintToLog("%s(): %s\n", __FUNCTION__, p_cdex->ToString());

            if ((p_cdex->getDesProperty() != property_desired) || (p_cdex->getAddr() != sender_addr)) {
                ++iitt;
                continue;
            }

            rc = 0;
            PrintToLog("%s(): REMOVING %s\n", __FUNCTION__, p_cdex->ToString());

            // move from reserve to main
            assert(update_tally_map(p_cdex->getAddr(), p_cdex->getProperty(), -p_cdex->getAmountRemaining(), METADEX_RESERVE));
            assert(update_tally_map(p_cdex->getAddr(), p_cdex->getProperty(), p_cdex->getAmountRemaining(), BALANCE));

            // record the cancellation
            bool bValid = true;
            p_txlistdb->recordContractDexCancelTX(txid, p_cdex->getHash(), bValid, block, p_cdex->getProperty(), p_cdex->getAmountRemaining());

            indexes->erase(iitt++);
        }
    }

    if (msc_debug_metadex3) MetaDEx_debug_print();

    return rc;
}
////////////////////////////////

/**
 * Scans the orderbook and remove everything for an address.
 */
int mastercore::MetaDEx_CANCEL_EVERYTHING(const uint256& txid, unsigned int block, const std::string& sender_addr, unsigned char ecosystem)
{
    int rc = METADEX_ERROR -40;

    PrintToLog("%s()\n", __FUNCTION__);

    if (msc_debug_metadex2) MetaDEx_debug_print();

    PrintToLog("<<<<<<\n");

    for (md_PropertiesMap::iterator my_it = metadex.begin(); my_it != metadex.end(); ++my_it) {
        unsigned int prop = my_it->first;

        // skip property, if it is not in the expected ecosystem
        if (isMainEcosystemProperty(ecosystem) && !isMainEcosystemProperty(prop)) continue;
        if (isTestEcosystemProperty(ecosystem) && !isTestEcosystemProperty(prop)) continue;

        PrintToLog(" ## property: %u\n", prop);
        md_PricesMap& prices = my_it->second;

        for (md_PricesMap::iterator it = prices.begin(); it != prices.end(); ++it) {
            rational_t price = it->first;
            md_Set& indexes = it->second;

            PrintToLog("  # Price Level: %s\n", xToString(price));

            for (md_Set::iterator it = indexes.begin(); it != indexes.end();) {
                PrintToLog("%s= %s\n", xToString(price), it->ToString());

                if (it->getAddr() != sender_addr) {
                    ++it;
                    continue;
                }

                rc = 0;
                PrintToLog("%s(): REMOVING %s\n", __FUNCTION__, it->ToString());

                // move from reserve to balance
                assert(update_tally_map(it->getAddr(), it->getProperty(), -it->getAmountRemaining(), METADEX_RESERVE));
                assert(update_tally_map(it->getAddr(), it->getProperty(), it->getAmountRemaining(), BALANCE));

                // record the cancellation
                bool bValid = true;
                p_txlistdb->recordMetaDExCancelTX(txid, it->getHash(), bValid, block, it->getProperty(), it->getAmountRemaining());

                indexes.erase(it++);
            }
        }
    }
    PrintToLog(">>>>>>\n");

    if (msc_debug_metadex2) MetaDEx_debug_print();

    return rc;
}

/**
 * Scans the orderbook and remove everything for an address.
 */
//////////////////////////////////////
/** New things for Contracts */
int mastercore::ContractDex_CANCEL_EVERYTHING(const uint256& txid, unsigned int block, const std::string& sender_addr, unsigned char ecosystem)
{
    int rc = METADEX_ERROR -40;

    PrintToLog("%s()\n", __FUNCTION__);

    if (msc_debug_metadex2) MetaDEx_debug_print();

    PrintToLog("<<<<<<\n");

    for (cd_PropertiesMap::iterator my_it = contractdex.begin(); my_it != contractdex.end(); ++my_it) {
        unsigned int prop = my_it->first;

        // skip property, if it is not in the expected ecosystem
        if (isMainEcosystemProperty(ecosystem) && !isMainEcosystemProperty(prop)) continue;
        if (isTestEcosystemProperty(ecosystem) && !isTestEcosystemProperty(prop)) continue;

        PrintToLog(" ## property: %u\n", prop);
        cd_PricesMap &prices = my_it->second;

        for (cd_PricesMap::iterator it = prices.begin(); it != prices.end(); ++it) {
            uint64_t price = it->first;
            cd_Set &indexes = it->second;

            PrintToLog("  # Price Level: %s\n", xToString(price));

            for (cd_Set::iterator it = indexes.begin(); it != indexes.end();) {
                PrintToLog("%s= %s\n", xToString(price), it->ToString());

                if (it->getAddr() != sender_addr) {
                    ++it;
                    continue;
                }

                rc = 0;
                PrintToLog("%s(): REMOVING %s\n", __FUNCTION__, it->ToString());

                // move from reserve to balance
                assert(update_tally_map(it->getAddr(), it->getProperty(), -it->getAmountForSale(), METADEX_RESERVE));
                assert(update_tally_map(it->getAddr(), it->getProperty(), it->getAmountForSale(), BALANCE));

                // record the cancellation
                bool bValid = true;
                p_txlistdb->recordContractDexCancelTX(txid, it->getHash(), bValid, block, it->getProperty(), it->getAmountForSale());

                indexes.erase(it++);
            }
        }
    }
    PrintToLog(">>>>>>\n");

    if (msc_debug_metadex2) MetaDEx_debug_print();

    return rc;
}
//////////////////////////////////////

/**
 * Scans the orderbook and removes every all-pair order
 */
int mastercore::MetaDEx_SHUTDOWN_ALLPAIR()
{
    int rc = 0;
    PrintToLog("%s()\n", __FUNCTION__);
    for (md_PropertiesMap::iterator my_it = metadex.begin(); my_it != metadex.end(); ++my_it) {
        md_PricesMap& prices = my_it->second;
        for (md_PricesMap::iterator it = prices.begin(); it != prices.end(); ++it) {
            md_Set& indexes = it->second;
            for (md_Set::iterator it = indexes.begin(); it != indexes.end();) {
                if (it->getDesProperty() > OMNI_PROPERTY_TMSC && it->getProperty() > OMNI_PROPERTY_TMSC) { // no OMNI/TOMNI side to the trade
                    PrintToLog("%s(): REMOVING %s\n", __FUNCTION__, it->ToString());
                    // move from reserve to balance
                    assert(update_tally_map(it->getAddr(), it->getProperty(), -it->getAmountRemaining(), METADEX_RESERVE));
                    assert(update_tally_map(it->getAddr(), it->getProperty(), it->getAmountRemaining(), BALANCE));
                    indexes.erase(it++);
                }
            }
        }
    }
    return rc;
}

///////////////////////////////////
/** New things for Contracts */
int mastercore::ContractDex_SHUTDOWN_ALLPAIR()
{
    int rc = 0;
    PrintToLog("%s()\n", __FUNCTION__);
    for (cd_PropertiesMap::iterator my_it = contractdex.begin(); my_it != contractdex.end(); ++my_it) {
        cd_PricesMap &prices = my_it->second;
        for (cd_PricesMap::iterator it = prices.begin(); it != prices.end(); ++it) {
            cd_Set &indexes = it->second;
            for (cd_Set::iterator it = indexes.begin(); it != indexes.end();) {
                if (it->getDesProperty() > OMNI_PROPERTY_TMSC && it->getProperty() > OMNI_PROPERTY_TMSC) { // no OMNI/TOMNI side to the trade
                    PrintToLog("%s(): REMOVING %s\n", __FUNCTION__, it->ToString());
                    // move from reserve to balance
                    assert(update_tally_map(it->getAddr(), it->getProperty(), -it->getAmountRemaining(), METADEX_RESERVE));
                    assert(update_tally_map(it->getAddr(), it->getProperty(),  it->getAmountRemaining(), BALANCE));
                    indexes.erase(it++);
                }
            }
        }
    }
    return rc;
}
///////////////////////////////////

/**
 * Scans the orderbook and removes every order
 */
int mastercore::MetaDEx_SHUTDOWN()
{
    int rc = 0;
    PrintToLog("%s()\n", __FUNCTION__);
    for (md_PropertiesMap::iterator my_it = metadex.begin(); my_it != metadex.end(); ++my_it) {
        md_PricesMap& prices = my_it->second;
        for (md_PricesMap::iterator it = prices.begin(); it != prices.end(); ++it) {
            md_Set& indexes = it->second;
            for (md_Set::iterator it = indexes.begin(); it != indexes.end();) {
                PrintToLog("%s(): REMOVING %s\n", __FUNCTION__, it->ToString());
                // move from reserve to balance
                assert(update_tally_map(it->getAddr(), it->getProperty(), -it->getAmountRemaining(), METADEX_RESERVE));
                assert(update_tally_map(it->getAddr(), it->getProperty(), it->getAmountRemaining(), BALANCE));
                indexes.erase(it++);
            }
        }
    }
    return rc;
}

//////////////////////////////////
/** New things for Contracts */
int mastercore::ContractDex_SHUTDOWN()
{
    int rc = 0;
    PrintToLog("%s()\n", __FUNCTION__);
    for (cd_PropertiesMap::iterator my_it = contractdex.begin(); my_it != contractdex.end(); ++my_it) {
        cd_PricesMap &prices = my_it->second;
        for (cd_PricesMap::iterator it = prices.begin(); it != prices.end(); ++it) {
            cd_Set &indexes = it->second;
            for (cd_Set::iterator it = indexes.begin(); it != indexes.end();) {
                PrintToLog("%s(): REMOVING %s\n", __FUNCTION__, it->ToString());
                // move from reserve to balance
                assert(update_tally_map(it->getAddr(), it->getProperty(), -it->getAmountRemaining(), METADEX_RESERVE));
                assert(update_tally_map(it->getAddr(), it->getProperty(), it->getAmountRemaining(), BALANCE));
                indexes.erase(it++);
            }
        }
    }
    return rc;
}
//////////////////////////////////

// searches the metadex maps to see if a trade is still open
// allows search to be optimized if propertyIdForSale is specified
bool mastercore::MetaDEx_isOpen(const uint256& txid, uint32_t propertyIdForSale)
{
    for (md_PropertiesMap::iterator my_it = metadex.begin(); my_it != metadex.end(); ++my_it) {
        if (propertyIdForSale != 0 && propertyIdForSale != my_it->first) continue;
        md_PricesMap & prices = my_it->second;
        for (md_PricesMap::iterator it = prices.begin(); it != prices.end(); ++it) {
            md_Set & indexes = (it->second);
            for (md_Set::iterator it = indexes.begin(); it != indexes.end(); ++it) {
                CMPMetaDEx obj = *it;
                if( obj.getHash().GetHex() == txid.GetHex() ) return true;
            }
        }
    }
    return false;
}

/////////////////////////////////////
/** New things for Contracts */
bool mastercore::ContractDex_isOpen(const uint256& txid, uint32_t propertyIdForSale)
{
    for (cd_PropertiesMap::iterator my_it = contractdex.begin(); my_it != contractdex.end(); ++my_it) {
        if (propertyIdForSale != 0 && propertyIdForSale != my_it->first) continue;
        cd_PricesMap &prices = my_it->second;
        for (cd_PricesMap::iterator it = prices.begin(); it != prices.end(); ++it) {
            cd_Set &indexes = (it->second);
            for (cd_Set::iterator it = indexes.begin(); it != indexes.end(); ++it) {
                CMPContractDex obj = *it;
                if( obj.getHash().GetHex() == txid.GetHex() ) return true;
            }
        }
    }
    return false;
}
////////////////////////////////////

/**
 * Returns a string describing the status of a trade
 *
 */
std::string mastercore::MetaDEx_getStatusText(int tradeStatus)
{
    switch (tradeStatus) {
        case TRADE_OPEN: return "open";
        case TRADE_OPEN_PART_FILLED: return "open part filled";
        case TRADE_FILLED: return "filled";
        case TRADE_CANCELLED: return "cancelled";
        case TRADE_CANCELLED_PART_FILLED: return "cancelled part filled";
        case TRADE_INVALID: return "trade invalid";
        default: return "unknown";
    }
}

////////////////////////////////
/** New things for Contracts */
std::string mastercore::ContractDex_getStatusText(int tradeStatus)
{
    switch (tradeStatus) {
        case TRADE_OPEN: return "open";
        case TRADE_OPEN_PART_FILLED: return "open part filled";
        case TRADE_FILLED: return "filled";
        case TRADE_CANCELLED: return "cancelled";
        case TRADE_CANCELLED_PART_FILLED: return "cancelled part filled";
        case TRADE_INVALID: return "trade invalid";
        default: return "unknown";
    }
}
////////////////////////////////

/**
 * Returns the status of a MetaDEx trade
 *
 */
int mastercore::MetaDEx_getStatus(const uint256& txid, uint32_t propertyIdForSale, int64_t amountForSale, int64_t totalSold)
{
    // NOTE: If the calling code is already aware of the total amount sold, pass the value in to this function to avoid duplication of
    //       work.  If the calling code doesn't know the amount, leave default (-1) and we will calculate it from levelDB lookups.
    if (totalSold == -1) {
        UniValue tradeArray(UniValue::VARR);
        int64_t totalReceived;
        t_tradelistdb->getMatchingTrades(txid, propertyIdForSale, tradeArray, totalSold, totalReceived);
    }

    // Return a "trade invalid" status if the trade was invalidated at parsing/interpretation (eg insufficient funds)
    if (!getValidMPTX(txid)) return TRADE_INVALID;

    // Calculate and return the status of the trade via the amount sold and open/closed attributes.
    if (MetaDEx_isOpen(txid, propertyIdForSale)) {
        if (totalSold == 0) {
            return TRADE_OPEN;
        } else {
            return TRADE_OPEN_PART_FILLED;
        }
    } else {
        if (totalSold == 0) {
            return TRADE_CANCELLED;
        } else if (totalSold < amountForSale) {
            return TRADE_CANCELLED_PART_FILLED;
        } else {
            return TRADE_FILLED;
        }
    }
}

///////////////////////////////
/** New things for Contracts */
int mastercore::ContractDex_getStatus(const uint256& txid, uint32_t propertyIdForSale, int64_t amountForSale, int64_t totalSold)
{
    // NOTE: If the calling code is already aware of the total amount sold, pass the value in to this function to avoid duplication of
    //       work.  If the calling code doesn't know the amount, leave default (-1) and we will calculate it from levelDB lookups.
    if (totalSold == -1) {
        UniValue tradeArray(UniValue::VARR);
        int64_t totalReceived;
        t_tradelistdb->getMatchingTrades(txid, propertyIdForSale, tradeArray, totalSold, totalReceived);
    }

    // Return a "trade invalid" status if the trade was invalidated at parsing/interpretation (eg insufficient funds)
    if (!getValidMPTX(txid)) return TRADE_INVALID;

    // Calculate and return the status of the trade via the amount sold and open/closed attributes.
    if (ContractDex_isOpen(txid, propertyIdForSale)) {
        if (totalSold == 0) {
            return TRADE_OPEN;
        } else {
            return TRADE_OPEN_PART_FILLED;
        }
    } else {
        if (totalSold == 0) {
            return TRADE_CANCELLED;
        } else if (totalSold < amountForSale) {
            return TRADE_CANCELLED_PART_FILLED;
        } else {
            return TRADE_FILLED;
        }
    }
}
///////////////////////////////

void mastercore::MetaDEx_debug_print(bool bShowPriceLevel, bool bDisplay)
{
    PrintToLog("<<<\n");
    for (md_PropertiesMap::iterator my_it = metadex.begin(); my_it != metadex.end(); ++my_it) {
        uint32_t prop = my_it->first;

        PrintToLog(" ## property: %u\n", prop);
        md_PricesMap& prices = my_it->second;

        for (md_PricesMap::iterator it = prices.begin(); it != prices.end(); ++it) {
            rational_t price = it->first;
            md_Set& indexes = it->second;

            if (bShowPriceLevel) PrintToLog("  # Price Level: %s\n", xToString(price));

            for (md_Set::iterator it = indexes.begin(); it != indexes.end(); ++it) {
                const CMPMetaDEx& obj = *it;

                if (bDisplay) PrintToConsole("%s= %s\n", xToString(price), obj.ToString());
                else PrintToLog("%s= %s\n", xToString(price), obj.ToString());
            }
        }
    }
    PrintToLog(">>>\n");
}

/////////////////////////////////////
/** New things for Contracts */
void mastercore::ContractDex_debug_print(bool bShowPriceLevel, bool bDisplay)
{
    PrintToLog("<<<\n");
    for (cd_PropertiesMap::iterator my_it = contractdex.begin(); my_it != contractdex.end(); ++my_it) {
        uint32_t prop = my_it->first;

        PrintToLog(" ## property: %u\n", prop);
        cd_PricesMap &prices = my_it->second;

        for (cd_PricesMap::iterator it = prices.begin(); it != prices.end(); ++it) {
            uint64_t price = it->first;
            cd_Set &indexes = it->second;

            if (bShowPriceLevel) PrintToLog("  # Price Level: %s\n", xToString(price));

            for (cd_Set::iterator it = indexes.begin(); it != indexes.end(); ++it) {
                const CMPContractDex &obj = *it;

                if (bDisplay) PrintToConsole("%s= %s\n", xToString(price), obj.ToString());
                else PrintToLog("%s= %s\n", xToString(price), obj.ToString());
            }
        }
    }
    PrintToLog(">>>\n");
}
/////////////////////////////////////

/**
 * Locates a trade in the MetaDEx maps via txid and returns the trade object
 *
 */
const CMPMetaDEx* mastercore::MetaDEx_RetrieveTrade(const uint256& txid)
{
    for (md_PropertiesMap::iterator propIter = metadex.begin(); propIter != metadex.end(); ++propIter) {
        md_PricesMap & prices = propIter->second;
        for (md_PricesMap::iterator pricesIter = prices.begin(); pricesIter != prices.end(); ++pricesIter) {
            md_Set & indexes = pricesIter->second;
            for (md_Set::iterator tradesIter = indexes.begin(); tradesIter != indexes.end(); ++tradesIter) {
                if (txid == (*tradesIter).getHash()) return &(*tradesIter);
            }
        }
    }
    return (CMPMetaDEx*) NULL;
}

////////////////////////////////////////
/** New things for Contracts */
const CMPContractDex* mastercore::ContractDex_RetrieveTrade(const uint256& txid)
{
    for (cd_PropertiesMap::iterator propIter = contractdex.begin(); propIter != contractdex.end(); ++propIter) {
        cd_PricesMap &prices = propIter->second;
        for (cd_PricesMap::iterator pricesIter = prices.begin(); pricesIter != prices.end(); ++pricesIter) {
            cd_Set &indexes = pricesIter->second;
            for (cd_Set::iterator tradesIter = indexes.begin(); tradesIter != indexes.end(); ++tradesIter) {
                if (txid == (*tradesIter).getHash()) return &(*tradesIter);
            }
        }
    }
    return (CMPContractDex*) NULL;
}
////////////////////////////////////////
