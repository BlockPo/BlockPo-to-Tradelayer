/**
 * @file rpctxobject.cpp
 *
 * Handler for populating RPC transaction objects.
 */

#include <tradelayer/rpctxobject.h>

#include <tradelayer/dex.h>
#include <tradelayer/errors.h>
#include <tradelayer/pending.h>
#include <tradelayer/rpctxobject.h>
#include <tradelayer/sp.h>
#include <tradelayer/tradelayer.h>
#include <tradelayer/tx.h>
#include <tradelayer/utilsbitcoin.h>
#include <tradelayer/wallettxs.h>

#include <chainparams.h>
#include <primitives/transaction.h>
#include <sync.h>
#include <uint256.h>
#include <validation.h>

#include <stdint.h>
#include <string>
#include <univalue.h>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

// Namespaces
using namespace mastercore;

/**
 * Function to standardize RPC output for transactions into a JSON object in either basic or extended mode.
 *
 * Use basic mode for generic calls (e.g. tl_gettransaction/tl_listtransaction etc.)
 * Use extended mode for transaction specific calls (e.g. tl_getsto, tl_gettrade etc.)
 *
 * DEx payments and the extended mode are only available for confirmed transactions.
 */
int populateRPCTransactionObject(const uint256& txid, UniValue& txobj, std::string filterAddress, bool extendedDetails, std::string extendedDetailsFilter)
{
    // retrieve the transaction from the blockchain and obtain it's height/confs/time
    CTransactionRef tx;
    uint256 blockHash;
    if (!GetTransaction(txid, tx, Params().GetConsensus(), blockHash, true)) {
        return MP_TX_NOT_FOUND;
    }
    const CTransaction txp = *(tx) ;
    return populateRPCTransactionObject(txp, blockHash, txobj, filterAddress, extendedDetails, extendedDetailsFilter);
}

int populateRPCTransactionObject(const CTransaction& tx, const uint256& blockHash, UniValue& txobj, std::string filterAddress, bool extendedDetails, std::string extendedDetailsFilter, int blockHeight)
{
    int confirmations = 0;
    int64_t blockTime = 0;
    int positionInBlock = 0;

    if(blockHeight == 0){
        blockHeight = GetHeight();
    }

    if(!blockHash.IsNull())
    {
        CBlockIndex* pBlockIndex = GetBlockIndex(blockHash);
        if (nullptr != pBlockIndex)
        {
            confirmations = 1 + blockHeight - pBlockIndex->nHeight;
            blockTime = pBlockIndex->nTime;
            blockHeight = pBlockIndex->nHeight;
        }
    }

    // attempt to parse the transaction
    CMPTransaction mp_obj;
    int parseRC = ParseTransaction(tx, blockHeight, 0, mp_obj, blockTime);
    if (parseRC < 0) return MP_TX_IS_NOT_MASTER_PROTOCOL;

    const uint256& txid = tx.GetHash();

    // check if we're filtering from listtransactions_MP, and if so whether we have a non-match we want to skip
    if (!filterAddress.empty() && mp_obj.getSender() != filterAddress && mp_obj.getReceiver() != filterAddress) return -1;

    // parse packet and populate mp_obj
    if (!mp_obj.interpret_Transaction()) return MP_TX_IS_NOT_MASTER_PROTOCOL;

    // obtain validity - only confirmed transactions can be valid
    bool valid = false;
    std::string reason;

    if (confirmations > 0)
    {
        LOCK(cs_tally);
        valid = getValidMPTX(txid, &reason);
        positionInBlock = p_TradeTXDB->FetchTransactionPosition(txid);
    }

    if (msc_debug_populate_rpc_transaction_obj)
    {
        PrintToLog("Checking valid : %s\n", valid ? "true" : "false");
        PrintToLog("Reason for invalid tx : %s\n", reason);
        PrintToLog("Checking positionInBlock : %d\n", positionInBlock);
    }

    // populate some initial info for the transaction
    bool fMine = false;
    if (IsMyAddress(mp_obj.getSender()) || IsMyAddress(mp_obj.getReceiver())) fMine = true;

    txobj.push_back(Pair("txid", txid.GetHex()));
    txobj.push_back(Pair("fee", FormatDivisibleMP(mp_obj.getFeePaid())));
    txobj.push_back(Pair("sendingaddress", mp_obj.getSender()));

    if (showRefForTx(mp_obj.getType())) txobj.push_back(Pair("referenceaddress", mp_obj.getReceiver()));

    txobj.push_back(Pair("ismine", fMine));
    txobj.push_back(Pair("version", (uint64_t)mp_obj.getVersion()));
    txobj.push_back(Pair("type_int", (uint64_t)mp_obj.getType()));

    if (mp_obj.getType() != MSC_TYPE_SIMPLE_SEND) { // Type 0 will add "Type" attribute during populateRPCTypeSimpleSend
      txobj.push_back(Pair("type", mp_obj.getTypeString()));
    }

    // populate type specific info and extended details if requested
    // extended details are not available for unconfirmed transactions
    if (confirmations <= 0) extendedDetails = false;

    if(!populateRPCTypeInfo(mp_obj, txobj, mp_obj.getType(), extendedDetails, extendedDetailsFilter)) return MP_TX_NOT_FOUND;

    // state and chain related information
    if (confirmations != 0 && !blockHash.IsNull())
    {
        txobj.push_back(Pair("valid", valid));
        if (!valid) txobj.push_back(Pair("invalidation reason", reason));
        txobj.push_back(Pair("blockhash", blockHash.GetHex()));
        txobj.push_back(Pair("blocktime", blockTime));
        txobj.push_back(Pair("positioninblock", positionInBlock));
    }

    if (confirmations != 0) {
        txobj.push_back(Pair("block", blockHeight));
    }

    txobj.push_back(Pair("confirmations", confirmations));

    // finished
    return 0;
}

/* Function to call respective populators based on message type
 */
bool populateRPCTypeInfo(CMPTransaction& mp_obj, UniValue& txobj, uint32_t txType, bool extendedDetails, std::string extendedDetailsFilter)
{
    //NOTE: populateRPCType functions needs more work
    switch (txType) {
        case MSC_TYPE_SIMPLE_SEND:
            populateRPCTypeSimpleSend(mp_obj, txobj);
            return true;

        case MSC_TYPE_SEND_MANY:
            populateRPCTypeSendMany(mp_obj, txobj);
            return true;

        case MSC_TYPE_SEND_ALL:
            populateRPCTypeSendAll(mp_obj, txobj);
            return true;

        case MSC_TYPE_CREATE_PROPERTY_FIXED:
            populateRPCTypeCreatePropertyFixed(mp_obj, txobj);
            return true;

        case MSC_TYPE_CREATE_PROPERTY_VARIABLE:
            populateRPCTypeCreatePropertyVariable(mp_obj, txobj);
            return true;

        case MSC_TYPE_CREATE_PROPERTY_MANUAL:
            populateRPCTypeCreatePropertyManual(mp_obj, txobj);
            return true;

        case MSC_TYPE_GRANT_PROPERTY_TOKENS:
            populateRPCTypeGrant(mp_obj, txobj);
            return true;

        case MSC_TYPE_REVOKE_PROPERTY_TOKENS:
            populateRPCTypeRevoke(mp_obj, txobj);
            return true;

        case MSC_TYPE_CHANGE_ISSUER_ADDRESS:
            populateRPCTypeChangeIssuer(mp_obj, txobj);
            return true;

        case TL_MESSAGE_TYPE_ACTIVATION:
            populateRPCTypeActivation(mp_obj, txobj);
            return true;

        case MSC_TYPE_CREATE_CONTRACT:
            populateRPCTypeCreateContract(mp_obj, txobj);
            return true;

        case MSC_TYPE_SEND_VESTING:
            populateRPCTypeVestingTokens(mp_obj, txobj);
            return true;

        case MSC_TYPE_CREATE_ORACLE_CONTRACT:
            populateRPCTypeCreateOracle(mp_obj, txobj);
            return true;

        case MSC_TYPE_METADEX_TRADE:
            populateRPCTypeMetaDExTrade(mp_obj, txobj);
            return true;

        case MSC_TYPE_CONTRACTDEX_TRADE:
            populateRPCTypeContractDexTrade(mp_obj, txobj);
            return true;

        case MSC_TYPE_CONTRACTDEX_CANCEL_ECOSYSTEM:
            populateRPCTypeContractDexCancelEcosystem(mp_obj, txobj);
            return true;

        case MSC_TYPE_PEGGED_CURRENCY:
            populateRPCTypeCreatePeggedCurrency(mp_obj, txobj);
            return true;

        case MSC_TYPE_SEND_PEGGED_CURRENCY:
            populateRPCTypeSendPeggedCurrency(mp_obj, txobj);
            return true;

        case MSC_TYPE_REDEMPTION_PEGGED:
            populateRPCTypeRedemptionPegged(mp_obj, txobj);
            return true;

        case MSC_TYPE_CONTRACTDEX_CLOSE_POSITION:
            populateRPCTypeContractDexClosePosition(mp_obj, txobj);
            return true;

        case MSC_TYPE_CONTRACTDEX_CANCEL_ORDERS_BY_BLOCK:
            populateRPCTypeContractDex_Cancel_Orders_By_Block(mp_obj, txobj);
            return true;

        case MSC_TYPE_DEX_SELL_OFFER:
            populateRPCTypeTradeOffer(mp_obj, txobj);
            return true;

        case MSC_TYPE_DEX_BUY_OFFER:
            populateRPCTypeDExBuy(mp_obj, txobj);
            return true;

        case MSC_TYPE_ACCEPT_OFFER_BTC:
            populateRPCTypeAcceptOfferBTC(mp_obj, txobj);
            return true;

        case MSC_TYPE_CHANGE_ORACLE_REF:
            populateRPCTypeChange_OracleRef(mp_obj, txobj);
            return true;

        case MSC_TYPE_SET_ORACLE:
            populateRPCTypeSet_Oracle(mp_obj, txobj);
            return true;

        case MSC_TYPE_ORACLE_BACKUP:
            populateRPCTypeOracleBackup(mp_obj, txobj);
            return true;

        case MSC_TYPE_CLOSE_ORACLE:
            populateRPCTypeCloseOracle(mp_obj, txobj);
            return true;

        case MSC_TYPE_COMMIT_CHANNEL:
            populateRPCTypeCommitChannel(mp_obj, txobj);
            return true;

        case MSC_TYPE_WITHDRAWAL_FROM_CHANNEL:
            populateRPCTypeWithdrawal_FromChannel(mp_obj, txobj);
            return true;

        case MSC_TYPE_INSTANT_TRADE:
            populateRPCTypeInstant_Trade(mp_obj, txobj);
            return true;

        case MSC_TYPE_INSTANT_LTC_TRADE:
            populateRPCTypeInstant_LTC_Trade(mp_obj, txobj);
            return true;

        case MSC_TYPE_TRANSFER:
            populateRPCTypeTransfer(mp_obj, txobj);
            return true;

        case MSC_TYPE_CONTRACT_INSTANT:
            populateRPCTypeContract_Instant(mp_obj, txobj);
            return true;

        case MSC_TYPE_NEW_ID_REGISTRATION:
            populateRPCTypeNew_Id_Registration(mp_obj, txobj);
            return true;

        case MSC_TYPE_UPDATE_ID_REGISTRATION:
            populateRPCTypeUpdate_Id_Registration(mp_obj, txobj);
            return true;

        case MSC_TYPE_DEX_PAYMENT:
            populateRPCTypeDEx_Payment(mp_obj, txobj);
            return true;

        case MSC_TYPE_CLOSE_CHANNEL:
            populateRPCTypeClose_Channel(mp_obj, txobj);
            return true;
        
        case MSC_TYPE_LITECOIN_PAYMENT:
            populateRPCTypeLitecoinPayment(mp_obj, txobj);
            return true;

        default:
            return false;
    }
}

/* Function to determine whether to display the reference address based on transaction type
 */
bool showRefForTx(uint32_t txType)
{
    switch (txType) {
        case MSC_TYPE_SIMPLE_SEND: return true;
        case MSC_TYPE_SEND_MANY: return true;
        case MSC_TYPE_CREATE_PROPERTY_FIXED: return false;
        case MSC_TYPE_CREATE_PROPERTY_MANUAL: return false;
        case MSC_TYPE_GRANT_PROPERTY_TOKENS: return true;
        case MSC_TYPE_REVOKE_PROPERTY_TOKENS: return false;
        case MSC_TYPE_CHANGE_ISSUER_ADDRESS: return true;
        case MSC_TYPE_SEND_ALL: return true;
        case MSC_TYPE_LITECOIN_PAYMENT: return false;
        case TL_MESSAGE_TYPE_ACTIVATION: return false;
    }
    return true; // default to true, shouldn't be needed but just in case
}

void populateRPCTypeSimpleSend(CMPTransaction& tlObj, UniValue& txobj)
{
    uint32_t propertyId = tlObj.getProperty();
    int64_t crowdPropertyId = 0, crowdTokens = 0, issuerTokens = 0;
    
    LOCK(cs_tally);
    
    if (isCrowdsalePurchase(tlObj.getHash(), tlObj.getReceiver(), &crowdPropertyId, &crowdTokens, &issuerTokens)) {
        CMPSPInfo::Entry sp;
        if (false == _my_sps->getSP(crowdPropertyId, sp)) {
            PrintToLog("SP Error: Crowdsale purchase for non-existent property %d in transaction %s", crowdPropertyId, tlObj.getHash().GetHex());
            return;
        }

        txobj.push_back(Pair("type", "Crowdsale Purchase"));
        txobj.push_back(Pair("propertyid", (uint64_t)propertyId));
        txobj.push_back(Pair("divisible", isPropertyDivisible(propertyId)));
        txobj.push_back(Pair("amount", FormatMP(propertyId, tlObj.getAmount())));
        txobj.push_back(Pair("purchasedpropertyid", crowdPropertyId));
        txobj.push_back(Pair("purchasedpropertyname", sp.name));
        txobj.push_back(Pair("purchasedpropertydivisible", sp.isDivisible()));
        txobj.push_back(Pair("purchasedtokens", FormatMP(crowdPropertyId, crowdTokens)));
        txobj.push_back(Pair("issuertokens", FormatMP(crowdPropertyId, issuerTokens)));
    } else {
        txobj.push_back(Pair("type", "Simple Send"));
        txobj.push_back(Pair("propertyid", (uint64_t)propertyId));
        txobj.push_back(Pair("divisible", isPropertyDivisible(propertyId)));
        txobj.push_back(Pair("amount", FormatMP(propertyId, tlObj.getAmount())));
    }
}

void populateRPCTypeSendMany(CMPTransaction& tlObj, UniValue& txobj)
{
    uint32_t propertyId = tlObj.getProperty();
    LOCK(cs_tally);
    txobj.push_back(Pair("type", "Send Many"));
    txobj.push_back(Pair("propertyid", (uint64_t)propertyId));
    txobj.push_back(Pair("divisible", isPropertyDivisible(propertyId)));
    txobj.push_back(Pair("total", FormatMP(propertyId, tlObj.getAmountTotal())));
}

void populateRPCTypeSendAll(CMPTransaction& tlObj, UniValue& txobj)
{
    UniValue subSends(UniValue::VARR);
    if (populateRPCSendAllSubSends(tlObj.getHash(), subSends) > 0) txobj.push_back(Pair("subsends", subSends));
}

void populateRPCTypeCreatePropertyFixed(CMPTransaction& tlObj, UniValue& txobj)
{
    LOCK(cs_tally);
    uint32_t propertyId = _my_sps->findSPByTX(tlObj.getHash());
    if (propertyId > 0) txobj.push_back(Pair("propertyid", (uint64_t) propertyId));
    if (propertyId > 0) txobj.push_back(Pair("divisible", isPropertyDivisible(propertyId)));

    txobj.push_back(Pair("propertytype", strPropertyType(tlObj.getPropertyType())));
    txobj.push_back(Pair("propertyname", tlObj.getSPName()));
    txobj.push_back(Pair("data", tlObj.getSPData()));
    txobj.push_back(Pair("url", tlObj.getSPUrl()));
    const std::string strAmount = FormatByType(tlObj.getAmount(), tlObj.getPropertyType());
    txobj.push_back(Pair("amount", strAmount));
}

void populateRPCTypeCreatePropertyVariable(CMPTransaction& tlObj, UniValue& txobj)
{
    LOCK(cs_tally);
    uint32_t propertyId = _my_sps->findSPByTX(tlObj.getHash());
    if (propertyId > 0) txobj.push_back(Pair("propertyid", (uint64_t) propertyId));
    if (propertyId > 0) txobj.push_back(Pair("divisible", isPropertyDivisible(propertyId)));

    txobj.push_back(Pair("propertytype", strPropertyType(tlObj.getPropertyType())));
    txobj.push_back(Pair("category", tlObj.getSPCategory()));
    txobj.push_back(Pair("subcategory", tlObj.getSPSubCategory()));
    txobj.push_back(Pair("propertyname", tlObj.getSPName()));
    txobj.push_back(Pair("data", tlObj.getSPData()));
    txobj.push_back(Pair("url", tlObj.getSPUrl()));
    txobj.push_back(Pair("propertyiddesired", (uint64_t) tlObj.getProperty()));
    std::string strPerUnit = FormatMP(tlObj.getProperty(), tlObj.getAmount());
    txobj.push_back(Pair("tokensperunit", strPerUnit));
    txobj.push_back(Pair("deadline", tlObj.getDeadline()));
    txobj.push_back(Pair("earlybonus", tlObj.getEarlyBirdBonus()));
    txobj.push_back(Pair("percenttoissuer", tlObj.getIssuerBonus()));
    std::string strAmount = FormatByType(0, tlObj.getPropertyType());
    txobj.push_back(Pair("amount", strAmount)); // crowdsale token creations don't issue tokens with the create tx
}

void populateRPCTypeCreatePropertyManual(CMPTransaction& tlObj, UniValue& txobj)
{
    LOCK(cs_tally);
    uint32_t propertyId = _my_sps->findSPByTX(tlObj.getHash());
    if (propertyId > 0) txobj.push_back(Pair("propertyid", (uint64_t) propertyId));
    if (propertyId > 0) txobj.push_back(Pair("divisible", isPropertyDivisible(propertyId)));

    txobj.push_back(Pair("propertytype", strPropertyType(tlObj.getPropertyType())));
    txobj.push_back(Pair("propertyname", tlObj.getSPName()));
    txobj.push_back(Pair("data", tlObj.getSPData()));
    txobj.push_back(Pair("url", tlObj.getSPUrl()));
    const std::string strAmount = FormatByType(0, tlObj.getPropertyType());
    txobj.push_back(Pair("amount", strAmount)); // managed token creations don't issue tokens with the create tx
}

void populateRPCTypeGrant(CMPTransaction& tlObj, UniValue& txobj)
{
    uint32_t propertyId = tlObj.getProperty();
    txobj.push_back(Pair("propertyid", (uint64_t)propertyId));
    txobj.push_back(Pair("divisible", isPropertyDivisible(propertyId)));
    txobj.push_back(Pair("amount", FormatMP(propertyId, tlObj.getAmount())));
}

void populateRPCTypeRevoke(CMPTransaction& tlObj, UniValue& txobj)
{
    uint32_t propertyId = tlObj.getProperty();
    txobj.push_back(Pair("propertyid", (uint64_t)propertyId));
    txobj.push_back(Pair("divisible", isPropertyDivisible(propertyId)));
    txobj.push_back(Pair("amount", FormatMP(propertyId, tlObj.getAmount())));
}

void populateRPCTypeChangeIssuer(CMPTransaction& tlObj, UniValue& txobj)
{
    uint32_t propertyId = tlObj.getProperty();
    txobj.push_back(Pair("propertyid", (uint64_t)propertyId));
    txobj.push_back(Pair("divisible", isPropertyDivisible(propertyId)));
}

void populateRPCTypeActivation(CMPTransaction& tlObj, UniValue& txobj)
{
    txobj.push_back(Pair("featureid", (uint64_t) tlObj.getFeatureId()));
    txobj.push_back(Pair("activationblock", (uint64_t) tlObj.getActivationBlock()));
    txobj.push_back(Pair("minimumversion", (uint64_t) tlObj.getMinClientVersion()));
}

/* Function to enumerate sub sends for a given txid and add to supplied JSON array
 * Note: this function exists as send all has the potential to carry multiple sends in a single transaction.
 */
int populateRPCSendAllSubSends(const uint256& txid, UniValue& subSends)
{
    int numberOfSubSends = 0;
    {
        LOCK(cs_tally);
        numberOfSubSends = p_txlistdb->getNumberOfSubRecords(txid);
    }
    if (numberOfSubSends <= 0) {
        PrintToLog("TXLISTDB Error: Transaction %s parsed as a send all but could not locate sub sends in txlistdb.\n", txid.GetHex());
        return -1;
    }
    for (int subSend = 1; subSend <= numberOfSubSends; subSend++)
    {
        UniValue subSendObj(UniValue::VOBJ);
        uint32_t propertyId = 0;
        int64_t amount = 0;
        {
            LOCK(cs_tally);
            p_txlistdb->getSendAllDetails(txid, subSend, propertyId, amount);
        }
        subSendObj.push_back(Pair("propertyid", (uint64_t)propertyId));
        subSendObj.push_back(Pair("divisible", isPropertyDivisible(propertyId)));
        subSendObj.push_back(Pair("amount", FormatMP(propertyId, amount)));
        subSends.push_back(subSendObj);
    }

    return subSends.size();
}

void populateRPCTypeCreateContract(CMPTransaction& tlObj, UniValue& txobj)
{
    txobj.push_back(Pair("contract name", tlObj.getSPName()));
    txobj.push_back(Pair("notional size", FormatDivisibleMP(tlObj.getNotionalSize())));
    txobj.push_back(Pair("collateral currency", (uint64_t) tlObj.getCollateral()));
    txobj.push_back(Pair("margin requirement", FormatDivisibleMP(tlObj.getMarginRequirement())));
    txobj.push_back(Pair("blocks until expiration", (uint64_t) tlObj.getBlockUntilExpiration()));

}

void populateRPCTypeVestingTokens(CMPTransaction& tlObj, UniValue& txobj)
{
    txobj.push_back(Pair("propertyname", tlObj.getSPName()));
    txobj.push_back(Pair("propertyId", (uint64_t) tlObj.getProperty()));

}

void populateRPCTypeCreateOracle(CMPTransaction& tlObj, UniValue& txobj)
{
    txobj.push_back(Pair("contract name", tlObj.getSPName()));
    txobj.push_back(Pair("notional size", FormatDivisibleMP(tlObj.getNotionalSize())));
    txobj.push_back(Pair("collateral currency", (uint64_t) tlObj.getCollateral()));
    txobj.push_back(Pair("margin requirement", FormatDivisibleMP(tlObj.getMarginRequirement())));
    txobj.push_back(Pair("blocks until expiration", (uint64_t) tlObj.getBlockUntilExpiration()));
    txobj.push_back(Pair("backup address", tlObj.getReceiver()));
    txobj.push_back(Pair("hight price", FormatDivisibleMP(tlObj.getHighPrice())));
    txobj.push_back(Pair("low price", FormatDivisibleMP(tlObj.getLowPrice())));

}

void populateRPCTypeMetaDExTrade(CMPTransaction& tlObj, UniValue& txobj)
{
    const std::string name = getPropertyName((uint32_t) tlObj.getProperty());
    txobj.push_back(Pair("propertyname", name));
    txobj.push_back(Pair("propertyId", (uint64_t) tlObj.getProperty()));
    txobj.push_back(Pair("amount", FormatDivisibleMP(tlObj.getAmountForSale())));
    txobj.push_back(Pair("desire property", (uint64_t) tlObj.getDesiredProperty()));
    txobj.push_back(Pair("desired value", FormatDivisibleMP(tlObj.getDesiredValue())));

}

void populateRPCTypeContractDexTrade(CMPTransaction& tlObj, UniValue& txobj)
{
    txobj.push_back(Pair("contractId", (uint64_t) tlObj.getPropertyId()));
    txobj.push_back(Pair("amount", FormatDivisibleMP(tlObj.getContractAmount())));
    txobj.push_back(Pair("effective price", FormatDivisibleMP(tlObj.getEffectivePrice())));

    const std::string action = (tlObj.getTradingAction() == static_cast<uint8_t>(buy)) ? "buy" : "sell";
    txobj.push_back(Pair("trading action", action));
    txobj.push_back(Pair("leverage",FormatDivisibleMP(tlObj.getLeverage())));

}


void populateRPCTypeContractDexCancelEcosystem(CMPTransaction& tlObj, UniValue& txobj)
{
    txobj.push_back(Pair("cancel all orders",""));
}

void populateRPCTypeCreatePeggedCurrency(CMPTransaction& tlObj, UniValue& txobj)
{
    txobj.push_back(Pair("propertyId", (uint64_t) tlObj.getPropertyId()));
    txobj.push_back(Pair("amount", FormatDivisibleMP(tlObj.getXAmount())));
    txobj.push_back(Pair("contract related", (uint64_t) tlObj.getContractId()));
}

void populateRPCTypeSendPeggedCurrency(CMPTransaction& tlObj, UniValue& txobj)
{
    txobj.push_back(Pair("propertyId", (uint64_t) tlObj.getPropertyId()));
    txobj.push_back(Pair("amount", FormatDivisibleMP(tlObj.getXAmount())));
}

void populateRPCTypeRedemptionPegged(CMPTransaction& tlObj, UniValue& txobj)
{
    uint32_t propertyId = tlObj.getProperty();
    txobj.push_back(Pair("propertyId", (uint64_t) propertyId));
    txobj.push_back(Pair("amount", FormatDivisibleMP(tlObj.getXAmount())));
    txobj.push_back(Pair("contract related", (uint64_t) tlObj.getContractId()));
}

void populateRPCTypeContractDexClosePosition(CMPTransaction& tlObj, UniValue& txobj)
{
    txobj.push_back(Pair("contractId", (uint64_t) tlObj.getContractId()));
}

void populateRPCTypeContractDex_Cancel_Orders_By_Block(CMPTransaction& tlObj, UniValue& txobj)
{
    txobj.push_back(Pair("block", (uint64_t) tlObj.getBlock()));
    txobj.push_back(Pair("block index", (uint64_t) tlObj.getIndexInBlock()));
}

void populateRPCTypeTradeOffer(CMPTransaction& tlObj, UniValue& txobj)
{
    CMPOffer temp_offer(tlObj);
    uint32_t propertyId = tlObj.getProperty();
    int64_t amountOffered = tlObj.getAmount();
    int64_t amountDesired = temp_offer.getLTCDesiredOriginal();
    uint8_t sellSubAction = temp_offer.getSubaction();

    txobj.push_back(Pair("propertyId", (uint64_t) propertyId));
    txobj.push_back(Pair("amount offered", FormatDivisibleMP(amountOffered)));
    txobj.push_back(Pair("amount desired", FormatDivisibleMP(amountDesired)));
    txobj.push_back(Pair("time limit", (uint64_t) tlObj.getTimeLimit()));
    txobj.push_back(Pair("min fee", FormatDivisibleMP(tlObj.getMinFee())));
    txobj.push_back(Pair("subAction", (uint64_t) sellSubAction));
}

void populateRPCTypeDExBuy(CMPTransaction& tlObj, UniValue& txobj)
{
    uint32_t propertyId = _my_sps->findSPByTX(tlObj.getHash());
    txobj.push_back(Pair("propertyId", (uint64_t) propertyId));
    txobj.push_back(Pair("amount offered", (uint64_t) tlObj.getAmount()));
    txobj.push_back(Pair("effective price", (uint64_t) tlObj.getEffectivePrice()));
    txobj.push_back(Pair("time limit", (uint64_t) tlObj.getTimeLimit()));
    txobj.push_back(Pair("min fee", (uint64_t) tlObj.getMinFee()));
    txobj.push_back(Pair("subAction", (uint64_t) tlObj.getSubAction()));
}

void populateRPCTypeAcceptOfferBTC(CMPTransaction& tlObj, UniValue& txobj)
{
    uint32_t propertyId = tlObj.getProperty();
    uint64_t amount = tlObj.getAmount();

    int tmpblock = 0;
    uint32_t tmptype = 0;
    uint64_t amountNew = 0;
    std::string reason;

    LOCK(cs_tally);
    bool tmpValid = mastercore::getValidMPTX(tlObj.getHash(), &reason, &tmpblock, &tmptype, &amountNew);
    if (tmpValid && amountNew > 0) amount = amountNew;

    const uint64_t uAmount = static_cast<uint64_t>(amount);

    txobj.push_back(Pair("propertyId", (uint64_t) propertyId));
    txobj.push_back(Pair("amount", FormatDivisibleMP(uAmount)));

}

void populateRPCTypeChange_OracleRef(CMPTransaction& tlObj, UniValue& txobj)
{
    txobj.push_back(Pair("contractId", (uint64_t) tlObj.getContractId()));
}

void populateRPCTypeSet_Oracle(CMPTransaction& tlObj, UniValue& txobj)
{
    txobj.push_back(Pair("contractId", (uint64_t) tlObj.getContractId()));
    txobj.push_back(Pair("high price", (uint64_t) tlObj.getHighPrice()));
    txobj.push_back(Pair("low price", (uint64_t) tlObj.getLowPrice()));
}

void populateRPCTypeOracleBackup(CMPTransaction& tlObj, UniValue& txobj)
{
    txobj.push_back(Pair("contractId", (uint64_t) tlObj.getContractId()));
}

void populateRPCTypeCloseOracle(CMPTransaction& tlObj, UniValue& txobj)
{
    txobj.push_back(Pair("contractId", (uint64_t) tlObj.getContractId()));
}

void populateRPCTypeCommitChannel(CMPTransaction& tlObj, UniValue& txobj)
{
    txobj.push_back(Pair("propertyId", (uint64_t) tlObj.getPropertyId()));
    txobj.push_back(Pair("amount_commited", FormatMP(tlObj.getPropertyId(),tlObj.getAmountCommited())));
}

void populateRPCTypeWithdrawal_FromChannel(CMPTransaction& tlObj, UniValue& txobj)
{
    txobj.push_back(Pair("propertyId", (uint64_t) tlObj.getPropertyId()));
    txobj.push_back(Pair("amount to withdraw", (uint64_t) tlObj.getAmountToWith()));
}

void populateRPCTypeInstant_Trade(CMPTransaction& tlObj, UniValue& txobj)
{
    txobj.push_back(Pair("first address",  tlObj.getSpecial()));
    txobj.push_back(Pair("second address",  tlObj.getReceiver()));
    txobj.push_back(Pair("propertyId", (uint64_t) tlObj.getProperty()));
    txobj.push_back(Pair("amount for sale", FormatMP(tlObj.getProperty(), tlObj.getAmountForSale())));
    txobj.push_back(Pair("block for expiry", (uint64_t) tlObj.getBlockForExpiry()));
    txobj.push_back(Pair("desired property", (uint64_t) tlObj.getDesiredProperty()));
    txobj.push_back(Pair("desired value", FormatMP(tlObj.getDesiredProperty(), tlObj.getDesiredValue())));
}

void populateRPCTypeInstant_LTC_Trade(CMPTransaction& tlObj, UniValue& txobj)
{
    txobj.push_back(Pair("propertyId", (uint64_t) tlObj.getProperty()));
    txobj.push_back(Pair("amount for sale", FormatMP(tlObj.getProperty(), tlObj.getAmountForSale())));
    txobj.push_back(Pair("ltc price", FormatDivisibleMP(tlObj.getPrice())));
}

void populateRPCTypeClose_Channel(CMPTransaction& tlObj, UniValue& txobj)
{
}

void populateRPCTypeTransfer(CMPTransaction& tlObj, UniValue& txobj)
{
}

void populateRPCTypeCreate_Channel(CMPTransaction& tlObj, UniValue& txobj)
{
    txobj.push_back(Pair("first address",  tlObj.getSender()));
    txobj.push_back(Pair("second address", tlObj.getReceiver()));
    txobj.push_back(Pair("blocks", (uint64_t) tlObj.getBlockForExpiry()));
}

void populateRPCTypeContract_Instant(CMPTransaction& tlObj, UniValue& txobj)
{
    txobj.push_back(Pair("propertyId", (uint64_t) tlObj.getProperty()));
    txobj.push_back(Pair("amount", (uint64_t) tlObj.getAmountForSale()));
    txobj.push_back(Pair("block for expiry", (uint64_t) tlObj.getBlockForExpiry()));
    txobj.push_back(Pair("price", (uint64_t) tlObj.getPrice()));
    txobj.push_back(Pair("trading action", (uint64_t) tlObj.getItradingAction()));
    txobj.push_back(Pair("leverage", (uint64_t) tlObj.getIleverage()));
}

void populateRPCTypeNew_Id_Registration(CMPTransaction& tlObj, UniValue& txobj)
{
 /** Do we need this? (public information?)*/
}

void populateRPCTypeUpdate_Id_Registration(CMPTransaction& tlObj, UniValue& txobj)
{
/** Do we need this? (public information?)*/
}

void populateRPCTypeDEx_Payment(CMPTransaction& tlObj, UniValue& txobj)
{
/** If it needs more info*/
}

void populateRPCTypeLitecoinPayment(CMPTransaction& tlObj, UniValue& txobj)
{
    uint256 linked_txid = tlObj.getLinkedTXID();
    txobj.push_back(Pair("linkedtxid", linked_txid.GetHex()));

    CTransactionRef tx;
    uint256 linked_blockHash;
    int linked_blockHeight = 0;
    int linked_blockTime = 0;
    if (GetTransaction(linked_txid, tx, Params().GetConsensus(), linked_blockHash, true)) {
        if (linked_blockHash.size() != 0) {
            CBlockIndex* pBlockIndex = GetBlockIndex(linked_blockHash);
            if (NULL != pBlockIndex) {
                linked_blockHeight = pBlockIndex->nHeight;
                linked_blockTime = pBlockIndex->nTime;
            }
            CMPTransaction mp_obj;
            int parseRC = ParseTransaction(*tx, linked_blockHeight, 0, mp_obj, linked_blockTime);
            if (parseRC >= 0) {
                if (mp_obj.interpret_Transaction()) {
                    txobj.push_back(Pair("linkedtxtype", mp_obj.getTypeString()));
                    txobj.push_back(Pair("paymentrecipient", mp_obj.getSender()));
                    txobj.push_back(Pair("paymentamount", FormatDivisibleMP(GetLitecoinPaymentAmount(tlObj.getHash(), mp_obj.getSender()))));
                }
            }
        }
    }

    // TODO: what about details about what this payment did (eg crowdsale purchase, paid accept etc)?
}