// Master Protocol transaction code

#include <tradelayer/tx.h>

#include <tradelayer/activation.h>
#include <tradelayer/ce.h>
#include <tradelayer/convert.h>
#include <tradelayer/dex.h>
#include <tradelayer/externfns.h>
#include <tradelayer/log.h>
#include <tradelayer/mdex.h>
#include <tradelayer/notifications.h>
#include <tradelayer/parse_string.h>
#include <tradelayer/register.h>
#include <tradelayer/rules.h>
#include <tradelayer/sp.h>
#include <tradelayer/tradelayer.h>
#include <tradelayer/tradelayer_matrices.h>
#include <tradelayer/uint256_extensions.h>
#include <tradelayer/utilsbitcoin.h>
#include <tradelayer/varint.h>
#include <tradelayer/insurancefund.h>

#include <amount.h>
#include <sync.h>
#include <util/time.h>
#include <validation.h>

#include <algorithm>
// #include <arpa/inet.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

using boost::algorithm::token_compress_on;
typedef boost::multiprecision::uint128_t ui128;

using namespace mastercore;
typedef boost::rational<boost::multiprecision::checked_int128_t> rational_t;
typedef boost::multiprecision::cpp_dec_float_100 dec_float;
typedef boost::multiprecision::checked_int128_t int128_t;

std::map<std::string,uint32_t> peggedIssuers;
std::map<uint32_t,std::map<int,oracledata>> oraclePrices;

/** Pending withdrawals **/
std::map<std::string,vector<withdrawalAccepted>> withdrawal_Map;
mutex mReward;
using mastercore::StrToInt64;


//! Empty FutureContractObject && TokenDataByName
FutureContractObject FutureContractObject::Empty;
TokenDataByName TokenDataByName::Empty;

/* Mapping of a transaction type to a textual description. */
std::string mastercore::strTransactionType(unsigned int txType)
{
     switch (txType) {
        case MSC_TYPE_SIMPLE_SEND: return "Simple Send";
				case MSC_TYPE_SEND_MANY: return "Send Many";
				case MSC_TYPE_RESTRICTED_SEND: return "Restricted Send";
				case MSC_TYPE_SEND_ALL: return "Send All";
				case MSC_TYPE_SEND_VESTING: return "Send Vesting Tokens";
				case MSC_TYPE_SAVINGS_MARK: return"Savings";
				case MSC_TYPE_SAVINGS_COMPROMISED: return "Savings COMPROMISED";
				case MSC_TYPE_CREATE_PROPERTY_FIXED: return "Create Property - Fixed";
				case MSC_TYPE_CREATE_PROPERTY_MANUAL: return "Create Property - Manual";
				case MSC_TYPE_GRANT_PROPERTY_TOKENS: return "Grant Property Tokens";
				case MSC_TYPE_REVOKE_PROPERTY_TOKENS: return"Revoke Property Tokens";
				case MSC_TYPE_CHANGE_ISSUER_ADDRESS: return "Change Issuer Address";
				case TL_MESSAGE_TYPE_ALERT: return "ALERT";
				case TL_MESSAGE_TYPE_DEACTIVATION: return "Feature Deactivation";
				case TL_MESSAGE_TYPE_ACTIVATION: return "Feature Activation";
				case MSC_TYPE_METADEX_TRADE: return "Metadex Order";
				case MSC_TYPE_CONTRACTDEX_TRADE: return "Future Contract";
				case MSC_TYPE_CONTRACTDEX_CANCEL_ECOSYSTEM: return "ContractDex cancel-ecosystem";
				case MSC_TYPE_CREATE_CONTRACT: return "Create Native Contract";
				case MSC_TYPE_PEGGED_CURRENCY: return "Pegged Currency";
				case MSC_TYPE_REDEMPTION_PEGGED: return "Redemption Pegged Currency";
				case MSC_TYPE_SEND_PEGGED_CURRENCY: return "Send Pegged Currency";
				case MSC_TYPE_CONTRACTDEX_CLOSE_POSITION: return "Close Position";
				case MSC_TYPE_CONTRACTDEX_CANCEL_ORDERS_BY_BLOCK: return "Cancel Orders by Block";
				case MSC_TYPE_DEX_SELL_OFFER: return "DEx Sell Offer";
				case MSC_TYPE_DEX_BUY_OFFER: return "DEx Buy Offer";
				case MSC_TYPE_ACCEPT_OFFER_BTC: return "DEx Accept Offer LTC";
				case MSC_TYPE_CHANGE_ORACLE_REF: return "Oracle Change Reference";
				case MSC_TYPE_SET_ORACLE: return "Oracle Set Address";
				case MSC_TYPE_ORACLE_BACKUP: return "Oracle Backup";
				case MSC_TYPE_CLOSE_ORACLE: return "Oracle Close";
				case MSC_TYPE_COMMIT_CHANNEL: return "Channel Commit";
				case MSC_TYPE_WITHDRAWAL_FROM_CHANNEL: return "Channel Withdrawal";
				case MSC_TYPE_TRANSFER: return "Channel Transfer";
				case MSC_TYPE_INSTANT_TRADE: return "Channel Instant Trade";
				case MSC_TYPE_CONTRACT_INSTANT: return "Channel Contract Instant Trade";
				case MSC_TYPE_NEW_ID_REGISTRATION: return "New Id Registration";
				case MSC_TYPE_UPDATE_ID_REGISTRATION: return "Update Id Registration";
				case MSC_TYPE_DEX_PAYMENT: return "DEx payment";
				case MSC_TYPE_ATTESTATION: return "KYC Attestation";
				case MSC_TYPE_REVOKE_ATTESTATION: return "KYC Revoke Attestation";
				case MSC_TYPE_CREATE_ORACLE_CONTRACT: return "Create Oracle Contract";
				case MSC_TYPE_METADEX_CANCEL_ALL: return "Cancel all MetaDEx orders";
				case MSC_TYPE_CONTRACTDEX_CANCEL: return "Cancel specific contract order";
				case MSC_TYPE_INSTANT_LTC_TRADE: return "Instant LTC for Tokens trade";
				case MSC_TYPE_METADEX_CANCEL: return "Cancel specific MetaDEx order";
				case MSC_TYPE_METADEX_CANCEL_BY_PRICE: return "MetaDEx cancel-price";
				case MSC_TYPE_METADEX_CANCEL_BY_PAIR: return "MetaDEx cancel-by-pair";
				case MSC_TYPE_SUBMIT_NODE_ADDRESS: return "Submit Node Reward Address";
				case MSC_TYPE_CLAIM_NODE_REWARD: return "Claim Node Reward";
				case MSC_TYPE_CLOSE_CHANNEL: return "Close Channel";
				case MSC_TYPE_SEND_DONATION: return "Send Donation";
     }

		 return "* unknown type *";
}
/** Helper to convert class number to string. */
static std::string intToClass(int encodingClass)
{
    switch (encodingClass) {
        case TL_CLASS_D:
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

  switch (type)
  {
      case MSC_TYPE_SIMPLE_SEND:
          return interpret_SimpleSend();

    case MSC_TYPE_SEND_MANY:
          return interpret_SendMany();

      case MSC_TYPE_SEND_ALL:
          return interpret_SendAll();

      case MSC_TYPE_SEND_VESTING:
          return interpret_SendVestingTokens();

      case MSC_TYPE_CREATE_PROPERTY_FIXED:
          return interpret_CreatePropertyFixed();

      case MSC_TYPE_CREATE_PROPERTY_MANUAL:
          return interpret_CreatePropertyManaged();

      case MSC_TYPE_GRANT_PROPERTY_TOKENS:
         return interpret_GrantTokens();

      case MSC_TYPE_REVOKE_PROPERTY_TOKENS:
          return interpret_RevokeTokens();

      case MSC_TYPE_CHANGE_ISSUER_ADDRESS:
          return interpret_ChangeIssuer();

      case TL_MESSAGE_TYPE_DEACTIVATION:
          return interpret_Deactivation();

      case TL_MESSAGE_TYPE_ACTIVATION:
          return interpret_Activation();

      case TL_MESSAGE_TYPE_ALERT:
          return interpret_Alert();

      case MSC_TYPE_METADEX_TRADE:
          return interpret_MetaDExTrade();

      case MSC_TYPE_METADEX_CANCEL_ALL:
          return interpret_MetaDExCancelAll();

      case MSC_TYPE_CREATE_CONTRACT:
          return interpret_CreateContractDex();

      case MSC_TYPE_CREATE_ORACLE_CONTRACT:
          return interpret_CreateOracleContract();

      case MSC_TYPE_CONTRACTDEX_TRADE:
          return interpret_ContractDexTrade();

      case MSC_TYPE_CONTRACTDEX_CANCEL_ECOSYSTEM:
          return interpret_ContractDexCancelEcosystem();

      case MSC_TYPE_CONTRACTDEX_CANCEL:
          return interpret_ContractDExCancel();

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

      case MSC_TYPE_DEX_SELL_OFFER:
          return interpret_DExSell();

      case MSC_TYPE_DEX_BUY_OFFER:
          return interpret_DExBuy();

      case MSC_TYPE_ACCEPT_OFFER_BTC:
          return interpret_AcceptOfferBTC();

      case MSC_TYPE_CHANGE_ORACLE_REF:
          return interpret_Change_OracleAdm();

      case MSC_TYPE_SET_ORACLE:
          return interpret_Set_Oracle();

      case MSC_TYPE_ORACLE_BACKUP:
          return interpret_OracleBackup();

      case MSC_TYPE_CLOSE_ORACLE:
          return interpret_CloseOracle();

      case MSC_TYPE_COMMIT_CHANNEL:
          return interpret_CommitChannel();

      case MSC_TYPE_WITHDRAWAL_FROM_CHANNEL:
          return interpret_Withdrawal_FromChannel();

      case MSC_TYPE_INSTANT_TRADE:
          return interpret_Instant_Trade();

      case MSC_TYPE_TRANSFER:
          return interpret_Transfer();

      case MSC_TYPE_CONTRACT_INSTANT:
          return interpret_Contract_Instant();

      case MSC_TYPE_NEW_ID_REGISTRATION:
          return interpret_New_Id_Registration();

      case MSC_TYPE_UPDATE_ID_REGISTRATION:
          return interpret_Update_Id_Registration();

      case MSC_TYPE_DEX_PAYMENT:
          return interpret_DEx_Payment();

      case MSC_TYPE_ATTESTATION:
          return interpret_Attestation();

      case MSC_TYPE_REVOKE_ATTESTATION:
          return interpret_Revoke_Attestation();

      case MSC_TYPE_INSTANT_LTC_TRADE:
          return interpret_Instant_LTC_Trade();

      case MSC_TYPE_METADEX_CANCEL:
          return interpret_MetaDExCancel();

      case MSC_TYPE_METADEX_CANCEL_BY_PAIR:
          return interpret_MetaDExCancel_ByPair();

      case MSC_TYPE_METADEX_CANCEL_BY_PRICE:
          return interpret_MetaDExCancel_ByPrice();

      case MSC_TYPE_CLOSE_CHANNEL:
          return interpret_SimpleSend();

			case MSC_TYPE_SUBMIT_NODE_ADDRESS:
			    return interpret_SubmitNodeAddr();

			case MSC_TYPE_CLAIM_NODE_REWARD:
          return interpret_ClaimNodeReward();

      case MSC_TYPE_SEND_DONATION:
          return interpret_SendDonation();
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
        type = (unsigned int) DecompressInteger(vecTypeBytes);
				PrintToLog("%s(): type: %d\n",__func__, type);
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

// /** Tx 1 */
bool CMPTransaction::interpret_SendMany()
{
    int i = 0;

    auto vecVersionBytes = GetNextVarIntBytes(i);
    auto vecTypeBytes = GetNextVarIntBytes(i);
    auto vecPropIdBytes = GetNextVarIntBytes(i);

    if (type != 1 || vecPropIdBytes.empty()) return false;
    property = DecompressInteger(vecPropIdBytes);

		do
    {
        auto vecKyc = GetNextVarIntBytes(i);
        if (!vecKyc.empty())
        {
            auto n = DecompressInteger(vecKyc);
						if (n == 0) break;
            values.push_back(n);
						PrintToLog("\t value: %d\n", n);
        }

    } while(i < pkt_size);

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t        property: %d (%s)\n", property, strMPProperty(property));
        PrintToLog("\t           total: %s\n", FormatMP(property, getAmountTotal()));
    }

    return true;
}

/** Tx 5 */
bool CMPTransaction::interpret_SendVestingTokens()
{
  int i = 0;

  std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecAmountBytes = GetNextVarIntBytes(i);

  if (!vecAmountBytes.empty()) {
    nValue = DecompressInteger(vecAmountBytes);
    nNewValue = nValue;
  } else return false;

  if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
    PrintToLog("\t        property: %d (%s)\n", TL_PROPERTY_VESTING, strMPProperty(property));
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

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t       inside interpret \n");
    }

    return true;
}

/** Tx 50 */
bool CMPTransaction::interpret_CreatePropertyFixed()
{
    int i = 0;

    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
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

    do
    {
        std::vector<uint8_t> vecKyc = GetNextVarIntBytes(i);
        if (!vecKyc.empty())
        {
            const int64_t num = static_cast<int64_t>(DecompressInteger(vecKyc));
            kyc_Ids.push_back(num);
        }

    } while(i < pkt_size);

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
        PrintToLog("\t   property type: %d (%s)\n", prop_type, strPropertyType(prop_type));
        PrintToLog("\tprev property id: %d\n", prev_prop_id);
        PrintToLog("\t            name: %s\n", name);
        PrintToLog("\t             url: %s\n", url);
        PrintToLog("\t            data: %s\n", data);
        PrintToLog("\t           value: %s\n", FormatByType(nValue, prop_type));
    }

    return true;
}

/** Tx 54 */
bool CMPTransaction::interpret_CreatePropertyManaged()
{
    int i = 0;

    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
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

    do
    {
        std::vector<uint8_t> vecKyc = GetNextVarIntBytes(i);
        if (!vecKyc.empty())
        {
            const int64_t num = static_cast<int64_t>(DecompressInteger(vecKyc));
            kyc_Ids.push_back(num);
        }

    } while(i < pkt_size);

    if (!vecPropTypeBytes.empty()) {
        prop_type = DecompressInteger(vecPropTypeBytes);
    } else return false;

    if (!vecPrevPropIdBytes.empty()) {
        prev_prop_id = DecompressInteger(vecPrevPropIdBytes);
    } else return false;

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
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
bool CMPTransaction::interpret_DExSell()
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

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
    {
        PrintToLog("\t version: %d\n", version);
        PrintToLog("\t messageType: %d\n",type);
        PrintToLog("\t property: %d\n", propertyId);
        PrintToLog("\t amount : %d\n", nValue);
        PrintToLog("\t amount desired : %d\n", amountDesired);
        PrintToLog("\t block limit : %d\n", timeLimit);
        PrintToLog("\t min fees : %d\n", minFee);
        PrintToLog("\t subaction : %d\n", subAction);
    }

    return true;
}

/*Tx 21*/
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

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
    {
        PrintToLog("\t version: %d\n", version);
        PrintToLog("\t messageType: %d\n",type);
        PrintToLog("\t property: %d\n", propertyId);
        PrintToLog("\t amount : %d\n", nValue);
        PrintToLog("\t price : %d\n", effective_price);
        PrintToLog("\t block limit : %d\n", timeLimit);
        PrintToLog("\t min fees : %d\n", minFee);
        PrintToLog("\t subaction : %d\n", subAction);
    }

    return true;
}

bool CMPTransaction::interpret_AcceptOfferBTC()
{
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

  if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
  {
      PrintToLog("\t version: %d\n", version);
      PrintToLog("\t messageType: %d\n",type);
      PrintToLog("\t property: %d\n", propertyId);
  }

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

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
    {
        PrintToLog("\t version: %d\n", version);
        PrintToLog("\t messageType: %d\n",type);
        PrintToLog("\t property: %d\n", property);
        PrintToLog("\t amount : %d\n", amount_forsale);
        PrintToLog("\t property desired : %d\n", desired_property);
        PrintToLog("\t amount desired : %d\n", desired_value);
    }

    return true;
}

/** Tx  40*/
bool CMPTransaction::interpret_CreateContractDex()
{
  int i = 0;
  std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecNum = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecDen = GetNextVarIntBytes(i);
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
  std::vector<uint8_t> vecInverse = GetNextVarIntBytes(i);

  do
  {
      std::vector<uint8_t> vecKyc = GetNextVarIntBytes(i);
      if (!vecKyc.empty())
      {
          const int64_t num = static_cast<int64_t>(DecompressInteger(vecKyc));
          kyc_Ids.push_back(num);
      }

  } while(i < pkt_size);

  if (!vecVersionBytes.empty()) {
    version = DecompressInteger(vecVersionBytes);
  } else return false;

  if (!vecTypeBytes.empty()) {
    type = DecompressInteger(vecTypeBytes);
  } else return false;

  if (!vecNum.empty()) {
    numerator = DecompressInteger(vecNum);
  } else return false;

  if (!vecDen.empty()) {
    denominator = DecompressInteger(vecDen);
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

  if (!vecInverse.empty()) {
      uint8_t inverse = DecompressInteger(vecInverse);
      if (inverse == 0) inverse_quoted = false;

  } else return false;

  (blocks_until_expiration == 0) ? prop_type = ALL_PROPERTY_TYPE_PERPETUAL_CONTRACTS : prop_type = ALL_PROPERTY_TYPE_NATIVE_CONTRACT;

  if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
  {
      PrintToLog("\t version: %d\n", version);
      PrintToLog("\t messageType: %d\n",type);
      PrintToLog("\t numerator: %d\n", numerator);
      PrintToLog("\t denominator: %d\n", denominator);
      PrintToLog("\t blocks until expiration : %d\n", blocks_until_expiration);
      PrintToLog("\t notional size : %d\n", notional_size);
      PrintToLog("\t collateral currency: %d\n", collateral_currency);
      PrintToLog("\t margin requirement: %d\n", margin_requirement);
      PrintToLog("\t name: %s\n", name);
      PrintToLog("\t prop_type: %d\n", prop_type);
      PrintToLog("\t inverse quoted: %d\n", inverse_quoted);
  }

  return true;
}

/**Tx 29 */
bool CMPTransaction::interpret_ContractDexTrade()
{
    int i = 0;
    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);

    const char* p = i + (char*) &pkt;
    std::vector<std::string> spstr;
    for (int j = 0; j < 1; j++)
    {
        spstr.push_back(std::string(p));
        p += spstr.back().size() + 1;
    }

    if (isOverrun(p))
    {
        PrintToLog("%s(): rejected: malformed string value(s)\n", __func__);
        return false;
    }

    int j = 0;
    memcpy(name_traded, spstr[j].c_str(), std::min(spstr[j].length(), sizeof(name_traded)-1)); j++;
    i = i + strlen(name_traded) + 1;

    std::vector<uint8_t> vecAmountForSaleBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecEffectivePriceBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTradingActionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecLeverage = GetNextVarIntBytes(i);

    if (!vecTypeBytes.empty()) {
      type = DecompressInteger(vecTypeBytes);
    } else return false;

    if (!vecVersionBytes.empty()) {
      version = DecompressInteger(vecVersionBytes);
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

    if (!vecLeverage.empty()) {
      leverage = (int64_t) DecompressInteger(vecLeverage);
    } else return false;

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
    {
        PrintToLog("\t leverage: %d\n", leverage);
        PrintToLog("\t messageType: %d\n",type);
        PrintToLog("\t contractName: %s\n", name_traded);
        PrintToLog("\t amount of contracts : %d\n", amount);
        PrintToLog("\t effective price : %d\n", effective_price);
        PrintToLog("\t trading action : %d\n", trading_action);
     }

     return true;
}

/** Tx 31 */
bool CMPTransaction::interpret_ContractDExCancel()
{
    int i = 0;
    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);

    const char* p = i + (char*) &pkt;
    std::vector<std::string> spstr;
    for (int j = 0; j < 1; j++)
    {
        spstr.push_back(std::string(p));
        p += spstr.back().size() + 1;
    }

    if (isOverrun(p))
    {
        PrintToLog("%s(): rejected: malformed string value(s)\n", __func__);
        return false;
    }

    int j = 0;
    memcpy(hash, spstr[j].c_str(), std::min(spstr[j].length(), sizeof(hash)-1)); j++;
    i = i + strlen(hash) + 1;


    if (!vecTypeBytes.empty()) {
        type = DecompressInteger(vecTypeBytes);
    } else return false;

    if (!vecVersionBytes.empty()) {
        version = DecompressInteger(vecVersionBytes);
    } else return false;


    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
    {
        PrintToLog("\t version: %d\n", version);
        PrintToLog("\t messageType: %d\n",type);
        PrintToLog("\t hash: %s\n", hash);
    }

    return true;
}

/** Tx 32 */
bool CMPTransaction::interpret_ContractDexCancelEcosystem()
{
  int i = 0;
  std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecContractIdBytes = GetNextVarIntBytes(i);

  if (!vecTypeBytes.empty()) {
    type = DecompressInteger(vecTypeBytes);
  } else return false;

  if (!vecVersionBytes.empty()) {
    version = DecompressInteger(vecVersionBytes);
  } else return false;

  if (!vecContractIdBytes.empty()) {
    contractId = DecompressInteger(vecContractIdBytes);
  } else return false;


  if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
  {
     PrintToLog("\t version: %d\n", version);
     PrintToLog("\t messageType: %d\n",type);
     PrintToLog("\t contractId: %d\n", contractId);
  }

  return true;
}

  /** Tx 33 */
bool CMPTransaction::interpret_ContractDexClosePosition()
{
    int i = 0;

    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecContractIdBytes = GetNextVarIntBytes(i);

    if (!vecTypeBytes.empty()) {
        type = DecompressInteger(vecTypeBytes);
    } else return false;

    if (!vecVersionBytes.empty()) {
        version = DecompressInteger(vecVersionBytes);
    } else return false;

    if (!vecContractIdBytes.empty()) {
        contractId = DecompressInteger(vecContractIdBytes);
    } else return false;

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
    {
        PrintToLog("\t version: %d\n", version);
        PrintToLog("\t messageType: %d\n",type);
        PrintToLog("\t contractId: %d\n", contractId);
    }

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
    } else return false;

    if (!vecIdxBytes.empty()) {
        tx_idx = DecompressInteger(vecIdxBytes);
    } else return false;

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
    {
      PrintToLog("\t version: %d\n", version);
      PrintToLog("\t messageType: %d\n",type);
      PrintToLog("\t block: %d\n", block);
      PrintToLog("\t tx_idx: %d\n", tx_idx);
    }

    return true;
}

/** Tx 35 */
bool CMPTransaction::interpret_MetaDExCancel()
{
    int i = 0;
    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);

    const char* p = i + (char*) &pkt;
    std::vector<std::string> spstr;
    for (int j = 0; j < 1; j++)
    {
        spstr.push_back(std::string(p));
        p += spstr.back().size() + 1;
    }

    if (isOverrun(p))
    {
        PrintToLog("%s(): rejected: malformed string value(s)\n", __func__);
        return false;
    }

    int j = 0;
    memcpy(hash, spstr[j].c_str(), std::min(spstr[j].length(), sizeof(hash)-1)); j++;
    i = i + strlen(hash) + 1;


    if (!vecTypeBytes.empty()) {
        type = DecompressInteger(vecTypeBytes);
    } else return false;

    if (!vecVersionBytes.empty()) {
        version = DecompressInteger(vecVersionBytes);
    } else return false;


    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
    {
        PrintToLog("\t version: %d\n", version);
        PrintToLog("\t messageType: %d\n",type);
        PrintToLog("\t hash: %s\n", hash);
    }

    return true;
}

/** Tx 36 */
bool CMPTransaction::interpret_MetaDExCancel_ByPair()
{
    int i = 0;
    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecPropertyIdForSale = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecPropertyIdDesired = GetNextVarIntBytes(i);

    if (!vecTypeBytes.empty()) {
        type = DecompressInteger(vecTypeBytes);
    } else return false;

    if (!vecVersionBytes.empty()) {
        version = DecompressInteger(vecVersionBytes);
    } else return false;

    if (!vecPropertyIdForSale.empty()) {
        propertyId = DecompressInteger(vecPropertyIdForSale);
    } else return false;

    if (!vecPropertyIdDesired.empty()) {
        desired_property = DecompressInteger(vecPropertyIdDesired);
    } else return false;


    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
    {
        PrintToLog("\t version: %d\n", version);
        PrintToLog("\t messageType: %d\n",type);
        PrintToLog("\t propertyIdForSale: %d\n", propertyId);
        PrintToLog("\t propertyIdDesired: %d\n", desired_property);
    }

    return true;
}

/** Tx 37 */
bool CMPTransaction::interpret_MetaDExCancel_ByPrice()
{
    int i = 0;
    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecPropertyIdForSale = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecAmountForSale = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecPropertyIdDesired = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecAmountDesired = GetNextVarIntBytes(i);

    if (!vecTypeBytes.empty()) {
        type = DecompressInteger(vecTypeBytes);
    } else return false;

    if (!vecVersionBytes.empty()) {
        version = DecompressInteger(vecVersionBytes);
    } else return false;

    if (!vecPropertyIdForSale.empty()) {
        propertyId = DecompressInteger(vecPropertyIdForSale);
    } else return false;

    if (!vecAmountForSale.empty()) {
        amount_forsale = DecompressInteger(vecAmountForSale);
    } else return false;

    if (!vecAmountDesired.empty()) {
        desired_value = DecompressInteger(vecAmountDesired);
    } else return false;

    if (!vecPropertyIdDesired.empty()) {
        desired_property = DecompressInteger(vecPropertyIdDesired);
    } else return false;

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
    {
        PrintToLog("\t version: %d\n", version);
        PrintToLog("\t messageType: %d\n",type);
        PrintToLog("\t propertyIdForSale: %d\n", propertyId);
        PrintToLog("\t amountForSale: %d\n", amount_forsale);
        PrintToLog("\t propertyIdDesired: %d\n", desired_property);
        PrintToLog("\t amountDesired: %d\n", desired_value);
    }

    return true;
}

  /** Tx 101 */
bool CMPTransaction::interpret_CreatePeggedCurrency()
{
    int i = 0;

    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecPropTypeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecPrevPropIdBytes = GetNextVarIntBytes(i);
    const char* p = i + (char*) &pkt;
    std::vector<std::string> spstr;
    spstr.push_back(std::string(p));
    p += spstr.back().size() + 1;

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

    prop_type = ALL_PROPERTY_TYPE_PEGGEDS;

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
    {
        PrintToLog("\t version: %d\n", version);
        PrintToLog("\t messageType: %d\n",type);
        PrintToLog("\t property type: %d\n",prop_type);
        PrintToLog("\t prev prop id: %d\n",prev_prop_id);
        PrintToLog("\t contractId: %d\n", contractId);
        PrintToLog("\t propertyId: %d\n", propertyId);
        PrintToLog("\t amount of pegged currency : %d\n", amount);
        PrintToLog("\t name : %s\n", name);
    }

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

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
    {
        PrintToLog("\t version: %d\n", version);
        PrintToLog("\t messageType: %d\n",type);
        PrintToLog("\t propertyId: %d\n", propertyId);
        PrintToLog("\t amount of pegged currency : %d\n", amount);
    }

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

  if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
  {
      PrintToLog("\t version: %d\n", version);
      PrintToLog("\t messageType: %d\n",type);
      PrintToLog("\t propertyId: %d\n", propertyId);
      PrintToLog("\t contractId: %d\n", contractId);
      PrintToLog("\t amount of pegged currency : %d\n", amount);
  }

  return true;
}

/** Tx  103*/
bool CMPTransaction::interpret_CreateOracleContract()
{
  int i = 0;

  std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
  const char* p = i + (char*) &pkt;
  std::vector<std::string> spstr;
  spstr.push_back(std::string(p));
  p += spstr.back().size() + 1;

  if (isOverrun(p)) {
    PrintToLog("%s(): rejected: malformed string value(s)\n", __func__);
    return false;
  }

  int j = 0;
  memcpy(name, spstr[j].c_str(), std::min(spstr[j].length(), sizeof(name)-1)); j++;
  i = i + strlen(name) + 1; // data sizes + 2 null terminators

  std::vector<uint8_t> vecBlocksUntilExpiration = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecNotionalSize = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecCollateralCurrency = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecMarginRequirement = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecInverse = GetNextVarIntBytes(i);


  if (!vecVersionBytes.empty()) {
    version = DecompressInteger(vecVersionBytes);
  } else return false;

  if (!vecTypeBytes.empty()) {
    type = DecompressInteger(vecTypeBytes);
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


  (blocks_until_expiration == 0) ? prop_type = ALL_PROPERTY_TYPE_PERPETUAL_ORACLE : prop_type = ALL_PROPERTY_TYPE_ORACLE_CONTRACT;

  if (!vecInverse.empty()) {
    uint8_t inverse = DecompressInteger(vecInverse);
    if(inverse == 0) inverse_quoted = false;

  } else return false;


  do
  {
      std::vector<uint8_t> vecKyc = GetNextVarIntBytes(i);
      if (!vecKyc.empty())
      {
          const int64_t num = static_cast<int64_t>(DecompressInteger(vecKyc));
          kyc_Ids.push_back(num);
      }

  } while(i < pkt_size);


  if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
  {
      PrintToLog("\t version: %d\n", version);
      PrintToLog("\t messageType: %d\n",type);
      PrintToLog("\t blocks until expiration : %d\n", blocks_until_expiration);
      PrintToLog("\t notional size : %d\n", notional_size);
      PrintToLog("\t collateral currency: %d\n", collateral_currency);
      PrintToLog("\t margin requirement: %d\n", margin_requirement);
      PrintToLog("\t name: %s\n", name);
      PrintToLog("\t oracleAddress: %s\n", sender);
      PrintToLog("\t backupAddress: %s\n", receiver);
      PrintToLog("\t prop_type: %d\n", prop_type);
      PrintToLog("\t inverse quoted: %d\n", inverse_quoted);

  }

  return true;
}

/** Tx 104 */
bool CMPTransaction::interpret_Change_OracleAdm()
{
    int i = 0;

    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecContIdBytes = GetNextVarIntBytes(i);

    if (!vecContIdBytes.empty()) {
        contractId = DecompressInteger(vecContIdBytes);
    } else return false;

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
    {
        PrintToLog("\t version: %d\n", version);
        PrintToLog("\t messageType: %d\n",type);
        PrintToLog("\t contractId: %d\n", contractId);
    }

    return true;
}

/** Tx 105 */
bool CMPTransaction::interpret_Set_Oracle()
{
    int i = 0;

    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecContIdBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecHighBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecLowBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecCloseBytes = GetNextVarIntBytes(i);

    if (!vecContIdBytes.empty()) {
        contractId = DecompressInteger(vecContIdBytes);
    } else return false;

    if (!vecHighBytes.empty()) {
        oracle_high = DecompressInteger(vecHighBytes);
    } else return false;

    if (!vecLowBytes.empty()) {
        oracle_low = DecompressInteger(vecLowBytes);
    } else return false;

    if (!vecLowBytes.empty()) {
        oracle_close = DecompressInteger(vecCloseBytes);
    } else return false;

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
    {
        PrintToLog("\t version: %d\n", version);
        PrintToLog("\t oracle high price: %d\n",oracle_high);
        PrintToLog("\t oracle low price: %d\n",oracle_low);
        PrintToLog("\t oracle close price: %d\n",oracle_close);
        PrintToLog("\t propertyId: %d\n", propertyId);
    }

    return true;
}

/** Tx 106 */
bool CMPTransaction::interpret_OracleBackup()
{
    int i = 0;

    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecContIdBytes = GetNextVarIntBytes(i);

    if (!vecContIdBytes.empty()) {
        contractId = DecompressInteger(vecContIdBytes);
    } else return false;

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
    {
        PrintToLog("\t version: %d\n", version);
        PrintToLog("\t contractId: %d\n", contractId);
    }

    return true;
}

/** Tx 107 */
bool CMPTransaction::interpret_CloseOracle()
{
    int i = 0;

    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecContIdBytes = GetNextVarIntBytes(i);

    if (!vecContIdBytes.empty()) {
        contractId = DecompressInteger(vecContIdBytes);
    } else return false;

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
    {
        PrintToLog("\t version: %d\n", version);
        PrintToLog("\t contractId: %d\n", contractId);
    }

    return true;
}

/** Tx 108 */
bool CMPTransaction::interpret_CommitChannel()
{
    int i = 0;

    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);

    std::vector<uint8_t> vecContIdBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecAmountBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecVoutBytes = GetNextVarIntBytes(i);

    if (!vecContIdBytes.empty()) {
        propertyId = DecompressInteger(vecContIdBytes);
    } else return false;

    if (!vecAmountBytes.empty()) {
        amount_commited = DecompressInteger(vecAmountBytes);
    } else return false;


    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
    {
        PrintToLog("\t channelAddress: %s\n", receiver);
        PrintToLog("\t version: %d\n", version);
        PrintToLog("\t propertyId: %d\n", propertyId);
        PrintToLog("\t amount commited: %d\n", amount_commited);
    }

    return true;
}

/** Tx 109 */
bool CMPTransaction::interpret_Withdrawal_FromChannel()
{
    int i = 0;

    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);

    std::vector<uint8_t> vecContIdBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecAmountBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecVoutBytes = GetNextVarIntBytes(i);

    if (!vecContIdBytes.empty()) {
        propertyId = DecompressInteger(vecContIdBytes);
    } else return false;

    if (!vecAmountBytes.empty()) {
        amount_to_withdraw = DecompressInteger(vecAmountBytes);
    } else return false;

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
    {
        PrintToLog("\t channelAddress: %s\n", receiver);
        PrintToLog("\t version: %d\n", version);
        PrintToLog("\t propertyId: %d\n", propertyId);
        PrintToLog("\t amount to withdrawal: %d\n", amount_to_withdraw);
    }

    return true;
}

/** Tx 110 */
bool CMPTransaction::interpret_Instant_Trade()
{
  int i = 0;

  std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecPropertyIdForSaleBytes = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecAmountForSaleBytes = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecBlock = GetNextVarIntBytes(i);
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
  } else return false;

  if (!vecBlock.empty()) {
      block_forexpiry = DecompressInteger(vecBlock);
  } else return false;

  if (!vecPropertyIdDesiredBytes.empty()) {
      desired_property = DecompressInteger(vecPropertyIdDesiredBytes);
  } else return false;

  if (!vecAmountDesiredBytes.empty()) {
      desired_value = DecompressInteger(vecAmountDesiredBytes);
  } else return false;

  if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly || true)
  {
      PrintToLog("\t version: %d\n", version);
      PrintToLog("\t messageType: %d\n",type);
      PrintToLog("\t property: %d\n", property);
      PrintToLog("\t amount : %d\n", amount_forsale);
      PrintToLog("\t blockheight_expiry : %d\n", block_forexpiry);
      PrintToLog("\t property desired : %d\n", desired_property);
      PrintToLog("\t amount desired : %d\n", desired_value);
      PrintToLog("\t sender : %s\n", sender);
      PrintToLog("\t receiver : %s\n", receiver);
  }

  return true;
}

/** Tx 111 */
bool CMPTransaction::interpret_Update_PNL()
{
  int i = 0;

  std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecPropertyId = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecAmount = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecBlock = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecVoutBef = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecVoutPay = GetNextVarIntBytes(i);

  if (!vecTypeBytes.empty()) {
      type = DecompressInteger(vecTypeBytes);
  } else return false;

  if (!vecVersionBytes.empty()) {
      version = DecompressInteger(vecVersionBytes);
  } else return false;

  if (!vecPropertyId.empty()) {
      property = DecompressInteger(vecPropertyId);
  } else return false;

  if (!vecAmount.empty()) {
      pnl_amount = DecompressInteger(vecAmount);
  } else return false;


  if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
  {
      PrintToLog("\t version: %d\n", version);
      PrintToLog("\t messageType: %d\n",type);
      PrintToLog("\t property: %d\n", property);
      PrintToLog("\t amount : %d\n", amount_forsale);
      PrintToLog("\t property desired : %d\n", desired_property);
      PrintToLog("\t amount desired : %d\n", desired_value);
  }

  return true;
}

/** Tx 112 */
bool CMPTransaction::interpret_Transfer()
{
  int i = 0;

  std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecOptionBytes = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecPropertyIdBytes = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecAmountBytes = GetNextVarIntBytes(i);

  if (!vecTypeBytes.empty()) {
      type = DecompressInteger(vecTypeBytes);
  } else return false;

  if (!vecVersionBytes.empty()) {
      version = DecompressInteger(vecVersionBytes);
  } else return false;

  if (!vecOptionBytes.empty()) {
      address_option = (DecompressInteger(vecOptionBytes) == 0) ? false : true;
  } else return false;

  if (!vecPropertyIdBytes.empty()) {
      propertyId = DecompressInteger(vecPropertyIdBytes);
  } else return false;

  if (!vecAmountBytes.empty()) {
      amount_transfered = DecompressInteger(vecAmountBytes);
  } else return false;

  if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
  {
      PrintToLog("\t version: %d\n", version);
      PrintToLog("\t messageType: %d\n",type);
      PrintToLog("\t address option: %d\n", address_option);
      PrintToLog("\t propertyId: %d\n",propertyId);
      PrintToLog("\t amount transfered: %d\n",amount_transfered);
  }

  return true;
}

/** Tx 113 */
bool CMPTransaction::interpret_Instant_LTC_Trade()
{
  int i = 0;

  std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecPropertyId = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecAmountForSale = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecPrice = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecBlock = GetNextVarIntBytes(i);

  if (!vecTypeBytes.empty()) {
      type = DecompressInteger(vecTypeBytes);
  } else return false;

  if (!vecVersionBytes.empty()) {
      version = DecompressInteger(vecVersionBytes);
  } else return false;

  if (!vecPropertyId.empty()) {
      property = DecompressInteger(vecPropertyId);
  } else return false;

  if (!vecAmountForSale.empty()) {
      amount_forsale = DecompressInteger(vecAmountForSale);
  } else return false;

  if (!vecPrice.empty()) {
      price = DecompressInteger(vecPrice);
  } else return false;

  if (!vecPrice.empty()) {
      block_forexpiry = DecompressInteger(vecBlock);
  } else return false;

  if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
  {
      PrintToLog("\t version: %d\n", version);
      PrintToLog("\t messageType: %d\n",type);
      PrintToLog("\t property: %d\n", property);
      PrintToLog("\t amount : %d\n", amount_forsale);
      PrintToLog("\t price : %d\n", price);
      PrintToLog("\t sender : %s\n", sender);
      PrintToLog("\t receiver : %s\n", receiver);
      PrintToLog("\t expiry : %d\n", block_forexpiry);
  }

  return true;
}
/** Tx 114 */
bool CMPTransaction::interpret_Contract_Instant()
{
  PrintToLog("s%(): inside interpret function!!!!!\n",__func__);
  int i = 0;

  std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecContractId = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecAmount = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecBlock = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecPrice = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecTrading = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecLeverage = GetNextVarIntBytes(i);


  if (!vecTypeBytes.empty()) {
      type = DecompressInteger(vecTypeBytes);
  } else return false;

  if (!vecVersionBytes.empty()) {
      version = DecompressInteger(vecVersionBytes);
  } else return false;

  if (!vecContractId.empty()) {
      contractId = DecompressInteger(vecContractId);
  } else return false;

  if (!vecAmount.empty()) {
      instant_amount = DecompressInteger(vecAmount);
  } else return false;

  if (!vecBlock.empty()) {
      block_forexpiry = DecompressInteger(vecBlock);
  } else return false;

  if (!vecPrice.empty()) {
      price = DecompressInteger(vecPrice);
  } else return false;

  if (!vecTrading.empty()) {
      itrading_action = DecompressInteger(vecTrading);
  } else return false;

  if (!vecLeverage.empty()) {
      ileverage = DecompressInteger(vecLeverage);
  } else return false;

  PrintToLog("s%(): function returning !!!!!\n",__func__);
  if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly || true)
  {
      PrintToLog("\t version: %d\n", version);
      PrintToLog("\t messageType: %d\n",type);
      PrintToLog("\t contractId: %d\n", contractId);
      PrintToLog("\t amount : %d\n", instant_amount);
      PrintToLog("\t blockfor_expiry : %d\n", block_forexpiry);
      PrintToLog("\t price : %d\n", price);
      PrintToLog("\t trading action : %d\n", itrading_action);
      PrintToLog("\t leverage : %d\n", ileverage);
  }

  return true;
}

/** Tx  115*/
bool CMPTransaction::interpret_New_Id_Registration()
{
  int i = 0;

  std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);

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
  memcpy(website, spstr[j].c_str(), std::min(spstr[j].length(), sizeof(website)-1)); j++;
  memcpy(company_name, spstr[j].c_str(), std::min(spstr[j].length(), sizeof(company_name)-1)); j++;
  i = i + strlen(website) + strlen(company_name) + 2;


  if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
  {
      PrintToLog("\t address: %s\n", sender);
      PrintToLog("\t website: %s\n", website);
      PrintToLog("\t company name: %s\n", company_name);

  }

  return true;
}


/** Tx  116*/
bool CMPTransaction::interpret_Update_Id_Registration()
{
  int i = 0;

  std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);


  return true;
}

/** Tx  117*/
bool CMPTransaction::interpret_DEx_Payment()
{
  int i = 0;

  std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);


  if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
  {
      PrintToLog("\t sender: %s\n", sender);
      PrintToLog("\t receiver: %s\n", receiver);
  }

  return true;
}

/** Tx  118*/
bool CMPTransaction::interpret_Attestation()
{
  int i = 0;

  std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);

  const char* p = i + (char*) &pkt;
  std::vector<std::string> spstr;
  spstr.push_back(std::string(p));
  p += spstr.back().size() + 1;

  if (isOverrun(p)) {
    PrintToLog("%s(): rejected: malformed string value(s)\n", __func__);
    return false;
  }

  int j = 0;
  memcpy(hash, spstr[j].c_str(), std::min(spstr[j].length(), sizeof(hash)-1)); j++;
  i = i + strlen(hash) + 1;

  if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
  {
      PrintToLog("%s(): hash: %s\n",__func__, hash);
      PrintToLog("\t sender: %s\n", sender);
      PrintToLog("\t receiver: %s\n", receiver);
  }

  return true;
}

/** Tx  119*/
bool CMPTransaction::interpret_Revoke_Attestation()
{
  int i = 0;

  std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);

  if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
  {
      PrintToLog("%s(): hash: %s\n",__func__, hash);
      PrintToLog("\t sender: %s\n", sender);
      PrintToLog("\t receiver: %s\n", receiver);
  }

  return true;
}

/** Tx 26 */
bool CMPTransaction::interpret_MetaDExCancelAll()
{
    int i = 0;

    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t  %s(): inside interpret \n",__func__);
    }

    return true;
}

/** Tx 120 */
bool CMPTransaction::interpret_Close_Channel()
{
  int i = 0;

  std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
  std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);

  if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
      PrintToLog("\t  %s(): inside interpret \n",__func__);
  }

  return true;
}

/** Tx 121 */
bool CMPTransaction::interpret_SubmitNodeAddr()
{
    int i = 0;
    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);


    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t  %s(): inside interpret \n",__func__);
    }

    return true;
}

/** Tx 122 */
bool CMPTransaction::interpret_ClaimNodeReward()
{
    int i = 0;
    std::vector<uint8_t> vecVersionBytes = GetNextVarIntBytes(i);
    std::vector<uint8_t> vecTypeBytes = GetNextVarIntBytes(i);


    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t  %s(): inside interpret \n",__func__);
    }

    return true;
}

/** Tx 123 */
bool CMPTransaction::interpret_SendDonation()
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

    if (!interpret_Transaction())
    {
        return (PKT_ERROR -2);
    }

    LOCK(cs_tally);

    switch (type)
    {
        case MSC_TYPE_SIMPLE_SEND:
            return logicMath_SimpleSend();

        case MSC_TYPE_SEND_MANY:
            return logicMath_SendMany();

        case MSC_TYPE_SEND_ALL:
            return logicMath_SendAll();

        case MSC_TYPE_SEND_VESTING:
            return logicMath_SendVestingTokens();

        case MSC_TYPE_CREATE_PROPERTY_FIXED:
            return logicMath_CreatePropertyFixed();

        case MSC_TYPE_CREATE_PROPERTY_MANUAL:
            return logicMath_CreatePropertyManaged();

        case MSC_TYPE_GRANT_PROPERTY_TOKENS:
            return logicMath_GrantTokens();

        case MSC_TYPE_REVOKE_PROPERTY_TOKENS:
            return logicMath_RevokeTokens();

        case MSC_TYPE_CHANGE_ISSUER_ADDRESS:
            return logicMath_ChangeIssuer();

        case TL_MESSAGE_TYPE_DEACTIVATION:
            return logicMath_Deactivation();

        case TL_MESSAGE_TYPE_ACTIVATION:
            return logicMath_Activation();

        case TL_MESSAGE_TYPE_ALERT:
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

        case MSC_TYPE_CONTRACTDEX_CANCEL:
            return logicMath_ContractDExCancel();

        case MSC_TYPE_CONTRACTDEX_CANCEL_ORDERS_BY_BLOCK:
            return logicMath_ContractDex_Cancel_Orders_By_Block();

        case MSC_TYPE_DEX_SELL_OFFER:
            return logicMath_DExSell();

        case MSC_TYPE_DEX_BUY_OFFER:
            return logicMath_DExBuy();

        case MSC_TYPE_ACCEPT_OFFER_BTC:
            return logicMath_AcceptOfferBTC();

        case MSC_TYPE_METADEX_TRADE:
            return logicMath_MetaDExTrade();

        case MSC_TYPE_METADEX_CANCEL_ALL:
            return logicMath_MetaDExCancelAll();

        case MSC_TYPE_CREATE_ORACLE_CONTRACT:
            return logicMath_CreateOracleContract();

        case MSC_TYPE_CHANGE_ORACLE_REF:
            return logicMath_Change_OracleAdm();

        case MSC_TYPE_SET_ORACLE:
            return logicMath_Set_Oracle();

        case MSC_TYPE_ORACLE_BACKUP:
            return logicMath_OracleBackup();

        case MSC_TYPE_CLOSE_ORACLE:
            return logicMath_CloseOracle();

        case MSC_TYPE_COMMIT_CHANNEL:
            return logicMath_CommitChannel();

        case MSC_TYPE_WITHDRAWAL_FROM_CHANNEL:
            return logicMath_Withdrawal_FromChannel();

        case MSC_TYPE_INSTANT_TRADE:
            return logicMath_Instant_Trade();

        case MSC_TYPE_TRANSFER:
            return logicMath_Transfer();

        case MSC_TYPE_CONTRACT_INSTANT:
            return logicMath_Contract_Instant();

        case MSC_TYPE_NEW_ID_REGISTRATION:
            return logicMath_New_Id_Registration();

        case MSC_TYPE_UPDATE_ID_REGISTRATION:
            return logicMath_Update_Id_Registration();

        case MSC_TYPE_DEX_PAYMENT:
            return logicMath_DEx_Payment();

        case MSC_TYPE_ATTESTATION:
            return logicMath_Attestation();

        case MSC_TYPE_REVOKE_ATTESTATION:
            return logicMath_Revoke_Attestation();

        case MSC_TYPE_INSTANT_LTC_TRADE:
            return logicMath_Instant_LTC_Trade();

        case MSC_TYPE_METADEX_CANCEL:
            return logicMath_MetaDExCancel();

        case MSC_TYPE_METADEX_CANCEL_BY_PAIR:
            return logicMath_MetaDExCancel_ByPair();

        case MSC_TYPE_CLOSE_CHANNEL:
            return logicMath_Close_Channel();

        case MSC_TYPE_METADEX_CANCEL_BY_PRICE:
            return logicMath_MetaDExCancel_ByPrice();

				case MSC_TYPE_SUBMIT_NODE_ADDRESS:
						return logicMath_SubmitNodeAddr();

				case MSC_TYPE_CLAIM_NODE_REWARD:
						return logicMath_ClaimNodeReward();

        case MSC_TYPE_SEND_DONATION:
            return logicMath_SendDonation();

        default:
            return -1;
    }

    return (PKT_ERROR -100);
}

/** Tx 0 */
int CMPTransaction::logicMath_SimpleSend()
{
    if (!IsTransactionTypeAllowed(block, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_SP -22);
    }

    if (nValue <= 0 || MAX_INT_8_BYTES < nValue) {
        PrintToLog("%s(): rejected: value out of range or zero: %d", __func__, nValue);
        return (PKT_ERROR_SEND -23);
    }

    if (!IsPropertyIdValid(property)) {
        PrintToLog("%s(): rejected: property %d does not exist\n", __func__, property);
        return (PKT_ERROR_SEND -24);
    }

    // if (isPropertyContract(property)) {
    //     PrintToLog("%s(): rejected: property %d should not be a contract\n", __func__, property);
    //     return (PKT_ERROR_SEND -25);
    // }


     if(property == TL_PROPERTY_VESTING){
         PrintToLog("%s(): rejected: property should not be vesting tokens (id = 3)\n", __func__);
         return (PKT_ERROR_SEND -26);
     }

    if(property != ALL)
    {
        int kyc_id;

        if(!t_tradelistdb->checkAttestationReg(sender,kyc_id)){
          PrintToLog("%s(): rejected: kyc ckeck for sender failed\n", __func__);
          return (PKT_ERROR_KYC -10);
        }

        if(!t_tradelistdb->kycPropertyMatch(property,kyc_id)){
          PrintToLog("%s(): rejected: property %d can't be traded with this kyc\n", __func__, property);
          return (PKT_ERROR_KYC -20);
        }


        if(!t_tradelistdb->checkAttestationReg(receiver,kyc_id)){
          PrintToLog("%s(): rejected: kyc ckeck for receiver failed\n", __func__);
          return (PKT_ERROR_KYC -10);
        }

        if(!t_tradelistdb->kycPropertyMatch(property,kyc_id)){
          PrintToLog("%s(): rejected: property %d can't be traded with this kyc\n", __func__, property);
          return (PKT_ERROR_KYC -20);
        }

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

    if (sender == receiver) {
        PrintToLog("%s(): rejected: sender sending tokens to himself\n", __func__);
        return (PKT_ERROR_SEND -26);
    }

    // Special case: if can't find the receiver -- assume send to self!
    if (receiver.empty()) {
        receiver = sender;
    }

    // Move the tokens
    assert(update_tally_map(sender, property, -nValue, BALANCE));
    assert(update_tally_map(receiver, property, nValue, BALANCE));


    return 0;
}

/** Tx 1 */
int CMPTransaction::logicMath_SendMany()
{
    if (!IsTransactionTypeAllowed(block, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_SP -22);
    }

    auto amount = getAmountTotal();
    if (amount <= 0 || MAX_INT_8_BYTES < amount) {
        PrintToLog("%s(): rejected: value out of range or zero: %d", __func__, amount);
        return (PKT_ERROR_SEND -23);
    }

    if (!IsPropertyIdValid(property)) {
        PrintToLog("%s(): rejected: property %d does not exist\n", __func__, property);
        return (PKT_ERROR_SEND -24);
    }

    // if (isPropertyContract(property)) {
    //     PrintToLog("%s(): rejected: property %d should not be a contract\n", __func__, property);
    //     return (PKT_ERROR_SEND -25);
    // }

     if(property == TL_PROPERTY_VESTING){
         PrintToLog("%s(): rejected: property should not be vesting tokens (id = 3)\n", __func__);
         return (PKT_ERROR_SEND -26);
     }

    if(property != ALL)
    {
        int kyc_id;

        if(!t_tradelistdb->checkAttestationReg(sender,kyc_id)){
          PrintToLog("%s(): rejected: kyc ckeck for sender failed\n", __func__);
          return (PKT_ERROR_KYC -10);
        }

        if(!t_tradelistdb->kycPropertyMatch(property,kyc_id)){
          PrintToLog("%s(): rejected: property %d can't be traded with this kyc\n", __func__, property);
          return (PKT_ERROR_KYC -20);
        }

        // TODO: recipients ?!
				for (const auto& addr : recipients)
				{
				    int kyc_id;
					  if(!t_tradelistdb->checkAttestationReg(addr,kyc_id)){
					      PrintToLog("%s(): rejected: kyc ckeck for receiver failed\n", __func__);
					      return (PKT_ERROR_KYC -10);
				  	}

					  if(!t_tradelistdb->kycPropertyMatch(property,kyc_id)){
						    PrintToLog("%s(): rejected: property %d can't be traded with this kyc\n", __func__, property);
						    return (PKT_ERROR_KYC -20);
					  }
				}



    }

    int64_t nBalance = getMPbalance(sender, property, BALANCE);
    if (nBalance < (int64_t)amount) {
        PrintToLog("%s(): rejected: sender %s has insufficient balance of property %d [%s < %s]\n",
                __func__,
                sender,
                property,
                FormatMP(property, nBalance),
                FormatMP(property, nValue));
        return (PKT_ERROR_SEND -25);
    }

    // ------------------------------------------
		for (const auto& addr : recipients) {
        PrintToLog("%s(): address: %s, sender: %s\n",__func__, addr, sender);
		}

    const auto& from = sender;
    auto pred = [&from](const std::string& a){ return a == from; };
    if (std::find_if(recipients.begin(), recipients.end(), pred) != recipients.end()) {
        PrintToLog("%s(): rejected: sender sending tokens to himself\n", __func__);
        return (PKT_ERROR_SEND -26);
    }

    // // Special case: if can't find the receiver -- assume send to self!
    // if (receiver.empty()) {
    //     receiver = sender;
    // }

    if (recipients.size() != values.size()) {
        PrintToLog("%s(): rejected: recipients/amounts mismatch: addresses=%d values=%d\n",
                __func__,
								recipients.size(),
								values.size());
        return (PKT_ERROR_SEND -27);
    }

    // Move the tokens
    for (size_t i=0; i <recipients.size(); ++i) {
        auto v = values[i];
				if (v > 0) {
				    assert(update_tally_map(sender, property, -v, BALANCE));
					  assert(update_tally_map(recipients[i], property, v, BALANCE));
				}

    }

    return 0;
}

/** Tx 5 */
int CMPTransaction::logicMath_SendVestingTokens()
{
  if (sender == receiver) {
      PrintToLog("%s(): rejected: sender sending vesting tokens to himself\n", __func__);
      return (PKT_ERROR_SEND -26);
  }

  if (!sanityChecks(sender, block)) {
      PrintToLog("%s(): rejected: sanity checks for send vesting tokens failed\n",__func__);
      return (PKT_ERROR_SEND -21);
  }

  if (!IsTransactionTypeAllowed(block, type, version)) {
      PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
              __func__,
              type,
              version,
              TL_PROPERTY_VESTING,
              block);
      return (PKT_ERROR_SP -22);
  }

  const int64_t nBalance = getMPbalance(sender, TL_PROPERTY_VESTING, BALANCE);
  if (nBalance < (int64_t) nValue) {
      PrintToLog("%s(): rejected: sender %s has insufficient balance of property %d [%s < %s]\n",
              __func__,
              sender,
              TL_PROPERTY_VESTING,
              FormatMP(TL_PROPERTY_VESTING, nBalance),
              FormatMP(TL_PROPERTY_VESTING, nValue));
      return (PKT_ERROR_SEND -25);
  }

  assert(update_tally_map(sender, TL_PROPERTY_VESTING, -nValue, BALANCE));
  assert(update_tally_map(receiver, TL_PROPERTY_VESTING, nValue, BALANCE));

	const int64_t unVested = getMPbalance(sender, ALL, UNVESTED);

	if(0 < unVested) {
	    assert(update_tally_map(sender, ALL, -nValue, UNVESTED));
	    assert(update_tally_map(receiver, ALL, nValue, UNVESTED));
	}

  vestingAddresses.insert(receiver);

  return 0;
}

/** Tx 4 */
int CMPTransaction::logicMath_SendAll()
{
    if (!IsTransactionTypeAllowed(block, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_SP -22);
    }

    // ------------------------------------------
    if (sender == receiver) {
        PrintToLog("%s(): rejected: sender sending tokens to himself\n", __func__);
        return (PKT_ERROR_SEND -26);
    }

    // Special case: if can't find the receiver -- assume send to self!
    if (receiver.empty()) {
        receiver = sender;
    }

    CMPTally* ptally = getTally(sender);
    if (ptally == nullptr) {
        PrintToLog("%s(): rejected: sender %s has no tokens to send\n", __func__, sender);
        return (PKT_ERROR_SEND_ALL -54);
    }

    uint32_t propertyId = ptally->init();
    int numberOfPropertiesSent = 0;

    while (0 != (propertyId = ptally->next())) {

        int64_t moneyAvailable = ptally->getMoney(propertyId, BALANCE);
        if (moneyAvailable > 0 && propertyId != TL_PROPERTY_VESTING) {
            ++numberOfPropertiesSent;

            if (propertyId != ALL)
            {
                int kyc_id;

                if(!t_tradelistdb->checkAttestationReg(sender,kyc_id)){
                  PrintToLog("%s(): rejected: kyc ckeck for sender failed\n", __func__);
                  return (PKT_ERROR_KYC -10);
                }

                if(!t_tradelistdb->kycPropertyMatch(propertyId,kyc_id)){
                  PrintToLog("%s(): rejected: property %d can't be traded with this kyc\n", __func__, propertyId);
                  return (PKT_ERROR_KYC -20);
                }

                if(!t_tradelistdb->checkAttestationReg(receiver,kyc_id)){
                  PrintToLog("%s(): rejected: kyc ckeck for receiver failed\n", __func__);
                  return (PKT_ERROR_KYC -10);
                }

                if(!t_tradelistdb->kycPropertyMatch(propertyId,kyc_id)){
                  PrintToLog("%s(): rejected: property %d can't be traded with this kyc\n", __func__, propertyId);
                  return (PKT_ERROR_KYC -20);
                }

            }

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
      if (pindex == nullptr) {
	        PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
	        return (PKT_ERROR_SP -20);
      }
      blockHash = pindex->GetBlockHash();
    }

    if (!IsTransactionTypeAllowed(block, type, version)) {
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

    if (ALL_PROPERTY_TYPE_INDIVISIBLE != prop_type && ALL_PROPERTY_TYPE_DIVISIBLE != prop_type) {
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
    newSP.init_block = block;
    newSP.kyc.push_back(0);

    for_each(kyc_Ids.begin(), kyc_Ids.end(), [&newSP] (const int64_t& aux) { if (aux != 0) newSP.kyc.push_back(aux);});

    const uint32_t propertyId = _my_sps->putSP(newSP);
    assert(propertyId > 0);
    assert(update_tally_map(sender, propertyId, nValue, BALANCE));

    NotifyTotalTokensChanged(propertyId);

    return 0;
}

/** Tx 54 */
int CMPTransaction::logicMath_CreatePropertyManaged()
{
    uint256 blockHash;
    {
        LOCK(cs_main);

        CBlockIndex* pindex = chainActive[block];
        if (pindex == nullptr) {
            PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
            return (PKT_ERROR_SP -20);
        }
        blockHash = pindex->GetBlockHash();
    }

    if (!IsTransactionTypeAllowed(block, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_SP -22);
    }

    if (ALL_PROPERTY_TYPE_INDIVISIBLE != prop_type && ALL_PROPERTY_TYPE_DIVISIBLE != prop_type) {
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
    newSP.kyc.push_back(0);

    for_each(kyc_Ids.begin(), kyc_Ids.end(), [&newSP] (const int64_t& aux) { if (aux != 0) newSP.kyc.push_back(aux);});

    uint32_t propertyId = _my_sps->putSP(newSP);
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
    if (pindex == nullptr) {
      PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
      return (PKT_ERROR_SP -20);
    }
    blockHash = pindex->GetBlockHash();
  }

  if (!IsTransactionTypeAllowed(block, type, version)) {
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
    return (PKT_ERROR_TOKENS -23);
  }

  if (!IsPropertyIdValid(property)) {
    PrintToLog("%s(): rejected: property %d does not exist\n", __func__, property);
    return (PKT_ERROR_TOKENS -24);
  }

  // if (isPropertyContract(property)) {
  //     PrintToLog("%s(): rejected: property %d should not be a contract\n", __func__, property);
  //     return (PKT_ERROR_TOKENS -25);
  // }


  if(property == TL_PROPERTY_VESTING){
       PrintToLog("%s(): rejected: property should not be vesting tokens (id = 3)\n", __func__);
       return (PKT_ERROR_TOKENS -26);
  }

  int kyc_id;

  if(!t_tradelistdb->checkAttestationReg(sender,kyc_id)){
    PrintToLog("%s(): rejected: kyc ckeck for sender failed\n", __func__);
    return (PKT_ERROR_KYC -10);
  }

  if(!t_tradelistdb->kycPropertyMatch(property,kyc_id)){
    PrintToLog("%s(): rejected: property %d can't be traded with this kyc\n", __func__, property);
    return (PKT_ERROR_KYC -20);
  }

  if(!t_tradelistdb->checkAttestationReg(receiver,kyc_id)){
    PrintToLog("%s(): rejected: kyc ckeck for receiver failed\n", __func__);
    return (PKT_ERROR_KYC -10);
  }

  if(!t_tradelistdb->kycPropertyMatch(property,kyc_id)){
    PrintToLog("%s(): rejected: property %d can't be traded with this kyc\n", __func__, property);
    return (PKT_ERROR_KYC -20);
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
  sp.num_tokens += nValue; // updating created tokens

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
        if (pindex == nullptr) {
            PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
            return (PKT_ERROR_TOKENS -20);
        }
        blockHash = pindex->GetBlockHash();
    }

    if (!IsTransactionTypeAllowed(block, type, version)) {
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
        return (PKT_ERROR_TOKENS -23);
    }

    if (!IsPropertyIdValid(property)) {
        PrintToLog("%s(): rejected: property %d does not exist\n", __func__, property);
        return (PKT_ERROR_TOKENS -24);
    }

    // if (isPropertyContract(property)) {
    //     PrintToLog("%s(): rejected: property %d should not be a contract\n", __func__, property);
    //     return (PKT_ERROR_TOKENS -25);
    // }

    if(property == TL_PROPERTY_VESTING){
         PrintToLog("%s(): rejected: property should not be vesting tokens (id = 3)\n", __func__);
         return (PKT_ERROR_TOKENS -26);
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
        if (pindex == nullptr) {
            PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
            return (PKT_ERROR_TOKENS -20);
        }
        blockHash = pindex->GetBlockHash();
    }

    if (!IsTransactionTypeAllowed(block, type, version)) {
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
        return (PKT_ERROR_TOKENS -24);
    }

    CMPSPInfo::Entry sp;
    assert(_my_sps->getSP(property, sp));

    if (sender != sp.issuer) {
        PrintToLog("%s(): rejected: sender %s is not issuer of property %d [issuer=%s]\n", __func__, sender, property, sp.issuer);
        return (PKT_ERROR_TOKENS -43);
    }

    if (receiver.empty()) {
        PrintToLog("%s(): rejected: receiver is empty\n", __func__);
        return (PKT_ERROR_TOKENS -45);
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
    if (!IsTransactionTypeAllowed(block, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_SP -22);
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
	  PrintToLog("%s(): inside function\n",__func__);

    if (!IsTransactionTypeAllowed(block, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted at block %d\n",
                __func__,
                type,
                version,
                block);
        return (PKT_ERROR_SP -22);
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
    if (!IsTransactionTypeAllowed(block, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_SP -22);
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
    DoWarning(alert_text);

    return 0;
}

int CMPTransaction::logicMath_MetaDExTrade()
{
  if (!IsTransactionTypeAllowed(block, type, version)) {
      PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
              __func__,
              type,
              version,
              property,
              block);
      return (PKT_ERROR_SP -22);
  }

  if (property == desired_property) {
      PrintToLog("%s(): rejected: property for sale %d and desired property %d must not be equal\n",
              __func__,
              property,
              desired_property);
      return (PKT_ERROR_METADEX -29);
  }

  if (!IsPropertyIdValid(property)) {
      PrintToLog("%s(): rejected: property for sale %d does not exist\n", __func__, property);
      return (PKT_ERROR_METADEX -31);
  }

  if (!IsPropertyIdValid(desired_property)) {
      PrintToLog("%s(): rejected: desired property %d does not exist\n", __func__, desired_property);
      return (PKT_ERROR_METADEX -32);
  }

  // if (isPropertyContract(property)) {
  //     PrintToLog("%s(): rejected: property %d should not be a contract\n", __func__, property);
  //     return (PKT_ERROR_METADEX -25);
  // }


  if(property == TL_PROPERTY_VESTING){
       PrintToLog("%s(): rejected: property should not be vesting tokens (id = 3)\n", __func__);
       return (PKT_ERROR_METADEX -26);
  }


  // if (isPropertyContract(desired_property)) {
  //     PrintToLog("%s(): rejected: property %d should not be a contract\n", __func__, desired_property);
  //     return (PKT_ERROR_METADEX -25);
  // }


  if(desired_property == TL_PROPERTY_VESTING){
       PrintToLog("%s(): rejected: property should not be vesting tokens (id = 3)\n", __func__);
       return (PKT_ERROR_METADEX -26);
  }

  int kyc_id;

  if (property != ALL)
  {
      if(!t_tradelistdb->checkAttestationReg(sender,kyc_id)){
        PrintToLog("%s(): rejected: kyc ckeck failed\n", __func__);
        return (PKT_ERROR_KYC -10);
      }

      if(!t_tradelistdb->kycPropertyMatch(property,kyc_id)){
        PrintToLog("%s(): rejected: property %d can't be traded with this kyc\n", __func__, property);
        return (PKT_ERROR_KYC -20);
      }
  }

  if(propertyId != ALL && !t_tradelistdb->kycPropertyMatch(desired_property,kyc_id)){
    PrintToLog("%s(): rejected: property %d can't be traded with this kyc\n", __func__, desired_property);
    return (PKT_ERROR_METADEX -34);
  }

  if (nNewValue <= 0 || MAX_INT_8_BYTES < nNewValue) {
      PrintToLog("%s(): rejected: amount for sale out of range or zero: %d\n", __func__, nNewValue);
      return (PKT_ERROR_METADEX -34);
  }

  if ((uint64_t) desired_value <= 0 || MAX_INT_8_BYTES < (uint64_t) desired_value) {
      PrintToLog("%s(): rejected: desired amount out of range or zero: %d\n", __func__, desired_value);
      return (PKT_ERROR_METADEX -35);
  }

  int64_t nBalance = 0;
  if (!mastercore::checkReserve(sender, nNewValue, property, nBalance)) {
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
  return (MetaDEx_ADD(sender, property, nNewValue, block, desired_property, desired_value, txid, tx_idx));
}

/** Tx 40 */
int CMPTransaction::logicMath_CreateContractDex()
{
  uint256 blockHash;
  {
      LOCK(cs_main);
      CBlockIndex* pindex = chainActive[block];

      if (pindex == nullptr)
      {
	        PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
	        return (PKT_ERROR_SP -20);
      }
      blockHash = pindex->GetBlockHash();
  }

  if (!IsTransactionTypeAllowed(block, type, version)) {
      PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
          __func__,
          type,
          version,
          propertyId,
          block);
      return (PKT_ERROR_SP -22);
  }

  if ('\0' == name[0]) {
    PrintToLog("%s(): rejected: property name must not be empty\n", __func__);
    return (PKT_ERROR_SP -37);
  }

  // -----------------------------------------------

  CDInfo::Entry newCD;
  newCD.txid = txid;
  newCD.issuer = sender;
  newCD.prop_type = prop_type;
  newCD.name.assign(name);
  newCD.creation_block = blockHash;
  newCD.update_block = blockHash;
  newCD.blocks_until_expiration = blocks_until_expiration;
  newCD.notional_size = notional_size;
  newCD.collateral_currency = collateral_currency;
  newCD.margin_requirement = margin_requirement;
  newCD.init_block = block;
  newCD.numerator = numerator;
  newCD.denominator = denominator;
  newCD.attribute_type = attribute_type;
  newCD.expirated = false;
  newCD.inverse_quoted = inverse_quoted;
  newCD.kyc.push_back(0);

  for_each(kyc_Ids.begin(), kyc_Ids.end(), [&newCD] (const int64_t& aux) { if (aux != 0) newCD.kyc.push_back(aux);});

  const uint32_t contractId = _my_cds->putCD(newCD);
  PrintToLog("%s(): contractId: %d\n",__func__, contractId);
  assert(contractId > 0);

  return 0;
}

int CMPTransaction::logicMath_ContractDexTrade()
{
  auto fco = getFutureContractObject(name_traded);
  const uint32_t contractId = fco.fco_propertyId;
  const uint32_t expiration = fco.fco_blocks_until_expiration;
  const uint32_t colateralh = fco.fco_collateral_currency;
  // (pfuture->fco_prop_type == ALL_PROPERTY_TYPE_NATIVE_CONTRACT) ? result = 5 : result = 6;

  int kyc_id;

  if(!t_tradelistdb->checkAttestationReg(sender,kyc_id)){
    PrintToLog("%s(): rejected: kyc ckeck failed\n", __func__);
    return (PKT_ERROR_KYC -10);
  }

  if(!t_tradelistdb->kycContractMatch(contractId,kyc_id)){
    PrintToLog("%s(): rejected: contract %d can't be traded with this kyc\n", __func__, contractId);
    return (PKT_ERROR_KYC -20);
  }

  if ((block > fco.fco_init_block + static_cast<int>(fco.fco_blocks_until_expiration) || block < fco.fco_init_block) && expiration > 0)
  {
      PrintToLog("%s(): ERROR: Contract expirated \n", __func__);
      return PKT_ERROR_SP -38;
  }

  const int64_t rleverage = getContractRecord(sender, contractId, LEVERAGE);
  const int64_t position = getContractRecord(sender, contractId, CONTRACT_POSITION);

  PrintToLog("%s(): Leverage in register: %d, position: %d \n", __func__, rleverage, position);

  if (position == 0 && rleverage == 0) {
      //setting leverage
      PrintToLog("%s(): setting leverage: %d \n", __func__, leverage);
      assert(update_register_map(sender, contractId, leverage, LEVERAGE));

  } else if(rleverage != leverage &&  0 < position) {
      PrintToLog("%s(): ERROR: Bad leverage \n", __func__);
      return CONTRACTDEX_ERROR - 1;
  }

  int64_t nBalance = 0;
  int64_t amountToReserve = 0;

  if (!mastercore::checkContractReserve(sender, amount, contractId, leverage, nBalance, amountToReserve) || nBalance == 0)
  {
        PrintToLog("%s(): rejected: sender %s has insufficient balance for contracts %d [%s < %s] \n",
		      __func__,
		      sender,
		      property,
		      FormatMP(property, nBalance),
		      FormatMP(property, amountToReserve));
        return (PKT_ERROR_SEND -25);
  }else {
      if (amountToReserve > 0)
	    {
           //NOTE: this amount is transfered to position margin when exist matches in x_TradeBidirectional function
	        assert(update_tally_map(sender, colateralh, -amountToReserve, BALANCE));
	        assert(update_tally_map(sender, colateralh,  amountToReserve, CONTRACTDEX_RESERVE));
	    }

  }

  /*********************************************/
  /**Logic for Node Reward**/
  // const CConsensusParams &params = ConsensusParams();
  // int BlockInit = params.MSC_NODE_REWARD_BLOCK;
  // int nBlockNow = GetHeight();
	//
  // BlockClass NodeRewardObj(BlockInit, nBlockNow);
  // NodeRewardObj.SendNodeReward(sender);

  /*********************************************/
  t_tradelistdb->recordNewTrade(txid, sender, contractId, desired_property, block, tx_idx, 0);
  return (ContractDex_ADD(sender, contractId, amount, block, txid, tx_idx, effective_price, trading_action, amountToReserve));
}

/** Tx 31 */
int CMPTransaction::logicMath_ContractDExCancel()
{
  if (!IsTransactionTypeAllowed(block, type, version)) {
    PrintToLog("%s(): rejected: type %d or version %d not permitted at block %d\n",
	       __func__,
	       type,
	       version,
	       block);
    return (PKT_ERROR_SP -22);
  }

  return (ContractDex_CANCEL(sender,hash));
}

/** Tx 32 */
int CMPTransaction::logicMath_ContractDexCancelEcosystem()
{
  if (!IsTransactionTypeAllowed(block, type, version)) {
    PrintToLog("%s(): rejected: type %d or version %d not permitted at block %d\n",
	       __func__,
	       type,
         version,
	       block);
    return (PKT_ERROR_SP -22);
  }

  return (ContractDex_CANCEL_EVERYTHING(txid, block, sender, contractId));
}

/** Tx 33 */
int CMPTransaction::logicMath_ContractDexClosePosition()
{
    if (!IsTransactionTypeAllowed(block, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
            __func__,
            type,
            version,
            property,
            block);
        return (PKT_ERROR_SP -22);
    }

    CDInfo::Entry sp;
    {
        LOCK(cs_tally);
        if (!_my_cds->getCD(contractId, sp)) {
            PrintToLog(" %s() : Property identifier %d does not exist\n",
                __func__,
                contractId);
            return (PKT_ERROR_SEND -24);
        }
    }

    return (ContractDex_CLOSE_POSITION(txid, block, sender, contractId, sp.collateral_currency, false));
}

int CMPTransaction::logicMath_ContractDex_Cancel_Orders_By_Block()
{
  if (!IsTransactionTypeAllowed(block, type, version)) {
      PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
              __func__,
              type,
              version,
              propertyId,
              block);
     return (PKT_ERROR_SP -22);

    }

    return (ContractDex_CANCEL_FOR_BLOCK(txid, block, tx_idx, sender));
}

/** Tx 35 */
int CMPTransaction::logicMath_MetaDExCancel()
{
    if (!IsTransactionTypeAllowed(block, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted at block %d\n",
	        __func__,
	        type,
	        version,
	        block);
     return (PKT_ERROR_SP -22);
    }

    return (MetaDEx_CANCEL(txid, sender, block, hash));
}


/** Tx 36 */
int CMPTransaction::logicMath_MetaDExCancel_ByPair()
{
  if (!IsTransactionTypeAllowed(block, type, version)) {
         PrintToLog("%s(): rejected: type %d or version %d not permitted at block %d\n",
                 __func__,
                 type,
                 version,
                 block);
         return (PKT_ERROR_SP -22);
     }

     if (property == desired_property) {
         PrintToLog("%s(): rejected: property for sale %d and desired property %d must not be equal\n",
                 __func__,
                 property,
                 desired_property);
         return (PKT_ERROR_METADEX -29);
     }

     if (!IsPropertyIdValid(property)) {
         PrintToLog("%s(): rejected: property for sale %d does not exist\n", __func__, property);
         return (PKT_ERROR_METADEX -31);
     }

     if (!IsPropertyIdValid(desired_property)) {
         PrintToLog("%s(): rejected: desired property %d does not exist\n", __func__, desired_property);
         return (PKT_ERROR_METADEX -32);
     }

     // ------------------------------------------

     return (MetaDEx_CANCEL_ALL_FOR_PAIR(txid, block, sender, propertyId, desired_property));

}


/** Tx 37 */
int CMPTransaction::logicMath_MetaDExCancel_ByPrice()
{
  if (!IsTransactionTypeAllowed(block, type, version)) {
         PrintToLog("%s(): rejected: type %d or version %d not permitted at block %d\n",
                 __func__,
                 type,
                 version,
                 block);
         return (PKT_ERROR_SP -22);
     }

     if (property == desired_property) {
         PrintToLog("%s(): rejected: property for sale %d and desired property %d must not be equal\n",
                 __func__,
                 property,
                 desired_property);
         return (PKT_ERROR_METADEX -29);
     }

     if (!IsPropertyIdValid(property)) {
         PrintToLog("%s(): rejected: property for sale %d does not exist\n", __func__, property);
         return (PKT_ERROR_METADEX -31);
     }

     if (!IsPropertyIdValid(desired_property)) {
         PrintToLog("%s(): rejected: desired property %d does not exist\n", __func__, desired_property);
         return (PKT_ERROR_METADEX -32);
     }

     // ------------------------------------------

     return (MetaDEx_CANCEL_AT_PRICE(txid, block, sender, propertyId, amount_forsale, desired_property, desired_value));
}

/** Tx 100 */
int CMPTransaction::logicMath_CreatePeggedCurrency()
{
    uint256 blockHash;
    uint32_t notSize = 0;
    uint32_t npropertyId = 0;
    int64_t contracts = 0;

    {
        LOCK(cs_main);

        CBlockIndex* pindex = chainActive[block];
        if (pindex == nullptr) {
            PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
            return (PKT_ERROR_SP -20);
        }

        blockHash = pindex->GetBlockHash();
    }

    if (!IsTransactionTypeAllowed(block, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_SP -22);
    }

    if (ALL_PROPERTY_TYPE_INDIVISIBLE != prop_type && ALL_PROPERTY_TYPE_DIVISIBLE != prop_type && ALL_PROPERTY_TYPE_PEGGEDS != prop_type) {
        PrintToLog("%s(): rejected: invalid property type: %d\n", __func__, prop_type);
        return (PKT_ERROR_SP -36);
    }

    if ('\0' == name[0]) {
        PrintToLog("%s(): rejected: property name must not be empty\n", __func__);
        return (PKT_ERROR_SP -37);
    }

    // checking collateral currency
    uint64_t nBalance = (uint64_t) getMPbalance(sender, propertyId, BALANCE);
    if (nBalance == 0) {
        PrintToLog("%s(): rejected: sender %s has insufficient collateral currency in balance %d \n",
             __func__,
             sender,
             propertyId);
        return (PKT_ERROR_SEND -25);
    }

    CDInfo::Entry cd;
    {
        LOCK(cs_register);

        if (!_my_cds->getCD(contractId, cd)) {
            PrintToLog(" %s() : Contract identifier %d does not exist\n",
                __func__,
                contractId);
            return (PKT_ERROR_SEND -24);

        } else if (cd.collateral_currency != propertyId) {
            PrintToLog(" %s() : Future contract has not this collateral currency %d\n",
            __func__,
            propertyId);
            return (PKT_ERROR_CONTRACTDEX -22);

        }

        notSize = static_cast<int64_t>(cd.notional_size);
    }

    const int64_t position = getContractRecord(sender, contractId, CONTRACT_POSITION);
    const arith_uint256 Contracts = ConvertTo256(amount) * ConvertTo256(notSize) / (ConvertTo256(COIN) * ConvertTo256(COIN));
    contracts = ConvertTo64(Contracts);

    PrintToLog("%s(): position: %d, contracts: %d, amount: %d, nBalance: %d, contractId: %d\n",__func__, position, contracts, amount, nBalance, contractId);

    if (position >= 0 || nBalance < amount || abs(position) < contracts) {
        PrintToLog("%s(): rejected:Sender has not required short position on this contract or balance enough\n",__func__);
        return (PKT_ERROR_CONTRACTDEX -23);
    }


    {
        LOCK(cs_tally);
        uint32_t nextSPID = _my_sps->peekNextSPID();
        for (uint32_t prop = 1; prop < nextSPID; prop++) {
            CMPSPInfo::Entry sp;
            if (_my_sps->getSP(prop, sp)) {
                if (sp.prop_type == ALL_PROPERTY_TYPE_PEGGEDS){
                    // checking if there's a synthetic property created by this contract yet
                    if (sp.contract_associated == contractId && sp.currency_associated == prop)
                    {
                        PrintToLog("%() Creating more currency (propertyId: %d, contract associated: %d, currency associated: %d)\n",__func__, npropertyId, sp.contract_associated, sp.currency_associated);
                    } else {
                        PrintToLog("%() Creating New synthetic currency (propertyId: %d, contract associated: %d, currency associated: %d)\n",__func__, npropertyId, sp.contract_associated, sp.currency_associated);
                        npropertyId = prop;
                    }

                    break;

                }
            }
        }
    }

    // ------------------------------------------

    if (npropertyId == 0) {   // putting the first one pegged currency of this denominator
        CMPSPInfo::Entry newSP;
        newSP.issuer = sender;
        newSP.txid = txid;
        newSP.prop_type = ALL_PROPERTY_TYPE_PEGGEDS;
        newSP.subcategory.assign(subcategory);
        newSP.name.assign(name);
        newSP.fixed = true;
        newSP.manual = true;
        newSP.creation_block = blockHash;
        newSP.update_block = newSP.creation_block;
        newSP.num_tokens = amount;
        newSP.contracts_needed = contracts;
        newSP.contract_associated = contractId;
        newSP.currency_associated = cd.collateral_currency; // we need to see the real currency associated to this syntetic
        npropertyId = _my_sps->putSP(newSP);

    } else {
        CMPSPInfo::Entry newSP;
        _my_sps->getSP(npropertyId, newSP);
        newSP.num_tokens += amount;
        _my_sps->updateSP(npropertyId, newSP);
    }

    assert(npropertyId > 0);
    CMPSPInfo::Entry SP;
    _my_sps->getSP(npropertyId, SP);

    // synthetic tokens update
    assert(update_tally_map(sender, npropertyId, amount, BALANCE));
    // t_tradelistdb->NotifyPeggedCurrency(txid, sender, npropertyId, amount,SP.series); //TODO: Watch this function!

    // Adding the element to map of pegged currency owners
    peggedIssuers.insert (std::pair<std::string,uint32_t>(sender,npropertyId));

    if (msc_debug_create_pegged)
    {
        PrintToLog("Pegged currency Id: %d\n", npropertyId);
        PrintToLog("associated currency Id: %d\n", propertyId);
        PrintToLog("CREATED PEGGED PROPERTY id: %d admin: %s\n", npropertyId, sender);
    }


    //putting into reserve contracts and collateral currency
    assert(update_register_map(sender, contractId, contracts, CONTRACT_POSITION));
    assert(update_register_map(sender, contractId, contracts, CONTRACT_RESERVE));
    assert(update_tally_map(sender, propertyId, -amount, BALANCE));
    assert(update_tally_map(sender, propertyId, amount, CONTRACTDEX_RESERVE));

    return 0;
}

int CMPTransaction::logicMath_SendPeggedCurrency()
{
    if (!IsTransactionTypeAllowed(block, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
            __func__,
            type,
            version,
            property,
            block);
        return (PKT_ERROR_SP -22);
    }

    // NOTE: we need to check if property is a synthetic/pegged currency here


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
        return (PKT_ERROR_SEND -25);
    }

    if (msc_debug_send_pegged)
    {
        PrintToLog("nBalance Pegged Currency Sender : %d \n",nBalance);
        PrintToLog("amount to send of Pegged Currency : %d \n",amount);
    }

    // ------------------------------------------

    // Special case: if can't find the receiver -- assume send to self!
    if (receiver.empty()) {
        receiver = sender;
    }

    // Move the tokensss

    assert(update_tally_map(sender, propertyId, -amount, BALANCE));
    assert(update_tally_map(receiver, propertyId, amount, BALANCE));

    // Adding the element to map of pegged currency owners
    peggedIssuers.insert (std::pair<std::string,uint32_t>(receiver,propertyId));

    return 0;
}

int CMPTransaction::logicMath_RedemptionPegged()
{
    if (!IsTransactionTypeAllowed(block, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                propertyId,
                block);
        return (PKT_ERROR_SP -22);
    }

    if (!IsPropertyIdValid(propertyId)) {
        PrintToLog("%s(): rejected: property %d does not exist\n", __func__, property);
        return (PKT_ERROR_SEND -24);
    }

    const int64_t nBalance = getMPbalance(sender, propertyId, BALANCE);

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

    CDInfo::Entry sp;
    {
        LOCK(cs_register);
        if (!_my_cds->getCD(contractId, sp)) {
            PrintToLog(" %s() : Property identifier %d does not exist\n",
            __func__,
            contractId);
           return (PKT_ERROR_SEND -24);
        }

        collateralId = sp.collateral_currency;
        notSize = static_cast<int64_t>(sp.notional_size);
        // sp.num_tokens -= amount;
    }

    const arith_uint256 conNeeded = ConvertTo256(amount) * ConvertTo256(notSize) / (ConvertTo256(COIN) * ConvertTo256(COIN));
    const int64_t contractsNeeded = ConvertTo64(conNeeded);

    const int64_t position = getContractRecord(sender, contractId, CONTRACT_RESERVE);

    PrintToLog("%s(): position reserved: %d, contractsNeeded: %d\n",__func__, position, contractsNeeded);


    if (position < contractsNeeded) {
        PrintToLog("%s(): rejected: sender %s has insufficient short position reserved\n",
                __func__,
                sender);
        return (PKT_ERROR_SEND -26);
    }

    if (contractsNeeded != 0 && amount > 0)
    {
       // Delete the tokens
       assert(update_tally_map(sender, propertyId, -amount, BALANCE));
       // delete contracts in reserve
       assert(update_register_map(sender, contractId, -contractsNeeded, CONTRACT_RESERVE));
       // getting back short position
       assert(update_register_map(sender, contractId, -contractsNeeded, CONTRACT_POSITION));
       // getting back the collateral
       assert(update_tally_map(sender, collateralId, amount, BALANCE));


    } else {
        PrintToLog("amount redeemed must be equal at least to value of 1 future contract \n");
    }

    return 0;
}

int CMPTransaction::logicMath_DExSell()
{
    if (!IsTransactionTypeAllowed(block, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for this transaction at block %d\n",
            __func__,
            type,
            version,
            block);
      return (PKT_ERROR_SP -22);
    }


    if (MAX_INT_8_BYTES < nValue) {
        PrintToLog("%s(): rejected: value out of range or zero: %d\n", __func__, nValue);
        return (PKT_ERROR_SEND -23);
    }

    // ------------------------------------------

    int rc = PKT_ERROR_TRADEOFFER;

    // figure out which Action this is based on amount for sale, version & etc.
    // TODO: delete old version
    switch (version)
    {
        case MP_TX_PKT_V0:
        {
            if (0 != nValue) {

                if (!DEx_offerExists(sender, propertyId)) {
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

/*Tx 21*/
int CMPTransaction::logicMath_DExBuy()
{
    if (!IsTransactionTypeAllowed(block, type, version)) {
     PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
             __func__,
             type,
             version,
             propertyId,
             block);
      return (PKT_ERROR_SP -22);
    }

    // if(!t_tradelistdb->checkKYCRegister(sender,4))
    // {
    //     PrintToLog("%s: tx disable from kyc register!\n",__func__);
    //     return (PKT_ERROR_KYC -10);
    // }

    if (MAX_INT_8_BYTES < nValue) {
        PrintToLog("%s(): rejected: value out of range or zero: %d\n", __func__, nValue);
        return (PKT_ERROR_SEND -23);
    }


  // ------------------------------------------

    int rc = PKT_ERROR_TRADEOFFER;

    // figure out which Action this is based on amount for sale, version & etc.
    switch (version)
      {
      case MP_TX_PKT_V0:
        {
	  if (0 != nValue) {

	    if (!DEx_offerExists(sender, propertyId)) {
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

  if (nValue <= 0 || MAX_INT_8_BYTES < nValue) {
    PrintToLog("%s(): rejected: value out of range or zero: %d\n", __func__, nValue);
  }

  // if(!t_tradelistdb->checkKYCRegister(sender,4) || !t_tradelistdb->checkKYCRegister(receiver,4))
  // {
  //     PrintToLog("%s: tx disable from kyc register!\n",__func__);
  //     return (PKT_ERROR_KYC -10);
  // }

  // the min fee spec requirement is checked in the following function
  return (DEx_acceptCreate(sender, receiver, propertyId, nValue, block, tx_fee_paid, &nNewValue));
}


/** Tx 103 */
int CMPTransaction::logicMath_CreateOracleContract()
{

    uint256 blockHash;
    {
        LOCK(cs_main);
        CBlockIndex* pindex = chainActive[block];

        if (pindex == nullptr)
        {
            PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
            return (PKT_ERROR_SP -20);
        }

        blockHash = pindex->GetBlockHash();
    }

    if (sender == receiver)
    {
        PrintToLog("%s(): ERROR: oracle and backup addresses can't be the same!\n", __func__);
        return (PKT_ERROR_ORACLE -10);
    }


    if (!IsTransactionTypeAllowed(block, type, version)) {
      PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
          __func__,
          type,
          version,
          propertyId,
          block);
      return (PKT_ERROR_SP -22);
    }


    if ('\0' == name[0])
    {
        PrintToLog("%s(): rejected: property name must not be empty\n", __func__);
        return (PKT_ERROR_SP -37);
    }

    // -----------------------------------------------

    CDInfo::Entry newSP;
    newSP.txid = txid;
    newSP.issuer = sender;
    newSP.prop_type = prop_type;
    newSP.name.assign(name);
    newSP.creation_block = blockHash;
    newSP.update_block = blockHash;
    newSP.blocks_until_expiration = blocks_until_expiration;
    newSP.notional_size = notional_size;
    newSP.collateral_currency = collateral_currency;
    newSP.margin_requirement = margin_requirement;
    newSP.init_block = block;
    newSP.attribute_type = attribute_type;
    newSP.backup_address = receiver;
    newSP.expirated = false;
    newSP.inverse_quoted = inverse_quoted;
    newSP.oracle_high = 0;
    newSP.oracle_low = 0;
    newSP.oracle_close = 0;
    newSP.kyc.push_back(0);

    for_each(kyc_Ids.begin(), kyc_Ids.end(), [&newSP] (const int64_t& aux) { if (aux != 0) newSP.kyc.push_back(aux);});

    const uint32_t contractId = _my_cds->putCD(newSP);
    assert(contractId > 0);

    return 0;
}

/** Tx 104 */
int CMPTransaction::logicMath_Change_OracleAdm()
{
    uint256 blockHash;
    {
        LOCK(cs_main);
        CBlockIndex* pindex = chainActive[block];

        if (pindex == nullptr)
        {
            PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
            return (PKT_ERROR_SP -20);
        }
        blockHash = pindex->GetBlockHash();
    }

    if (!IsTransactionTypeAllowed(block, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for contract %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_SP -22);
    }

    if (!IsPropertyIdValid(contractId)) {
        PrintToLog("%s(): rejected: oracle contract %d does not exist\n", __func__, property);
        return (PKT_ERROR_ORACLE -11);
    }

    CDInfo::Entry cd;
    assert(_my_cds->getCD(contractId, cd));

    if (sender != cd.issuer) {
        PrintToLog("%s(): rejected: sender %s is not issuer of contract %d [issuer=%s]\n", __func__, sender, property, cd.issuer);
        return (PKT_ERROR_ORACLE -12);
    }

    if (receiver.empty()) {
        PrintToLog("%s(): rejected: receiver is empty\n", __func__);
        return (PKT_ERROR_ORACLE -13);
    }

    // ------------------------------------------

    cd.issuer = receiver;
    cd.update_block = blockHash;

    assert(_my_cds->updateCD(contractId, cd));

    return 0;
}

/** Tx 105 */
int CMPTransaction::logicMath_Set_Oracle()
{
    if (!IsTransactionTypeAllowed(block, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_SP -22);
    }

    if (!IsPropertyIdValid(contractId)) {
        PrintToLog("%s(): rejected: oracle %d does not exist\n", __func__, property);
        return (PKT_ERROR_ORACLE -11);
    }

    CDInfo::Entry sp;
    assert(_my_cds->getCD(contractId, sp));

    if (sender != sp.issuer) {
        PrintToLog("%s(): rejected: sender %s is not the oracle address of the future contract %d [oracle address=%s]\n", __func__, sender, contractId, sp.issuer);
        return (PKT_ERROR_ORACLE -12);
    }


    // ------------------------------------------
    oracledata Ol;

    Ol.high = oracle_high;
    Ol.low = oracle_low;
    Ol.close = oracle_close;

    oraclePrices[contractId][block] = Ol;

    // PrintToLog("%s():Ol element:,high:%d, low:%d, close:%d\n",__func__, Ol.high, Ol.low, Ol.close);

    // saving on db
    sp.oracle_high = oracle_high;
    sp.oracle_low = oracle_low;
    sp.oracle_close = oracle_close;

   if (msc_debug_set_oracle)
   {
       (oraclePrices.empty()) ? PrintToLog("%s(): element was not inserted !\n",__func__) : PrintToLog("%s(): element was INSERTED \n",__func__);
   }


    assert(_my_cds->updateCD(contractId, sp));

    // if (msc_debug_set_oracle) PrintToLog("oracle data for contract: block: %d,high:%d, low:%d, close:%d\n",block, oracle_high, oracle_low, oracle_close);

    return 0;
}

/** Tx 106 */
int CMPTransaction::logicMath_OracleBackup()
{
    uint256 blockHash;
    {
        LOCK(cs_main);
        CBlockIndex* pindex = chainActive[block];

        if (pindex == nullptr)
        {
            PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
            return (PKT_ERROR_SP -20);
        }

        blockHash = pindex->GetBlockHash();
    }

    if (!IsTransactionTypeAllowed(block, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted at block %d\n",
                __func__,
                type,
                version,
                block);
        return (PKT_ERROR_SP -22);
    }

    if (!IsPropertyIdValid(contractId)) {
        PrintToLog("%s(): rejected: oracle %d does not exist\n", __func__, property);
        return (PKT_ERROR_ORACLE -11);
    }

    CDInfo::Entry sp;
    assert(_my_cds->getCD(contractId, sp));

    if (sender != sp.backup_address) {
        PrintToLog("%s(): rejected: sender %s is not the backup address of the Oracle Future Contract\n", __func__,sender);
        return (PKT_ERROR_ORACLE -14);
    }

    // ------------------------------------------

    sp.issuer = sender;
    sp.update_block = blockHash;

    assert(_my_cds->updateCD(contractId, sp));

    return 0;
}

/** Tx 107 */
int CMPTransaction::logicMath_CloseOracle()
{
    if (!IsTransactionTypeAllowed(block, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted at block %d\n",
                __func__,
                type,
                version,
                block);
        return (PKT_ERROR_SP -22);
    }

    if (!IsPropertyIdValid(contractId)) {
        PrintToLog("%s(): rejected: oracle %d does not exist\n", __func__, property);
        return (PKT_ERROR_ORACLE -11);
    }

    CDInfo::Entry sp;
    assert(_my_cds->getCD(contractId, sp));

    if (sender != sp.backup_address) {
        PrintToLog("%s(): rejected: sender (%s) is not the backup address of the Oracle Future Contract\n", __func__,sender);
        return (PKT_ERROR_ORACLE -14);
    }

    // ------------------------------------------

    sp.blocks_until_expiration = 0;

    assert(_my_cds->updateCD(contractId, sp));

    PrintToLog("%s(): Oracle Contract (id:%d) Closed\n", __func__, contractId);

    return 0;
}

/** Tx 108 */
int CMPTransaction::logicMath_CommitChannel()
{
    if (!IsTransactionTypeAllowed(block, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_SP -22);
    }

    if (!IsPropertyIdValid(propertyId)) {
        PrintToLog("%s(): rejected: property %d does not exist\n", __func__, property);
        return (PKT_ERROR_TOKENS -24);
    }

    int64_t nBalance = getMPbalance(sender, propertyId, BALANCE);
    if (nBalance < (int64_t) amount_commited) {
        PrintToLog("%s(): rejected: sender %s has insufficient balance of property %d [%s < %s]\n",
                __func__,
                sender,
                propertyId,
                FormatMP(propertyId, nBalance),
                FormatMP(propertyId, amount_commited));
        return (PKT_ERROR_TOKENS -25);
    }

    if (!channelSanityChecks(sender, receiver, propertyId, amount_commited, block, tx_idx)){
        PrintToLog("%s(): rejected: invalid address or channel is inactive\n", __func__);
        return (PKT_ERROR_TOKENS -23);
    }

    if(msc_debug_commit_channel) PrintToLog("%s():sender: %s, channelAddress: %s, amount_commited: %d, propertyId: %d\n",__func__, sender, receiver, amount_commited, propertyId);

    assert(update_tally_map(sender, propertyId, -amount_commited, BALANCE));

    t_tradelistdb->recordNewCommit(txid, receiver, sender, propertyId, amount_commited, block, tx_idx);

    return 0;
}

/** Tx 109 */
int CMPTransaction::logicMath_Withdrawal_FromChannel()
{
    if (!IsTransactionTypeAllowed(block, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_SP -22);
    }

    if (!IsPropertyIdValid(propertyId)) {
        PrintToLog("%s(): rejected: property %d does not exist\n", __func__, property);
        return (PKT_ERROR_TOKENS -24);
    }

    if (!checkChannelAddress(receiver)) {
        PrintToLog("%s(): rejected: receiver: %s is not multisig channel\n", __func__, receiver);
        return (PKT_ERROR_CHANNELS -10);
    }

    // checking the amount remaining in the channel
    auto it = channels_Map.find(receiver);
    assert(it != channels_Map.end());
    Channel &chn = it->second;

    //checking amount in channel for sender
    const uint64_t remaining = static_cast<uint64_t>(chn.getRemaining(sender, propertyId));

    if (amount_to_withdraw > remaining)
    {
        PrintToLog("%s(): withdrawal request is not possible (amount to withdraw > remaining for given address))\n", __func__);
        return (PKT_ERROR_TOKENS -26);
    }

    withdrawalAccepted wthd;
    wthd.address = sender;
    wthd.deadline_block = block + 7;
    wthd.propertyId = propertyId;
    wthd.amount = amount_to_withdraw;
    wthd.txid = txid;

    if (msc_debug_withdrawal_from_channel) PrintToLog("checking wthd element : address: %s, deadline: %d, propertyId: %d, amount: %d \n", wthd.address, wthd.deadline_block, wthd.propertyId, wthd.amount);

    auto p = withdrawal_Map.find(receiver);

    // channel found !
    if(p != withdrawal_Map.end())
    {
        vector<withdrawalAccepted>& whAc = p->second;
        whAc.push_back(wthd);

    } else {
        vector<withdrawalAccepted> whAcc;
        whAcc.push_back(wthd);
        withdrawal_Map.insert(std::make_pair(receiver,whAcc));
    }


    t_tradelistdb->recordNewWithdrawal(txid, receiver, sender, propertyId, amount_to_withdraw, block, tx_idx);

    return 0;
}


/** Tx 110 */
int CMPTransaction::logicMath_Instant_Trade()
{
  int rc = 0;

  if (!IsTransactionTypeAllowed(block, type, version)) {
      PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
              __func__,
              type,
              version,
              property,
              block);
      return (PKT_ERROR_SP -22);
  }

  if (property == desired_property) {
      PrintToLog("%s(): rejected: property for sale %d and desired property %d must not be equal\n",
              __func__,
              property,
              desired_property);
      return (PKT_ERROR_CHANNELS -11);
  }

  if (!IsPropertyIdValid(property)) {
      PrintToLog("%s(): rejected: property for sale %d does not exist\n", __func__, property);
      return (PKT_ERROR_CHANNELS -13);
  }

  if (!IsPropertyIdValid(desired_property)) {
      PrintToLog("%s(): rejected: desired property %d does not exist\n", __func__, desired_property);
      return (PKT_ERROR_CHANNELS -14);
  }

  PrintToLog("%s(): SENDER (multisig address): %s, SPECIAL REF (first address): %s,  RECEIVER (second address): %s\n",__func__, sender, special, receiver);


  // NOTE: no way to see if commit txs arrived before this tx.

  auto it = channels_Map.find(sender);
  if (it == channels_Map.end()){
      PrintToLog("%s(): rejected: channel not found\n", __func__);
      return (PKT_ERROR_CHANNELS -19);
  }

  Channel &chn = it->second;


  // using first address data
  int kyc_id;
  if(!t_tradelistdb->checkAttestationReg(special,kyc_id)){
    PrintToLog("%s(): rejected: first address (%s) kyc ckeck failed\n", __func__, special);
    return (PKT_ERROR_KYC -10);
  }

  if(!t_tradelistdb->kycPropertyMatch(property, kyc_id)){
    PrintToLog("%s(): rejected: property %d can't be traded with this kyc for first address (%s)\n", __func__, property, special);
    return (PKT_ERROR_KYC -20);
  }

  if(!t_tradelistdb->kycPropertyMatch(desired_property, kyc_id)){
    PrintToLog("%s(): rejected: property %d can't be traded with this kyc for first address (%s)\n", __func__, desired_property, special);
    return (PKT_ERROR_KYC -20);
  }

  // using second address data
  int kyc_id2;
  if(!t_tradelistdb->checkAttestationReg(receiver,kyc_id2)){
    PrintToLog("%s(): rejected: second address (%s) kyc ckeck failed\n", __func__, receiver);
    return (PKT_ERROR_KYC -10);
  }

  if(!t_tradelistdb->kycPropertyMatch(property, kyc_id2)){
    PrintToLog("%s(): rejected: property %d can't be traded with this kyc for second address (%s)\n", __func__, property, receiver);
    return (PKT_ERROR_KYC -20);
  }

  if(!t_tradelistdb->kycPropertyMatch(desired_property, kyc_id2)){
    PrintToLog("%s(): rejected: property %d can't be traded with this kyc for second address (%s)\n", __func__, desired_property, receiver);
    return (PKT_ERROR_KYC -20);
  }

  if(block_forexpiry < block) {
      PrintToLog("%s(): rejected: tx expired (actual block: %d, expiry: %d\n", __func__, block , block_forexpiry);
      return (PKT_ERROR_CHANNELS -16);
  }

  // NOTE: this should be checked before somehow

  const int64_t fRemaining = chn.getRemaining(special, property);
  if (property > 0 && fRemaining < (int64_t) amount_forsale) {
      PrintToLog("%s(): rejected: address %s has insufficient balance of property %d [%s < %s] in channel %s\n",
              __func__,
              chn.getFirst(),
              property,
              FormatMP(property, fRemaining),
              FormatMP(property, amount_forsale),
              chn.getMultisig());

      return (PKT_ERROR_CHANNELS -19);
  }

  const int64_t sRemaining = chn.getRemaining(receiver, desired_property);
  if (desired_property > 0 && sRemaining < (int64_t) desired_value) {
      PrintToLog("%s(): rejected: address %s has insufficient balance of property %d [%s < %s] in channel %s\n",
              __func__,
              chn.getFirst(),
              desired_property,
              FormatMP(property, sRemaining),
              FormatMP(property, desired_value),
              chn.getMultisig());

      return (PKT_ERROR_CHANNELS -19);
  }

  // ------------------------------------------

  if (property > LTC && desired_property > LTC)
  {
      t_tradelistdb->recordNewInstantTrade(txid, chn.getMultisig(), chn.getFirst(), chn.getSecond(), property, amount_forsale, desired_property, desired_value, block, tx_idx);

      PrintToLog("%s(): sender: %s, receiver: %s, special: %s, multisig: %s, first: %s, second: %s,  property: %d, amount_forsale: %d, desired_property: %d, desired_value: %d, block: %d\n",__func__, sender, receiver, special, chn.getMultisig(), chn.getFirst(), chn.getSecond(), property, amount_forsale, desired_property, desired_value, block);

      // updating channel balance for each address
      assert(chn.updateChannelBal(special, property, -amount_forsale));
      assert(chn.updateChannelBal(receiver, desired_property, -desired_value));

      assert(update_tally_map(special, desired_property, desired_value, BALANCE));
      assert(update_tally_map(receiver, property, amount_forsale, BALANCE));

  }

  return rc;
}

/** Tx 111 */
int CMPTransaction::logicMath_Update_PNL()
{
  if (!IsTransactionTypeAllowed(block, type, version)) {
      PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
              __func__,
              type,
              version,
              property,
              block);
      return (PKT_ERROR_SP -22);
  }

  if (!IsPropertyIdValid(propertyId)) {
      PrintToLog("%s(): rejected: property %d does not exist\n", __func__, property);
      return (PKT_ERROR_CHANNELS -13);
  }


  // ------------------------------------------

  //logic for PNLS
  // assert(update_tally_map(sender, propertyId, -pnl_amount, CHANNEL_RESERVE));
  assert(update_tally_map(receiver, propertyId, pnl_amount, BALANCE));


  return 0;

}

/** Tx 112 */
int CMPTransaction::logicMath_Transfer()
{
    if (!IsTransactionTypeAllowed(block, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
              __func__,
              type,
              version,
              propertyId,
              block);
        return (PKT_ERROR_SP -22);
    }

    auto it = channels_Map.find(sender);

    if (it == channels_Map.end()) {
        PrintToLog("%s(): rejected: address doesn't belong to multisig channel \n", __func__);
        return (PKT_ERROR_CHANNELS -15);
    }

    Channel& chn = it->second;

    const std::string& address = (address_option) ? chn.getSecond(): chn.getFirst();

    // if receiver channel doesn't exist, create it
    if (!checkChannelAddress(receiver)) {
        createChannel(address, receiver, propertyId, 0, block, tx_idx);
    }

    auto itt = channels_Map.find(receiver);
    if (itt == channels_Map.end()) {
        PrintToLog("%s(): rejected: address doesn't belong to multisig channel \n", __func__);
        return (PKT_ERROR_CHANNELS -16);
    }

    Channel& nwChn =  itt->second;

    if(!chn.updateChannelBal(address, propertyId, -amount_transfered)){
        PrintToLog("%s(): withdrawal is not possible\n",__func__);
        return (PKT_ERROR_CHANNELS -17);
    }

    if(!nwChn.updateChannelBal(address, propertyId, amount_transfered)){
        PrintToLog("%s(): withdrawal is not possible\n",__func__);
        return (PKT_ERROR_CHANNELS -18);
    }

    // recordNewTransfer
    t_tradelistdb->recordNewTransfer(txid, sender, receiver, block, tx_idx);


  return 0;

}

/** Tx 113 */
int CMPTransaction::logicMath_Instant_LTC_Trade()
{
  int rc = 1;

  if (!IsTransactionTypeAllowed(block, type, version)) {
      PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
              __func__,
              type,
              version,
              property,
              block);
      return (PKT_ERROR_SP -22);
  }

  if (!IsPropertyIdValid(property)) {
      PrintToLog("%s(): rejected: property for sale %d does not exist\n", __func__, property);
      return (PKT_ERROR_CHANNELS -13);
  }

  std::string chnAddr;
  if(!t_tradelistdb->checkChannelRelation(receiver, chnAddr)){
        PrintToLog("%s(): seller addresses (%s) are not related with any channel\n", __func__, receiver);
        return (PKT_ERROR_CHANNELS -15);
  }

  if(block_forexpiry < block) {
      PrintToLog("%s(): rejected: tx expired (actual block: %d, expiry: %d\n", __func__, block , block_forexpiry);
      return (PKT_ERROR_CHANNELS -16);
  }

  auto it = channels_Map.find(chnAddr);
  if (it == channels_Map.end()){
      PrintToLog("%s(): rejected: channel not found\n", __func__);
      return (PKT_ERROR_CHANNELS -19);
  }

  return rc;
}

/** Tx 114 */
int CMPTransaction::logicMath_Contract_Instant()
{
    int rc = 0;

    if (!IsTransactionTypeAllowed(block, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted at block %d\n",
            __func__,
            type,
            version,
            block);
        return (PKT_ERROR_SP -22);
    }

    // PrintToLog("%s(): SENDER (multisig address): %s, SPECIAL REF (first address): %s,  RECEIVER (second address): %s\n",__func__, sender, special, receiver);

    Channel chn;
    auto it = channels_Map.find(sender);
    if(it != channels_Map.end()){
        chn = it->second;
    }

    if (sender.empty() || chn.getFirst().empty() || chn.getSecond().empty())
    {
        PrintToLog("%s(): rejected: some address doesn't belong to multisig channel\n", __func__);
        return (PKT_ERROR_CHANNELS -15);
    }

    CDInfo::Entry sp;
    if (!_my_cds->getCD(contractId, sp)) {
        PrintToLog("%s(): rejected: contractId doesn't exist (contractId: %d)\n", __func__, contractId);
        return (PKT_ERROR_CHANNELS -13);
    }

    if (block > sp.init_block + static_cast<int>(sp.blocks_until_expiration) || block < sp.init_block)
    {
         const int& initblock = sp.init_block;
         const int deadline = initblock + static_cast<int>(sp.blocks_until_expiration);
         PrintToLog("\nTrade out of deadline!!: actual block: %d, deadline: %d\n", initblock, deadline);
         return (PKT_ERROR_CHANNELS -16);
    }

    int kyc_id;

    if(!t_tradelistdb->checkAttestationReg(chn.getFirst(),kyc_id))
    {
        PrintToLog("%s(): rejected: kyc ckeck failed\n", __func__);
        return (PKT_ERROR_KYC -10);
    }

    if(!t_tradelistdb->kycContractMatch(contractId, kyc_id))
    {
       PrintToLog("%s(): rejected: contract %d can't be traded with this kyc\n", __func__, contractId);
       return (PKT_ERROR_KYC -20);
    }

    const int64_t marginRe = static_cast<int64_t>(sp.margin_requirement);
    const arith_uint256 amountTR = (ConvertTo256(instant_amount) * ConvertTo256(marginRe)) / (ConvertTo256(ileverage));
    const int64_t amountToReserve = ConvertTo64(amountTR);

    if(msc_debug_contract_instant_trade) PrintToLog("%s: AmountToReserve: %d\n", __func__, amountToReserve);

    if(msc_debug_contract_instant_trade) PrintToLog("%s: sender: %s, channel Address: %s\n", __func__, sender, chn.getMultisig());

    if (amountToReserve > 0)
    {

        if(!chn.updateChannelBal(chn.getFirst(), sp.collateral_currency, -amountToReserve)){
            if (msc_debug_contract_instant_trade) PrintToLog("%s: first Address: %s\n, has not enough collateral", __func__, chn.getFirst());
            return 0;
        }

        if(!chn.updateChannelBal(chn.getSecond(), sp.collateral_currency, -amountToReserve)){
            if (msc_debug_contract_instant_trade) PrintToLog("%s: second Address: %s\n, has not enough collateral", __func__, chn.getSecond());
            return 0;
        }

        assert(update_tally_map(chn.getFirst(), sp.collateral_currency, amountToReserve, CONTRACTDEX_RESERVE));
        if (msc_debug_contract_instant_trade) PrintToLog("%s(): first address reserve done\n", __func__);
        assert(update_tally_map(chn.getSecond(), sp.collateral_currency, amountToReserve, CONTRACTDEX_RESERVE));
        if (msc_debug_contract_instant_trade) PrintToLog("%s(): second address reserve done\n", __func__);

        mastercore::Instant_x_Trade(txid, itrading_action, chn.getMultisig(), chn.getFirst(), chn.getSecond(), contractId, instant_amount, price, sp.collateral_currency, sp.prop_type, block, tx_idx);

        // t_tradelistdb->recordNewInstContTrade(txid, receiver, sender, propertyId, amount_commited, price, block, tx_idx);

        if (msc_debug_contract_instant_trade)PrintToLog("%s(): End of Logic Instant Contract Trade\n\n",__func__);

    }


    return rc;
}

/** Tx 115 */
int CMPTransaction::logicMath_New_Id_Registration()
{
    if (!IsTransactionTypeAllowed(block, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
            __func__,
            type,
            version,
            property,
            block);
        return (PKT_ERROR_SP -22);
  }

  int kyc_id;

  if(t_tradelistdb->checkKYCRegister(sender,kyc_id)){
    PrintToLog("%s(): rejected: address is on kyc register yet\n", __func__);
    return (PKT_ERROR_KYC -10);
  }


  // ---------------------------------------
  if (msc_debug_new_id_registration) PrintToLog("%s(): channelAddres in register: %s \n",__func__,receiver);

  t_tradelistdb->recordNewIdRegister(txid, sender, company_name, website, block, tx_idx);

  return 0;
}

/** Tx 116 */
int CMPTransaction::logicMath_Update_Id_Registration()
{
    if (!IsTransactionTypeAllowed(block, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
            __func__,
            type,
            version,
            property,
            block);
        return (PKT_ERROR_SP -22);
  }

  // ---------------------------------------

  assert(t_tradelistdb->updateIdRegister(txid,sender, receiver,block, tx_idx));

  return 0;
}

/** Tx 117 */
int CMPTransaction::logicMath_DEx_Payment()
{
  int rc = 2;

  if (!IsTransactionTypeAllowed(block, type, version)) {
      PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
              __func__,
              type,
              version,
              property,
              block);
      return (PKT_ERROR_SP -22);
  }

  return rc;
}

/** Tx 118 */
int CMPTransaction::logicMath_Attestation()
{
    if (!IsTransactionTypeAllowed(block, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
            __func__,
            type,
            version,
            property,
            block);
        return (PKT_ERROR_SP -22);
    }

    int kyc_id;
    if(!t_tradelistdb->checkKYCRegister(sender,kyc_id))
    {
        kyc_id = KYC_0;
        if (sender != receiver)
        {
            PrintToLog("%s(): rejected: sender (%s) can't assign attestation to other address\n",
                __func__,
                sender);
            return (PKT_ERROR_METADEX -22);
        }

    }

    t_tradelistdb->recordNewAttestation(txid, sender, receiver, block, tx_idx, kyc_id);

    return 0;
}

/** Tx 119 */
int CMPTransaction::logicMath_Revoke_Attestation()
{
    if (!IsTransactionTypeAllowed(block, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted at block %d\n",
            __func__,
            type,
            version,
            block);
        return (PKT_ERROR_SP -22);
    }

    assert(t_tradelistdb->deleteAttestationReg(sender, receiver));

    return 0;
}

/** Tx 26 */
int CMPTransaction::logicMath_MetaDExCancelAll()
{
  if (!IsTransactionTypeAllowed(block, type, version)) {
    PrintToLog("%s(): rejected: type %d or version %d not permitted at block %d\n",
	       __func__,
	       type,
	       version,
	       block);
    return (PKT_ERROR_SP -22);
  }

  return (MetaDEx_CANCEL_EVERYTHING(txid, block, sender));
}

/** Tx 120 */
int CMPTransaction::logicMath_Close_Channel()
{
   if (!IsTransactionTypeAllowed(block, type, version))
   {
       PrintToLog("%s(): rejected: type %d or version %d not permitted at block %d\n",
	         __func__,
	         type,
	         version,
	         block);
       return (PKT_ERROR_SP -22);
    }

    if(!closeChannel(sender, receiver)){
        PrintToLog("%s(): unable to close the channel (%s)\n",__func__, receiver);
        return (PKT_ERROR_CHANNELS -21);
    }

    return 0;
}

/** Tx 121 */
int CMPTransaction::logicMath_SubmitNodeAddr()
{
   if (!IsTransactionTypeAllowed(block, type, version)) {
      PrintToLog("%s(): rejected: type %d or version %d not permitted at block %d\n",
              __func__,
              type,
              version,
              block);
      return (PKT_ERROR_SP -22);
  }

  // adding address
	nR.updateAddressStatus(receiver, false);

  return 0;
}

/** Tx 121 */
int CMPTransaction::logicMath_ClaimNodeReward()
{
   if (!IsTransactionTypeAllowed(block, type, version)) {
      PrintToLog("%s(): rejected: type %d or version %d not permitted at block %d\n",
              __func__,
              type,
              version,
              block);
      return (PKT_ERROR_SP -22);
  }

  // adding address
	if(!nR.isAddressIncluded(sender)) {
	    PrintToLog("%s(): rejected: address not found\n",__func__);
		  return (NODE_REWARD_ERROR -1);
	}

	nR.updateAddressStatus(sender, true);

  return 0;
}


int CMPTransaction::logicMath_SendDonation()
{
    if (!IsTransactionTypeAllowed(block, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_SP -22);
    }

    if (nValue <= 0 || MAX_INT_8_BYTES < nValue) {
        PrintToLog("%s(): rejected: value out of range or zero: %d", __func__, nValue);
        return (PKT_ERROR_SEND -23);
    }

    if (!IsPropertyIdValid(property)) {
        PrintToLog("%s(): rejected: property %d does not exist\n", __func__, property);
        return (PKT_ERROR_SEND -24);
    }

     if(property == TL_PROPERTY_VESTING){
         PrintToLog("%s(): rejected: property should not be vesting tokens (id = 3)\n", __func__);
         return (PKT_ERROR_SEND -26);
     }

    if(property != ALL)
    {
        int kyc_id;

        if(!t_tradelistdb->checkAttestationReg(sender,kyc_id)){
          PrintToLog("%s(): rejected: kyc ckeck for sender failed\n", __func__);
          return (PKT_ERROR_KYC -10);
        }

        if(!t_tradelistdb->kycPropertyMatch(property,kyc_id)){
          PrintToLog("%s(): rejected: property %d can't be traded with this kyc\n", __func__, property);
          return (PKT_ERROR_KYC -20);
        }


        if(!t_tradelistdb->checkAttestationReg(receiver,kyc_id)){
          PrintToLog("%s(): rejected: kyc ckeck for receiver failed\n", __func__);
          return (PKT_ERROR_KYC -10);
        }

        if(!t_tradelistdb->kycPropertyMatch(property,kyc_id)){
          PrintToLog("%s(): rejected: property %d can't be traded with this kyc\n", __func__, property);
          return (PKT_ERROR_KYC -20);
        }

    }

    int64_t nBalance = getMPbalance(sender, property, BALANCE);
    if (nBalance < (int64_t) nValue) {
        PrintToLog("%s(): rejected: sender %s has insufficient balance of property %d [%s < %s]\n",
                __func__,
                sender,
                property,
                FormatMP(property, nBalance),
                FormatMP(property, nValue));
        return (PKT_ERROR_SEND_DONATION - 21);
    }

    // ------------------------------------------

    // Move the tokens
    assert(update_tally_map(sender, property, -nValue, BALANCE));
    //update insurance here!
    g_fund->AccrueFees(property, nValue);

    return 0;
}

FutureContractObject getFutureContractObject(std::string identifier)
{
  LOCK(cs_register);
  
  const uint32_t nextCDID = _my_cds->peekNextContractID();
  for (uint32_t contractId = 1; contractId < nextCDID; contractId++)
  {
      CDInfo::Entry sp;
      if (_my_cds->getCD(contractId, sp))
	    {
	        if (sp.name == identifier )
	        {
                auto fco = FutureContractObject();
                fco.fco_denominator = sp.numerator;
	            fco.fco_denominator = sp.denominator;
	            fco.fco_blocks_until_expiration = sp.blocks_until_expiration;
	            fco.fco_notional_size = sp.notional_size;
	            fco.fco_collateral_currency = sp.collateral_currency;
	            fco.fco_margin_requirement = sp.margin_requirement;
	            fco.fco_name = sp.name;
	            fco.fco_issuer = sp.issuer;
	            fco.fco_init_block = sp.init_block;
                fco.fco_backup_address = sp.backup_address;
	            fco.fco_propertyId = contractId;
                fco.fco_prop_type = sp.prop_type;
                fco.fco_expirated = sp.expirated;
                fco.fco_quoted = sp.inverse_quoted;
                return fco;
          }
	        // } else if ( sp.isPegged() && sp.name == identifier ){
	        //     pt_fco->fco_denominator = sp.denominator;
	        //     pt_fco->fco_blocks_until_expiration = sp.blocks_until_expiration;
	        //     pt_fco->fco_notional_size = sp.notional_size;
	        //     pt_fco->fco_collateral_currency = sp.collateral_currency;
	        //     pt_fco->fco_margin_requirement = sp.margin_requirement;
	        //     pt_fco->fco_name = sp.name;
	        //     pt_fco->fco_subcategory = sp.subcategory;
	        //     pt_fco->fco_issuer = sp.issuer;
	        //     pt_fco->fco_init_block = sp.init_block;
	        //     pt_fco->fco_propertyId = propertyId;
	        // }
	    }
  }
  return FutureContractObject::Empty;
}

TokenDataByName getTokenDataByName(std::string identifier)
{
    LOCK(cs_tally);

    uint32_t nextSPID = _my_sps->peekNextSPID();
    for (uint32_t propertyId = 1; propertyId < nextSPID; propertyId++)
    {
        CMPSPInfo::Entry sp;
        if (_my_sps->getSP(propertyId, sp) && sp.name == identifier)
        {
            // pt_data->data_denominator = sp.denominator;
            // pt_data->data_blocks_until_expiration = sp.blocks_until_expiration;
            // pt_data->data_notional_size = sp.notional_size;
            // pt_data->data_collateral_currency = sp.collateral_currency;
            // pt_data->data_margin_requirement = sp.margin_requirement;
            TokenDataByName token;
            token.data_name = sp.name;
            token.data_subcategory = sp.subcategory;
            token.data_issuer = sp.issuer;
            token.data_init_block = sp.init_block;
            token.data_propertyId = propertyId;
            return token;
        }
    }
    return TokenDataByName::Empty;
}

TokenDataByName getTokenDataById(uint32_t propertyId)
{
    LOCK(cs_tally);

    CMPSPInfo::Entry sp;
    if (_my_sps->getSP(propertyId, sp))
    {
        // pt_data->data_denominator = sp.denominator;
        // pt_data->data_blocks_until_expiration = sp.blocks_until_expiration;
        // pt_data->data_notional_size = sp.notional_size;
        // pt_data->data_collateral_currency = sp.collateral_currency;
        // pt_data->data_margin_requirement = sp.margin_requirement;
        TokenDataByName token;
        token.data_name = sp.name;
        token.data_subcategory = sp.subcategory;
        token.data_issuer = sp.issuer;
        token.data_init_block = sp.init_block;
        token.data_propertyId = propertyId;
        return token;
	}
    return TokenDataByName::Empty;
}

/**********************************************************************/
/**Logic for Node Reward**/

void BlockClass::SendNodeReward(std::string sender)
{
    if(msc_debug_send_reward) PrintToLog("\nm_BlockInit = %d\t m_BockNow = %s\t sender = %s\n", m_BlockInit, m_BlockNow, sender);
    int64_t Reward = 0;

    if (m_BlockNow > m_BlockInit && m_BlockNow <= 100000)
    {
        double SpeedUp = 0.1*pow(CompoundRate, static_cast<double>(m_BlockNow - m_BlockInit));
        Reward = DoubleToInt64(SpeedUp);
        if (m_BlockNow == 100000)
	      {
	          mReward.lock();
	          RewardFirstI = Reward;
	          mReward.unlock();
	      }

        if(msc_debug_send_reward) PrintToLog("\nI1: Reward to Balance = %s\n", FormatDivisibleMP(Reward));

    } else if (m_BlockNow > 100000 && m_BlockNow <= 220000) {
        double SpeedDw = RewardFirstI*pow(DecayRate, static_cast<double>(m_BlockNow - (m_BlockInit+100000)));
        Reward = DoubleToInt64(SpeedDw);

        if (m_BlockNow == 220000)
	      {
	          mReward.lock();
	          RewardSecndI = Reward;
	          mReward.unlock();
	      }

        if(msc_debug_send_reward) PrintToLog("I2: \nReward to Balance = %s\n", FormatDivisibleMP(Reward));

    } else if (m_BlockNow > 220000) {
        double SpeedDw = RewardSecndI*pow(LongTailDecay, static_cast<double>(m_BlockNow - (m_BlockInit+220000)));
        Reward = LosingSatoshiLongTail(m_BlockNow, DoubleToInt64(SpeedDw));

        if(msc_debug_send_reward) PrintToLog("\nI3: Reward to Balance = %s\n", FormatDivisibleMP(Reward));
    }
}

int64_t LosingSatoshiLongTail(int BlockNow, int64_t Reward)
{
    int64_t RewardH = Reward;

    bool RBool1 = (BlockNow > 220000   && BlockNow <= 720000)   && BlockNow%2 == 0;
    bool RBool2 = (BlockNow > 720000   && BlockNow <= 1500000)  && BlockNow%3 == 0;
    bool RBool3 = (BlockNow > 1500000  && BlockNow <= 7500000)  && BlockNow%4 == 0;
    bool RBool4 = (BlockNow > 7500000  && BlockNow <= 15000000) && BlockNow%5 == 0;
    bool RBool5 = (BlockNow > 15000000 && BlockNow <= 30000000) && BlockNow%6 == 0;

    bool BoolReward = (((RBool1 || RBool2) || RBool3) || RBool4) || RBool5;
    if (BoolReward)
        RewardH -= SatoshiH;

    return RewardH;
}
/**********************************************************************/
