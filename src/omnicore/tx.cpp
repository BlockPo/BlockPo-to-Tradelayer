// Master Protocol transaction code

#include "omnicore/tx.h"

#include "omnicore/activation.h"
#include "omnicore/convert.h"
#include "omnicore/log.h"
#include "omnicore/notifications.h"
#include "omnicore/omnicore.h"
#include "omnicore/rules.h"
#include "omnicore/sp.h"
#include "omnicore/varint.h"
#include "omnicore/mdex.h"

#include "amount.h"
#include "main.h"
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
#include <inttypes.h>


using boost::algorithm::token_compress_on;

using namespace mastercore;
const int64_t factor = 100000000;

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

        /** New things for Contract */
        case MSC_TYPE_CONTRACTDEX_TRADE: return "Future Contract";
        // case MSC_TYPE_CONTRACTDEX_CANCEL_PRICE: return "ContractDex cancel-price";
        case MSC_TYPE_CONTRACTDEX_CANCEL_ECOSYSTEM: return "ContractDex cancel-ecosystem";
        case MSC_TYPE_CREATE_CONTRACT: return "Create Contract";
        case MSC_TYPE_PEGGED_CURRENCY: return "Pegged Currency";
        case MSC_TYPE_REDEMPTION_PEGGED: return "Redemption Pegged Currency";
        case MSC_TYPE_SEND_PEGGED_CURRENCY: return "Send Pegged Currency";

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
      /*New things for contracts*///////////////////////////////////////////////
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
      //////////////////////////////////////////////////////////////////////////
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
/*New things for contracts*/////////////////////////////////////////////////////
/** Tx  40*/
bool CMPTransaction::interpret_CreateContractDex()
{
    int i = 0;

    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);

    memcpy(&ecosystem, &pkt[i], 1);
    i++;
    std::vector<uint8_t> vecPropTypeBytes = GetNextVarIntBytes(i);

    const char* p = i + (char*) &pkt;
    std::vector<std::string> spstr;
    for (int j = 0; j < 2; j++) {
        spstr.push_back(std::string(p));
        p += spstr.back().size() + 1;
    }

    if (isOverrun(p)) {
        PrintToLog("%s(): rejected: malformed string value(s)\n", __func__);
        return false;
    }

    int j = 0;
    memcpy(subcategory, spstr[j].c_str(), std::min(spstr[j].length(), sizeof(subcategory)-1)); j++;
    memcpy(name, spstr[j].c_str(), std::min(spstr[j].length(), sizeof(name)-1)); j++;
    i = i + strlen(subcategory) + strlen(name) + 2; // data sizes + 3 null terminators
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

    if (!vecPropTypeBytes.empty()) {
        prop_type = DecompressInteger(vecPropTypeBytes);
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
    PrintToConsole("------------------------------------------------------------\n");
    PrintToConsole("Inside interpret_CreateContractDex function\n");
    PrintToConsole("version: %d\n", version);
    PrintToConsole("messageType: %d\n",type);
    PrintToConsole("ecosystem: %d\n", ecosystem);
    PrintToConsole("property type: %d\n", prop_type);
    PrintToConsole("name: %s\n", name);
    PrintToConsole("subcategory: %s\n", subcategory);
    PrintToConsole("blocks until expiration : %d\n", blocks_until_expiration);
    PrintToConsole("notional size : %d\n", notional_size);
    PrintToConsole("collateral currency: %d\n", collateral_currency);
    PrintToConsole("margin requirement: %d\n", margin_requirement);
    PrintToConsole("------------------------------------------------------------\n");
    return true;
}
/**Tx 29 */
bool CMPTransaction::interpret_ContractDexTrade()
{
  PrintToConsole("Inside of trade contractdexTrade function\n");
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

  PrintToConsole("version: %d\n", version);
  PrintToConsole("messageType: %d\n",type);
  PrintToConsole("contractId: %d\n", contractId);
  PrintToConsole("amount of contracts : %d\n", amount);
  PrintToConsole("effective price : %d\n", effective_price);
  PrintToConsole("trading action : %d\n", trading_action);

  return true;
}

/** Tx 32 */
bool CMPTransaction::interpret_ContractDexCancelEcosystem()
{
    PrintToConsole("Inside the interpret_ContractDexCancelEcosystem!!!!!\n");
    int i = 0;

    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecEcosystemBytes = GetNextVarIntBytes(i);


    if (!vecTypeBytes.empty()) {
        type = DecompressInteger(vecTypeBytes);
    } else return false;

    if (!vecVersionBytes.empty()) {
        version = DecompressInteger(vecVersionBytes);
    } else return false;

    if (!vecEcosystemBytes.empty()) {
        ecosystem = DecompressInteger(vecEcosystemBytes);
    } else return false;

    PrintToConsole("version: %d\n", version);
    PrintToConsole("messageType: %d\n",type);
    PrintToConsole("ecosystem: %d\n", ecosystem);
    return true;
  }

  /** Tx 101 */
  bool CMPTransaction::interpret_CreatePeggedCurrency()
  {

      PrintToConsole("Inside the interpret_CreatePeggedCurrency function!!! \n");

      int i = 0;

      std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
      std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);

      memcpy(&ecosystem, &pkt[i], 1);
      i++;
      std::vector<uint8_t> vecPropTypeBytes = GetNextVarIntBytes(i);
      std::vector<uint8_t> vecPrevPropIdBytes = GetNextVarIntBytes(i);
      const char* p = i + (char*) &pkt;
      std::vector<std::string> spstr;
      for (int j = 0; j < 2; j++){
          spstr.push_back(std::string(p));
          p += spstr.back().size() + 1;
      }

      if (isOverrun(p)) {
          PrintToLog("%s(): rejected: malformed string value(s)\n", __func__);
          return false;
      }

      int j = 0;
      memcpy(subcategory, spstr[j].c_str(), std::min(spstr[j].length(), sizeof(subcategory)-1)); j++;
      memcpy(name, spstr[j].c_str(), std::min(spstr[j].length(), sizeof(name)-1)); j++;
      i = i + strlen(name) + strlen(subcategory)  + 2; // data sizes + 3 null terminators
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

        PrintToConsole("version: %d\n", version);
        PrintToConsole("messageType: %d\n",type);
        PrintToConsole("ecosystem: %d\n", ecosystem);
        PrintToConsole("property type: %d\n",prop_type);
        PrintToConsole("prev prop id: %d\n",prev_prop_id);
        PrintToConsole("contractId: %d\n", contractId);
        PrintToConsole("propertyId: %d\n", propertyId);
        PrintToConsole("amount of pegged currency : %d\n", amount);
        PrintToConsole("name : %d\n", name);
        PrintToConsole("subcategory: %d\n", subcategory);

      return true;
  }

  bool CMPTransaction::interpret_SendPeggedCurrency()
  {

    PrintToConsole("Inside the interpret_SendPeggedCurrency function!!! \n");

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

    PrintToConsole("version: %d\n", version);
    PrintToConsole("messageType: %d\n",type);
    PrintToConsole("propertyId: %d\n", propertyId);
    PrintToConsole("amount of pegged currency : %d\n", amount);
    return true;
  }

 bool CMPTransaction::interpret_RedemptionPegged(){

    PrintToConsole("Inside the interpret_RedemptionPegged function!!! \n");

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

    PrintToConsole("version: %d\n", version);
    PrintToConsole("messageType: %d\n",type);
    PrintToConsole("propertyId: %d\n", propertyId);
    PrintToConsole("contractId: %d\n", contractId);
    PrintToConsole("amount of pegged currency : %d\n", amount);

    return true;
 }

////////////////////////////////////////////////////////////////////////////////



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
     /*New things for contracts*////////////////////////////////////////////////
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

    ////////////////////////////////////////////////////////////////////////////
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

/*New things for contracts*/////////////////////////////////////////////////////


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
    newSP.subcategory.assign(subcategory);
    newSP.name.assign(name);
    // newSP.url.assign(url);
    // newSP.data.assign(data);
    newSP.fixed = false;
    // newSP.property_desired = property;
    // newSP.deadline = deadline;
    // newSP.early_bird = early_bird;
    // newSP.percentage = percentage;
    newSP.creation_block = blockHash;
    newSP.update_block = newSP.creation_block;

    ////////////////////////////////////////
    /** New things for Contracts */
    newSP.blocks_until_expiration = blocks_until_expiration;
    newSP.notional_size = notional_size;
    newSP.collateral_currency = collateral_currency;
    newSP.margin_requirement = margin_requirement;
    newSP.init_block = block;

    const uint32_t propertyId = _my_sps->putSP(ecosystem, newSP);
    assert(propertyId > 0);
    PrintToConsole("Contract id: %d\n",propertyId);
    PrintToConsole("------------------------------------------------------------\n");
    PrintToConsole("Inside logicMath_CreateContractDex function\n");
    PrintToConsole("version: %d\n", version);
    PrintToConsole("messageType: %d\n",type);
    PrintToConsole("ecosystem: %d\n", ecosystem);
    PrintToConsole("property type: %d\n", prop_type);
    PrintToConsole("name: %s\n", name);
    PrintToConsole("subcategory: %s\n", subcategory);
    PrintToConsole("blocks until expiration : %d\n", blocks_until_expiration);
    PrintToConsole("notional size : %d\n", notional_size);
    PrintToConsole("collateral currency: %d\n", collateral_currency);
    PrintToConsole("margin requirement: %d\n", margin_requirement);
    PrintToConsole("------------------------------------------------------------\n");


    return 0;
}

/** Tx 29 */
int CMPTransaction::logicMath_ContractDexTrade()
{
  PrintToConsole("----------------------------------------------------------\n");
  PrintToConsole("Inside of logicMath_ContractDexTrade\n");
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
//
// if (OMNI_PROPERTY_MSC != ecosystem && OMNI_PROPERTY_TMSC != ecosystem) {
//     PrintToLog("%s(): rejected: invalid ecosystem: %d\n", __func__, (uint32_t) ecosystem);
//     return (PKT_ERROR_SP -21);
// }
//
// if (!IsTransactionTypeAllowed(block, ecosystem, type, version)) {
//     PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
//             __func__,
//             type,
//             version,
//             property,
//             block);
//     return (PKT_ERROR_SP -22);
// }



// if (MSC_PROPERTY_TYPE_INDIVISIBLE != prop_type && MSC_PROPERTY_TYPE_DIVISIBLE != prop_type) {
//     PrintToLog("%s(): rejected: invalid property type: %d\n", __func__, prop_type);
//     return (PKT_ERROR_SP -36);
// }

CMPSPInfo::Entry sp;
assert(_my_sps->getSP(contractId, sp));

margin_requirement = sp.margin_requirement;
collateral_currency = sp.collateral_currency;
PrintToConsole("margin requirement of contract : %d\n",margin_requirement);
PrintToConsole("collateral currency id of contract : %d\n",collateral_currency);
// ------------------------------------------
double percentLiqPrice = 0.8;

int64_t nBalance = getMPbalance(sender, collateral_currency, BALANCE);
uint32_t Sum = margin_requirement;
int64_t amountToReserve = static_cast<int64_t>(amount*Sum*factor);
PrintToConsole("nBalance: %d\n",nBalance);
PrintToConsole("margin requirement: %d\n",Sum);
PrintToConsole("amountToReserve: %d\n",amountToReserve);
PrintToConsole("----------------------------------------------------------\n");
if (nBalance < amountToReserve) {
    PrintToLog("%s(): rejected: sender %s has insufficient balance for contracts %d [%s < %s]\n",
            __func__,
            sender,
            property,
            FormatMP(property, nBalance),
            FormatMP(property, amountToReserve));
    PrintToConsole("rejected: sender has insufficient balance for contracts\n");
    return (PKT_ERROR_SEND -25);
} else {

    assert(update_tally_map(sender, collateral_currency, -amountToReserve, BALANCE));
    assert(update_tally_map(sender, collateral_currency,  amountToReserve, CONTRACTDEX_RESERVE));

    t_tradelistdb->recordNewTrade(txid, sender, property, desired_property, block, tx_idx);

    int rc = ContractDex_ADD(sender, contractId, amount, block, txid, tx_idx, effective_price, trading_action,amountToReserve);
    //SOCKET
    char buffAction[10];
    char buffTradingAction[3];
    char buffQuantity[21];
    char buffPrice[21];
    char buffEnd[10];
    char buffProperty[10];
    char buffSeparator[10];
    char buffer[512];

    sprintf(buffAction, "%s", "trade");
    sprintf(buffTradingAction, "%u", trading_action);
    sprintf(buffQuantity, "%" PRIu64, nValue);
    sprintf(buffPrice, "%" PRIu64, effective_price);
    sprintf(buffProperty, "%u", contractId);
    sprintf(buffEnd, "%s", "<<<");
    sprintf(buffSeparator, "%s", "-");

    strcpy(buffer,buffAction);
    strcat(buffer,buffSeparator);
    strcat(buffer,buffQuantity);
    strcat(buffer,buffSeparator);
    strcat(buffer,buffPrice);
    strcat(buffer,buffSeparator);
    strcat(buffer,buffProperty);
    strcat(buffer,buffSeparator);
    strcat(buffer,buffTradingAction);
    strcat(buffer,buffEnd);

    int sockfd;

    sockfd = socket(AF_INET,SOCK_DGRAM,0);
    struct sockaddr_in serv;
    serv.sin_family = AF_INET;
    serv.sin_port = htons(666);
    serv.sin_addr.s_addr = inet_addr("127.0.0.1");
    socklen_t m = sizeof(serv);
    sendto(sockfd,buffer,sizeof(buffer),0,(struct sockaddr *)&serv,m);


    return rc;
  }
  return 0;
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
        PrintToConsole("rejected: invalid ecosystem %d\n",ecosystem);
        return (PKT_ERROR_METADEX -21);
    }
    PrintToConsole("Inside the logicMath_ContractDexCancelEcosystem!!!!!\n");
    int rc = ContractDex_CANCEL_EVERYTHING(txid, block, sender, ecosystem);

    return rc;
}
/** Tx 100 */
int CMPTransaction::logicMath_CreatePeggedCurrency()
{
    PrintToConsole("------------------------------------------------------------\n");
    PrintToConsole("Inside logicMath_CreatePeggedCurrency !!!!!\n");
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
        PrintToConsole("rejected: property name must not be empty\n");
        return (PKT_ERROR_SP -37);
    }

        // checking collateral currency
    uint64_t nBalance = static_cast<uint64_t>(getMPbalance(sender, propertyId, BALANCE));
    if (nBalance == 0) {
        PrintToLog("%s(): rejected: sender %s has insufficient collateral currency in balance %d \n",
           __func__,
           sender,
           propertyId);
           PrintToConsole("rejected: sender has insufficient collateral currency in balance\n");
           return (PKT_ERROR_SEND -25);
        }

    uint32_t marginReq = 0;
    uint32_t notSize = 0;
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
       notSize = sp.notional_size;
    }
     int64_t position = getMPbalance(sender, contractId, NEGATIVE_BALANCE);

     int64_t contractsNeeded = static_cast<int64_t> (amount/(notSize*factor)); // We must check the role of marginReq here.


     PrintToConsole("Address of sender : %s\n",sender);
     PrintToConsole("Property type : %d\n",prop_type);
     PrintToConsole("Collateral currency Id : %d\n",propertyId);
     PrintToConsole("Contract Id : %d\n",contractId);
     PrintToConsole("Amount of pegged currency created: %d\n",amount);
     PrintToConsole("nBalance : %d\n",nBalance);
     PrintToConsole("Contracts Needed : %d\n",contractsNeeded);
     PrintToConsole("Notional Size : %d\n",notSize);
     PrintToConsole("Short Position : %d\n",position);
     PrintToConsole("Margin Requirement : %d\n",marginReq);


     if ((nBalance < amount) || (position < contractsNeeded)) {
   //PrintToLog("%s(): rejected:Sender has not required short position on this contract %d \n",
   //       __func__,
   //       sender,
   //       propertyId);
   PrintToConsole("rejected:Sender has not required short position on this contract\n");
   return (PKT_ERROR_SEND -25);
   }

    // ------------------------------------------
   PrintToConsole("First test point\n");
   CMPSPInfo::Entry newSP;
   newSP.issuer = sender;
   newSP.txid = txid;
   newSP.prop_type = prop_type;
   // newSP.category.assign(category);
   newSP.subcategory.assign(subcategory);
   newSP.name.assign(name);
   // newSP.url.assign(url);
   // newSP.data.assign(data);
   newSP.fixed = true;
   newSP.manual = true;
   newSP.creation_block = blockHash;
   newSP.update_block = newSP.creation_block;
   newSP.num_tokens = amount;
   newSP.contracts_needed = contractsNeeded;
   newSP.contract_associated = contractId;
  //  ecosystem = 1;
   const uint32_t npropertyId = _my_sps->putSP(ecosystem, newSP);
   assert(npropertyId > 0);
  //  PrintToConsole("checking ecosystem \n",ecosystem);
  //  PrintToConsole("Pegged currency Id: %d\n",npropertyId);
   assert(update_tally_map(sender, npropertyId, amount, BALANCE));
  // NotifyTotalTokensChanged(npropertyId, block);

  // PrintToLog("CREATED MANUAL PROPERTY id: %d admin: %s\n", propertyId, sender);




    //putting into reserve contracts and collateral currency
    assert(update_tally_map(sender, contractId, -contractsNeeded, NEGATIVE_BALANCE));
    assert(update_tally_map(sender, contractId, contractsNeeded, CONTRACTDEX_RESERVE));
    // PrintToConsole("Third test point\n");
    assert(update_tally_map(sender, propertyId, -amount, BALANCE));
    assert(update_tally_map(sender, propertyId, amount, CONTRACTDEX_RESERVE));

    // PrintToConsole("------------------------------------------------------------\n");
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
        PrintToConsole("rejected: type or version not permitted for property at block\n");
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
                PrintToConsole("rejected: sender has insufficient balance of property\n");
                PrintToConsole("nBalance Pegged Currency Sender : %d \n",nBalance);
                PrintToConsole("amount to send of Pegged Currency : %d \n",amount);
        return (PKT_ERROR_SEND -25);
    }
    PrintToConsole("nBalance Pegged Currency Sender : %d \n",nBalance);
    PrintToConsole("amount to send of Pegged Currency : %d \n",amount);

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
    PrintToConsole("____________________________________________________________\n");
    PrintToConsole("Inside logicMath_RedemptionPegged !!!!!\n");
    // if (!IsTransactionTypeAllowed(block, propertyId, type, version)) {
    //     PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
    //             __func__,
    //             type,
    //             version,
    //             propertyId,
    //             block);
    //     PrintToConsole("rejected: type %d or version %d not permitted for property %d at block %d\n");
    //     return (PKT_ERROR_SEND -22);
    // }

    if (!IsPropertyIdValid(propertyId)) {
        PrintToLog("%s(): rejected: property %d does not exist\n", __func__, property);
        return (PKT_ERROR_SEND -24);
    }

    int64_t nBalance = getMPbalance(sender, propertyId, BALANCE);
    int64_t nContracts = getMPbalance(sender, contractId, CONTRACTDEX_RESERVE);
    int64_t negContracts = getMPbalance(sender, contractId, NEGATIVE_BALANCE);
    int64_t posContracts = getMPbalance(sender, contractId, POSSITIVE_BALANCE);
    PrintToConsole("nBalance : %d\n",nBalance);
    PrintToConsole("nContracts : %d\n",nContracts);
    PrintToConsole("amount : %d\n",amount);
    PrintToConsole("property : %d\n",propertyId);
    PrintToConsole("contractId : %d\n",contractId);
    PrintToConsole("Contracts in long position : %d\n",posContracts);
    PrintToConsole("Contracts in short position : %d\n",negContracts);

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
    uint32_t notSize = 0;
    const int64_t factor = 100000000;
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
           notSize = sp.notional_size;
        }
    }

    int64_t contractsNeeded = static_cast<int64_t> (amount/(notSize*factor));

       PrintToConsole("contracts needed : %d\n",contractsNeeded);
       PrintToConsole("Notional Size : %d\n",notSize);
       PrintToConsole("collateral id : %d\n",collateralId);

    if ((contractsNeeded > 0) && (amount > 0)) {
       // Delete the tokens
       assert(update_tally_map(sender, propertyId, -amount, BALANCE));
       // get back the contracts
       assert(update_tally_map(sender, contractId, -contractsNeeded, CONTRACTDEX_RESERVE));
        // get back the collateral
       assert(update_tally_map(sender, collateralId, -amount, CONTRACTDEX_RESERVE));
       assert(update_tally_map(sender, collateralId, amount, BALANCE));
       if ((posContracts > 0) && (negContracts == 0)){

          int64_t dif = posContracts - contractsNeeded;
          PrintToConsole("Difference of contracts : %d\n",dif);
          if (dif >= 0){
             assert(update_tally_map(sender, contractId, -contractsNeeded, POSSITIVE_BALANCE));
          }else {
             assert(update_tally_map(sender, contractId, -posContracts, POSSITIVE_BALANCE));
             assert(update_tally_map(sender, contractId, -dif, NEGATIVE_BALANCE));
          }
       } else if ((posContracts == 0) && (negContracts >= 0)) {
          assert(update_tally_map(sender, contractId, contractsNeeded, NEGATIVE_BALANCE));
       }

    }else {
       PrintToLog("amount redeemed must be equal at least to value of one future contract \n");
       PrintToConsole("amount redeemed must be equal at least to value of one future contract \n");
       return (PKT_ERROR_SEND -25);
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
