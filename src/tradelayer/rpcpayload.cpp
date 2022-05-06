#include <tradelayer/rpcpayload.h>

#include <tradelayer/createpayload.h>
#include <tradelayer/rpcrequirements.h>
#include <tradelayer/rpcvalues.h>
#include <tradelayer/sp.h>
#include <tradelayer/tradelayer.h>
#include <tradelayer/tx.h>

#include <rpc/server.h>
#include <rpc/util.h>
#include <util/strencodings.h>

#include <univalue.h>

using std::runtime_error;
using namespace mastercore;

UniValue tl_createpayload_simplesend(const JSONRPCRequest& request)
{
   if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "tl_createpayload_simplesend \"propertyid\" \"amount\"\n"

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

UniValue tl_createpayload_sendmany(const JSONRPCRequest& request)
{
   if (request.fHelp || request.params.size() < 2 || request.params.size() > 5)
        throw runtime_error(
            "tl_createpayload_sendmany \"propertyid\" \"amount\"\n"

            "\nPayload to simple send transaction.\n"

            "\nArguments:\n"
            "1. propertyid            (number, required) the identifier of the tokens to send\n"
            "2. amount1               (string, required) the amount to send\n"
            "3. amount2               (string, optional) the amount to send\n"
            "4. amount3               (string, optional) the amount to send\n"
            "5. amount4               (string, optional) the amount to send\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_sendmany", "1 \"100.0\", \"200.0\", \"300.0\", \"400.0\"")
            + HelpExampleCli("tl_createpayload_sendmany", "1 \"110.0\", \"220.0\", \"330.0\", \"440.0\"")
        );

    uint32_t propertyId = ParsePropertyId(request.params[0]);
    std::vector<uint64_t> amounts;
    const int allAmounts = request.params.size();
    for (int i = 1; i < allAmounts ; ++i) {
      auto n = ParseAmount(request.params[i], isPropertyDivisible(propertyId));
      amounts.push_back(n);
    }

    auto payload = CreatePayload_SendMany(propertyId, amounts);

    return HexStr(payload);
}

UniValue tl_createpayload_sendall(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw runtime_error(
            "tl_createpayload_sendall \" \n"

            "\nPayload to transfers all available tokens  to the recipient.\n"

            "\nArguments:\n"
            "\nno arguments\n"
            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_sendall", "\"")
            + HelpExampleRpc("tl_createpayload_sendall", "\"")
        );

    std::vector<unsigned char> payload = CreatePayload_SendAll();

    return HexStr(payload.begin(), payload.end());

}

UniValue tl_createpayload_issuancefixed(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 7)
        throw runtime_error(
            "tl_createpayload_issuancefixed \"type\" \"previousid\" \"name\" \"url\" \"data\" \"amount\"  \"kyc\"\n"

            "\nPayload to create new tokens with fixed supply.\n"

            "\nArguments:\n"
            "1. type                 (number, required) the type of the tokens to create: (1 for indivisible tokens, 2 for divisible tokens)\n"
            "2. previousid           (number, required) an identifier of a predecessor token (use 0 for new tokens)\n"
            "3. name                 (string, required) the name of the new tokens to create\n"
            "4. url                  (string, required) an URL for further information about the new tokens (can be \"\")\n"
            "5. data                 (string, required) a description for the new tokens (can be \"\")\n"
            "6. amount               (string, required) the number of tokens to create\n"
            "7. kyc options          (array, required) A json with the kyc allowed.\n"
            "    [\n"
            "      2,3,5             (number) kyc id\n"
            "      ,...\n"
            "    ]\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_issuancefixed", "\" 2 1 0 \"Quantum Miner\" \"\" \"\" \"1000000\" \"[1,3,5]\"")
            + HelpExampleRpc("tl_createpayload_issuancefixed", "\", 2, 1, 0, \"Quantum Miner\", \"\", \"\", \"1000000\"\"[1,3]\"")
        );

    uint16_t type = ParsePropertyType(request.params[0]);
    uint32_t previousId = ParsePreviousPropertyId(request.params[1]);
    std::string name = ParseText(request.params[2]);
    std::string url = ParseText(request.params[3]);
    std::string data = ParseText(request.params[4]);
    int64_t amount = ParseAmount(request.params[5], type);
    std::vector<int> numbers = ParseArray(request.params[6]);

    std::vector<unsigned char> payload = CreatePayload_IssuanceFixed(type, previousId, name, url, data, amount, numbers);

    return HexStr(payload.begin(), payload.end());
}


UniValue tl_createpayload_issuancemanaged(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 6)
        throw runtime_error(
            "tl_createpayload_issuancemanaged  \"type\" \"previousid\" \"name\" \"url\" \"data\"  \"kyc\"\n"

            "\nPayload to create new tokens with manageable supply.\n"

            "\nArguments:\n"
            "1. type                 (number, required) the type of the tokens to create: (1 for indivisible tokens, 2 for divisible tokens)\n"
            "2. previousid           (number, required) an identifier of a predecessor token (use 0 for new tokens)\n"
            "3. name                 (string, required) the name of the new tokens to create\n"
            "4. url                  (string, required) an URL for further information about the new tokens (can be \"\")\n"
            "5. data                 (string, required) a description for the new tokens (can be \"\")\n"
            "6. kyc options          (array, required) A json with the kyc allowed.\n"
            "    [\n"
            "      2,3,5             (number) kyc id\n"
            "      ,...\n"
            "    ]\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_issuancemanaged", "\" 2 0 \"Companies\" \"Bitcoin Mining\" \"Quantum Miner\" \"[1,4,6]\"")
            + HelpExampleRpc("tl_createpayload_issuancemanaged", "\", 2, 0, \"Companies\", \"Bitcoin Mining\", \"Quantum Miner\",\"[1,4,6]\"")
        );

    uint16_t type = ParsePropertyType(request.params[0]);
    uint32_t previousId = ParsePreviousPropertyId(request.params[1]);
    std::string name = ParseText(request.params[2]);
    std::string url = ParseText(request.params[3]);
    std::string data = ParseText(request.params[4]);
    std::vector<int> numbers = ParseArray(request.params[5]);

    std::vector<unsigned char> payload = CreatePayload_IssuanceManaged(type, previousId, name, url, data, numbers);

    return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_sendgrant(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() != 2)
    throw runtime_error(
			"tl_createpayload_sendgrant \" propertyid \"amount\"\n"

			"\nPayload to issue or grant new units of managed tokens.\n"

			"\nArguments:\n"
			"1. propertyid           (number, required) the identifier of the tokens to grant\n"
			"2. amount               (string, required) the amount of tokens to create\n"

			"\nResult:\n"
			"\"payload\"             (string) the hex-encoded payload\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_createpayload_sendgrant", "\"51 \"7000\"")
			+ HelpExampleRpc("tl_createpayload_sendgrant", "\"51\", \"7000\"")
			);


  uint32_t propertyId = ParsePropertyId(request.params[0]);
  int64_t amount = ParseAmount(request.params[1], isPropertyDivisible(propertyId));

  std::vector<unsigned char> payload = CreatePayload_Grant(propertyId, amount);

  return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_sendrevoke(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "tl_createpayload_sendrevoke \" propertyid \"amount\" \n"

            "\nPayload to revoke units of managed tokens.\n"

            "\nArguments:\n"
            "1. propertyid           (number, required) the identifier of the tokens to revoke\n"
            "2. amount               (string, required) the amount of tokens to revoke\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_sendrevoke", " \"51 \"100\"")
            + HelpExampleRpc("tl_createpayload_sendrevoke", "\", 51, \"100\"")
        );

    uint32_t propertyId = ParsePropertyId(request.params[0]);
    int64_t amount = ParseAmount(request.params[1], isPropertyDivisible(propertyId));

    std::vector<unsigned char> payload = CreatePayload_Revoke(propertyId, amount);

    return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_changeissuer(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "tl_createpayload_changeissuer \" propertyid\n"

            "\nPayload to change the issuer on record of the given tokens.\n"

            "\nArguments:\n"
            "1. propertyid           (number, required) the identifier of the tokens\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_changeissuer", "\"3")
            + HelpExampleRpc("tl_createpayload_changeissuer","\"3")
        );

    uint32_t propertyId = ParsePropertyId(request.params[0]);

    std::vector<unsigned char> payload = CreatePayload_ChangeIssuer(propertyId);

    return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_sendactivation(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 3)
        throw runtime_error(
            "tl_createpayload_sendactivation \"featureid\" \"block\" \"minclientversion\" \n"

            "\nPayload to activate a protocol feature.\n"

            "\nNote: Trade Layer ignores activations from unauthorized sources.\n"

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
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "tl_senddeactivation \"fromaddress\" \"featureid\" \n"

            "\nPayload to deactivate a protocol feature.  For Emergency Use Only.\n"

            "\nNote: Trade Layer Core ignores deactivations from unauthorized sources.\n"

            "\nArguments:\n"
            "1. featureid             (number, required) the identifier of the feature to activate\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_senddeactivation", "\"1")
            + HelpExampleRpc("tl_createpayload_senddeactivation", "\"1")
        );

    uint16_t featureId = request.params[0].get_int64();

    std::vector<unsigned char> payload = CreatePayload_DeactivateFeature(featureId);

    return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_sendalert(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 3)
        throw runtime_error(
            "tl_createpayload_sendalert \"alerttype \" \"expiryvalue\" \"typecheck\" \"versioncheck\" \"message\" \n"

            "\nPayload to creates and broadcasts an Trade Layer Core alert.\n"
            "\nNote: Trade Layer Core ignores alerts from unauthorized sources.\n"

            "\nArguments:\n"
            "1. alerttype            (number, required) the alert type\n"
            "2. expiryvalue          (number, required) the value when the alert expires (depends on alert type)\n"
            "3. message              (string, required) the user-faced alert message\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_sendalert", "\"3\" \"7\" \"message here\"")
            + HelpExampleRpc("tl_createpayload_sendalert", "\"3\", \"7\" ,\"message here\"")
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

    std::vector<unsigned char> payload = CreatePayload_TradeLayerAlert(alertType, expiryValue, alertMessage);

    return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_sendtrade(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() != 4) {
    throw runtime_error(
			"tl_createpayload_sendtrade \"fromaddress\" \"propertyidforsale\" \"amountforsale\" \"propertiddesired\" \"amountdesired\"\n"

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
  if (request.fHelp || request.params.size() != 9)

    throw runtime_error(
			"tl_createpayload_createcontract \"numerator\" \"denominator\" \"name\" \"blocksuntilexpiration\" \"notionalsize\" \"collateralcurrency\" \"marginrequirement\" \"quoting\" \"kyc\" \n"

			"Payload to create new Future Contract."

			"\nArguments:\n"
      "1. numerator                 (number, required) propertyId (Asset) \n"
      "2. denominator               (number, required) propertyId of denominator\n"
			"3. name                      (string, required) the name of the new tokens to create\n"
			"4. blocks until expiration   (number, required) life of contract, in blocks\n"
			"5. notional size             (number, required) notional size\n"
			"6. collateral currency       (number, required) collateral currency\n"
      "7. margin requirement        (number, required) margin requirement\n"
      "8. quoting                   (number, required) 0: inverse quoting contract, 1: normal quoting\n"
      "9. kyc options               (array, required) A json with the kyc allowed.\n"
      "    [\n"
      "      2,3,5         (number) kyc id\n"
      "      ,...\n"
      "    ]\n"

			"\nResult:\n"
			"\"payload\"             (string) the hex-encoded payload\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_createpayload_createcontract", "2 1 \"Contract1\" \"2560\" \"1\" \"3\" \"0.1\" 0 \"[1,2,4]\"")
			+ HelpExampleRpc("tl_createpayload_createcontract", "2, 1, \"Contract1\", \"2560\", \"1\", \"3\", \"0.1\", 0, \"[1,2,4]\"")
			);

  uint32_t num = ParsePropertyId(request.params[0]);
  uint32_t den = ParsePropertyId(request.params[1]);
  std::string name = ParseText(request.params[2]);
  uint32_t blocks_until_expiration = request.params[3].get_int();
  uint32_t notional_size = ParseAmount32t(request.params[4]);
  uint32_t collateral_currency = request.params[5].get_int();
  uint64_t margin_requirement = ParseAmount64t(request.params[6]);
  uint8_t inverse = ParseBinary(request.params[7]);
  std::vector<int> numbers = ParseArray(request.params[8]);

  std::vector<unsigned char> payload = CreatePayload_CreateContract(num, den, name, blocks_until_expiration, notional_size, collateral_currency, margin_requirement, inverse, numbers);


  return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_tradecontract(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() != 5)
    throw runtime_error(
			"tl_createpayload_tradecontract \"name\" \"amountforsale\" \"effectiveprice\" \"tradingaction\" \"leverage\" \n"

			"\nPayload to place a trade offer on the distributed Futures Contracts exchange.\n"

			"\nArguments:\n"
		  "1. name                 (number, required) the name of the contract to list for trade\n"
			"2. amountforsale        (number, required) the amount of contracts to trade\n"
			"3. effective price      (number, required) limit price desired in exchange\n"
			"4. trading action       (number, required) 1 to BUY contracts, 2 to SELL contracts \n"
			"5. leverage             (number, required) leverage (2x, 3x, ... 10x)\n"

			"\nResult:\n"
			"\"payload\"             (string) the hex-encoded payload\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_tradecontract", "\"Contract1\" \"250.0\" \"1000.0\" \"2\" \"10\"")
			+ HelpExampleRpc("tl_tradecontract", ",\"Contract1\", \"250.0\",\"1000.0\",\"2\",\"10\"")
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
  if (request.fHelp || request.params.size() != 1)
    throw runtime_error(
			"tl_cancelallcontractsbyaddress \" \"name (or id)\"\n"

			"\nPayload to cancel all offers on a given Futures Contract .\n"

			"\nArguments:\n"
			"1. name or id           (string, required) the Name (or id) of Future Contract \n"

			"\nResult:\n"
			"\"payload\"             (string) the hex-encoded payload\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_createpayload_cancelallcontractsbyaddress", "\"3")
			+ HelpExampleRpc("tl_createpayload_cancelallcontractsbyaddress", "\"3")
			);

  uint32_t contractId = ParseNameOrId(request.params[0]);

  std::vector<unsigned char> payload = CreatePayload_ContractDexCancelAll(contractId);

  return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_closeposition(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "tl_createpayload_closeposition \" \"contractId\" \n"

            "\nPayload to close the position on a given Futures Contract .\n"

            "\nArguments:\n"
            "1. contractId           (number, required) the Id of Future Contract \n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_closeposition", "\" 1")
            + HelpExampleRpc("tl_createpayload_closeposition", "\", 1")
        );

    uint32_t contractId = ParsePropertyId(request.params[0]);

    std::vector<unsigned char> payload = CreatePayload_ContractDexClosePosition(contractId);

    return HexStr(payload.begin(), payload.end());
}


UniValue tl_createpayload_sendissuance_pegged(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() != 6)
    throw runtime_error(
			"tl_createpayload_sendissuance_pegged\"  \"type\" \"previousid\" \"name\" \"collateralcurrency\" \"futurecontractname\" \"amount\" \n"

			"\nPayload to create new pegged currency with manageable supply.\n"

			"\nArguments:\n"
			"1. type                  (number, required) the type of the pegged to create: (1 for indivisible tokens, 2 for divisible tokens)\n"
			"2. previousid            (number, required) an identifier of a predecessor token (use 0 for new tokens)\n"
			"3. name                  (string, required) the name of the new pegged to create\n"
			"4. collateralcurrency    (number, required) the collateral currency for the new pegged \n"
			"5. future contract name  (number, required) the future contract name for the new pegged \n"
			"6. amount of pegged      (number, required) amount of pegged to create \n"

			"\nResult:\n"
			"\"payload\"              (string) the hex-encoded payload\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_createpayload_sendissuance_pegged", "\" 2 0 \"Pegged1\" \"4\" \"Quantum Future Contract\" \"500.2\"")
			+ HelpExampleRpc("tl_createpayload_sendissuance_pegged", "\", 2, 0, \"Pegged1\", \"4\", \"Quantum Miner Future Contract\", \"500.2\"")
			);

  uint16_t type = ParsePropertyType(request.params[0]);
  uint32_t previousId = ParsePreviousPropertyId(request.params[1]);
  std::string name = ParseText(request.params[2]);
  uint32_t propertyId = ParsePropertyId(request.params[3]);
  std::string name_traded = ParseText(request.params[4]);
  uint64_t amount = ParseAmount(request.params[5], isPropertyDivisible(propertyId));
  uint32_t contractId = getFutureContractObject(name_traded).fco_propertyId;

  std::vector<unsigned char> payload = CreatePayload_IssuancePegged(type, previousId, name, propertyId, contractId, amount);

  return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_send_pegged(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() != 2)
    throw runtime_error(
			"tl_send \"propertyname\" \"amount\" \n"

			"\nPayload to send the pegged currency to other addresses.\n"

			"\nArguments:\n"
			"1. property name        (string, required) the identifier of the tokens to send\n"
			"2. amount               (string, required) the amount to send\n"

			"\nResult:\n"
			"\"payload\"             (string) the hex-encoded payload\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_createpayload_send_pegged", "\" NCoin \"100.0\"")
			+ HelpExampleRpc("tl_createpayload_send_pegged", "\", NCoin, \"100.0\"")
			);

  std::string name_pegged = ParseText(request.params[0]);
  uint32_t propertyId = getFutureContractObject(name_pegged).fco_propertyId;
  int64_t amount = ParseAmount(request.params[1], true);

  std::vector<unsigned char> payload = CreatePayload_SendPeggedCurrency(propertyId, amount);

  return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_redemption_pegged(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 3)
       throw runtime_error(
			  "tl_createpayload_redemption_pegged \"nameofpegged\" \"amount\" \"nameofcontract\"\n"

			  "\n Payload to redeem pegged currency .\n"

			  "\nArguments:\n"
			  "1. name of pegged       (string, required) name of the tokens to redeem\n"
			  "2. amount               (number, required) the amount of pegged currency for redemption"
			  "3. name of contract     (string, required) the identifier of the future contract involved\n"

			  "\nResult:\n"
			  "\"payload\"             (string) the hex-encoded payload\n"

		  	"\nExamples:\n"
		  	+ HelpExampleCli("tl_createpayload_redemption_pegged", "\"Pegged1\" \"100.0\" \"Contract1\"")
		  	+ HelpExampleRpc("tl_createpayload_redemption_pegged", "\"Pegged1\", \"100.0\", \"Contract1\"")
			  );

    std::string name_pegged = ParseText(request.params[0]);
    std::string name_contract = ParseText(request.params[2]);
    uint32_t propertyId = getFutureContractObject(name_pegged).fco_propertyId;
    uint32_t contractId = getFutureContractObject(name_contract).fco_propertyId;

    uint64_t amount = ParseAmount(request.params[1], true);

    std::vector<unsigned char> payload = CreatePayload_RedemptionPegged(propertyId, contractId, amount);

    return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_cancelorderbyblock(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "tl_createpayload_cancelorderbyblock \"block\" \"idx\" \n"

            "\nPayload to cancel an specific offer on the distributed token exchange.\n"

            "\nArguments:\n"
            "1. block           (number, required) the block of order to cancel\n"
            "2. idx             (number, required) the idx in block of order to cancel\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_cancelorderbyblock", "\"1\" \"2\"")
            + HelpExampleRpc("tl_createpayload_cancelorderbyblock", "\"1\", \"2\"")
        );

    int block = static_cast<int>(ParseNewValues(request.params[0]));
    int idx = static_cast<int>(ParseNewValues(request.params[1]));

    std::vector<unsigned char> payload = CreatePayload_ContractDexCancelOrderByTxId(block,idx);

    return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_dexoffer(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() != 7) {
    throw runtime_error(
			"tl_createpayload_dexsell \"propertyidoffer\" \"amountoffered\" \"price\" \"paymentwindow\" \"minacceptfee\" \"option\" \"action\" \n"

			"\nPayload to place, update or cancel a sell offer on the traditional distributed Trade Layer/LTC exchange.\n"

			"\nArguments:\n"
			"1. propertyidoffered   (number, required) the identifier of the tokens to list for sale \n"
			"2. amountoffering      (string, required) the amount of tokens to list for sale\n"
			"3. price               (string, required) the price in litecoin of the offer \n"
			"4. paymentwindow       (number, required) a time limit in blocks a buyer has to pay following a successful accepting order\n"
			"5. minacceptfee        (string, required) a minimum mining fee a buyer has to pay to accept the offer\n"
			"6. option              (number, required) 1 for buy tokens, 2 to sell\n"
			"7. action              (number, required) the action to take (1 for new offers, 2 to update\", 3 to cancel)\n"

			"\nResult:\n"
			"\"payload\"             (string) the hex-encoded payload\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_createpayload_dexsell", "\"1\" \"1.5\" \"0.75\" 250 \"0.0005\" \"1\" \"2\"")
			+ HelpExampleRpc("tl_createpayload_dexsell", "\"1\", \"1.5\", \"0.75\", 250, \"0.0005\", 1 , \"2\"")
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
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "tl_createpayload_dexaccept \"propertyid\" \"amount\" \n"

            "\nPayload to create and broadcast an accept offer for the specified token and amount.\n"

            "\nArguments:\n"
            "1. propertyid           (number, required) the identifier of the token traded\n"
            "2. amount               (string, required) the amount traded\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_dexaccept", "\"1\" \"15.0\"")
            + HelpExampleRpc("tl_createpayload_dexaccept", "\"1\", \"15.0\"")
        );

    uint32_t propertyId = ParsePropertyId(request.params[0]);
    int64_t amount = ParseAmount(request.params[1], true); // MSC/TMSC is divisible

    std::vector<unsigned char> payload = CreatePayload_DExAccept(propertyId, amount);

    return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_sendvesting(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
			         "tl_createpayload_sendvesting \"amount\" \n"

			          "\nCreate and broadcast a send vesting transaction.\n"

			          "\nArguments:\n"
			          "1. amount               (string, required) the amount of vesting tokens to send\n"

			          "\nResult:\n"
			          "\"hash\"                  (string) the hex-encoded transaction hash\n"

			          "\nExamples:\n"
			          + HelpExampleCli("tl_createpayload_sendvesting", "\"100.0\"")
			          + HelpExampleRpc("tl_createpayload_sendvesting", "\"100.0\"")
			  );

    // obtain parameters & info
    int64_t amount = ParseAmount(request.params[0], true);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_SendVestingTokens(amount);

    return HexStr(payload.begin(), payload.end());

}


UniValue tl_createpayload_instant_trade(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() != 5)
    throw runtime_error(
			"tl_createpayload_instant_trade \"propertyid\" \"amount\" \"propertydesired\" \"amountdesired\" \"blockheightexpiry\" \n"

			"\nCreate a token for token instant trade payload.\n"

			"\nArguments:\n"
			"1. propertyId            (number, required) the identifier of the property\n"
			"2. amount                (string, required) the amount of the property traded for the first address of channel\n"
      "3. propertydesired       (number, optional) the identifier of the property traded for the second address of channel\n"
      "4. amountdesired         (string, optional) the amount desired of tokens\n"
      "5. blockheight expiry    (number, required) block of expiry\n"

			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_createpayload_instant_trade", "\"1\" \"100.0\" \"5\" \"500.2\" \"252\"")
			+ HelpExampleRpc("tl_createpayload_instant_trade", "\"1\" , \"100.0\", \"5\", \"500.2\", \"252\"")
			);

  // obtain parameters & info
  uint32_t propertyId = ParsePropertyId(request.params[0]);
  int64_t amount = ParseAmount(request.params[1], isPropertyDivisible(propertyId));
  uint32_t propertyDesired = ParsePropertyId(request.params[2]);
  int64_t amountDesired = ParseAmount(request.params[3], isPropertyDivisible(propertyDesired));
  int blockheight_expiry = request.params[4].get_int();

  // create a payload for the transaction
  std::vector<unsigned char> payload = CreatePayload_Instant_Trade(propertyId, amount, blockheight_expiry, propertyDesired, amountDesired);

  return HexStr(payload.begin(), payload.end());

}

UniValue tl_createpayload_instant_ltc_trade(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() != 4)
    throw runtime_error(
			"tl_createpayload_instant_ltc_trade \"propertyid\"  \"amountoffered\"  \"price\" \"blockheightexpiry\" \n"

			"\nCreate an ltc for tokens instant trade payload.\n"

			"\nArguments:\n"
			"1. propertyId            (number, required) the identifier of the property offered\n"
			"2. amount offered        (string, required) the amount of tokens offered for the address\n"
      "3. price                 (string, required) total price of tokens in LTC\n"
      "4. blockheight_expiry    (number, required) block of expiry\n"

			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_createpayload_instant_ltc_trade", "1 \"100.0\" \"2.0\" \"250\"")
			+ HelpExampleRpc("tl_createpayload_instant_ltc_trade", " 1, \"100.0\", \"2.0\", \"250\"")
			);

  // obtain parameters & info
  uint32_t propertyId = ParsePropertyId(request.params[0]);
  int64_t amount = ParseAmount(request.params[1], isPropertyDivisible(propertyId));
  int64_t totalPrice = ParseAmount(request.params[2],true);
  int blockheight_expiry = request.params[3].get_int();
  // create a payload for the transaction
  std::vector<unsigned char> payload = CreatePayload_Instant_LTC_Trade(propertyId, amount, totalPrice, blockheight_expiry);

  return HexStr(payload.begin(), payload.end());

}

UniValue tl_createpayload_contract_instant_trade(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() < 6)
    throw runtime_error(
			"tl_createpayload_instant_trade \"contractid\" \"amount\" \"blockheightexpiry\" \"effectiveprice\" \"trading\" \"leverage\" \n"

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
			+ HelpExampleCli("tl_createpayload_contract_instant_trade", "\"1\" \"100.0\" \"255\" \"3500.6\" \"1\" \"10\"")
			+ HelpExampleRpc("tl_createpayload_contract_instant_trade", "\"1\", \"100.0\", \"255\", \"3500.6\" ,\"1\", \"10\"")
			);

  // obtain parameters & info
  uint32_t contractId = ParsePropertyId(request.params[0]);
  int64_t amount = ParseAmountContract(request.params[1]);
  uint32_t blockheight_expiry = request.params[2].get_int();
  uint64_t price = ParseAmount(request.params[3],true);
  uint8_t trading_action = ParseContractDexAction(request.params[4]);
  uint64_t leverage = ParseLeverage(request.params[5]);

  // create a payload for the transaction
  std::vector<unsigned char> payload = CreatePayload_Contract_Instant_Trade(contractId, amount, blockheight_expiry, price, trading_action, leverage);

  return HexStr(payload.begin(), payload.end());

}

UniValue tl_createpayload_pnl_update(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() != 3)
    throw runtime_error(
			"tl_createpayload_pnl_update \"propertyid\" \"amount\" \"blockheightexpiry\" \n"

			"\nCreate an pnl update payload.\n"

			"\nArguments:\n"
			"1. propertyId            (number, required) the identifier of the property\n"
			"2. amount                (string, required) the amount of the property traded\n"
      "3. blockheight expiry    (number, required) block of expiry\n"

			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_createpayload_pnl_update", "\"1\" \"15.0\" \"564218\"")
			+ HelpExampleRpc("tl_createpayload_pnl_update", "\"1\", \"15.0\", \"512522\"")
			);

  // obtain parameters & info
  uint32_t propertyId = ParsePropertyId(request.params[0]);
  int64_t amount = ParseAmount(request.params[1], true);
  uint32_t blockheight_expiry = request.params[2].get_int();

  // create a payload for the transaction
  std::vector<unsigned char> payload = CreatePayload_PNL_Update(propertyId, amount, blockheight_expiry);

  return HexStr(payload.begin(), payload.end());

}

UniValue tl_createpayload_transfer(const JSONRPCRequest& request)
{
  if (request.fHelp)
    throw runtime_error(
			"tl_createpayload_transfer \"address option\"  \"propertyId\" \"amount\"\n"

			"\nCreate an transfer payload.\n"
      "\nArguments:\n"
			"1. address option      (number, required) 0 to use first address, 1 to use second address in channel\n"
      "2. propertyId          (number, required) the identifier of the property\n"
			"3. amount               (string, required) the amount of tokens to transfer\n"

			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_createpayload_transfer", "\"1\" \"3\" \"564\"")
			+ HelpExampleRpc("tl_createpayload_transfer", "\"0\", \"3\" ,\"564218\"")
			);

  uint8_t option = ParseBinary(request.params[0]);
  uint32_t propertyId = ParsePropertyId(request.params[1]);
  uint64_t amount = ParseAmount(request.params[2], isPropertyDivisible(propertyId));

  // create a payload for the transaction
  std::vector<unsigned char> payload = CreatePayload_Transfer(option, propertyId, amount);

  return HexStr(payload.begin(), payload.end());

}

UniValue tl_createpayload_dex_payment(const JSONRPCRequest& request)
{
  if (request.fHelp)
    throw runtime_error(
			"tl_createpayload_dex_payment\n"

			"\nCreate an transfer payload.\n"

			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
		  + HelpExampleCli("tl_createpayload_dex_payment", "\"")
			+ HelpExampleRpc("tl_createpayload_dex_payment", "\"")
			);

  // create a payload for the transaction
  std::vector<unsigned char> payload = CreatePayload_DEx_Payment();

  return HexStr(payload.begin(), payload.end());

}

UniValue tl_createpayload_change_oracleadm(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "tl_createpayload_change_oracleadm \"contractname (or id)\" \n"

            "\n Payload to change the admin on record of the Oracle Future Contract.\n"

            "\nArguments:\n"
            "1. name or id             (string, required) the name (or id) of the Future Contract\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_change_oracleadm", "\"Contract 1\"")
            + HelpExampleRpc("tl_createpayload_change_oracleadm", "\"Contract 1\"")
        );

    // obtain parameters & info
    uint32_t contractId = ParseNameOrId(request.params[0]);
    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_Change_OracleAdm(contractId);

    return HexStr(payload.begin(), payload.end());

}

UniValue tl_createpayload_create_oraclecontract(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() != 8)
    throw runtime_error(
			"tl_createpayload_create_oraclecontract \"name\" \"blocksuntilexpiration\" \"notionalsize\" \"collateralcurrency\" \"marginrequirement\" \"backupaddress\" \"quoting\" \"kyc\" \n"

			"Payload for create new Oracle Future Contract."

			"\nArguments:\n"
			"1. name                      (string, required) the name of the new tokens to create\n"
			"2. blocks until expiration   (number, required) life of contract, in blocks\n"
			"3. notional size             (number, required) notional size\n"
			"4. collateral currency       (number, required) collateral currency\n"
			"5. margin requirement        (number, required) margin requirement\n"
      "6. backup address            (string, required) backup admin address contract\n"
      "7. quoting                   (number, required) 0: inverse quoting contract, 1: normal quoting\n"
      "8. kyc options               (array, required) A json with the kyc allowed.\n"
      "    [\n"
      "      2,3,5                  (number) kyc id\n"
      "      ,...\n"
      "    ]\n"

			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_createpayload_create_oraclecontract", "\"Contract1\" \"200\" \"1\" \"3\" \"0.1\" \"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"0\" \"[1,5,4]\"")
			+ HelpExampleRpc("tl_createpayload_create_oraclecontract" ,"\"Contract1\", \"200\", \"1\", \"3\", \"0.1\", \"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"0\", \"[1,5,4]\"")
			);

      std::string name = ParseText(request.params[0]);
      uint32_t blocks_until_expiration = request.params[1].get_int();
      uint32_t notional_size = ParseAmount32t(request.params[2]);
      uint32_t collateral_currency = request.params[3].get_int();
      uint64_t margin_requirement = ParseAmount64t(request.params[4]);
      std::string oracleAddress = ParseAddress(request.params[5]);
      uint8_t inverse = ParseBinary(request.params[6]);
      std::vector<int> numbers = ParseArray(request.params[7]);

      std::vector<unsigned char> payload = CreatePayload_CreateOracleContract(name, blocks_until_expiration, notional_size, collateral_currency, margin_requirement, inverse, numbers);


      return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_setoracle(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 4)
        throw runtime_error(
            "tl_createpayload_setoracle  \"contract name\" \"high\" \"low \" \"close\" \n"

            "\nPayload to set the price for an oracle address.\n"

            "\nArguments:\n"
            "1. name or id           (string, required) the name (or id) of the Future Contract\n"
            "2. high price           (number, required) the highest price of the asset\n"
            "3. low price            (number, required) the lowest price of the asset\n"
            "4. close price          (number, required) the close price of the asset\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_setoracle", "\"Contract 1\"\"956.3\" \"452.1\" \"754.6\" ")
            + HelpExampleRpc("tl_createpayload_setoracle", "\"Contract 1\", \"956.3\", \"452.1 \", \"754.6\" ")
        );

    // obtain parameters & info
    uint32_t contractId = ParseNameOrId(request.params[0]);
    uint64_t high = ParseEffectivePrice(request.params[1]);
    uint64_t low = ParseEffectivePrice(request.params[2]);
    uint64_t close = ParseEffectivePrice(request.params[3]);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_Set_Oracle(contractId, high, low, close);

    return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_closeoracle(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "tl_createpayload_closeoracle \"contract name\n"

            "\nPayload to close an Oracle Future Contract.\n"

            "\nArguments:\n"
            "1. name or id            (string, required) the name (or id) of the Oracle Future Contract\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_closeoracle", "\"Contract 1\"")
            + HelpExampleRpc("tl_createpayload_closeoracle", "\"Contract 1\"")
        );

    // obtain parameters & info
    uint32_t contractId = ParseNameOrId(request.params[0]);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_Close_Oracle(contractId);

    return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_new_id_registration(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "tl_createpayload_new_id_registration  \"website url\" \"company name\" \n"

            "\nPayload for KYC: setting identity registrar Id number for address.\n"

            "\nArguments:\n"
            "1. website url                  (string, required) official web site of company\n"
            "2. company name                 (string, required) official name of company\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_new_id_registration", "\"www.companyone.com\" company one , 1,0,0,0 \"")
            + HelpExampleRpc("tl_createpayload_new_id_registration", "\"www.companyone.com\",company one, 1,1,0,0 \"")
        );

    // obtain parameters & info
    std::string website = ParseText(request.params[0]);
    std::string name = ParseText(request.params[1]);
    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_New_Id_Registration(website, name);

    return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_update_id_registration(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw runtime_error(
            "tl_createpayload_update_id_registration \"hash\" \n"

            "\nPayload to update the address on id registration.\n"

            "\nArguments:\n"
            "no arguments\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_update_id_registration", "\"\" , \"\"")
            + HelpExampleRpc("tl_createpayload_update_id_registration", "\"\",  \"\"")
        );

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_Update_Id_Registration();

    return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_attestation(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw runtime_error(
            "tl_createpayload_attestation \"hash\" \n"

            "\nPayload of attestation tx.\n"

            "\nArguments:\n"
            "1. string hash            (string, optional) the hash\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_attestation", "\"2507f85b9992c0d518f56c8e1a7cd43e1282c898\"")
            + HelpExampleRpc("tl_createpayload_attestation", "\"2507f85b9992c0d518f56c8e1a7cd43e1282c898\"")
        );


    std::string hash;
    (request.params.size() == 1) ? hash = ParseText(request.params[0]) : "";

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_Attestation(hash);

    return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_commit_tochannel(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "tl_createpayload_commit_tochannel \"propertyid\" \"amount\"\n"

            "\nPayload of commit to channel tx.\n"

            "\nArguments:\n"
            "1. propertyid             (number, required) the propertyid of token commited into the channel\n"
            "2. amount                 (number, required) amount of tokens traded in the channel\n"
            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_commit_tochannel", "\"2\" ,\"5\"")
            + HelpExampleRpc("tl_createpayload_commit_tochannel", "\"2\",\"5\"")
        );

    uint32_t propertyId = ParsePropertyId(request.params[0]);
    int64_t amount = ParseAmount(request.params[1], isPropertyDivisible(propertyId));

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_Commit_Channel(propertyId, amount);

    return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_withdrawal_fromchannel(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "tl_createpayload_withdrawal_fromchannel \"propertyId\" \"amount\" \n"

            "\nPayload of withdraw from channel tx.\n"

            "\nArguments:\n"
            "1. propertyId             (number, required) the propertyId of token commited into the channel\n"
            "2. amount                 (number, required) amount of tokens traded in the channel\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_withdrawal_fromchannel", "\"2\" ,\"5\"")
            + HelpExampleRpc("tl_createpayload_withdrawal_fromchannel", "\"2\",\"5\"")
        );

    uint32_t propertyId = ParsePropertyId(request.params[0]);
    int64_t amount = ParseAmount(request.params[1], isPropertyDivisible(propertyId));

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_Withdrawal_FromChannel(propertyId, amount);

    return HexStr(payload.begin(), payload.end());
}

UniValue tl_createpayload_closechannel(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw runtime_error(
            "tl_createpayload_closechannel \n"

            "\nClose channel tx.\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_closechannel", "")
            + HelpExampleRpc("tl_createpayload_closechannel", "")
        );

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_Close_Channel();

    return HexStr(payload.begin(), payload.end());
}

static const CRPCCommand commands[] =
  { //  category                         name                                             actor (function)                               okSafeMode
    //  -------------------------------- -----------------------------------------       ----------------------------------------        ----------
    { "trade layer (payload creation)", "tl_createpayload_simplesend",                    &tl_createpayload_simplesend,                      {}   },
    { "trade layer (payload creation)", "tl_createpayload_sendmany",                      &tl_createpayload_sendmany,                      {}   },
    { "trade layer (payload creation)", "tl_createpayload_sendall",                       &tl_createpayload_sendall,                         {}   },
    { "trade layer (payload creation)", "tl_createpayload_issuancefixed",                 &tl_createpayload_issuancefixed,                   {}   },
    { "trade layer (payload creation)", "tl_createpayload_issuancemanaged",               &tl_createpayload_issuancemanaged,                 {}   },
    { "trade layer (payload creation)", "tl_createpayload_sendgrant",                     &tl_createpayload_sendgrant,                       {}   },
    { "trade layer (payload creation)", "tl_createpayload_sendrevoke",                    &tl_createpayload_sendrevoke,                      {}   },
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
    { "trade layer (payload creation)", "tl_createpayload_dex_payment",                   &tl_createpayload_dex_payment,                     {}   },
    { "trade layer (payload creation)", "tl_createpayload_contract_instant_trade",        &tl_createpayload_contract_instant_trade,          {}   },
    { "trade layer (payload creation)", "tl_createpayload_change_oracleadm",              &tl_createpayload_change_oracleadm,                {}   },
    { "trade layer (payload creation)", "tl_createpayload_create_oraclecontract",         &tl_createpayload_create_oraclecontract,           {}   },
    { "trade layer (payload creation)", "tl_createpayload_setoracle",                     &tl_createpayload_setoracle,                       {}   },
    { "trade layer (payload creation)", "tl_createpayload_closeoracle",                   &tl_createpayload_closeoracle,                     {}   },
    { "trade layer (payload creation)", "tl_createpayload_new_id_registration",           &tl_createpayload_new_id_registration,             {}   },
    { "trade layer (payload creation)", "tl_createpayload_update_id_registration",        &tl_createpayload_update_id_registration,          {}   },
    { "trade layer (payload creation)", "tl_createpayload_attestation",                   &tl_createpayload_attestation,                     {}   },
    { "trade layer (payload creation)", "tl_createpayload_instant_ltc_trade",             &tl_createpayload_instant_ltc_trade,               {}   },
    { "trade layer (payload creation)", "tl_createpayload_commit_tochannel",              &tl_createpayload_commit_tochannel,                {}   },
    { "trade layer (payload creation)", "tl_createpayload_withdrawal_fromchannel",        &tl_createpayload_withdrawal_fromchannel,          {}   },
    { "trade layer (payload creation)", "tl_createpayload_closechannel",                  &tl_createpayload_closechannel,        {}   }
  };



void RegisterTLPayloadCreationRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
