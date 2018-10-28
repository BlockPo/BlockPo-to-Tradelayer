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

UniValue omni_createpayload_simplesend(const JSONRPCRequest& request)
{
   if (request.params.size() != 2)
        throw runtime_error(
            "omni_createpayload_simplesend propertyid \"amount\"\n"

            "\nCreate the payload for a simple send transaction.\n"

            "\nArguments:\n"
            "1. propertyid           (number, required) the identifier of the tokens to send\n"
            "2. amount               (string, required) the amount to send\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("omni_createpayload_simplesend", "1 \"100.0\"")
            + HelpExampleRpc("omni_createpayload_simplesend", "1, \"100.0\"")
        );

    uint32_t propertyId = ParsePropertyId(request.params[0]);
    int64_t amount = ParseAmount(request.params[1], false);

    std::vector<unsigned char> payload = CreatePayload_SimpleSend(propertyId, amount);

    return HexStr(payload.begin(), payload.end());
}

// UniValue omni_createpayload_createcontract(const JSONRPCRequest& request)
// {
//   if (request.params.size() != 8)
//     throw runtime_error(
// 			"omni_createpayload_createcontract ecosystem type previousid \"category\" \"subcategory\" \"name\" \"url\" \"data\" propertyiddesired tokensperunit deadline earlybonus issuerpercentage\n"
			
// 			"\nCreates the payload for a new tokens issuance with crowdsale.\n"
			
// 			"\nArguments:\n"
// 			"1. fromaddress               (string, required) the address to send from\n"
// 			"2. ecosystem                 the ecosystem to create the tokens in (1 for main ecosystem, 2 for test ecosystem)\n"
// 			"3. type                      (number, required) 4: ALL, 5: sLTC, 6: LTC.\n"
// 			"4. name                      (string, required) the name of the new tokens to create\n"
// 			"5. type                      (number, required) 1 for weekly, 2 for monthly contract\n"
// 			"6. notional size             (number, required) notional size\n"
// 			"7. collateral currency       (number, required) collateral currency\n"
//                         "8. margin requirement        (number, required) margin requirement\n"
			
// 			"\nResult:\n"
// 			"\"payload\"             (string) the hex-encoded payload\n"
			
// 			"\nExamples:\n"
// 			+ HelpExampleCli("omni_createpayload_createcontract", "2 1 0 \"Companies\" \"Bitcoin Mining\" \"Quantum Miner\" \"\" \"\" 2 \"100\" 1483228800 30 2 4461 100 1 25")
// 			+ HelpExampleRpc("omni_createpayload_createcontract", "2, 1, 0, \"Companies\", \"Bitcoin Mining\", \"Quantum Miner\", \"\", \"\", 2, \"100\", 1483228800, 30, 2, 4461, 100, 1, 25")
// 			);

//   std::string fromAddress = ParseAddress(request.params[0]);
//   uint8_t ecosystem = ParseEcosystem(request.params[1]);
//   uint32_t type = ParseNewValues(request.params[2]);
//   std::string name = ParseText(request.params[3]);
   
//   /** New things for Contracts */
//   uint32_t blocks_until_expiration = ParseNewValues(request.params[4]);
//   uint32_t notional_size = ParseNewValues(request.params[5]);
//   uint32_t collateral_currency = ParseNewValues(request.params[6]);
//   uint32_t margin_requirement = ParseNewValues(request.params[7]);
  
//   std::vector<unsigned char> payload = CreatePayload_CreateContract(ecosystem, type, name, blocks_until_expiration, notional_size, collateral_currency, margin_requirement);
  
//   return HexStr(payload.begin(), payload.end());
// }

/* UniValue omni_createpayload_sendall(const JSONRPCRequest& request)
{
    if (fHelp || request.params.size() != 1)
        throw runtime_error(
            "omni_createpayload_sendall ecosystem\n"

            "\nCreate the payload for a send all transaction.\n"

            "\nArguments:\n"
            "1. ecosystem              (number, required) the ecosystem of the tokens to send (1 for main ecosystem, 2 for test ecosystem)\n"

            "\nResult:\n"
            "\"payload\"               (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("omni_createpayload_sendall", "2")
            + HelpExampleRpc("omni_createpayload_sendall", "2")
        );

    uint8_t ecosystem = ParseEcosystem(request.params[0]);

    std::vector<unsigned char> payload = CreatePayload_SendAll(ecosystem);

    return HexStr(payload.begin(), payload.end());
}

UniValue omni_createpayload_issuancefixed(const JSONRPCRequest& request)
{
    if (fHelp || request.params.size() != 9)
        throw runtime_error(
            "omni_createpayload_issuancefixed ecosystem type previousid \"name\" \"url\" \"data\" \"amount\"\n"

            "\nCreates the payload for a new tokens issuance with fixed supply.\n"

            "\nArguments:\n"
            "1. ecosystem            (string, required) the ecosystem to create the tokens in (1 for main ecosystem, 2 for test ecosystem)\n"
            "2. type                 (number, required) the type of the tokens to create: (1 for indivisible tokens, 2 for divisible tokens)\n"
            "3. previousid           (number, required) an identifier of a predecessor token (use 0 for new tokens)\n"
            "4. name                 (string, required) the name of the new tokens to create\n"
            "5. url                  (string, required) an URL for further information about the new tokens (can be \"\")\n"
            "6. data                 (string, required) a description for the new tokens (can be \"\")\n"
            "7. amount               (string, required) the number of tokens to create\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("omni_createpayload_issuancefixed", "2 1 0 \"Quantum Miner\" \"\" \"\" \"1000000\"")
            + HelpExampleRpc("omni_createpayload_issuancefixed", "2, 1, 0, \"Quantum Miner\", \"\", \"\", \"1000000\"")
        );

    uint8_t ecosystem = ParseEcosystem(request.params[0]);
    uint16_t type = ParsePropertyType(request.params[1]);
    uint32_t previousId = ParsePreviousPropertyId(request.params[2]);
    std::string name = ParseText(request.params[3]);
    std::string url = ParseText(request.params[4]);
    std::string data = ParseText(request.params[5]);
    int64_t amount = ParseAmount(request.params[6], type);

    RequirePropertyName(name);

    std::vector<unsigned char> payload = CreatePayload_IssuanceFixed(ecosystem, type, previousId, name, url, data, amount);

    return HexStr(payload.begin(), payload.end());
}

UniValue omni_createpayload_issuancecrowdsale(const JSONRPCRequest& request)
{
    if (fHelp || request.params.size() != 11)
        throw runtime_error(
            "omni_createpayload_issuancecrowdsale ecosystem type previousid \"name\" \"url\" \"data\" propertyiddesired tokensperunit deadline earlybonus issuerpercentage\n"

            "\nCreates the payload for a new tokens issuance with crowdsale.\n"

            "\nArguments:\n"
            "1. ecosystem            (string, required) the ecosystem to create the tokens in (1 for main ecosystem, 2 for test ecosystem)\n"
            "2. type                 (number, required) the type of the tokens to create: (1 for indivisible tokens, 2 for divisible tokens)\n"
            "3. previousid           (number, required) an identifier of a predecessor token (0 for new crowdsales)\n"
            "4. name                 (string, required) the name of the new tokens to create\n"
            "5. url                  (string, required) an URL for further information about the new tokens (can be \"\")\n"
            "6. data                 (string, required) a description for the new tokens (can be \"\")\n"
            "7. propertyiddesired    (number, required) the identifier of a token eligible to participate in the crowdsale\n"
            "8. tokensperunit       (string, required) the amount of tokens granted per unit invested in the crowdsale\n"
            "9. deadline            (number, required) the deadline of the crowdsale as Unix timestamp\n"
            "10. earlybonus          (number, required) an early bird bonus for participants in percent per week\n"
            "11. issuerpercentage    (number, required) a percentage of tokens that will be granted to the issuer\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("omni_createpayload_issuancecrowdsale", "2 1 0 \"Companies\" \"Bitcoin Mining\" \"Quantum Miner\" \"\" \"\" 2 \"100\" 1483228800 30 2")
            + HelpExampleRpc("omni_createpayload_issuancecrowdsale", "2, 1, 0, \"Companies\", \"Bitcoin Mining\", \"Quantum Miner\", \"\", \"\", 2, \"100\", 1483228800, 30, 2")
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

    RequirePropertyName(name);
    RequireExistingProperty(propertyIdDesired);
    RequireSameEcosystem(ecosystem, propertyIdDesired);

    std::vector<unsigned char> payload = CreatePayload_IssuanceVariable(ecosystem, type, previousId, name, url, data, propertyIdDesired, numTokens, deadline, earlyBonus, issuerPercentage);

    return HexStr(payload.begin(), payload.end());
}

UniValue omni_createpayload_issuancemanaged(const JSONRPCRequest& request)
{
    if (fHelp || request.params.size() != 6)
        throw runtime_error(
            "omni_createpayload_issuancemanaged ecosystem type previousid \"name\" \"url\" \"data\"\n"

            "\nCreates the payload for a new tokens issuance with manageable supply.\n"

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
            + HelpExampleCli("omni_createpayload_issuancemanaged", "2 1 0 \"Companies\" \"Bitcoin Mining\" \"Quantum Miner\" \"\" \"\"")
            + HelpExampleRpc("omni_createpayload_issuancemanaged", "2, 1, 0, \"Companies\", \"Bitcoin Mining\", \"Quantum Miner\", \"\", \"\"")
        );

    uint8_t ecosystem = ParseEcosystem(request.params[0]);
    uint16_t type = ParsePropertyType(request.params[1]);
    uint32_t previousId = ParsePreviousPropertyId(request.params[2]);
    std::string name = ParseText(request.params[3]);
    std::string url = ParseText(request.params[4]);
    std::string data = ParseText(request.params[5]);

    RequirePropertyName(name);

    std::vector<unsigned char> payload = CreatePayload_IssuanceManaged(ecosystem, type, previousId, name, url, data);

    return HexStr(payload.begin(), payload.end());
}

UniValue omni_createpayload_closecrowdsale(const JSONRPCRequest& request)
{
    if (fHelp || request.params.size() != 1)
        throw runtime_error(
            "omni_createpayload_closecrowdsale propertyid\n"

            "\nCreates the payload to manually close a crowdsale.\n"

            "\nArguments:\n"
            "1. propertyid             (number, required) the identifier of the crowdsale to close\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("omni_createpayload_closecrowdsale", "70")
            + HelpExampleRpc("omni_createpayload_closecrowdsale", "70")
        );

    uint32_t propertyId = ParsePropertyId(request.params[0]);

    // checks bypassed because someone may wish to prepare the payload to close a crowdsale creation not yet broadcast

    std::vector<unsigned char> payload = CreatePayload_CloseCrowdsale(propertyId);

    return HexStr(payload.begin(), payload.end());
}

UniValue omni_createpayload_grant(const JSONRPCRequest& request)
{
    if (fHelp || request.params.size() < 2 || request.params.size() > 3)
        throw runtime_error(
            "omni_createpayload_grant propertyid \"amount\"\n"

            "\nCreates the payload to issue or grant new units of managed tokens.\n"

            "\nArguments:\n"
            "1. propertyid           (number, required) the identifier of the tokens to grant\n"
            "2. amount               (string, required) the amount of tokens to create\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("omni_createpayload_grant", "51 \"7000\"")
            + HelpExampleRpc("omni_createpayload_grant", "51, \"7000\"")
        );

    uint32_t propertyId = ParsePropertyId(request.params[0]);
    RequireExistingProperty(propertyId);
    RequireManagedProperty(propertyId);
    int64_t amount = ParseAmount(request.params[1], isPropertyDivisible(propertyId));

    std::vector<unsigned char> payload = CreatePayload_Grant(propertyId, amount);

    return HexStr(payload.begin(), payload.end());
}

UniValue omni_createpayload_revoke(const JSONRPCRequest& request)
{
    if (fHelp || request.params.size() < 2 || request.params.size() > 3)
        throw runtime_error(
            "omni_createpayload_revoke propertyid \"amount\"\n"

            "\nCreates the payload to revoke units of managed tokens.\n"

            "\nArguments:\n"
            "1. propertyid           (number, required) the identifier of the tokens to revoke\n"
            "2. amount               (string, required) the amount of tokens to revoke\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("omni_createpayload_revoke", "51 \"100\"")
            + HelpExampleRpc("omni_createpayload_revoke", "51, \"100\"")
        );

    uint32_t propertyId = ParsePropertyId(request.params[0]);
    RequireExistingProperty(propertyId);
    RequireManagedProperty(propertyId);
    int64_t amount = ParseAmount(request.params[1], isPropertyDivisible(propertyId));
    std::string memo = (request.params.size() > 2) ? ParseText(request.params[2]): "";

    std::vector<unsigned char> payload = CreatePayload_Revoke(propertyId, amount);

    return HexStr(payload.begin(), payload.end());
}

UniValue omni_createpayload_changeissuer(const JSONRPCRequest& request)
{
    if (fHelp || request.params.size() != 1)
        throw runtime_error(
            "omni_createpayload_changeissuer propertyid\n"

            "\nCreats the payload to change the issuer on record of the given tokens.\n"

            "\nArguments:\n"
            "1. propertyid           (number, required) the identifier of the tokens\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("omni_createpayload_changeissuer", "3")
            + HelpExampleRpc("omni_createpayload_changeissuer", "3")
        );

    uint32_t propertyId = ParsePropertyId(request.params[0]);
    RequireExistingProperty(propertyId);

    std::vector<unsigned char> payload = CreatePayload_ChangeIssuer(propertyId);

    return HexStr(payload.begin(), payload.end());
}

*/
static const CRPCCommand commands[] =
  { //  category                         name                                      actor (function)                         okSafeMode
    //  -------------------------------- ----------------------------------------- ---------------------------------------- ----------
    { "omni layer (payload creation)", "omni_createpayload_simplesend",      &omni_createpayload_simplesend,          {} },
    // { "omni layer (payload creation)", "omni_createpayload_createcontract",  &omni_createpayload_createcontract,      {} },
    /* { "omni layer (payload creation)", "omni_createpayload_sendall",             &omni_createpayload_sendall,             true },
       { "omni layer (payload creation)", "omni_createpayload_grant",               &omni_createpayload_grant,               true },
       { "omni layer (payload creation)", "omni_createpayload_revoke",              &omni_createpayload_revoke,              true },
       { "omni layer (payload creation)", "omni_createpayload_changeissuer",        &omni_createpayload_changeissuer,        true },
       { "omni layer (payload creation)", "omni_createpayload_issuancefixed",       &omni_createpayload_issuancefixed,       true },
       { "omni layer (payload creation)", "omni_createpayload_issuancecrowdsale",   &omni_createpayload_issuancecrowdsale,   true },
       { "omni layer (payload creation)", "omni_createpayload_issuancemanaged",     &omni_createpayload_issuancemanaged,     true },
       { "omni layer (payload creation)", "omni_createpayload_closecrowdsale",      &omni_createpayload_closecrowdsale,      true },*/
  };

void RegisterOmniPayloadCreationRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
