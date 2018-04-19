#include "omnicore/rpcvalues.h"

#include "omnicore/createtx.h"
#include "omnicore/parse_string.h"
#include "omnicore/wallettxs.h"

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
    CBitcoinAddress address(value.get_str());
    if (!address.IsValid()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }
    return address.ToString();
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
    CTransaction tx;
    if (value.isNull() || value.get_str().empty()) {
        return tx;
    }
    if (!DecodeHexTx(tx, value.get_str())) {
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
