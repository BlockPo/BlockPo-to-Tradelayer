// This file serves to provide payload creation functions.

#include "omnicore/createpayload.h"
#include "omnicore/convert.h"
#include "omnicore/varint.h"

#include "tinyformat.h"

#include <stdint.h>
#include <string>
#include <vector>

/**
 * Pushes bytes to the end of a vector.
 */
#define PUSH_BACK_BYTES(vector, value)\
    vector.insert(vector.end(), reinterpret_cast<unsigned char *>(&(value)),\
    reinterpret_cast<unsigned char *>(&(value)) + sizeof((value)));

/**
 * Pushes bytes to the end of a vector based on a pointer.
 */
#define PUSH_BACK_BYTES_PTR(vector, ptr, size)\
    vector.insert(vector.end(), reinterpret_cast<unsigned char *>((ptr)),\
    reinterpret_cast<unsigned char *>((ptr)) + (size));


std::vector<unsigned char> CreatePayload_SimpleSend(uint32_t propertyId, uint64_t amount)
{
    std::vector<unsigned char> payload;

    uint64_t messageType = 0;
    uint64_t messageVer = 0;

    std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);
    std::vector<uint8_t> vecPropertyId = CompressInteger((uint64_t)propertyId);
    std::vector<uint8_t> vecAmount = CompressInteger(amount);

    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
    payload.insert(payload.end(), vecPropertyId.begin(), vecPropertyId.end());
    payload.insert(payload.end(), vecAmount.begin(), vecAmount.end());

    return payload;
}

std::vector<unsigned char> CreatePayload_SendAll(uint8_t ecosystem)
{
    std::vector<unsigned char> payload;

    uint64_t messageType = 4;
    uint64_t messageVer = 0;

    std::vector<uint8_t> vecMessageType = CompressInteger(messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger(messageVer);
    std::vector<uint8_t> vecEcosystem = CompressInteger(ecosystem);

    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
    payload.insert(payload.end(), vecEcosystem.begin(), vecEcosystem.end());

    return payload;
}

std::vector<unsigned char> CreatePayload_IssuanceFixed(uint8_t ecosystem, uint16_t propertyType, uint32_t previousPropertyId, std::string name,
                                                       std::string url, std::string data, uint64_t amount)
{
    std::vector<unsigned char> payload;

    uint64_t messageType = 50;
    uint64_t messageVer = 0;

    std::vector<uint8_t> vecMessageType = CompressInteger(messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger(messageVer);
    std::vector<uint8_t> vecPropertyType = CompressInteger(propertyType);
    std::vector<uint8_t> vecPrevPropertyId = CompressInteger(previousPropertyId);
    std::vector<uint8_t> vecAmount = CompressInteger(amount);

    if (name.size() > 255) name = name.substr(0,255);
    if (url.size() > 255) url = url.substr(0,255);
    if (data.size() > 255) data = data.substr(0,255);

    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
    PUSH_BACK_BYTES(payload, ecosystem);
    payload.insert(payload.end(), vecPropertyType.begin(), vecPropertyType.end());
    payload.insert(payload.end(), vecPrevPropertyId.begin(), vecPrevPropertyId.end());
    payload.insert(payload.end(), name.begin(), name.end());
    payload.push_back('\0');
    payload.insert(payload.end(), url.begin(), url.end());
    payload.push_back('\0');
    payload.insert(payload.end(), data.begin(), data.end());
    payload.push_back('\0');
    payload.insert(payload.end(), vecAmount.begin(), vecAmount.end());

    return payload;
}

std::vector<unsigned char> CreatePayload_IssuanceVariable(uint8_t ecosystem, uint16_t propertyType, uint32_t previousPropertyId,
                                                          std::string name, std::string url, std::string data, uint32_t propertyIdDesired,
                                                          uint64_t amountPerUnit, uint64_t deadline, uint8_t earlyBonus, uint8_t issuerPercentage)
{
    std::vector<unsigned char> payload;

    uint64_t messageType = 51;
    uint64_t messageVer = 0;

    std::vector<uint8_t> vecMessageType = CompressInteger(messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger(messageVer);
    std::vector<uint8_t> vecPropertyType = CompressInteger(propertyType);
    std::vector<uint8_t> vecPrevPropertyId = CompressInteger(previousPropertyId);
    std::vector<uint8_t> vecPropertyIdDesired = CompressInteger(propertyIdDesired);
    std::vector<uint8_t> vecAmountPerUnit = CompressInteger(amountPerUnit);
    std::vector<uint8_t> vecDeadline = CompressInteger(deadline);

    if (name.size() > 255) name = name.substr(0,255);
    if (url.size() > 255) url = url.substr(0,255);
    if (data.size() > 255) data = data.substr(0,255);

    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
    PUSH_BACK_BYTES(payload, ecosystem);
    payload.insert(payload.end(), vecPropertyType.begin(), vecPropertyType.end());
    payload.insert(payload.end(), vecPrevPropertyId.begin(), vecPrevPropertyId.end());
    payload.insert(payload.end(), name.begin(), name.end());
    payload.push_back('\0');
    payload.insert(payload.end(), url.begin(), url.end());
    payload.push_back('\0');
    payload.insert(payload.end(), data.begin(), data.end());
    payload.push_back('\0');
    payload.insert(payload.end(), vecPropertyIdDesired.begin(), vecPropertyIdDesired.end());
    payload.insert(payload.end(), vecAmountPerUnit.begin(), vecAmountPerUnit.end());
    payload.insert(payload.end(), vecDeadline.begin(), vecDeadline.end());
    PUSH_BACK_BYTES(payload, earlyBonus);
    PUSH_BACK_BYTES(payload, issuerPercentage);

    return payload;
}

std::vector<unsigned char> CreatePayload_IssuanceManaged(uint8_t ecosystem, uint16_t propertyType, uint32_t previousPropertyId,
                                                       std::string name, std::string url, std::string data)
{
    std::vector<unsigned char> payload;

    uint64_t messageType = 54;
    uint64_t messageVer = 0;

    std::vector<uint8_t> vecMessageType = CompressInteger(messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger(messageVer);
    std::vector<uint8_t> vecPropertyType = CompressInteger(propertyType);
    std::vector<uint8_t> vecPrevPropertyId = CompressInteger(previousPropertyId);

    if (name.size() > 255) name = name.substr(0,255);
    if (url.size() > 255) url = url.substr(0,255);
    if (data.size() > 255) data = data.substr(0,255);

    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
    PUSH_BACK_BYTES(payload, ecosystem);
    payload.insert(payload.end(), vecPropertyType.begin(), vecPropertyType.end());
    payload.insert(payload.end(), vecPrevPropertyId.begin(), vecPrevPropertyId.end());
    payload.insert(payload.end(), name.begin(), name.end());
    payload.push_back('\0');
    payload.insert(payload.end(), url.begin(), url.end());
    payload.push_back('\0');
    payload.insert(payload.end(), data.begin(), data.end());
    payload.push_back('\0');

    return payload;
}

std::vector<unsigned char> CreatePayload_CloseCrowdsale(uint32_t propertyId)
{
    std::vector<unsigned char> payload;

    uint64_t messageType = 53;
    uint64_t messageVer = 0;

    std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);
    std::vector<uint8_t> vecPropertyId = CompressInteger((uint64_t)propertyId);

    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
    payload.insert(payload.end(), vecPropertyId.begin(), vecPropertyId.end());

    return payload;
}

std::vector<unsigned char> CreatePayload_Grant(uint32_t propertyId, uint64_t amount)
{
    std::vector<unsigned char> payload;

    uint64_t messageType = 55;
    uint64_t messageVer = 0;

    std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);
    std::vector<uint8_t> vecPropertyId = CompressInteger((uint64_t)propertyId);
    std::vector<uint8_t> vecAmount = CompressInteger((uint64_t)amount);

    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
    payload.insert(payload.end(), vecPropertyId.begin(), vecPropertyId.end());
    payload.insert(payload.end(), vecAmount.begin(), vecAmount.end());

    return payload;
}


std::vector<unsigned char> CreatePayload_Revoke(uint32_t propertyId, uint64_t amount)
{
    std::vector<unsigned char> payload;

    uint64_t messageType = 56;
    uint64_t messageVer = 0;

    std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);
    std::vector<uint8_t> vecPropertyId = CompressInteger((uint64_t)propertyId);
    std::vector<uint8_t> vecAmount = CompressInteger((uint64_t)amount);

    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
    payload.insert(payload.end(), vecPropertyId.begin(), vecPropertyId.end());
    payload.insert(payload.end(), vecAmount.begin(), vecAmount.end());

    return payload;
}

std::vector<unsigned char> CreatePayload_ChangeIssuer(uint32_t propertyId)
{
    std::vector<unsigned char> payload;

    uint64_t messageType = 70;
    uint64_t messageVer = 0;

    std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);
    std::vector<uint8_t> vecPropertyId = CompressInteger((uint64_t)propertyId);

    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
    payload.insert(payload.end(), vecPropertyId.begin(), vecPropertyId.end());

    return payload;
}

std::vector<unsigned char> CreatePayload_DeactivateFeature(uint16_t featureId)
{
    std::vector<unsigned char> payload;

    uint64_t messageVer = 65535;
    uint64_t messageType = 65533;

    std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);
    std::vector<uint8_t> vecFeatureId = CompressInteger((uint64_t)featureId);

    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
    payload.insert(payload.end(), vecFeatureId.begin(), vecFeatureId.end());

    return payload;
}

std::vector<unsigned char> CreatePayload_ActivateFeature(uint16_t featureId, uint32_t activationBlock, uint32_t minClientVersion)
{
    std::vector<unsigned char> payload;

    uint16_t messageVer = 65535;
    uint16_t messageType = 65534;

    std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);
    std::vector<uint8_t> vecFeatureId = CompressInteger((uint64_t)featureId);
    std::vector<uint8_t> vecActivationBlock = CompressInteger((uint64_t)activationBlock);
    std::vector<uint8_t> vecMinClientVer = CompressInteger((uint64_t)minClientVersion);

    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
    payload.insert(payload.end(), vecFeatureId.begin(), vecFeatureId.end());
    payload.insert(payload.end(), vecActivationBlock.begin(), vecActivationBlock.end());
    payload.insert(payload.end(), vecMinClientVer.begin(), vecMinClientVer.end());

    return payload;
}

std::vector<unsigned char> CreatePayload_OmniCoreAlert(uint16_t alertType, uint32_t expiryValue, const std::string& alertMessage)
{
    std::vector<unsigned char> payload;

    uint64_t messageType = 65535;
    uint64_t messageVer = 65535;

    std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);
    std::vector<uint8_t> vecAlertType = CompressInteger((uint64_t)alertType);
    std::vector<uint8_t> vecExpiryValue = CompressInteger((uint64_t)expiryValue);

    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
    payload.insert(payload.end(), vecAlertType.begin(), vecAlertType.end());
    payload.insert(payload.end(), vecExpiryValue.begin(), vecExpiryValue.end());
    payload.insert(payload.end(), alertMessage.begin(), alertMessage.end());
    payload.push_back('\0');

    return payload;
}

////////////////////////////////////////
/** New things for Future Contracts */
std::vector<unsigned char> CreatePayload_CreateContract(uint8_t ecosystem, uint16_t propertyType, std::string subcategory, std::string name, uint32_t blocks_until_expiration, uint32_t notional_size, uint32_t collateral_currency, uint32_t margin_requirement)
{
    std::vector<unsigned char> payload;
    uint64_t messageType = 40;
    uint64_t messageVer = 0;

    std::vector<uint8_t> vecMessageType = CompressInteger(messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger(messageVer);
    std::vector<uint8_t> vecEcosystem = CompressInteger(ecosystem);
    std::vector<uint8_t> vecPropertyType = CompressInteger(propertyType);
    std::vector<uint8_t> vecBlocksUntilExpiration = CompressInteger(blocks_until_expiration);
    std::vector<uint8_t> vecNotionalSize = CompressInteger(notional_size);
    std::vector<uint8_t> vecCollateralCurrency = CompressInteger(collateral_currency);
    std::vector<uint8_t> vecMarginRequirement = CompressInteger(margin_requirement);

    if (subcategory.size() > 255) subcategory = subcategory.substr(0,255);
    if ((name).size() > 255) name = name.substr(0,255);
    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
    payload.insert(payload.end(), vecEcosystem.begin(), vecEcosystem.end());
    payload.insert(payload.end(), vecPropertyType.begin(), vecPropertyType.end());
    payload.insert(payload.end(), subcategory.begin(), subcategory.end());
    payload.push_back('\0');
    payload.insert(payload.end(), name.begin(), name.end());
    payload.push_back('\0');
    payload.insert(payload.end(), vecBlocksUntilExpiration.begin(), vecBlocksUntilExpiration.end());
    payload.insert(payload.end(), vecNotionalSize.begin(), vecNotionalSize.end());
    payload.insert(payload.end(), vecCollateralCurrency.begin(), vecCollateralCurrency.end());
    payload.insert(payload.end(), vecMarginRequirement.begin(), vecMarginRequirement.end());
    return payload;

}

std::vector<unsigned char> CreatePayload_ContractDexTrade(uint32_t propertyIdForSale, uint64_t amountForSale, uint64_t effective_price, uint8_t trading_action)
{
    std::vector<unsigned char> payload;

    uint16_t messageVer = 0;
    uint16_t messageType = 29;


        std::vector<uint8_t> vecMessageType = CompressInteger(messageType);
        std::vector<uint8_t> vecMessageVer = CompressInteger(messageVer);
        std::vector<uint8_t> vecPropertyId = CompressInteger(propertyIdForSale);
        std::vector<uint8_t> vecAmountForSale = CompressInteger(amountForSale);
        std::vector<uint8_t> vecEffectivePrice = CompressInteger(effective_price);
        std::vector<uint8_t> vecTradingAction = CompressInteger(trading_action);
        payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
        payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
        payload.insert(payload.end(), vecPropertyId.begin(), vecPropertyId.end());
        payload.insert(payload.end(), vecAmountForSale.begin(), vecAmountForSale.end());
        payload.insert(payload.end(), vecEffectivePrice.begin(), vecEffectivePrice.end());
        payload.insert(payload.end(), vecTradingAction.begin(), vecTradingAction.end());

        return payload;
}

std::vector<unsigned char> CreatePayload_ContractDexCancelEcosystem(uint8_t ecosystem, uint32_t contractId)
{
    std::vector<unsigned char> payload;

    uint16_t messageType = 32;
    uint16_t messageVer = 0;


    std::vector<uint8_t> vecMessageType = CompressInteger(messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger(messageVer);
    std::vector<uint8_t> vecEcosystem = CompressInteger(ecosystem);
    std::vector<uint8_t> vecContractId = CompressInteger(contractId);
    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
    payload.insert(payload.end(), vecEcosystem.begin(), vecEcosystem.end());
    payload.insert(payload.end(), vecContractId.begin(), vecContractId.end());

    return payload;
}

std::vector<unsigned char> CreatePayload_ContractDexClosePosition(uint8_t ecosystem, uint32_t contractId)
{
    std::vector<unsigned char> payload;

    uint16_t messageType = 33;
    uint16_t messageVer = 0;

    std::vector<uint8_t> vecMessageType = CompressInteger(messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger(messageVer);
    std::vector<uint8_t> vecEcosystem = CompressInteger(ecosystem);
    std::vector<uint8_t> vecContractId = CompressInteger(contractId);
    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
    payload.insert(payload.end(), vecEcosystem.begin(), vecEcosystem.end());
    payload.insert(payload.end(), vecContractId.begin(), vecContractId.end());

    return payload;
}

std::vector<unsigned char> CreatePayload_ContractDexCancelOrderByTxId(int block, unsigned int idx)
{
    std::vector<unsigned char> payload;

    uint16_t messageType = 34;
    uint16_t messageVer = 0;
    uint32_t tblock = static_cast<uint32_t>(block);
    uint32_t tidx = static_cast<uint32_t>(idx);
    std::vector<uint8_t> vecMessageType = CompressInteger(messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger(messageVer);
    std::vector<uint8_t> vecBlock = CompressInteger(tblock);
    std::vector<uint8_t> vecIdx = CompressInteger(tidx);
    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
    payload.insert(payload.end(), vecBlock.begin(), vecBlock.end());
    payload.insert(payload.end(), vecIdx.begin(), vecIdx.end());
    return payload;
}

std::vector<unsigned char> CreatePayload_ContractDexCancelPrice(uint32_t propertyIdForSale, uint64_t amountForSale, uint32_t propertyIdDesired, uint64_t amountDesired, uint64_t effective_price, uint8_t trading_action)
{
    std::vector<unsigned char> payload;

    uint16_t messageType = 30;
    uint16_t messageVer = 0;

    mastercore::swapByteOrder16(messageVer);
    mastercore::swapByteOrder16(messageType);
    mastercore::swapByteOrder32(propertyIdForSale);
    mastercore::swapByteOrder64(amountForSale);
    mastercore::swapByteOrder32(propertyIdDesired);
    mastercore::swapByteOrder64(amountDesired);
    mastercore::swapByteOrder64(effective_price);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, propertyIdForSale);
    PUSH_BACK_BYTES(payload, amountForSale);
    PUSH_BACK_BYTES(payload, propertyIdDesired);
    PUSH_BACK_BYTES(payload, amountDesired);
    PUSH_BACK_BYTES(payload, effective_price);
    PUSH_BACK_BYTES(payload, trading_action);

    return payload;
}
std::vector<unsigned char> CreatePayload_IssuancePegged(uint8_t ecosystem, uint16_t propertyType, uint32_t previousPropertyId,std::string subcategory, std::string name, uint32_t propertyId, uint32_t contractId, uint64_t amount)
{
    std::vector<unsigned char> payload;
    uint16_t messageType = 100;
    uint16_t messageVer = 0;


    std::vector<uint8_t> vecMessageType = CompressInteger(messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger(messageVer);
    std::vector<uint8_t> vecEcosystem = CompressInteger(ecosystem);
    std::vector<uint8_t> vecPropertyType = CompressInteger(propertyType);
    std::vector<uint8_t> vecPrevPropertyId = CompressInteger(previousPropertyId);
    std::vector<uint8_t> vecContractId = CompressInteger(contractId);
    std::vector<uint8_t> vecAmount = CompressInteger(amount);
    std::vector<uint8_t> vecPropertyId = CompressInteger(propertyId);
    if (subcategory.size() > 255) subcategory = subcategory.substr(0,255);
    if (name.size() > 255) name = name.substr(0,255);
    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
    payload.insert(payload.end(), vecEcosystem.begin(), vecEcosystem.end());
    payload.insert(payload.end(), vecPropertyType.begin(), vecPropertyType.end());
    payload.insert(payload.end(), vecPrevPropertyId.begin(), vecPrevPropertyId.end());
    payload.insert(payload.end(), subcategory.begin(), subcategory.end());
    payload.push_back('\0');
    payload.insert(payload.end(), name.begin(), name.end());
    payload.push_back('\0');
    payload.insert(payload.end(), vecPropertyId.begin(), vecPropertyId.end());
    payload.insert(payload.end(), vecContractId.begin(), vecContractId.end());
    payload.insert(payload.end(), vecAmount.begin(), vecAmount.end());
    return payload;
}

std::vector<unsigned char> CreatePayload_SendPeggedCurrency(uint32_t propertyId, uint64_t amount)
{
    std::vector<unsigned char> payload;
    uint16_t messageType = 102;
    uint16_t messageVer = 0;
    std::vector<uint8_t> vecMessageType = CompressInteger(messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger(messageVer);
    std::vector<uint8_t> vecPropertyId = CompressInteger(propertyId);
    std::vector<uint8_t> vecAmount = CompressInteger(amount);
    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
    payload.insert(payload.end(), vecPropertyId.begin(), vecPropertyId.end());
    payload.insert(payload.end(), vecAmount.begin(), vecAmount.end());
    return payload;
}
std::vector<unsigned char> CreatePayload_RedemptionPegged(uint32_t propertyId, uint32_t contractId, uint64_t amount)
{
  std::vector<unsigned char> payload;
  uint16_t messageType = 101;
  uint16_t messageVer = 0;
  std::vector<uint8_t> vecMessageType = CompressInteger(messageType);
  std::vector<uint8_t> vecMessageVer = CompressInteger(messageVer);
  std::vector<uint8_t> vecPropertyId = CompressInteger(propertyId);
  std::vector<uint8_t> vecContractId = CompressInteger(contractId);
  std::vector<uint8_t> vecAmount = CompressInteger(amount);
  payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
  payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
  payload.insert(payload.end(), vecPropertyId.begin(), vecPropertyId.end());
  payload.insert(payload.end(), vecContractId.begin(), vecContractId.end());
  payload.insert(payload.end(), vecAmount.begin(), vecAmount.end());
  return payload;

}

////////////////////////////////////////////////////////////////////////////////
std::vector<unsigned char> CreatePayload_MetaDExTrade(uint32_t propertyIdForSale, uint64_t amountForSale, uint32_t propertyIdDesired, uint64_t amountDesired)
{
    std::vector<unsigned char> payload;

    uint16_t messageType = 25;
    uint16_t messageVer = 0;

    mastercore::swapByteOrder16(messageVer);
    mastercore::swapByteOrder16(messageType);
    mastercore::swapByteOrder32(propertyIdForSale);
    mastercore::swapByteOrder64(amountForSale);
    mastercore::swapByteOrder32(propertyIdDesired);
    mastercore::swapByteOrder64(amountDesired);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, propertyIdForSale);
    PUSH_BACK_BYTES(payload, amountForSale);
    PUSH_BACK_BYTES(payload, propertyIdDesired);
    PUSH_BACK_BYTES(payload, amountDesired);

    return payload;
}

std::vector<unsigned char> CreatePayload_MetaDExCancelPrice(uint32_t propertyIdForSale, uint64_t amountForSale, uint32_t propertyIdDesired, uint64_t amountDesired)
{
    std::vector<unsigned char> payload;

    uint16_t messageType = 26;
    uint16_t messageVer = 0;

    mastercore::swapByteOrder16(messageVer);
    mastercore::swapByteOrder16(messageType);
    mastercore::swapByteOrder32(propertyIdForSale);
    mastercore::swapByteOrder64(amountForSale);
    mastercore::swapByteOrder32(propertyIdDesired);
    mastercore::swapByteOrder64(amountDesired);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, propertyIdForSale);
    PUSH_BACK_BYTES(payload, amountForSale);
    PUSH_BACK_BYTES(payload, propertyIdDesired);
    PUSH_BACK_BYTES(payload, amountDesired);

    return payload;
}

std::vector<unsigned char> CreatePayload_MetaDExCancelEcosystem(uint8_t ecosystem)
{
    std::vector<unsigned char> payload;

    uint16_t messageType = 28;
    uint16_t messageVer = 0;

    mastercore::swapByteOrder16(messageVer);
    mastercore::swapByteOrder16(messageType);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, ecosystem);

    return payload;
}

std::vector<unsigned char> CreatePayload_MetaDExCancelPair(uint32_t propertyIdForSale, uint32_t propertyIdDesired)
{
    std::vector<unsigned char> payload;

    uint16_t messageType = 27;
    uint16_t messageVer = 0;

    mastercore::swapByteOrder16(messageVer);
    mastercore::swapByteOrder16(messageType);
    mastercore::swapByteOrder32(propertyIdForSale);
    mastercore::swapByteOrder32(propertyIdDesired);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, propertyIdForSale);
    PUSH_BACK_BYTES(payload, propertyIdDesired);

    return payload;
}

#undef PUSH_BACK_BYTES
#undef PUSH_BACK_BYTES_PTR
