#include <tradelayer/rpcvalues.h>
#include <tradelayer/ce.h>
#include <tradelayer/createtx.h>
#include <tradelayer/log.h>
#include <tradelayer/parse_string.h>
#include <tradelayer/script.h>
#include <tradelayer/sp.h>
#include <tradelayer/tx.h>
#include <tradelayer/wallettxs.h>

#include <base58.h>
#include <core_io.h>
#include <primitives/transaction.h>
#include <pubkey.h>
#include <rpc/protocol.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <script/script.h>
#include <uint256.h>

#include <string>
#include <univalue.h>
#include <vector>

#include <boost/assign/list_of.hpp>

using mastercore::StrToInt64;

std::string ParseAddress(const UniValue& value)
{
    std::string address = value.get_str();
    CTxDestination dest = DecodeDestination(address);
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }

    return address;
}

std::string ParseAddressOrEmpty(const UniValue& value)
{
    if (value.isNull() || value.get_str().empty()) {
        return "";
    }
    return ParseAddress(value);
}

std::string ParseAddressOrWildcard(const UniValue& value)
{
    if (value.get_str() == "*") {
        return "*";
    }
    return ParseAddress(value);
}

uint32_t ParsePropertyId(const UniValue& value)
{
    int64_t propertyId = value.get_int64();
    if (propertyId < 0 || 4294967295LL < propertyId) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Property identifier is out of range");
    }
    return static_cast<uint32_t>(propertyId);
}

int64_t ParseAmount(const UniValue& value, bool isDivisible)
{
    int64_t amount = mastercore::StrToInt64(value.get_str(), isDivisible);
    if (amount < 1) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
    }
    return amount;
}

int64_t ParseAmount(const UniValue& value, int propertyType)
{
    bool fDivisible = (propertyType == 2);  // 1 = indivisible, 2 = divisible
    return ParseAmount(value, fDivisible);
}

uint8_t ParseEcosystem(const UniValue& value)
{
    int64_t ecosystem = value.get_int64();
    if (ecosystem < 1 || 2 < ecosystem) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid ecosystem (1 = main, 2 = test only)");
    }
    return static_cast<uint8_t>(ecosystem);
}

uint8_t ParsePermission(const UniValue& value)
{
    int64_t number = value.get_int64();
    if (number != 0 && number != 1) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid number (0 = false, 1 = true)");
    }
    return static_cast<uint8_t>(number);
}

uint64_t ParsePercent(const UniValue& value, bool isDivisible)
{
    int64_t amount = mastercore::StrToInt64(value.get_str(), isDivisible);
    PrintToLog("amount: %d\n",amount);
    if (amount <= 0 || 10000000000 < amount) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
    }
    return static_cast<uint64_t>(amount);
}

uint16_t ParsePropertyType(const UniValue& value)
{
    int64_t propertyType = value.get_int64();
    if (propertyType < 1 || 2 < propertyType) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid property type (1 = indivisible, 2 = divisible only)");
    }
    return static_cast<uint16_t>(propertyType);
}

uint32_t ParsePreviousPropertyId(const UniValue& value)
{
    int64_t previousId = value.get_int64();
    if (previousId != 0) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Property appends/replaces are not yet supported");
    }
    return static_cast<uint32_t>(previousId);
}

std::string ParseText(const UniValue& value)
{
  std::string text = value.get_str();
  if (text.size() > 255) {
    throw JSONRPCError(RPC_INVALID_PARAMETER, "Text must not be longer than 255 characters");
  }
  return text;
}

int64_t ParseDeadline(const UniValue& value)
{
    int64_t deadline = value.get_int64();
    if (deadline < 0) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Deadline must be positive");
    }
    return deadline;
}

uint8_t ParseEarlyBirdBonus(const UniValue& value)
{
    int64_t percentage = value.get_int64();
    if (percentage < 0 || 255 < percentage) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Early bird bonus must be in the range of 0-255 percent per week");
    }
    return static_cast<uint8_t>(percentage);
}

uint8_t ParseIssuerBonus(const UniValue& value)
{
    int64_t percentage = value.get_int64();
    if (percentage < 0 || 255 < percentage) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Bonus for issuer must be in the range of 0-255 percent");
    }
    return static_cast<uint8_t>(percentage);
}

CTransaction ParseTransaction(const UniValue& value)
{
    CMutableTransaction tx;
    if (value.isNull() || value.get_str().empty()) {
        return CTransaction(tx);
    }

    if (!DecodeHexTx(tx, value.get_str())) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Transaction deserialization failed");
    }

    return CTransaction(tx);
}

CMutableTransaction ParseMutableTransaction(const UniValue& value)
{
    CTransaction tx = ParseTransaction(value);
    return CMutableTransaction(tx);
}

CPubKey ParsePubKeyOrAddress(const UniValue& value)
{
    CPubKey pubKey;
    if (!mastercore::AddressToPubKey(value.get_str(), pubKey)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid redemption key or address");
    }
    return pubKey;
}

uint32_t ParseOutputIndex(const UniValue& value)
{
    int nOut = value.get_int();
    if (nOut < 0) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Vout index must be positive");
    }
    return static_cast<uint32_t>(nOut);
}

/** Parses previous transaction outputs. */
std::vector<PrevTxsEntry> ParsePrevTxs(const UniValue& value)
{
    UniValue prevTxs = value.get_array();

    std::vector<PrevTxsEntry> prevTxsParsed;
    prevTxsParsed.reserve(prevTxs.size());

    for (size_t i = 0; i < prevTxs.size(); ++i) {
        const UniValue& p = prevTxs[i];
        if (p.type() != UniValue::VOBJ) {
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "expected object with {\"txid\",\"vout\",\"scriptPubKey\",\"value\":n.nnnnnnnn}");
        }
        UniValue prevOut = p.get_obj();
        // RPCTypeCheck(prevOut, boost::assign::list_of(UniValue::VSTR)(UniValue::VNUM)(UniValue::VSTR)(UniValue::VNUM));
        //RPCTypeCheck(prevOut, boost::assign::map_list_of("txid", UniValue::VSTR)("vout", UniValue::VNUM)("scriptPubKey", UniValue::VSTR)("value", UniValue::VNUM));

        uint256 txid = ParseHashO(prevOut, "txid");
        UniValue outputIndex = find_value(prevOut, "vout");
        UniValue outputValue = find_value(prevOut, "value");
        std::vector<unsigned char> pkData(ParseHexO(prevOut, "scriptPubKey"));

        uint32_t nOut = ParseOutputIndex(outputIndex);
        int64_t nValue = AmountFromValue(outputValue);
        CScript scriptPubKey(pkData.begin(), pkData.end());

        PrevTxsEntry entry(txid, nOut, nValue, scriptPubKey);
        prevTxsParsed.push_back(entry);
    }

    return prevTxsParsed;
}

/** New things for Future Contracts */
int64_t ParseAmountContract(const UniValue& value)
{
  int64_t amount = mastercore::StrToInt64(value.get_str(), false);
  if (amount < 1) {
    throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
  }
  return amount;
}

uint64_t ParseLeverage(const UniValue& value)
{
    int64_t amount = mastercore::StrToInt64(value.get_str(), false);
    if (amount < 1 || 10 < amount) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Leverage out of range");
    }
    return static_cast<uint64_t>(amount);
}

uint32_t ParseNewValues(const UniValue& value)
{
  int64_t Nvalue = value.get_int64();
  if (Nvalue < 1 || 4294967295LL < Nvalue) {
    throw JSONRPCError(RPC_INVALID_PARAMETER, "Value is out of range");
  }
  return static_cast<uint32_t>(Nvalue);
}

uint32_t ParseAmount32t(const UniValue& value)
{
  int64_t amount = StrToInt64(value.getValStr(), true);
  if (amount <= 0) {
    throw JSONRPCError(RPC_TYPE_ERROR, "Amount should be positive");
  }
  return static_cast<uint32_t>(amount);
}

uint64_t ParseAmount64t(const UniValue& value)
{
  int64_t amount = StrToInt64(value.getValStr(), true);
  if (amount <= 0) {
    throw JSONRPCError(RPC_TYPE_ERROR, "Amount should be positive");
  }
  return static_cast<uint64_t>(amount);
}

uint64_t ParseEffectivePrice(const UniValue& value)
{
  int64_t effPrice = StrToInt64(value.getValStr(), true);
  if (effPrice <= 0) {
    throw JSONRPCError(RPC_TYPE_ERROR, "Price should be positive");
  }
  return effPrice;
}


uint64_t ParseEffectivePrice(const UniValue& value, uint32_t contractId)
{
  int64_t effPrice = StrToInt64(value.getValStr(), true);
  if (effPrice <= 0) {
    throw JSONRPCError(RPC_TYPE_ERROR, "Price should be positive");
  }

  LOCK(cs_tally);
  CMPSPInfo::Entry sp;
  if (!mastercore::_my_sps->getSP(contractId, sp)) {
    throw JSONRPCError(RPC_DATABASE_ERROR, "Failed to retrieve property");
  }

  return effPrice;
}

uint8_t ParseContractDexAction(const UniValue& value)
{
    int64_t action = value.get_int64();
    if (action < 1 || 2 < action) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid action (1=BUY, 2=SELL only)");
    }
    return static_cast<uint8_t>(action);
}

int64_t ParseDExFee(const UniValue& value)
{
    int64_t fee = StrToInt64(value.get_str(), true);  // BTC is divisible
    if (fee <= 0) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Mininmum accept fee must be positive");
    }
    return fee;
}

uint8_t ParseDExAction(const UniValue& value)
{
    int64_t action = value.get_int64();
    if (action <= 0 || 3 < action) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid action (1 = new, 2 = update, 3 = cancel only)");
    }
    return static_cast<uint8_t>(action);
}

uint8_t ParseDExPaymentWindow(const UniValue& value)
{
  int64_t blocks = value.get_int64();
  if (blocks < 1 || 255 < blocks) {
      throw JSONRPCError(RPC_TYPE_ERROR, "Payment window must be within 1-255 blocks");
  }
  return static_cast<uint8_t>(blocks);
}

uint32_t ParseContractType(const UniValue& value)
{
  int64_t Nvalue = value.get_int64();

  if (Nvalue < 1 || 4294967295LL < Nvalue) {
    throw JSONRPCError(RPC_INVALID_PARAMETER, "Value is out of range");
  }
  return static_cast<uint32_t>(Nvalue);

}

uint32_t ParseContractDen(const UniValue& value)
{
    int64_t Nvalue = value.get_int64();
    if (Nvalue != 1 && Nvalue != 2 && Nvalue != 3 && Nvalue != 4 && Nvalue != 5 && Nvalue != 6) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "denomination no valid");
    }

    return static_cast<uint32_t>(Nvalue);
}

uint8_t ParseBinary(const UniValue& value)
{
    int64_t action = value.get_int64();
    if (action != 0 && action != 1) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid action (only 0 or 1)");
    }
    return static_cast<uint8_t>(action);
}

std::vector<int> ParseArray(const UniValue& value)
{
    UniValue kycOptions(UniValue::VARR);
    if (value.isNull())
        throw JSONRPCError(RPC_TYPE_ERROR, "array is empty");

    kycOptions = value.get_array();

    std::vector<int> numbers;

    for (unsigned int idx = 0; idx < kycOptions.size(); idx++)
    {
            const UniValue& elem = kycOptions[idx];
            const int num = elem.isNum() ? elem.get_int() : atoi(elem.get_str());
            numbers.push_back(num);
            PrintToLog("%s(): num: %d\n",__func__, num);
    }

    return numbers;
}

std::string ParseHash(const UniValue& value)
{
     uint256 result = ParseHashV(value, "txid");
     return result.ToString();
}

uint32_t ParseNameOrId(const UniValue& value)
{
    int64_t amount = mastercore::StrToInt64(value.get_str(), false);

    PrintToLog("%s(): amount: %d\n",__func__, amount);
    
    if (amount != 0)
    {
        uint32_t am = static_cast<uint32_t>(amount);
        CDInfo::Entry cd;
        if (!mastercore::_my_cds->getCD(am, cd)) {
            throw JSONRPCError(RPC_DATABASE_ERROR, "Contract Id not found");
        }

        return am;
    }

    uint32_t contractId = getFutureContractObject(value.get_str()).fco_propertyId;

    if (contractId == 0) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Contract not found!");
    }

    return contractId;
}
