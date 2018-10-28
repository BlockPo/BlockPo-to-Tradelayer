#ifndef OMNICORE_CREATEPAYLOAD_H
#define OMNICORE_CREATEPAYLOAD_H

#include <string>
#include <vector>
#include <stdint.h>

std::vector<unsigned char> CreatePayload_SimpleSend(uint32_t propertyId, uint64_t amount);
std::vector<unsigned char> CreatePayload_SendAll(uint8_t ecosystem);
std::vector<unsigned char> CreatePayload_DExSell(uint32_t propertyId, uint64_t amountForSale, uint64_t amountDesired, uint8_t timeLimit, uint64_t minFee, uint8_t subAction);
std::vector<unsigned char> CreatePayload_DEx(uint32_t propertyId, uint64_t amount, uint64_t price, uint8_t timeLimit, uint64_t minFee, uint8_t subAction);
std::vector<unsigned char> CreatePayload_DExAccept(uint32_t propertyId, uint64_t amount);
std::vector<unsigned char> CreatePayload_IssuanceFixed(uint8_t ecosystem, uint16_t propertyType, uint32_t previousPropertyId, std::string name, std::string url,
                                                       std::string data, uint64_t amount);
std::vector<unsigned char> CreatePayload_IssuanceVariable(uint8_t ecosystem, uint16_t propertyType, uint32_t previousPropertyId,
                                                          std::string name, std::string url, std::string data, uint32_t propertyIdDesired,
                                                          uint64_t amountPerUnit, uint64_t deadline, uint8_t earlyBonus, uint8_t issuerPercentage);
std::vector<unsigned char> CreatePayload_IssuanceManaged(uint8_t ecosystem, uint16_t propertyType, uint32_t previousPropertyId,
                                                       std::string name, std::string url, std::string data);
std::vector<unsigned char> CreatePayload_CloseCrowdsale(uint32_t propertyId);
std::vector<unsigned char> CreatePayload_Grant(uint32_t propertyId, uint64_t amount);
std::vector<unsigned char> CreatePayload_Revoke(uint32_t propertyId, uint64_t amount);
std::vector<unsigned char> CreatePayload_ChangeIssuer(uint32_t propertyId);
std::vector<unsigned char> CreatePayload_OmniCoreAlert(uint16_t alertType, uint32_t expiryValue, const std::string& alertMessage);
std::vector<unsigned char> CreatePayload_DeactivateFeature(uint16_t featureId);
std::vector<unsigned char> CreatePayload_ActivateFeature(uint16_t featureId, uint32_t activationBlock, uint32_t minClientVersion);
std::vector<unsigned char> CreatePayload_CreateContract(uint8_t ecosystem, uint32_t denomType, std::string name, uint32_t blocks_until_expiration, uint32_t notional_size, uint32_t collateral_currency, uint32_t margin_requirement);
std::vector<unsigned char> CreatePayload_ContractDexTrade(uint32_t propertyIdForSale, uint64_t amountForSale, uint64_t effective_price, uint8_t trading_action);
std::vector<unsigned char> CreatePayload_ContractDexCancelEcosystem(uint8_t ecosystem, uint32_t contractId);
std::vector<unsigned char> CreatePayload_ContractDexCancelOrderByTxId(int block, unsigned int idx);
std::vector<unsigned char> CreatePayload_ContractDexClosePosition(uint8_t ecosystem, uint32_t contractId);
std::vector<unsigned char> CreatePayload_ContractDexCancelPrice(uint32_t propertyIdForSale, uint64_t amountForSale, uint32_t propertyIdDesired, uint64_t amountDesired, uint64_t effective_price, uint8_t trading_action);
std::vector<unsigned char> CreatePayload_IssuancePegged(uint8_t ecosystem, uint16_t propertyType, uint32_t previousPropertyId, std::string name, uint32_t propertyId, uint32_t contractId, uint64_t amount);
std::vector<unsigned char> CreatePayload_RedemptionPegged(uint32_t propertyId, uint32_t contractId, uint64_t amount);
std::vector<unsigned char> CreatePayload_SendPeggedCurrency(uint32_t propertyId, uint64_t amount);
std::vector<unsigned char> CreatePayload_MetaDExTrade(uint32_t propertyIdForSale, uint64_t amountForSale, uint32_t propertyIdDesired, uint64_t amountDesired);
std::vector<unsigned char> CreatePayload_MetaDExCancelPrice(uint32_t propertyIdForSale, uint64_t amountForSale, uint32_t propertyIdDesired, uint64_t amountDesired);
std::vector<unsigned char> CreatePayload_MetaDExCancelPair(uint32_t propertyIdForSale, uint32_t propertyIdDesired);
std::vector<unsigned char> CreatePayload_MetaDExCancelEcosystem(uint8_t ecosystem);

#endif // OMNICORE_CREATEPAYLOAD_H
