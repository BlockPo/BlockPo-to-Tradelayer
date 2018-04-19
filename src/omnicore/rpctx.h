#ifndef OMNICORE_RPCTX
#define OMNICORE_RPCTX

#include <univalue.h>

UniValue omni_sendrawtx(const UniValue& params, bool fHelp);
UniValue omni_send(const UniValue& params, bool fHelp);
UniValue omni_sendall(const UniValue& params, bool fHelp);
UniValue omni_sendissuancecrowdsale(const UniValue& params, bool fHelp);
UniValue omni_sendissuancefixed(const UniValue& params, bool fHelp);
UniValue omni_sendissuancemanaged(const UniValue& params, bool fHelp);
UniValue omni_sendgrant(const UniValue& params, bool fHelp);
UniValue omni_sendrevoke(const UniValue& params, bool fHelp);
UniValue omni_sendclosecrowdsale(const UniValue& params, bool fHelp);
UniValue omni_sendchangeissuer(const UniValue& params, bool fHelp);
UniValue omni_sendactivation(const UniValue& params, bool fHelp);
UniValue omni_sendalert(const UniValue& params, bool fHelp);

#endif // OMNICORE_RPCTX
