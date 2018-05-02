#ifndef OMNICORE_RPCREQUIREMENTS_H
#define OMNICORE_RPCREQUIREMENTS_H

#include <stdint.h>
#include <string>

void RequireBalance(const std::string& address, uint32_t propertyId, int64_t amount);
void RequirePrimaryToken(uint32_t propertyId);
void RequirePropertyName(const std::string& name);
void RequireExistingProperty(uint32_t propertyId);
void RequireSameEcosystem(uint32_t propertyId, uint32_t otherId);
void RequireDifferentIds(uint32_t propertyId, uint32_t otherId);
void RequireCrowdsale(uint32_t propertyId);
void RequireActiveCrowdsale(uint32_t propertyId);
void RequireManagedProperty(uint32_t propertyId);
void RequireTokenIssuer(const std::string& address, uint32_t propertyId);
void RequireSaneReferenceAmount(int64_t amount);
void RequireHeightInChain(int blockHeight);

/*New things for contracts *///////////////////////////////////////////////////
void RequireForPegged(const std::string& address, uint32_t propertyId, uint32_t contractId, uint64_t amount);
void RequireNotContract(uint32_t propertyId);
void RequireContract(uint32_t propertyId);
void RequirePeggedCurrency(uint32_t propertyId);
////////////////////////////////////////////////////////////////////////////////


// TODO:
// Checks for MetaDEx orders for cancel operations


#endif // OMNICORE_RPCREQUIREMENTS_H
