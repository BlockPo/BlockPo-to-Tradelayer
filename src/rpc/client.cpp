// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/client.h>
#include <rpc/protocol.h>
#include <util/system.h>

#include <set>
#include <stdint.h>

class CRPCConvertParam
{
public:
    std::string methodName; //!< method whose params want conversion
    int paramIdx;           //!< 0-based idx of param to convert
    std::string paramName;  //!< parameter name
};

/**
 * Specify a (method, idx, name) here if the argument is a non-string RPC
 * argument and needs to be converted from JSON.
 *
 * @note Parameter indexes start from 0.
 */
static const CRPCConvertParam vRPCConvertParams[] =
{
    { "setmocktime", 0, "timestamp" },
    { "generate", 0, "nblocks" },
    { "generate", 1, "maxtries" },
    { "generatetoaddress", 0, "nblocks" },
    { "generatetoaddress", 2, "maxtries" },
    { "getnetworkhashps", 0, "nblocks" },
    { "getnetworkhashps", 1, "height" },
    { "sendtoaddress", 1, "amount" },
    { "sendtoaddress", 4, "subtractfeefromamount" },
    { "sendtoaddress", 5 , "replaceable" },
    { "sendtoaddress", 6 , "conf_target" },
    { "settxfee", 0, "amount" },
    { "getreceivedbyaddress", 1, "minconf" },
    { "getreceivedbyaccount", 1, "minconf" },
    { "listreceivedbyaddress", 0, "minconf" },
    { "listreceivedbyaddress", 1, "include_empty" },
    { "listreceivedbyaddress", 2, "include_watchonly" },
    { "listreceivedbyaccount", 0, "minconf" },
    { "listreceivedbyaccount", 1, "include_empty" },
    { "listreceivedbyaccount", 2, "include_watchonly" },
    { "getbalance", 1, "minconf" },
    { "getbalance", 2, "include_watchonly" },
    { "getblockhash", 0, "height" },
    { "waitforblockheight", 0, "height" },
    { "waitforblockheight", 1, "timeout" },
    { "waitforblock", 1, "timeout" },
    { "waitfornewblock", 0, "timeout" },
    { "move", 2, "amount" },
    { "move", 3, "minconf" },
    { "sendfrom", 2, "amount" },
    { "sendfrom", 3, "minconf" },
    { "listtransactions", 1, "count" },
    { "listtransactions", 2, "skip" },
    { "listtransactions", 3, "include_watchonly" },
    { "listaccounts", 0, "minconf" },
    { "listaccounts", 1, "include_watchonly" },
    { "walletpassphrase", 1, "timeout" },
    { "getblocktemplate", 0, "template_request" },
    { "listsinceblock", 1, "target_confirmations" },
    { "listsinceblock", 2, "include_watchonly" },
    { "listsinceblock", 3, "include_removed" },
    { "sendmany", 1, "amounts" },
    { "sendmany", 2, "minconf" },
    { "sendmany", 4, "subtractfeefrom" },
    { "sendmany", 5 , "replaceable" },
    { "sendmany", 6 , "conf_target" },
    { "addmultisigaddress", 0, "nrequired" },
    { "addmultisigaddress", 1, "keys" },
    { "createmultisig", 0, "nrequired" },
    { "createmultisig", 1, "keys" },
    { "listunspent", 0, "minconf" },
    { "listunspent", 1, "maxconf" },
    { "listunspent", 2, "addresses" },
    { "listunspent", 3, "include_unsafe" },
    { "listunspent", 4, "query_options" },
    { "getblock", 1, "verbosity" },
    { "getblock", 1, "verbose" },
    { "getblockheader", 1, "verbose" },
    { "getchaintxstats", 0, "nblocks" },
    { "gettransaction", 1, "include_watchonly" },
    { "getrawtransaction", 1, "verbose" },
    { "createrawtransaction", 0, "inputs" },
    { "createrawtransaction", 1, "outputs" },
    { "createrawtransaction", 2, "locktime" },
    { "createrawtransaction", 3, "replaceable" },
    { "decoderawtransaction", 1, "iswitness" },
    { "signrawtransaction", 1, "prevtxs" },
    { "signrawtransaction", 2, "privkeys" },
    { "sendrawtransaction", 1, "allowhighfees" },
    { "combinerawtransaction", 0, "txs" },
    { "fundrawtransaction", 1, "options" },
    { "fundrawtransaction", 2, "iswitness" },
    { "gettxout", 1, "n" },
    { "gettxout", 2, "include_mempool" },
    { "gettxoutproof", 0, "txids" },
    { "lockunspent", 0, "unlock" },
    { "lockunspent", 1, "transactions" },
    { "importprivkey", 2, "rescan" },
    { "importaddress", 2, "rescan" },
    { "importaddress", 3, "p2sh" },
    { "importpubkey", 2, "rescan" },
    { "importmulti", 0, "requests" },
    { "importmulti", 1, "options" },
    { "verifychain", 0, "checklevel" },
    { "verifychain", 1, "nblocks" },
    { "pruneblockchain", 0, "height" },
    { "keypoolrefill", 0, "newsize" },
    { "getrawmempool", 0, "verbose" },
    { "estimatefee", 0, "nblocks" },
    { "estimatesmartfee", 0, "conf_target" },
    { "estimaterawfee", 0, "conf_target" },
    { "estimaterawfee", 1, "threshold" },
    { "prioritisetransaction", 1, "dummy" },
    { "prioritisetransaction", 2, "fee_delta" },
    { "setban", 2, "bantime" },
    { "setban", 3, "absolute" },
    { "setnetworkactive", 0, "state" },
    { "getmempoolancestors", 1, "verbose" },
    { "getmempooldescendants", 1, "verbose" },
    { "bumpfee", 1, "options" },
    { "logging", 0, "include" },
    { "logging", 1, "exclude" },
    { "disconnectnode", 1, "nodeid" },
    { "addwitnessaddress", 1, "p2sh" },
    // Echo with conversion (For testing only)
    { "echojson", 0, "arg0" },
    { "echojson", 1, "arg1" },
    { "echojson", 2, "arg2" },
    { "echojson", 3, "arg3" },
    { "echojson", 4, "arg4" },
    { "echojson", 5, "arg5" },
    { "echojson", 6, "arg6" },
    { "echojson", 7, "arg7" },
    { "echojson", 8, "arg8" },
    { "echojson", 9, "arg9" },
    { "rescanblockchain", 0, "start_height"},
    { "rescanblockchain", 1, "stop_height"},

     /* Trade Layer - payload creation */
    { "tl_createpayload_sendactivation", 0, "arg0"},
    { "tl_createpayload_sendactivation", 1, "arg1"},
    { "tl_createpayload_sendactivation", 2, "arg2"},
    { "tl_createpayload_senddeactivation", 0, "arg0"},
    { "tl_createpayload_sendalert", 0, "arg0"},
    { "tl_createpayload_sendalert", 1, "arg1"},
    { "tl_createpayload_simplesend", 0, "arg0" },

    { "tl_createpayload_dexsell", 0, "arg0" },
    { "tl_createpayload_dexsell", 3, "arg3" },
    { "tl_createpayload_dexsell", 5, "arg5" },
    { "tl_createpayload_dexaccept", 0, "arg0" },
    { "tl_createpayload_issuancefixed", 0, "arg0" },
    { "tl_createpayload_issuancefixed", 1, "arg1" },
    { "tl_createpayload_issuancefixed", 5, "arg5" },
    { "tl_createpayload_issuancemanaged", 0, "arg0" },
    { "tl_createpayload_issuancemanaged", 1, "arg1" },
    { "tl_createpayload_issuancemanaged", 5, "arg5" },

    { "tl_createpayload_sendgrant", 0, "arg0" },
    { "tl_createpayload_sendrevoke", 0, "arg0" },
    { "tl_createpayload_changeissuer", 0, "arg0" },
    { "tl_createpayload_sendtrade", 0, "arg0" },
    { "tl_createpayload_sendtrade", 2, "arg2" },
    { "tl_createpayload_createcontract", 0, "arg0"},
    { "tl_createpayload_createcontract", 1, "arg1"},
    { "tl_createpayload_createcontract", 3, "arg3"},
    { "tl_createpayload_createcontract", 4, "arg4"},
    { "tl_createpayload_createcontract", 5, "arg5"},
    { "tl_createpayload_createcontract", 6, "arg6"},
    { "tl_createpayload_createcontract", 7, "arg7"},
    { "tl_createpayload_createcontract", 8, "arg8"},


    { "tl_createpayload_tradecontract", 2, "arg2"},
    { "tl_createpayload_tradecontract", 3, "arg3"},
    { "tl_createpayload_cancelallcontractsbyaddress", 0, "arg0"},
    { "tl_createpayload_closeposition", 0, "arg0" },
    { "tl_createpayload_sendissuance_pegged", 0, "arg0" },
    { "tl_createpayload_sendissuance_pegged", 1, "arg1" },
    { "tl_createpayload_sendissuance_pegged", 3, "arg3" },
    { "tl_createpayload_cancelorderbyblock", 0, "arg0"},
    { "tl_createpayload_cancelorderbyblock", 1, "arg1" },
    { "tl_createpayload_dexoffer", 0, "arg0" },
    { "tl_createpayload_dexoffer", 3, "arg3" },
    { "tl_createpayload_dexoffer", 6, "arg6" },
    { "tl_createpayload_senddexaccept", 1, "arg1" },
    { "tl_createpayload_sendvesting", 0, "arg0"},
    { "tl_createpayload_instant_trade", 0, "arg0"},
    { "tl_createpayload_instant_trade", 2, "arg2"},
    { "tl_createpayload_instant_trade", 4, "arg4"},

    { "tl_createpayload_contract_instant_trade", 0, "arg0"},
    { "tl_createpayload_contract_instant_trade", 2, "arg2"},
    { "tl_createpayload_contract_instant_trade", 4, "arg4"},
    // { "tl_createpayload_contract_instant_trade", 5, "arg5"},

    { "tl_createpayload_create_oraclecontract", 1, "arg1"},
    { "tl_createpayload_create_oraclecontract", 2, "arg2"},
    { "tl_createpayload_create_oraclecontract", 3, "arg3"},
    { "tl_createpayload_create_oraclecontract", 4, "arg4"},
    { "tl_createpayload_create_oraclecontract", 5, "arg5"},
    { "tl_createpayload_create_oraclecontract", 7, "arg7"},

    { "tl_createpayload_instant_ltc_trade", 0, "arg0"},
    { "tl_createpayload_instant_ltc_trade", 3, "arg3"},

    { "tl_createpayload_pnl_update", 0, "arg0"},
    { "tl_createpayload_pnl_update", 2, "arg2"},
    { "tl_createpayload_pnl_update", 3, "arg3"},
    { "tl_createpayload_pnl_update", 4, "arg4"},

    { "tl_createpayload_setoracle", 1, "arg1" },
    { "tl_createpayload_setoracle", 2, "arg2" },
    { "tl_createpayload_setoracle", 3, "arg3" },

    { "tl_createpayload_commit_tochannel", 0, "arg0" },

    { "tl_createpayload_withdrawal_fromchannel", 0, "arg0" },


    { "tl_createpayload_transfer", 0, "arg0"},
    { "tl_createpayload_transfer", 1, "arg1"},


    /* Trade Layer - raw transaction calls */
    { "tl_createrawtx_reference", 2, "arg2" },
    { "tl_createrawtx_input", 2, "arg2" },
    { "tl_createrawtx_change", 1, "arg1" },
    { "tl_createrawtx_change", 3, "arg3" },
    { "tl_createrawtx_change", 4, "arg4" },


    /* Trade Layer - transaction calls */
    { "tl_send", 2, "arg2" },
    { "tl_senddonation", 1, "arg1" },
    // { "tl_sendvesting", 2, "arg2" },
    { "tl_sendissuancemanaged", 1, "arg1" },
    { "tl_sendissuancemanaged", 2, "arg2" },
    { "tl_sendissuancemanaged", 6, "arg6" },

    { "tl_sendissuancefixed", 1, "arg1" },
    { "tl_sendissuancefixed", 2, "arg2" },
    { "tl_sendissuancefixed", 7, "arg7" },

    { "tl_createcontract", 1, "arg1"},
    { "tl_createcontract", 2, "arg2"},
    { "tl_createcontract", 4, "arg4"},
    { "tl_createcontract", 5, "arg5"},
    { "tl_createcontract", 6, "arg6"},
    { "tl_createcontract", 7, "arg7"},
    { "tl_createcontract", 8, "arg8"},
    { "tl_createcontract", 9, "arg9"},

    { "tl_create_oraclecontract", 2, "arg2"},
    { "tl_create_oraclecontract", 3, "arg3"},
    { "tl_create_oraclecontract", 4, "arg4"},
    { "tl_create_oraclecontract", 5, "arg5"},
    { "tl_create_oraclecontract", 7, "arg7"},
    { "tl_create_oraclecontract", 8, "arg8"},
    { "tl_tradecontract", 3, "arg3"},
    { "tl_tradecontract", 4, "arg4"},
    { "tl_sendgrant", 2, "arg2"},
    { "tl_getcontract_orderbook", 1, "arg1" },
    { "tl_cancelorderbyblock", 1, "arg1"},
    { "tl_cancelorderbyblock", 2, "arg2" },
    { "tl_closeposition", 1, "arg1" },
    { "tl_sendtrade", 1, "arg1" },
    { "tl_sendtrade", 3, "arg3" },
    { "tl_getmax_peggedcurrency", 1, "arg1" },
    { "tl_sendissuance_pegged", 1, "arg1" },
    { "tl_sendissuance_pegged", 2, "arg2" },
    { "tl_sendissuance_pegged", 4, "arg4" },
    { "tl_sendissuance_pegged", 5, "arg5" },
    { "tl_send_pegged", 3, "arg3" },
    { "tl_senddexoffer", 1, "arg1" },
    { "tl_senddexoffer", 4, "arg4" },
    { "tl_senddexoffer", 7, "arg7" },
    { "tl_listtransactions", 1, "arg1" },
    { "tl_listtransactions", 2, "arg2" },
    { "tl_listtransactions", 3, "arg3" },
    { "tl_listtransactions", 4, "arg4" },
    { "tl_setoracle", 2, "arg2" },
    { "tl_setoracle", 3, "arg3" },
    { "tl_setoracle", 4, "arg4" },
    { "tl_commit_tochannel", 2, "arg2" },
    { "tl_commit_tochannel", 4, "arg4" },
    { "tl_withdrawal_fromchannel", 2, "arg2" },
    { "tl_withdrawal_fromchannel", 4, "arg4" },

    { "tl_sendactivation", 1, "arg1"},
    { "tl_sendactivation", 2, "arg2"},
    { "tl_sendactivation", 3, "arg3"},

    {"tl_senddeactivation", 1, "arg1"},

    {"tl_sendcanceltradesbypair", 1, "arg1"},
    {"tl_sendcanceltradesbypair", 2, "arg2"},

    {"tl_redemption_pegged ", 1, "arg1"},
    {"tl_redemption_pegged ", 3, "arg3"},


    /* Trade Layer - data retrieval calls */
    { "tl_setautocommit", 0, "arg0" },
    { "tl_getgrants", 0, "arg0"},
    { "tl_getbalance", 1, "arg1" },
    { "tl_getreserve", 1, "arg1" },
    { "tl_getproperty", 0, "arg0" },
    { "tl_getupnl", 1, "arg1" },
    { "tl_getpnl", 1, "arg1" },
    { "tl_listproperties", 0, "verbose" },
    { "tl_listtransactions", 1, "arg1" },
    { "tl_listtransactions", 2, "arg2" },
    { "tl_listtransactions", 3, "arg3" },
    { "tl_listtransactions", 4, "arg4" },
    { "tl_getallbalancesforid", 0, "arg0" },
    { "tl_listblocktransactions", 0, "arg0" },
    { "tl_getalltxonblock", 0, "arg0" },
    { "tl_gettradehistory_unfiltered", 0, "arg0" },
    { "tl_getpeggedhistory",0, "arg0" },
    { "tl_getorderbook",0, "arg0" },
    { "tl_getorderbook",1, "arg1" },
    { "tl_getcontract_reserve", 1 ,"arg1" },
    { "tl_getmargin", 1, "arg1" },
    { "tl_senddexaccept", 2, "arg2" },
    { "tl_senddexaccept", 4, "arg4" },
    { "tl_getmarketprice", 0, "arg0" },
    {"tl_getaverage_entry",1,"arg1" },
    { "tl_getcache", 0, "arg0" }, // NOTE: only to test persistence
    { "tl_get_channelreserve", 1, "arg1" },
    { "tl_check_kyc", 1, "arg1" },
    { "tl_getoraclecache", 0, "arg0" },

    { "tl_get_ltcvolume", 0, "arg1" },
    { "tl_get_ltcvolume", 1, "arg2" },
    { "tl_get_ltcvolume", 2, "arg3" },

    { "tl_getmdexvolume", 0, "arg1" },
    { "tl_getmdexvolume", 1, "arg2" },
    { "tl_getmdexvolume", 2, "arg3" },
    { "tl_getmdexvolume", 3, "arg4" },
    { "tl_getcurrencytotal", 0, "arg0" },

    { "tl_gettradehistory", 0, "arg0" },

    {"tl_getupnl", 2, "arg2"},

    {"tl_get_channelremaining", 2, "arg2"},

    { "tl_getmdextradehistoryforaddress", 1 , "arg1"},
    { "tl_getmdextradehistoryforaddress", 2, "arg2" },

    { "tl_getdextradehistoryforaddress", 1 , "arg1"},
    { "tl_getdextradehistoryforaddress", 2, "arg2" },

    { "tl_gettradehistoryforpair", 0, "arg0" },
    { "tl_gettradehistoryforpair", 1, "arg1" },
    { "tl_gettradehistoryforpair", 2, "arg2" },

    { "tl_getchannel_historyforpair", 1, "arg1" },
    { "tl_getchannel_historyforpair", 2, "arg2" },
    { "tl_getchannel_historyforpair", 3, "arg3" },

    { "tl_getchannel_historyforaddress", 2 , "arg2"},
    { "tl_getchannel_historyforaddress", 3, "arg3" },


    { "tl_getchannel_tokenhistoryforaddresses", 2 , "arg2"},
    { "tl_getchannel_tokenhistoryforaddresses", 3, "arg3" },

    { "tl_sendmany", 1, "json" },
    { "tl_sendmany", 2, "propertyid" },

};

class CRPCConvertTable
{
private:
    std::set<std::pair<std::string, int>> members;
    std::set<std::pair<std::string, std::string>> membersByName;

public:
    CRPCConvertTable();

    bool convert(const std::string& method, int idx) {
        return (members.count(std::make_pair(method, idx)) > 0);
    }
    bool convert(const std::string& method, const std::string& name) {
        return (membersByName.count(std::make_pair(method, name)) > 0);
    }
};

CRPCConvertTable::CRPCConvertTable()
{
    const unsigned int n_elem =
        (sizeof(vRPCConvertParams) / sizeof(vRPCConvertParams[0]));

    for (unsigned int i = 0; i < n_elem; i++) {
        members.insert(std::make_pair(vRPCConvertParams[i].methodName,
                                      vRPCConvertParams[i].paramIdx));
        membersByName.insert(std::make_pair(vRPCConvertParams[i].methodName,
                                            vRPCConvertParams[i].paramName));
    }
}

static CRPCConvertTable rpcCvtTable;

/** Non-RFC4627 JSON parser, accepts internal values (such as numbers, true, false, null)
 * as well as objects and arrays.
 */
UniValue ParseNonRFCJSONValue(const std::string& strVal)
{
    UniValue jVal;
    if (!jVal.read(std::string("[")+strVal+std::string("]")) ||
        !jVal.isArray() || jVal.size()!=1)
        throw std::runtime_error(std::string("Error parsing JSON:")+strVal);
    return jVal[0];
}

UniValue RPCConvertValues(const std::string &strMethod, const std::vector<std::string> &strParams)
{
    UniValue params(UniValue::VARR);

    for (unsigned int idx = 0; idx < strParams.size(); idx++) {
        const std::string& strVal = strParams[idx];

        if (!rpcCvtTable.convert(strMethod, idx)) {
            // insert string value directly
            params.push_back(strVal);
        } else {
            // parse string as JSON, insert bool/number/object/etc. value
            params.push_back(ParseNonRFCJSONValue(strVal));
        }
    }

    return params;
}

UniValue RPCConvertNamedValues(const std::string &strMethod, const std::vector<std::string> &strParams)
{
    UniValue params(UniValue::VOBJ);

    for (const std::string &s: strParams) {
        size_t pos = s.find('=');
        if (pos == std::string::npos) {
            throw(std::runtime_error("No '=' in named argument '"+s+"', this needs to be present for every argument (even if it is empty)"));
        }

        std::string name = s.substr(0, pos);
        std::string value = s.substr(pos+1);

        if (!rpcCvtTable.convert(strMethod, name)) {
            // insert string value directly
            params.pushKV(name, value);
        } else {
            // parse string as JSON, insert bool/number/object/etc. value
            params.pushKV(name, ParseNonRFCJSONValue(value));
        }
    }

    return params;
}
