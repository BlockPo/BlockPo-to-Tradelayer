/**
 * @file rpctxobject.cpp
 *
 * Handler for populating RPC transaction objects.
 */

#include "omnicore/rpctxobject.h"

#include "omnicore/errors.h"
#include "omnicore/omnicore.h"
#include "omnicore/pending.h"
#include "omnicore/rpctxobject.h"
#include "omnicore/sp.h"
#include "omnicore/tx.h"
#include "omnicore/utilsbitcoin.h"
#include "omnicore/wallettxs.h"

#include "chainparams.h"
#include "validation.h"
#include "primitives/transaction.h"
#include "sync.h"
#include "uint256.h"

#include <univalue.h>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include <stdint.h>
#include <string>
#include <vector>

// Namespaces
using namespace mastercore;

/**
 * Function to standardize RPC output for transactions into a JSON object in either basic or extended mode.
 *
 * Use basic mode for generic calls (e.g. omni_gettransaction/omni_listtransaction etc.)
 * Use extended mode for transaction specific calls (e.g. omni_getsto, omni_gettrade etc.)
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
  
  if (blockHeight == 0) {
    blockHeight = GetHeight();
  }
  
  if (!blockHash.IsNull()) {
    CBlockIndex* pBlockIndex = GetBlockIndex(blockHash);
    if (NULL != pBlockIndex) {
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
  if (confirmations > 0) {
    LOCK(cs_tally);
    valid = getValidMPTX(txid);
    positionInBlock = p_OmniTXDB->FetchTransactionPosition(txid);
  }
  PrintToLog("Checking valid : %s\n", valid ? "true" : "false");
  PrintToLog("Checking positionInBlock : %d\n", positionInBlock);
  
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
  populateRPCTypeInfo(mp_obj, txobj, mp_obj.getType(), extendedDetails, extendedDetailsFilter);
  
  // state and chain related information
  if (confirmations != 0 && !blockHash.IsNull()) {
    txobj.push_back(Pair("valid", valid));
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
void populateRPCTypeInfo(CMPTransaction& mp_obj, UniValue& txobj, uint32_t txType, bool extendedDetails, std::string extendedDetailsFilter)
{
    switch (txType) {
        case MSC_TYPE_SIMPLE_SEND:
            populateRPCTypeSimpleSend(mp_obj, txobj);
            break;
        case MSC_TYPE_SEND_ALL:
            populateRPCTypeSendAll(mp_obj, txobj);
            break;
        case MSC_TYPE_CREATE_PROPERTY_FIXED:
            populateRPCTypeCreatePropertyFixed(mp_obj, txobj);
            break;
        case MSC_TYPE_CREATE_PROPERTY_VARIABLE:
            populateRPCTypeCreatePropertyVariable(mp_obj, txobj);
            break;
        case MSC_TYPE_CREATE_PROPERTY_MANUAL:
            populateRPCTypeCreatePropertyManual(mp_obj, txobj);
            break;
        case MSC_TYPE_CLOSE_CROWDSALE:
            populateRPCTypeCloseCrowdsale(mp_obj, txobj);
            break;
        case MSC_TYPE_GRANT_PROPERTY_TOKENS:
            populateRPCTypeGrant(mp_obj, txobj);
            break;
        case MSC_TYPE_REVOKE_PROPERTY_TOKENS:
            populateRPCTypeRevoke(mp_obj, txobj);
            break;
        case MSC_TYPE_CHANGE_ISSUER_ADDRESS:
            populateRPCTypeChangeIssuer(mp_obj, txobj);
            break;
        case OMNICORE_MESSAGE_TYPE_ACTIVATION:
            populateRPCTypeActivation(mp_obj, txobj);
            break;
    }
}

/* Function to determine whether to display the reference address based on transaction type
 */
bool showRefForTx(uint32_t txType)
{
    switch (txType) {
        case MSC_TYPE_SIMPLE_SEND: return true;
        case MSC_TYPE_CREATE_PROPERTY_FIXED: return false;
        case MSC_TYPE_CREATE_PROPERTY_VARIABLE: return false;
        case MSC_TYPE_CREATE_PROPERTY_MANUAL: return false;
        case MSC_TYPE_CLOSE_CROWDSALE: return false;
        case MSC_TYPE_GRANT_PROPERTY_TOKENS: return true;
        case MSC_TYPE_REVOKE_PROPERTY_TOKENS: return false;
        case MSC_TYPE_CHANGE_ISSUER_ADDRESS: return true;
        case MSC_TYPE_SEND_ALL: return true;
        case OMNICORE_MESSAGE_TYPE_ACTIVATION: return false;
    }
    return true; // default to true, shouldn't be needed but just in case
}

void populateRPCTypeSimpleSend(CMPTransaction& omniObj, UniValue& txobj)
{
    uint32_t propertyId = omniObj.getProperty();
    int64_t crowdPropertyId = 0, crowdTokens = 0, issuerTokens = 0;
    LOCK(cs_tally);
    bool crowdPurchase = isCrowdsalePurchase(omniObj.getHash(), omniObj.getReceiver(), &crowdPropertyId, &crowdTokens, &issuerTokens);
    if (crowdPurchase) {
        CMPSPInfo::Entry sp;
        if (false == _my_sps->getSP(crowdPropertyId, sp)) {
            PrintToLog("SP Error: Crowdsale purchase for non-existent property %d in transaction %s", crowdPropertyId, omniObj.getHash().GetHex());
            return;
        }
        txobj.push_back(Pair("type", "Crowdsale Purchase"));
        txobj.push_back(Pair("propertyid", (uint64_t)propertyId));
        txobj.push_back(Pair("divisible", isPropertyDivisible(propertyId)));
        txobj.push_back(Pair("amount", FormatMP(propertyId, omniObj.getAmount())));
        txobj.push_back(Pair("purchasedpropertyid", crowdPropertyId));
        txobj.push_back(Pair("purchasedpropertyname", sp.name));
        txobj.push_back(Pair("purchasedpropertydivisible", sp.isDivisible()));
        txobj.push_back(Pair("purchasedtokens", FormatMP(crowdPropertyId, crowdTokens)));
        txobj.push_back(Pair("issuertokens", FormatMP(crowdPropertyId, issuerTokens)));
    } else {
        txobj.push_back(Pair("type", "Simple Send"));
        txobj.push_back(Pair("propertyid", (uint64_t)propertyId));
        txobj.push_back(Pair("divisible", isPropertyDivisible(propertyId)));
        txobj.push_back(Pair("amount", FormatMP(propertyId, omniObj.getAmount())));
    }
}

void populateRPCTypeSendAll(CMPTransaction& omniObj, UniValue& txobj)
{
    UniValue subSends(UniValue::VARR);
    if (omniObj.getEcosystem() == 1) txobj.push_back(Pair("ecosystem", "main"));
    if (omniObj.getEcosystem() == 2) txobj.push_back(Pair("ecosystem", "test"));
    if (populateRPCSendAllSubSends(omniObj.getHash(), subSends) > 0) txobj.push_back(Pair("subsends", subSends));
}

void populateRPCTypeCreatePropertyFixed(CMPTransaction& omniObj, UniValue& txobj)
{
    LOCK(cs_tally);
    uint32_t propertyId = _my_sps->findSPByTX(omniObj.getHash());
    if (propertyId > 0) txobj.push_back(Pair("propertyid", (uint64_t) propertyId));
    if (propertyId > 0) txobj.push_back(Pair("divisible", isPropertyDivisible(propertyId)));

    txobj.push_back(Pair("ecosystem", strEcosystem(omniObj.getEcosystem())));
    txobj.push_back(Pair("propertytype", strPropertyType(omniObj.getPropertyType())));
    txobj.push_back(Pair("propertyname", omniObj.getSPName()));
    txobj.push_back(Pair("data", omniObj.getSPData()));
    txobj.push_back(Pair("url", omniObj.getSPUrl()));
    std::string strAmount = FormatByType(omniObj.getAmount(), omniObj.getPropertyType());
    txobj.push_back(Pair("amount", strAmount));
}

void populateRPCTypeCreatePropertyVariable(CMPTransaction& omniObj, UniValue& txobj)
{
    LOCK(cs_tally);
    uint32_t propertyId = _my_sps->findSPByTX(omniObj.getHash());
    if (propertyId > 0) txobj.push_back(Pair("propertyid", (uint64_t) propertyId));
    if (propertyId > 0) txobj.push_back(Pair("divisible", isPropertyDivisible(propertyId)));

    txobj.push_back(Pair("ecosystem", strEcosystem(omniObj.getEcosystem())));
    txobj.push_back(Pair("propertytype", strPropertyType(omniObj.getPropertyType())));
    txobj.push_back(Pair("propertyname", omniObj.getSPName()));
    txobj.push_back(Pair("data", omniObj.getSPData()));
    txobj.push_back(Pair("url", omniObj.getSPUrl()));
    txobj.push_back(Pair("propertyiddesired", (uint64_t) omniObj.getProperty()));
    std::string strPerUnit = FormatMP(omniObj.getProperty(), omniObj.getAmount());
    txobj.push_back(Pair("tokensperunit", strPerUnit));
    txobj.push_back(Pair("deadline", omniObj.getDeadline()));
    txobj.push_back(Pair("earlybonus", omniObj.getEarlyBirdBonus()));
    txobj.push_back(Pair("percenttoissuer", omniObj.getIssuerBonus()));
    std::string strAmount = FormatByType(0, omniObj.getPropertyType());
    txobj.push_back(Pair("amount", strAmount)); // crowdsale token creations don't issue tokens with the create tx
}

void populateRPCTypeCreatePropertyManual(CMPTransaction& omniObj, UniValue& txobj)
{
    LOCK(cs_tally);
    uint32_t propertyId = _my_sps->findSPByTX(omniObj.getHash());
    if (propertyId > 0) txobj.push_back(Pair("propertyid", (uint64_t) propertyId));
    if (propertyId > 0) txobj.push_back(Pair("divisible", isPropertyDivisible(propertyId)));

    txobj.push_back(Pair("ecosystem", strEcosystem(omniObj.getEcosystem())));
    txobj.push_back(Pair("propertytype", strPropertyType(omniObj.getPropertyType())));
    txobj.push_back(Pair("propertyname", omniObj.getSPName()));
    txobj.push_back(Pair("data", omniObj.getSPData()));
    txobj.push_back(Pair("url", omniObj.getSPUrl()));
    std::string strAmount = FormatByType(0, omniObj.getPropertyType());
    txobj.push_back(Pair("amount", strAmount)); // managed token creations don't issue tokens with the create tx
}

void populateRPCTypeCloseCrowdsale(CMPTransaction& omniObj, UniValue& txobj)
{
    uint32_t propertyId = omniObj.getProperty();
    txobj.push_back(Pair("propertyid", (uint64_t)propertyId));
    txobj.push_back(Pair("divisible", isPropertyDivisible(propertyId)));
}

void populateRPCTypeGrant(CMPTransaction& omniObj, UniValue& txobj)
{
    uint32_t propertyId = omniObj.getProperty();
    txobj.push_back(Pair("propertyid", (uint64_t)propertyId));
    txobj.push_back(Pair("divisible", isPropertyDivisible(propertyId)));
    txobj.push_back(Pair("amount", FormatMP(propertyId, omniObj.getAmount())));
}

void populateRPCTypeRevoke(CMPTransaction& omniObj, UniValue& txobj)
{
    uint32_t propertyId = omniObj.getProperty();
    txobj.push_back(Pair("propertyid", (uint64_t)propertyId));
    txobj.push_back(Pair("divisible", isPropertyDivisible(propertyId)));
    txobj.push_back(Pair("amount", FormatMP(propertyId, omniObj.getAmount())));
}

void populateRPCTypeChangeIssuer(CMPTransaction& omniObj, UniValue& txobj)
{
    uint32_t propertyId = omniObj.getProperty();
    txobj.push_back(Pair("propertyid", (uint64_t)propertyId));
    txobj.push_back(Pair("divisible", isPropertyDivisible(propertyId)));
}

void populateRPCTypeActivation(CMPTransaction& omniObj, UniValue& txobj)
{
    txobj.push_back(Pair("featureid", (uint64_t) omniObj.getFeatureId()));
    txobj.push_back(Pair("activationblock", (uint64_t) omniObj.getActivationBlock()));
    txobj.push_back(Pair("minimumversion", (uint64_t) omniObj.getMinClientVersion()));
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
    for (int subSend = 1; subSend <= numberOfSubSends; subSend++) {
        UniValue subSendObj(UniValue::VOBJ);
        uint32_t propertyId;
        int64_t amount;
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
