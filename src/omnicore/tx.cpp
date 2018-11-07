// Master Protocol transaction code

#include "omnicore/tx.h"

#include "omnicore/activation.h"
#include "omnicore/convert.h"
#include "omnicore/dex.h"
#include "omnicore/log.h"
#include "omnicore/notifications.h"
#include "omnicore/omnicore.h"
#include "omnicore/rules.h"
#include "omnicore/sp.h"
#include "omnicore/varint.h"
#include "omnicore/mdex.h"
#include "omnicore/uint256_extensions.h"

#include "amount.h"
#include "validation.h"
#include "sync.h"
#include "utiltime.h"

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <utility>
#include <vector>
#include<arpa/inet.h>
#include<unistd.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<inttypes.h>
#include<math.h>
#include "tradelayer_matrices.h"

using boost::algorithm::token_compress_on;

using namespace mastercore;
typedef boost::rational<boost::multiprecision::checked_int128_t> rational_t;
extern std::map<std::string,uint32_t> peggedIssuers;
extern int64_t factorE;
extern int64_t priceIndex;
extern int64_t allPrice;
extern double denMargin;
extern uint64_t marketP[NPTYPES];
extern volatile int id_contract;

/** Returns a label for the given transaction type. */
std::string mastercore::strTransactionType(uint16_t txType)
{
    switch (txType) {
        case MSC_TYPE_SIMPLE_SEND: return "Simple Send";
        case MSC_TYPE_RESTRICTED_SEND: return "Restricted Send";
        case MSC_TYPE_SEND_ALL: return "Send All";
        case MSC_TYPE_SAVINGS_MARK: return "Savings";
        case MSC_TYPE_SAVINGS_COMPROMISED: return "Savings COMPROMISED";
        case MSC_TYPE_CREATE_PROPERTY_FIXED: return "Create Property - Fixed";
        case MSC_TYPE_CREATE_PROPERTY_VARIABLE: return "Create Property - Variable";
        case MSC_TYPE_CLOSE_CROWDSALE: return "Close Crowdsale";
        case MSC_TYPE_CREATE_PROPERTY_MANUAL: return "Create Property - Manual";
        case MSC_TYPE_GRANT_PROPERTY_TOKENS: return "Grant Property Tokens";
        case MSC_TYPE_REVOKE_PROPERTY_TOKENS: return "Revoke Property Tokens";
        case MSC_TYPE_CHANGE_ISSUER_ADDRESS: return "Change Issuer Address";
        case OMNICORE_MESSAGE_TYPE_ALERT: return "ALERT";
        case OMNICORE_MESSAGE_TYPE_DEACTIVATION: return "Feature Deactivation";
        case OMNICORE_MESSAGE_TYPE_ACTIVATION: return "Feature Activation";
        case MSC_TYPE_METADEX_TRADE: return "Metadex Order";

        /** New things for Contract */
        case MSC_TYPE_CONTRACTDEX_TRADE: return "Future Contract";
        // case MSC_TYPE_CONTRACTDEX_CANCEL_PRICE: return "ContractDex cancel-price";
        case MSC_TYPE_CONTRACTDEX_CANCEL_ECOSYSTEM: return "ContractDex cancel-ecosystem";
        case MSC_TYPE_CREATE_CONTRACT: return "Create Contract";
        case MSC_TYPE_PEGGED_CURRENCY: return "Pegged Currency";
        case MSC_TYPE_REDEMPTION_PEGGED: return "Redemption Pegged Currency";
        case MSC_TYPE_SEND_PEGGED_CURRENCY: return "Send Pegged Currency";
        case MSC_TYPE_CONTRACTDEX_CLOSE_POSITION: return "Close Position";
        case MSC_TYPE_CONTRACTDEX_CANCEL_ORDERS_BY_BLOCK: return "Cancel Orders by Block";
        case MSC_TYPE_TRADE_OFFER: return "DEx Sell Offer";
        case MSC_TYPE_DEX_BUY_OFFER: return "DEx Buy Offer";
        ////////////////////////////////////////////////////////////////////////
        default: return "* unknown type *";
    }
}

/** Helper to convert class number to string. */
static std::string intToClass(int encodingClass)
{
    switch (encodingClass) {
        case OMNI_CLASS_D:
            return "D";
    }

    return "-";
}

/** Obtains the next varint from a payload. */
std::vector<uint8_t> CMPTransaction::GetNextVarIntBytes(int &i) {
    std::vector<uint8_t> vecBytes;

    do {
        vecBytes.push_back(pkt[i]);
        if (!IsMSBSet(&pkt[i])) break;
        i++;
    } while (i < pkt_size);

    i++;

    return vecBytes;
}


/** Checks whether a pointer to the payload is past it's last position. */
bool CMPTransaction::isOverrun(const char* p)
{
    ptrdiff_t pos = (char*) p - (char*) &pkt;
    return (pos > pkt_size);
}

// -------------------- PACKET PARSING -----------------------

/** Parses the packet or payload. */
bool CMPTransaction::interpret_Transaction()
{
    if (!interpret_TransactionType()) {
        PrintToLog("Failed to interpret type and version\n");
        return false;
    }

    switch (type) {
        case MSC_TYPE_SIMPLE_SEND:
            return interpret_SimpleSend();

        case MSC_TYPE_SEND_ALL:
            return interpret_SendAll();

        case MSC_TYPE_CREATE_PROPERTY_FIXED:
            return interpret_CreatePropertyFixed();

        case MSC_TYPE_CREATE_PROPERTY_VARIABLE:
            return interpret_CreatePropertyVariable();

        case MSC_TYPE_CLOSE_CROWDSALE:
            return interpret_CloseCrowdsale();

        case MSC_TYPE_CREATE_PROPERTY_MANUAL:
            return interpret_CreatePropertyManaged();

        case MSC_TYPE_GRANT_PROPERTY_TOKENS:
            return interpret_GrantTokens();

        case MSC_TYPE_REVOKE_PROPERTY_TOKENS:
            return interpret_RevokeTokens();

        case MSC_TYPE_CHANGE_ISSUER_ADDRESS:
            return interpret_ChangeIssuer();

        case OMNICORE_MESSAGE_TYPE_DEACTIVATION:
            return interpret_Deactivation();

        case OMNICORE_MESSAGE_TYPE_ACTIVATION:
            return interpret_Activation();

        case OMNICORE_MESSAGE_TYPE_ALERT:
            return interpret_Alert();

        case MSC_TYPE_METADEX_TRADE:
            return interpret_MetaDExTrade();

        case MSC_TYPE_CREATE_CONTRACT:
            return interpret_CreateContractDex();

        case MSC_TYPE_CONTRACTDEX_TRADE:
            return interpret_ContractDexTrade();

        case MSC_TYPE_CONTRACTDEX_CANCEL_ECOSYSTEM:
            return interpret_ContractDexCancelEcosystem();

        case MSC_TYPE_PEGGED_CURRENCY:
            return interpret_CreatePeggedCurrency();

        case MSC_TYPE_SEND_PEGGED_CURRENCY:
            return interpret_SendPeggedCurrency();

        case MSC_TYPE_REDEMPTION_PEGGED:
            return interpret_RedemptionPegged();

        case MSC_TYPE_CONTRACTDEX_CLOSE_POSITION:
            return interpret_ContractDexClosePosition();

        case MSC_TYPE_CONTRACTDEX_CANCEL_ORDERS_BY_BLOCK:
            return interpret_ContractDex_Cancel_Orders_By_Block();

        case MSC_TYPE_TRADE_OFFER:
            return interpret_TradeOffer();

        case MSC_TYPE_DEX_BUY_OFFER:
            return interpret_DExBuy();

        case MSC_TYPE_ACCEPT_OFFER_BTC:
            return interpret_AcceptOfferBTC();

    }

    return false;
}

/** Version and type */
bool CMPTransaction::interpret_TransactionType()
{
    int i = 0;

    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);

    if (!vecVersionBytes.empty()) {
        version = DecompressInteger(vecVersionBytes);
    } else return false;

    if (!vecTypeBytes.empty()) {
        type = DecompressInteger(vecTypeBytes);
    } else return false;

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t------------------------------\n");
        PrintToLog("\t         version: %d, class %s\n", version, intToClass(encodingClass));
        PrintToLog("\t            type: %d (%s)\n", type, strTransactionType(type));

    }

    return true;
}

/** Tx 1 */
bool CMPTransaction::interpret_SimpleSend()
{
    int i = 0;

    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecPropIdBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecAmountBytes = GetNextVarIntBytes(i);

    if (!vecPropIdBytes.empty()) {
        property = DecompressInteger(vecPropIdBytes);
    } else return false;

    if (!vecAmountBytes.empty()) {
        nValue = DecompressInteger(vecAmountBytes);
        nNewValue = nValue;
    } else return false;

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t        property: %d (%s)\n", property, strMPProperty(property));
        PrintToLog("\t           value: %s\n", FormatMP(property, nValue));
    }

    return true;
}

/** Tx 4 */
bool CMPTransaction::interpret_SendAll()
{
    int i = 0;

    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
    memcpy(&ecosystem, &pkt[i], 1);

    property = ecosystem; // provide a hint for the UI, TODO: better handling!

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t       ecosystem: %d\n", (int)ecosystem);
    }

    return true;
}

/** Tx 50 */
bool CMPTransaction::interpret_CreatePropertyFixed()
{
    int i = 0;

    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);

    memcpy(&ecosystem, &pkt[i], 1);
    i++;

    std::vector<uint8_t> vecPropTypeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecPrevPropIdBytes = GetNextVarIntBytes(i);

    const char* p = i + (char*) &pkt;
    std::vector<std::string> spstr;
    for (int j = 0; j < 3; j++) {
        spstr.push_back(std::string(p));
        p += spstr.back().size() + 1;
    }

    if (isOverrun(p)) {
        PrintToLog("%s(): rejected: malformed string value(s)\n", __func__);
        return false;
    }

    int j = 0;
    memcpy(name, spstr[j].c_str(), std::min(spstr[j].length(), sizeof(name)-1)); j++;
    memcpy(url, spstr[j].c_str(), std::min(spstr[j].length(), sizeof(url)-1)); j++;
    memcpy(data, spstr[j].c_str(), std::min(spstr[j].length(), sizeof(data)-1)); j++;
    i = i + strlen(name) + strlen(url) + strlen(data) + 3; // data sizes + 3 null terminators

    std::vector<uint8_t> vecAmountBytes = GetNextVarIntBytes(i);

    if (!vecPropTypeBytes.empty()) {
        prop_type = DecompressInteger(vecPropTypeBytes);
    } else return false;

    if (!vecPrevPropIdBytes.empty()) {
        prev_prop_id = DecompressInteger(vecPrevPropIdBytes);
    } else return false;

    if (!vecAmountBytes.empty()) {
        nValue = DecompressInteger(vecAmountBytes);
        nNewValue = nValue;
    } else return false;

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t       ecosystem: %d\n", ecosystem);
        PrintToLog("\t   property type: %d (%s)\n", prop_type, strPropertyType(prop_type));
        PrintToLog("\tprev property id: %d\n", prev_prop_id);
        PrintToLog("\t            name: %s\n", name);
        PrintToLog("\t             url: %s\n", url);
        PrintToLog("\t            data: %s\n", data);
        PrintToLog("\t           value: %s\n", FormatByType(nValue, prop_type));
    }

    return true;
}

/** Tx 51 */
bool CMPTransaction::interpret_CreatePropertyVariable()
{
    int i = 0;

    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);

    memcpy(&ecosystem, &pkt[i], 1);
    i++;

    std::vector<uint8_t> vecPropTypeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecPrevPropIdBytes = GetNextVarIntBytes(i);

    const char* p = i + (char*) &pkt;
    std::vector<std::string> spstr;
    for (int j = 0; j < 3; j++) {
        spstr.push_back(std::string(p));
        p += spstr.back().size() + 1;
    }

    if (isOverrun(p)) {
        PrintToLog("%s(): rejected: malformed string value(s)\n", __func__);
        return false;
    }

    int j = 0;
    memcpy(name, spstr[j].c_str(), std::min(spstr[j].length(), sizeof(name)-1)); j++;
    memcpy(url, spstr[j].c_str(), std::min(spstr[j].length(), sizeof(url)-1)); j++;
    memcpy(data, spstr[j].c_str(), std::min(spstr[j].length(), sizeof(data)-1)); j++;
    i = i + strlen(name) + strlen(url) + strlen(data) + 3; // data sizes + 3 null terminators

    std::vector<uint8_t> vecPropertyIdDesiredBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecAmountPerUnitBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecDeadlineBytes = GetNextVarIntBytes(i);
    memcpy(&early_bird, &pkt[i], 1);
    i++;
    memcpy(&percentage, &pkt[i], 1);
    i++;

    if (!vecPropTypeBytes.empty()) {
        prop_type = DecompressInteger(vecPropTypeBytes);
    } else return false;

    if (!vecPrevPropIdBytes.empty()) {
        prev_prop_id = DecompressInteger(vecPrevPropIdBytes);
    } else return false;

    if (!vecPropertyIdDesiredBytes.empty()) {
        property = DecompressInteger(vecPropertyIdDesiredBytes);
    } else return false;

    if (!vecAmountPerUnitBytes.empty()) {
        nValue = DecompressInteger(vecAmountPerUnitBytes);
        nNewValue = nValue;
    } else return false;

    if (!vecDeadlineBytes.empty()) {
        deadline = DecompressInteger(vecDeadlineBytes);
    } else return false;

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t       ecosystem: %d\n", ecosystem);
        PrintToLog("\t   property type: %d (%s)\n", prop_type, strPropertyType(prop_type));
        PrintToLog("\tprev property id: %d\n", prev_prop_id);
        PrintToLog("\t            name: %s\n", name);
        PrintToLog("\t             url: %s\n", url);
        PrintToLog("\t            data: %s\n", data);
        PrintToLog("\tproperty desired: %d (%s)\n", property, strMPProperty(property));
        PrintToLog("\t tokens per unit: %s\n", FormatByType(nValue, prop_type));
        PrintToLog("\t        deadline: %s (%x)\n", DateTimeStrFormat("%Y-%m-%d %H:%M:%S", deadline), deadline);
        PrintToLog("\tearly bird bonus: %d\n", early_bird);
        PrintToLog("\t    issuer bonus: %d\n", percentage);
    }

    return true;
}

/** Tx 53 */
bool CMPTransaction::interpret_CloseCrowdsale()
{
    int i = 0;

    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecPropIdBytes = GetNextVarIntBytes(i);

    if (!vecPropIdBytes.empty()) {
        property = DecompressInteger(vecPropIdBytes);
    } else return false;

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t        property: %d (%s)\n", property, strMPProperty(property));
    }

    return true;
}

/** Tx 54 */
bool CMPTransaction::interpret_CreatePropertyManaged()
{
    int i = 0;

    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);

    memcpy(&ecosystem, &pkt[i], 1);
    i++;

    std::vector<uint8_t> vecPropTypeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecPrevPropIdBytes = GetNextVarIntBytes(i);

    const char* p = i + (char*) &pkt;
    std::vector<std::string> spstr;
    for (int j = 0; j < 3; j++) {
        spstr.push_back(std::string(p));
        p += spstr.back().size() + 1;
    }

    if (isOverrun(p)) {
        PrintToLog("%s(): rejected: malformed string value(s)\n", __func__);
        return false;
    }

    int j = 0;
    memcpy(name, spstr[j].c_str(), std::min(spstr[j].length(), sizeof(name)-1)); j++;
    memcpy(url, spstr[j].c_str(), std::min(spstr[j].length(), sizeof(url)-1)); j++;
    memcpy(data, spstr[j].c_str(), std::min(spstr[j].length(), sizeof(data)-1)); j++;
    i = i + strlen(name) + strlen(url) + strlen(data) + 3; // data sizes + 3 null terminators

    if (!vecPropTypeBytes.empty()) {
        prop_type = DecompressInteger(vecPropTypeBytes);
    } else return false;

    if (!vecPrevPropIdBytes.empty()) {
        prev_prop_id = DecompressInteger(vecPrevPropIdBytes);
    } else return false;

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t       ecosystem: %d\n", ecosystem);
        PrintToLog("\t   property type: %d (%s)\n", prop_type, strPropertyType(prop_type));
        PrintToLog("\tprev property id: %d\n", prev_prop_id);
        PrintToLog("\t            name: %s\n", name);
        PrintToLog("\t             url: %s\n", url);
        PrintToLog("\t            data: %s\n", data);
    }

    return true;
}

/** Tx 55 */
bool CMPTransaction::interpret_GrantTokens()
{
    int i = 0;

    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecPropIdBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecAmountBytes = GetNextVarIntBytes(i);

    if (!vecPropIdBytes.empty()) {
      property = DecompressInteger(vecPropIdBytes);
    } else return false;

    if (!vecAmountBytes.empty()) {
      nValue = DecompressInteger(vecAmountBytes);
      nNewValue = nValue;
    } else return false;

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
      PrintToLog("\t        property: %d (%s)\n", property, strMPProperty(property));
      PrintToLog("\t           value: %s\n", FormatMP(property, nValue));
    }

    return true;
}

/** Tx 56 */
bool CMPTransaction::interpret_RevokeTokens()
{
    int i = 0;

    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecPropIdBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecAmountBytes = GetNextVarIntBytes(i);

    if (!vecPropIdBytes.empty()) {
        property = DecompressInteger(vecPropIdBytes);
    } else return false;

    if (!vecAmountBytes.empty()) {
        nValue = DecompressInteger(vecAmountBytes);
        nNewValue = nValue;
    } else return false;

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t        property: %d (%s)\n", property, strMPProperty(property));
        PrintToLog("\t           value: %s\n", FormatMP(property, nValue));
    }

    return true;
}

/** Tx 70 */
bool CMPTransaction::interpret_ChangeIssuer()
{
    int i = 0;

    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecPropIdBytes = GetNextVarIntBytes(i);

    if (!vecPropIdBytes.empty()) {
        property = DecompressInteger(vecPropIdBytes);
    } else return false;

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t        property: %d (%s)\n", property, strMPProperty(property));
    }

    return true;
}

/** Tx 65533 */
bool CMPTransaction::interpret_Deactivation()
{
    int i = 0;

    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecFeatureIdBytes = GetNextVarIntBytes(i);

    if (!vecFeatureIdBytes.empty()) {
        feature_id = DecompressInteger(vecFeatureIdBytes);
    } else return false;

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t      feature id: %d\n", feature_id);
    }

    return true;
}

/** Tx 65534 */
bool CMPTransaction::interpret_Activation()
{
    int i = 0;

    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecFeatureIdBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecActivationBlockBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecMinClientBytes = GetNextVarIntBytes(i);

    if (!vecFeatureIdBytes.empty()) {
        feature_id = DecompressInteger(vecFeatureIdBytes);
    } else return false;

    if (!vecActivationBlockBytes.empty()) {
        activation_block = DecompressInteger(vecActivationBlockBytes);
    } else return false;

    if (!vecMinClientBytes.empty()) {
        min_client_version = DecompressInteger(vecMinClientBytes);
    } else return false;

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t      feature id: %d\n", feature_id);
        PrintToLog("\tactivation block: %d\n", activation_block);
        PrintToLog("\t minimum version: %d\n", min_client_version);
    }

    return true;
}

/** Tx 65535 */
bool CMPTransaction::interpret_Alert()
{
    int i = 0;

    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecAlertTypeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecAlertExpiryBytes = GetNextVarIntBytes(i);

    const char* p = i + (char*) &pkt;
    std::string spstr(p);
    memcpy(alert_text, spstr.c_str(), std::min(spstr.length(), sizeof(alert_text)-1));

    if (isOverrun(p)) {
        PrintToLog("%s(): rejected: malformed string value(s)\n", __func__);
        return false;
    }

    if (!vecAlertTypeBytes.empty()) {
        alert_type = DecompressInteger(vecAlertTypeBytes);
    } else return false;

    if (!vecAlertExpiryBytes.empty()) {
        alert_expiry = DecompressInteger(vecAlertExpiryBytes);
    } else return false;

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t      alert type: %d\n", alert_type);
        PrintToLog("\t    expiry value: %d\n", alert_expiry);
        PrintToLog("\t   alert message: %s\n", alert_text);
    }

    return true;
}

/*Tx 20*/
bool CMPTransaction::interpret_TradeOffer()
{
    int i = 0;

    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecPropertyIdBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecAmountForSaleBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecAmountDesiredBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTimeLimitBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecMinFeeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecSubActionBytes = GetNextVarIntBytes(i);

    if (!vecTypeBytes.empty()) {
        type = DecompressInteger(vecTypeBytes);
    } else return false;

    if (!vecVersionBytes.empty()) {
        version = DecompressInteger(vecVersionBytes);
    } else return false;

    if (!vecPropertyIdBytes.empty()) {
        propertyId = DecompressInteger(vecPropertyIdBytes);
    } else return false;

    if (!vecAmountForSaleBytes.empty()) {
        nValue = DecompressInteger(vecAmountForSaleBytes);
        nNewValue = nValue;
    } else return false;

    if (!vecAmountDesiredBytes.empty()) {
        amountDesired = DecompressInteger(vecAmountDesiredBytes);
    } else return false;

    if (!vecTimeLimitBytes.empty()) {
        timeLimit = DecompressInteger(vecTimeLimitBytes);
    } else return false;

    if (!vecMinFeeBytes.empty()) {
        minFee = DecompressInteger(vecMinFeeBytes);
    } else return false;

    if (!vecSubActionBytes.empty()) {
        subAction = DecompressInteger(vecSubActionBytes);
    } else return false;

    PrintToLog("version: %d\n", version);
    PrintToLog("messageType: %d\n",type);
    PrintToLog("property: %d\n", propertyId);
    PrintToLog("amount : %d\n", nValue);
    PrintToLog("amount desired : %d\n", amountDesired);
    PrintToLog("block limit : %d\n", timeLimit);
    PrintToLog("min fees : %d\n", minFee);
    PrintToLog("subaction : %d\n", subAction);

    return true;
}

bool CMPTransaction::interpret_DExBuy()
{
    int i = 0;

    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecPropertyIdBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecAmountForSaleBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecPriceBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTimeLimitBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecMinFeeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecSubActionBytes = GetNextVarIntBytes(i);

    if (!vecTypeBytes.empty()) {
        type = DecompressInteger(vecTypeBytes);
    } else return false;

    if (!vecVersionBytes.empty()) {
        version = DecompressInteger(vecVersionBytes);
    } else return false;

    if (!vecPropertyIdBytes.empty()) {
        propertyId = DecompressInteger(vecPropertyIdBytes);
    } else return false;

    if (!vecAmountForSaleBytes.empty()) {
        nValue = DecompressInteger(vecAmountForSaleBytes);
        nNewValue = nValue;
    } else return false;

    if (!vecPriceBytes.empty()) {
        effective_price = DecompressInteger(vecPriceBytes);
    } else return false;

    if (!vecTimeLimitBytes.empty()) {
        timeLimit = DecompressInteger(vecTimeLimitBytes);
    } else return false;

    if (!vecMinFeeBytes.empty()) {
        minFee = DecompressInteger(vecMinFeeBytes);
    } else return false;

    if (!vecSubActionBytes.empty()) {
        subAction = DecompressInteger(vecSubActionBytes);
    } else return false;

    PrintToLog("version: %d\n", version);
    PrintToLog("messageType: %d\n",type);
    PrintToLog("property: %d\n", propertyId);
    PrintToLog("amount : %d\n", nValue);
    PrintToLog("price : %d\n", effective_price);
    PrintToLog("block limit : %d\n", timeLimit);
    PrintToLog("min fees : %d\n", minFee);
    PrintToLog("subaction : %d\n", subAction);

  return true;
}

bool CMPTransaction::interpret_AcceptOfferBTC()
{
    PrintToLog("Inside of interpret_AcceptOfferBTC function!!!!!!!!!\n");
    int i = 0;
    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecPropertyIdForSaleBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecAmountForSaleBytes = GetNextVarIntBytes(i);

    if (!vecTypeBytes.empty()) {
        type = DecompressInteger(vecTypeBytes);
    } else return false;

    if (!vecVersionBytes.empty()) {
        version = DecompressInteger(vecVersionBytes);
    } else return false;

    if (!vecPropertyIdForSaleBytes.empty()) {
        propertyId = DecompressInteger(vecPropertyIdForSaleBytes);
    } else return false;

    if (!vecAmountForSaleBytes.empty()) {
        nValue = DecompressInteger(vecAmountForSaleBytes);
        nNewValue = nValue;
    } else return false;

    PrintToLog("version: %d\n", version);
    PrintToLog("messageType: %d\n",type);
    PrintToLog("property: %d\n", propertyId);
    PrintToLog("amount : %d\n", nValue);

    return true;
}

/** Tx  25*/
bool CMPTransaction::interpret_MetaDExTrade()
{

    int i = 0;

    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecPropertyIdForSaleBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecAmountForSaleBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecPropertyIdDesiredBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecAmountDesiredBytes = GetNextVarIntBytes(i);

    if (!vecTypeBytes.empty()) {
        type = DecompressInteger(vecTypeBytes);
    } else return false;

    if (!vecVersionBytes.empty()) {
        version = DecompressInteger(vecVersionBytes);
    } else return false;

    if (!vecPropertyIdForSaleBytes.empty()) {
        property = DecompressInteger(vecPropertyIdForSaleBytes);
    } else return false;

    if (!vecAmountForSaleBytes.empty()) {
        amount_forsale = DecompressInteger(vecAmountForSaleBytes);
        nNewValue = amount_forsale;
    } else return false;

    if (!vecPropertyIdDesiredBytes.empty()) {
        desired_property = DecompressInteger(vecPropertyIdDesiredBytes);
    } else return false;

    if (!vecAmountDesiredBytes.empty()) {
        desired_value = DecompressInteger(vecAmountDesiredBytes);
    } else return false;

    PrintToLog("version: %d\n", version);
    PrintToLog("messageType: %d\n",type);
    PrintToLog("property: %d\n", property);
    PrintToLog("amount : %d\n", amount_forsale);
    PrintToLog("property desired : %d\n", desired_property);
    PrintToLog("amount desired : %d\n", desired_value);

    return true;
}

/** Tx  40*/
bool CMPTransaction::interpret_CreateContractDex()
{
  int i = 0;

  std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);

  memcpy(&ecosystem, &pkt[i], 1);
  i++;
  std::vector<uint8_t> vecDenomTypeBytes = GetNextVarIntBytes(i);

  const char* p = i + (char*) &pkt;
  std::vector<std::string> spstr;
  for (int j = 0; j < 1; j++) {
    spstr.push_back(std::string(p));
    p += spstr.back().size() + 1;
  }

  if (isOverrun(p)) {
    PrintToLog("%s(): rejected: malformed string value(s)\n", __func__);
    return false;
  }

  int j = 0;
  memcpy(name, spstr[j].c_str(), std::min(spstr[j].length(), sizeof(name)-1)); j++;
  i = i + strlen(name) + 1; // data sizes + 1 null terminators
  std::vector<uint8_t> vecBlocksUntilExpiration = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecNotionalSize = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecCollateralCurrency = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecMarginRequirement = GetNextVarIntBytes(i);

  if (!vecVersionBytes.empty()) {
    version = DecompressInteger(vecVersionBytes);
  } else return false;

  if (!vecTypeBytes.empty()) {
    type = DecompressInteger(vecTypeBytes);
  } else return false;

  if (!vecDenomTypeBytes.empty()) {
    denomination = DecompressInteger(vecDenomTypeBytes);
  } else return false;

  if (!vecBlocksUntilExpiration.empty()) {
    blocks_until_expiration = DecompressInteger(vecBlocksUntilExpiration);
  } else return false;

  if (!vecNotionalSize.empty()) {
    notional_size = DecompressInteger(vecNotionalSize);
  } else return false;

  if (!vecCollateralCurrency.empty()) {
    collateral_currency = DecompressInteger(vecCollateralCurrency);
  } else return false;

  if (!vecMarginRequirement.empty()) {
    margin_requirement = DecompressInteger(vecMarginRequirement);
  } else return false;

  std::string sub = "Futures Contracts";
  strcpy(subcategory, sub.c_str());

  PrintToLog("------------------------------------------------------------\n");
  PrintToLog("Inside interpret_CreateContractDex function\n");
  PrintToLog("version: %d\n", version);
  PrintToLog("messageType: %d\n",type);
  PrintToLog("denomination: %d\n", denomination);
  PrintToLog("blocks until expiration : %d\n", blocks_until_expiration);
  PrintToLog("notional size : %d\n", notional_size);
  PrintToLog("collateral currency: %d\n", collateral_currency);
  PrintToLog("margin requirement: %d\n", margin_requirement);
  PrintToLog("ecosystem: %d\n", ecosystem);
  PrintToLog("name: %s\n", name);
  PrintToLog("------------------------------------------------------------\n");

  return true;
}

/**Tx 29 */
bool CMPTransaction::interpret_ContractDexTrade()
{
  PrintToLog("Inside of trade contractdexTrade function\n");
  int i = 0;

  std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecPropertyIdBytes = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecAmountForSaleBytes = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecEffectivePriceBytes = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecTradingActionBytes = GetNextVarIntBytes(i);

  if (!vecTypeBytes.empty()) {
    type = DecompressInteger(vecTypeBytes);
  } else return false;

  if (!vecVersionBytes.empty()) {
      version = DecompressInteger(vecVersionBytes);
  } else return false;

  if (!vecPropertyIdBytes.empty()) {
      contractId = DecompressInteger(vecPropertyIdBytes);
  } else return false;

  if (!vecAmountForSaleBytes.empty()) {
      amount = DecompressInteger(vecAmountForSaleBytes);
  } else return false;

  if (!vecEffectivePriceBytes.empty()) {
      effective_price = DecompressInteger(vecEffectivePriceBytes);
  } else return false;

  if (!vecTradingActionBytes.empty()) {
      trading_action = DecompressInteger(vecTradingActionBytes);
  } else return false;

  PrintToLog("version: %d\n", version);
  PrintToLog("messageType: %d\n",type);
  PrintToLog("contractId: %d\n", contractId);
  PrintToLog("amount of contracts : %d\n", amount);
  PrintToLog("effective price : %d\n", effective_price);
  PrintToLog("trading action : %d\n", trading_action);

  return true;
}

/** Tx 32 */
bool CMPTransaction::interpret_ContractDexCancelEcosystem()
{
    int i = 0;

    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecEcosystemBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecContractIdBytes = GetNextVarIntBytes(i);

    if (!vecTypeBytes.empty()) {
        type = DecompressInteger(vecTypeBytes);
    } else return false;

    if (!vecVersionBytes.empty()) {
        version = DecompressInteger(vecVersionBytes);
    } else return false;

    if (!vecEcosystemBytes.empty()) {
        ecosystem = DecompressInteger(vecEcosystemBytes);
    } else return false;
    if (!vecContractIdBytes.empty()) {
        contractId = DecompressInteger(vecContractIdBytes);
    } else return false;

    PrintToLog("version: %d\n", version);
    PrintToLog("messageType: %d\n",type);
    PrintToLog("ecosystem: %d\n", ecosystem);
    return true;
  }

  /** Tx 33 */
bool CMPTransaction::interpret_ContractDexClosePosition()
{
    int i = 0;

    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecEcosystemBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecContractIdBytes = GetNextVarIntBytes(i);

    if (!vecTypeBytes.empty()) {
        type = DecompressInteger(vecTypeBytes);
    } else return false;

    if (!vecVersionBytes.empty()) {
        version = DecompressInteger(vecVersionBytes);
    } else return false;

    if (!vecEcosystemBytes.empty()) {
        ecosystem = DecompressInteger(vecEcosystemBytes);
    } else return false;

    if (!vecContractIdBytes.empty()) {
        contractId = DecompressInteger(vecContractIdBytes);
    } else return false;

    PrintToLog("version: %d\n", version);
    PrintToLog("messageType: %d\n",type);
    PrintToLog("ecosystem: %d\n", ecosystem);
    PrintToLog("contractId: %d\n", contractId);

    return true;
}

/** Tx 34 */
bool CMPTransaction::interpret_ContractDex_Cancel_Orders_By_Block()
{
    int i = 0;

    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecBlockBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecIdxBytes = GetNextVarIntBytes(i);

    if (!vecVersionBytes.empty()) {
        version = DecompressInteger(vecVersionBytes);
    } else return false;

    if (!vecTypeBytes.empty()) {
        type = DecompressInteger(vecTypeBytes);
    } else return false;

    if (!vecBlockBytes.empty()) {
        block = DecompressInteger(vecBlockBytes);
        PrintToLog("block : %d\n",block);
    } else return false;

    if (!vecIdxBytes.empty()) {
        tx_idx = DecompressInteger(vecIdxBytes);
        PrintToLog("idx : %d\n",tx_idx);
    } else return false;

    return true;
}

  /** Tx 101 */
bool CMPTransaction::interpret_CreatePeggedCurrency()
{
    int i = 0;

    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);

    memcpy(&ecosystem, &pkt[i], 1);
    i++;
    std::vector<uint8_t> vecPropTypeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecPrevPropIdBytes = GetNextVarIntBytes(i);
    const char* p = i + (char*) &pkt;
    std::vector<std::string> spstr;
    for (int j = 0; j < 1; j++){
        spstr.push_back(std::string(p));
        p += spstr.back().size() + 1;
    }

    if (isOverrun(p)) {
        PrintToLog("%s(): rejected: malformed string value(s)\n", __func__);
        return false;
    }

    int j = 0;

    memcpy(name, spstr[j].c_str(), std::min(spstr[j].length(), sizeof(name)-1)); j++;
    i = i + strlen(name) + 1; // data sizes + 3 null terminators
    std::vector<uint8_t> vecPropertyIdBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecContractIdBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecAmountBytes = GetNextVarIntBytes(i);

    if (!vecPropTypeBytes.empty()) {
        prop_type = DecompressInteger(vecPropTypeBytes);
    } else return false;

    if (!vecPrevPropIdBytes.empty()) {
        prev_prop_id = DecompressInteger(vecPrevPropIdBytes);
    } else return false;

    if (!vecVersionBytes.empty()) {
        version = DecompressInteger(vecVersionBytes);
    } else return false;

    if (!vecContractIdBytes.empty()) {
        contractId = DecompressInteger(vecContractIdBytes);
    } else return false;

    if (!vecVersionBytes.empty()) {
        amount = DecompressInteger(vecAmountBytes);
    } else return false;

    if (!vecPropertyIdBytes.empty()) {
        propertyId = DecompressInteger(vecPropertyIdBytes);
    } else return false;

    std::string sub = "Pegged Currency";
    strcpy(subcategory, sub.c_str());

    PrintToLog("version: %d\n", version);
    PrintToLog("messageType: %d\n",type);
    PrintToLog("ecosystem: %d\n", ecosystem);
    PrintToLog("property type: %d\n",prop_type);
    PrintToLog("prev prop id: %d\n",prev_prop_id);
    PrintToLog("contractId: %d\n", contractId);
    PrintToLog("propertyId: %d\n", propertyId);
    PrintToLog("amount of pegged currency : %d\n", amount);
    PrintToLog("name : %d\n", name);
    PrintToLog("subcategory: %d\n", subcategory);

    return true;
}

bool CMPTransaction::interpret_SendPeggedCurrency()
{
    int i = 0;

    std::vector<uint8_t> vecMessageVerBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecMessageTypeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecPropertyIdBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecAmountBytes = GetNextVarIntBytes(i);

    if (!vecMessageVerBytes.empty()) {
        version = DecompressInteger(vecMessageVerBytes);
    } else return false;

    if (!vecMessageTypeBytes.empty()) {
        type = DecompressInteger(vecMessageTypeBytes);
    } else return false;

    if (!vecPropertyIdBytes.empty()) {
        propertyId = DecompressInteger(vecPropertyIdBytes);
    } else return false;

    if (!vecAmountBytes.empty()) {
        amount = DecompressInteger(vecAmountBytes);
    } else return false;

    PrintToLog("version: %d\n", version);
    PrintToLog("messageType: %d\n",type);
    PrintToLog("propertyId: %d\n", propertyId);
    PrintToLog("amount of pegged currency : %d\n", amount);

    return true;
}

bool CMPTransaction::interpret_RedemptionPegged()
{
    int i = 0;

    std::vector<uint8_t> vecMessageVerBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecMessageTypeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecPropertyIdBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecContractIdBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecAmountBytes = GetNextVarIntBytes(i);

    if (!vecMessageVerBytes.empty()) {
        version = DecompressInteger(vecMessageVerBytes);
    } else return false;

    if (!vecMessageTypeBytes.empty()) {
        type = DecompressInteger(vecMessageTypeBytes);
    } else return false;

    if (!vecPropertyIdBytes.empty()) {
        propertyId = DecompressInteger(vecPropertyIdBytes);
    } else return false;

    if (!vecContractIdBytes.empty()) {
        contractId = DecompressInteger(vecContractIdBytes);
    } else return false;

    if (!vecAmountBytes.empty()) {
        amount = DecompressInteger(vecAmountBytes);
    } else return false;

    PrintToLog("version: %d\n", version);
    PrintToLog("messageType: %d\n",type);
    PrintToLog("propertyId: %d\n", propertyId);
    PrintToLog("contractId: %d\n", contractId);
    PrintToLog("amount of pegged currency : %d\n", amount);

    return true;
 }


// ---------------------- CORE LOGIC -------------------------

/**
 * Interprets the payload and executes the logic.
 *
 * @return  0  if the transaction is fully valid
 *         <0  if the transaction is invalid
 */
int CMPTransaction::interpretPacket()
{
    if (rpcOnly) {
        PrintToLog("%s(): ERROR: attempt to execute logic in RPC mode\n", __func__);
        return (PKT_ERROR -1);
    }

    if (!interpret_Transaction()) {
        return (PKT_ERROR -2);
    }

    LOCK(cs_tally);
    switch (type) {
        case MSC_TYPE_SIMPLE_SEND:
            return logicMath_SimpleSend();

        case MSC_TYPE_SEND_ALL:
            return logicMath_SendAll();

        case MSC_TYPE_CREATE_PROPERTY_FIXED:
            return logicMath_CreatePropertyFixed();

        case MSC_TYPE_CREATE_PROPERTY_VARIABLE:
            return logicMath_CreatePropertyVariable();

        case MSC_TYPE_CLOSE_CROWDSALE:
            return logicMath_CloseCrowdsale();

        case MSC_TYPE_CREATE_PROPERTY_MANUAL:
            return logicMath_CreatePropertyManaged();

        case MSC_TYPE_GRANT_PROPERTY_TOKENS:
            return logicMath_GrantTokens();

        case MSC_TYPE_REVOKE_PROPERTY_TOKENS:
            return logicMath_RevokeTokens();

        case MSC_TYPE_CHANGE_ISSUER_ADDRESS:
            return logicMath_ChangeIssuer();

        case OMNICORE_MESSAGE_TYPE_DEACTIVATION:
            return logicMath_Deactivation();

        case OMNICORE_MESSAGE_TYPE_ACTIVATION:
            return logicMath_Activation();

        case OMNICORE_MESSAGE_TYPE_ALERT:
            return logicMath_Alert();

        case MSC_TYPE_CREATE_CONTRACT:
            return logicMath_CreateContractDex();

        case MSC_TYPE_CONTRACTDEX_TRADE:
            return logicMath_ContractDexTrade();

        case MSC_TYPE_CONTRACTDEX_CANCEL_ECOSYSTEM:
            return logicMath_ContractDexCancelEcosystem();

        case MSC_TYPE_PEGGED_CURRENCY:
            return logicMath_CreatePeggedCurrency();

        case MSC_TYPE_SEND_PEGGED_CURRENCY:
            return logicMath_SendPeggedCurrency();

        case MSC_TYPE_REDEMPTION_PEGGED:
            return logicMath_RedemptionPegged();

        case MSC_TYPE_CONTRACTDEX_CLOSE_POSITION:
            return logicMath_ContractDexClosePosition();

        case MSC_TYPE_CONTRACTDEX_CANCEL_ORDERS_BY_BLOCK:
            return logicMath_ContractDex_Cancel_Orders_By_Block();

        case MSC_TYPE_TRADE_OFFER:
            return logicMath_TradeOffer();

        case MSC_TYPE_DEX_BUY_OFFER:
            return logicMath_DExBuy();

        case MSC_TYPE_ACCEPT_OFFER_BTC:
            return logicMath_AcceptOfferBTC();

        case MSC_TYPE_METADEX_TRADE:
            return logicMath_MetaDExTrade();

    }

    return (PKT_ERROR -100);
}

/** Passive effect of crowdsale participation. */
int CMPTransaction::logicHelper_CrowdsaleParticipation()
{
    CMPCrowd* pcrowdsale = getCrowd(receiver);

    // No active crowdsale
    if (pcrowdsale == NULL) {
        return (PKT_ERROR_CROWD -1);
    }
    // Active crowdsale, but not for this property
    if (pcrowdsale->getCurrDes() != property) {
        return (PKT_ERROR_CROWD -2);
    }

    CMPSPInfo::Entry sp;
    assert(_my_sps->getSP(pcrowdsale->getPropertyId(), sp));
    PrintToLog("INVESTMENT SEND to Crowdsale Issuer: %s\n", receiver);

    // Holds the tokens to be credited to the sender and issuer
    std::pair<int64_t, int64_t> tokens;

    // Passed by reference to determine, if max_tokens has been reached
    bool close_crowdsale = false;

    // Units going into the calculateFundraiser function must match the unit of
    // the fundraiser's property_type. By default this means satoshis in and
    // satoshis out. In the condition that the fundraiser is divisible, but
    // indivisible tokens are accepted, it must account for .0 Div != 1 Indiv,
    // but actually 1.0 Div == 100000000 Indiv. The unit must be shifted or the
    // values will be incorrect, which is what is checked below.
    bool inflateAmount = isPropertyDivisible(property) ? false : true;

    // Calculate the amounts to credit for this fundraiser
    calculateFundraiser(inflateAmount, nValue, sp.early_bird, sp.deadline, blockTime,
            sp.num_tokens, sp.percentage, getTotalTokens(pcrowdsale->getPropertyId()),
            tokens, close_crowdsale);

    if (msc_debug_sp) {
        PrintToLog("%s(): granting via crowdsale to user: %s %d (%s)\n",
                __func__, FormatMP(property, tokens.first), property, strMPProperty(property));
        PrintToLog("%s(): granting via crowdsale to issuer: %s %d (%s)\n",
                __func__, FormatMP(property, tokens.second), property, strMPProperty(property));
    }

    // Update the crowdsale object
    pcrowdsale->incTokensUserCreated(tokens.first);
    pcrowdsale->incTokensIssuerCreated(tokens.second);

    // Data to pass to txFundraiserData
    int64_t txdata[] = {(int64_t) nValue, blockTime, tokens.first, tokens.second};
    std::vector<int64_t> txDataVec(txdata, txdata + sizeof(txdata) / sizeof(txdata[0]));

    // Insert data about crowdsale participation
    pcrowdsale->insertDatabase(txid, txDataVec);

    // Credit tokens for this fundraiser
    if (tokens.first > 0) {
        assert(update_tally_map(sender, pcrowdsale->getPropertyId(), tokens.first, BALANCE));
    }
    if (tokens.second > 0) {
        assert(update_tally_map(receiver, pcrowdsale->getPropertyId(), tokens.second, BALANCE));
    }

    // Number of tokens has changed, update fee distribution thresholds
    NotifyTotalTokensChanged(pcrowdsale->getPropertyId());

    // Close crowdsale, if we hit MAX_TOKENS
    if (close_crowdsale) {
        eraseMaxedCrowdsale(receiver, blockTime, block);
    }

    // Indicate, if no tokens were transferred
    if (!tokens.first && !tokens.second) {
        return (PKT_ERROR_CROWD -3);
    }

    return 0;
}

/** Tx 0 */
int CMPTransaction::logicMath_SimpleSend()
{
    if (!IsTransactionTypeAllowed(block, property, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_SEND -22);
    }

    if (nValue <= 0 || MAX_INT_8_BYTES < nValue) {
        PrintToLog("%s(): rejected: value out of range or zero: %d", __func__, nValue);
        return (PKT_ERROR_SEND -23);
    }

    if (!IsPropertyIdValid(property)) {
        PrintToLog("%s(): rejected: property %d does not exist\n", __func__, property);
        return (PKT_ERROR_SEND -24);
    }

    int64_t nBalance = getMPbalance(sender, property, BALANCE);
    if (nBalance < (int64_t) nValue) {
        PrintToLog("%s(): rejected: sender %s has insufficient balance of property %d [%s < %s]\n",
                __func__,
                sender,
                property,
                FormatMP(property, nBalance),
                FormatMP(property, nValue));
        return (PKT_ERROR_SEND -25);
    }

    // ------------------------------------------

    // Special case: if can't find the receiver -- assume send to self!
    if (receiver.empty()) {
        receiver = sender;
    }

    // Move the tokens
    assert(update_tally_map(sender, property, -nValue, BALANCE));
    assert(update_tally_map(receiver, property, nValue, BALANCE));

    // Is there an active crowdsale running from this recepient?
    logicHelper_CrowdsaleParticipation();

    return 0;
}

/** Tx 4 */
int CMPTransaction::logicMath_SendAll()
{
    if (!IsTransactionTypeAllowed(block, ecosystem, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                ecosystem,
                block);
        return (PKT_ERROR_SEND_ALL -22);
    }

    // ------------------------------------------

    // Special case: if can't find the receiver -- assume send to self!
    if (receiver.empty()) {
        receiver = sender;
    }

    CMPTally* ptally = getTally(sender);
    if (ptally == NULL) {
        PrintToLog("%s(): rejected: sender %s has no tokens to send\n", __func__, sender);
        return (PKT_ERROR_SEND_ALL -54);
    }

    uint32_t propertyId = ptally->init();
    int numberOfPropertiesSent = 0;

    while (0 != (propertyId = ptally->next())) {
        // only transfer tokens in the specified ecosystem
        if (ecosystem == OMNI_PROPERTY_MSC && isTestEcosystemProperty(propertyId)) {
            continue;
        }
        if (ecosystem == OMNI_PROPERTY_TMSC && isMainEcosystemProperty(propertyId)) {
            continue;
        }

        int64_t moneyAvailable = ptally->getMoney(propertyId, BALANCE);
        if (moneyAvailable > 0) {
            ++numberOfPropertiesSent;
            assert(update_tally_map(sender, propertyId, -moneyAvailable, BALANCE));
            assert(update_tally_map(receiver, propertyId, moneyAvailable, BALANCE));
            p_txlistdb->recordSendAllSubRecord(txid, numberOfPropertiesSent, propertyId, moneyAvailable);
        }
    }

    if (!numberOfPropertiesSent) {
        PrintToLog("%s(): rejected: sender %s has no tokens to send\n", __func__, sender);
        return (PKT_ERROR_SEND_ALL -55);
    }

    nNewValue = numberOfPropertiesSent;

    return 0;
}

/** Tx 50 */
int CMPTransaction::logicMath_CreatePropertyFixed()
{
    uint256 blockHash;
    {
        LOCK(cs_main);

        CBlockIndex* pindex = chainActive[block];
        if (pindex == NULL) {
            PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
            return (PKT_ERROR_SP -20);
        }
        blockHash = pindex->GetBlockHash();
    }

    if (OMNI_PROPERTY_MSC != ecosystem && OMNI_PROPERTY_TMSC != ecosystem) {
        PrintToLog("%s(): rejected: invalid ecosystem: %d\n", __func__, (uint32_t) ecosystem);
        return (PKT_ERROR_SP -21);
    }

    if (!IsTransactionTypeAllowed(block, ecosystem, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_SP -22);
    }

    if (nValue <= 0 || MAX_INT_8_BYTES < nValue) {
        PrintToLog("%s(): rejected: value out of range or zero: %d\n", __func__, nValue);
        return (PKT_ERROR_SP -23);
    }

    if (MSC_PROPERTY_TYPE_INDIVISIBLE != prop_type && MSC_PROPERTY_TYPE_DIVISIBLE != prop_type) {
        PrintToLog("%s(): rejected: invalid property type: %d\n", __func__, prop_type);
        return (PKT_ERROR_SP -36);
    }

    if ('\0' == name[0]) {
        PrintToLog("%s(): rejected: property name must not be empty\n", __func__);
        return (PKT_ERROR_SP -37);
    }

    // ------------------------------------------

    CMPSPInfo::Entry newSP;
    newSP.issuer = sender;
    newSP.txid = txid;
    newSP.prop_type = prop_type;
    newSP.num_tokens = nValue;
    newSP.category.assign(category);
    newSP.subcategory.assign(subcategory);
    newSP.name.assign(name);
    newSP.url.assign(url);
    newSP.data.assign(data);
    newSP.fixed = true;
    newSP.creation_block = blockHash;
    newSP.update_block = newSP.creation_block;

    const uint32_t propertyId = _my_sps->putSP(ecosystem, newSP);
    assert(propertyId > 0);
    assert(update_tally_map(sender, propertyId, nValue, BALANCE));

    NotifyTotalTokensChanged(propertyId);

    return 0;
}

/** Tx 51 */
int CMPTransaction::logicMath_CreatePropertyVariable()
{
    uint256 blockHash;
    {
        LOCK(cs_main);

        CBlockIndex* pindex = chainActive[block];
        if (pindex == NULL) {
            PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
            return (PKT_ERROR_SP -20);
        }
        blockHash = pindex->GetBlockHash();
    }

    if (OMNI_PROPERTY_MSC != ecosystem && OMNI_PROPERTY_TMSC != ecosystem) {
        PrintToLog("%s(): rejected: invalid ecosystem: %d\n", __func__, (uint32_t) ecosystem);
        return (PKT_ERROR_SP -21);
    }

    if (isTestEcosystemProperty(ecosystem) != isTestEcosystemProperty(property)) {
        PrintToLog("%s(): rejected: ecosystem %d of tokens to issue and desired property %d not in same ecosystem\n",
                __func__,
                ecosystem,
                property);
        return (PKT_ERROR_SP -50);
    }

    if (!IsTransactionTypeAllowed(block, ecosystem, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_SP -22);
    }

    if (nValue <= 0 || MAX_INT_8_BYTES < nValue) {
        PrintToLog("%s(): rejected: value out of range or zero: %d\n", __func__, nValue);
        return (PKT_ERROR_SP -23);
    }

    if (!IsPropertyIdValid(property)) {
        PrintToLog("%s(): rejected: property %d does not exist\n", __func__, property);
        return (PKT_ERROR_SP -24);
    }

    if (MSC_PROPERTY_TYPE_INDIVISIBLE != prop_type && MSC_PROPERTY_TYPE_DIVISIBLE != prop_type) {
        PrintToLog("%s(): rejected: invalid property type: %d\n", __func__, prop_type);
        return (PKT_ERROR_SP -36);
    }

    if ('\0' == name[0]) {
        PrintToLog("%s(): rejected: property name must not be empty\n", __func__);
        return (PKT_ERROR_SP -37);
    }

    if (!deadline || (int64_t) deadline < blockTime) {
        PrintToLog("%s(): rejected: deadline must not be in the past [%d < %d]\n", __func__, deadline, blockTime);
        return (PKT_ERROR_SP -38);
    }

    if (NULL != getCrowd(sender)) {
        PrintToLog("%s(): rejected: sender %s has an active crowdsale\n", __func__, sender);
        return (PKT_ERROR_SP -39);
    }

    // ------------------------------------------

    CMPSPInfo::Entry newSP;
    newSP.issuer = sender;
    newSP.txid = txid;
    newSP.prop_type = prop_type;
    newSP.num_tokens = nValue;
    newSP.category.assign(category);
    newSP.subcategory.assign(subcategory);
    newSP.name.assign(name);
    newSP.url.assign(url);
    newSP.data.assign(data);
    newSP.fixed = false;
    newSP.property_desired = property;
    newSP.deadline = deadline;
    newSP.early_bird = early_bird;
    newSP.percentage = percentage;
    newSP.creation_block = blockHash;
    newSP.update_block = newSP.creation_block;

    const uint32_t propertyId = _my_sps->putSP(ecosystem, newSP);
    assert(propertyId > 0);
    my_crowds.insert(std::make_pair(sender, CMPCrowd(propertyId, nValue, property, deadline, early_bird, percentage, 0, 0)));

    PrintToLog("CREATED CROWDSALE id: %d value: %d property: %d\n", propertyId, nValue, property);

    return 0;
}

/** Tx 53 */
int CMPTransaction::logicMath_CloseCrowdsale()
{
    uint256 blockHash;
    {
        LOCK(cs_main);

        CBlockIndex* pindex = chainActive[block];
        if (pindex == NULL) {
            PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
            return (PKT_ERROR_SP -20);
        }
        blockHash = pindex->GetBlockHash();
    }

    if (!IsTransactionTypeAllowed(block, property, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_SP -22);
    }

    if (!IsPropertyIdValid(property)) {
        PrintToLog("%s(): rejected: property %d does not exist\n", __func__, property);
        return (PKT_ERROR_SP -24);
    }

    CrowdMap::iterator it = my_crowds.find(sender);
    if (it == my_crowds.end()) {
        PrintToLog("%s(): rejected: sender %s has no active crowdsale\n", __func__, sender);
        return (PKT_ERROR_SP -40);
    }

    const CMPCrowd& crowd = it->second;
    if (property != crowd.getPropertyId()) {
        PrintToLog("%s(): rejected: property identifier mismatch [%d != %d]\n", __func__, property, crowd.getPropertyId());
        return (PKT_ERROR_SP -41);
    }

    // ------------------------------------------

    CMPSPInfo::Entry sp;
    assert(_my_sps->getSP(property, sp));

    int64_t missedTokens = GetMissedIssuerBonus(sp, crowd);

    sp.historicalData = crowd.getDatabase();
    sp.update_block = blockHash;
    sp.close_early = true;
    sp.timeclosed = blockTime;
    sp.txid_close = txid;
    sp.missedTokens = missedTokens;

    assert(_my_sps->updateSP(property, sp));
    if (missedTokens > 0) {
        assert(update_tally_map(sp.issuer, property, missedTokens, BALANCE));
    }
    my_crowds.erase(it);

    if (msc_debug_sp) PrintToLog("CLOSED CROWDSALE id: %d=%X\n", property, property);

    return 0;
}

/** Tx 54 */
int CMPTransaction::logicMath_CreatePropertyManaged()
{
    uint256 blockHash;
    {
        LOCK(cs_main);

        CBlockIndex* pindex = chainActive[block];
        if (pindex == NULL) {
            PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
            return (PKT_ERROR_SP -20);
        }
        blockHash = pindex->GetBlockHash();
    }

    if (OMNI_PROPERTY_MSC != ecosystem && OMNI_PROPERTY_TMSC != ecosystem) {
        PrintToLog("%s(): rejected: invalid ecosystem: %d\n", __func__, (uint32_t) ecosystem);
        return (PKT_ERROR_SP -21);
    }

    if (!IsTransactionTypeAllowed(block, ecosystem, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_SP -22);
    }

    if (MSC_PROPERTY_TYPE_INDIVISIBLE != prop_type && MSC_PROPERTY_TYPE_DIVISIBLE != prop_type) {
        PrintToLog("%s(): rejected: invalid property type: %d\n", __func__, prop_type);
        return (PKT_ERROR_SP -36);
    }

    if ('\0' == name[0]) {
        PrintToLog("%s(): rejected: property name must not be empty\n", __func__);
        return (PKT_ERROR_SP -37);
    }

    // ------------------------------------------

    CMPSPInfo::Entry newSP;
    newSP.issuer = sender;
    newSP.txid = txid;
    newSP.prop_type = prop_type;
    newSP.category.assign(category);
    newSP.subcategory.assign(subcategory);
    newSP.name.assign(name);
    newSP.url.assign(url);
    newSP.data.assign(data);
    newSP.fixed = false;
    newSP.manual = true;
    newSP.creation_block = blockHash;
    newSP.update_block = newSP.creation_block;

    uint32_t propertyId = _my_sps->putSP(ecosystem, newSP);
    assert(propertyId > 0);

    PrintToLog("CREATED MANUAL PROPERTY id: %d admin: %s\n", propertyId, sender);

    return 0;
}

/** Tx 55 */
int CMPTransaction::logicMath_GrantTokens()
{
  uint256 blockHash;
  {
    LOCK(cs_main);

    CBlockIndex* pindex = chainActive[block];
    if (pindex == NULL) {
      PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
      return (PKT_ERROR_SP -20);
    }
    blockHash = pindex->GetBlockHash();
  }

  if (!IsTransactionTypeAllowed(block, property, type, version)) {
    PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
  	       __func__,
  	       type,
  	       version,
  	       property,
                block);
    return (PKT_ERROR_TOKENS -22);
  }

  if (nValue <= 0 || MAX_INT_8_BYTES < nValue) {
    PrintToLog("%s(): rejected: value out of range or zero: %d\n", __func__, nValue);
    return (PKT_ERROR_TOKENS -23);
  }

  if (!IsPropertyIdValid(property)) {
    PrintToLog("%s(): rejected: property %d does not exist\n", __func__, property);
    return (PKT_ERROR_TOKENS -24);
  }

  CMPSPInfo::Entry sp;
  assert(_my_sps->getSP(property, sp));

  if (!sp.manual) {
    PrintToLog("%s(): rejected: property %d is not managed\n", __func__, property);
    return (PKT_ERROR_TOKENS -42);
  }

  if (sender != sp.issuer) {
    PrintToLog("%s(): rejected: sender %s is not issuer of property %d [issuer=%s]\n", __func__, sender, property, sp.issuer);
    return (PKT_ERROR_TOKENS -43);
  }

  int64_t nTotalTokens = getTotalTokens(property);
  if (nValue > (MAX_INT_8_BYTES - nTotalTokens)) {
    PrintToLog("%s(): rejected: no more than %s tokens can ever exist [%s + %s > %s]\n",
	       __func__,
	       FormatMP(property, MAX_INT_8_BYTES),
	       FormatMP(property, nTotalTokens),
	       FormatMP(property, nValue),
	       FormatMP(property, MAX_INT_8_BYTES));
    return (PKT_ERROR_TOKENS -44);
  }

  // ------------------------------------------

  std::vector<int64_t> dataPt;
  dataPt.push_back(nValue);
  dataPt.push_back(0);
  sp.historicalData.insert(std::make_pair(txid, dataPt));
  sp.update_block = blockHash;

  // Persist the number of granted tokens
  assert(_my_sps->updateSP(property, sp));

  // Special case: if can't find the receiver -- assume grant to self!
  if (receiver.empty()) {
    receiver = sender;
  }

  // Move the tokens
  assert(update_tally_map(receiver, property, nValue, BALANCE));

  NotifyTotalTokensChanged(property);

  return 0;
}

/** Tx 56 */
int CMPTransaction::logicMath_RevokeTokens()
{
    uint256 blockHash;
    {
        LOCK(cs_main);

        CBlockIndex* pindex = chainActive[block];
        if (pindex == NULL) {
            PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
            return (PKT_ERROR_TOKENS -20);
        }
        blockHash = pindex->GetBlockHash();
    }

    if (!IsTransactionTypeAllowed(block, property, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_TOKENS -22);
    }

    if (nValue <= 0 || MAX_INT_8_BYTES < nValue) {
        PrintToLog("%s(): rejected: value out of range or zero: %d\n", __func__, nValue);
        return (PKT_ERROR_TOKENS -23);
    }

    if (!IsPropertyIdValid(property)) {
        PrintToLog("%s(): rejected: property %d does not exist\n", __func__, property);
        return (PKT_ERROR_TOKENS -24);
    }

    CMPSPInfo::Entry sp;
    assert(_my_sps->getSP(property, sp));

    if (!sp.manual) {
        PrintToLog("%s(): rejected: property %d is not managed\n", __func__, property);
        return (PKT_ERROR_TOKENS -42);
    }

    int64_t nBalance = getMPbalance(sender, property, BALANCE);
    if (nBalance < (int64_t) nValue) {
        PrintToLog("%s(): rejected: sender %s has insufficient balance of property %d [%s < %s]\n",
                __func__,
                sender,
                property,
                FormatMP(property, nBalance),
                FormatMP(property, nValue));
        return (PKT_ERROR_TOKENS -25);
    }

    // ------------------------------------------

    std::vector<int64_t> dataPt;
    dataPt.push_back(0);
    dataPt.push_back(nValue);
    sp.historicalData.insert(std::make_pair(txid, dataPt));
    sp.update_block = blockHash;

    assert(update_tally_map(sender, property, -nValue, BALANCE));
    assert(_my_sps->updateSP(property, sp));

    NotifyTotalTokensChanged(property);

    return 0;
}

/** Tx 70 */
int CMPTransaction::logicMath_ChangeIssuer()
{
    uint256 blockHash;
    {
        LOCK(cs_main);

        CBlockIndex* pindex = chainActive[block];
        if (pindex == NULL) {
            PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
            return (PKT_ERROR_TOKENS -20);
        }
        blockHash = pindex->GetBlockHash();
    }

    if (!IsTransactionTypeAllowed(block, property, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_TOKENS -22);
    }

    if (!IsPropertyIdValid(property)) {
        PrintToLog("%s(): rejected: property %d does not exist\n", __func__, property);
        return (PKT_ERROR_TOKENS -24);
    }

    CMPSPInfo::Entry sp;
    assert(_my_sps->getSP(property, sp));

    if (sender != sp.issuer) {
        PrintToLog("%s(): rejected: sender %s is not issuer of property %d [issuer=%s]\n", __func__, sender, property, sp.issuer);
        return (PKT_ERROR_TOKENS -43);
    }

    if (NULL != getCrowd(sender)) {
        PrintToLog("%s(): rejected: sender %s has an active crowdsale\n", __func__, sender);
        return (PKT_ERROR_TOKENS -39);
    }

    if (receiver.empty()) {
        PrintToLog("%s(): rejected: receiver is empty\n", __func__);
        return (PKT_ERROR_TOKENS -45);
    }

    if (NULL != getCrowd(receiver)) {
        PrintToLog("%s(): rejected: receiver %s has an active crowdsale\n", __func__, receiver);
        return (PKT_ERROR_TOKENS -46);
    }

    // ------------------------------------------

    sp.issuer = receiver;
    sp.update_block = blockHash;

    assert(_my_sps->updateSP(property, sp));

    return 0;
}

/** Tx 65533 */
int CMPTransaction::logicMath_Deactivation()
{
    if (!IsTransactionTypeAllowed(block, property, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR -22);
    }

    // is sender authorized
    bool authorized = CheckDeactivationAuthorization(sender);

    PrintToLog("\t          sender: %s\n", sender);
    PrintToLog("\t      authorized: %s\n", authorized);

    if (!authorized) {
        PrintToLog("%s(): rejected: sender %s is not authorized to deactivate features\n", __func__, sender);
        return (PKT_ERROR -51);
    }

    // authorized, request feature deactivation
    bool DeactivationSuccess = DeactivateFeature(feature_id, block);

    if (!DeactivationSuccess) {
        PrintToLog("%s(): DeactivateFeature failed\n", __func__);
        return (PKT_ERROR -54);
    }

    return 0;
}

/** Tx 65534 */
int CMPTransaction::logicMath_Activation()
{
    if (!IsTransactionTypeAllowed(block, property, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR -22);
    }

    // is sender authorized - temporarily use alert auths but ## TO BE MOVED TO FOUNDATION P2SH KEY ##
    bool authorized = CheckActivationAuthorization(sender);

    PrintToLog("\t          sender: %s\n", sender);
    PrintToLog("\t      authorized: %s\n", authorized);

    if (!authorized) {
        PrintToLog("%s(): rejected: sender %s is not authorized for feature activations\n", __func__, sender);
        return (PKT_ERROR -51);
    }

    // authorized, request feature activation
    bool activationSuccess = ActivateFeature(feature_id, activation_block, min_client_version, block);

    if (!activationSuccess) {
        PrintToLog("%s(): ActivateFeature failed to activate this feature\n", __func__);
        return (PKT_ERROR -54);
    }

    return 0;
}

/** Tx 65535 */
int CMPTransaction::logicMath_Alert()
{
    if (!IsTransactionTypeAllowed(block, property, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR -22);
    }

    // is sender authorized?
    bool authorized = CheckAlertAuthorization(sender);

    PrintToLog("\t          sender: %s\n", sender);
    PrintToLog("\t      authorized: %s\n", authorized);

    if (!authorized) {
        PrintToLog("%s(): rejected: sender %s is not authorized for alerts\n", __func__, sender);
        return (PKT_ERROR -51);
    }

    if (alert_type == 65535) { // set alert type to FFFF to clear previously sent alerts
        DeleteAlerts(sender);
    } else {
        AddAlert(sender, alert_type, alert_expiry, alert_text);
    }

    // we have a new alert, fire a notify event if needed
    // TODO AlertNotify(alert_text);

    return 0;
}

int CMPTransaction::logicMath_MetaDExTrade()
{

  if (!IsTransactionTypeAllowed(block, property, type, version)) {
      PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
              __func__,
              type,
              version,
              property,
              block);
      return (PKT_ERROR_METADEX -22);
  }

  if (property == desired_property) {
      PrintToLog("%s(): rejected: property for sale %d and desired property %d must not be equal\n",
              __func__,
              property,
              desired_property);
      return (PKT_ERROR_METADEX -29);
  }

  if (isTestEcosystemProperty(property) != isTestEcosystemProperty(desired_property)) {
      PrintToLog("%s(): rejected: property for sale %d and desired property %d not in same ecosystem\n",
              __func__,
              property,
              desired_property);
      return (PKT_ERROR_METADEX -30);
  }

  if (!IsPropertyIdValid(property)) {
      PrintToLog("%s(): rejected: property for sale %d does not exist\n", __func__, property);
      return (PKT_ERROR_METADEX -31);
  }

  if (!IsPropertyIdValid(desired_property)) {
      PrintToLog("%s(): rejected: desired property %d does not exist\n", __func__, desired_property);
      return (PKT_ERROR_METADEX -32);
  }

  if (nNewValue <= 0 || MAX_INT_8_BYTES < nNewValue) {
      PrintToLog("%s(): rejected: amount for sale out of range or zero: %d\n", __func__, nNewValue);
      return (PKT_ERROR_METADEX -33);
  }

  if (desired_value <= 0 || MAX_INT_8_BYTES < desired_value) {
      PrintToLog("%s(): rejected: desired amount out of range or zero: %d\n", __func__, desired_value);
      return (PKT_ERROR_METADEX -34);
  }


  int64_t nBalance = getMPbalance(sender, property, BALANCE);
  if (nBalance < (int64_t) nNewValue) {
      PrintToLog("%s(): rejected: sender %s has insufficient balance of property %d [%s < %s]\n",
              __func__,
              sender,
              property,
              FormatMP(property, nBalance),
              FormatMP(property, nNewValue));
      return (PKT_ERROR_METADEX -25);
  }

  // ------------------------------------------

  t_tradelistdb->recordNewTrade(txid, sender, property, desired_property, block, tx_idx);
  int rc = MetaDEx_ADD(sender, property, nNewValue, block, desired_property, desired_value, txid, tx_idx);
  return rc;

}

/** Tx 40 */
int CMPTransaction::logicMath_CreateContractDex()
{
    uint256 blockHash;
    {
        LOCK(cs_main);

        CBlockIndex* pindex = chainActive[block];

        if (pindex == NULL) {
            PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
            return (PKT_ERROR_SP -20);
        }
            blockHash = pindex->GetBlockHash();
    }

    // if (OMNI_PROPERTY_MSC != ecosystem && OMNI_PROPERTY_TMSC != ecosystem) {
    //     PrintToLog("%s(): rejected: invalid ecosystem: %d\n", __func__, (uint32_t) ecosystem);
    //     return (PKT_ERROR_SP -21);
    // }

    // if (!IsTransactionTypeAllowed(block, ecosystem, type, version)) {
    //     PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
    //         __func__,
    //         type,
    //         version,
    //         propertyId,
    //         block);
    //     return (PKT_ERROR_SP -22);
    // }

    // if ('\0' == name[0]) {
    //    PrintToLog("%s(): rejected: property name must not be empty\n", __func__);
    //    return (PKT_ERROR_SP -37);
    // }

    // PrintToLog("type of denomination: %d\n",denomination);

    // if (denomination != TL_dUSD && denomination != TL_dEUR && denomination!= TL_dYEN && denomination != TL_ALL && denomination != TL_sLTC && denomination!= TL_LTC) {
    //   PrintToLog("rejected: denomination invalid\n");
    //   return (PKT_ERROR_SP -37);
    // }

    // if (numerator != TL_ALL && numerator != TL_sLTC && numerator != TL_LTC) {
    //   PrintToLog("rejected: denomination invalid\n");
    //   return (PKT_ERROR_SP -37);
    // }

    // -----------------------------------------------

    PrintToLog("------------------------------------------------------------\n");
    PrintToLog("Inside logicMath_CreateContractDex function\n");
    PrintToLog("version: %d\n", version);
    PrintToLog("messageType: %d\n",type);
    PrintToLog("denomination: %d\n", denomination);
    PrintToLog("blocks until expiration : %d\n", blocks_until_expiration);
    PrintToLog("notional size : %d\n", notional_size);
    PrintToLog("collateral currency: %d\n", collateral_currency);
    PrintToLog("margin requirement: %d\n", margin_requirement);
    PrintToLog("ecosystem: %d\n", ecosystem);
    PrintToLog("name: %s\n", name);
    PrintToLog("sugcategory : %s\n", subcategory);
    PrintToLog("Sender: %s\n", sender);
    PrintToLog("property type: %s\n", prop_type);
    PrintToLog("blockhash: %s\n", blockHash.ToString());
    PrintToLog("block: %d\n", block);
    PrintToLog("denomination: %d\n", denomination);
    PrintToLog("------------------------------------------------------------\n");

    CMPSPInfo::Entry newSP;
    newSP.issuer = sender;
    newSP.prop_type = prop_type;
    newSP.subcategory.assign(subcategory);
    newSP.name.assign(name);
    newSP.fixed = false;
    newSP.manual = true;
    newSP.creation_block = blockHash;
    newSP.update_block = blockHash;
    newSP.blocks_until_expiration = blocks_until_expiration;
    newSP.notional_size = notional_size;
    newSP.collateral_currency = collateral_currency;
    newSP.margin_requirement = margin_requirement;
    newSP.init_block = block;
    newSP.denomination = denomination;

    const uint32_t propertyId = _my_sps->putSP(ecosystem, newSP);
    assert(propertyId > 0);

    PrintToLog("Contract id: %d\n",propertyId);

    return 0;
}

/** Tx 29 */
int CMPTransaction::logicMath_ContractDexTrade()
{
  PrintToLog("Inside of logicMath_ContractDexTrade\n");
  uint256 blockHash;
  {
    LOCK(cs_main);

    CBlockIndex* pindex = chainActive[block];
    if (pindex == NULL)
      {
	PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
	return (PKT_ERROR_SP -20);
      }
    blockHash = pindex->GetBlockHash();
  }

  CMPSPInfo::Entry sp;
  if(!_my_sps->getSP(contractId, sp)) {
      PrintToLog("%s(): ERROR: No contractId found!\n", __func__);
      return -1;
  }

  id_contract = contractId;
  int64_t marginRe = static_cast<int64_t>(sp.margin_requirement);
  int64_t nBalance = getMPbalance(sender, sp.collateral_currency, BALANCE);
  rational_t conv = notionalChange(contractId);
  int64_t num = conv.numerator().convert_to<int64_t>();
  int64_t den = conv.denominator().convert_to<int64_t>();
  arith_uint256 amountTR = (ConvertTo256(amount) * ConvertTo256(marginRe) * ConvertTo256(num) / (ConvertTo256(den) * ConvertTo256(factorE)));
  int64_t amountToReserve = ConvertTo64(amountTR);

  PrintToLog("sp.margin_requirement: %d\n",sp.margin_requirement);
  PrintToLog("collateral currency id of contract : %d\n",sp.collateral_currency);
  PrintToLog("margin Requirement: %d\n",marginRe);
  PrintToLog("amount: %d\n",amount);
  PrintToLog("nBalance: %d\n",nBalance);
  PrintToLog("amountToReserve-------: %d\n",amountToReserve);
  PrintToLog("----------------------------------------------------------\n");

  if (nBalance < amountToReserve || nBalance == 0)
    {
      PrintToLog("%s(): rejected: sender %s has insufficient balance for contracts %d [%s < %s] \n",
		  __func__,
		  sender,
		  property,
		  FormatMP(property, nBalance),
		  FormatMP(property, amountToReserve));
      return (PKT_ERROR_SEND -25);

    }
  else if (conv != 0)
    {
      if (amountToReserve > 0)
	{
	  assert(update_tally_map(sender, sp.collateral_currency, -amountToReserve, BALANCE));
	  assert(update_tally_map(sender, sp.collateral_currency,  amountToReserve, CONTRACTDEX_RESERVE));
	}

      int64_t reserva = getMPbalance(sender, sp.collateral_currency,CONTRACTDEX_RESERVE);
      std::string reserved = FormatDivisibleMP(reserva,false);

    }
  t_tradelistdb->recordNewTrade(txid, sender, contractId, desired_property, block, tx_idx, 0);
  int rc = ContractDex_ADD(sender, contractId, amount, block, txid, tx_idx, effective_price, trading_action,0);

  return rc;
}

/** Tx 32 */
int CMPTransaction::logicMath_ContractDexCancelEcosystem()
{
    if (!IsTransactionTypeAllowed(block, ecosystem, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_METADEX -22);
    }

    if (OMNI_PROPERTY_MSC != ecosystem && OMNI_PROPERTY_TMSC != ecosystem) {
        PrintToLog("%s(): rejected: invalid ecosystem: %d\n", __func__, ecosystem);
        PrintToLog("rejected: invalid ecosystem %d\n",ecosystem);
        return (PKT_ERROR_METADEX -21);
    }
    PrintToLog("Inside the logicMath_ContractDexCancelEcosystem!!!!!\n");
    int rc = ContractDex_CANCEL_EVERYTHING(txid, block, sender, ecosystem, contractId);

    return rc;
}


/** Tx 33 */
int CMPTransaction::logicMath_ContractDexClosePosition()
{
    if (!IsTransactionTypeAllowed(block, ecosystem, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
            __func__,
            type,
            version,
            property,
            block);
        return (PKT_ERROR_METADEX -22);
    }

    if (OMNI_PROPERTY_MSC != ecosystem && OMNI_PROPERTY_TMSC != ecosystem) {
        PrintToLog("%s(): rejected: invalid ecosystem: %d\n", __func__, ecosystem);
        PrintToLog("rejected: invalid ecosystem %d\n",ecosystem);
        return (PKT_ERROR_METADEX -21);
    }

    CMPSPInfo::Entry sp;
    {
        LOCK(cs_tally);
        if (!_my_sps->getSP(contractId, sp)) {
            PrintToLog(" %s() : Property identifier %d does not exist\n",
                __func__,
                sender,
                contractId);
                PrintToLog("Property identifier no exist\n");
            return (PKT_ERROR_SEND -25);
        }
    }

    uint32_t collateralCurrency = sp.collateral_currency;
    int rc = ContractDex_CLOSE_POSITION(txid, block, sender, ecosystem, contractId, collateralCurrency);

    return rc;
}

int CMPTransaction::logicMath_ContractDex_Cancel_Orders_By_Block()
{
  // if (!IsTransactionTypeAllowed(block, ecosystem, type, version)) {
  //     PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
  //             __func__,
  //             type,
  //             version,
  //             propertyId,
  //             block);
    //  return (PKT_ERROR_METADEX -22);
  // }

  if (OMNI_PROPERTY_MSC != ecosystem && OMNI_PROPERTY_TMSC != ecosystem) {
      PrintToLog("%s(): rejected: invalid ecosystem: %d\n", __func__, ecosystem);
      PrintToLog("rejected: invalid ecosystem %d\n",ecosystem);
      return (PKT_ERROR_METADEX -21);
  }

    ContractDex_CANCEL_FOR_BLOCK(txid, block, tx_idx, sender, ecosystem);
    int rc = 0;

    return rc;
}

/** Tx 100 */
int CMPTransaction::logicMath_CreatePeggedCurrency()
{
    uint256 blockHash;
    uint32_t den;
    uint32_t marginReq = 0;
    uint32_t notSize = 0;
    uint32_t npropertyId = 0;
    int64_t amountNeeded;
    int64_t contracts;
    // int index = static_cast<int>(contractId);

    {
        LOCK(cs_main);

        CBlockIndex* pindex = chainActive[block];
        if (pindex == NULL) {
            PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
            return (PKT_ERROR_SP -20);
        }

        blockHash = pindex->GetBlockHash();
    }

    if (OMNI_PROPERTY_MSC != ecosystem && OMNI_PROPERTY_TMSC != ecosystem) {
        PrintToLog("%s(): rejected: invalid ecosystem: %d\n", __func__, (uint32_t) ecosystem);
        return (PKT_ERROR_SP -21);
    }
//
//     if (!IsTransactionTypeAllowed(block, ecosystem, type, version)) {
//         PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
//                 __func__,
//                 type,
//                 version,
//                 property,
//                 block);
//         return (PKT_ERROR_SP -22);
//     }

    if (MSC_PROPERTY_TYPE_INDIVISIBLE != prop_type && MSC_PROPERTY_TYPE_DIVISIBLE != prop_type) {
        PrintToLog("%s(): rejected: invalid property type: %d\n", __func__, prop_type);
        return (PKT_ERROR_SP -36);
    }

    if ('\0' == name[0]) {
        PrintToLog("%s(): rejected: property name must not be empty\n", __func__);
        PrintToLog("rejected: property name must not be empty\n");
        return (PKT_ERROR_SP -37);
    }

        // checking collateral currency
    int64_t nBalance = getMPbalance(sender, propertyId, BALANCE);
    if (nBalance == 0) {
        PrintToLog("%s(): rejected: sender %s has insufficient collateral currency in balance %d \n",
             __func__,
             sender,
             propertyId);
             PrintToLog("rejected: sender has insufficient collateral currency in balance\n");
        return (PKT_ERROR_SEND -25);
    }

    CMPSPInfo::Entry sp;
    {
        LOCK(cs_tally);

        if (!_my_sps->getSP(contractId, sp)) {
            PrintToLog(" %s() : Property identifier %d does not exist\n",
                __func__,
                sender,
                contractId);
            return (PKT_ERROR_SEND -25);

        } else if (sp.subcategory != "Futures Contracts") {
            PrintToLog(" %s() : property identifier %d does not a future contract\n",
            __func__,
            sender,
            contractId);
            return (PKT_ERROR_SEND -25);

        } else if (sp.collateral_currency != propertyId) {
            PrintToLog(" %s() : Future contract has not this collateral currency %d\n",
            __func__,
            sender,
            propertyId);
            return (PKT_ERROR_SEND -25);

        }

        marginReq = sp.margin_requirement;
        notSize = static_cast<int64_t>(sp.notional_size);
        den = sp.denomination;
        PrintToLog("denomination in the contract: %d, marginReq %d\n", den, marginReq);
    }

    int64_t position = getMPbalance(sender, contractId, NEGATIVE_BALANCE);
    arith_uint256 rAmount = ConvertTo256(amount); // Alls needed
    arith_uint256 Contracts = DivideAndRoundUp(rAmount * ConvertTo256(notSize), ConvertTo256(factorE));
    amountNeeded = ConvertTo64(rAmount);
    contracts = ConvertTo64(Contracts * ConvertTo256(factorE));

    PrintToLog("short position: %d\n",position);
    PrintToLog("notSize : %d\n",notSize);
    PrintToLog("amount of pegged: %d\n", amount);
    PrintToLog("nBalance : %d\n", nBalance);
    PrintToLog("contracts : %d\n", contracts);
    PrintToLog("Required ALLs : %d\n", amountNeeded);
    PrintToLog("Position : %d\n",position);
    PrintToLog("--------------------------------------------------%d\n");

    if (nBalance < amountNeeded || position < contracts) {
        PrintToLog("rejected:Sender has not required short position on this contract or balance enough\n");
        return (PKT_ERROR_SEND -25);
    }
    // back to normality contracts needed:
    PrintToLog("contracts Needed in normality : %d", contracts);

    {
        LOCK(cs_tally);
        uint32_t nextSPID = _my_sps->peekNextSPID(1);
        for (uint32_t propertyId = 1; propertyId < nextSPID; propertyId++) {
            CMPSPInfo::Entry sp;
            if (_my_sps->getSP(propertyId, sp)) {
                PrintToLog("Property Id: %d\n",propertyId);
                PrintToLog("den: %d\n",den);
                PrintToLog("denomination: %d\n",sp.denomination);
                if (sp.subcategory == "Pegged Currency" && sp.denomination == den){
                    PrintToLog("We have another pegged currency with the denomination: %d\n",den);
                    npropertyId = propertyId;
                    break;
                }
            }
        }
    }

    // ------------------------------------------

    if (npropertyId == 0) {   // putting the first one pegged currency of this denomination
        PrintToLog("putting the first one pegged currency of this denomination \n");
        CMPSPInfo::Entry newSP;
        newSP.issuer = sender;
        newSP.txid = txid;
        newSP.prop_type = prop_type;
        newSP.subcategory.assign(subcategory);
        newSP.name.assign(name);
        newSP.fixed = true;
        newSP.manual = true;
        newSP.creation_block = blockHash;
        newSP.update_block = newSP.creation_block;
        newSP.num_tokens = amountNeeded;
        newSP.contracts_needed = contracts;
        newSP.contract_associated = contractId;
        newSP.denomination = den;
        newSP.series = strprintf("Nº 1 - %d",(amountNeeded / factorE));
        npropertyId = _my_sps->putSP(ecosystem, newSP);

    } else {
        CMPSPInfo::Entry newSP;
        _my_sps->getSP(npropertyId, newSP);
        PrintToLog("tokens before: %d\n",newSP.num_tokens);
        int64_t inf = (newSP.num_tokens) / factorE + 1 ;
        newSP.num_tokens += ConvertTo64(rAmount);
        int64_t sup = (newSP.num_tokens) / factorE ;
        newSP.series = strprintf("Nº %d - %d",inf,sup);
        _my_sps->updateSP(npropertyId, newSP);
    }

    assert(npropertyId > 0);
    CMPSPInfo::Entry SP;
    _my_sps->getSP(npropertyId, SP);
    assert(update_tally_map(sender, npropertyId, amount, BALANCE));
    t_tradelistdb->NotifyPeggedCurrency(txid, sender, npropertyId, amount,SP.series); //TODO: Watch this function!

    // Adding the element to map of pegged currency owners
    peggedIssuers.insert (std::pair<std::string,uint32_t>(sender,npropertyId));

    PrintToLog("Pegged currency Id: %d\n",npropertyId);
    PrintToLog("CREATED PEGGED PROPERTY id: %d admin: %s\n", npropertyId, sender);

    //putting into reserve contracts and collateral currency
    assert(update_tally_map(sender, contractId, -contracts, NEGATIVE_BALANCE));
    assert(update_tally_map(sender, contractId, contracts, CONTRACTDEX_RESERVE));
    assert(update_tally_map(sender, propertyId, -amountNeeded, BALANCE));
    assert(update_tally_map(sender, propertyId, amountNeeded, CONTRACTDEX_RESERVE));

    return 0;
}

int CMPTransaction::logicMath_SendPeggedCurrency()
{
    if (!IsTransactionTypeAllowed(block, propertyId, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
            __func__,
            type,
            version,
            property,
            block);
        PrintToLog("rejected: type or version not permitted for property at block\n");
        return (PKT_ERROR_SEND -22);
    }

    if (!IsPropertyIdValid(propertyId)) {
        PrintToLog("%s(): rejected: property %d does not exist\n", __func__, property);
        return (PKT_ERROR_SEND -24);
    }

    int64_t nBalance = getMPbalance(sender, propertyId, BALANCE);
    if (nBalance < (int64_t) amount) {
        PrintToLog("%s(): rejected: sender %s has insufficient balance of property %d [%s < %s]\n",
            __func__,
            sender,
            property,
            FormatMP(property, nBalance),
            FormatMP(property, nValue));
            PrintToLog("rejected: sender has insufficient balance of property\n");
            PrintToLog("nBalance Pegged Currency Sender : %d \n",nBalance);
            PrintToLog("amount to send of Pegged Currency : %d \n",amount);
        return (PKT_ERROR_SEND -25);
    }

    PrintToLog("nBalance Pegged Currency Sender : %d \n",nBalance);
    PrintToLog("amount to send of Pegged Currency : %d \n",amount);

    // ------------------------------------------

    // Special case: if can't find the receiver -- assume send to self!
    if (receiver.empty()) {
        receiver = sender;
    }

    // Move the tokensss

    assert(update_tally_map(sender, propertyId, -amount, BALANCE));
    assert(update_tally_map(receiver, propertyId, amount, BALANCE));

    return 0;
}

int CMPTransaction::logicMath_RedemptionPegged()
{

    // if (!IsTransactionTypeAllowed(block, propertyId, type, version)) {
    //     PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
    //             __func__,
    //             type,
    //             version,
    //             propertyId,
    //             block);
    //     PrintToLog("rejected: type %d or version %d not permitted for property %d at block %d\n");
    //     return (PKT_ERROR_SEND -22);
    // }

    if (!IsPropertyIdValid(propertyId)) {
        PrintToLog("%s(): rejected: property %d does not exist\n", __func__, property);
        //return (PKT_ERROR_SEND -24);
    }

    int64_t nBalance = getMPbalance(sender, propertyId, BALANCE);
    int64_t nContracts = getMPbalance(sender, contractId, CONTRACTDEX_RESERVE);
    int64_t negContracts = getMPbalance(sender, contractId, NEGATIVE_BALANCE);
    int64_t posContracts = getMPbalance(sender, contractId, POSSITIVE_BALANCE);

    PrintToLog("nBalance : %d\n",nBalance);
    PrintToLog("nContracts : %d\n",nContracts);
    PrintToLog("amount : %d\n",amount);
    PrintToLog("property : %d\n",propertyId);
    PrintToLog("contractId : %d\n",contractId);
    PrintToLog("Contracts in long position : %d\n",posContracts);
    PrintToLog("Contracts in short position : %d\n",negContracts);

    if (nBalance < (int64_t) amount) {
        PrintToLog("%s(): rejected: sender %s has insufficient balance of pegged currency %d [%s < %s]\n",
                __func__,
                sender,
                propertyId,
                FormatMP(propertyId, nBalance),
                FormatMP(propertyId, amount));
        return (PKT_ERROR_SEND -25);
    }

    uint32_t collateralId = 0;
    int64_t notSize = 0;

    CMPSPInfo::Entry sp;
    {
        LOCK(cs_tally);
        if (!_my_sps->getSP(contractId, sp)) {
            PrintToLog(" %s() : Property identifier %d does not exist\n",
            __func__,
            sender,
            contractId);
           return (PKT_ERROR_SEND -25);
        } else {
           collateralId = sp.collateral_currency;
           notSize = static_cast<int64_t>(sp.notional_size);
        }
    }

    arith_uint256 conNeeded = ConvertTo256(amount) / (ConvertTo256(notSize) * ConvertTo256(factorE));
    int64_t contractsNeeded = ConvertTo64(conNeeded);

    PrintToLog("contracts needed : %d\n",contractsNeeded);
    PrintToLog("Notional Size : %d\n",notSize);
    PrintToLog("collateral id : %d\n",collateralId);

    if ((contractsNeeded > 0) && (amount > 0)) {
       // Delete the tokens
       //assert(update_tally_map(sender, propertyId, -amount, BALANCE));
       // get back the contracts
       //assert(update_tally_map(sender, contractId, -contractsNeeded, CONTRACTDEX_RESERVE));
        // get back the collateral
       //assert(update_tally_map(sender, collateralId, -amount, CONTRACTDEX_RESERVE));
       //assert(update_tally_map(sender, collateralId, amount, BALANCE));
       if ((posContracts > 0) && (negContracts == 0)){

         // int64_t dif = posContracts - contractsNeeded;
        //  PrintToLog("Difference of contracts : %d\n",dif);
         // if (dif >= 0){
            // assert(update_tally_map(sender, contractId, -contractsNeeded, POSSITIVE_BALANCE));
         // }else {
           //  assert(update_tally_map(sender, contractId, -posContracts, POSSITIVE_BALANCE));
           //  assert(update_tally_map(sender, contractId, -dif, NEGATIVE_BALANCE));
         // }
       } else if ((posContracts == 0) && (negContracts >= 0)) {
          //assert(update_tally_map(sender, contractId, contractsNeeded, NEGATIVE_BALANCE));
       }

    } else {
        PrintToLog("amount redeemed must be equal at least to value of one future contract \n");
        // return (PKT_ERROR_SEND -25);
    }

    return 0;
}

int CMPTransaction::logicMath_TradeOffer()
{
    if (!IsTransactionTypeAllowed(block, propertyId, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
            __func__,
            type,
            version,
            propertyId,
            block);
      // return (PKT_ERROR_TRADEOFFER -22);
    }

    if (MAX_INT_8_BYTES < nValue) {
        PrintToLog("%s(): rejected: value out of range or zero: %d\n", __func__, nValue);
        // return (PKT_ERROR_TRADEOFFER -23);
    }

  // if (OMNI_PROPERTY_TMSC != property && OMNI_PROPERTY_MSC != property) {
  //     PrintToLog("%s(): rejected: property for sale %d must be OMNI or TOMNI\n", __func__, property);
  //     return (PKT_ERROR_TRADEOFFER -47);
  // }

  // ------------------------------------------

      int rc = PKT_ERROR_TRADEOFFER;

    // figure out which Action this is based on amount for sale, version & etc.
    switch (version)
    {
        case MP_TX_PKT_V0:
        {
            if (0 != nValue) {

                if (!DEx_offerExists(sender, propertyId)) {
                    PrintToLog("Dex offer doesn't exist\n");
                    rc = DEx_offerCreate(sender, propertyId, nValue, block, amountDesired, minFee, timeLimit, txid, &nNewValue);
                } else {
                    rc = DEx_offerUpdate(sender, propertyId, nValue, block, amountDesired, minFee, timeLimit, txid, &nNewValue);
                }
            } else {
                // what happens if nValue is 0 for V0 ?  ANSWER: check if exists and it does -- cancel, otherwise invalid
                if (DEx_offerExists(sender, propertyId)) {
                    rc = DEx_offerDestroy(sender, propertyId);
                } else {
                    PrintToLog("%s(): rejected: sender %s has no active sell offer for property: %d\n", __func__, sender, propertyId);
                    rc = (PKT_ERROR_TRADEOFFER -49);
                }
            }

            break;
        }

        case MP_TX_PKT_V1:
        {
            PrintToLog("Case MP_TX_PKT_V1\n");
            if (DEx_offerExists(sender, propertyId)) {
                if (CANCEL != subAction && UPDATE != subAction) {
                    PrintToLog("%s(): rejected: sender %s has an active sell offer for property: %d\n", __func__, sender, property);
                    rc = (PKT_ERROR_TRADEOFFER -48);
                    break;
                }
            } else {
                // Offer does not exist
                if (NEW != subAction) {
                    PrintToLog("%s(): rejected: sender %s has no active sell offer for property: %d\n", __func__, sender, property);
                    rc = (PKT_ERROR_TRADEOFFER -49);
                    break;
                }
            }

            switch (subAction) {
                case NEW:
                    PrintToLog("Subaction: NEW\n");
                    rc = DEx_offerCreate(sender, propertyId, nValue, block, amountDesired, minFee, timeLimit, txid, &nNewValue);
                    break;

                case UPDATE:
                    rc = DEx_offerUpdate(sender, propertyId, nValue, block, amountDesired, minFee, timeLimit, txid, &nNewValue);
                    break;

                case CANCEL:
                    rc = DEx_offerDestroy(sender, propertyId);
                    break;

                default:
                    rc = (PKT_ERROR -999);
                    break;
            }
            break;
        }

        default:
            rc = (PKT_ERROR -500); // neither V0 nor V1
            break;
};

  return rc;
}

int CMPTransaction::logicMath_DExBuy()
{
    if (!IsTransactionTypeAllowed(block, propertyId, type, version)) {
     // PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
     //         __func__,
     //         type,
     //         version,
     //         propertyId,
     //         block);
      // return (PKT_ERROR_TRADEOFFER -22);
    }

    if (MAX_INT_8_BYTES < nValue) {
        PrintToLog("%s(): rejected: value out of range or zero: %d\n", __func__, nValue);
        // return (PKT_ERROR_TRADEOFFER -23);
    }

  // if (OMNI_PROPERTY_TMSC != property && OMNI_PROPERTY_MSC != property) {
  //     PrintToLog("%s(): rejected: property for sale %d must be OMNI or TOMNI\n", __func__, property);
  //     return (PKT_ERROR_TRADEOFFER -47);
  // }

  // ------------------------------------------

    int rc = PKT_ERROR_TRADEOFFER;
    PrintToLog("Inside logicMath_DExBuy function ----------------------------\n");
    // figure out which Action this is based on amount for sale, version & etc.
    switch (version)
    {
        case MP_TX_PKT_V0:
        {
            if (0 != nValue) {

                if (!DEx_offerExists(sender, propertyId)) {
                    PrintToLog("Dex offer doesn't exist\n");
                    rc = DEx_BuyOfferCreate(sender, propertyId, nValue, block, effective_price, minFee, timeLimit, txid, &nNewValue);
                } else {
                  rc = DEx_offerUpdate(sender, propertyId, nValue, block, effective_price, minFee, timeLimit, txid, &nNewValue);
                }
            } else {
                // what happens if nValue is 0 for V0 ?  ANSWER: check if exists and it does -- cancel, otherwise invalid
                  if (DEx_offerExists(sender, propertyId)) {
                      rc = DEx_offerDestroy(sender, propertyId);
                  } else {
                     PrintToLog("%s(): rejected: sender %s has no active sell offer for property: %d\n", __func__, sender, propertyId);
                    rc = (PKT_ERROR_TRADEOFFER -49);
                  }
            }

            break;
        }

        case MP_TX_PKT_V1:
        {
            PrintToLog("Case MP_TX_PKT_V1\n");
            if (DEx_offerExists(sender, propertyId)) {
                if (CANCEL != subAction && UPDATE != subAction) {
                    PrintToLog("%s(): rejected: sender %s has an active sell offer for property: %d\n", __func__, sender, property);
                    rc = (PKT_ERROR_TRADEOFFER -48);
                    break;
                }
            } else {
                // Offer does not exist
                if (NEW != subAction) {
                    PrintToLog("%s(): rejected: sender %s has no active sell offer for property: %d\n", __func__, sender, property);
                    rc = (PKT_ERROR_TRADEOFFER -49);
                    break;
                }
            }

            switch (subAction) {
                case NEW:
                    PrintToLog("Subaction: NEW\n");
                    rc = DEx_BuyOfferCreate(sender, propertyId, nValue, block, effective_price, minFee, timeLimit, txid, &nNewValue);
                    break;

              case UPDATE:
                  rc = DEx_offerUpdate(sender, propertyId, nValue, block, effective_price, minFee, timeLimit, txid, &nNewValue);
                  break;

              case CANCEL:
                  rc = DEx_offerDestroy(sender, propertyId);
                  break;

                default:
                    rc = (PKT_ERROR -999);
                    break;
            }
        }
        break;
    }

    return rc;
}

int CMPTransaction::logicMath_AcceptOfferBTC()
{
    PrintToLog("Inside logicMath_AcceptOffer_BTC ----------------------------\n");
  //  if (!IsTransactionTypeAllowed(block, propertyId, type, version)) {
  //      PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
  //              __func__,
  //              type,
  //              version,
  //              propertyId,
  //              block);
  //      return (DEX_ERROR_ACCEPT -22);
  //  }

    if (nValue <= 0 || MAX_INT_8_BYTES < nValue) {
        PrintToLog("%s(): rejected: value out of range or zero: %d\n", __func__, nValue);
      // return (DEX_ERROR_ACCEPT -23);
    }

    // ------------------------------------------

    // the min fee spec requirement is checked in the following function
    int rc = DEx_acceptCreate(sender, receiver, propertyId, nValue, block, tx_fee_paid, &nNewValue);

    return rc;
}
