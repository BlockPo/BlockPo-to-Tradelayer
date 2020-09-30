#ifndef TRADELAYER_ENCODING_H
#define TRADELAYER_ENCODING_H

class CPubKey;
class CTxOut;

#include <script/script.h>

#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

/** Embedds a payload in an OP_RETURN output, prefixed with a transaction marker. */
bool TradeLayer_Encode_ClassD(const std::vector<unsigned char>& vecPayload, std::vector<std::pair<CScript, int64_t> >& vecOutputs);

#endif // TRADELAYER_ENCODING_H
