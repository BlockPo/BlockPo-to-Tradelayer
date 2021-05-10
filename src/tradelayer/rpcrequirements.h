#ifndef TRADELAYER_RPCREQUIREMENTS_H
#define TRADELAYER_RPCREQUIREMENTS_H

#include <stdint.h>
#include <string>

void RequireBalance(const std::string& address, uint32_t propertyId, int64_t amount);
void RequirePosition(const std::string& address, uint32_t contractId);
void RequirePrimaryToken(uint32_t propertyId);
void RequirePropertyName(const std::string& name);
void RequirePeggedSaneName(std::string& name);
void RequireExistingProperty(uint32_t propertyId);
void RequireDifferentIds(uint32_t propertyId, uint32_t otherId);
void RequireManagedProperty(uint32_t propertyId);
void RequireTokenIssuer(const std::string& address, uint32_t propertyId);
void RequireSaneReferenceAmount(int64_t amount);
void RequireHeightInChain(int blockHeight);

/*New things for contracts *///////////////////////////////////////////////////
void RequireForPegged(const std::string& address, uint32_t propertyId, uint32_t contractId, uint64_t amount);
void RequireNotVesting(uint32_t propertyId);
void RequireNotContract(uint32_t propertyId);
void RequireContract(uint32_t propertyId);
void RequireContract(std::string name_contract);
void RequireOracleContract(uint32_t contractId);
void RequireNoOrders(std::string sender, uint32_t propertyId);
void RequireAssociation(uint32_t propertyId,uint32_t contractId); // origin contract for pegged
void RequirePeggedCurrency(uint32_t propertyId);
void RequireCollateral(const std::string& address, std::string name_traded, int64_t amount, uint64_t leverage);
void RequireMatchingDExOffer(const std::string& address, uint32_t propertyId);
void RequireNoOtherDExOffer(const std::string& address, uint32_t propertyId);
void RequireContractOrder(std::string& fromAddress, uint32_t contractId);
void RequireContractTxId(std::string& txid);
void RequireSaneDExPaymentWindow(const std::string& address, uint32_t propertyId);
void RequireSaneDExFee(const std::string& address, uint32_t propertyId);
void RequireSaneName(std::string& name);
void RequireDifferentAddrs(const std::string& oracleAddress, const std::string& backupAddress);
void RequireShort(std::string& fromAddress, uint32_t contractId, uint64_t amount);
void RequireFeatureActivated(const uint16_t feature);
void RequireAmountForFee(const std::string& address, uint32_t propertyId, int64_t amount);
void RequireBlockHeight(const int& block);
////////////////////////////////////////////////////////////////////////////////


// TODO:
// Checks for MetaDEx orders for cancel operations


#endif // TRADELAYER_RPCREQUIREMENTS_H
