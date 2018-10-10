#include "omnicore/encoding.h"

#include "omnicore/omnicore.h"
#include "omnicore/script.h"

#include "base58.h"
#include "pubkey.h"
#include "random.h"
#include "script/script.h"
#include "script/standard.h"
#include "utilstrencodings.h"

#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

/**
 * Embedds a payload in an OP_RETURN output, prefixed with a transaction marker.
 *
 * The request is rejected, if the size of the payload with marker is larger than
 * the allowed data carrier size ("-datacarriersize=n").
 */
bool OmniCore_Encode_ClassD(const std::vector<unsigned char>& vchPayload,
        std::vector<std::pair <CScript, int64_t> >& vecOutputs)
{
    std::vector<unsigned char> vchData;
    std::vector<unsigned char> vchOmBytes = GetOmMarker();
    vchData.insert(vchData.end(), vchOmBytes.begin(), vchOmBytes.end());
    vchData.insert(vchData.end(), vchPayload.begin(), vchPayload.end());
    if (vchData.size() > nMaxDatacarrierBytes) { return false; }

    CScript script = CScript() << OP_RETURN << vchData;
    vecOutputs.push_back(std::make_pair(script, 0));

    return true;
}
