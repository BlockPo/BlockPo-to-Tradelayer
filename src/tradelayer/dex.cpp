/**
 * @file dex.cpp
 *
 * This file contains DEx logic.
 */

#include <tradelayer/dex.h>
#include <tradelayer/ce.h>
#include <tradelayer/convert.h>
#include <tradelayer/errors.h>
#include <tradelayer/externfns.h>
#include <tradelayer/log.h>
#include <tradelayer/mdex.h>
#include <tradelayer/register.h>
#include <tradelayer/rules.h>
#include <tradelayer/sp.h>
#include <tradelayer/tradelayer.h>
#include <tradelayer/uint256_extensions.h>

#include <arith_uint256.h>
#include <hash.h>
#include <tinyformat.h>
#include <uint256.h>

#include <stdint.h>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>

std::map<int, std::map<uint32_t,int64_t>> mastercore::MapLTCVolume;
std::map<int, std::map<uint32_t,int64_t>> mastercore::MapTokenVolume;
std::map<uint32_t,std::map<int,std::vector<std::pair<int64_t,int64_t>>>> mastercore::tokenvwap;


namespace mastercore
{

/**
 * Checks, if such a sell offer exists.
 */
bool DEx_offerExists(const std::string& addressSeller, uint32_t propertyId)
{
    std::string key = STR_SELLOFFER_ADDR_PROP_COMBO(addressSeller, propertyId);

    return !(my_offers.find(key) == my_offers.end());
}

/**
 * Retrieves a sell offer.
 *
 * @return The sell offer, or NULL, if no match was found
 */
CMPOffer* DEx_getOffer(const std::string& addressSeller, uint32_t propertyId)
{
    if (msc_debug_dex) PrintToLog("%s(%s, %d)\n", __func__, addressSeller, propertyId);

    std::string key = STR_SELLOFFER_ADDR_PROP_COMBO(addressSeller, propertyId);
    OfferMap::iterator it = my_offers.find(key);

    if (it != my_offers.end()) return &(it->second);

    return static_cast<CMPOffer*>(nullptr);
}

/**
 * Checks, if such an accept order exists.
 */
bool DEx_acceptExists(const std::string& addressSeller, uint32_t propertyId, const std::string& addressBuyer)
{
    std::string key = STR_ACCEPT_ADDR_PROP_ADDR_COMBO(addressSeller, addressBuyer, propertyId);

    return !(my_accepts.find(key) == my_accepts.end());
}

/**
 * Retrieves an accept order.
 *
 * @return The accept order, or NULL, if no match was found
 */
CMPAccept* DEx_getAccept(const std::string& addressSeller, uint32_t propertyId, const std::string& addressBuyer)
{
    if (msc_debug_dex) PrintToLog("%s(%s, %d, %s)\n", __func__, addressSeller, propertyId, addressBuyer);

    std::string key = STR_ACCEPT_ADDR_PROP_ADDR_COMBO(addressSeller, addressBuyer, propertyId);
    AcceptMap::iterator it = my_accepts.find(key);

    if (it != my_accepts.end()) {
      if (msc_debug_dex) PrintToLog("%s(): ORDER FOUND!, getAcceptBlock : %d\n",__func__, (it->second).getAcceptBlock());
      return &(it->second);
    }

    if (msc_debug_dex) PrintToLog("%s(): accept order not found!\n",__func__);

    return static_cast<CMPAccept*>(nullptr);
}

/**
 * Determines the amount of litecoins desired, in case it needs to be recalculated.
 *
 * TODO: don't expose it!
 * @return The amount of litecoins desired
 */
int64_t calculateDesiredLTC(const int64_t amountOffered, const int64_t amountDesired, const int64_t amountAvailable)
{
    if (amountOffered == 0) {
        return 0; // divide by null protection
    }

    arith_uint256 amountOffered256 = ConvertTo256(amountOffered);
    arith_uint256 amountDesired256 = ConvertTo256(amountDesired);
    arith_uint256 amountAvailable256 = ConvertTo256(amountAvailable);

    amountDesired256 = DivideAndRoundUp((amountDesired256 * amountAvailable256), amountOffered256);

    return ConvertTo64(amountDesired256);
}

/**
 * Creates a new sell offer.
 *
 * @return 0 if everything is OK
 */
int DEx_offerCreate(const std::string& addressSeller, uint32_t propertyId, int64_t amountOffered, int block, int64_t amountDesired, int64_t minAcceptFee, uint8_t paymentWindow, const uint256& txid, uint64_t* nAmended)
{
    int rc = DEX_ERROR_SELLOFFER;

    // sanity checks
    // shold not be removed, because it may be used when updating/destroying an offer
    if (paymentWindow == 0) {
        return (DEX_ERROR_SELLOFFER -101);
    }
    if (amountDesired == 0) {
        return (DEX_ERROR_SELLOFFER -102);
    }

    if (propertyId == TL_PROPERTY_VESTING) {
        return (DEX_ERROR_SELLOFFER -103);
    }

    if (DEx_getOffer(addressSeller, propertyId)) {
        return (DEX_ERROR_SELLOFFER -10); // offer already exists
    }

    const std::string key = STR_SELLOFFER_ADDR_PROP_COMBO(addressSeller, propertyId);
    if (msc_debug_dex) PrintToLog("%s(%s|%s), nValue=%d)\n", __func__, addressSeller, key, amountOffered);

    const int64_t balanceReallyAvailable = getMPbalance(addressSeller, propertyId, BALANCE);

    if (amountOffered > balanceReallyAvailable) {
        PrintToLog("%s: rejected: sender %s has insufficient balance of property %d [%s < %s]\n", __func__,
                        addressSeller, propertyId, FormatDivisibleMP(balanceReallyAvailable), FormatDivisibleMP(amountOffered));
        return (DEX_ERROR_SELLOFFER -25);
      }

    // -------------------------------------------------------------------------

    if (amountOffered > 0) {
        assert(update_tally_map(addressSeller, propertyId, -amountOffered, BALANCE));
        assert(update_tally_map(addressSeller, propertyId, amountOffered, SELLOFFER_RESERVE));
        CMPOffer sellOffer(block, amountOffered, propertyId, amountDesired, minAcceptFee, paymentWindow, txid,0, 2);
        my_offers.insert(std::make_pair(key, sellOffer));

        rc = 0;
    }

    return rc;
}

int DEx_BuyOfferCreate(const std::string& addressMaker, uint32_t propertyId, int64_t amountOffered, int block, int64_t price, int64_t minAcceptFee, uint8_t paymentWindow, const uint256& txid, uint64_t* nAmended)
{
    // bool ready = false;
    int rc = DEX_ERROR_SELLOFFER;

    // sanity checks
    if (paymentWindow == 0)
    {
        if (msc_debug_dex) PrintToLog("DEX_ERROR_SELLOFFER\n");
        return (DEX_ERROR_SELLOFFER -101);
    }

    if (price == 0)
    {
        if (msc_debug_dex) PrintToLog("DEX_ERROR_SELLOFFER\n");
        return (DEX_ERROR_SELLOFFER -101);
    }

    if (DEx_getOffer(addressMaker, propertyId))
    {
        if (msc_debug_dex) PrintToLog("DEX_ERROR_SELLOFFER\n");
        return (DEX_ERROR_SELLOFFER -10); // offer already exists
    }

    const std::string key = STR_SELLOFFER_ADDR_PROP_COMBO(addressMaker, propertyId);
    if (msc_debug_dex) PrintToLog("%s(%s|%s), nValue=%d)\n", __func__, addressMaker, key, amountOffered);

    // ------------------------------------------------------------------------
    // On this part we need to put in reserve synth Litecoins.
    // LOCK(cs_register);
    //
    // arith_uint256 sumValues;
    // uint32_t nextCDID = _my_cds->peekNextContractID();
    //
    // for (uint32_t contractId = 1; contractId < nextCDID; contractId++)
    // {
    //     CDInfo::Entry cd;
    //     if (_my_cds->getCD(contractId, cd))
    //     {
    //         if (msc_debug_dex) PrintToLog("Property Id: %d\n", contractId);
    //
    //         int64_t longs = 0;
    //         int64_t shorts = 0;
    //
    //         const int64_t balance = getMPbalance(addressMaker, contractId, CONTRACT_BALANCE);
    //
    //         (balance > 0) ? longs = balance : shorts = balance;
    //
    //         if (msc_debug_dex) PrintToLog("%s(): longs: %d, shorts: %d, notional: %d\n",__func__, longs, shorts, cd.notional_size);
    //
    //         // price of one sLTC in ALLs
    //         // NOTE: it needs MORE WORK!
    //         // int64_t pair = getPairMarketPrice("sLTC", "ALL");
    //         int64_t pair = 1;
    //
    //         if (msc_debug_dex) PrintToLog("pair: %d\n", pair);
    //
    //         const int64_t side = (longs >= 0 && shorts == 0) ? longs : (longs == 0 && shorts >= 0) ? shorts : 0;
    //         arith_uint256 sumNum = (ConvertTo256(side) * ConvertTo256(cd.notional_size)) * ConvertTo256(pair);
    //         sumValues += DivideAndRoundUp(sumNum, COIN);
	  //         if (sumValues > ConvertTo256(price))
    //         {
    //             // price = price of entire order   .
	  //             if (msc_debug_dex) PrintToLog("You can buy tokens now\n");
	  //             // ready = true;
	  //             break;
    //         }
    //     }
    // }

    // NOTE: check conditions to buy tokens using litecoin
    // if (ready)
    if (true)
    {
        CMPOffer sellOffer(block, amountOffered, propertyId, price, minAcceptFee, paymentWindow, txid, 0, 1);
        my_offers.insert(std::make_pair(key, sellOffer));
        rc = 0;
    } else {
        if (msc_debug_dex) PrintToLog("You can't buy tokens, you need more position value\n");
        rc = -1;
    }

    return rc;
}

/**
 * Destroys a sell offer.
 *
 * The remaining amount reserved for the offer is returned to the available balance.
 *
 * @return 0 if the offer was destroyed
 */
int DEx_offerDestroy(const std::string& addressSeller, uint32_t propertyId)
{
    if (!DEx_offerExists(addressSeller, propertyId)) {
        return (DEX_ERROR_SELLOFFER -11); // offer does not exist
    }

    const int64_t amountReserved = getMPbalance(addressSeller, propertyId, SELLOFFER_RESERVE);

    // return the remaining reserved amount back to the seller
    if (amountReserved > 0) {
        assert(update_tally_map(addressSeller, propertyId, -amountReserved, SELLOFFER_RESERVE));
        assert(update_tally_map(addressSeller, propertyId, amountReserved, BALANCE));
    }

    // delete the offer
    const std::string key = STR_SELLOFFER_ADDR_PROP_COMBO(addressSeller, propertyId);
    OfferMap::iterator it = my_offers.find(key);
    if (it != my_offers.end()) my_offers.erase(it);

    if (msc_debug_dex) PrintToLog("%s(%s|%s)\n", __func__, addressSeller, key);

    return 0;
}

/**
 * Updates a sell offer.
 *
 * The old offer is destroyed, and replaced by the updated offer.
 *
 * @return 0 if everything is OK
 */
int DEx_offerUpdate(const std::string& addressSeller, uint32_t propertyId, int64_t amountOffered, int block, int64_t amountDesired, int64_t minAcceptFee, uint8_t paymentWindow, const uint256& txid, uint64_t* nAmended)
{
    if (msc_debug_dex) PrintToLog("%s(%s, %d)\n", __func__, addressSeller, propertyId);

    if (!DEx_offerExists(addressSeller, propertyId)) {
        return (DEX_ERROR_SELLOFFER -12); // offer does not exist
    }

    int rc = DEx_offerDestroy(addressSeller, propertyId);

    if (rc == 0) {
        // if the old offer was destroyed successfully, try to create a new one
        rc = DEx_offerCreate(addressSeller, propertyId, amountOffered, block, amountDesired, minAcceptFee, paymentWindow, txid, nAmended);
    }

    return rc;
}

/**
 * Creates a new accept order.
 *
 * @return 0 if everything is OK
 */
int DEx_acceptCreate(const std::string& addressTaker, const std::string& addressMaker, uint32_t propertyId, int64_t amountAccepted, int block, int64_t feePaid, uint64_t* nAmended)
{
    int rc = DEX_ERROR_ACCEPT -10;
    const std::string keySellOffer = STR_SELLOFFER_ADDR_PROP_COMBO(addressMaker, propertyId);
    const std::string keyAcceptOrder = STR_ACCEPT_ADDR_PROP_ADDR_COMBO(addressMaker, addressTaker, propertyId);

    OfferMap::const_iterator my_it = my_offers.find(keySellOffer);

    if (my_it == my_offers.end()) {
        PrintToLog("%s(): rejected: no matching sell offer for accept order found\n", __func__);
        return (DEX_ERROR_ACCEPT -15);
    }

    const CMPOffer& offer = my_it->second;

    if (msc_debug_dex) PrintToLog("%s(): found a matching sell offer [seller: %s, buyer: %s, property: %d)\n", __func__,
                    addressMaker, addressTaker, propertyId);

    // the older accept is the valid one: do not accept any new ones!
    if (DEx_acceptExists(addressMaker, propertyId, addressTaker)) {
        PrintToLog("%s(): rejected: an accept order from this same maker for this same offer already exists\n", __func__);
        return DEX_ERROR_ACCEPT -205;
    }

    // ensure the correct LTC fee was paid in this acceptance message
    if (msc_debug_dex) PrintToLog("%s() Checking: feePaid: %d, offer.getMinFee(): %d\n",__func__, feePaid, offer.getMinFee());

    if (feePaid < offer.getMinFee()) {
        PrintToLog("%s(): rejected: transaction fee too small [%d < %d]\n", __func__, feePaid, offer.getMinFee());
        return DEX_ERROR_ACCEPT -105;
    }

    if (offer.getOption() == 1)
    {   // if market maker is buying tokens

        if (msc_debug_dex)
        {
            PrintToLog("getProperty: %d\n",offer.getProperty());
            PrintToLog("getOfferAmountOriginal: %d\n",offer.getOfferAmountOriginal());
            PrintToLog("getLTCDesiredOriginal: %d\n",offer.getLTCDesiredOriginal());
        }

        if (amountAccepted > offer.getOfferAmountOriginal()) {
            amountAccepted = offer.getOfferAmountOriginal();
        }

        int64_t amountInBalance = getMPbalance(addressTaker, propertyId, BALANCE);

        if (msc_debug_dex) PrintToLog("%s(): amountInBalance: %d, amountAccepted: %d\n",__func__, amountInBalance, amountAccepted);

        if (amountInBalance == 0){
            PrintToLog("%s(): rejected: amount of token in seller  is zero \n",__func__);
            return rc;
        }

        if (amountInBalance >= amountAccepted) {
            assert(update_tally_map(addressTaker, propertyId, -amountAccepted, BALANCE));
            assert(update_tally_map(addressTaker, propertyId, amountAccepted, ACCEPT_RESERVE));
        } else {
            if (msc_debug_dex) PrintToLog("amountInBalance < amountAccepted ???\n");
        }

        CMPAccept acceptOffer(amountAccepted, block, offer.getBlockTimeLimit(), offer.getProperty(), offer.getOfferAmountOriginal(), offer.getLTCDesiredOriginal(), offer.getHash());
        my_accepts.insert(std::make_pair(keyAcceptOrder, acceptOffer));

        return 0;
    }

    int64_t amountReserved = 0;
    int64_t amountRemainingForSale = getMPbalance(addressMaker, propertyId, SELLOFFER_RESERVE);

    // ensure the buyer only can reserve the amount that is still available
    if (amountRemainingForSale >= amountAccepted) {
        amountReserved = amountAccepted;
    } else {
        amountReserved = amountRemainingForSale;
        if (msc_debug_dex) PrintToLog("%s(): buyer wants to reserve %d tokens, but only %d tokens are available\n", __func__, amountAccepted, amountRemainingForSale);
    }

    if (amountReserved > 0) {
        assert(update_tally_map(addressMaker, propertyId, -amountReserved, SELLOFFER_RESERVE));
        assert(update_tally_map(addressMaker, propertyId, amountReserved, ACCEPT_RESERVE));

        CMPAccept acceptOffer(amountReserved, block, offer.getBlockTimeLimit(), offer.getProperty(), offer.getOfferAmountOriginal(), offer.getLTCDesiredOriginal(), offer.getHash());
        my_accepts.insert(std::make_pair(keyAcceptOrder, acceptOffer));

        rc = 0;
    }

    if (nAmended) *nAmended = amountReserved;

    return rc;
}

/**
 * Finalizes an accepted offer.
 *
 * This function is called by handler_block() for each accepted offer that has
 * expired. This function is also called when the purchase has been completed,
 * and the buyer bought everything that was reserved.
 *
 * @return 0 if everything is OK
 */
int DEx_acceptDestroy(const std::string& addressBuyer, const std::string& addressSeller, uint32_t propertyid, bool fForceErase)
{
    int rc = DEX_ERROR_ACCEPT -20;
    CMPOffer* p_offer = DEx_getOffer(addressSeller, propertyid);
    CMPAccept* p_accept = DEx_getAccept(addressSeller, propertyid, addressBuyer);
    bool fReturnToMoney;

    if (!p_accept) return rc; // sanity check

    // if the offer is gone, ACCEPT_RESERVE should go back to seller's BALANCE
    // otherwise move the previously accepted amount back to SELLOFFER_RESERVE
    if (!p_offer) fReturnToMoney = true;

    PrintToLog("%s(): finalize trade [offer=%s, accept=%s]\n", __func__, p_offer->getHash().GetHex(), p_accept->getHash().GetHex());

    // offer exists, determine whether it's the original offer or some random new one
    fReturnToMoney = (p_offer->getHash() == p_accept->getHash()) ? false : true;

    const int64_t amountRemaining = p_accept->getAcceptAmountRemaining();


    if (amountRemaining > 0) {
        TallyType typ = (fReturnToMoney) ? BALANCE : SELLOFFER_RESERVE;
        assert(update_tally_map(addressSeller, propertyid, -amountRemaining, ACCEPT_RESERVE));
        assert(update_tally_map(addressSeller, propertyid, amountRemaining, typ));

    }

    // can only erase when is NOT called from an iterator loop
    if (fForceErase) {
        std::string key = STR_ACCEPT_ADDR_PROP_ADDR_COMBO(addressSeller, addressBuyer, propertyid);
        AcceptMap::iterator it = my_accepts.find(key);

        if (my_accepts.end() != it) {
            my_accepts.erase(it);
        }
    }

    return 0;
}


/**
 * Determines the purchased amount of tokens.
 *
 * The amount is rounded up, which may be in favor of the buyer, to avoid small
 * leftovers of 1 willet. This is not exploitable due to transaction fees.
 *
 * The desired amount must not be zero, and the purchased amount should be checked
 * against the actual amount that is still left for sale.
 *
 * @return The amount of tokens purchased
 */
int64_t calculateDExPurchase(const int64_t amountOffered, const int64_t amountDesired, const int64_t amountPaid)
{
    // conversion
    arith_uint256 amountOffered256 = ConvertTo256(amountOffered);
    arith_uint256 amountDesired256 = ConvertTo256(amountDesired);
    arith_uint256 amountPaid256 = ConvertTo256(amountPaid);

    // actual calculation; round up
    arith_uint256 amountPurchased256 = DivideAndRoundUp((amountPaid256 * amountOffered256), amountDesired256);

    // convert back to int64_t
    return ConvertTo64(amountPurchased256);
}

/**
 * Handles incoming LTC payment for the offer in tradelayer.cpp
 */
 int DEx_payment(const uint256& txid, unsigned int vout, const std::string& addressSeller, const std::string& addressBuyer, int64_t amountPaid, int block, uint64_t* nAmended)
 {
     if (msc_debug_dex) PrintToLog("%s(%s, %s)\n", __func__, addressSeller, addressBuyer);
     int rc = DEX_ERROR_PAYMENT;
     uint32_t propertyId;

     CMPAccept* p_accept = nullptr;

     // logic here: we look only into main properties if there's some match
     for (propertyId = 1; propertyId < _my_sps->peekNextSPID(); propertyId++)
     {
         CMPSPInfo::Entry sp;
         if (msc_debug_dex) PrintToLog("propertyId: %d\n",propertyId);
         if (!_my_sps->getSP(propertyId, sp)) continue;

         // seller market maker?
         p_accept = DEx_getAccept(addressSeller, propertyId, addressBuyer);

         if (p_accept)
         {
             if (msc_debug_dex) PrintToLog("Found seller market maker!\n");
             break;
         }

         // buyer market maker?
         if (IsFeatureActivated(FEATURE_DEX_BUY, block))
         {
             p_accept = DEx_getAccept(addressBuyer, propertyId, addressSeller);

             if (p_accept)
             {
                 if (msc_debug_dex) PrintToLog("Found buyer market maker!\n");
                 break;
             }

         }
     }

     if (!p_accept && msc_debug_dex)
     {
        PrintToLog("%s(): ERROR: there must be an active accept order for this payment, seller: %s, buyer: %s\n",__func__, addressSeller, addressBuyer);
        return (DEX_ERROR_PAYMENT -1);
     }

     // -------------------------------------------------------------------------
     const int64_t amountDesired = p_accept->getLTCDesiredOriginal();
     const int64_t amountOffered = p_accept->getOfferAmountOriginal();

     if (msc_debug_dex) PrintToLog("%s(): amountDesired : %d, amountOffered : %d\n",__func__, amountDesired, amountOffered);

     // divide by 0 protection
     if (0 == amountDesired && msc_debug_dex)
     {
         PrintToLog("%s(): ERROR: desired amount of accept order is zero", __func__);
         return (DEX_ERROR_PAYMENT -2);
     }

    /**
     * Plain integer math is used to determine
     * the purchased amount. The purchased amount is rounded up, which may be
     * in favor of the buyer, to avoid small leftovers of 1 willet.
     *
     * This is not exploitable due to transaction fees.
     */
      int64_t amountPurchased = mastercore::calculateDExPurchase(amountOffered, amountDesired, amountPaid);


     // -------------------------------------------------------------------------

     // adding LTC volume added by this property
     PrintToLog("%s(): block: %d, propertyId: %d. amountPaid (LTC): %d\n",__func__, block, propertyId, amountPaid);
     MapLTCVolume[block][propertyId] += amountPaid;

     const arith_uint256 amountDesired256  = ConvertTo256(amountDesired);
     const arith_uint256 amountOffered256 = ConvertTo256(amountOffered);
     const arith_uint256 unitPrice256 = (ConvertTo256(COIN) * amountDesired256) / amountOffered256;

     const int64_t unitPrice = (isPropertyDivisible(propertyId)) ? ConvertTo64(unitPrice256) : ConvertTo64(unitPrice256) / COIN;

     // adding last price
     lastPrice[propertyId] = unitPrice;

     // adding numerator of vwap
     tokenvwap[propertyId][block].push_back(std::make_pair(unitPrice, amountPurchased));

     // saving DEx token volume
     MapTokenVolume[block][propertyId] += amountPurchased;


     // adding Last token/ ltc price
     rational_t market_pricetokens_LTC(amountPurchased, amountPaid);
     int64_t market_p_tokens_LTC = mastercore::RationalToInt64(market_pricetokens_LTC);
     market_priceMap[LTC][propertyId] = market_p_tokens_LTC;

     if(msc_debug_dex) PrintToLog("%s(): amountPaid for propertyId : %d,  inside MapLTCVolume: %d, market_p_tokens_LTC : %d\n", __func__, propertyId, amountPaid, market_p_tokens_LTC);

     // -------------------------------------------------------------------------
     if (msc_debug_dex) PrintToLog("amountPurchased: %d\n",amountPurchased);
     const int64_t amountRemaining = p_accept->getAcceptAmountRemaining(); // actual amount desired, in the Accept

     if (msc_debug_dex) PrintToLog(
     "%s(): LTC desired: %s, offered amount: %s, amount to purchase: %s, amount remaining: %s\n", __func__,
     FormatDivisibleMP(amountDesired), FormatDivisibleMP(amountOffered),
     FormatDivisibleMP(amountPurchased), FormatDivisibleMP(amountRemaining));


     // if units_purchased is greater than what's in the Accept, the buyer gets only what's in the Accept
     if (amountRemaining < amountPurchased)
     {
         amountPurchased = amountRemaining;
         if (nAmended) *nAmended = amountPurchased;
     }

     if (msc_debug_dex) PrintToLog("amountPurchased: %d",amountPurchased);

     if (amountPurchased > 0)
     {
         if(msc_debug_dex) PrintToLog("%s(): seller %s offered %s %s for %s LTC\n", __func__,
 	      addressSeller, FormatDivisibleMP(amountOffered), strMPProperty(propertyId), FormatDivisibleMP(amountDesired));

         if(msc_debug_dex) PrintToLog("%s: buyer %s pays %s LTC to purchase %s %s\n", __func__,
 	      addressBuyer, FormatDivisibleMP(amountPaid), FormatDivisibleMP(amountPurchased), strMPProperty(propertyId));

         assert(update_tally_map(addressSeller, propertyId, -amountPurchased, ACCEPT_RESERVE));
         assert(update_tally_map(addressBuyer, propertyId, amountPurchased, BALANCE));

         if(msc_debug_dex) PrintToLog("AmountPurchased : %d\n",amountPurchased);

         bool valid = true;
         p_txlistdb->recordPaymentTX(txid, valid, block, vout, propertyId, amountPurchased, amountPaid, addressBuyer, addressSeller);

         rc = 0;
         if(msc_debug_dex) PrintToLog("#######################################################\n");
     }

     // reduce the amount of units still desired by the buyer and if 0 destroy the Accept order
     if (p_accept->reduceAcceptAmountRemaining_andIsZero(amountPurchased))
     {
         if(msc_debug_dex) PrintToLog("p_accept->reduceAcceptAmountRemaining_andIsZero true\n");

         const int64_t reserveSell = getMPbalance(addressSeller, propertyId, SELLOFFER_RESERVE);
         const int64_t reserveAccept = getMPbalance(addressSeller, propertyId, ACCEPT_RESERVE);

         if(msc_debug_dex) PrintToLog("reserveSell: %d, reserveAccept: %d\n",reserveSell, reserveAccept);


         DEx_acceptDestroy(addressBuyer, addressSeller, propertyId, true);

         // delete the Offer object if there is nothing in its Reserves -- everything got puchased and paid for
         if (0 == reserveSell && 0 == reserveAccept)
         {
             if(msc_debug_dex) PrintToLog("0 == reserveSell && 0 == reserveAccept true\n");
             DEx_offerDestroy(addressSeller, propertyId);
         }

         rc = 0;
     }

     return rc;
 }


unsigned int eraseExpiredAccepts(int blockNow)
{
    unsigned int how_many_erased = 0;
    AcceptMap::iterator it = my_accepts.begin();

    while (my_accepts.end() != it) {
        const CMPAccept& acceptOrder = it->second;

        int blocksSinceAccept = blockNow - acceptOrder.getAcceptBlock();
        int blocksPaymentWindow = static_cast<int>(acceptOrder.getBlockTimeLimit());

        if (blocksSinceAccept >= blocksPaymentWindow) {
            PrintToLog("%s(): erasing at block: %d, order confirmed at block: %d, payment window: %d\n",
                    __func__, blockNow, acceptOrder.getAcceptBlock(), acceptOrder.getBlockTimeLimit());

            // extract the seller, buyer and property from the key
            std::vector<std::string> vstr(3);
            boost::split(vstr, it->first, boost::is_any_of("-+"), boost::token_compress_on);
            const std::string& addressSeller = vstr[0];
            uint32_t propertyId = atoi(vstr[1]);
            const std::string& addressBuyer = vstr[2];

            DEx_acceptDestroy(addressBuyer, addressSeller, propertyId);

            my_accepts.erase(it++);

            ++how_many_erased;

        } else it++;
    }

    return how_many_erased;
}


} // namespace mastercore
