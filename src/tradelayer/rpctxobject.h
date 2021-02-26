#ifndef TRADELAYER_RPCTXOBJECT_H
#define TRADELAYER_RPCTXOBJECT_H

#include <univalue.h>

#include <string>

class uint256;
class CMPTransaction;
class CTransaction;

int populateRPCTransactionObject(const uint256& txid, UniValue& txobj, std::string filterAddress = "", bool extendedDetails = false, std::string extendedDetailsFilter = "");
int populateRPCTransactionObject(const CTransaction& tx, const uint256& blockHash, UniValue& txobj, std::string filterAddress = "", bool extendedDetails = false, std::string extendedDetailsFilter = "", int blockHeight = 0);

bool populateRPCTypeInfo(CMPTransaction& mp_obj, UniValue& txobj, uint32_t txType, bool extendedDetails, std::string extendedDetailsFilter);

void populateRPCTypeSimpleSend(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeSendAll(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeCreatePropertyFixed(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeCreatePropertyVariable(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeCreatePropertyManual(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeCloseCrowdsale(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeGrant(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeRevoke(CMPTransaction& tlOobj, UniValue& txobj);
void populateRPCTypeChangeIssuer(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeActivation(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeCreateContract(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeVestingTokens(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeCreateOracle(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeMetaDExTrade(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeContractDexTrade(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeContractDexCancelEcosystem(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeCreatePeggedCurrency(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeSendPeggedCurrency(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeRedemptionPegged(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeContractDexClosePosition(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeContractDex_Cancel_Orders_By_Block(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeTradeOffer(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeDExBuy(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeAcceptOfferBTC(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeChange_OracleRef(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeSet_Oracle(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeOracleBackup(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeCloseOracle(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeCommitChannel(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeWithdrawal_FromChannel(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeInstant_Trade(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeTransfer(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeCreate_Channel(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeContract_Instant(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeNew_Id_Registration(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeUpdate_Id_Registration(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeDEx_Payment(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeInstant_LTC_Trade(CMPTransaction& tlObj, UniValue& txobj);
void populateRPCTypeClose_Channel(CMPTransaction& tlObj, UniValue& txobj);

int populateRPCSendAllSubSends(const uint256& txid, UniValue& subSends);

bool showRefForTx(uint32_t txType);

#endif // TRADELAYER_RPCTXOBJECT_H
