/**
 * @file rpctx.cpp
 *
 * This file contains RPC calls for creating and sending Trade Layer transactions.
 */

#include <tradelayer/rpctx.h>
#include <tradelayer/ce.h>
#include <tradelayer/createpayload.h>
#include <tradelayer/dex.h>
#include <tradelayer/errors.h>
#include <tradelayer/pending.h>
#include <tradelayer/rpcrequirements.h>
#include <tradelayer/rpcvalues.h>
#include <tradelayer/rules.h>
#include <tradelayer/sp.h>
#include <tradelayer/tradelayer.h>
#include <tradelayer/tx.h>

#include <init.h>
#include <rpc/server.h>
#include <sync.h>
#ifdef ENABLE_WALLET
#include <wallet/wallet.h>
#endif

#include <univalue.h>

#include <stdint.h>
#include <stdexcept>
#include <string>

using std::runtime_error;
using namespace mastercore;

UniValue tl_sendrawtx(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 2 || request.params.size() > 5)
        throw runtime_error(
            "tl_sendrawtx \"fromaddress\" \"rawtransaction\" ( \"referenceaddress\" \"referenceamount\" )\n"

            "\nBroadcasts a raw trade layer transaction.\n"

            "\nArguments:\n"

            "1. fromaddress       (string, required) the sender address\n"
            "2. rawtransaction       (string, required) the hex-encoded raw transaction\n"
            "3. referenceaddress     (string, optional) a reference address (none by default)\n"
            "4. referenceamount      (string, optional) a bitcoin amount that is sent to the receiver (minimal by default)\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_sendrawtx", "\"1MCHESTptvd2LnNp7wmr2sGTpRomteAkq8\" \"000000000000000100000000017d7840\" \"1EqTta1Rt8ixAA32DuC29oukbsSWU62qAV\"")
            + HelpExampleRpc("tl_sendrawtx", "\"1MCHESTptvd2LnNp7wmr2sGTpRomteAkq8\", \"000000000000000100000000017d7840\", \"1EqTta1Rt8ixAA32DuC29oukbsSWU62qAV\"")
        );

    const std::string fromAddress = ParseAddress(request.params[0]);
    std::vector<unsigned char> data = ParseHexV(request.params[1], "raw transaction");
    const std::string toAddress = (request.params.size() > 2) ? ParseAddressOrEmpty(request.params[2]): "";
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

UniValue tl_submit_nodeaddress(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "tl_submit_nodeaddress \"address\" \n"

            "\nSubmiting node reward address.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) sender address \n"
            "2. receiver             (string, required) the node address participating\n"
            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_submit_nodeaddress", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"LEr4hNAefWYhBMgxCFP2Po1NPrUeiK8kM2\"")
            + HelpExampleRpc("tl_submit_nodeaddress", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"LEr4hNAefWYhBMgxCFP2Po1NPrUeiK8kM2\"")
        );

    // obtain parameters & info
    const std::string sender = ParseAddress(request.params[0]);
    const std::string nodeRewardsAddr = ParseAddress(request.params[1]);

    RequireFeatureActivated(FEATURE_NODE_REWARD);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_SubmitNodeAddress();

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(sender, nodeRewardsAddr, 0, payload, txid, rawHex, autoCommit);

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


UniValue tl_claim_nodereward(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "tl_claim_nodereward \"address\" \n"

            "\nClaim node reward.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the node address participating\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_claim_nodereward", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\"")
            + HelpExampleRpc("tl_claim_nodereward", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\"")
        );

    // obtain parameters & info
    const std::string nodeRewardsAddr = ParseAddress(request.params[0]);

    RequireFeatureActivated(FEATURE_NODE_REWARD);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_ClaimNodeReward();

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(nodeRewardsAddr, "", 0, payload, txid, rawHex, autoCommit);

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

UniValue tl_send(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 4 || request.params.size() > 6)
        throw runtime_error(
            "tl_send \"fromaddress\" \"toaddress\" \"propertyid\" \"amount\" ( \"referenceamount\" )\n"

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
    const std::string fromAddress = ParseAddress(request.params[0]);
    const std::string toAddress = ParseAddress(request.params[1]);
    uint32_t propertyId = ParsePropertyId(request.params[2]);
    int64_t amount = ParseAmount(request.params[3], isPropertyDivisible(propertyId));
    int64_t referenceAmount = (request.params.size() > 4) ? ParseAmount(request.params[4], true): 0;

    // perform checks
    if (fromAddress == toAddress)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "sending tokens to same address");

    RequireExistingProperty(propertyId);
    // RequireNotContract(propertyId);
    RequireBalance(fromAddress, propertyId, amount);
    RequireSaneReferenceAmount(referenceAmount);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_SimpleSend(propertyId, amount);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, toAddress, referenceAmount, payload, txid, rawHex, autoCommit);

    PrintToLog("%s(): CHECKING ERROR: %s\n",__func__, error_str(result));

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

UniValue tl_sendmany(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 3 || request.params[1].size() < 1 || request.params[1].size() > 4)
        throw runtime_error(
            "tl_sendmany \"fromaddress\" \"{\"toaddress\":\"amount\", ...}\" \"propertyid\" ( \"referenceamount\" )\n"

            "\nCreate and broadcast a send to many transaction.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. \"amounts\"          (string, required) A json object with addresses and amounts (upto 4 pairs)\n"
            "    {\n"
            "      \"address\":\"amount\" (string) The litecoin address is the key, the numeric amount (can be string) in " + CURRENCY_UNIT + " is the value\n"
            "      ,...\n"
            "    }\n"
            "3. propertyid           (number, required) the identifier of the tokens to send\n"
            "4. referenceamount      (string, optional) a bitcoin amount that is sent to the receiver (minimal by default)\n"
            "\nResult:\n"
            "\"hash\"                (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_sendmany", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"{\"LEr4hNAefWYhBMgxCFP2Po1NPrUeiK8kM2\":\"10\",\"LbhhnrHHVFP1eUjP1tdNIYeEVsNHfN9FCw\":\"20\"}\" 1")
            + HelpExampleCli("tl_sendmany", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"{\"LEr4hNAefWYhBMgxCFP2Po1NPrUeiK8kM2\":\"30\",\"LbhhnrHHVFP1eUjP1tdNIYeEVsNHfN9FCw\":\"40\"}\" 1")
        );

    // obtain parameters & info

    PrintToLog("%s(): request.params[1].getType(): %d\n",__func__, request.params[1].getType());

    // if(!request.params[1].isObject()) PrintToLog("%s(): it's not Object!\n",__func__);

    auto& keys = request.params[1].getKeys();
    auto& vals = request.params[1].getValues();
    const std::string fromAddress = ParseAddress(request.params[0]);
    uint32_t propertyId = ParsePropertyId(request.params[2]);
    int64_t referenceAmount = (request.params.size() > 3) ? ParseAmount(request.params[3], true) : 0;

    uint64_t totalAmount = 0;
    std::vector<uint64_t> amounts;
    std::vector<std::string> recipients;
    for (size_t i=0; i<keys.size(); ++i) {
        auto v = ParseAmount(vals[i].getValStr(), isPropertyDivisible(propertyId));
        if (!v) continue;
        totalAmount += v;
        amounts.push_back(v);
        recipients.push_back(keys[i]);
    }

    RequireExistingProperty(propertyId);
    // RequireNotContract(propertyId);
    RequireBalance(fromAddress, propertyId, totalAmount);
    RequireSaneReferenceAmount(referenceAmount);

    // perform checks
    if (std::find_if(keys.begin(), keys.end(), [&fromAddress](const std::string& a){ return a == fromAddress; }) != keys.end()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "sending tokens to same address");
    }

    // create a payload for the transaction
    auto payload = CreatePayload_SendMany(propertyId, amounts);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilderEx(fromAddress, recipients, referenceAmount, payload, txid, rawHex, autoCommit);

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            PendingAdd(txid, fromAddress, MSC_TYPE_SEND_MANY, propertyId, totalAmount);
            return txid.GetHex();
        }
    }
}

UniValue tl_sendvesting(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() != 3)
    throw runtime_error(
			"tl_sendvesting \"fromaddress\" \"toaddress\" \"amount\" \n"

			"\nCreate and broadcast a simple send transaction.\n"

			"\nArguments:\n"
			"1. fromaddress          (string, required) the address to send from\n"
			"2. toaddress            (string, required) the address of the receiver\n"
			"3. amount               (string, required) the amount of vesting tokens to send\n"

			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_sendvesting", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" \"100.0\"")
			+ HelpExampleRpc("tl_sendvesting", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\", \"100.0\"")
			);

  // obtain parameters & info
  const std::string fromAddress = ParseAddress(request.params[0]);
  const std::string toAddress = ParseAddress(request.params[1]);
  int64_t amount = ParseAmount(request.params[2], true);


  RequireFeatureActivated(FEATURE_VESTING);

  // create a payload for the transaction
  std::vector<unsigned char> payload = CreatePayload_SendVestingTokens(amount);

  // request the wallet build the transaction (and if needed commit it)
  uint256 txid;
  std::string rawHex;
  int result = WalletTxBuilder(fromAddress, toAddress,0, payload, txid, rawHex, autoCommit);

  // check error and return the txid (or raw hex depending on autocommit)
  if (result != 0) {
    throw JSONRPCError(result, error_str(result));
  } else {
    if (!autoCommit) {
      return rawHex;
    } else {
      PendingAdd(txid, fromAddress,MSC_TYPE_SEND_VESTING, ALL_PROPERTY_TYPE_VESTING, amount);
      return txid.GetHex();
    }
  }
}

UniValue tl_sendall(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 2 || request.params.size() > 3)
        throw runtime_error(
            "tl_sendall \"fromaddress\" \"toaddress\" ( \"referenceamount\" )\n"

            "\nTransfers all available tokens to the recipient.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. toaddress            (string, required) the address of the receiver\n"
            "3. referenceamount      (string, optional) a bitcoin amount that is sent to the receiver (minimal by default)\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_sendall", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" 2")
            + HelpExampleRpc("tl_sendall", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" 2")
        );

    // obtain parameters & info
    const std::string fromAddress = ParseAddress(request.params[0]);
    const std::string toAddress = ParseAddress(request.params[1]);
    int64_t referenceAmount = (request.params.size() > 2) ? ParseAmount(request.params[2], true): 0;

    // perform checks
    RequireSaneReferenceAmount(referenceAmount);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_SendAll();

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

UniValue tl_sendissuancefixed(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 8)
        throw runtime_error(
            "tl_sendissuancefixed \"fromaddress\" \"type\" \"previousid\" \"name\" \"url\" \"data\" \"amount\" \"kyc\"\n"

            "\nCreate new tokens with fixed supply.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. type                 (number, required) the type of the tokens to create: (1 for indivisible tokens, 2 for divisible tokens)\n"
            "3. previousid           (number, required) an identifier of a predecessor token (use 0 for new tokens)\n"
            "4. name                 (string, required) the name of the new tokens to create\n"
            "5. url                  (string, required) an URL for further information about the new tokens (can be \"\")\n"
            "6. data                 (string, required) a description for the new tokens (can be \"\")\n"
            "7. amount               (string, required) the number of tokens to create\n"
            "8. kyc options          (array, required) A json with the kyc allowed.\n"
            "    [\n"
            "      2,3,5         (number) kyc id\n"
            "      ,...\n"
            "    ]\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_sendissuancefixed", "\"3Ck2kEGLJtZw9ENj2tameMCtS3HB7uRar3\" 2 1 \"Quantum Miner\" \"ww.qm.com\" \"dataexample\" \"1000000\"")
            + HelpExampleRpc("tl_sendissuancefixed", "\"3Ck2kEGLJtZw9ENj2tameMCtS3HB7uRar3\", 2, 1, \"Quantum Miner\", \"www.qm.com\", \"dataexample\", \"1000000\"")
        );

    // obtain parameters & info
    const std::string fromAddress = ParseAddress(request.params[0]);
    uint16_t type = ParsePropertyType(request.params[1]);
    uint32_t previousId = ParsePreviousPropertyId(request.params[2]);
    std::string name = ParseText(request.params[3]);
    std::string url = ParseText(request.params[4]);
    std::string data = ParseText(request.params[5]);
    int64_t amount = ParseAmount(request.params[6], type);
    std::vector<int> numbers = ParseArray(request.params[7]);

    // perform checks
    RequirePropertyName(name);
    RequireSaneName(name);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_IssuanceFixed(type, previousId, name, url, data, amount, numbers);

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
    if (request.fHelp || request.params.size() != 7)
        throw runtime_error(
            "tl_sendissuancemanaged \"fromaddress\" \"type\" \"previousid\" \"name\" \"url\" \"data\"  \"kyc\" \n"

            "\nCreate new tokens with manageable supply.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. type                 (number, required) the type of the tokens to create: (1 for indivisible tokens, 2 for divisible tokens)\n"
            "3. previousid           (number, required) an identifier of a predecessor token (use 0 for new tokens)\n"
            "4. name                 (string, required) the name of the new tokens to create\n"
            "5. url                  (string, required) an URL for further information about the new tokens (can be \"\")\n"
            "6. data                 (string, required) a description for the new tokens (can be \"\")\n"
            "7. kyc options          (array, required) A json with the kyc allowed.\n"
            "    [\n"
            "      2,3,5         (number) kyc id\n"
            "      ,...\n"
            "    ]\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_sendissuancemanaged", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\" 2 1 \"Companies\" \"www.companies.com\" \"dataexample\" \"[1,4,7]\"")
            + HelpExampleRpc("tl_sendissuancemanaged", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\", 2, 1, \"Companies\", \"www.companies.com\", \"dataexample\", \"[1,4,7]\"")
        );

    // obtain parameters & info
    const std::string fromAddress = ParseAddress(request.params[0]);
    uint16_t type = ParsePropertyType(request.params[1]);
    uint32_t previousId = ParsePreviousPropertyId(request.params[2]);

    // RequireNotContract(previousId);  TODO: check this condition
    std::string name = ParseText(request.params[3]);
    std::string url = ParseText(request.params[4]);
    std::string data = ParseText(request.params[5]);
    std::vector<int> numbers = ParseArray(request.params[6]);

    // perform checks
    RequirePropertyName(name);
    RequireSaneName(name);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_IssuanceManaged(type, previousId, name, url, data, numbers);

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

UniValue tl_sendgrant(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() < 4 || request.params.size() > 5)
    throw runtime_error(
			"tl_sendgrant \"fromaddress\" \"toaddress\" \"propertyid\" \"amount\"\n"

			"\nIssue or grant new units of managed tokens.\n"

			"\nArguments:\n"
			"1. fromaddress          (string, required) the address to send from\n"
			"2. toaddress            (string, required) the receiver of the tokens (sender by default, can be \"\")\n"
			"3. propertyid           (number, required) the identifier of the tokens to grant\n"
			"4. amount               (string, required) the amount of tokens to create\n"

			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_sendgrant", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\" \"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"51\" \"7000.3\"")
			+ HelpExampleRpc("tl_sendgrant", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\", \"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"51\", \"7000.3\"")
			);

  // obtain parameters & info
  const std::string fromAddress = ParseAddress(request.params[0]);
  const std::string toAddress = !ParseText(request.params[1]).empty() ? ParseAddress(request.params[1]): "";
  uint32_t propertyId = ParsePropertyId(request.params[2]);
  int64_t amount = ParseAmount(request.params[3], isPropertyDivisible(propertyId));

  // perform checks
  RequireExistingProperty(propertyId);
  RequireManagedProperty(propertyId);
  // RequireNotContract(propertyId);
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
    if (request.fHelp || request.params.size() != 3)
        throw runtime_error(
            "tl_sendrevoke \"fromaddress\" \"propertyid\" \"amount\" \n"

            "\nRevoke units of managed tokens.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to revoke the tokens from\n"
            "2. propertyid           (number, required) the identifier of the tokens to revoke\n"
            "3. amount               (string, required) the amount of tokens to revoke\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_sendrevoke", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\" \"51\" \"100\"")
            + HelpExampleRpc("tl_sendrevoke", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\", 51, \"100\"")
        );

    // obtain parameters & info
    const std::string fromAddress = ParseAddress(request.params[0]);
    uint32_t propertyId = ParsePropertyId(request.params[1]);
    int64_t amount = ParseAmount(request.params[2], isPropertyDivisible(propertyId));

    // perform checks
    RequireExistingProperty(propertyId);
    RequireManagedProperty(propertyId);
    // RequireNotContract(propertyId);
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

UniValue tl_sendchangeissuer(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 3)
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
    const std::string fromAddress = ParseAddress(request.params[0]);
    const std::string toAddress = ParseAddress(request.params[1]);
    uint32_t propertyId = ParsePropertyId(request.params[2]);

    // perform checks
    RequireExistingProperty(propertyId);
    RequireManagedProperty(propertyId);
    // RequireNotContract(propertyId);
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
    if (request.fHelp || request.params.size() != 4)
        throw runtime_error(
            "tl_sendactivation \"fromaddress\" \"featureid\" \"block\" \"minclientversion\" \n"

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
    const std::string fromAddress = ParseAddress(request.params[0]);
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
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "tl_senddeactivation \"fromaddress\" \"featureid\" \n"

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
    const std::string fromAddress = ParseAddress(request.params[0]);
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
    if (request.fHelp || request.params.size() != 4)
        throw runtime_error(
            "tl_sendalert \"fromaddress\" \"alerttype\" \"expiryvalue\" \"message\"\n"

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
            + HelpExampleCli("tl_sendalert", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\" \"1\" \"2\" \"alert\"")
            + HelpExampleRpc("tl_sendalert", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\", \"1\", \"2\" \"alert\"")
        );

    // obtain parameters & info
    const std::string fromAddress = ParseAddress(request.params[0]);
    int64_t tempAlertType = request.params[1].get_int64();
    if (tempAlertType < 1 || 65535 < tempAlertType) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Alert type is out of range");
    }
    uint16_t alertType = static_cast<uint16_t>(tempAlertType);
    int64_t tempExpiryValue = request.params[2].get_int64();
    if (tempExpiryValue < 1 || 4294967295L < tempExpiryValue) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Expiry value is out of range");
    }
    uint32_t expiryValue = static_cast<uint32_t>(tempExpiryValue);
    const std::string alertMessage = ParseText(request.params[3]);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_TradeLayerAlert(alertType, expiryValue, alertMessage);

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

UniValue tl_sendtrade(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() != 5) {
    throw runtime_error(
			"tl_sendtrade \"fromaddress\" \"propertyidforsale\" \"amountforsale\" \"propertiddesired\" \"amountdesired\"\n"

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
  }
  // obtain parameters & info
  const std::string fromAddress = ParseAddress(request.params[0]);
  uint32_t propertyIdForSale = ParsePropertyId(request.params[1]);
  int64_t amountForSale = ParseAmount(request.params[2], isPropertyDivisible(propertyIdForSale));
  uint32_t propertyIdDesired = ParsePropertyId(request.params[3]);
  int64_t amountDesired = ParseAmount(request.params[4], isPropertyDivisible(propertyIdDesired));

  // perform checks
  RequireFeatureActivated(FEATURE_METADEX);
  RequireExistingProperty(propertyIdForSale);
  // RequireNotContract(propertyIdForSale);
  RequireNotVesting(propertyIdForSale);
  RequireExistingProperty(propertyIdDesired);
  // RequireNotContract(propertyIdDesired);
  RequireNotVesting(propertyIdDesired);

  //checking amount+fee
  RequireAmountForFee(fromAddress, propertyIdForSale, amountForSale);

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
  if (request.fHelp || request.params.size() != 10)
    throw runtime_error(
			"tl_createcontract \"fromaddress\" \"numerator\" \"denominator\" \"name\" \"blocksuntilexpiration\" \"notionalsize\" \"collateralcurrency\" \"marginrequirement\" \"quoting\" \"kyc\" \n"

			"Create new Future Contract."

			"\nArguments:\n"
			"1. fromaddress               (string, required) the address to send from\n"
			"2. numerator                 (number, required) propertyId (Asset) \n"
      "3. denominator               (number, required) propertyId of denominator\n"
			"4. name                      (string, required) the name of the new tokens to create\n"
			"5. blocks until expiration   (number, required) life of contract, in blocks\n"
			"6. notional size             (number, required) notional size\n"
			"7. collateral currency       (number, required) collateral currency\n"
			"8. margin requirement        (number, required) margin requirement\n"
      "9. quoting                   (number, required) 1: inverse quoting contract, 0: normal quoting\n"
      "10. kyc options              (array, required) A json with the kyc allowed.\n"
      "    [\n"
      "      2,3,5         (number) kyc id\n"
      "      ,...\n"
      "    ]\n"

			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_createcontract", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\" 2 1 \"Contract1\" \"2560\" \"1\" \"3\" \"0.1\" 0 \"[1,2,4]\"")
			+ HelpExampleRpc("tl_createcontract", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\", 2, 1, \"Contract1\", \"2560\", \"1\", \"3\", \"0.1\", 0, \"[1,2,4]\"")
			);

   const std::string fromAddress = ParseAddress(request.params[0]);
   uint32_t num = ParsePropertyId(request.params[1]);
   uint32_t den = ParsePropertyId(request.params[2]);
   std::string name = ParseText(request.params[3]);
   uint32_t blocks_until_expiration = request.params[4].get_int();
   uint32_t notional_size = ParseAmount32t(request.params[5]);
   uint32_t collateral_currency = request.params[6].get_int();
   uint64_t margin_requirement = ParseAmount64t(request.params[7]);
   uint8_t inverse = ParseBinary(request.params[8]);
   std::vector<int> numbers = ParseArray(request.params[9]);

   RequirePropertyName(name);
   RequireSaneName(name);

   std::vector<unsigned char> payload = CreatePayload_CreateContract(num, den, name, blocks_until_expiration, notional_size, collateral_currency, margin_requirement,inverse, numbers);


   uint256 txid;
   std::string rawHex;
   int result = WalletTxBuilder(fromAddress, "", 0, payload, txid, rawHex, autoCommit);

   if (result != 0)
   {
      throw JSONRPCError(result, error_str(result));
   } else {
      if (!autoCommit)
	    {
	        return rawHex;
	    } else {
	        return txid.GetHex();
	    }
   }
}


UniValue tl_create_oraclecontract(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() != 9)
    throw runtime_error(
			"tl_create_oraclecontract \"address\" \"name\" \"blocksuntilexpiration\" \"notionalsize\" \"collateralcurrency\" \"marginrequirement\" \"backupaddress\" \"quoting\" \"kyc\" \n"

			"Create new Oracle Future Contract."

			"\nArguments:\n"
			"1. oracle address            (string, required) the address to send from (admin)\n"
			"2. name                      (string, required) the name of the new tokens to create\n"
			"3. blocks until expiration   (number, required) life of contract, in blocks\n"
			"4. notional size             (number, required) notional size\n"
			"5. collateral currency       (number, required) collateral currency\n"
			"6. margin requirement        (number, required) margin requirement\n"
      "7. backup address            (string, required) backup admin address contract\n"
      "8. quoting                   (number, required) 0: inverse quoting contract, 1: normal quoting\n"
      "9. kyc options               (array, required) A json with the kyc allowed.\n"
      "    [\n"
      "      2,3,5         (number) kyc id\n"
      "      ,...\n"
      "    ]\n"

			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_create_oraclecontract", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\" \"Contract1\" \"2560\" \"1\" \"3\" \"1\" \"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\" 0 \"[1,2,4]\"")
			+ HelpExampleRpc("tl_create_oraclecontract", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\", \"Contract1\", \"2560\", \"1\", \"3\", \"1\",\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\", 0, \"[1,2,4]\"")
			);

  const std::string fromAddress = ParseAddress(request.params[0]);
  std::string name = ParseText(request.params[1]);
  uint32_t blocks_until_expiration = request.params[2].get_int();
  uint32_t notional_size = ParseAmount32t(request.params[3]);
  uint32_t collateral_currency = request.params[4].get_int();
  uint64_t margin_requirement = ParseAmount64t(request.params[5]);
  std::string oracleAddress = ParseAddress(request.params[6]);
  uint8_t inverse = ParseBinary(request.params[7]);
  std::vector<int> numbers = ParseArray(request.params[8]);

  RequirePropertyName(name);
  RequireSaneName(name);

  std::vector<unsigned char> payload = CreatePayload_CreateOracleContract(name, blocks_until_expiration, notional_size, collateral_currency, margin_requirement, inverse, numbers);


  uint256 txid;
  std::string rawHex;
  int result = WalletTxBuilder(fromAddress, oracleAddress, 0, payload, txid, rawHex, autoCommit);

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
  if (request.fHelp || request.params.size() != 6)
    throw runtime_error(
			"tl_tradecontract \"fromaddress\" \"name (or id)\" \"amountforsale\" \"effectiveprice\" \"tradingaction\" \"leverage\" \n"

			"\nPlace a trade offer on the distributed Futures Contracts exchange.\n"

			"\nArguments:\n"
			"1. fromaddress          (string, required) the address to trade with\n"
			"2. name or id           (string, required) the name or the identifier of the contract to list for trade\n"
			"3. amountforsale        (number, required) the amount of contracts to trade\n"
			"4. effective price      (number, required) limit price desired in exchange\n"
			"5. trading action       (number, required) 1 to BUY contracts, 2 to SELL contracts \n"
			"6. leverage             (number, required) leverage (2x, 3x, ... 10x)\n"

			"\nResult:\n"
			"\"payload\"             (string) the hex-encoded payload\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_tradecontract", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\" \"31\" \"250.0\" \"10.0\" \"70.0\" \"80.0\"")
			+ HelpExampleRpc("tl_tradecontract", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\", 31,\"250.0\",1,\"10.0\"")
			);

      const std::string fromAddress = ParseAddress(request.params[0]);
      std::string name_traded = ParseText(request.params[1]);
      int64_t amountForSale = ParseAmountContract(request.params[2]);
      uint64_t effective_price = ParseEffectivePrice(request.params[3]);
      uint8_t trading_action = ParseContractDexAction(request.params[4]);
      uint64_t leverage = ParseLeverage(request.params[5]);

      // RequireContract(name_traded);

      RequireCollateral(fromAddress, name_traded, amountForSale, leverage);

      std::vector<unsigned char> payload = CreatePayload_ContractDexTrade(name_traded, amountForSale, effective_price, trading_action, leverage);

      uint256 txid;
      std::string rawHex;
      int result = WalletTxBuilder(fromAddress, "", 0, payload, txid, rawHex, autoCommit);

      if (result != 0)
      {
          throw JSONRPCError(result, error_str(result));
      } else {
          if (!autoCommit)
	        {
	            return rawHex;
          } else { //TODO: PendingAdd function
	            // PendingAdd(txid, fromAddress, MSC_TYPE_CONTRACTDEX_TRADE, propertyIdForSale, amountForSale);
	            return txid.GetHex();
          }
      }
}

UniValue tl_cancelallcontractsbyaddress(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() != 2)
    throw runtime_error(
			"tl_cancelallcontractsbyaddress \"fromaddress\" \"name (or id)\" \n"

			"\nCancel all offers on a given Futures Contract .\n"

			"\nArguments:\n"
			"1. fromaddress          (string, required) the address to trade with\n"
			"2. name or id           (string, required) the name (or id) of Future Contract \n"
			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_cancelallcontractsbyaddress", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\" \"1\"")
			+ HelpExampleRpc("tl_cancelallcontractsbyaddress", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\", \"1\"")
			);

  // obtain parameters & info
  std::string fromAddress = ParseAddress(request.params[0]);
  uint32_t contractId = ParseNameOrId(request.params[1]);

  // perform checks
  // RequireContract(contractId);
  // check, if there are matching offers to cancel
  RequireContractOrder(fromAddress, contractId);

  // create a payload for the transaction
  std::vector<unsigned char> payload = CreatePayload_ContractDexCancelAll(contractId);

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
      // PendingAdd(txid, fromAddress, MSC_TYPE_CONTRACTDEX_CANCEL_ECOSYSTEM, ecosystem, 0, false);
      return txid.GetHex();
    }
  }
}

UniValue tl_closeposition(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "tl_closeposition \"fromaddress\" \"contractid\" \n"

            "\nClose the position on a given Futures Contract .\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to trade with\n"
            "2. contractid           (number, required) the Id of Future Contract \n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_closeposition", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\" \"1\"")
            + HelpExampleRpc("tl_closeposition", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\", \"1\"")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    uint32_t contractId = ParsePropertyId(request.params[1]);

    // perform checks
    // RequireContract(contractId);
    RequireNoOrders(fromAddress, contractId);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_ContractDexClosePosition(contractId);

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
            // PendingAdd(txid, fromAddress, MSC_TYPE_CONTRACTDEX_CANCEL_ECOSYSTEM, ecosystem, 0, false);
            return txid.GetHex();
        }
    }
}



UniValue tl_sendissuance_pegged(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() != 7)
    throw runtime_error(
			"tl_sendissuance_pegged \"fromaddress\"  \"type\" \"previousid\" \"name\" \"collateralcurrency\" \"contractname (or id)\" \"amountofpegged\" \n"

			"\nCreate new pegged currency with manageable supply.\n"

			"\nArguments:\n"
			"1. fromaddress           (string, required) the address to send from\n"
			"2. type                  (number, required) the type of the pegged to create: (1 for indivisible tokens, 2 for divisible tokens)\n"
			"3. previousid            (number, required) an identifier of a predecessor token (use 0 for new tokens)\n"
			"4. name                  (string, required) the name of the new pegged to create\n"
			"5. collateralcurrency    (number, required) the collateral currency for the new pegged \n"
			"6. contract name or id   (string, required) the future contract name (or id) for the new pegged \n"
			"7. amount of pegged      (number, required) amount of pegged to create \n"

			"\nResult:\n"
			"\"hash\"                 (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_sendissuance_pegged", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\" 2 1  \"Companies\" \"4\" \"Quantum Miner Contract\" \"1000.0\"")
			+ HelpExampleRpc("tl_sendissuance_pegged", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\", 2, 1, \"Companies\", \"4\", \"Quantum Miner Contract\", \"1000.0\"")
			);

  // obtain parameters & info
  std::string fromAddress = ParseAddress(request.params[0]);
  uint16_t type = ParsePropertyType(request.params[1]);
  uint32_t previousId = ParsePreviousPropertyId(request.params[2]);
  std::string name = ParseText(request.params[3]);
  uint32_t propertyId = ParsePropertyId(request.params[4]);
  uint32_t contractId = ParseNameOrId(request.params[5]);
  uint64_t amount = ParseAmount(request.params[6], isPropertyDivisible(propertyId));

  // perform checks
  RequirePeggedSaneName(name);

  // Checking existing
  RequireExistingProperty(propertyId);

  // Checking for future contract
  // RequireContract(contractId);

  // Checking for short position in given future contract
  RequireShort(fromAddress, contractId, amount);

  // checking for collateral balance, checking for short position in given contract
  // RequireForPegged(fromAddress, propertyId, contractId, amount);

  // create a payload for the transaction
  std::vector<unsigned char> payload = CreatePayload_IssuancePegged(type, previousId, name, propertyId, contractId, amount);

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
  if (request.fHelp || request.params.size() != 4)
    throw runtime_error(
			"tl_send \"fromaddress\" \"toaddress\" \"propertyname\" \"amount\" \n"

			"\nSend the pegged currency to other addresses.\n"

			"\nArguments:\n"
			"1. fromaddress          (string, required) the address to send from\n"
			"2. toaddress            (string, required) the address of the receiver\n"
			"3. propertyid           (number, required) the identifier the dMoney\n"
			"4. amount               (string, required) the amount of dMoney to send\n"

			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_send_pegged", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\"  \"1\" \"100.0\"")
			+ HelpExampleRpc("tl_send_pegged", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\", \"1\", \"100.0\"")
			);

  // obtain parameters & info
  const std::string fromAddress = ParseAddress(request.params[0]);
  const std::string toAddress = ParseAddress(request.params[1]);
  const uint32_t propertyId = ParsePropertyId(request.params[2]);

  int64_t amount = ParseAmount(request.params[3], true);

  // perform checks
  RequirePeggedCurrency(propertyId);
  RequireBalance(fromAddress, propertyId, amount);

  // create a payload for the transaction
  std::vector<unsigned char> payload = CreatePayload_SendPeggedCurrency(propertyId, amount);

  // request the wallet build the transaction (and if needed commit it)
  uint256 txid;
  std::string rawHex;
  int result = WalletTxBuilder(fromAddress, toAddress,0, payload, txid, rawHex, autoCommit);

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
  if (request.fHelp || request.params.size() != 4)
    throw runtime_error(
			"tl_redemption_pegged \"fromaddress\" \"name (or id)\" \"amount\" \"nameofcontract\"\n"

			"\n Redemption of the pegged currency .\n"

			"\nArguments:\n"
			"1. redeemaddress        (string, required) the address of owner \n"
			"2. peggedId             (number, required) id of pegged tokens to redeem\n"
			"3. amount               (number, required) the amount of pegged currency for redemption"
			"4. contractId          (string, required) the identifier of the future contract involved\n"

			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_redemption_pegged", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"Pegged1\" \"100.0\"  \"Contract1\"")
			+ HelpExampleRpc("tl_redemption_pegged", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"Pegged1\", \"100.0\", \"Contract1\"")
			);

  // obtain parameters & info
  const std::string fromAddress = ParseAddress(request.params[0]);
  const uint32_t propertyId = ParsePropertyId(request.params[1]);
  uint32_t contractId = ParseNameOrId(request.params[3]);

  uint64_t amount = ParseAmount(request.params[2], true);

  // perform checks
  RequireExistingProperty(propertyId);
  RequirePeggedCurrency(propertyId);
  // RequireContract(contractId);
  RequireBalance(fromAddress, propertyId, amount);

  // is given contract origin of pegged?
  RequireAssociation(propertyId,contractId);

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
    if (request.fHelp || request.params.size() != 3)
        throw runtime_error(
            "tl_cancelorderbyblock \"fromaddress\" \"block\" \"idx\" \n"

            "\nCancel an specific offer on the MetaDEx.\n"

            "\nArguments:\n"
            "1. address         (string, required) the txid of order to cancel\n"
            "2. block           (number, required) the block of order to cancel\n"
            "2. idx             (number, required) the idx in block of order to cancel\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_cancelorderbyblock", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\" \"1\" \"2\"")
            + HelpExampleRpc("tl_cancelorderbyblock", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\", \"1\", \"2\"")
        );

    // obtain parameters & info
    const std::string fromAddress = ParseAddress(request.params[0]);
    int block = static_cast<int>(ParseNewValues(request.params[1]));
    int idx = static_cast<int>(ParseNewValues(request.params[2]));

    RequireFeatureActivated(FEATURE_METADEX);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_ContractDexCancelOrderByTxId(block,idx);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", 0, payload, txid, rawHex, autoCommit);

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
  if (request.fHelp || request.params.size() != 8) {
    throw runtime_error(
			"tl_senddexoffer \"fromaddress\" \"propertyidforsale\" \"amountforsale\" \"amountdesired\" \"paymentwindow\" \"minacceptfee\" \"option\" \"action\" \n"

			"\nPlace, update or cancel a sell offer on the traditional distributed Trade Layer/LTC exchange.\n"

			"\nArguments:\n"

			"1. fromaddress         (string, required) the address to send from\n"
			"2. propertyidoffer     (number, required) the identifier of the tokens to list for sale\n"
			"3. amountoffering      (string, required) the amount of tokens to list for sale\n"
			"4. price               (string, required) the price in litecoin of the offer \n"
			"5. paymentwindow       (number, required) a time limit in blocks a buyer has to pay following a successful accepting order\n"
			"6. minacceptfee        (string, required) a minimum mining fee a buyer has to pay to accept the offer\n"
			"7. option              (number, required) 1 for buy tokens, 2 to sell\n"
			"8. action              (number, required) the action to take (1 for new offers, 2 to update\", 3 to cancel)\n"

			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_senddexoffer", "\"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" \"1\" \"1.5\" \"0.75\" 25 \"0.0005\" \"1\" \"2\"")
			+ HelpExampleRpc("tl_senddexoffer", "\"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\", \"1\", \"1.5\", \"0.75\", 25, \"0.0005\", \"1\", \"2\"")
			);
  }

  // obtain parameters & info
  const std::string fromAddress = ParseAddress(request.params[0]);
  uint32_t propertyIdForSale = ParsePropertyId(request.params[1]);
  int64_t amountForSale = ParseAmount(request.params[2], isPropertyDivisible(propertyIdForSale));
  int64_t price = ParseAmount(request.params[3], true); // BTC is divisible
  uint8_t paymentWindow = ParseDExPaymentWindow(request.params[4]);
  int64_t minAcceptFee = ParseDExFee(request.params[5]);
  int64_t option = ParseAmount(request.params[6], false);  // buy : 1 ; sell : 2;
  uint8_t action = ParseDExAction(request.params[7]);

  std::vector<unsigned char> payload;

  if (action == 1) RequireNoOtherDExOffer(fromAddress, propertyIdForSale);

  if (option == 1)
  {
      RequireFeatureActivated(FEATURE_DEX_BUY);
      payload = CreatePayload_DEx(propertyIdForSale, amountForSale, price, paymentWindow, minAcceptFee, action);
  } else {
      RequireFeatureActivated(FEATURE_DEX_SELL);
      RequireBalance(fromAddress, propertyIdForSale, amountForSale);
      payload = CreatePayload_DExSell(propertyIdForSale, amountForSale, price, paymentWindow, minAcceptFee, action);
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
    if (request.fHelp || request.params.size() < 4 || request.params.size() > 5)
        throw runtime_error(
            "tl_senddexaccept \"fromaddress\" \"toaddress\" \"propertyid\" \"amount\" ( override )\n"

            "\nCreate and broadcast an accept offer for the specified token and amount.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. toaddress            (string, required) the address of the seller\n"
            "3. propertyid           (number, required) the identifier of the token traded\n"
            "4. amount               (string, required) the amount traded\n"
            "5. override             (boolean or number, optional) override minimum accept fee and payment window checks , options: true (1), false (0) (use with caution!)\n"

            "\nResult:\n"
            "\"hash\"                (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_senddexaccept", "\"35URq1NN3xL6GeRKUP6vzaQVcxoJiiJKd8\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" 1 \"15.0\"")
            + HelpExampleRpc("tl_senddexaccept", "\"35URq1NN3xL6GeRKUP6vzaQVcxoJiiJKd8\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\", 1, \"15.0\"")
        );

    // obtain parameters & info
    const std::string fromAddress = ParseAddress(request.params[0]);
    const std::string toAddress = ParseAddress(request.params[1]);
    uint32_t propertyId = ParsePropertyId(request.params[2]);
    int64_t amount = ParseAmount(request.params[3], true); // MSC/TMSC is divisible

    // Accept either a bool (true) or a num (>=1) to indicate override output.
    bool override = false;
    if (request.params.size() > 4) {
        override = request.params[4].isNum() ? (request.params[4].get_int() != 0) : request.params[4].get_bool();
    }

    // perform checks
    RequireFeatureActivated(FEATURE_DEX_SELL);
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
        if (sellOffer == nullptr) throw JSONRPCError(RPC_TYPE_ERROR, "Unable to load sell offer from the distributed exchange");
        nMinimumAcceptFee = sellOffer->getMinFee();
    }

#endif

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_DExAccept(propertyId, amount);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, toAddress, 0, payload, txid, rawHex, autoCommit, nMinimumAcceptFee);

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

UniValue tl_setoracle(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 5)
        throw runtime_error(
            "tl_setoracle \"fromaddress\" \"contractname\" \"high\" \"low \" \"close\" \n"

            "\nSet the price for an oracle address.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the oracle address for the Future Contract\n"
            "2. contract name        (string, required) the name of the Future Contract\n"
            "3. high price           (number, required) the highest price of the asset\n"
            "4. low price            (number, required) the lowest price of the asset\n"
            "5. close price          (number, required) the close price of the asset\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_setoracle", "\"1ARjWDkZ7kT9fwjPrjcQyvbXDkEySzKHwu\" ,\"3HTHRxu3aSDV4de+akjC7VmsiUp7c6dfbvs\" ,\"Contract 1\" \"900.2\" \"400.0\" \"600.5\"")
            + HelpExampleRpc("tl_setoracle", "\"1ARjWDkZ7kT9fwjPrjcQyvbXDkEySzKHwu\", \"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\", \"Contract 1\", \"900.2\",\"400.0\",\"600.5\"")
        );

    // obtain parameters & info
    const std::string fromAddress = ParseAddress(request.params[0]);
    uint32_t contractId = ParseNameOrId(request.params[1]);
    uint64_t high = ParseEffectivePrice(request.params[2]);
    uint64_t low = ParseEffectivePrice(request.params[3]);
    uint64_t close = ParseEffectivePrice(request.params[4]);

    CDInfo::Entry cd;
    assert(_my_cds->getCD(contractId, cd));

    const std::string& oracleAddress = cd.issuer;

    // checks
    if (oracleAddress != fromAddress)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "address is not the oracle address of contract");

    RequireExistingProperty(contractId);
    RequireOracleContract(contractId);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_Set_Oracle(contractId, high, low, close);

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

UniValue tl_change_oracleadm(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 3)
        throw runtime_error(
            "tl_change_oracleadm \"fromaddress\" \"toaddress\" \"contractname\" \n"

            "\nChange the admin on record of the Oracle Future Contract.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address associated with the oracle Future Contract\n"
            "2. toaddress            (string, required) the address to transfer administrative control to\n"
            "3. name or id           (string, required) the name (or id) of the Future Contract\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_change_oracleadm", "\"1ARjWDkZ7kT9fwjPrjcQyvbXDkEySzKHwu\" ,\"3HTHRxu3aSDV4de+akjC7VmsiUp7c6dfbvs\" ,\"Contract 1\"")
            + HelpExampleRpc("tl_change_oracleadm", "\"1ARjWDkZ7kT9fwjPrjcQyvbXDkEySzKHwu\", \"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\", \"Contract 1\"")
        );

    // obtain parameters & info
    const std::string fromAddress = ParseAddress(request.params[0]);
    const std::string toAddress = ParseAddress(request.params[1]);
    uint32_t contractId = ParseNameOrId(request.params[2]);

    PrintToLog("%s(): contractId: %d\n",__func__, contractId);

    CDInfo::Entry cd;
    assert(_my_cds->getCD(contractId, cd));

    const std::string& oracleAddress = cd.issuer;

    // checks
    if (oracleAddress != fromAddress)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "address is not the oracle address of contract");

    RequireExistingProperty(contractId);
    RequireOracleContract(contractId);  //RequireOracleContract


    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_Change_OracleAdm(contractId);

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


UniValue tl_oraclebackup(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "tl_oraclebackup \"oracleaddress\" \"contractname\" \n"

            "\n Activation of backup address (backup is now the new oracle address).\n"

            "\nArguments:\n"
            "1. backup address          (string, required) the address associated with the oracle Future Contract\n"
            "2. contract name           (string, required) the name of the Future Contract\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_oraclebackup", "\"1ARjWDkZ7kT9fwjPrjcQyvbXDkEySzKHwu\" ,\"Contract 1\"")
            + HelpExampleRpc("tl_oraclebackup", "\"1ARjWDkZ7kT9fwjPrjcQyvbXDkEySzKHwu\", \"Contract 1\"")
        );

    // obtain parameters & info
    const std::string fromAddress = ParseAddress(request.params[0]);
    uint32_t contractId = ParseNameOrId(request.params[1]);


    CDInfo::Entry cd;
    assert(_my_cds->getCD(contractId, cd));

    const std::string& backupAddress = cd.backup_address;

    // checks
    if (backupAddress != fromAddress)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "address is not the backup address of contract");

    RequireExistingProperty(contractId);
    RequireOracleContract(contractId);  //RequireOracleContract

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_OracleBackup(contractId);

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

UniValue tl_closeoracle(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "tl_closeoracle \"backupaddress\" \"contractname\" \n"

            "\nClose an Oracle Future Contract.\n"

            "\nArguments:\n"
            "1. backup address         (string, required) the backup address associated with the oracle Future Contract\n"
            "2. name or id             (string, required) the name of the Oracle Future Contract\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_closeoracle", "\"1ARjWDkZ7kT9fwjPrjcQyvbXDkEySzKHwu\" , \"Contract 1\"")
            + HelpExampleRpc("tl_closeoracle", "\"1ARjWDkZ7kT9fwjPrjcQyvbXDkEySzKHwu\", \"Contract 1\"")
        );

    // obtain parameters & info
    const std::string backupAddress = ParseAddress(request.params[0]);
    uint32_t contractId = ParseNameOrId(request.params[1]);

    CDInfo::Entry cd;
    assert(_my_cds->getCD(contractId, cd));

    const std::string& bckup_address  = cd.backup_address;

    // checks
    if (bckup_address != backupAddress)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "address is not the backup address of contract");

    RequireExistingProperty(contractId);
    RequireOracleContract(contractId);  //RequireOracleContract


    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_Close_Oracle(contractId);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(backupAddress, "", 0, payload, txid, rawHex, autoCommit);

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


UniValue tl_commit_tochannel(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 4)
        throw runtime_error(
            "tl_commit_tochannel \"sender\" \"channeladdress\" \"propertyId\" \"amount\" \n"

            "\nCommit fundings into the channel.\n"

            "\nArguments:\n"
            "1. sender                 (string, required) the sender address that commit into the channel\n"
            "2. channel address        (string, required) multisig address of channel\n"
            "3. propertyId             (number, required) the propertyId of token commited into the channel\n"
            "4. amount                 (number, required) amount of tokens traded in the channel\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"

            + HelpExampleCli("tl_commit_tochannel", "\"1M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" \"100\" \"1\"")
            + HelpExampleRpc("tl_commit_tochannel", "\"1M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\", \"100\", \"1\"")
        );

    // obtain parameters & info
    const std::string senderAddress = ParseAddress(request.params[0]);
    const std::string channelAddress = ParseAddress(request.params[1]);
    uint32_t propertyId = ParsePropertyId(request.params[2]);
    int64_t amount = ParseAmount(request.params[3], isPropertyDivisible(propertyId));

    RequireFeatureActivated(FEATURE_TRADECHANNELS_TOKENS);
    RequireExistingProperty(propertyId);
    RequireBalance(senderAddress, propertyId, amount);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_Commit_Channel(propertyId, amount);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(senderAddress, channelAddress, 0, payload, txid, rawHex, autoCommit);

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

UniValue tl_withdrawal_fromchannel(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 4)
        throw runtime_error(
            "tl_withdrawal_fromchannel \"sender\" \"channel address\" \"propertyId\" \"amount\" \n"

            "\nwithdrawal from the channel.\n"

            "\nArguments:\n"
            "1. sender                 (string, required) the address that claims for withdrawal from channel\n"
            "2. channel address        (string, required) multisig address of channel\n"
            "3. propertyId             (number, required) the propertyId of token commited into the channel\n"
            "4. amount                 (number, required) amount to withdrawal from channel\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_withdrawal_fromchannel", "\"1M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" \"100\" \"1\"")
            + HelpExampleRpc("tl_withdrawal_fromchannel", "\"1M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\", \"100\", \"1\"")
        );

    // obtain parameters & info
    const std::string senderAddress = ParseAddress(request.params[0]);
    const std::string channelAddress = ParseAddress(request.params[1]);
    uint32_t propertyId = ParsePropertyId(request.params[2]);
    int64_t amount = ParseAmount(request.params[3], true);

    RequireFeatureActivated(FEATURE_TRADECHANNELS_TOKENS);
    RequireExistingProperty(propertyId);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_Withdrawal_FromChannel(propertyId, amount);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(senderAddress, channelAddress, 0, payload, txid, rawHex, autoCommit);

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


UniValue tl_new_id_registration(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 3)
        throw runtime_error(
            "tl_new_id_registration \"sender\" \"websiteurl\" \"companyname\" \n"

            "\n KYC: setting identity registrar Id number for address.\n"

            "\nArguments:\n"
            "1. sender                       (string, required) sender address\n"
            "2. website url                  (string, required) official web site of company\n"
            "3. company name                 (string, required) official name of company\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_new_id_registration", "\"1M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"www.companyone.com\" \"company one\"")
            + HelpExampleRpc("tl_new_id_registration", "\"1M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"www.companyone.com\", \"company one\"")
        );

    // obtain parameters & info
    std::string sender = ParseAddress(request.params[0]);
    std::string website = ParseText(request.params[1]);
    std::string name = ParseText(request.params[2]);

    RequireFeatureActivated(FEATURE_KYC);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_New_Id_Registration(website, name);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(sender, "", 0, payload, txid, rawHex, autoCommit);
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

UniValue tl_update_id_registration(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "tl_update_id_registration \"address\" \"newaddress\" \n"

            "\nupdate the address on id registration.\n"

            "\nArguments:\n"
            "1. address                      (string, required) old address registered\n"
            "2. new address                  (string, required) new address into register\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_update_id_registration", "\"1M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" , \"1M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\"")
            + HelpExampleRpc("tl_update_id_registration", "\"1M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\",  \"1M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\"")
        );

    // obtain parameters & info
    const std::string address = ParseAddress(request.params[0]);
    const std::string newAddr = ParseAddress(request.params[1]);

    RequireFeatureActivated(FEATURE_KYC);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_Update_Id_Registration();

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(address, newAddr, 0, payload, txid, rawHex, autoCommit);
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

UniValue tl_send_dex_payment(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 3)
        throw runtime_error(
            "tl_send_dex_payment \"fromaddress\" \"toaddress\" \"amount\" \n"

            "\nCreate and broadcast a dex payment.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. toaddress            (string, required) the address of the receiver\n"
            "3. amount               (string, required) the amount of Litecoins to send\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_send_dex_payment", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\"100.0\"")
            + HelpExampleRpc("tl_send_dex_payment", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\",\"100.0\"")
        );

    // obtain parameters & info
    const std::string fromAddress = ParseAddress(request.params[0]);
    const std::string toAddress = ParseAddress(request.params[1]);
    int64_t amount = ParseAmount(request.params[2], true);

    RequireFeatureActivated(FEATURE_DEX_SELL);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_DEx_Payment();

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, toAddress, amount, payload, txid, rawHex, autoCommit);

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

UniValue tl_attestation(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 3)
        throw runtime_error(
            "tl_attestation \"fromaddress\" \"toaddress\" \"amount\" \n"

            "\nCreate and broadcast a kyc attestation.\n"

            "\nArguments:\n"
            "1. sender address       (string, required) authority address\n"
            "2. receiver address     (string, required) receiver address\n"
            "3. string hash          (string, optional) the hash\n"
            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_attestation", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" \"hash\"")
            + HelpExampleRpc("tl_attestation", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\", \"hash\"")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string receiverAddress = ParseAddress(request.params[1]);
    std::string hash = (request.params.size() == 3) ? ParseText(request.params[2]) : "";

    RequireFeatureActivated(FEATURE_KYC);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_Attestation(hash);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, receiverAddress, 0, payload, txid, rawHex, autoCommit);

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

UniValue tl_revoke_attestation(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "tl_revoke_attestation \"fromaddress\" \"toaddress\" \n"

            "\nRevoke the kyc attestation.\n"

            "\nArguments:\n"
            "1. sender address       (string, required) authority address\n"
            "2. receiver address     (string, required) receiver address\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_revoke_attestation", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PtMtv41\"")
            + HelpExampleRpc("tl_revoke_attestation", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSgRTv2\"")
        );

    // obtain parameters & info
    const std::string fromAddress = ParseAddress(request.params[0]);
    const std::string receiverAddress = ParseAddress(request.params[1]);

    RequireFeatureActivated(FEATURE_KYC);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_Revoke_Attestation();

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, receiverAddress, 0, payload, txid, rawHex, autoCommit);

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

UniValue tl_sendcancelalltrades(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            "tl_sendcancelalltrades \n"

            "\nCancel all metaDEx orders.\n"

            "\nArguments:\n"
            "1. address       (string, required) authority address\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_sendcancelalltrades", "\"\"")
            + HelpExampleRpc("tl_sendcancelalltrades", "\"\"")
        );

    // obtain parameters & info
    const std::string fromAddress = ParseAddress(request.params[0]);

    RequireFeatureActivated(FEATURE_METADEX);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_MetaDExCancelAll();

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

UniValue tl_sendcancel_order(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
           "tl_sendcancel_order \"address\" \"hash\" \n"

           "\nCancel specific metaDEx order .\n"

           "\nArguments:\n"
           "1. address       (string, required) sender address\n"
           "2. txid          (string, required) transaction hash\n"

           "\nExamples:\n"
         + HelpExampleCli("tl_sendcancel_order", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"e8a97a4ab61389d6a4174beb8c0dbd52d094132d64d2a0bc813ab4942050f036\"")
         + HelpExampleRpc("tl_sendcancel_order", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"e8a97a4ab61389d6a4174beb8c0dbd52d094132d64d2a0bc813ab4942050f036\"")
        );

    // obtain parameters & info
    const std::string fromAddress = ParseAddress(request.params[0]);
    std::string stxS = ParseHash(request.params[1]);

    RequireFeatureActivated(FEATURE_METADEX);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_DExCancel(stxS);

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

UniValue tl_sendcanceltradesbypair(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 3)
        throw runtime_error(
          "tl_sendcanceltradesbypair \"address\" \"propertyidforsale\" \"propertyiddesired\" \n"

          "\nCancel specific contract order .\n"

          "\nArguments:\n"
          "1. fromaddress               (string, required) sender address\n"
          "2. propertyidforsale         (string, required) the identifier of the tokens listed for sale\n"
          "3. propertyiddesired         (string, required) the identifier of the tokens desired in exchange\n"

          "\nExamples:\n"
          + HelpExampleCli("\"tl_sendcanceltradesbypair\"",  "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\" \"1\" \"31\"")
          + HelpExampleRpc("tl_sendcanceltradesbypair", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\", \"1\", \"31\"")
        );

    // obtain parameters & info
    const std::string fromAddress = ParseAddress(request.params[0]);
    uint32_t propertyIdForSale = ParsePropertyId(request.params[1]);
    uint32_t propertyIdDesired = ParsePropertyId(request.params[2]);

    // perform checks
    RequireFeatureActivated(FEATURE_METADEX);
    RequireExistingProperty(propertyIdForSale);
    RequireExistingProperty(propertyIdDesired);
    RequireDifferentIds(propertyIdForSale, propertyIdDesired);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_MetaDExCancelPair(propertyIdForSale, propertyIdDesired);

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

UniValue tl_sendcanceltradesbyprice(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 5)
        throw runtime_error(
          "tl_sendcanceltradesbyprice \"address\" \"propertyidforsale\" \"propertyiddesired\"\n"

          "\nCancel specific contract order .\n"

          "\nArguments:\n"
          "1. fromaddress          (string, required) the address to trade with\n"
    			"2. propertyidforsale    (number, required) the identifier of the tokens to list for sale\n"
    			"3. amountforsale        (string, required) the amount of tokens to list for sale\n"
    			"4. propertiddesired     (number, required) the identifier of the tokens desired in exchange\n"
    			"5. amountdesired        (string, required) the amount of tokens desired in exchange\n"

          "\nExamples:\n"
          + HelpExampleCli("\"tl_sendcanceltradesbyprice\"", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\" \"1\" \"31\" \"6\" \"71\"")
          + HelpExampleRpc("tl_sendcanceltradesbyprice", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\", \"1\", \"31\", \"2\", \"56\"")
        );

    // obtain parameters & info
    const std::string fromAddress = ParseAddress(request.params[0]);
    uint32_t propertyIdForSale = ParsePropertyId(request.params[1]);
    int64_t amountForSale = ParseAmount(request.params[2], isPropertyDivisible(propertyIdForSale));
    uint32_t propertyIdDesired = ParsePropertyId(request.params[3]);
    int64_t amountDesired = ParseAmount(request.params[4], isPropertyDivisible(propertyIdDesired));

    // perform checks
    RequireFeatureActivated(FEATURE_METADEX);
    RequireExistingProperty(propertyIdForSale);
    RequireExistingProperty(propertyIdDesired);

    RequireDifferentIds(propertyIdForSale, propertyIdDesired);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_MetaDExCancelPrice(propertyIdForSale, amountForSale, propertyIdDesired, amountDesired);

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

UniValue tl_sendcancel_contract_order(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
           "tl_sendcancel_contract_order \"address\" \"txid\" \n"

           "\nCancel specific contract order .\n"

           "\nArguments:\n"
           "1. address       (string, required) sender address\n"
           "2. txid          (string, required) transaction hash\n"

           "\nExamples:\n"
           + HelpExampleCli("tl_sendcancel_contract_order", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\" \"e8a97a4ab61389d6a4174beb8c0dbd52d094132d64d2a0bc813ab4942050f036\"")
           + HelpExampleRpc("tl_sendcancel_contract_order", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\", \"e8a97a4ab61389d6a4174beb8c0dbd52d094132d64d2a0bc813ab4942050f036\"")
        );

    // obtain parameters & info
    const std::string fromAddress = ParseAddress(request.params[0]);
    std::string stxS = ParseHash(request.params[1]);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_ContractDExCancel(stxS);

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


UniValue tl_send_closechannel(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
           "tl_send_closechannel \"address\" \"channel\" \n"

           "\nClose Trade Channel .\n"

           "\nArguments:\n"
           "1. address       (string, required) sender address, must be part of the multisig\n"
           "2. channel       (string, required) the channel address\n"

           "\nExamples:\n"
           + HelpExampleCli("tl_send_closechannel", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\" \"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\"")
           + HelpExampleRpc("tl_send_closechannel", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\", \"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\"")
        );

    // obtain parameters & info
    const std::string fromAddress = ParseAddress(request.params[0]);
    const std::string channelAddr = ParseAddress(request.params[1]);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_Close_Channel();

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, channelAddr, 0, payload, txid, rawHex, autoCommit);

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

UniValue tl_senddonation(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 3 || request.params.size() > 4)
        throw runtime_error(
            "tl_senddonation \"fromaddress\" \"propertyid\" \"amount\" ( \"referenceamount\" )\n"

            "\nCreate and broadcast a donation transaction.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. propertyid           (number, required) the identifier of the tokens to send\n"
            "3. amount               (string, required) the amount to send\n"
            "4. referenceamount      (string, optional) a litecoin amount that is sent to the receiver (minimal by default)\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_senddonation", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" 1 \"100.0\"")
            + HelpExampleRpc("tl_senddonation", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", 1, \"100.0\"")
        );

    // obtain parameters & info
    const std::string fromAddress = ParseAddress(request.params[0]);
    uint32_t propertyId = ParsePropertyId(request.params[1]);
    int64_t amount = ParseAmount(request.params[2], isPropertyDivisible(propertyId));
    int64_t referenceAmount = (request.params.size() > 3) ? ParseAmount(request.params[3], true): 0;

    RequireExistingProperty(propertyId);
    // RequireNotContract(propertyId);
    RequireBalance(fromAddress, propertyId, amount);
    RequireSaneReferenceAmount(referenceAmount);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_SendDonation(propertyId, amount);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", referenceAmount, payload, txid, rawHex, autoCommit);

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


static const CRPCCommand commands[] =
{ //  category                             name                            actor (function)               okSafeMode
  //  ------------------------------------ ------------------------------- ------------------------------ ----------
#ifdef ENABLE_WALLET
    { "trade layer (transaction creation)", "tl_sendrawtx",                    &tl_sendrawtx,                       {} },
    { "trade layer (transaction creation)", "tl_send",                         &tl_send,                            {} },
    { "trade layer (transaction creation)", "tl_sendvesting",                  &tl_sendvesting,                     {} },
    { "trade layer (transaction creation)", "tl_sendissuancefixed",            &tl_sendissuancefixed,               {} },
    { "trade layer (transaction creation)", "tl_sendissuancemanaged",          &tl_sendissuancemanaged,             {} },
    { "trade layer (transaction creation)", "tl_sendgrant",                    &tl_sendgrant,                       {} },
    { "trade layer (transaction creation)", "tl_sendrevoke",                   &tl_sendrevoke,                      {} },
    { "trade layer (transaction creation)", "tl_sendchangeissuer",             &tl_sendchangeissuer,                {} },
    { "trade layer (transaction creation)", "tl_sendall",                      &tl_sendall,                         {} },
    { "trade layer (transaction creation)", "tl_sendmany",                     &tl_sendmany,                        {} },
    { "hidden",                             "tl_senddeactivation",             &tl_senddeactivation,                {} },
    { "hidden",                             "tl_sendactivation",               &tl_sendactivation,                  {} },
    { "hidden",                             "tl_sendalert",                    &tl_sendalert,                       {} },
    { "trade layer (transaction creation)", "tl_createcontract",               &tl_createcontract,                  {} },
    { "trade layer (transaction creation)", "tl_tradecontract",                &tl_tradecontract,                   {} },
    { "trade layer (transaction creation)", "tl_sendcancel_contract_order",    &tl_sendcancel_contract_order,       {} },
    { "trade layer (transaction creation)", "tl_cancelallcontractsbyaddress",  &tl_cancelallcontractsbyaddress,     {} },
    { "trade layer (transaction creation)", "tl_cancelorderbyblock",           &tl_cancelorderbyblock,              {} },
    { "trade layer (transaction creation)", "tl_sendissuance_pegged",          &tl_sendissuance_pegged,             {} },
    { "trade layer (transaction creation)", "tl_send_pegged",                  &tl_send_pegged,                     {} },
    { "trade layer (transaction creation)", "tl_redemption_pegged",            &tl_redemption_pegged,               {} },
    { "trade layer (transaction creation)", "tl_closeposition",                &tl_closeposition,                   {} },
    { "trade layer (transaction creation)", "tl_sendtrade",                    &tl_sendtrade,                       {} },
    { "trade layer (transaction creation)", "tl_sendcancelalltrades",          &tl_sendcancelalltrades,             {} },
    { "trade layer (transaction creation)", "tl_senddexoffer",                 &tl_senddexoffer,                    {} },
    { "trade layer (transaction creation)", "tl_senddexaccept",                &tl_senddexaccept,                   {} },
    { "trade layer (transaction cration)",  "tl_send_dex_payment",             &tl_send_dex_payment,                {} },
    { "trade layer (transaction creation)", "tl_create_oraclecontract",        &tl_create_oraclecontract,           {} },
    { "trade layer (transaction creation)", "tl_setoracle",                    &tl_setoracle,                       {} },
    { "trade layer (transaction creation)", "tl_change_oracleadm",             &tl_change_oracleadm,                {} },
    { "trade layer (transaction creation)", "tl_oraclebackup",                 &tl_oraclebackup,                    {} },
    { "trade layer (transaction creation)", "tl_closeoracle",                  &tl_closeoracle,                     {} },
    { "trade layer (transaction creation)", "tl_commit_tochannel",             &tl_commit_tochannel,                {} },
    { "trade layer (transaction creation)", "tl_withdrawal_fromchannel",       &tl_withdrawal_fromchannel,          {} },
    { "trade layer (transaction cration)",  "tl_new_id_registration",          &tl_new_id_registration,             {} },
    { "trade layer (transaction cration)",  "tl_update_id_registration",       &tl_update_id_registration,          {} },
    { "trade layer (transaction creation)", "tl_attestation",                  &tl_attestation,                     {} },
    { "trade layer (transaction creation)", "tl_revoke_attestation",           &tl_revoke_attestation,              {} },
    { "trade layer (transaction creation)", "tl_sendcancel_order",             &tl_sendcancel_order,                {} },
    { "trade layer (transaction creation)", "tl_sendcanceltradesbypair",       &tl_sendcanceltradesbypair,          {} },
    { "trade layer (transaction creation)", "tl_sendcanceltradesbyprice",      &tl_sendcanceltradesbyprice,         {} },
    { "trade layer (transaction creation)", "tl_send_closechannel",            &tl_send_closechannel,               {} },
    { "trade layer (transaction creation)", "tl_submit_nodeaddress",           &tl_submit_nodeaddress,              {} },
    { "trade layer (transaction creation)", "tl_claim_nodereward",             &tl_claim_nodereward,                {} },
    { "trade layer (transaction creation)", "tl_sendmany",                     &tl_sendmany,                        {} },
    { "trade layer (transaction creation)", "tl_senddonation",                 &tl_senddonation,                    {} }
#endif
};

void RegisterTLTransactionCreationRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
