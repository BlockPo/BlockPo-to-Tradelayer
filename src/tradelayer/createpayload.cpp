// This file serves to provide payload creation functions.

#include "tradelayer/createpayload.h"
#include "tradelayer/convert.h"
#include "tradelayer/varint.h"

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
  std::vector<uint8_t> vecAmount = CompressInteger((uint64_t)amount);

  payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
  payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
  payload.insert(payload.end(), vecPropertyId.begin(), vecPropertyId.end());
  payload.insert(payload.end(), vecAmount.begin(), vecAmount.end());

  return payload;
}

std::vector<unsigned char> CreatePayload_SendVestingTokens(uint64_t amount)
{
  std::vector<unsigned char> payload;

  uint64_t messageType = 5;
  uint64_t messageVer = 0;

  std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
  std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);
  std::vector<uint8_t> vecAmount = CompressInteger((uint64_t)amount);

  payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
  payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
  payload.insert(payload.end(), vecAmount.begin(), vecAmount.end());

  return payload;
}

std::vector<unsigned char> CreatePayload_SendAll()
{
    std::vector<unsigned char> payload;

    uint64_t messageType = 4;
    uint64_t messageVer = 0;

    std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);

    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());

    return payload;
}

std::vector<unsigned char> CreatePayload_IssuanceFixed(uint16_t propertyType, uint32_t previousPropertyId, std::string name, std::string url, std::string data, uint64_t amount, std::vector<int> kycVec)
{
    std::vector<unsigned char> payload;

    uint64_t messageType = 50;
    uint64_t messageVer = 0;

    std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);
    std::vector<uint8_t> vecPropertyType = CompressInteger((uint64_t)propertyType);
    std::vector<uint8_t> vecPrevPropertyId = CompressInteger((uint64_t)previousPropertyId);
    std::vector<uint8_t> vecAmount = CompressInteger((uint64_t)amount);

    std::vector<std::vector<uint8_t>> auxVec;

    for (std::vector<int>::iterator it = kycVec.begin(); it != kycVec.end();++it)
    {
        std::vector<uint8_t> vecNum = CompressInteger((uint64_t) *it);
        auxVec.push_back(vecNum);
    }

    if (name.size() > 255) name = name.substr(0,255);
    if (url.size() > 255) url = url.substr(0,255);
    if (data.size() > 255) data = data.substr(0,255);

    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
    payload.insert(payload.end(), vecPropertyType.begin(), vecPropertyType.end());
    payload.insert(payload.end(), vecPrevPropertyId.begin(), vecPrevPropertyId.end());
    payload.insert(payload.end(), name.begin(), name.end());
    payload.push_back('\0');
    payload.insert(payload.end(), url.begin(), url.end());
    payload.push_back('\0');
    payload.insert(payload.end(), data.begin(), data.end());
    payload.push_back('\0');
    payload.insert(payload.end(), vecAmount.begin(), vecAmount.end());

    for (std::vector<std::vector<uint8_t>>::iterator itt = auxVec.begin(); itt != auxVec.end(); ++itt)
    {
        const std::vector<uint8_t> vec = *itt;
        payload.insert(payload.end(), vec.begin(), vec.end());
    }

    return payload;
}

std::vector<unsigned char> CreatePayload_IssuanceVariable(uint16_t propertyType, uint32_t previousPropertyId, std::string name, std::string url, std::string data, uint32_t propertyIdDesired, uint64_t amountPerUnit, uint64_t deadline, uint8_t earlyBonus, uint8_t issuerPercentage)
{
    std::vector<unsigned char> payload;

    uint64_t messageType = 51;
    uint64_t messageVer = 0;

    std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);
    std::vector<uint8_t> vecPropertyType = CompressInteger((uint64_t)propertyType);
    std::vector<uint8_t> vecPrevPropertyId = CompressInteger((uint64_t)previousPropertyId);
    std::vector<uint8_t> vecPropertyIdDesired = CompressInteger((uint64_t)propertyIdDesired);
    std::vector<uint8_t> vecAmountPerUnit = CompressInteger(amountPerUnit);
    std::vector<uint8_t> vecDeadline = CompressInteger(deadline);

    if (name.size() > 255) name = name.substr(0,255);
    if (url.size() > 255) url = url.substr(0,255);
    if (data.size() > 255) data = data.substr(0,255);

    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
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


std::vector<unsigned char> CreatePayload_IssuanceManaged(uint16_t propertyType, uint32_t previousPropertyId, std::string name, std::string url, std::string data, std::vector<int> kycVec)
{
    std::vector<unsigned char> payload;

    uint64_t messageType = 54;
    uint64_t messageVer = 0;

    std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);
    std::vector<uint8_t> vecPropertyType = CompressInteger((uint64_t)propertyType);
    std::vector<uint8_t> vecPrevPropertyId = CompressInteger((uint64_t)previousPropertyId);

    std::vector<std::vector<uint8_t>> auxVec;

    for (std::vector<int>::iterator it = kycVec.begin(); it != kycVec.end();++it)
    {
        std::vector<uint8_t> vecNum = CompressInteger((uint64_t) *it);
        auxVec.push_back(vecNum);
    }

    if (name.size() > 255) name = name.substr(0,255);
    if (url.size() > 255) url = url.substr(0,255);
    if (data.size() > 255) data = data.substr(0,255);

    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
    payload.insert(payload.end(), vecPropertyType.begin(), vecPropertyType.end());
    payload.insert(payload.end(), vecPrevPropertyId.begin(), vecPrevPropertyId.end());
    payload.insert(payload.end(), name.begin(), name.end());
    payload.push_back('\0');
    payload.insert(payload.end(), url.begin(), url.end());
    payload.push_back('\0');
    payload.insert(payload.end(), data.begin(), data.end());
    payload.push_back('\0');

    for (std::vector<std::vector<uint8_t>>::iterator itt = auxVec.begin(); itt != auxVec.end(); ++itt)
    {
        const std::vector<uint8_t> vec = *itt;
        payload.insert(payload.end(), vec.begin(), vec.end());
    }

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

    uint64_t messageVer = 65535;
    uint64_t messageType = 65534;

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

std::vector<unsigned char> CreatePayload_TradeLayerAlert(uint16_t alertType, uint32_t expiryValue, const std::string& alertMessage)
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

std::vector<unsigned char> CreatePayload_CreateContract(uint32_t num, uint32_t den, std::string name, uint32_t blocks_until_expiration, uint32_t notional_size, uint32_t collateral_currency, uint32_t margin_requirement, uint8_t inverse, std::vector<int> kycVec)
{
  std::vector<unsigned char> payload;

  uint64_t messageType = 40;
  uint64_t messageVer = 0;

  std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
  std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);
  std::vector<uint8_t> vecNum = CompressInteger((uint64_t)num);
  std::vector<uint8_t> vecDen = CompressInteger((uint64_t)den);
  std::vector<uint8_t> vecBlocksUntilExpiration = CompressInteger((uint64_t)blocks_until_expiration);
  std::vector<uint8_t> vecNotionalSize = CompressInteger((uint64_t)notional_size);
  std::vector<uint8_t> vecCollateralCurrency = CompressInteger((uint64_t)collateral_currency);
  std::vector<uint8_t> vecMarginRequirement = CompressInteger((uint64_t)margin_requirement);
  std::vector<uint8_t> vecInverse = CompressInteger((uint64_t)inverse);

  std::vector<std::vector<uint8_t>> auxVec;

  for (std::vector<int>::iterator it = kycVec.begin(); it != kycVec.end();++it)
  {
      std::vector<uint8_t> vecNum = CompressInteger((uint64_t) *it);
      auxVec.push_back(vecNum);
  }

  if ((name).size() > 255) name = name.substr(0,255);
  payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
  payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
  payload.insert(payload.end(), vecNum.begin(), vecNum.end());
  payload.insert(payload.end(), vecDen.begin(), vecDen.end());
  payload.insert(payload.end(), name.begin(), name.end());
  payload.push_back('\0');
  payload.insert(payload.end(), vecBlocksUntilExpiration.begin(), vecBlocksUntilExpiration.end());
  payload.insert(payload.end(), vecNotionalSize.begin(), vecNotionalSize.end());
  payload.insert(payload.end(), vecCollateralCurrency.begin(), vecCollateralCurrency.end());
  payload.insert(payload.end(), vecMarginRequirement.begin(), vecMarginRequirement.end());
  payload.insert(payload.end(), vecInverse.begin(), vecInverse.end());

  for (std::vector<std::vector<uint8_t>>::iterator itt = auxVec.begin(); itt != auxVec.end(); ++itt)
  {
      const std::vector<uint8_t> vec = *itt;
      payload.insert(payload.end(), vec.begin(), vec.end());
  }

  return payload;
}

std::vector<unsigned char> CreatePayload_ContractDexTrade(std::string name_traded, uint64_t amountForSale, uint64_t effective_price, uint8_t trading_action, uint64_t leverage)
{
  std::vector<unsigned char> payload;

  uint64_t messageVer = 0;
  uint64_t messageType = 29;

  std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
  std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);
  std::vector<uint8_t> vecAmountForSale = CompressInteger((uint64_t)amountForSale);
  std::vector<uint8_t> vecEffectivePrice = CompressInteger((uint64_t)effective_price);
  std::vector<uint8_t> vecTradingAction = CompressInteger((uint64_t)trading_action);
    std::vector<uint8_t> vecLeverage = CompressInteger(leverage);

  payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
  payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
  payload.insert(payload.end(), name_traded.begin(), name_traded.end());
  payload.push_back('\0');
  payload.insert(payload.end(), vecAmountForSale.begin(), vecAmountForSale.end());
  payload.insert(payload.end(), vecEffectivePrice.begin(), vecEffectivePrice.end());
  payload.insert(payload.end(), vecTradingAction.begin(), vecTradingAction.end());
  payload.insert(payload.end(), vecLeverage.begin(), vecLeverage.end());

  return payload;
}

std::vector<unsigned char> CreatePayload_ContractDexCancelAll(uint32_t contractId)
{
  std::vector<unsigned char> payload;

    uint64_t messageType = 32;
    uint64_t messageVer = 0;

    std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);
    std::vector<uint8_t> vecContractId = CompressInteger((uint64_t)contractId);
    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
    payload.insert(payload.end(), vecContractId.begin(), vecContractId.end());

    return payload;
}

std::vector<unsigned char> CreatePayload_ContractDexClosePosition(uint32_t contractId)
{
    std::vector<unsigned char> payload;

    uint64_t messageType = 33;
    uint64_t messageVer = 0;

    std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);
    std::vector<uint8_t> vecContractId = CompressInteger((uint64_t)contractId);
    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
    payload.insert(payload.end(), vecContractId.begin(), vecContractId.end());

    return payload;
}

std::vector<unsigned char> CreatePayload_ContractDexCancelOrderByTxId(int block, unsigned int idx)
{
    std::vector<unsigned char> payload;

    uint64_t messageType = 34;
    uint64_t messageVer = 0;
    uint32_t tblock = static_cast<uint32_t>(block);
    uint32_t tidx = static_cast<uint32_t>(idx);
    std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);
    std::vector<uint8_t> vecBlock = CompressInteger((uint64_t)tblock);
    std::vector<uint8_t> vecIdx = CompressInteger((uint64_t)tidx);
    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
    payload.insert(payload.end(), vecBlock.begin(), vecBlock.end());
    payload.insert(payload.end(), vecIdx.begin(), vecIdx.end());

    return payload;
}

std::vector<unsigned char> CreatePayload_IssuancePegged(uint16_t propertyType, uint32_t previousPropertyId, std::string name, uint32_t propertyId, uint32_t contractId, uint64_t amount)
{
    std::vector<unsigned char> payload;

    uint64_t messageType = 100;
    uint64_t messageVer = 0;

    std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);
    std::vector<uint8_t> vecPropertyType = CompressInteger((uint64_t)propertyType);
    std::vector<uint8_t> vecPrevPropertyId = CompressInteger((uint64_t)previousPropertyId);
    std::vector<uint8_t> vecContractId = CompressInteger((uint64_t)contractId);
    std::vector<uint8_t> vecAmount = CompressInteger((uint64_t)amount);
    std::vector<uint8_t> vecPropertyId = CompressInteger((uint64_t)propertyId);
    if (name.size() > 255) name = name.substr(0,255);
    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
    payload.insert(payload.end(), vecPropertyType.begin(), vecPropertyType.end());
    payload.insert(payload.end(), vecPrevPropertyId.begin(), vecPrevPropertyId.end());
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

    uint64_t messageType = 102;
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

std::vector<unsigned char> CreatePayload_RedemptionPegged(uint32_t propertyId, uint32_t contractId, uint64_t amount)
{
    std::vector<unsigned char> payload;

    uint64_t messageType = 101;
    uint64_t messageVer = 0;

    std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);
    std::vector<uint8_t> vecPropertyId = CompressInteger((uint64_t)propertyId);
    std::vector<uint8_t> vecContractId = CompressInteger((uint64_t)contractId);
    std::vector<uint8_t> vecAmount = CompressInteger((uint64_t)amount);
    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
    payload.insert(payload.end(), vecPropertyId.begin(), vecPropertyId.end());
    payload.insert(payload.end(), vecContractId.begin(), vecContractId.end());
    payload.insert(payload.end(), vecAmount.begin(), vecAmount.end());

    return payload;

}

std::vector<unsigned char> CreatePayload_DExSell(uint32_t propertyId, uint64_t amountForSale, uint64_t amountDesired, uint8_t timeLimit, uint64_t minFee, uint8_t subAction)
{
    std::vector<unsigned char> payload;

    uint64_t messageType = 20;
    uint64_t messageVer = 1;

    std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);
    std::vector<uint8_t> vecPropertyId = CompressInteger((uint64_t)propertyId);
    std::vector<uint8_t> vecAmountForSale = CompressInteger((uint64_t)amountForSale);
    std::vector<uint8_t> vecAmountDesired = CompressInteger((uint64_t)amountDesired);
    std::vector<uint8_t> vecTimeLimit = CompressInteger((uint64_t)timeLimit);
    std::vector<uint8_t> vecMinFee = CompressInteger((uint64_t)minFee);
    std::vector<uint8_t> vecSubAction = CompressInteger((uint64_t)subAction);
    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
    payload.insert(payload.end(), vecPropertyId.begin(), vecPropertyId.end());
    payload.insert(payload.end(), vecAmountForSale.begin(), vecAmountForSale.end());
    payload.insert(payload.end(), vecAmountDesired.begin(), vecAmountDesired.end());
    payload.insert(payload.end(), vecTimeLimit.begin(), vecTimeLimit.end());
    payload.insert(payload.end(), vecMinFee.begin(), vecMinFee.end());
    payload.insert(payload.end(), vecSubAction.begin(), vecSubAction.end());

    return payload;
}

std::vector<unsigned char> CreatePayload_DEx(uint32_t propertyId, uint64_t amount, uint64_t price,  uint8_t timeLimit, uint64_t minFee, uint8_t subAction)
{
    std::vector<unsigned char> payload;

    uint64_t messageType = 21;
    uint64_t messageVer = 0;

    std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);
    std::vector<uint8_t> vecPropertyId = CompressInteger((uint64_t)propertyId);
    std::vector<uint8_t> vecAmountForSale = CompressInteger((uint64_t)amount);
    std::vector<uint8_t> vecPrice = CompressInteger(price);
    std::vector<uint8_t> vecTimeLimit = CompressInteger((uint64_t)timeLimit);
    std::vector<uint8_t> vecMinFee = CompressInteger((uint64_t)minFee);
    std::vector<uint8_t> vecSubAction = CompressInteger((uint64_t)subAction);
    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
    payload.insert(payload.end(), vecPropertyId.begin(), vecPropertyId.end());
    payload.insert(payload.end(), vecAmountForSale.begin(), vecAmountForSale.end());
    payload.insert(payload.end(), vecPrice.begin(), vecPrice.end());
    payload.insert(payload.end(), vecTimeLimit.begin(), vecTimeLimit.end());
    payload.insert(payload.end(), vecMinFee.begin(), vecMinFee.end());
    payload.insert(payload.end(), vecSubAction.begin(), vecSubAction.end());

    return payload;
}

std::vector<unsigned char> CreatePayload_DExAccept(uint32_t propertyId, uint64_t amount)
{
    std::vector<unsigned char> payload;

    uint64_t messageType = 22;
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

std::vector<unsigned char> CreatePayload_MetaDExTrade(uint32_t propertyIdForSale, uint64_t amountForSale, uint32_t propertyIdDesired, uint64_t amountDesired)
{
    std::vector<unsigned char> payload;

    uint64_t messageType = 25;
    uint64_t messageVer = 0;

    std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);
    std::vector<uint8_t> vecPropertyIdForSale = CompressInteger((uint64_t)propertyIdForSale);
    std::vector<uint8_t> vecAmountForSale = CompressInteger((uint64_t)amountForSale);
    std::vector<uint8_t> vecPropertyIdDesired = CompressInteger((uint64_t)propertyIdDesired);
    std::vector<uint8_t> vecAmountDesired = CompressInteger((uint64_t)amountDesired);
    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
    payload.insert(payload.end(), vecPropertyIdForSale.begin(), vecPropertyIdForSale.end());
    payload.insert(payload.end(), vecAmountForSale.begin(), vecAmountForSale.end());
    payload.insert(payload.end(), vecPropertyIdDesired.begin(), vecPropertyIdDesired.end());
    payload.insert(payload.end(), vecAmountDesired.begin(), vecAmountDesired.end());

    return payload;
}

/* Tx 103 */

std::vector<unsigned char> CreatePayload_CreateOracleContract(std::string name, uint32_t blocks_until_expiration, uint32_t notional_size, uint32_t collateral_currency, uint32_t margin_requirement, uint8_t inverse, std::vector<int> kycVec)
{
  std::vector<unsigned char> payload;

  uint64_t messageType = 103;
  uint64_t messageVer = 0;

  std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
  std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);
  std::vector<uint8_t> vecBlocksUntilExpiration = CompressInteger((uint64_t)blocks_until_expiration);
  std::vector<uint8_t> vecNotionalSize = CompressInteger((uint64_t)notional_size);
  std::vector<uint8_t> vecCollateralCurrency = CompressInteger((uint64_t)collateral_currency);
  std::vector<uint8_t> vecMarginRequirement = CompressInteger((uint64_t)margin_requirement);
  std::vector<uint8_t> vecInverse = CompressInteger((uint64_t)inverse);

  std::vector<std::vector<uint8_t>> auxVec;

  for (std::vector<int>::iterator it = kycVec.begin(); it != kycVec.end();++it)
  {
      std::vector<uint8_t> vecNum = CompressInteger((uint64_t) *it);
      auxVec.push_back(vecNum);
  }

  if ((name).size() > 255) name = name.substr(0,255);
  payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
  payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
  payload.insert(payload.end(), name.begin(), name.end());
  payload.push_back('\0');
  payload.insert(payload.end(), vecBlocksUntilExpiration.begin(), vecBlocksUntilExpiration.end());
  payload.insert(payload.end(), vecNotionalSize.begin(), vecNotionalSize.end());
  payload.insert(payload.end(), vecCollateralCurrency.begin(), vecCollateralCurrency.end());
  payload.insert(payload.end(), vecMarginRequirement.begin(), vecMarginRequirement.end());
  payload.insert(payload.end(), vecInverse.begin(), vecInverse.end());

  for (std::vector<std::vector<uint8_t>>::iterator itt = auxVec.begin(); itt != auxVec.end(); ++itt)
  {
      const std::vector<uint8_t> vec = *itt;
      payload.insert(payload.end(), vec.begin(), vec.end());
  }

  return payload;
}

/* Tx 104 */
std::vector<unsigned char> CreatePayload_Change_OracleAdm(uint32_t contractId)
{
    std::vector<unsigned char> payload;

    uint64_t messageType = 104;
    uint64_t messageVer = 0;

    std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);
    std::vector<uint8_t> vecContractId = CompressInteger((uint64_t)contractId);

    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
    payload.insert(payload.end(), vecContractId.begin(), vecContractId.end());

    return payload;
}

/* Tx 105 */
std::vector<unsigned char> CreatePayload_Set_Oracle(uint32_t contractId, uint64_t high, uint64_t low, uint64_t close)
{
    std::vector<unsigned char> payload;

    uint64_t messageType = 105;
    uint64_t messageVer = 0;

    std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);
    std::vector<uint8_t> vecContractId = CompressInteger((uint64_t)contractId);
    std::vector<uint8_t> vecHigh = CompressInteger(high);
    std::vector<uint8_t> vecLow = CompressInteger(low);
    std::vector<uint8_t> vecClose = CompressInteger(close);

    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
    payload.insert(payload.end(), vecContractId.begin(), vecContractId.end());
    payload.insert(payload.end(), vecHigh.begin(), vecHigh.end());
    payload.insert(payload.end(), vecLow.begin(), vecLow.end());
    payload.insert(payload.end(), vecClose.begin(), vecClose.end());

    return payload;
}

std::vector<unsigned char> CreatePayload_OracleBackup(uint32_t contractId)
{
    std::vector<unsigned char> payload;

    uint64_t messageType = 106;
    uint64_t messageVer = 0;

    std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);
    std::vector<uint8_t> vecContractId = CompressInteger((uint64_t)contractId);

    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
    payload.insert(payload.end(), vecContractId.begin(), vecContractId.end());

    return payload;
}

std::vector<unsigned char> CreatePayload_Close_Oracle(uint32_t contractId)
{
    std::vector<unsigned char> payload;

    uint64_t messageType = 107;
    uint64_t messageVer = 0;

    std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
    std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);
    std::vector<uint8_t> vecContractId = CompressInteger((uint64_t)contractId);

    payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
    payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
    payload.insert(payload.end(), vecContractId.begin(), vecContractId.end());

    return payload;
}

std::vector<unsigned char> CreatePayload_Commit_Channel(uint32_t propertyId, uint64_t amount)
{
  std::vector<unsigned char> payload;

  uint64_t messageType = 108;
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

std::vector<unsigned char> CreatePayload_Withdrawal_FromChannel(uint32_t propertyId, uint64_t amount)
{
  std::vector<unsigned char> payload;

  uint64_t messageType = 109;
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

std::vector<unsigned char> CreatePayload_Instant_Trade(uint32_t propertyId, uint64_t amount, uint32_t blockheight_expiry, uint32_t propertyDesired, uint64_t amountDesired)
{
  std::vector<unsigned char> payload;

  uint64_t messageType = 110;
  uint64_t messageVer = 0;

  std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
  std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);
  std::vector<uint8_t> vecPropertyId = CompressInteger((uint64_t)propertyId);
  std::vector<uint8_t> vecAmount = CompressInteger((uint64_t)amount);
  std::vector<uint8_t> vecBlock = CompressInteger((uint64_t)blockheight_expiry);
  std::vector<uint8_t> vecPropertyDesired = CompressInteger((uint64_t)propertyDesired);
  std::vector<uint8_t> vecAmountDesired = CompressInteger((uint64_t)amountDesired);

  payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
  payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
  payload.insert(payload.end(), vecPropertyId.begin(), vecPropertyId.end());
  payload.insert(payload.end(), vecAmount.begin(), vecAmount.end());
  payload.insert(payload.end(), vecBlock.begin(), vecBlock.end());
  payload.insert(payload.end(), vecPropertyDesired.begin(), vecPropertyDesired.end());
  payload.insert(payload.end(), vecAmountDesired.begin(), vecAmountDesired.end());

  return payload;
}

std::vector<unsigned char> CreatePayload_Contract_Instant_Trade(uint32_t contractId, uint64_t amount, uint32_t blockheight_expiry, uint64_t price, uint8_t trading_action, uint64_t leverage)
{
  std::vector<unsigned char> payload;

  uint64_t messageType = 114;
  uint64_t messageVer = 0;

  std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
  std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);
  std::vector<uint8_t> vecContractId = CompressInteger((uint64_t)contractId);
  std::vector<uint8_t> vecAmount = CompressInteger((uint64_t)amount);
  std::vector<uint8_t> vecBlock = CompressInteger((uint64_t)blockheight_expiry);
  std::vector<uint8_t> vecPrice = CompressInteger(price);
  std::vector<uint8_t> vecTrading = CompressInteger((uint64_t)trading_action);
  std::vector<uint8_t> vecLeverage = CompressInteger(leverage);

  payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
  payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
  payload.insert(payload.end(), vecContractId.begin(), vecContractId.end());
  payload.insert(payload.end(), vecAmount.begin(), vecAmount.end());
  payload.insert(payload.end(), vecBlock.begin(), vecBlock.end());
  payload.insert(payload.end(), vecPrice.begin(), vecPrice.end());
  payload.insert(payload.end(), vecTrading.begin(), vecTrading.end());
  payload.insert(payload.end(), vecLeverage.begin(), vecLeverage.end());

  return payload;
}

std::vector<unsigned char> CreatePayload_PNL_Update(uint32_t propertyId, uint64_t amount, uint32_t blockheight_expiry)
{
  std::vector<unsigned char> payload;

  uint64_t messageType = 111;
  uint64_t messageVer = 0;

  std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
  std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);
  std::vector<uint8_t> vecPropertyId = CompressInteger((uint64_t)propertyId);
  std::vector<uint8_t> vecAmount = CompressInteger((uint64_t)amount);
  std::vector<uint8_t> vecBlock = CompressInteger((uint64_t)blockheight_expiry);

  payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
  payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
  payload.insert(payload.end(), vecPropertyId.begin(), vecPropertyId.end());
  payload.insert(payload.end(), vecAmount.begin(), vecAmount.end());
  payload.insert(payload.end(), vecBlock.begin(), vecBlock.end());

  return payload;
}

std::vector<unsigned char> CreatePayload_Transfer(uint32_t propertyId, uint64_t amount)
{
  std::vector<unsigned char> payload;

  uint64_t messageType = 112;
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

std::vector<unsigned char> CreatePayload_Create_Channel(std::string channelAddress, uint32_t blocks)
{
  std::vector<unsigned char> payload;

  uint64_t messageType = 113;
  uint64_t messageVer = 0;

  std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
  std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);
  std::vector<uint8_t> vecBlocks = CompressInteger((uint64_t)blocks);

  if ((channelAddress).size() > 255) channelAddress = channelAddress.substr(0,255);

  payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
  payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
  payload.insert(payload.end(), vecBlocks.begin(), vecBlocks.end());
  payload.insert(payload.end(), channelAddress.begin(), channelAddress.end());
  payload.push_back('\0');

  return payload;
}

std::vector<unsigned char> CreatePayload_New_Id_Registration(std::string website, std::string name)
{
  std::vector<unsigned char> payload;

  uint64_t messageType = 115;
  uint64_t messageVer = 0;

  std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
  std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);

  if ((website).size() > 255) website = website.substr(0,255);
  if ((name).size() > 255) name = name.substr(0,255);

  payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
  payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());

  payload.insert(payload.end(), website.begin(), website.end());
  payload.push_back('\0');
  payload.insert(payload.end(), name.begin(), name.end());
  payload.push_back('\0');

  return payload;
}

std::vector<unsigned char> CreatePayload_Update_Id_Registration()
{
  std::vector<unsigned char> payload;

  uint64_t messageType = 116;
  uint64_t messageVer = 0;

  std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
  std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);

  payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
  payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());

  return payload;
}

std::vector<unsigned char> CreatePayload_DEx_Payment()
{
  std::vector<unsigned char> payload;

  uint64_t messageType = 117;
  uint64_t messageVer = 0;

  std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
  std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);

  payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
  payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());

  return payload;
}

std::vector<unsigned char> CreatePayload_Attestation(std::string hash)
{
  std::vector<unsigned char> payload;

  uint64_t messageType = 118;
  uint64_t messageVer = 0;

  std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
  std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);

  if ((hash).size() > 255) hash = hash.substr(0,255);

  payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
  payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
  payload.insert(payload.end(), hash.begin(), hash.end());
  payload.push_back('\0');

  return payload;
}

std::vector<unsigned char> CreatePayload_MetaDExCancelAll()
{
  std::vector<unsigned char> payload;

  uint64_t messageType = 26;
  uint64_t messageVer = 0;

  std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
  std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);

  payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
  payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());

  return payload;
}

std::vector<unsigned char> CreatePayload_ContractDExCancel(std::string hash)
{
  std::vector<unsigned char> payload;

  uint64_t messageType = 31;
  uint64_t messageVer = 0;

  std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
  std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);

  if ((hash).size() > 255) hash = hash.substr(0,255);

  payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
  payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
  payload.insert(payload.end(), hash.begin(), hash.end());
  payload.push_back('\0');

  return payload;
}

#undef PUSH_BACK_BYTES
#undef PUSH_BACK_BYTES_PTR
