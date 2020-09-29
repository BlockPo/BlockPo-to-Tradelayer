#ifndef TRADELAYER_RPCRAWTX_H
#define TRADELAYER_RPCRAWTX_H

#include <rpc/server.h>
#include <univalue.h>

UniValue tl_decodetransaction(const JSONRPCRequest& request);
UniValue tl_createrawtx_opreturn(const JSONRPCRequest& request);
UniValue tl_createrawtx_input(const JSONRPCRequest& request);
UniValue tl_createrawtx_reference(const JSONRPCRequest& request);
UniValue tl_createrawtx_change(const JSONRPCRequest& request);


#endif // TRADELAYER_RPCRAWTX_H
