#ifndef TRADELAYER_CREATEPAYLOAD_H
#define TRADELAYER_CREATEPAYLOAD_H

#include <stdint.h>
#include <string>
#include <vector>

std::vector<unsigned char> CreatePayload_SimpleSend(uint32_t propertyId, uint64_t amount);
std::vector<unsigned char> CreatePayload_SendVestingTokens(uint64_t amount);
std::vector<unsigned char> CreatePayload_SendAll();
std::vector<unsigned char> CreatePayload_DExSell(uint32_t propertyId, uint64_t amountForSale, uint64_t amountDesired, uint8_t timeLimit, uint64_t minFee, uint8_t subAction);
std::vector<unsigned char> CreatePayload_DEx(uint32_t propertyId, uint64_t amount, uint64_t price, uint8_t timeLimit, uint64_t minFee, uint8_t subAction);
std::vector<unsigned char> CreatePayload_DExAccept(uint32_t propertyId, uint64_t amount);
std::vector<unsigned char> CreatePayload_IssuanceFixed(uint16_t propertyType, uint32_t previousPropertyId, std::string& name, std::string& url,
                                                       std::string& data, uint64_t amount, std::vector<int>& kycVec);
std::vector<unsigned char> CreatePayload_IssuanceManaged(uint16_t propertyType, uint32_t previousPropertyId,
                                                      std::string& name, std::string& url, std::string& data, std::vector<int>& kycVec);
std::vector<unsigned char> CreatePayload_Grant(uint32_t propertyId, uint64_t amount);
std::vector<unsigned char> CreatePayload_Revoke(uint32_t propertyId, uint64_t amount);
std::vector<unsigned char> CreatePayload_ChangeIssuer(uint32_t propertyId);
std::vector<unsigned char> CreatePayload_DeactivateFeature(uint16_t featureId);
std::vector<unsigned char> CreatePayload_ActivateFeature(uint16_t featureId, uint32_t activationBlock, uint32_t minClientVersion);
std::vector<unsigned char> CreatePayload_CreateContract(uint32_t num, uint32_t den, std::string& name, uint32_t blocks_until_expiration, uint32_t notional_size, uint32_t collateral_currency, uint64_t margin_requirement, uint8_t inverse, std::vector<int>& kycVec);
std::vector<unsigned char> CreatePayload_ContractDexTrade(std::string& contractName, uint64_t amountForSale, uint64_t effective_price, uint8_t trading_action, uint64_t leverage);
std::vector<unsigned char> CreatePayload_ContractDexCancelAll(uint32_t contractId);
std::vector<unsigned char> CreatePayload_ContractDexCancelOrderByTxId(int block, unsigned int idx);
std::vector<unsigned char> CreatePayload_ContractDexClosePosition(uint32_t contractId);
std::vector<unsigned char> CreatePayload_ContractDexCancelPrice(uint32_t propertyIdForSale, uint64_t amountForSale, uint32_t propertyIdDesired, uint64_t amountDesired, uint64_t effective_price, uint8_t trading_action);
std::vector<unsigned char> CreatePayload_IssuancePegged(uint16_t propertyType, uint32_t previousPropertyId, std::string& name, uint32_t propertyId, uint32_t contractId, uint64_t amount);
std::vector<unsigned char> CreatePayload_RedemptionPegged(uint32_t propertyId, uint32_t contractId, uint64_t amount);
std::vector<unsigned char> CreatePayload_SendPeggedCurrency(uint32_t propertyId, uint64_t amount);
std::vector<unsigned char> CreatePayload_MetaDExTrade(uint32_t propertyIdForSale, uint64_t amountForSale, uint32_t propertyIdDesired, uint64_t amountDesired);
std::vector<unsigned char> CreatePayload_MetaDExCancelPrice(uint32_t propertyIdForSale, uint64_t amountForSale, uint32_t propertyIdDesired, uint64_t amountDesired);
std::vector<unsigned char> CreatePayload_MetaDExCancelPair(uint32_t propertyIdForSale, uint32_t propertyIdDesired);
std::vector<unsigned char> CreatePayload_MetaDExCancelAll();
std::vector<unsigned char> CreatePayload_CreateOracleContract(std::string& name, uint32_t blocks_until_expiration, uint32_t notional_size, uint32_t collateral_currency, uint64_t margin_requirement, uint8_t inverse, std::vector<int>& kycVec);
std::vector<unsigned char> CreatePayload_Change_OracleAdm(uint32_t contractId);
std::vector<unsigned char> CreatePayload_OracleBackup(uint32_t contractId);
std::vector<unsigned char> CreatePayload_Close_Oracle(uint32_t contractId);
std::vector<unsigned char> CreatePayload_Set_Oracle(uint32_t contractId, uint64_t high, uint64_t low, uint64_t close);
std::vector<unsigned char> CreatePayload_Commit_Channel(uint32_t propertyId, uint64_t amount);
std::vector<unsigned char> CreatePayload_Withdrawal_FromChannel(uint32_t propertyId, uint64_t amount);
std::vector<unsigned char> CreatePayload_Instant_Trade(uint32_t propertyId, uint64_t amount, int blockheight_expiry, uint32_t propertyDesired, uint64_t amountDesired);
std::vector<unsigned char> CreatePayload_Contract_Instant_Trade(uint32_t contractId, uint64_t amount, uint32_t blockheight_expiry, uint64_t price, uint8_t trading_action, uint64_t leverage);
std::vector<unsigned char> CreatePayload_PNL_Update(uint32_t propertyId, uint64_t amount, uint32_t blockheight_expiry);
std::vector<unsigned char> CreatePayload_Transfer(uint8_t option, uint32_t propertyId, uint64_t amount);
std::vector<unsigned char> CreatePayload_New_Id_Registration(std::string& website, std::string& name);
std::vector<unsigned char> CreatePayload_Update_Id_Registration();
std::vector<unsigned char> CreatePayload_DEx_Payment();
std::vector<unsigned char> CreatePayload_TradeLayerAlert(uint16_t alertType, uint32_t expiryValue, const std::string& alertMessage);
std::vector<unsigned char> CreatePayload_Attestation(std::string& hash);
std::vector<unsigned char> CreatePayload_Revoke_Attestation();
std::vector<unsigned char> CreatePayload_MetaDExCancelEcosystem();
std::vector<unsigned char> CreatePayload_ContractDExCancel(std::string& hash);
std::vector<unsigned char> CreatePayload_Instant_LTC_Trade(uint32_t propertyId, uint64_t amount, uint64_t totalPrice, int blockheight_expiry);
std::vector<unsigned char> CreatePayload_DExCancel(std::string& hash);
std::vector<unsigned char> CreatePayload_MetaDExCancelPair(uint32_t propertyIdForSale, uint32_t propertyIdDesired);
std::vector<unsigned char>CreatePayload_MetaDExCancelPrice(uint32_t propertyIdForSale, int64_t amountForSale, uint32_t propertyIdDesired, int64_t amountDesired);
std::vector<unsigned char>CreatePayload_Close_Channel();

#endif // TRADELAYER_CREATEPAYLOAD_H
