#ifndef TRADELAYER_RPCTXOBJECT_H
#define TRADELAYER_RPCTXOBJECT_H

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
void populateRPCTypeCreateContract(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeVestingTokens(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeCreateOracle(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeMetaDExTrade(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeContractDexTrade(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeContractDexCancelEcosystem(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeCreatePeggedCurrency(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeSendPeggedCurrency(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeRedemptionPegged(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeContractDexClosePosition(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeContractDex_Cancel_Orders_By_Block(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeTradeOffer(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeDExBuy(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeAcceptOfferBTC(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeChange_OracleRef(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeSet_Oracle(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeOracleBackup(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeCloseOracle(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeCommitChannel(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeWithdrawal_FromChannel(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeInstant_Trade(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeTransfer(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeCreate_Channel(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeContract_Instant(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeNew_Id_Registration(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeUpdate_Id_Registration(CMPTransaction& omniObj, UniValue& txobj);
void populateRPCTypeDEx_Payment(CMPTransaction& omniObj, UniValue& txobj);


int populateRPCSendAllSubSends(const uint256& txid, UniValue& subSends);

bool showRefForTx(uint32_t txType);

#endif // TRADELAYER_RPCTXOBJECT_H
