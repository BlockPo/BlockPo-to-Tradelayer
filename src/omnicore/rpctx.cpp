/**
 * @file rpctx.cpp
 *
 * This file contains RPC calls for creating and sending Omni transactions.
 */

#include "omnicore/rpctx.h"
#include "omnicore/dex.h"
#include "omnicore/createpayload.h"
#include "omnicore/errors.h"
#include "omnicore/omnicore.h"
#include "omnicore/pending.h"
#include "omnicore/rpcrequirements.h"
#include "omnicore/rpcvalues.h"
#include "omnicore/sp.h"
#include "omnicore/tx.h"

#include "init.h"
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

UniValue tl_sendrawtx(const JSONRPCRequest& request)
{
    if (request.params.size() < 2 || request.params.size() > 5)
        throw runtime_error(
            "tl_sendrawtx \"fromaddress\" \"rawtransaction\" ( \"referenceaddress\" \"referenceamount\" )\n"
            "\nBroadcasts a raw trade layer transaction.\n"
            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. rawtransaction       (string, required) the hex-encoded raw transaction\n"
            "3. referenceaddress     (string, optional) a reference address (none by default)\n"
            "4. referenceamount      (string, optional) a bitcoin amount that is sent to the receiver (minimal by default)\n"
            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"
            "\nExamples:\n"
            + HelpExampleCli("tl_sendrawtx", "\"1MCHESTptvd2LnNp7wmr2sGTpRomteAkq8\" \"000000000000000100000000017d7840\" \"1EqTta1Rt8ixAA32DuC29oukbsSWU62qAV\"")
            + HelpExampleRpc("tl_sendrawtx", "\"1MCHESTptvd2LnNp7wmr2sGTpRomteAkq8\", \"000000000000000100000000017d7840\", \"1EqTta1Rt8ixAA32DuC29oukbsSWU62qAV\"")
        );

    std::string fromAddress = ParseAddress(request.params[0]);
    std::vector<unsigned char> data = ParseHexV(request.params[1], "raw transaction");
    std::string toAddress = (request.params.size() > 2) ? ParseAddressOrEmpty(request.params[2]): "";
    int64_t referenceAmount = (request.params.size() > 3) ? ParseAmount(request.params[3], true): 0;

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

UniValue tl_send(const JSONRPCRequest& request)
{
    if (request.params.size() < 4 || request.params.size() > 6)
        throw runtime_error(
            "tl_send \"fromaddress\" \"toaddress\" propertyid \"amount\" ( \"referenceamount\" )\n"

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
            + HelpExampleCli("tl_send", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" 1 \"100.0\"")
            + HelpExampleRpc("tl_send", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\", 1, \"100.0\"")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string toAddress = ParseAddress(request.params[1]);
    uint32_t propertyId = ParsePropertyId(request.params[2]);
    int64_t amount = ParseAmount(request.params[3], isPropertyDivisible(propertyId));
    int64_t referenceAmount = (request.params.size() > 4) ? ParseAmount(request.params[4], true): 0;

    // perform checks
    RequireExistingProperty(propertyId);
    RequireNotContract(propertyId);
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

UniValue tl_sendall(const JSONRPCRequest& request)
{
    if (request.params.size() < 3 || request.params.size() > 5)
        throw runtime_error(
            "tl_sendall \"fromaddress\" \"toaddress\" ecosystem ( \"referenceamount\" )\n"

            "\nTransfers all available tokens in the given ecosystem to the recipient.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. toaddress            (string, required) the address of the receiver\n"
            "3. ecosystem            (number, required) the ecosystem of the tokens to send (1 for main ecosystem, 2 for test ecosystem)\n"
            "4. referenceamount      (string, optional) a bitcoin amount that is sent to the receiver (minimal by default)\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_sendall", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" 2")
            + HelpExampleRpc("tl_sendall", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" 2")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string toAddress = ParseAddress(request.params[1]);
    uint8_t ecosystem = ParseEcosystem(request.params[2]);
    int64_t referenceAmount = (request.params.size() > 3) ? ParseAmount(request.params[3], true): 0;

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

UniValue tl_sendissuancecrowdsale(const JSONRPCRequest& request)
{
    if (request.params.size() != 12)
        throw runtime_error(
            "tl_sendissuancecrowdsale \"fromaddress\" ecosystem type previousid \"name\" \"url\" \"data\" propertyiddesired tokensperunit deadline ( earlybonus issuerpercentage )\n"

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
            + HelpExampleCli("tl_sendissuancecrowdsale", "\"3JYd75REX3HXn1vAU83YuGfmiPXW7BpYXo\" 2 1 0 \"Companies\" \"Bitcoin Mining\" \"Quantum Miner\" \"\" \"\" 2 \"100\" 1483228800 30 2")
            + HelpExampleRpc("tl_sendissuancecrowdsale", "\"3JYd75REX3HXn1vAU83YuGfmiPXW7BpYXo\", 2, 1, 0, \"Companies\", \"Bitcoin Mining\", \"Quantum Miner\", \"\", \"\", 2, \"100\", 1483228800, 30, 2")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    uint8_t ecosystem = ParseEcosystem(request.params[1]);
    uint16_t type = ParsePropertyType(request.params[2]);
    uint32_t previousId = ParsePreviousPropertyId(request.params[3]);
    std::string name = ParseText(request.params[4]);
    std::string url = ParseText(request.params[5]);
    std::string data = ParseText(request.params[6]);
    uint32_t propertyIdDesired = ParsePropertyId(request.params[7]);
    int64_t numTokens = ParseAmount(request.params[8], type);
    int64_t deadline = ParseDeadline(request.params[9]);
    uint8_t earlyBonus = ParseEarlyBirdBonus(request.params[10]);
    uint8_t issuerPercentage = ParseIssuerBonus(request.params[11]);

    // perform checks
    RequirePropertyName(name);
    RequireExistingProperty(propertyIdDesired);
    RequireNotContract(propertyIdDesired);
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

UniValue tl_sendissuancefixed(const JSONRPCRequest& request)
{
    if (request.params.size() != 8)
        throw runtime_error(
            "tl_sendissuancefixed \"fromaddress\" ecosystem type previousid \"name\" \"url\" \"data\" \"amount\"\n"

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
            + HelpExampleCli("tl_sendissuancefixed", "\"3Ck2kEGLJtZw9ENj2tameMCtS3HB7uRar3\" 2 1 0 \"Quantum Miner\" \"\" \"\" \"1000000\"")
            + HelpExampleRpc("tl_sendissuancefixed", "\"3Ck2kEGLJtZw9ENj2tameMCtS3HB7uRar3\", 2, 1, 0, \"Quantum Miner\", \"\", \"\", \"1000000\"")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    uint8_t ecosystem = ParseEcosystem(request.params[1]);
    uint16_t type = ParsePropertyType(request.params[2]);
    uint32_t previousId = ParsePreviousPropertyId(request.params[3]);
    RequireNotContract(previousId);
    std::string name = ParseText(request.params[4]);
    std::string url = ParseText(request.params[5]);
    std::string data = ParseText(request.params[6]);
    int64_t amount = ParseAmount(request.params[7], type);

    // perform checks
    RequirePropertyName(name);
    RequireSaneName(name);

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

UniValue tl_sendissuancemanaged(const JSONRPCRequest& request)
{
    if (request.params.size() != 7)
        throw runtime_error(
            "tl_sendissuancemanaged \"fromaddress\" ecosystem type previousid \"name\" \"url\" \"data\"\n"

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
            + HelpExampleCli("tl_sendissuancemanaged", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\" 2 1 0 \"Companies\" \"Bitcoin Mining\" \"Quantum Miner\" \"\" \"\"")
            + HelpExampleRpc("tl_sendissuancemanaged", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\", 2, 1, 0, \"Companies\", \"Bitcoin Mining\", \"Quantum Miner\", \"\", \"\"")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    uint8_t ecosystem = ParseEcosystem(request.params[1]);
    uint16_t type = ParsePropertyType(request.params[2]);
    uint32_t previousId = ParsePreviousPropertyId(request.params[3]);
    // RequireNotContract(previousId);  TODO: check this condition
    std::string name = ParseText(request.params[4]);
    std::string url = ParseText(request.params[5]);
    std::string data = ParseText(request.params[6]);

    // perform checks
    RequirePropertyName(name);
    RequireSaneName(name);

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

UniValue tl_sendgrant(const JSONRPCRequest& request)
{
  if (request.params.size() < 4 || request.params.size() > 5)
    throw runtime_error(
			"tl_sendgrant \"fromaddress\" \"toaddress\" propertyid \"amount\"\n"

			"\nIssue or grant new units of managed tokens.\n"

			"\nArguments:\n"
			"1. fromaddress          (string, required) the address to send from\n"
			"2. toaddress            (string, required) the receiver of the tokens (sender by default, can be \"\")\n"
			"3. propertyid           (number, required) the identifier of the tokens to grant\n"
			"4. amount               (string, required) the amount of tokens to create\n"

			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_sendgrant", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\" \"\" 51 \"7000\"")
			+ HelpExampleRpc("tl_sendgrant", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\", \"\", 51, \"7000\"")
			);

  // obtain parameters & info
  std::string fromAddress = ParseAddress(request.params[0]);
  std::string toAddress = !ParseText(request.params[1]).empty() ? ParseAddress(request.params[1]): "";
  uint32_t propertyId = ParsePropertyId(request.params[2]);
  int64_t amount = ParseAmount(request.params[3], isPropertyDivisible(propertyId));

  // perform checks
  RequireExistingProperty(propertyId);
  RequireManagedProperty(propertyId);
  RequireNotContract(propertyId);
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

UniValue tl_sendrevoke(const JSONRPCRequest& request)
{
    if (request.params.size() < 3 || request.params.size() > 4)
        throw runtime_error(
            "tl_sendrevoke \"fromaddress\" propertyid \"amount\" ( \"memo\" )\n"

            "\nRevoke units of managed tokens.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to revoke the tokens from\n"
            "2. propertyid           (number, required) the identifier of the tokens to revoke\n"
            "3. amount               (string, required) the amount of tokens to revoke\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_sendrevoke", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\" \"\" 51 \"100\"")
            + HelpExampleRpc("tl_sendrevoke", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\", \"\", 51, \"100\"")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    uint32_t propertyId = ParsePropertyId(request.params[1]);
    int64_t amount = ParseAmount(request.params[2], isPropertyDivisible(propertyId));

    // perform checks
    RequireExistingProperty(propertyId);
    RequireManagedProperty(propertyId);
    RequireNotContract(propertyId);
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

UniValue tl_sendclosecrowdsale(const JSONRPCRequest& request)
{
    if (request.params.size() != 2)
        throw runtime_error(
            "tl_sendclosecrowdsale \"fromaddress\" propertyid\n"

            "\nManually close a crowdsale.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address associated with the crowdsale to close\n"
            "2. propertyid           (number, required) the identifier of the crowdsale to close\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_sendclosecrowdsale", "\"3JYd75REX3HXn1vAU83YuGfmiPXW7BpYXo\" 70")
            + HelpExampleRpc("tl_sendclosecrowdsale", "\"3JYd75REX3HXn1vAU83YuGfmiPXW7BpYXo\", 70")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    uint32_t propertyId = ParsePropertyId(request.params[1]);

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

UniValue tl_sendchangeissuer(const JSONRPCRequest& request)
{
    if (request.params.size() != 3)
        throw runtime_error(
            "tl_sendchangeissuer \"fromaddress\" \"toaddress\" propertyid\n"

            "\nChange the issuer on record of the given tokens.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address associated with the tokens\n"
            "2. toaddress            (string, required) the address to transfer administrative control to\n"
            "3. propertyid           (number, required) the identifier of the tokens\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_sendchangeissuer", "\"1ARjWDkZ7kT9fwjPrjcQyvbXDkEySzKHwu\" \"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\" 3")
            + HelpExampleRpc("tl_sendchangeissuer", "\"1ARjWDkZ7kT9fwjPrjcQyvbXDkEySzKHwu\", \"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\", 3")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string toAddress = ParseAddress(request.params[1]);
    uint32_t propertyId = ParsePropertyId(request.params[2]);

    // perform checks
    RequireExistingProperty(propertyId);
    RequireManagedProperty(propertyId);
    RequireNotContract(propertyId);
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

UniValue tl_sendactivation(const JSONRPCRequest& request)
{
    if (request.params.size() != 4)
        throw runtime_error(
            "tl_sendactivation \"fromaddress\" featureid block minclientversion\n"
            "\nActivate a protocol feature.\n"
            "\nNote: Trade Layer Core ignores activations from unauthorized sources.\n"
            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. featureid            (number, required) the identifier of the feature to activate\n"
            "3. block                (number, required) the activation block\n"
            "4. minclientversion     (number, required) the minimum supported client version\n"
            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"
            "\nExamples:\n"
            + HelpExampleCli("tl_sendactivation", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\" 1 370000 999")
            + HelpExampleRpc("tl_sendactivation", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\", 1, 370000, 999")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    uint16_t featureId = request.params[1].get_int();
    uint32_t activationBlock = request.params[2].get_int();
    uint32_t minClientVersion = request.params[3].get_int();

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

UniValue tl_senddeactivation(const JSONRPCRequest& request)
{
    if (request.params.size() != 2)
        throw runtime_error(
            "tl_senddeactivation \"fromaddress\" featureid\n"
            "\nDeactivate a protocol feature.  For Emergency Use Only.\n"
            "\nNote: Trade Layer Core ignores deactivations from unauthorized sources.\n"
            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. featureid            (number, required) the identifier of the feature to activate\n"
            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"
            "\nExamples:\n"
            + HelpExampleCli("tl_senddeactivation", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\" 1")
            + HelpExampleRpc("tl_senddeactivation", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\", 1")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    uint16_t featureId = request.params[1].get_int64();

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

UniValue tl_sendalert(const JSONRPCRequest& request)
{
    if (request.params.size() != 4)
        throw runtime_error(
            "tl_sendalert \"fromaddress\" alerttype expiryvalue typecheck versioncheck \"message\"\n"
            "\nCreates and broadcasts an Trade Layer Core alert.\n"
            "\nNote: Trade Layer Core ignores alerts from unauthorized sources.\n"
            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. alerttype            (number, required) the alert type\n"
            "3. expiryvalue          (number, required) the value when the alert expires (depends on alert type)\n"
            "4. message              (string, required) the user-faced alert message\n"
            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"
            "\nExamples:\n"
            + HelpExampleCli("tl_sendalert", "")
            + HelpExampleRpc("tl_sendalert", "")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    int64_t tempAlertType = request.params[1].get_int64();
    if (tempAlertType < 1 || 65535 < tempAlertType) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Alert type is out of range");
    }
    uint16_t alertType = static_cast<uint16_t>(tempAlertType);
    int64_t tempExpiryValue = request.params[2].get_int64();
    if (tempExpiryValue < 1 || 4294967295LL < tempExpiryValue) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Expiry value is out of range");
    }
    uint32_t expiryValue = static_cast<uint32_t>(tempExpiryValue);
    std::string alertMessage = ParseText(request.params[3]);

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
UniValue tl_sendtrade(const JSONRPCRequest& request)
{
    if (request.params.size() != 5)
        throw runtime_error(
            "tl_sendtrade \"fromaddress\" propertyidforsale \"amountforsale\" propertiddesired \"amountdesired\"\n"

            "\nPlace a trade offer on the distributed token exchange.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to trade with\n"
            "2. propertyidforsale    (number, required) the identifier of the tokens to list for sale\n"
            "3. amountforsale        (string, required) the amount of tokens to list for sale\n"
            "4. propertiddesired     (number, required) the identifier of the tokens desired in exchange\n"
            "5. amountdesired        (string, required) the amount of tokens desired in exchange\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_sendtrade", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\" 31 \"250.0\" 1 \"10.0\"")
            + HelpExampleRpc("tl_sendtrade", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\", 31, \"250.0\", 1, \"10.0\"")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    uint32_t propertyIdForSale = ParsePropertyId(request.params[1]);
    int64_t amountForSale = ParseAmount(request.params[2], isPropertyDivisible(propertyIdForSale));
    uint32_t propertyIdDesired = ParsePropertyId(request.params[3]);
    int64_t amountDesired = ParseAmount(request.params[4], isPropertyDivisible(propertyIdDesired));

    // perform checks
    RequireExistingProperty(propertyIdForSale);
    RequireNotContract(propertyIdForSale);
    RequireExistingProperty(propertyIdDesired);
    RequireNotContract(propertyIdDesired);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_MetaDExTrade(propertyIdForSale, amountForSale, propertyIdDesired, amountDesired);

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
            PendingAdd(txid, fromAddress, MSC_TYPE_METADEX_TRADE, propertyIdForSale, amountForSale);
            return txid.GetHex();
        }
    }
}

UniValue tl_createcontract(const JSONRPCRequest& request)
{
  if (request.params.size() != 8)
    throw runtime_error(
			"tl_createcontract \"fromaddress\" ecosystem type previousid \"category\" \"subcategory\" \"name\" \"url\" \"data\" propertyiddesired tokensperunit deadline ( earlybonus issuerpercentage )\n"

			"Create new Future Contract."

			"\nArguments:\n"
			"1. fromaddress               (string, required) the address to send from\n"
			"2. ecosystem                 (string, required) the ecosystem to create the tokens in (1 for main ecosystem, 2 for test ecosystem)\n"
			"3. numerator                 (number, required) 4: ALL, 5: sLTC, 6: LTC.\n"
			"4. name                      (string, required) the name of the new tokens to create\n"
			"5. type                      (number, required) 1 for weekly, 2 for monthly contract\n"
			"6. notional size             (number, required) notional size\n"
			"7. collateral currency       (number, required) collateral currency\n"
      "8. margin requirement        (number, required) margin requirement\n"

			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_createcontract", "2 1 0 \"Companies\" \"Bitcoin Mining\" \"Quantum Miner\" \"\" \"\" 2 \"100\" 1483228800 30 2 4461 100 1 25")
			+ HelpExampleRpc("tl_createcontract", "2, 1, 0, \"Companies\", \"Bitcoin Mining\", \"Quantum Miner\", \"\", \"\", 2, \"100\", 1483228800, 30, 2, 4461, 100, 1, 25")
			);

  std::string fromAddress = ParseAddress(request.params[0]);
  uint8_t ecosystem = ParseEcosystem(request.params[1]);
  uint32_t type = ParseNewValues(request.params[2]);
  std::string name = ParseText(request.params[3]);
  uint32_t blocks_until_expiration = ParseNewValues(request.params[4]);
  uint32_t notional_size = ParseNewValues(request.params[5]);
  uint32_t collateral_currency = ParseNewValues(request.params[6]);
  uint32_t margin_requirement = ParseNewValues(request.params[7]);

  RequirePropertyName(name);
  RequireSaneName(name);

  std::vector<unsigned char> payload = CreatePayload_CreateContract(ecosystem, type, name, blocks_until_expiration, notional_size, collateral_currency, margin_requirement);

  uint256 txid;
  std::string rawHex;
  int result = WalletTxBuilder(fromAddress, "", 0, payload, txid, rawHex, autoCommit);

  if ( result != 0 )
    {
      throw JSONRPCError(result, error_str(result));
    }
  else
    {
      if (!autoCommit)
	{
	  return rawHex;
	}
      else
	{
	  return txid.GetHex();
	}
    }
}

UniValue tl_tradecontract(const JSONRPCRequest& request)
{
  if (request.params.size() != 5)
    throw runtime_error(
			"tl_tradecontract \"fromaddress\" propertyidforsale \"amountforsale\" propertiddesired \"amountdesired\"\n"

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
			+ HelpExampleCli("tl_tradecontract", "31\"250.0\"1\"10.0\"70.0\"80.0\"")
			+ HelpExampleRpc("tl_tradecontract", "31,\"250.0\",1,\"10.0,\"70.0,\"80.0\"")
			);

  std::string fromAddress = ParseAddress(request.params[0]);
  uint32_t propertyIdForSale = ParsePropertyId(request.params[1]);
  int64_t amountForSale = ParseAmountContract(request.params[2]);
  uint64_t effective_price = ParseEffectivePrice(request.params[3], propertyIdForSale);
  uint8_t trading_action = ParseContractDexAction(request.params[4]);

  RequireContract(propertyIdForSale);

  std::vector<unsigned char> payload = CreatePayload_ContractDexTrade(propertyIdForSale, amountForSale, effective_price, trading_action);

  uint256 txid;
  std::string rawHex;
  int result = WalletTxBuilder(fromAddress, "", 0, payload, txid, rawHex, autoCommit);
  PrintToConsole("Result of WalletTxBuilder: %d\n",result);

  if (result != 0)
    {
      throw JSONRPCError(result, error_str(result));
    }
  else
    {
      if (!autoCommit)
	{
	  return rawHex;
        }
      else
	{ //TODO: PendingAdd function
	  // PendingAdd(txid, fromAddress, MSC_TYPE_CONTRACTDEX_TRADE, propertyIdForSale, amountForSale);
	  return txid.GetHex();
        }
    }
}

UniValue tl_cancelallcontractsbyaddress(const JSONRPCRequest& request)
{
    if (request.params.size() != 3)
        throw runtime_error(
            "tl_cancelallcontractsbyaddress \"fromaddress\" ecosystem\n"

            "\nCancel all offers on a given Futures Contract .\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to trade with\n"
            "2. ecosystem            (number, required) the ecosystem of the offers to cancel (1 for main ecosystem, 2 for test ecosystem)\n"
            "3. contractId           (number, required) the Id of Future Contract \n"
            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_cancelallcontractsbyaddress", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\" 1, 3")
            + HelpExampleRpc("tl_cancelallcontractsbyaddress", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\", 1, 3")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    uint8_t ecosystem = ParseEcosystem(request.params[1]);
    uint32_t contractId = ParsePropertyId(request.params[2]);

    PrintToLog("-------------------------------------------------------------\n");
    PrintToLog("ecosystem: %d \n",ecosystem);
    PrintToLog("contractId: %d \n",contractId);
    PrintToLog("-------------------------------------------------------------\n");

    // perform checks
    RequireContract(contractId);
    // check, if there are matching offers to cancel
    RequireContractOrder(fromAddress, contractId);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_ContractDexCancelEcosystem(ecosystem, contractId);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", 0, payload, txid, rawHex, autoCommit);

    PrintToLog("WalletTxBuilder result: %d\n", result);
    PrintToLog("rawHex: %s\n", rawHex);
    PrintToLog("txid: %s\n", txid.GetHex());
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

UniValue tl_closeposition(const JSONRPCRequest& request)
{
    if (request.params.size() != 3)
        throw runtime_error(
            "tl_closeposition \"fromaddress\" ecosystem\n"

            "\nClose the position on a given Futures Contract .\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to trade with\n"
            "2. ecosystem            (number, required) the ecosystem of the offers to cancel (1 for main ecosystem, 2 for test ecosystem)\n"
            "3. contractId           (number, required) the Id of Future Contract \n"
            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_closeposition", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\" 1, 3")
            + HelpExampleRpc("tl_closeposition", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\", 1, 3")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    uint8_t ecosystem = ParseEcosystem(request.params[1]);
    uint32_t contractId = ParsePropertyId(request.params[2]);
    // perform checks
    RequireContract(contractId);
    // TODO: check, if there are matching offers to cancel

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_ContractDexClosePosition(ecosystem, contractId);

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

UniValue tl_sendissuance_pegged(const JSONRPCRequest& request)
{
    if (request.params.size() != 8)
        throw runtime_error(
            "tl_sendissuance_pegged\"fromaddress\" ecosystem type previousid \"category\" \"subcategory\" \"name\" \"url\" \"data\"\n"

            "\nCreate new pegged currency with manageable supply.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. ecosystem            (string, required) the ecosystem to create the pegged currency in (1 for main ecosystem, 2 for test ecosystem)\n"
            "3. type                 (number, required) the type of the pegged to create: (1 for indivisible tokens, 2 for divisible tokens)\n"
            "4. previousid           (number, required) an identifier of a predecessor token (use 0 for new tokens)\n"
            "5. name                 (string, required) the name of the new pegged to create\n"
            "6. collateralcurrency  (number, required) the collateral currency for the new pegged \n"
            "7. future contract id  (number, required) the future contract id for the new pegged \n"
            "8. amount of pegged    (number, required) amount of pegged to create \n"
            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_sendissuance_pegged", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\" 2 1 0 \"Companies\" \"Bitcoin Mining\" \"Quantum Miner\" \"\" \"\"")
            + HelpExampleRpc("tl_sendissuance_pegged", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\", 2, 1, 0, \"Companies\", \"Bitcoin Mining\", \"Quantum Miner\", \"\", \"\"")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    uint8_t ecosystem = ParseEcosystem(request.params[1]);
    uint16_t type = ParsePropertyType(request.params[2]);
    uint32_t previousId = ParsePreviousPropertyId(request.params[3]);
    std::string name = ParseText(request.params[4]);
    uint32_t propertyId = ParsePropertyId(request.params[5]);
    uint32_t contractId = ParseNewValues(request.params[6]);
    uint64_t amount = ParseAmount(request.params[7], isPropertyDivisible(propertyId));

    // perform checks
    RequirePeggedSaneName(name);

     // Checking existing
    RequireExistingProperty(propertyId);

    // Property must not be a future contract
    RequireNotContract(propertyId);

    // Checking for future contract
    RequireContract(contractId);

    // Checking for short position in given future contract
    RequireShort(fromAddress, contractId, amount);

    // checking for collateral balance, checking for short position in given contract
    RequireForPegged(fromAddress, propertyId, contractId, amount);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_IssuancePegged(ecosystem, type, previousId, name, propertyId, contractId, amount);

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

UniValue tl_send_pegged(const JSONRPCRequest& request)
{
    if (request.params.size() != 4)
        throw runtime_error(
            "tl_send \"fromaddress\" \"toaddress\" propertyid \"amount\" ( \"redeemaddress\" \"referenceamount\" )\n"

            "\nSend the pegged currency to other addresses.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. toaddress            (string, required) the address of the receiver\n"
            "3. propertyid           (number, required) the identifier of the tokens to send\n"
            "4. amount               (string, required) the amount to send\n"


            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_send_pegged", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" 1 \"100.0\"")
            + HelpExampleRpc("tl_send_pegged", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\", 1, \"100.0\"")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string toAddress = ParseAddress(request.params[1]);
    uint32_t propertyId = ParsePropertyId(request.params[2]);
    RequirePeggedCurrency(propertyId);
    int64_t amount = ParseAmount(request.params[3], isPropertyDivisible(propertyId));
  //  std::string redeemAddress = (request.params.size() > 4 && !ParseText(request.params[4]).empty()) ? ParseAddress(request.params[4]): "";
    int64_t referenceAmount = (request.params.size() > 5) ? ParseAmount(request.params[5], true): 0;


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
UniValue tl_redemption_pegged(const JSONRPCRequest& request)
{
    if (request.params.size() != 4)
        throw runtime_error(
            "tl_redemption_pegged \"fromaddress\" propertyid \"amount\" ( \"redeemaddress\" distributionproperty )\n"

            "\n Redemption of the pegged currency .\n"

            "\nArguments:\n"
            "1. redeemaddress         (string, required) the address of owner \n"
            "2. propertyid           (number, required) the identifier of the tokens to redeem\n"
            "3. amount               (number, required) the amount of pegged currency for redemption"
            "4. contractid           (number, required) the identifier of the future contract involved\n"
            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_redemption_pegged", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" , 1")
            + HelpExampleRpc("tl_redemption_pegged", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", 1")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    uint32_t propertyId = ParsePropertyId(request.params[1]);
    uint64_t amount = ParseAmount(request.params[2], isPropertyDivisible(propertyId));
    uint32_t contractId = ParseNewValues(request.params[3]);
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
UniValue tl_cancelorderbyblock(const JSONRPCRequest& request)
{
    if (request.params.size() != 3)
        throw runtime_error(
            "tl_cancelorderbyblock \"fromaddress\" ecosystem\n"

            "\nCancel all offers on the distributed token exchange.\n"

            "\nArguments:\n"
            "1. address         (string, required) the txid of order to cancel\n"
            "2. block           (number, required) the block of order to cancel\n"
            "2. idx             (number, required) the idx in block of order to cancel\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_cancelorderbyblock", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\" 1, 2")
            + HelpExampleRpc("tl_cancelorderbyblock", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\", 1, 2")
        );

    // obtain parameters & info
       std::string fromAddress = ParseAddress(request.params[0]);
       int block = static_cast<int>(ParseNewValues(request.params[1]));
       int idx = static_cast<int>(ParseNewValues(request.params[2]));
       printf ("block: %d\n",block);
       printf ("idx: %d\n",idx);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_ContractDexCancelOrderByTxId(block,idx);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", 0, payload, txid, rawHex, autoCommit);

    PrintToConsole("WalletTxBuilder result: %d\n", result);
    PrintToConsole("rawHex: %s\n", rawHex);
    PrintToConsole("txid: %s\n", txid.GetHex());
    //check error and return the txid (or raw hex depending on autocommit)
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

/* The DEX 1 rpcs */

UniValue tl_senddexoffer(const JSONRPCRequest& request)
{
    if (request.params.size() != 8)
        throw runtime_error(
            "tl_senddexsell \"fromaddress\" propertyidforsale \"amountforsale\" \"amountdesired\" paymentwindow minacceptfee action\n"

            "\nPlace, update or cancel a sell offer on the traditional distributed Trade Layer/LTC exchange.\n"

            "\nArguments:\n"

            "1. fromaddress         (string, required) the address to send from\n"
            "2. propertyidoffer     (number, required) the identifier of the tokens to list for sale (must be 1 for OMNI or 2 for TOMNI)\n"
            "3. amountoffering      (string, required) the amount of tokens to list for sale\n"
            "4. price               (string, required) the price in litecoin of the offer \n"
            "5. paymentwindow       (number, required) a time limit in blocks a buyer has to pay following a successful accepting order\n"
            "6. minacceptfee        (string, required) a minimum mining fee a buyer has to pay to accept the offer\n"
            "7. option              (number, required) 1 for buy tokens, 2 to sell\n"
            "8. action              (number, required) the action to take (1 for new offers, 2 to update\", 3 to cancel)\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_senddexsell", "\"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" 1 \"1.5\" \"0.75\" 25 \"0.0005\" 1")
            + HelpExampleRpc("tl_senddexsell", "\"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\", 1, \"1.5\", \"0.75\", 25, \"0.0005\", 1")
        );

        // obtain parameters & info

    std::string fromAddress = ParseAddress(request.params[0]);
    uint32_t propertyIdForSale = ParsePropertyId(request.params[1]);
    int64_t amountForSale = ParseAmount(request.params[2], true); // TMSC/MSC is divisible
    int64_t price = ParseAmount(request.params[3], true); // BTC is divisible
    uint8_t paymentWindow = ParseDExPaymentWindow(request.params[4]);
    int64_t minAcceptFee = ParseDExFee(request.params[5]);
    int64_t option = ParseAmount(request.params[6], false);  // buy : 1 ; sell : 2;
    uint8_t action = ParseDExAction(request.params[7]);
    if (action == 1 ) { RequireNoOtherDExOffer(fromAddress, propertyIdForSale); }
    std::vector<unsigned char> payload;
    if (option == 2) {
    // RequirePrimaryToken(propertyIdForSale);
    // if (action <= CMPTransaction::UPDATE) {
        RequireBalance(fromAddress, propertyIdForSale, amountForSale);
        payload = CreatePayload_DExSell(propertyIdForSale, amountForSale, price, paymentWindow, minAcceptFee, action);
    // }
    } else if (option == 1) {
       payload = CreatePayload_DEx(propertyIdForSale, amountForSale, price, paymentWindow, minAcceptFee, action);
    }
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
            // bool fSubtract = (action <= CMPTransaction::UPDATE); // no pending balances for cancels
            // PendingAdd(txid, fromAddress, MSC_TYPE_TRADE_OFFER, propertyIdForSale, amountForSale, fSubtract);
            return txid.GetHex();
        }
    }
}

UniValue tl_senddexaccept(const JSONRPCRequest& request)
{
    if (request.params.size() < 4 || request.params.size() > 5)
        throw runtime_error(
            "tl_senddexaccept \"fromaddress\" \"toaddress\" propertyid \"amount\" ( override )\n"

            "\nCreate and broadcast an accept offer for the specified token and amount.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. toaddress            (string, required) the address of the seller\n"
            "3. propertyid           (number, required) the identifier of the token traded\n"
            "4. amount               (string, required) the amount traded\n"
            "5. override             (boolean, optional) override minimum accept fee and payment window checks (use with caution!)\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_senddexaccept", "\"35URq1NN3xL6GeRKUP6vzaQVcxoJiiJKd8\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" 1 \"15.0\"")
            + HelpExampleRpc("tl_senddexaccept", "\"35URq1NN3xL6GeRKUP6vzaQVcxoJiiJKd8\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\", 1, \"15.0\"")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string toAddress = ParseAddress(request.params[1]);
    uint32_t propertyId = ParsePropertyId(request.params[2]);
    int64_t amount = ParseAmount(request.params[3], true); // MSC/TMSC is divisible
    bool override = (request.params.size() > 4) ? request.params[4].get_bool(): false;

    // perform checks
    // RequirePrimaryToken(propertyId);
    RequireMatchingDExOffer(toAddress, propertyId);

    if (!override) { // reject unsafe accepts - note client maximum tx fee will always be respected regardless of override here
        RequireSaneDExFee(toAddress, propertyId);
        RequireSaneDExPaymentWindow(toAddress, propertyId);
    }

#ifdef ENABLE_WALLET
    // use new 0.10 custom fee to set the accept minimum fee appropriately
    int64_t nMinimumAcceptFee = 0;
    {
        LOCK(cs_tally);
        const CMPOffer* sellOffer = DEx_getOffer(toAddress, propertyId);
        if (sellOffer == NULL) throw JSONRPCError(RPC_TYPE_ERROR, "Unable to load sell offer from the distributed exchange");
        nMinimumAcceptFee = sellOffer->getMinFee();
    }

    // LOCK2(cs_main, pwalletMain->cs_wallet);

    // temporarily update the global transaction fee to pay enough for the accept fee
    CFeeRate payTxFeeOriginal = payTxFee;
    payTxFee = CFeeRate(nMinimumAcceptFee, 225); // TODO: refine!
    // fPayAtLeastCustomFee = true;
#endif

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_DExAccept(propertyId, amount);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, toAddress,0, payload, txid, rawHex, autoCommit);

#ifdef ENABLE_WALLET
    // set the custom fee back to original
    payTxFee = payTxFeeOriginal;
#endif

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
    { "trade layer (transaction creation)", "tl_sendrawtx",                    &tl_sendrawtx,                       {} },
    { "trade layer (transaction creation)", "tl_send",                         &tl_send,                            {} },
    { "trade layer (transaction creation)", "tl_sendissuancecrowdsale",        &tl_sendissuancecrowdsale,           {} },
    { "trade layer (transaction creation)", "tl_sendissuancefixed",            &tl_sendissuancefixed,               {} },
    { "trade layer (transaction creation)", "tl_sendissuancemanaged",          &tl_sendissuancemanaged,             {} },
    { "trade layer (transaction creation)", "tl_sendgrant",                    &tl_sendgrant,                       {} },
    { "trade layer (transaction creation)", "tl_sendrevoke",                   &tl_sendrevoke,                      {} },
    { "trade layer (transaction creation)", "tl_sendclosecrowdsale",           &tl_sendclosecrowdsale,              {} },
    { "trade layer (transaction creation)", "tl_sendchangeissuer",             &tl_sendchangeissuer,                {} },
    { "trade layer (transaction creation)", "tl_sendall",                      &tl_sendall,                         {} },
    { "hidden",                             "tl_senddeactivation",             &tl_senddeactivation,                {} },
    { "hidden",                             "tl_sendactivation",               &tl_sendactivation,                  {} },
    { "hidden",                             "tl_sendalert",                    &tl_sendalert,                       {} },
    { "trade layer (transaction creation)", "tl_createcontract",               &tl_createcontract,                  {} },
    { "trade layer (transaction creation)", "tl_tradecontract",                &tl_tradecontract,                   {} },
    { "trade layer (transaction creation)", "tl_cancelallcontractsbyaddress",  &tl_cancelallcontractsbyaddress,     {} },
    { "trade layer (transaction creation)", "tl_cancelorderbyblock"         ,  &tl_cancelorderbyblock,              {} },
    { "trade layer (transaction creation)", "tl_sendissuance_pegged",          &tl_sendissuance_pegged,             {} },
    { "trade layer (transaction creation)", "tl_send_pegged",                  &tl_send_pegged,                     {} },
    { "trade layer (transaction creation)", "tl_redemption_pegged",            &tl_redemption_pegged,               {} },
    { "trade layer (transaction creation)", "tl_closeposition",                &tl_closeposition,                   {} },
    { "trade layer (transaction creation)", "tl_sendtrade",                    &tl_sendtrade,                       {} },
    { "trade layer (transaction creation)", "tl_senddexoffer",                 &tl_senddexoffer,                    {} },
    { "trade layer (transaction creation)", "tl_senddexaccept",                &tl_senddexaccept,                   {} },
    // { "trade layer (transaction creation)", "tl_senddexbuy",                &tl_senddexbuy,                      {} },
#endif
};

void RegisterOmniTransactionCreationRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
