#ifndef OMNICORE_RPCRAWTX_H
#define OMNICORE_RPCRAWTX_H

#include "rpc/server.h"
#include <univalue.h>

UniValue tl_decodetransaction(const JSONRPCRequest& request);
UniValue tl_createrawtx_opreturn(const JSONRPCRequest& request);
UniValue tl_createrawtx_input(const JSONRPCRequest& request);
UniValue tl_createrawtx_reference(const JSONRPCRequest& request);
UniValue tl_createrawtx_change(const JSONRPCRequest& request);


#endif // OMNICORE_RPCRAWTX_H
