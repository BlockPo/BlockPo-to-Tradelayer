#ifndef OMNICORE_RPCPAYLOAD_H
#define OMNICORE_RPCPAYLOAD_H

#include <univalue.h>

UniValue omni_createpayload_simplesend(const UniValue& params, bool fHelp);
UniValue omni_createpayload_sendall(const UniValue& params, bool fHelp);
UniValue omni_createpayload_issuancefixed(const UniValue& params, bool fHelp);
UniValue omni_createpayload_issuancecrowdsale(const UniValue& params, bool fHelp);
UniValue omni_createpayload_issuancemanaged(const UniValue& params, bool fHelp);
UniValue omni_createpayload_closecrowdsale(const UniValue& params, bool fHelp);
UniValue omni_createpayload_grant(const UniValue& params, bool fHelp);
UniValue omni_createpayload_revoke(const UniValue& params, bool fHelp);
UniValue omni_createpayload_changeissuer(const UniValue& params, bool fHelp);

#endif // OMNICORE_RPCPAYLOAD_H
