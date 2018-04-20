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
std::vector<unsigned char> CreatePayload_CreateContract(uint8_t ecosystem, uint16_t propertyType, uint32_t previousPropertyId, std::string category,
                                                          std::string subcategory, std::string name, std::string url, std::string data, uint32_t propertyIdDesired,
                                                          uint64_t amountPerUnit, uint64_t deadline, uint8_t earlyBonus, uint8_t issuerPercentage,
                                                          uint32_t blocks_until_expiration, uint32_t notional_size, uint32_t collateral_currency, uint32_t margin_requirement)
{
    std::vector<unsigned char> payload;

    uint16_t messageVer = 0;
    uint16_t messageType = 40;

    mastercore::swapByteOrder16(messageVer);
    mastercore::swapByteOrder16(messageType);
    mastercore::swapByteOrder16(propertyType);
    mastercore::swapByteOrder32(previousPropertyId);
    mastercore::swapByteOrder32(propertyIdDesired);
    mastercore::swapByteOrder64(amountPerUnit);
    mastercore::swapByteOrder64(deadline);
    ////////////////////////////////////
    mastercore::swapByteOrder32(blocks_until_expiration);
    mastercore::swapByteOrder32(notional_size);
    mastercore::swapByteOrder32(collateral_currency);
    mastercore::swapByteOrder32(margin_requirement);
    ////////////////////////////////////

    if (category.size() > 255) category = category.substr(0,255);
    if (subcategory.size() > 255) subcategory = subcategory.substr(0,255);
    if (name.size() > 255) name = name.substr(0,255);
    if (url.size() > 255) url = url.substr(0,255);
    if (data.size() > 255) data = data.substr(0,255);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, ecosystem);
    PUSH_BACK_BYTES(payload, propertyType);
    PUSH_BACK_BYTES(payload, previousPropertyId);
    payload.insert(payload.end(), category.begin(), category.end());
    payload.push_back('\0');
    payload.insert(payload.end(), subcategory.begin(), subcategory.end());
    payload.push_back('\0');
    payload.insert(payload.end(), name.begin(), name.end());
    payload.push_back('\0');
    payload.insert(payload.end(), url.begin(), url.end());
    payload.push_back('\0');
    payload.insert(payload.end(), data.begin(), data.end());
    payload.push_back('\0');
    
    PUSH_BACK_BYTES(payload, propertyIdDesired);
    PUSH_BACK_BYTES(payload, amountPerUnit);
    PUSH_BACK_BYTES(payload, deadline);
    PUSH_BACK_BYTES(payload, earlyBonus);
    PUSH_BACK_BYTES(payload, issuerPercentage);
    ////////////////////////////////////
    PUSH_BACK_BYTES(payload, blocks_until_expiration);
    PUSH_BACK_BYTES(payload, notional_size);
    PUSH_BACK_BYTES(payload, collateral_currency);
    PUSH_BACK_BYTES(payload, margin_requirement);
    ////////////////////////////////////
    return payload;
}

std::vector<unsigned char> CreatePayload_ContractDexTrade(uint32_t propertyIdForSale, uint64_t amountForSale, uint32_t propertyIdDesired, uint64_t amountDesired, uint64_t effective_price, uint8_t trading_action)
{
    std::vector<unsigned char> payload;

    uint16_t messageVer = 0;
    uint16_t messageType = 29;

    mastercore::swapByteOrder16(messageVer);
    mastercore::swapByteOrder16(messageType);
    mastercore::swapByteOrder32(propertyIdForSale); /*pkt[4]*/
    mastercore::swapByteOrder64(amountForSale);     /*pkt[8]*/
    mastercore::swapByteOrder32(propertyIdDesired); /*pkt[16]*/
    mastercore::swapByteOrder64(amountDesired);     /*pkt[20]*/
    mastercore::swapByteOrder64(effective_price);   /*pkt[28]*/

    PUSH_BACK_BYTES(payload, messageVer);           /*vch[0]: 2 = 2 bytes*/
    PUSH_BACK_BYTES(payload, messageType);          /*vch[1]: 2+2 = 4 bytes*/
    PUSH_BACK_BYTES(payload, propertyIdForSale);    /*vch[2]: 2+2+ 4 = 8 bytes*/
    PUSH_BACK_BYTES(payload, amountForSale);        /*vch[3]: 2+2+4+ 8 = 16 bytes*/
    PUSH_BACK_BYTES(payload, propertyIdDesired);    /*vch[4]: 2+2+4+8+ 4 = 20 bytes*/
    PUSH_BACK_BYTES(payload, amountDesired);        /*vch[5]: 2+2+4+8+4+ 8 = 28 bytes*/
    PUSH_BACK_BYTES(payload, effective_price);      /*vch[6]: 2+2+4+8+4+8+ 8 = 36 bytes*/
    PUSH_BACK_BYTES(payload, trading_action);       /*vch[7]: 2+2+4+8+4+8+8 +1 = 37 bytes*/
    
    return payload;
}

std::vector<unsigned char> CreatePayload_ContractDexCancelEcosystem(uint8_t ecosystem)
{
    std::vector<unsigned char> payload;

    uint16_t messageType = 32;
    uint16_t messageVer = 0;

    mastercore::swapByteOrder16(messageVer);
    mastercore::swapByteOrder16(messageType);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, ecosystem);

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

#undef PUSH_BACK_BYTES
#undef PUSH_BACK_BYTES_PTR
