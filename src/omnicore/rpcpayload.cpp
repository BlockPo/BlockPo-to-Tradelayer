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

UniValue omni_createpayload_simplesend(const UniValue& params, bool fHelp)
{
   if (fHelp || params.size() != 2)
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

    uint32_t propertyId = ParsePropertyId(params[0]);
//    RequireExistingProperty(propertyId);
//    int64_t amount = ParseAmount(params[1], isPropertyDivisible(propertyId));
    int64_t amount = ParseAmount(params[1], false);

    std::vector<unsigned char> payload = CreatePayload_SimpleSend(propertyId, amount);

    return HexStr(payload.begin(), payload.end());
}

UniValue omni_createpayload_sendall(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
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

    uint8_t ecosystem = ParseEcosystem(params[0]);

    std::vector<unsigned char> payload = CreatePayload_SendAll(ecosystem);

    return HexStr(payload.begin(), payload.end());
}

UniValue omni_createpayload_issuancefixed(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 9)
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

    uint8_t ecosystem = ParseEcosystem(params[0]);
    uint16_t type = ParsePropertyType(params[1]);
    uint32_t previousId = ParsePreviousPropertyId(params[2]);
    std::string name = ParseText(params[3]);
    std::string url = ParseText(params[4]);
    std::string data = ParseText(params[5]);
    int64_t amount = ParseAmount(params[6], type);

    RequirePropertyName(name);

    std::vector<unsigned char> payload = CreatePayload_IssuanceFixed(ecosystem, type, previousId, name, url, data, amount);

    return HexStr(payload.begin(), payload.end());
}

UniValue omni_createpayload_issuancecrowdsale(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 11)
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

    uint8_t ecosystem = ParseEcosystem(params[0]);
    uint16_t type = ParsePropertyType(params[1]);
    uint32_t previousId = ParsePreviousPropertyId(params[2]);
    std::string name = ParseText(params[3]);
    std::string url = ParseText(params[4]);
    std::string data = ParseText(params[5]);
    uint32_t propertyIdDesired = ParsePropertyId(params[6]);
    int64_t numTokens = ParseAmount(params[7], type);
    int64_t deadline = ParseDeadline(params[8]);
    uint8_t earlyBonus = ParseEarlyBirdBonus(params[9]);
    uint8_t issuerPercentage = ParseIssuerBonus(params[10]);

    RequirePropertyName(name);
    RequireExistingProperty(propertyIdDesired);
    RequireSameEcosystem(ecosystem, propertyIdDesired);

    std::vector<unsigned char> payload = CreatePayload_IssuanceVariable(ecosystem, type, previousId, name, url, data, propertyIdDesired, numTokens, deadline, earlyBonus, issuerPercentage);

    return HexStr(payload.begin(), payload.end());
}

UniValue omni_createpayload_issuancemanaged(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 6)
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

    uint8_t ecosystem = ParseEcosystem(params[0]);
    uint16_t type = ParsePropertyType(params[1]);
    uint32_t previousId = ParsePreviousPropertyId(params[2]);
    std::string name = ParseText(params[3]);
    std::string url = ParseText(params[4]);
    std::string data = ParseText(params[5]);

    RequirePropertyName(name);

    std::vector<unsigned char> payload = CreatePayload_IssuanceManaged(ecosystem, type, previousId, name, url, data);

    return HexStr(payload.begin(), payload.end());
}

UniValue omni_createpayload_closecrowdsale(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
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

    uint32_t propertyId = ParsePropertyId(params[0]);

    // checks bypassed because someone may wish to prepare the payload to close a crowdsale creation not yet broadcast

    std::vector<unsigned char> payload = CreatePayload_CloseCrowdsale(propertyId);

    return HexStr(payload.begin(), payload.end());
}

UniValue omni_createpayload_grant(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 3)
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

    uint32_t propertyId = ParsePropertyId(params[0]);
    RequireExistingProperty(propertyId);
    RequireManagedProperty(propertyId);
    int64_t amount = ParseAmount(params[1], isPropertyDivisible(propertyId));

    std::vector<unsigned char> payload = CreatePayload_Grant(propertyId, amount);

    return HexStr(payload.begin(), payload.end());
}

UniValue omni_createpayload_revoke(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 3)
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

    uint32_t propertyId = ParsePropertyId(params[0]);
    RequireExistingProperty(propertyId);
    RequireManagedProperty(propertyId);
    int64_t amount = ParseAmount(params[1], isPropertyDivisible(propertyId));
    std::string memo = (params.size() > 2) ? ParseText(params[2]): "";

    std::vector<unsigned char> payload = CreatePayload_Revoke(propertyId, amount);

    return HexStr(payload.begin(), payload.end());
}

UniValue omni_createpayload_changeissuer(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
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

    uint32_t propertyId = ParsePropertyId(params[0]);
    RequireExistingProperty(propertyId);

    std::vector<unsigned char> payload = CreatePayload_ChangeIssuer(propertyId);

    return HexStr(payload.begin(), payload.end());
}

///////////////////////////////////////////
/** New things for future contracts */
UniValue omni_createpayload_createcontract(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 17)
        throw runtime_error(
            "omni_createpayload_createcontract ecosystem type previousid \"category\" \"subcategory\" \"name\" \"url\" \"data\" propertyiddesired tokensperunit deadline earlybonus issuerpercentage\n"

            "\nCreates the payload for a new tokens issuance with crowdsale.\n"

            "\nArguments:\n"
            "1. ecosystem            (string, required) the ecosystem to create the tokens in (1 for main ecosystem, 2 for test ecosystem)\n"
            "2. type                 (number, required) the type of the tokens to create: (1 for indivisible tokens, 2 for divisible tokens)\n"
            "3. previousid           (number, required) an identifier of a predecessor token (0 for new crowdsales)\n"
            "4. category             (string, required) a category for the new tokens (can be \"\")\n"
            "5. subcategory          (string, required) a subcategory for the new tokens  (can be \"\")\n"
            "6. name                 (string, required) the name of the new tokens to create\n"
            "7. url                  (string, required) an URL for further information about the new tokens (can be \"\")\n"
            "8. data                 (string, required) a description for the new tokens (can be \"\")\n"
            "9. propertyiddesired    (number, required) the identifier of a token eligible to participate in the crowdsale\n"
            "10. tokensperunit       (string, required) the amount of tokens granted per unit invested in the crowdsale\n"
            "11. deadline            (number, required) the deadline of the crowdsale as Unix timestamp\n"
            "12. earlybonus          (number, required) an early bird bonus for participants in percent per week\n"
            "13. issuerpercentage    (number, required) a percentage of tokens that will be granted to the issuer\n"
            "14. blocks_until_expiration    (number, required) blocks until expiration\n"
            "15. notional_size    (number, required) notional size\n"
            "16. collateral_currency    (number, required) collateral currency\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("omni_createpayload_createcontract", "2 1 0 \"Companies\" \"Bitcoin Mining\" \"Quantum Miner\" \"\" \"\" 2 \"100\" 1483228800 30 2 4461 100 1 25")
            + HelpExampleRpc("omni_createpayload_createcontract", "2, 1, 0, \"Companies\", \"Bitcoin Mining\", \"Quantum Miner\", \"\", \"\", 2, \"100\", 1483228800, 30, 2, 4461, 100, 1, 25")
        );

    uint8_t ecosystem = ParseEcosystem(params[0]);
    uint16_t type = ParsePropertyType(params[1]);
    uint32_t previousId = ParsePreviousPropertyId(params[2]);
    std::string category = ParseText(params[3]);
    std::string subcategory = ParseText(params[4]);
    std::string name = ParseText(params[5]);
    std::string url = ParseText(params[6]);
    std::string data = ParseText(params[7]);
    uint32_t propertyIdDesired = ParsePropertyId(params[8]);
    int64_t numTokens = ParseAmountContract(params[9], type);
    int64_t deadline = ParseDeadline(params[10]);
    uint8_t earlyBonus = ParseEarlyBirdBonus(params[11]);
    uint8_t issuerPercentage = ParseIssuerBonus(params[12]);
    ////////////////////////////////////
    /** New things for Contracts */
    uint32_t blocks_until_expiration = ParseNewValues(params[13]);
    uint32_t notional_size = ParseNewValues(params[14]);
    uint32_t collateral_currency = ParseNewValues(params[15]);
    uint32_t margin_requirement = ParseNewValues(params[16]);
    ////////////////////////////////////
    
    RequirePropertyName(name);
    RequireExistingProperty(propertyIdDesired);
    RequireSameEcosystem(ecosystem, propertyIdDesired);

    std::vector<unsigned char> payload = CreatePayload_CreateContract(ecosystem, type, previousId, category, subcategory, name, url, data, propertyIdDesired, numTokens, deadline, earlyBonus, issuerPercentage, blocks_until_expiration, notional_size, collateral_currency, margin_requirement);

    return HexStr(payload.begin(), payload.end());
}

UniValue omni_createpayload_contract_trade(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 6)
        throw runtime_error(
            "omni_createpayload_contract_trade propertyidforsale \"amountforsale\" propertiddesired \"amountdesired\"\n"

            "\nCreates the payload to place a trade offer on the distributed token exchange.\n"

            "\nArguments:\n"
            "1. propertyidforsale    (number, required) the identifier of the tokens to list for sale\n"
            "2. amountforsale        (string, required) the amount of tokens to list for sale\n"
            "3. propertiddesired     (number, required) the identifier of the tokens desired in exchange\n"
            "4. amountdesired        (string, required) the amount of tokens desired in exchange\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("omni_createpayload_contract_trade", "31 \"250.0\" 1 \"10.0\"")
            + HelpExampleRpc("omni_createpayload_contract_trade", "31, \"250.0\", 1, \"10.0\"")
        );

    uint32_t propertyIdForSale = ParsePropertyId(params[0]);
    int64_t amountForSale = ParseAmountContract(params[1], isPropertyContract(propertyIdForSale));
    uint32_t propertyIdDesired = ParsePropertyId(params[2]);
    int64_t amountDesired = ParseAmountContract(params[3], isPropertyContract(propertyIdDesired));
    uint64_t effective_price = ParseEffectivePrice(params[4]);
    uint8_t trading_action = ParseContractDexAction(params[5]);

    std::vector<unsigned char> payload = CreatePayload_ContractDexTrade(propertyIdForSale, amountForSale, propertyIdDesired, amountDesired, effective_price, trading_action);
    return HexStr(payload.begin(), payload.end());
}

UniValue omni_createpayload_cancelcontracttradesbyprice(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 6)
        throw runtime_error(
            "omni_createpayload_cancelcontracttradesbyprice propertyidforsale \"amountforsale\" propertiddesired \"amountdesired\"\n"

            "\nCreates the payload to cancel offers on the distributed token exchange with the specified price.\n"

            "\nArguments:\n"
            "1. propertyidforsale    (number, required) the identifier of the tokens listed for sale\n"
            "2. amountforsale        (string, required) the amount of tokens to listed for sale\n"
            "3. propertiddesired     (number, required) the identifier of the tokens desired in exchange\n"
            "4. amountdesired        (string, required) the amount of tokens desired in exchange\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("omni_createpayload_cancelcontracttradesbyprice", "31 \"100.0\" 1 \"5.0\" 100 1")
            + HelpExampleRpc("omni_createpayload_cancelcontracttradesbyprice", "31 \"100.0\" 1 \"5.0\" 100 1")
        );

    uint32_t propertyIdForSale = ParsePropertyId(params[0]);
    int64_t amountForSale = ParseAmountContract(params[1], isPropertyContract(propertyIdForSale));
    uint32_t propertyIdDesired = ParsePropertyId(params[2]);
    int64_t amountDesired = ParseAmountContract(params[3], isPropertyContract(propertyIdDesired));
    uint64_t effective_price = ParseEffectivePrice(params[4]);
    uint8_t trading_action = ParseContractDexAction(params[5]);
    
    std::vector<unsigned char> payload = CreatePayload_ContractDexCancelPrice(propertyIdForSale, amountForSale, propertyIdDesired, amountDesired, effective_price, trading_action);

    return HexStr(payload.begin(), payload.end());
}


UniValue omni_createpayload_cancelalltradescontract(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "omni_createpayload_cancelalltrades ecosystem\n"

            "\nCreates the payload to cancel all offers on the distributed token exchange.\n"

            "\nArguments:\n"
            "1. ecosystem            (number, required) the ecosystem of the offers to cancel (1 for main ecosystem, 2 for test ecosystem)\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("omni_createpayload_cancelalltrades", "1")
            + HelpExampleRpc("omni_createpayload_cancelalltrades", "1")
        );

    uint8_t ecosystem = ParseEcosystem(params[0]);

    std::vector<unsigned char> payload = CreatePayload_ContractDexCancelEcosystem(ecosystem);

    return HexStr(payload.begin(), payload.end());
}

static const CRPCCommand commands[] =
{ //  category                         name                                      actor (function)                         okSafeMode
  //  -------------------------------- ----------------------------------------- ---------------------------------------- ----------
    { "omni layer (payload creation)", "omni_createpayload_simplesend",          &omni_createpayload_simplesend,          true },
    { "omni layer (payload creation)", "omni_createpayload_sendall",             &omni_createpayload_sendall,             true },
    { "omni layer (payload creation)", "omni_createpayload_grant",               &omni_createpayload_grant,               true },
    { "omni layer (payload creation)", "omni_createpayload_revoke",              &omni_createpayload_revoke,              true },
    { "omni layer (payload creation)", "omni_createpayload_changeissuer",        &omni_createpayload_changeissuer,        true },
    { "omni layer (payload creation)", "omni_createpayload_issuancefixed",       &omni_createpayload_issuancefixed,       true },
    { "omni layer (payload creation)", "omni_createpayload_issuancecrowdsale",   &omni_createpayload_issuancecrowdsale,   true },
    { "omni layer (payload creation)", "omni_createpayload_issuancemanaged",     &omni_createpayload_issuancemanaged,     true },
    { "omni layer (payload creation)", "omni_createpayload_closecrowdsale",      &omni_createpayload_closecrowdsale,      true },
    { "omni layer (payload creation)", "omni_createpayload_createcontract",      &omni_createpayload_createcontract,      true },
    { "omni layer (payload creation)", "omni_createpayload_contract_trade",      &omni_createpayload_contract_trade,      true },
    { "omni layer (payload creation)", "omni_createpayload_cancelcontracttradesbyprice",&omni_createpayload_cancelcontracttradesbyprice,      true },
    { "omni layer (payload creation)", "omni_createpayload_cancelalltradescontract",    &omni_createpayload_cancelalltradescontract,     true },
};

void RegisterOmniPayloadCreationRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
