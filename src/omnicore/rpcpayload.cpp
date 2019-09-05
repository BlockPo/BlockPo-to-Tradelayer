#include "omnicore/rpcpayload.h"

#include "omnicore/createpayload.h"
#include "omnicore/rpcvalues.h"
#include "omnicore/rpcrequirements.h"
#include "omnicore/omnicore.h"
#include "omnicore/sp.h"
#include "omnicore/tx.h"

#include "rpc/server.h"
#include "utilstrencodings.h"

#include <univalue.h>

using std::runtime_error;
using namespace mastercore;

UniValue tl_createpayload_simplesend(const JSONRPCRequest& request)
{
   if (request.params.size() != 2)
        throw runtime_error(
            "tl_createpayload_simplesend propertyid \"amount\"\n"

            "\nPayload to simple send transaction.\n"

            "\nArguments:\n"
            "1. propertyid           (number, required) the identifier of the tokens to send\n"
            "2. amount               (string, required) the amount to send\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_simplesend", "1 \"100.0\"")
            + HelpExampleRpc("tl_createpayload_simplesend", "1, \"100.0\"")
        );

    uint32_t propertyId = ParsePropertyId(request.params[0]);
    int64_t amount = ParseAmount(request.params[1], isPropertyDivisible(propertyId));

    std::vector<unsigned char> payload = CreatePayload_SimpleSend(propertyId, amount);

    return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_sendvestingtokens(const JSONRPCRequest& request)
{
    if (request.params.size() != 2)
        throw runtime_error(
            "tl_createpayload_sendvestingtokens propertyid \"amount\"\n"

            "\nPayload to send vesting tokens.\n"

            "\nArguments:\n"
            "1. propertyid           (number, required) the identifier of the tokens to send\n"
            "2. amount               (string, required) the amount to send\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_sendvestingtokens", "1 \"100.0\"")
            + HelpExampleRpc("tl_createpayload_sendvestingtokens", "1, \"100.0\"")
        );

    uint32_t propertyId = ParsePropertyId(request.params[0]);
    int64_t amount = ParseAmount(request.params[1], isPropertyDivisible(propertyId));

    std::vector<unsigned char> payload = CreatePayload_SendVestingTokens(propertyId, amount);

    return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_sendall(const JSONRPCRequest& request)
{
    if (request.params.size() != 1)
        throw runtime_error(
            "tl_createpayload_sendall \" ecosystem \n"

            "\nPayload to transfers all available tokens in the given ecosystem to the recipient.\n"

            "\nArguments:\n"
            "1. ecosystem            (number, required) the ecosystem of the tokens to send (1 for main ecosystem, 2 for test ecosystem)\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_sendall", "\"2")
            + HelpExampleRpc("tl_createpayload_sendall", "\"2")
        );

    uint8_t ecosystem = ParseEcosystem(request.params[0]);

    std::vector<unsigned char> payload = CreatePayload_SendAll(ecosystem);

    return HexStr(payload.begin(), payload.end());

}

UniValue tl_createpayload_issuancecrowdsale(const JSONRPCRequest& request)
{
    if (request.params.size() != 11)
        throw runtime_error(
            "tl_createpayload_issuancecrowdsale \" ecosystem type previousid \"name\" \"url\" \"data\" propertyiddesired tokensperunit deadline ( earlybonus issuerpercentage )\n"

            "Payload to create crowdsale."

            "\nArguments:\n"
            "1. ecosystem            (string, required) the ecosystem to create the tokens in (1 for main ecosystem, 2 for test ecosystem)\n"
            "2. type                 (number, required) the type of the tokens to create: (1 for indivisible tokens, 2 for divisible tokens)\n"
            "3. previousid           (number, required) an identifier of a predecessor token (0 for new crowdsales)\n"
            "4. name                 (string, required) the name of the new tokens to create\n"
            "5. url                  (string, required) an URL for further information about the new tokens (can be \"\")\n"
            "6. data                 (string, required) a description for the new tokens (can be \"\")\n"
            "7. propertyiddesired    (number, required) the identifier of a token eligible to participate in the crowdsale\n"
            "8. tokensperunit        (string, required) the amount of tokens granted per unit invested in the crowdsale\n"
            "9. deadline             (number, required) the deadline of the crowdsale as Unix timestamp\n"
            "10. earlybonus          (number, required) an early bird bonus for participants in percent per week\n"
            "11. issuerpercentage    (number, required) a percentage of tokens that will be granted to the issuer\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_issuancecrowdsale"," \" 2 1 0 \"Companies\" \"Bitcoin Mining\" \"Quantum Miner\" \"\" \"\" 2 \"100\" 1483228800 30 2")
            + HelpExampleRpc("tl_createpayload_issuancecrowdsale", " \"2, 1, 0, \"Companies\", \"Bitcoin Mining\", \"Quantum Miner\", \"\", \"\", 2, \"100\", 1483228800, 30, 2")
        );

    uint8_t ecosystem = ParseEcosystem(request.params[0]);
    uint16_t type = ParsePropertyType(request.params[1]);
    uint32_t previousId = ParsePreviousPropertyId(request.params[2]);
    std::string name = ParseText(request.params[3]);
    std::string url = ParseText(request.params[4]);
    std::string data = ParseText(request.params[5]);
    uint32_t propertyIdDesired = ParsePropertyId(request.params[6]);
    int64_t numTokens = ParseAmount(request.params[7], type);
    int64_t deadline = ParseDeadline(request.params[8]);
    uint8_t earlyBonus = ParseEarlyBirdBonus(request.params[9]);
    uint8_t issuerPercentage = ParseIssuerBonus(request.params[10]);

    std::vector<unsigned char> payload = CreatePayload_IssuanceVariable(ecosystem, type, previousId, name, url, data, propertyIdDesired, numTokens, deadline, earlyBonus, issuerPercentage);

    return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_issuancefixed(const JSONRPCRequest& request)
{
    if (request.params.size() != 7)
        throw runtime_error(
            "tl_createpayload_issuancefixed \" ecosystem type previousid \"name\" \"url\" \"data\" \"amount\"\n"

            "\nPayload to create new tokens with fixed supply.\n"

            "\nArguments:\n"
            "1. ecosystem            (string, required) the ecosystem to create the tokens in (1 for main ecosystem, 2 for test ecosystem)\n"
            "2. type                 (number, required) the type of the tokens to create: (1 for indivisible tokens, 2 for divisible tokens)\n"
            "3. previousid           (number, required) an identifier of a predecessor token (use 0 for new tokens)\n"
            "4. name                 (string, required) the name of the new tokens to create\n"
            "5. url                  (string, required) an URL for further information about the new tokens (can be \"\")\n"
            "6. data                 (string, required) a description for the new tokens (can be \"\")\n"
            "7. amount              (string, required) the number of tokens to create\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_issuancefixed", "\" 2 1 0 \"Quantum Miner\" \"\" \"\" \"1000000\"")
            + HelpExampleRpc("tl_createpayload_issuancefixed", "\", 2, 1, 0, \"Quantum Miner\", \"\", \"\", \"1000000\"")
        );

    uint8_t ecosystem = ParseEcosystem(request.params[0]);
    uint16_t type = ParsePropertyType(request.params[1]);
    uint32_t previousId = ParsePreviousPropertyId(request.params[2]);
    std::string name = ParseText(request.params[3]);
    std::string url = ParseText(request.params[4]);
    std::string data = ParseText(request.params[5]);
    int64_t amount = ParseAmount(request.params[6], type);

    std::vector<unsigned char> payload = CreatePayload_IssuanceFixed(ecosystem, type, previousId, name, url, data, amount);

    return HexStr(payload.begin(), payload.end());
}


UniValue tl_createpayload_issuancemanaged(const JSONRPCRequest& request)
{
    if (request.params.size() != 6)
        throw runtime_error(
            "tl_createpayload_issuancemanaged \" ecosystem type previousid \"name\" \"url\" \"data\"\n"

            "\nPayload to create new tokens with manageable supply.\n"

            "\nArguments:\n"
            "1. ecosystem            (string, required) the ecosystem to create the tokens in (1 for main ecosystem, 2 for test ecosystem)\n"
            "2. type                 (number, required) the type of the tokens to create: (1 for indivisible tokens, 2 for divisible tokens)\n"
            "3. previousid           (number, required) an identifier of a predecessor token (use 0 for new tokens)\n"
            "4. name                 (string, required) the name of the new tokens to create\n"
            "5. url                  (string, required) an URL for further information about the new tokens (can be \"\")\n"
            "6. data                 (string, required) a description for the new tokens (can be \"\")\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_issuancemanaged", "\" 2 1 0 \"Companies\" \"Bitcoin Mining\" \"Quantum Miner\" \"\" \"\"")
            + HelpExampleRpc("tl_createpayload_issuancemanaged", "\", 2, 1, 0, \"Companies\", \"Bitcoin Mining\", \"Quantum Miner\", \"\", \"\"")
        );

    uint8_t ecosystem = ParseEcosystem(request.params[0]);
    uint16_t type = ParsePropertyType(request.params[1]);
    uint32_t previousId = ParsePreviousPropertyId(request.params[2]);
    std::string name = ParseText(request.params[3]);
    std::string url = ParseText(request.params[4]);
    std::string data = ParseText(request.params[5]);

    std::vector<unsigned char> payload = CreatePayload_IssuanceManaged(ecosystem, type, previousId, name, url, data);

    return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_sendgrant(const JSONRPCRequest& request)
{
  if (request.params.size() != 2)
    throw runtime_error(
			"tl_createpayload_sendgrant \" propertyid \"amount\"\n"

			"\nPayload to issue or grant new units of managed tokens.\n"

			"\nArguments:\n"
			"1. propertyid           (number, required) the identifier of the tokens to grant\n"
			"2. amount               (string, required) the amount of tokens to create\n"

			"\nResult:\n"
			"\"payload\"             (string) the hex-encoded payload\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_createpayload_sendgrant", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\" \"\" 51 \"7000\"")
			+ HelpExampleRpc("tl_createpayload_sendgrant", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\", \"\", 51, \"7000\"")
			);


  uint32_t propertyId = ParsePropertyId(request.params[0]);
  int64_t amount = ParseAmount(request.params[1], isPropertyDivisible(propertyId));

  std::vector<unsigned char> payload = CreatePayload_Grant(propertyId, amount);

  return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_sendrevoke(const JSONRPCRequest& request)
{
    if (request.params.size() != 2)
        throw runtime_error(
            "tl_createpayload_sendrevoke \" propertyid \"amount\" ( \"memo\" )\n"

            "\nPayload to revoke units of managed tokens.\n"

            "\nArguments:\n"
            "1. propertyid           (number, required) the identifier of the tokens to revoke\n"
            "2. amount               (string, required) the amount of tokens to revoke\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_sendrevoke", "\" \"\" 51 \"100\"")
            + HelpExampleRpc("tl_createpayload_sendrevoke", "\", \"\", 51, \"100\"")
        );

    uint32_t propertyId = ParsePropertyId(request.params[0]);
    int64_t amount = ParseAmount(request.params[1], isPropertyDivisible(propertyId));

    std::vector<unsigned char> payload = CreatePayload_Revoke(propertyId, amount);

    return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_closecrowdsale(const JSONRPCRequest& request)
{
    if (request.params.size() != 1)
        throw runtime_error(
            "tl_createpayload_closecrowdsale \" propertyid\n"

            "\nPayload to Manually close a crowdsale.\n"

            "\nArguments:\n"
            "1. propertyid           (number, required) the identifier of the crowdsale to close\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_closecrowdsale", "\" 70")
            + HelpExampleRpc("tl_createpayload_closecrowdsale", "\", 70")
        );

    uint32_t propertyId = ParsePropertyId(request.params[0]);

    std::vector<unsigned char> payload = CreatePayload_CloseCrowdsale(propertyId);

    return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_changeissuer(const JSONRPCRequest& request)
{
    if (request.params.size() != 1)
        throw runtime_error(
            "tl_createpayload_changeissuer \" propertyid\n"

            "\nPayload to change the issuer on record of the given tokens.\n"

            "\nArguments:\n"
            "1. propertyid           (number, required) the identifier of the tokens\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_changeissuer", "\" 3")
            + HelpExampleRpc("tl_createpayload_changeissuer","\"  3")
        );

    uint32_t propertyId = ParsePropertyId(request.params[0]);

    std::vector<unsigned char> payload = CreatePayload_ChangeIssuer(propertyId);

    return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_sendactivation(const JSONRPCRequest& request)
{
    if (request.params.size() != 3)
        throw runtime_error(
            "tl_createpayload_sendactivation \" featureid block minclientversion\n"
            "\nPayload to activate a protocol feature.\n"
            "\nNote: Trade Layer Core ignores activations from unauthorized sources.\n"
            "\nArguments:\n"
            "1. featureid            (number, required) the identifier of the feature to activate\n"
            "2. block                (number, required) the activation block\n"
            "3. minclientversion     (number, required) the minimum supported client version\n"
            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"
            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_sendactivation", "\" 1 370000 999")
            + HelpExampleRpc("tl_createpayload_sendactivation", "\", 1, 370000, 999")
        );

    uint16_t featureId = request.params[0].get_int();
    uint32_t activationBlock = request.params[1].get_int();
    uint32_t minClientVersion = request.params[2].get_int();

    std::vector<unsigned char> payload = CreatePayload_ActivateFeature(featureId, activationBlock, minClientVersion);

    return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_senddeactivation(const JSONRPCRequest& request)
{
    if (request.params.size() != 1)
        throw runtime_error(
            "tl_senddeactivation \"fromaddress\" featureid\n"
            "\nPayload to deactivate a protocol feature.  For Emergency Use Only.\n"
            "\nNote: Trade Layer Core ignores deactivations from unauthorized sources.\n"
            "\nArguments:\n"
            "1. featureid             (number, required) the identifier of the feature to activate\n"
            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"
            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_senddeactivation", "\" 1")
            + HelpExampleRpc("tl_createpayload_senddeactivation", "\" 1")
        );

    uint16_t featureId = request.params[0].get_int64();

    std::vector<unsigned char> payload = CreatePayload_DeactivateFeature(featureId);

    return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_sendalert(const JSONRPCRequest& request)
{
    if (request.params.size() != 3)
        throw runtime_error(
            "tl_createpayload_sendalert \" alerttype expiryvalue typecheck versioncheck \"message\"\n"
            "\nPayload to creates and broadcasts an Trade Layer Core alert.\n"
            "\nNote: Trade Layer Core ignores alerts from unauthorized sources.\n"
            "\nArguments:\n"
            "1. alerttype            (number, required) the alert type\n"
            "2. expiryvalue          (number, required) the value when the alert expires (depends on alert type)\n"
            "3. message              (string, required) the user-faced alert message\n"
            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"
            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_sendalert", "")
            + HelpExampleRpc("tl_createpayload_sendalert", "")
        );

    int64_t tempAlertType = request.params[0].get_int64();
    if (tempAlertType < 1 || 65535 < tempAlertType) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Alert type is out of range");
    }
    uint16_t alertType = static_cast<uint16_t>(tempAlertType);
    int64_t tempExpiryValue = request.params[1].get_int64();
    if (tempExpiryValue < 1 || 4294967295LL < tempExpiryValue) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Expiry value is out of range");
    }
    uint32_t expiryValue = static_cast<uint32_t>(tempExpiryValue);
    std::string alertMessage = ParseText(request.params[2]);

    std::vector<unsigned char> payload = CreatePayload_OmniCoreAlert(alertType, expiryValue, alertMessage);

    return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_sendtrade(const JSONRPCRequest& request)
{
  if (request.params.size() != 4) {
    throw runtime_error(
			"tl_createpayload_sendtrade \"fromaddress\" propertyidforsale \"amountforsale\" propertiddesired \"amountdesired\"\n"

			"\nPayload to place a trade offer on the distributed token exchange.\n"

			"\nArguments:\n"
			"1. propertyidforsale    (number, required) the identifier of the tokens to list for sale\n"
			"2. amountforsale        (string, required) the amount of tokens to list for sale\n"
			"3. propertiddesired     (number, required) the identifier of the tokens desired in exchange\n"
			"4. amountdesired        (string, required) the amount of tokens desired in exchange\n"

			"\nResult:\n"
			"\"payload\"             (string) the hex-encoded payload\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_createpayload_sendtrade", "\" 31 \"250.0\" 1 \"10.0\"")
			+ HelpExampleRpc("tl_createpayload_sendtrade", "\" 31, \"250.0\", 1, \"10.0\"")
			);
  }

  uint32_t propertyIdForSale = ParsePropertyId(request.params[0]);
  int64_t amountForSale = ParseAmount(request.params[1], isPropertyDivisible(propertyIdForSale));
  uint32_t propertyIdDesired = ParsePropertyId(request.params[2]);
  int64_t amountDesired = ParseAmount(request.params[3], isPropertyDivisible(propertyIdDesired));

  std::vector<unsigned char> payload = CreatePayload_MetaDExTrade(propertyIdForSale, amountForSale, propertyIdDesired, amountDesired);

  return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_createcontract(const JSONRPCRequest& request)
{
  if (request.params.size() != 7)
    throw runtime_error(
			"tl_createpayload_createcontract \" ecosystem type previousid \"category\" \"subcategory\" \"name\" \"url\" \"data\" propertyiddesired tokensperunit deadline ( earlybonus issuerpercentage )\n"

			"Payload to create new Future Contract."

			"\nArguments:\n"
			"1. ecosystem                 (string, required) the ecosystem to create the tokens in (1 for main ecosystem, 2 for test ecosystem)\n"
			"2. numerator                 (number, required) 4: ALL, 5: sLTC, 6: LTC.\n"
			"3. name                      (string, required) the name of the new tokens to create\n"
			"4. blocks until expiration   (number, required) life of contract, in blocks\n"
			"5. notional size             (number, required) notional size\n"
			"6. collateral currency       (number, required) collateral currency\n"
      "7. margin requirement        (number, required) margin requirement\n"

			"\nResult:\n"
			"\"payload\"             (string) the hex-encoded payload\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_createpayload_createcontract", "2 1 0 \"Companies\" \"Bitcoin Mining\" \"Quantum Miner\" \"\" \"\" 2 \"100\" 1483228800 30 2 4461 100 1 25")
			+ HelpExampleRpc("tl_createpayload_createcontract", "2, 1, 0, \"Companies\", \"Bitcoin Mining\", \"Quantum Miner\", \"\", \"\", 2, \"100\", 1483228800, 30, 2, 4461, 100, 1, 25")
			);

  uint8_t ecosystem = ParseEcosystem(request.params[0]);
  uint32_t type = ParseContractType(request.params[1]);
  std::string name = ParseText(request.params[2]);
  uint32_t blocks_until_expiration = ParseNewValues(request.params[3]);
  uint32_t notional_size = ParseNewValues(request.params[4]);
  uint32_t collateral_currency = ParseNewValues(request.params[5]);
  uint32_t margin_requirement = ParseAmount(request.params[6], true);

  std::vector<unsigned char> payload = CreatePayload_CreateContract(ecosystem, type, name, blocks_until_expiration, notional_size, collateral_currency, margin_requirement);

  return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_tradecontract(const JSONRPCRequest& request)
{
  if (request.params.size() != 5)
    throw runtime_error(
			"tl_createpayload_tradecontract \" propertyidforsale \"amountforsale\" propertiddesired \"amountdesired\"\n"

			"\nPayload to place a trade offer on the distributed Futures Contracts exchange.\n"

			"\nArguments:\n"
		  "1. propertyidforsale    (number, required) the identifier of the contract to list for trade\n"
			"2. amountforsale        (number, required) the amount of contracts to trade\n"
			"3. effective price      (number, required) limit price desired in exchange\n"
			"4. trading action       (number, required) 1 to BUY contracts, 2 to SELL contracts \n"
			"5. leverage             (number, required) leverage (2x, 3x, ... 10x)\n"
			"\nResult:\n"
			"\"payload\"             (string) the hex-encoded payload\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_tradecontract", "\"250.0\"1\"10.0\"70.0\"80.0\"")
			+ HelpExampleRpc("tl_tradecontract", ",\"250.0\",1,\"10.0,\"70.0,\"80.0\"")
			);

  std::string name_traded = ParseText(request.params[0]);
  int64_t amountForSale = ParseAmountContract(request.params[1]);
  uint64_t effective_price = ParseEffectivePrice(request.params[2]);
  uint8_t trading_action = ParseContractDexAction(request.params[3]);
  uint64_t leverage = ParseLeverage(request.params[4]);

  std::vector<unsigned char> payload = CreatePayload_ContractDexTrade(name_traded, amountForSale, effective_price, trading_action, leverage);

  return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_cancelallcontractsbyaddress(const JSONRPCRequest& request)
{
  if (request.params.size() != 2)
    throw runtime_error(
			"tl_cancelallcontractsbyaddress \" ecosystem\n"

			"\nPayload to cancel all offers on a given Futures Contract .\n"

			"\nArguments:\n"
			"1. ecosystem            (number, required) the ecosystem of the offers to cancel (1 for main ecosystem, 2 for test ecosystem)\n"
			"2. contractId           (number, required) the Id of Future Contract \n"
			"\nResult:\n"
			"\"payload\"             (string) the hex-encoded payload\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_createpayload_cancelallcontractsbyaddress", "\" 3")
			+ HelpExampleRpc("tl_createpayload_cancelallcontractsbyaddress", "\" 3")
			);

  uint8_t ecosystem = ParseEcosystem(request.params[0]);
  std::string name_traded = ParseText(request.params[1]);

  struct FutureContractObject *pfuture = getFutureContractObject(ALL_PROPERTY_TYPE_CONTRACT, name_traded);
  uint32_t contractId = pfuture->fco_propertyId;

  std::vector<unsigned char> payload = CreatePayload_ContractDexCancelEcosystem(ecosystem, contractId);

  return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_closeposition(const JSONRPCRequest& request)
{
    if (request.params.size() != 2)
        throw runtime_error(
            "tl_createpayload_closeposition \" ecosystem \" contractId\n"

            "\nPayload to close the position on a given Futures Contract .\n"

            "\nArguments:\n"
            "1. ecosystem            (number, required) the ecosystem of the offers to cancel (1 for main ecosystem, 2 for test ecosystem)\n"
            "2. contractId           (number, required) the Id of Future Contract \n"
            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_closeposition", "\" 1, 3")
            + HelpExampleRpc("tl_createpayload_closeposition", "\", 1, 3")
        );

    uint8_t ecosystem = ParseEcosystem(request.params[0]);
    uint32_t contractId = ParsePropertyId(request.params[1]);

    std::vector<unsigned char> payload = CreatePayload_ContractDexClosePosition(ecosystem, contractId);

    return HexStr(payload.begin(), payload.end());
}


UniValue tl_createpayload_sendissuance_pegged(const JSONRPCRequest& request)
{
  if (request.params.size() != 7)
    throw runtime_error(
			"tl_createpayload_sendissuance_pegged\" ecosystem type previousid \"category\" \"subcategory\" \"name\" \"url\" \"data\"\n"

			"\nPayload to create new pegged currency with manageable supply.\n"

			"\nArguments:\n"
			"1. ecosystem             (string, required) the ecosystem to create the pegged currency in (1 for main ecosystem, 2 for test ecosystem)\n"
			"2. type                  (number, required) the type of the pegged to create: (1 for indivisible tokens, 2 for divisible tokens)\n"
			"3. previousid            (number, required) an identifier of a predecessor token (use 0 for new tokens)\n"
			"4. name                  (string, required) the name of the new pegged to create\n"
			"5. collateralcurrency    (number, required) the collateral currency for the new pegged \n"
			"6. future contract name  (number, required) the future contract name for the new pegged \n"
			"7. amount of pegged      (number, required) amount of pegged to create \n"
			"\nResult:\n"
			"\"payload\"              (string) the hex-encoded payload\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_createpayload_sendissuance_pegged", "\" 2 1 0 \"Companies\" \"Bitcoin Mining\" \"Quantum Miner\" \"\" \"\"")
			+ HelpExampleRpc("tl_createpayload_sendissuance_pegged", "\", 2, 1, 0, \"Companies\", \"Bitcoin Mining\", \"Quantum Miner\", \"\", \"\"")
			);

  uint8_t ecosystem = ParseEcosystem(request.params[0]);
  uint16_t type = ParsePropertyType(request.params[1]);
  uint32_t previousId = ParsePreviousPropertyId(request.params[2]);
  std::string name = ParseText(request.params[3]);
  uint32_t propertyId = ParsePropertyId(request.params[4]);
  std::string name_traded = ParseText(request.params[5]);
  uint64_t amount = ParseAmount(request.params[6], isPropertyDivisible(propertyId));

  struct FutureContractObject *pfuture = getFutureContractObject(ALL_PROPERTY_TYPE_CONTRACT, name_traded);
  uint32_t contractId = pfuture->fco_propertyId;

  std::vector<unsigned char> payload = CreatePayload_IssuancePegged(ecosystem, type, previousId, name, propertyId, contractId, amount);

  return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_send_pegged(const JSONRPCRequest& request)
{
  if (request.params.size() != 2)
    throw runtime_error(
			"tl_send \" propertyid \"amount\" ( \"redeemaddress\" \"referenceamount\" )\n"

			"\nPayload to send the pegged currency to other addresses.\n"

			"\nArguments:\n"
			"1. property name        (string, required) the identifier of the tokens to send\n"
			"2. amount               (string, required) the amount to send\n"


			"\nResult:\n"
			"\"payload\"             (string) the hex-encoded payload\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_createpayload_send_pegged", "\" 1 \"100.0\"")
			+ HelpExampleRpc("tl_createpayload_send_pegged", "\", 1, \"100.0\"")
			);

  std::string name_pegged = ParseText(request.params[0]);

  struct FutureContractObject *pfuture = getFutureContractObject(ALL_PROPERTY_TYPE_PEGGEDS, name_pegged);
  uint32_t propertyId = pfuture->fco_propertyId;

  int64_t amount = ParseAmount(request.params[1], true);

  std::vector<unsigned char> payload = CreatePayload_SendPeggedCurrency(propertyId, amount);

  return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_redemption_pegged(const JSONRPCRequest& request)
{
    if (request.params.size() != 3)
       throw runtime_error(
			  "tl_createpayload_redemption_pegged \" propertyid \"amount\" ( \"redeemaddress\" distributionproperty )\n"

			  "\n Payload to redeem pegged currency .\n"

			  "\nArguments:\n"
			  "1. name of pegged       (string, required) name of the tokens to redeem\n"
			  "2. amount               (number, required) the amount of pegged currency for redemption"
			  "3. name of contract     (string, required) the identifier of the future contract involved\n"
			  "\nResult:\n"
			  "\"payload\"             (string) the hex-encoded payload\n"

		  	"\nExamples:\n"
		  	+ HelpExampleCli("tl_createpayload_redemption_pegged", "\" , 1")
		  	+ HelpExampleRpc("tl_createpayload_redemption_pegged", "\", 1")
			  );

    std::string name_pegged = ParseText(request.params[0]);
    std::string name_contract = ParseText(request.params[2]);
    struct FutureContractObject *pfuture_pegged = getFutureContractObject(ALL_PROPERTY_TYPE_PEGGEDS, name_pegged);
    uint32_t propertyId = pfuture_pegged->fco_propertyId;

    uint64_t amount = ParseAmount(request.params[1], true);

    struct FutureContractObject *pfuture_contract = getFutureContractObject(ALL_PROPERTY_TYPE_CONTRACT, name_contract);
    uint32_t contractId = pfuture_contract->fco_propertyId;

    std::vector<unsigned char> payload = CreatePayload_RedemptionPegged(propertyId, contractId, amount);

    return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_cancelorderbyblock(const JSONRPCRequest& request)
{
    if (request.params.size() != 2)
        throw runtime_error(
            "tl_createpayload_cancelorderbyblock \"block\" idx\n"

            "\nPayload to cancel an specific offer on the distributed token exchange.\n"

            "\nArguments:\n"
            "1. block           (number, required) the block of order to cancel\n"
            "2. idx             (number, required) the idx in block of order to cancel\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_cancelorderbyblock", "\" 1, 2")
            + HelpExampleRpc("tl_createpayload_cancelorderbyblock", "\", 1, 2")
        );

    int block = static_cast<int>(ParseNewValues(request.params[0]));
    int idx = static_cast<int>(ParseNewValues(request.params[1]));

    std::vector<unsigned char> payload = CreatePayload_ContractDexCancelOrderByTxId(block,idx);

    return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_dexoffer(const JSONRPCRequest& request)
{
  if (request.params.size() != 7) {
    throw runtime_error(
			"tl_createpayload_dexsell \" propertyidforsale \"amountforsale\" \"amountdesired\" paymentwindow minacceptfee action\n"

			"\nPayload to place, update or cancel a sell offer on the traditional distributed Trade Layer/LTC exchange.\n"

			"\nArguments:\n"
			"1. propertyidoffer     (number, required) the identifier of the tokens to list for sale (must be 1 for OMNI or 2 for TOMNI)\n"
			"2. amountoffering      (string, required) the amount of tokens to list for sale\n"
			"3. price               (string, required) the price in litecoin of the offer \n"
			"4. paymentwindow       (number, required) a time limit in blocks a buyer has to pay following a successful accepting order\n"
			"5. minacceptfee        (string, required) a minimum mining fee a buyer has to pay to accept the offer\n"
			"6. option              (number, required) 1 for buy tokens, 2 to sell\n"
			"7. action              (number, required) the action to take (1 for new offers, 2 to update\", 3 to cancel)\n"

			"\nResult:\n"
			"\"payload\"             (string) the hex-encoded payload\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_createpayload_dexsell", "\" 1 \"1.5\" \"0.75\" 25 \"0.0005\" 1")
			+ HelpExampleRpc("tl_createpayload_dexsell", "\", 1, \"1.5\", \"0.75\", 25, \"0.0005\", 1")
			);
  }

  uint32_t propertyIdForSale = ParsePropertyId(request.params[0]);
  int64_t amountForSale = ParseAmount(request.params[1], true); // TMSC/MSC is divisible
  int64_t price = ParseAmount(request.params[2], true); // BTC is divisible
  uint8_t paymentWindow = ParseDExPaymentWindow(request.params[3]);
  int64_t minAcceptFee = ParseDExFee(request.params[4]);
  int64_t option = ParseAmount(request.params[5], false);  // buy : 1 ; sell : 2;
  uint8_t action = ParseDExAction(request.params[6]);

  std::vector<unsigned char> payload;

  if (option == 1)
  {
      payload = CreatePayload_DEx(propertyIdForSale, amountForSale, price, paymentWindow, minAcceptFee, action);
  } else if (option == 2) {
      payload = CreatePayload_DExSell(propertyIdForSale, amountForSale, price, paymentWindow, minAcceptFee, action);
  }

  return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_dexaccept(const JSONRPCRequest& request)
{
    if (request.params.size() != 2)
        throw runtime_error(
            "tl_createpayload_dexaccept \" propertyid \"amount\n"

            "\nPayload to create and broadcast an accept offer for the specified token and amount.\n"

            "\nArguments:\n"
            "1. propertyid           (number, required) the identifier of the token traded\n"
            "2. amount               (string, required) the amount traded\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_dexaccept", "\" 1 \"15.0\"")
            + HelpExampleRpc("tl_createpayload_dexaccept", "\1, \"15.0\"")
        );

    uint32_t propertyId = ParsePropertyId(request.params[0]);
    int64_t amount = ParseAmount(request.params[1], true); // MSC/TMSC is divisible

    std::vector<unsigned char> payload = CreatePayload_DExAccept(propertyId, amount);

    return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_sendvesting(const JSONRPCRequest& request)
{
  if (request.params.size() < 2 || request.params.size() > 2)
    throw runtime_error(
			"tl_createpayload_sendvesting \"fromaddress\" \"toaddress\" propertyid \"amount\" ( \"referenceamount\" )\n"

			"\nCreate and broadcast a simple send transaction.\n"

			"\nArguments:\n"
			"1. propertyid           (number, required) the identifier of the tokens to send\n"
			"2. amount               (string, required) the amount of vesting tokens to send\n"

			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_createpayload_sendvesting", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" 1 \"100.0\"")
			+ HelpExampleRpc("tl_createpayload_sendvesting", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\", 1, \"100.0\"")
			);

  // obtain parameters & info
  uint32_t propertyId = ParsePropertyId(request.params[0]); /** id=3 Vesting Tokens**/
  int64_t amount = ParseAmount(request.params[1], true);

  PrintToLog("propertyid = %d\n", propertyId);
  PrintToLog("amount = %d\n", amount);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_SendVestingTokens(propertyId, amount);

    return HexStr(payload.begin(), payload.end());

}


UniValue tl_createpayload_instant_trade(const JSONRPCRequest& request)
{
  if (request.params.size() < 5)
    throw runtime_error(
			"tl_createpayload_instant_trade \"fromaddress\" \"toaddress\" propertyid \"amount\" ( \"referenceamount\" )\n"

			"\nCreate an instant trade payload.\n"

			"\nArguments:\n"
			"1. propertyId            (number, required) the identifier of the property\n"
			"2. amount                (string, required) the amount of the property traded for the first address of channel\n"
      "3. blockheight_expiry    (string, required) block of expiry\n"
      "4. propertyDesired       (number, optional) the identifier of the property traded for the second address of channel\n"
      "5. amountDesired         (string, optional) the amount desired of tokens\n"


			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_createpayload_instant_trade", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" 1 \"100.0\"")
			+ HelpExampleRpc("tl_createpayload_instant_trade", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\", 1, \"100.0\"")
			);

  // obtain parameters & info
  uint32_t propertyId = ParsePropertyId(request.params[0]);
  int64_t amount = ParseAmount(request.params[1], true);
  uint32_t blockheight_expiry = request.params[2].get_int();
  uint32_t propertyDesired = ParsePropertyId(request.params[3]);
  int64_t amountDesired = ParseAmount(request.params[4],true);


  PrintToLog("propertyid = %d\n", propertyId);
  PrintToLog("amount = %d\n", amount);
  PrintToLog("blockheight_expiry = %d\n", blockheight_expiry);
  PrintToLog("propertyDesired = %d\n", propertyDesired);
  PrintToLog("amountDesired = %d\n", amountDesired);

  // create a payload for the transaction
  std::vector<unsigned char> payload = CreatePayload_Instant_Trade(propertyId, amount, blockheight_expiry, propertyDesired, amountDesired);

  return HexStr(payload.begin(), payload.end());

}

UniValue tl_createpayload_contract_instant_trade(const JSONRPCRequest& request)
{
  if (request.params.size() < 6)
    throw runtime_error(
			"tl_createpayload_instant_trade \"fromaddress\" \"toaddress\" propertyid \"amount\" ( \"referenceamount\" )\n"

			"\nCreate an contract instant trade payload.\n"

			"\nArguments:\n"
			"1. contractId            (number, required) the identifier of the property\n"
			"2. amount                (string, required) the amount of the property traded for the first address of channel\n"
      "3. blockheight_expiry    (string, required) block of expiry\n"
      "4. effective price       (string, required) limit price desired in exchange\n"
			"5. trading action        (number, required) 1 to BUY contracts, 2 to SELL contracts \n"
			"6. leverage              (number, required) leverage (2x, 3x, ... 10x)\n"

			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_createpayload_contract_instant_trade", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" 1 \"100.0\"")
			+ HelpExampleRpc("tl_createpayload_contract_instant_trade", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\", 1, \"100.0\"")
			);

  // obtain parameters & info
  uint32_t contractId = ParsePropertyId(request.params[0]);
  int64_t amount = ParseAmount(request.params[1], true);
  uint32_t blockheight_expiry = request.params[2].get_int();
  uint64_t price = ParseAmount(request.params[3],true);
  uint8_t trading_action = ParseContractDexAction(request.params[4]);
  uint64_t leverage = ParseLeverage(request.params[5]);


  PrintToLog("propertyid = %d\n", contractId);
  PrintToLog("amount = %d\n", amount);
  PrintToLog("blockheight_expiry = %d\n", blockheight_expiry);
  PrintToLog("price= %d\n", price);

  // create a payload for the transaction
  std::vector<unsigned char> payload = CreatePayload_Contract_Instant_Trade(contractId, amount, blockheight_expiry, price, trading_action, leverage);

  return HexStr(payload.begin(), payload.end());

}

UniValue tl_createpayload_pnl_update(const JSONRPCRequest& request)
{
  if (request.params.size() != 3)
    throw runtime_error(
			"tl_createpayload_pnl_update \"fromaddress\" \"toaddress\" propertyid \"amount\" ( \"referenceamount\" )\n"

			"\nCreate an pnl update payload.\n"

			"\nArguments:\n"
			"1. propertyId            (number, required) the identifier of the property\n"
			"2. amount                (string, required) the amount of the property traded\n"
      "3. blockheight expiry    (number, required) block of expiry\n"

			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_createpayload_pnl_update", "\" 1 \"15.0\"")
			+ HelpExampleRpc("tl_createpayload_pnl_update", "\", 1 \"15.0\"")
			);

  // obtain parameters & info
  uint32_t propertyId = ParsePropertyId(request.params[0]);
  int64_t amount = ParseAmount(request.params[1], true);
  uint32_t blockheight_expiry = request.params[2].get_int();

  PrintToLog("propertyid = %d\n", propertyId);
  PrintToLog("amount = %d\n", amount);
  PrintToLog("blockheight_expiry = %d\n", blockheight_expiry);

  // create a payload for the transaction
  std::vector<unsigned char> payload = CreatePayload_PNL_Update(propertyId, amount, blockheight_expiry);

  return HexStr(payload.begin(), payload.end());

}

//Params: propertyid, amount, reference vOut for destination address

UniValue tl_createpayload_transfer(const JSONRPCRequest& request)
{
  if (request.params.size() != 2)
    throw runtime_error(
			"tl_createpayload_transfer \"fromaddress\" \"toaddress\" propertyid \"amount\" ( \"referenceamount\" )\n"

			"\nCreate an transfer payload.\n"

			"\nArguments:\n"
			"1. propertyId            (number, required) the identifier of the property\n"
			"2. amount                (string, required) the amount of the property traded\n"

			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_createpayload_transfer", "\" 1 \"15.0\"")
			+ HelpExampleRpc("tl_createpayload_transfer", "\", 1 \"15.0\"")
			);

  // obtain parameters & info
  uint32_t propertyId = ParsePropertyId(request.params[0]);
  int64_t amount = ParseAmount(request.params[1], true);


  PrintToLog("propertyid = %d\n", propertyId);
  PrintToLog("amount = %d\n", amount);

  // create a payload for the transaction
  std::vector<unsigned char> payload = CreatePayload_Transfer(propertyId, amount);

  return HexStr(payload.begin(), payload.end());

}

static const CRPCCommand commands[] =
  { //  category                         name                                             actor (function)                               okSafeMode
    //  -------------------------------- -----------------------------------------       ----------------------------------------        ----------
    { "trade layer (payload creation)", "tl_createpayload_simplesend",                    &tl_createpayload_simplesend,                      {}   },
    { "trade layer (payload creation)", "tl_createpayload_sendvestingtokens",             &tl_createpayload_sendvestingtokens,               {}   },
    { "trade layer (payload creation)", "tl_createpayload_sendall",                       &tl_createpayload_sendall,                         {}   },
    { "trade layer (payload creation)", "tl_createpayload_issuancecrowdsale",             &tl_createpayload_issuancecrowdsale,               {}   },
    { "trade layer (payload creation)", "tl_createpayload_issuancefixed",                 &tl_createpayload_issuancefixed,                   {}   },
    { "trade layer (payload creation)", "tl_createpayload_issuancemanaged",               &tl_createpayload_issuancemanaged,                 {}   },
    { "trade layer (payload creation)", "tl_createpayload_sendgrant",                     &tl_createpayload_sendgrant,                       {}   },
    { "trade layer (payload creation)", "tl_createpayload_sendrevoke",                    &tl_createpayload_sendrevoke,                      {}   },
    { "trade layer (payload creation)", "tl_createpayload_closecrowdsale",                &tl_createpayload_closecrowdsale,                  {}   },
    { "trade layer (payload creation)", "tl_createpayload_changeissuer",                  &tl_createpayload_changeissuer,                    {}   },
    { "trade layer (payload creation)", "tl_createpayload_sendactivation",                &tl_createpayload_sendactivation,                  {}   },
    { "trade layer (payload creation)", "tl_createpayload_senddeactivation",              &tl_createpayload_senddeactivation,                {}   },

    { "trade layer (payload creation)", "tl_createpayload_sendalert",                     &tl_createpayload_sendalert,                       {}   },
    { "trade layer (payload creation)", "tl_createpayload_sendtrade",                     &tl_createpayload_sendtrade,                       {}   },
    { "trade layer (payload creation)", "tl_createpayload_createcontract",                &tl_createpayload_createcontract,                  {}   },
    { "trade layer (payload creation)", "tl_createpayload_tradecontract",                 &tl_createpayload_tradecontract,                   {}   },
    { "trade layer (payload creation)", "tl_createpayload_cancelallcontractsbyaddress",   &tl_createpayload_cancelallcontractsbyaddress,     {}   },
    { "trade layer (payload creation)", "tl_createpayload_closeposition",                 &tl_createpayload_closeposition,                   {}   },
    { "trade layer (payload creation)", "tl_createpayload_sendissuance_pegged",           &tl_createpayload_sendissuance_pegged,             {}   },
    { "trade layer (payload creation)", "tl_createpayload_send_pegged",                   &tl_createpayload_send_pegged,                     {}   },
    { "trade layer (payload creation)", "tl_createpayload_redemption_pegged",             &tl_createpayload_redemption_pegged,               {}   },
    { "trade layer (payload creation)", "tl_createpayload_cancelorderbyblock",            &tl_createpayload_cancelorderbyblock,              {}   },
    { "trade layer (payload creation)", "tl_createpayload_dexoffer",                      &tl_createpayload_dexoffer,                        {}   },
    { "trade layer (payload creation)", "tl_createpayload_dexaccept",                     &tl_createpayload_dexaccept,                       {}   },
    { "trade layer (payload creation)", "tl_createpayload_sendvesting",                   &tl_createpayload_sendvesting,                     {}   },
    { "trade layer (payload creation)", "tl_createpayload_instant_trade",                 &tl_createpayload_instant_trade,                   {}   },
    { "trade layer (payload creation)", "tl_createpayload_pnl_update",                    &tl_createpayload_pnl_update,                      {}   },
    { "trade layer (payload creation)", "tl_createpayload_transfer",                      &tl_createpayload_transfer,                        {}   },
    { "trade layer (payload creation)", "tl_createpayload_contract_instant_trade",        &tl_createpayload_contract_instant_trade,          {}   }
  };



void RegisterOmniPayloadCreationRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
