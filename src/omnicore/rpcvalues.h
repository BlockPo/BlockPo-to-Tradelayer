#ifndef OMNICORE_RPCVALUES_H
#define OMNICORE_RPCVALUES_H

class CPubKey;
class CTransaction;
struct CMutableTransaction;
struct PrevTxsEntry;

#include <univalue.h>

#include <stdint.h>
#include <string>
#include <vector>

const uint32_t weekly = 100; //4032;   blocks: 7(days) * 24 (hours) * 60 (minutes) / 2.5 (minutes)
const uint32_t monthly = 17280;

std::string ParseAddress(const UniValue& value);
std::string ParseAddressOrEmpty(const UniValue& value);
std::string ParseAddressOrWildcard(const UniValue& value);
uint32_t ParsePropertyId(const UniValue& value);
int64_t ParseAmount(const UniValue& value, bool isDivisible);
int64_t ParseAmount(const UniValue& value, int propertyType);
uint8_t ParseEcosystem(const UniValue& value);
uint16_t ParsePropertyType(const UniValue& value);
uint32_t ParsePreviousPropertyId(const UniValue& value);
std::string ParseText(const UniValue& value);
int64_t ParseDeadline(const UniValue& value);
uint8_t ParseEarlyBirdBonus(const UniValue& value);
uint8_t ParseIssuerBonus(const UniValue& value);
CTransaction ParseTransaction(const UniValue& value);
CMutableTransaction ParseMutableTransaction(const UniValue& value);
CPubKey ParsePubKeyOrAddress(const UniValue& value);
uint32_t ParseOutputIndex(const UniValue& value);
/** Parses previous transaction outputs. */
std::vector<PrevTxsEntry> ParsePrevTxs(const UniValue& value);
int64_t ParseAmountContract(const UniValue& value, int propertyType);
int64_t ParseAmountContract(const UniValue& value);
uint32_t ParseNewValues(const UniValue& value);
uint32_t ParseContractType(const UniValue& value);
uint32_t ParseContractDen(const UniValue& value);
uint64_t ParseEffectivePrice(const UniValue& value, uint32_t contractId);
uint8_t ParseContractDexAction(const UniValue& value);
uint8_t ParseDExPaymentWindow(const UniValue& value);
int64_t ParseDExFee(const UniValue& value);
uint8_t ParseDExAction(const UniValue& value);

#endif // OMNICORE_RPCVALUES_H
