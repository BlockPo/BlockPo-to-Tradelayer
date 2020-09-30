#ifndef TRADELAYER_SCRIPT_H
#define TRADELAYER_SCRIPT_H

#include <script/standard.h>

#include <string>
#include <vector>

/** Determines the minimum output amount to be spent by an output. */
int64_t GetDustThld(const CScript& scriptPubKey);

/** Identifies standard output types based on a scriptPubKey. */
bool GetOutputType(const CScript& scriptPubKey, txnouttype& whichTypeRet);

/** Extracts the pushed data as hex-encoded string from a script. */
bool GetScriptPushes(const CScript& script, std::vector<std::string>& vstrRet, bool fSkipFirst = false);

/** Returns public keys or hashes from scriptPubKey, for standard transaction types. */
bool SafeSolver(const CScript& scriptPubKey, txnouttype& typeRet, std::vector<std::vector<unsigned char> >& vSolutionsRet);


#endif // TRADELAYER_SCRIPT_H
