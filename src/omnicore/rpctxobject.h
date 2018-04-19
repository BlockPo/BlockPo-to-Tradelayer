#ifndef OMNICORE_RPCTXOBJECT_H
#define OMNICORE_RPCTXOBJECT_H

#include <univalue.h>

#include <string>

class uint256;
class CMPTransaction;
class CTransaction;

int populateRPCTransactionObject(const uint256& txid, UniValue& txobj, std::string filterAddress = "", bool extendedDetails = false, std::string extendedDetailsFilter = "");
int populateRPCTransactionObject(const CTransaction& tx, const uint256& blockHash, UniValue& txobj, std::string filterAddress = "", bool extendedDetails = false, std::string extendedDetailsFilter = "", int blockHeight = 0);

void populateRPCTypeInfo(CMPTransaction& mp_obj, UniValue& txobj, uint32_t txType, bool extendedDetails, std::string extendedDetailsFilter);

void populateRPCTypeSimpleSend(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeSendAll(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeCreatePropertyFixed(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeCreatePropertyVariable(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeCreatePropertyManual(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeCloseCrowdsale(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeGrant(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeRevoke(CMPTransaction& omniOobj, UniValue& txobj);
void populateRPCTypeChangeIssuer(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeActivation(CMPTransaction& omniObj, UniValue& txobj);

int populateRPCSendAllSubSends(const uint256& txid, UniValue& subSends);

bool showRefForTx(uint32_t txType);

#endif // OMNICORE_RPCTXOBJECT_H
