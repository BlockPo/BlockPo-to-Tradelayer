#include "omnicore/rpcvalues.h"

#include "omnicore/createtx.h"
#include "omnicore/parse_string.h"
#include "omnicore/wallettxs.h"
#include "omnicore/log.h"
#include "omnicore/sp.h"
#include "base58.h"
#include "core_io.h"
#include "primitives/transaction.h"
#include "pubkey.h"
#include "rpc/protocol.h"
#include "rpc/server.h"
#include "script/script.h"
#include "uint256.h"

#include <univalue.h>

#include <boost/assign/list_of.hpp>
#include <boost/foreach.hpp>

#include <string>
#include <vector>

using mastercore::StrToInt64;

std::string ParseAddress(const UniValue& value)
{
    CTxDestination dest = DecodeDestination(value.get_str());
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }

    return EncodeDestination(dest);
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
    if (propertyId < 1 || 4294967295LL < propertyId) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Property identifier is out of range");
    }
    return static_cast<uint32_t>(propertyId);
}

int64_t ParseAmount(const UniValue& value, bool isDivisible)
{
    int64_t amount = mastercore::StrToInt64(value.get_str(), isDivisible);
    if (amount < 1) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount ???");
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
    const CTransaction tx;
    CMutableTransaction mutableTx(tx);
    if (value.isNull() || value.get_str().empty()) {
        return tx;
    }

    if (!DecodeHexTx(mutableTx, value.get_str())) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Transaction deserialization failed");
    }
    return tx;
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
        RPCTypeCheck(prevOut, boost::assign::list_of(UniValue::VSTR)(UniValue::VNUM)(UniValue::VSTR)(UniValue::VNUM));
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
  int64_t amount = mastercore::StrToInt64(value.get_str(), true);
  if (amount < 1) {
    throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
  }
  return amount;
}

int64_t ParseAmountContract(const UniValue& value, int propertyType)
{
  bool fContract = propertyType == 3;
  return ParseAmountContract(value, fContract);
}

uint32_t ParseNewValues(const UniValue& value)
{
  int64_t Nvalue = value.get_int64();
  if (Nvalue < 1 || 4294967295LL < Nvalue) {
    throw JSONRPCError(RPC_INVALID_PARAMETER, "Value is out of range");
  }
  return static_cast<uint32_t>(Nvalue);
}

uint64_t ParseEffectivePrice(const UniValue& value, uint32_t contractId)
{
  int64_t effPrice = StrToInt64(value.getValStr(), true);  
  if (effPrice < 0) {
    throw JSONRPCError(RPC_TYPE_ERROR, "Price should be positive");
  }
  
  LOCK(cs_tally);
  CMPSPInfo::Entry sp;
  if (!mastercore::_my_sps->getSP(contractId, sp)) {
    throw JSONRPCError(RPC_DATABASE_ERROR, "Failed to retrieve property");
  }
  
  PrintToConsole("effPrice: %d\n",effPrice);
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
    if (fee < 0) {
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

    if (Nvalue != 1 && Nvalue != 2) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "option no valid");
    }

    if(Nvalue == 1){
        return weekly;
    } else {
        return monthly;
    }

    return 0;
}

uint32_t ParseContractDen(const UniValue& value)
{
    int64_t Nvalue = value.get_int64();
    PrintToConsole(" Nvalue: %d\n",Nvalue);
    if (Nvalue != 1 && Nvalue != 2 && Nvalue != 3 && Nvalue != 4 && Nvalue != 5 && Nvalue != 6) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "denomination no valid");
    }

    return static_cast<uint32_t>(Nvalue);
}
