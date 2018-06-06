/**
 * @file rpctx.cpp
 *
 * This file contains RPC calls for creating and sending Omni transactions.
 */

#include "omnicore/rpctx.h"

#include "omnicore/createpayload.h"
#include "omnicore/errors.h"
#include "omnicore/omnicore.h"
#include "omnicore/pending.h"
#include "omnicore/rpcrequirements.h"
#include "omnicore/rpcvalues.h"
#include "omnicore/sp.h"
#include "omnicore/tx.h"

#include "init.h"
#include "main.h"
#include "rpc/server.h"
#include "sync.h"
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif

#include <univalue.h>

#include <stdint.h>
#include <stdexcept>
#include <string>

using std::runtime_error;
using namespace mastercore;

UniValue omni_sendrawtx(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 5)
        throw runtime_error(
            "omni_sendrawtx \"fromaddress\" \"rawtransaction\" ( \"referenceaddress\" \"referenceamount\" )\n"
            "\nBroadcasts a raw Omni Layer transaction.\n"
            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. rawtransaction       (string, required) the hex-encoded raw transaction\n"
            "3. referenceaddress     (string, optional) a reference address (none by default)\n"
            "4. referenceamount      (string, optional) a bitcoin amount that is sent to the receiver (minimal by default)\n"
            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"
            "\nExamples:\n"
            + HelpExampleCli("omni_sendrawtx", "\"1MCHESTptvd2LnNp7wmr2sGTpRomteAkq8\" \"000000000000000100000000017d7840\" \"1EqTta1Rt8ixAA32DuC29oukbsSWU62qAV\"")
            + HelpExampleRpc("omni_sendrawtx", "\"1MCHESTptvd2LnNp7wmr2sGTpRomteAkq8\", \"000000000000000100000000017d7840\", \"1EqTta1Rt8ixAA32DuC29oukbsSWU62qAV\"")
        );

    std::string fromAddress = ParseAddress(params[0]);
    std::vector<unsigned char> data = ParseHexV(params[1], "raw transaction");
    std::string toAddress = (params.size() > 2) ? ParseAddressOrEmpty(params[2]): "";
    int64_t referenceAmount = (params.size() > 3) ? ParseAmount(params[3], true): 0;

    //some sanity checking of the data supplied?
    uint256 newTX;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, toAddress, referenceAmount, data, newTX, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return newTX.GetHex();
        }
    }
}

UniValue omni_send(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 4 || params.size() > 6)
        throw runtime_error(
            "omni_send \"fromaddress\" \"toaddress\" propertyid \"amount\" ( \"referenceamount\" )\n"

            "\nCreate and broadcast a simple send transaction.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. toaddress            (string, required) the address of the receiver\n"
            "3. propertyid           (number, required) the identifier of the tokens to send\n"
            "4. amount               (string, required) the amount to send\n"
            "5. referenceamount      (string, optional) a bitcoin amount that is sent to the receiver (minimal by default)\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("omni_send", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" 1 \"100.0\"")
            + HelpExampleRpc("omni_send", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\", 1, \"100.0\"")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(params[0]);
    std::string toAddress = ParseAddress(params[1]);
    uint32_t propertyId = ParsePropertyId(params[2]);
    int64_t amount = ParseAmount(params[3], isPropertyDivisible(propertyId));
    int64_t referenceAmount = (params.size() > 4) ? ParseAmount(params[4], true): 0;

    // perform checks
    RequireExistingProperty(propertyId);
    RequireBalance(fromAddress, propertyId, amount);
    RequireSaneReferenceAmount(referenceAmount);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_SimpleSend(propertyId, amount);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, toAddress, referenceAmount, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            PendingAdd(txid, fromAddress, MSC_TYPE_SIMPLE_SEND, propertyId, amount);
            return txid.GetHex();
        }
    }
}

UniValue omni_sendall(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 3 || params.size() > 5)
        throw runtime_error(
            "omni_sendall \"fromaddress\" \"toaddress\" ecosystem ( \"referenceamount\" )\n"

            "\nTransfers all available tokens in the given ecosystem to the recipient.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. toaddress            (string, required) the address of the receiver\n"
            "3. ecosystem            (number, required) the ecosystem of the tokens to send (1 for main ecosystem, 2 for test ecosystem)\n"
            "4. referenceamount      (string, optional) a bitcoin amount that is sent to the receiver (minimal by default)\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("omni_sendall", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" 2")
            + HelpExampleRpc("omni_sendall", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" 2")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(params[0]);
    std::string toAddress = ParseAddress(params[1]);
    uint8_t ecosystem = ParseEcosystem(params[2]);
    int64_t referenceAmount = (params.size() > 3) ? ParseAmount(params[3], true): 0;

    // perform checks
    RequireSaneReferenceAmount(referenceAmount);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_SendAll(ecosystem);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, toAddress, referenceAmount, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            // TODO: pending
            return txid.GetHex();
        }
    }
}

UniValue omni_sendissuancecrowdsale(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 12)
        throw runtime_error(
            "omni_sendissuancecrowdsale \"fromaddress\" ecosystem type previousid \"name\" \"url\" \"data\" propertyiddesired tokensperunit deadline ( earlybonus issuerpercentage )\n"

            "Create new tokens as crowdsale."

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. ecosystem            (string, required) the ecosystem to create the tokens in (1 for main ecosystem, 2 for test ecosystem)\n"
            "3. type                 (number, required) the type of the tokens to create: (1 for indivisible tokens, 2 for divisible tokens)\n"
            "4. previousid           (number, required) an identifier of a predecessor token (0 for new crowdsales)\n"
            "5. name                 (string, required) the name of the new tokens to create\n"
            "6. url                  (string, required) an URL for further information about the new tokens (can be \"\")\n"
            "7. data                 (string, required) a description for the new tokens (can be \"\")\n"
            "8. propertyiddesired   (number, required) the identifier of a token eligible to participate in the crowdsale\n"
            "9. tokensperunit       (string, required) the amount of tokens granted per unit invested in the crowdsale\n"
            "10. deadline            (number, required) the deadline of the crowdsale as Unix timestamp\n"
            "11. earlybonus          (number, required) an early bird bonus for participants in percent per week\n"
            "12. issuerpercentage    (number, required) a percentage of tokens that will be granted to the issuer\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("omni_sendissuancecrowdsale", "\"3JYd75REX3HXn1vAU83YuGfmiPXW7BpYXo\" 2 1 0 \"Companies\" \"Bitcoin Mining\" \"Quantum Miner\" \"\" \"\" 2 \"100\" 1483228800 30 2")
            + HelpExampleRpc("omni_sendissuancecrowdsale", "\"3JYd75REX3HXn1vAU83YuGfmiPXW7BpYXo\", 2, 1, 0, \"Companies\", \"Bitcoin Mining\", \"Quantum Miner\", \"\", \"\", 2, \"100\", 1483228800, 30, 2")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(params[0]);
    uint8_t ecosystem = ParseEcosystem(params[1]);
    uint16_t type = ParsePropertyType(params[2]);
    uint32_t previousId = ParsePreviousPropertyId(params[3]);
    std::string name = ParseText(params[4]);
    std::string url = ParseText(params[5]);
    std::string data = ParseText(params[6]);
    uint32_t propertyIdDesired = ParsePropertyId(params[7]);
    int64_t numTokens = ParseAmount(params[8], type);
    int64_t deadline = ParseDeadline(params[9]);
    uint8_t earlyBonus = ParseEarlyBirdBonus(params[10]);
    uint8_t issuerPercentage = ParseIssuerBonus(params[11]);

    // perform checks
    RequirePropertyName(name);
    RequireExistingProperty(propertyIdDesired);
    RequireSameEcosystem(ecosystem, propertyIdDesired);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_IssuanceVariable(ecosystem, type, previousId, name, url, data, propertyIdDesired, numTokens, deadline, earlyBonus, issuerPercentage);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", 0, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

UniValue omni_sendissuancefixed(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 8)
        throw runtime_error(
            "omni_sendissuancefixed \"fromaddress\" ecosystem type previousid \"name\" \"url\" \"data\" \"amount\"\n"

            "\nCreate new tokens with fixed supply.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. ecosystem            (string, required) the ecosystem to create the tokens in (1 for main ecosystem, 2 for test ecosystem)\n"
            "3. type                 (number, required) the type of the tokens to create: (1 for indivisible tokens, 2 for divisible tokens)\n"
            "4. previousid           (number, required) an identifier of a predecessor token (use 0 for new tokens)\n"
            "5. name                 (string, required) the name of the new tokens to create\n"
            "6. url                  (string, required) an URL for further information about the new tokens (can be \"\")\n"
            "7. data                 (string, required) a description for the new tokens (can be \"\")\n"
            "8. amount              (string, required) the number of tokens to create\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("omni_sendissuancefixed", "\"3Ck2kEGLJtZw9ENj2tameMCtS3HB7uRar3\" 2 1 0 \"Quantum Miner\" \"\" \"\" \"1000000\"")
            + HelpExampleRpc("omni_sendissuancefixed", "\"3Ck2kEGLJtZw9ENj2tameMCtS3HB7uRar3\", 2, 1, 0, \"Quantum Miner\", \"\", \"\", \"1000000\"")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(params[0]);
    uint8_t ecosystem = ParseEcosystem(params[1]);
    uint16_t type = ParsePropertyType(params[2]);
    uint32_t previousId = ParsePreviousPropertyId(params[3]);
    std::string name = ParseText(params[4]);
    std::string url = ParseText(params[5]);
    std::string data = ParseText(params[6]);
    int64_t amount = ParseAmount(params[7], type);

    // perform checks
    RequirePropertyName(name);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_IssuanceFixed(ecosystem, type, previousId, name, url, data, amount);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", 0, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

UniValue omni_sendissuancemanaged(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 7)
        throw runtime_error(
            "omni_sendissuancemanaged \"fromaddress\" ecosystem type previousid \"name\" \"url\" \"data\"\n"

            "\nCreate new tokens with manageable supply.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. ecosystem            (string, required) the ecosystem to create the tokens in (1 for main ecosystem, 2 for test ecosystem)\n"
            "3. type                 (number, required) the type of the tokens to create: (1 for indivisible tokens, 2 for divisible tokens)\n"
            "4. previousid           (number, required) an identifier of a predecessor token (use 0 for new tokens)\n"
            "5. name                 (string, required) the name of the new tokens to create\n"
            "6. url                  (string, required) an URL for further information about the new tokens (can be \"\")\n"
            "7. data                 (string, required) a description for the new tokens (can be \"\")\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("omni_sendissuancemanaged", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\" 2 1 0 \"Companies\" \"Bitcoin Mining\" \"Quantum Miner\" \"\" \"\"")
            + HelpExampleRpc("omni_sendissuancemanaged", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\", 2, 1, 0, \"Companies\", \"Bitcoin Mining\", \"Quantum Miner\", \"\", \"\"")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(params[0]);
    uint8_t ecosystem = ParseEcosystem(params[1]);
    uint16_t type = ParsePropertyType(params[2]);
    uint32_t previousId = ParsePreviousPropertyId(params[3]);
    std::string name = ParseText(params[4]);
    std::string url = ParseText(params[5]);
    std::string data = ParseText(params[6]);

    // perform checks
    RequirePropertyName(name);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_IssuanceManaged(ecosystem, type, previousId, name, url, data);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", 0, payload, txid, rawHex, autoCommit);
    PrintToConsole("result of WalletTxBuilder: %d\n",result);
    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

UniValue omni_sendgrant(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 4 || params.size() > 5)
        throw runtime_error(
            "omni_sendgrant \"fromaddress\" \"toaddress\" propertyid \"amount\"\n"

            "\nIssue or grant new units of managed tokens.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. toaddress            (string, required) the receiver of the tokens (sender by default, can be \"\")\n"
            "3. propertyid           (number, required) the identifier of the tokens to grant\n"
            "4. amount               (string, required) the amount of tokens to create\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("omni_sendgrant", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\" \"\" 51 \"7000\"")
            + HelpExampleRpc("omni_sendgrant", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\", \"\", 51, \"7000\"")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(params[0]);
    std::string toAddress = !ParseText(params[1]).empty() ? ParseAddress(params[1]): "";
    uint32_t propertyId = ParsePropertyId(params[2]);
    int64_t amount = ParseAmount(params[3], isPropertyDivisible(propertyId));

    // perform checks
    RequireExistingProperty(propertyId);
    RequireManagedProperty(propertyId);
    RequireTokenIssuer(fromAddress, propertyId);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_Grant(propertyId, amount);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, toAddress, 0, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

UniValue omni_sendrevoke(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 3 || params.size() > 4)
        throw runtime_error(
            "omni_sendrevoke \"fromaddress\" propertyid \"amount\" ( \"memo\" )\n"

            "\nRevoke units of managed tokens.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to revoke the tokens from\n"
            "2. propertyid           (number, required) the identifier of the tokens to revoke\n"
            "3. amount               (string, required) the amount of tokens to revoke\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("omni_sendrevoke", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\" \"\" 51 \"100\"")
            + HelpExampleRpc("omni_sendrevoke", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\", \"\", 51, \"100\"")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(params[0]);
    uint32_t propertyId = ParsePropertyId(params[1]);
    int64_t amount = ParseAmount(params[2], isPropertyDivisible(propertyId));

    // perform checks
    RequireExistingProperty(propertyId);
    RequireManagedProperty(propertyId);
    RequireTokenIssuer(fromAddress, propertyId);
    RequireBalance(fromAddress, propertyId, amount);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_Revoke(propertyId, amount);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", 0, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

UniValue omni_sendclosecrowdsale(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error(
            "omni_sendclosecrowdsale \"fromaddress\" propertyid\n"

            "\nManually close a crowdsale.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address associated with the crowdsale to close\n"
            "2. propertyid           (number, required) the identifier of the crowdsale to close\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("omni_sendclosecrowdsale", "\"3JYd75REX3HXn1vAU83YuGfmiPXW7BpYXo\" 70")
            + HelpExampleRpc("omni_sendclosecrowdsale", "\"3JYd75REX3HXn1vAU83YuGfmiPXW7BpYXo\", 70")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(params[0]);
    uint32_t propertyId = ParsePropertyId(params[1]);

    // perform checks
    RequireExistingProperty(propertyId);
    RequireCrowdsale(propertyId);
    RequireActiveCrowdsale(propertyId);
    RequireTokenIssuer(fromAddress, propertyId);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_CloseCrowdsale(propertyId);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", 0, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

UniValue omni_sendchangeissuer(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 3)
        throw runtime_error(
            "omni_sendchangeissuer \"fromaddress\" \"toaddress\" propertyid\n"

            "\nChange the issuer on record of the given tokens.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address associated with the tokens\n"
            "2. toaddress            (string, required) the address to transfer administrative control to\n"
            "3. propertyid           (number, required) the identifier of the tokens\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("omni_sendchangeissuer", "\"1ARjWDkZ7kT9fwjPrjcQyvbXDkEySzKHwu\" \"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\" 3")
            + HelpExampleRpc("omni_sendchangeissuer", "\"1ARjWDkZ7kT9fwjPrjcQyvbXDkEySzKHwu\", \"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\", 3")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(params[0]);
    std::string toAddress = ParseAddress(params[1]);
    uint32_t propertyId = ParsePropertyId(params[2]);

    // perform checks
    RequireExistingProperty(propertyId);
    RequireManagedProperty(propertyId);
    RequireTokenIssuer(fromAddress, propertyId);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_ChangeIssuer(propertyId);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, toAddress, 0, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

UniValue omni_sendactivation(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 4)
        throw runtime_error(
            "omni_sendactivation \"fromaddress\" featureid block minclientversion\n"
            "\nActivate a protocol feature.\n"
            "\nNote: Omni Core ignores activations from unauthorized sources.\n"
            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. featureid            (number, required) the identifier of the feature to activate\n"
            "3. block                (number, required) the activation block\n"
            "4. minclientversion     (number, required) the minimum supported client version\n"
            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"
            "\nExamples:\n"
            + HelpExampleCli("omni_sendactivation", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\" 1 370000 999")
            + HelpExampleRpc("omni_sendactivation", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\", 1, 370000, 999")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(params[0]);
    uint16_t featureId = params[1].get_int();
    uint32_t activationBlock = params[2].get_int();
    uint32_t minClientVersion = params[3].get_int();

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_ActivateFeature(featureId, activationBlock, minClientVersion);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", 0, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

UniValue omni_senddeactivation(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error(
            "omni_senddeactivation \"fromaddress\" featureid\n"
            "\nDeactivate a protocol feature.  For Emergency Use Only.\n"
            "\nNote: Omni Core ignores deactivations from unauthorized sources.\n"
            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. featureid            (number, required) the identifier of the feature to activate\n"
            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"
            "\nExamples:\n"
            + HelpExampleCli("omni_senddeactivation", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\" 1")
            + HelpExampleRpc("omni_senddeactivation", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\", 1")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(params[0]);
    uint16_t featureId = params[1].get_int64();

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_DeactivateFeature(featureId);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", 0, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

UniValue omni_sendalert(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 4)
        throw runtime_error(
            "omni_sendalert \"fromaddress\" alerttype expiryvalue typecheck versioncheck \"message\"\n"
            "\nCreates and broadcasts an Omni Core alert.\n"
            "\nNote: Omni Core ignores alerts from unauthorized sources.\n"
            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. alerttype            (number, required) the alert type\n"
            "3. expiryvalue          (number, required) the value when the alert expires (depends on alert type)\n"
            "4. message              (string, required) the user-faced alert message\n"
            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"
            "\nExamples:\n"
            + HelpExampleCli("omni_sendalert", "")
            + HelpExampleRpc("omni_sendalert", "")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(params[0]);
    int64_t tempAlertType = params[1].get_int64();
    if (tempAlertType < 1 || 65535 < tempAlertType) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Alert type is out of range");
    }
    uint16_t alertType = static_cast<uint16_t>(tempAlertType);
    int64_t tempExpiryValue = params[2].get_int64();
    if (tempExpiryValue < 1 || 4294967295LL < tempExpiryValue) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Expiry value is out of range");
    }
    uint32_t expiryValue = static_cast<uint32_t>(tempExpiryValue);
    std::string alertMessage = ParseText(params[3]);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_OmniCoreAlert(alertType, expiryValue, alertMessage);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", 0, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}
//////////////////////////////////////
/** New things for Contract */
UniValue omni_createcontract(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 9)
        throw runtime_error(
            "omni_createcontract \"fromaddress\" ecosystem type previousid \"category\" \"subcategory\" \"name\" \"url\" \"data\" propertyiddesired tokensperunit deadline ( earlybonus issuerpercentage )\n"

            "Create new Future Contract."

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. ecosystem            (string, required) the ecosystem to create the tokens in (1 for main ecosystem, 2 for test ecosystem)\n"
            "3. type                 (number, required) the type of the tokens to create: (1 for indivisible tokens, 2 for divisible tokens)\n"
            "4. subcategory          (string, required) a subcategory for the new tokens  (can be \"\")\n"
            "5. name                 (string, required) the name of the new tokens to create\n"
            "6. issuerpercentage    (number, required) a percentage of tokens that will be granted to the issuer\n"
            "7. blocks_until_expiration    (number, required) blocks until expiration\n"
            "8. notional_size    (number, required) notional size\n"
            "9. collateral_currency    (number, required) collateral currency\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("omni_createcontract", "2 1 0 \"Companies\" \"Bitcoin Mining\" \"Quantum Miner\" \"\" \"\" 2 \"100\" 1483228800 30 2 4461 100 1 25")
            + HelpExampleRpc("omni_createcontract", "2, 1, 0, \"Companies\", \"Bitcoin Mining\", \"Quantum Miner\", \"\", \"\", 2, \"100\", 1483228800, 30, 2, 4461, 100, 1, 25")
        );

    std::string fromAddress = ParseAddress(params[0]);
    uint8_t ecosystem = ParseEcosystem(params[1]);
    uint16_t type = ParsePropertyType(params[2]);
    // uint32_t previousId = ParsePreviousPropertyId(params[3]);
    std::string subcategory = ParseText(params[3]);
    std::string name = ParseText(params[4]);
    // uint32_t propertyIdDesired = ParsePropertyId(params[6]);

    //////////////////////////////////////////
    uint32_t blocks_until_expiration = ParseNewValues(params[5]);
    uint32_t notional_size = ParseNewValues(params[6]);
    uint32_t collateral_currency = ParseNewValues(params[7]);
    uint32_t margin_requirement = ParseNewValues(params[8]);
    //////////////////////////////////////////

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_CreateContract(ecosystem, type, subcategory,name, blocks_until_expiration, notional_size, collateral_currency, margin_requirement);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", 0, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

UniValue omni_cancelallcontractsbyaddress(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 3)
        throw runtime_error(
            "omni_cancelallcontractsbyaddress \"fromaddress\" ecosystem\n"

            "\nCancel all offers on a given Futures Contract .\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to trade with\n"
            "2. ecosystem            (number, required) the ecosystem of the offers to cancel (1 for main ecosystem, 2 for test ecosystem)\n"
            "3. contractId           (number, required) the Id of Future Contract \n"
            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("omni_cancelallcontractsbyaddress", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\" 1, 3")
            + HelpExampleRpc("omni_cancelallcontractsbyaddress", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\", 1, 3")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(params[0]);
    uint8_t ecosystem = ParseEcosystem(params[1]);
    uint32_t contractId = ParseEcosystem(params[2]);
    // perform checks
    // TODO: check, if there are matching offers to cancel

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_ContractDexCancelEcosystem(ecosystem, contractId);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", 0, payload, txid, rawHex, autoCommit);

    PrintToConsole("WalletTxBuilder result: %d\n", result);
    PrintToConsole("rawHex: %s\n", rawHex);
    PrintToConsole("txid: %s\n", txid.GetHex());
    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            PendingAdd(txid, fromAddress, MSC_TYPE_CONTRACTDEX_CANCEL_ECOSYSTEM, ecosystem, 0, false);
            return txid.GetHex();
        }
    }
}


UniValue omni_tradecontract(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 5)
        throw runtime_error(
            "omni_tradecontract \"fromaddress\" propertyidforsale \"amountforsale\" propertiddesired \"amountdesired\"\n"

            "\nPlace a trade offer on the distributed Futures Contracts exchange.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to trade with\n"
            "2. propertyidforsale    (number, required) the identifier of the contract to list for trade\n"
            "3. amountforsale        (number, required) the amount of contracts to trade\n"
            "4. effective price     (number, required) limit price desired in exchange\n"
            "5. trading action        (number, required) 1 to BUY contracts, 2 to SELL contracts \n"
            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("omni_tradecontract", "31\"250.0\"1\"10.0\"70.0\"80.0\"")
            + HelpExampleRpc("omni_tradecontract", "31,\"250.0\",1,\"10.0,\"70.0,\"80.0\"")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(params[0]);
    uint32_t propertyIdForSale = ParsePropertyId(params[1]);
    int64_t amountForSale = ParseAmountContract(params[2], isPropertyContract(propertyIdForSale));
    uint64_t effective_price = ParseEffectivePrice(params[3]);
    uint8_t trading_action = ParseContractDexAction(params[4]);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_ContractDexTrade(propertyIdForSale, amountForSale, effective_price, trading_action);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", 0, payload, txid, rawHex, autoCommit);
    PrintToConsole("Result of WalletTxBuilder: %d\n",result);
    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            PendingAdd(txid, fromAddress, MSC_TYPE_CONTRACTDEX_TRADE, propertyIdForSale, amountForSale);
            return txid.GetHex();
        }
     }

}
UniValue omni_sendissuance_pegged(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 9)
        throw runtime_error(
            "omni_sendissuance_pegged\"fromaddress\" ecosystem type previousid \"category\" \"subcategory\" \"name\" \"url\" \"data\"\n"

            "\nCreate new pegged currency with manageable supply.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. ecosystem            (string, required) the ecosystem to create the pegged currency in (1 for main ecosystem, 2 for test ecosystem)\n"
            "3. type                 (number, required) the type of the pegged to create: (1 for indivisible tokens, 2 for divisible tokens)\n"
            "4. previousid           (number, required) an identifier of a predecessor token (use 0 for new tokens)\n"
            "5. subcategory          (string, required) a subcategory for the new pegged (can be \"\")\n"
            "6. name                 (string, required) the name of the new pegged to create\n"
            "7. collateralcurrency  (number, required) the collateral currency for the new pegged \n"
            "8. future contract id  (number, required) the future contract id for the new pegged \n"
            "9. amount of pegged    (number, required) amount of pegged to create \n"
            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("omni_sendissuance_pegged", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\" 2 1 0 \"Companies\" \"Bitcoin Mining\" \"Quantum Miner\" \"\" \"\"")
            + HelpExampleRpc("omni_sendissuance_pegged", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\", 2, 1, 0, \"Companies\", \"Bitcoin Mining\", \"Quantum Miner\", \"\", \"\"")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(params[0]);
    uint8_t ecosystem = ParseEcosystem(params[1]);
    uint16_t type = ParsePropertyType(params[2]);
    uint32_t previousId = ParsePreviousPropertyId(params[3]);
    // std::string category = ParseText(params[4]);
    std::string subcategory = ParseText(params[4]);
    std::string name = ParseText(params[5]);
    // std::string url = ParseText(params[7]);
    // std::string data = ParseText(params[8]);
    uint32_t propertyId = ParsePropertyId(params[6]);
    uint32_t contractId = ParseNewValues(params[7]);
    uint64_t amount = ParseAmount(params[8], isPropertyDivisible(propertyId));

    // perform checks
    RequirePropertyName(name);

     // Checking existing
    RequireExistingProperty(propertyId);

    // Property must not be a future contract
    RequireNotContract(propertyId);

    // Checking for future contract
    RequireContract(contractId);

    // checking for collateral balance, checking for short position in given contract
    RequireForPegged(fromAddress, propertyId, contractId, amount);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_IssuancePegged(ecosystem, type, previousId, subcategory, name, propertyId, contractId, amount);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", 0, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

UniValue omni_send_pegged(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 4)
        throw runtime_error(
            "omni_send \"fromaddress\" \"toaddress\" propertyid \"amount\" ( \"redeemaddress\" \"referenceamount\" )\n"

            "\nSend the pegged currency to other addresses.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. toaddress            (string, required) the address of the receiver\n"
            "3. propertyid           (number, required) the identifier of the tokens to send\n"
            "4. amount               (string, required) the amount to send\n"


            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("omni_send_pegged", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" 1 \"100.0\"")
            + HelpExampleRpc("omni_send_pegged", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\", 1, \"100.0\"")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(params[0]);
    std::string toAddress = ParseAddress(params[1]);
    uint32_t propertyId = ParsePropertyId(params[2]);
    RequirePeggedCurrency(propertyId);
    int64_t amount = ParseAmount(params[3], isPropertyDivisible(propertyId));
  //  std::string redeemAddress = (params.size() > 4 && !ParseText(params[4]).empty()) ? ParseAddress(params[4]): "";
    int64_t referenceAmount = (params.size() > 5) ? ParseAmount(params[5], true): 0;


    // perform checks
    RequireExistingProperty(propertyId);
    RequireBalance(fromAddress, propertyId, amount);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_SendPeggedCurrency(propertyId, amount);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, toAddress,referenceAmount, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            PendingAdd(txid, fromAddress, MSC_TYPE_SEND_PEGGED_CURRENCY, propertyId, amount);
            return txid.GetHex();
        }
    }
}
UniValue omni_redemption_pegged(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 4)
        throw runtime_error(
            "omni_redemption_pegged \"fromaddress\" propertyid \"amount\" ( \"redeemaddress\" distributionproperty )\n"

            "\n Redemption of the pegged currency .\n"

            "\nArguments:\n"
            "1. redeemaddress         (string, required) the address of owner \n"
            "2. propertyid           (number, required) the identifier of the tokens to redeem\n"
            "3. amount               (number, required) the amount of pegged currency for redemption"
            "4. contractid           (number, required) the identifier of the future contract involved\n"
            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("omni_redemption_pegged", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" , 1")
            + HelpExampleRpc("omni_redemption_pegged", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", 1")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(params[0]);
    uint32_t propertyId = ParsePropertyId(params[1]);
    uint64_t amount = ParseAmount(params[2], isPropertyDivisible(propertyId));
    uint32_t contractId = ParseNewValues(params[3]);
    // perform checks
    RequireExistingProperty(propertyId);
    RequirePeggedCurrency(propertyId);
    RequireContract(contractId);
    RequireBalance(fromAddress, propertyId, amount);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_RedemptionPegged(propertyId, contractId, amount);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", 0, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}


/////////////////////////////////////////////////////////////////////////////////
static const CRPCCommand commands[] =
{ //  category                             name                            actor (function)               okSafeMode
  //  ------------------------------------ ------------------------------- ------------------------------ ----------
#ifdef ENABLE_WALLET
    { "omni layer (transaction creation)", "omni_sendrawtx",               &omni_sendrawtx,               false },
    { "omni layer (transaction creation)", "omni_send",                    &omni_send,                    false },
    { "omni layer (transaction creation)", "omni_sendissuancecrowdsale",   &omni_sendissuancecrowdsale,   false },
    { "omni layer (transaction creation)", "omni_sendissuancefixed",       &omni_sendissuancefixed,       false },
    { "omni layer (transaction creation)", "omni_sendissuancemanaged",     &omni_sendissuancemanaged,     false },
    { "omni layer (transaction creation)", "omni_sendgrant",               &omni_sendgrant,               false },
    { "omni layer (transaction creation)", "omni_sendrevoke",              &omni_sendrevoke,              false },
    { "omni layer (transaction creation)", "omni_sendclosecrowdsale",      &omni_sendclosecrowdsale,      false },
    { "omni layer (transaction creation)", "omni_sendchangeissuer",        &omni_sendchangeissuer,        false },
    { "omni layer (transaction creation)", "omni_sendall",                 &omni_sendall,                 false },
    { "hidden",                            "omni_senddeactivation",        &omni_senddeactivation,        true  },
    { "hidden",                            "omni_sendactivation",          &omni_sendactivation,          false },
    { "hidden",                            "omni_sendalert",               &omni_sendalert,               true  },
    { "omni layer (transaction creation)", "omni_createcontract",          &omni_createcontract,          false },
    { "omni layer (transaction creation)", "omni_tradecontract",           &omni_tradecontract,           false },
    { "omni layer (transaction creation)", "omni_cancelallcontractsbyaddress",  &omni_cancelallcontractsbyaddress, false },
    { "omni layer (transaction creation)", "omni_sendissuance_pegged",       &omni_sendissuance_pegged,       false },
    { "omni layer (transaction creation)", "omni_send_pegged",             &omni_send_pegged,             false },
    { "omni layer (transaction creation)","omni_redemption_pegged",        &omni_redemption_pegged,       false },
#endif
};

void RegisterOmniTransactionCreationRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
