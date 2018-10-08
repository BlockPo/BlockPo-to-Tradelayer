#ifndef OMNICORE_CREATEPAYLOAD_H
#define OMNICORE_CREATEPAYLOAD_H

#include <string>
#include <vector>
#include <stdint.h>

std::vector<unsigned char> CreatePayload_SimpleSend(uint32_t propertyId, uint64_t amount);
std::vector<unsigned char> CreatePayload_SendAll(uint8_t ecosystem);
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

#endif // OMNICORE_CREATEPAYLOAD_H
