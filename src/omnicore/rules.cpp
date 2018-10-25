/**
 * @file rules.cpp
 *
 * This file contains consensus rules and restrictions.
 */

#include "omnicore/rules.h"

#include "omnicore/activation.h"
#include "omnicore/consensushash.h"
#include "omnicore/log.h"
#include "omnicore/omnicore.h"
#include "omnicore/notifications.h"
#include "omnicore/utilsbitcoin.h"
#include "omnicore/version.h"

#include "chainparams.h"
#include "validation.h"
#include "script/standard.h"
#include "uint256.h"
#include "ui_interface.h"

#include <openssl/sha.h>

#include <stdint.h>
#include <limits>
#include <string>
#include <vector>

namespace mastercore
{
/**
 * Returns a mapping of transaction types, and the blocks at which they are enabled.
 */
std::vector<TransactionRestriction> CConsensusParams::GetRestrictions() const
{
    const TransactionRestriction vTxRestrictions[] =
    { //  transaction type                    version        allow 0  activation block
      //  ----------------------------------  -------------  -------  ------------------
        { OMNICORE_MESSAGE_TYPE_ALERT,        0xFFFF,        true,             MSC_ALERT_BLOCK    },
        { OMNICORE_MESSAGE_TYPE_ACTIVATION,   0xFFFF,        true,             MSC_ALERT_BLOCK    },
        { OMNICORE_MESSAGE_TYPE_DEACTIVATION, 0xFFFF,        true,             MSC_ALERT_BLOCK    },

        { MSC_TYPE_SIMPLE_SEND,               MP_TX_PKT_V0,  false,            MSC_SEND_BLOCK     },

        { MSC_TYPE_CREATE_PROPERTY_FIXED,     MP_TX_PKT_V0,  false,            MSC_SP_BLOCK       },
        { MSC_TYPE_CREATE_PROPERTY_VARIABLE,  MP_TX_PKT_V0,  false,            MSC_SP_BLOCK       },
        { MSC_TYPE_CREATE_PROPERTY_VARIABLE,  MP_TX_PKT_V1,  false,            MSC_SP_BLOCK       },
        { MSC_TYPE_CLOSE_CROWDSALE,           MP_TX_PKT_V0,  false,            MSC_SP_BLOCK       },

        { MSC_TYPE_CREATE_PROPERTY_MANUAL,    MP_TX_PKT_V0,  false,            MSC_MANUALSP_BLOCK },
        { MSC_TYPE_GRANT_PROPERTY_TOKENS,     MP_TX_PKT_V0,  false,            MSC_MANUALSP_BLOCK },
        { MSC_TYPE_REVOKE_PROPERTY_TOKENS,    MP_TX_PKT_V0,  false,            MSC_MANUALSP_BLOCK },
        { MSC_TYPE_CHANGE_ISSUER_ADDRESS,     MP_TX_PKT_V0,  false,            MSC_MANUALSP_BLOCK },
        { MSC_TYPE_CREATE_CONTRACT,           MP_TX_PKT_V0,  false,         MSC_CONTRACTDEX_BLOCK },
        { MSC_TYPE_CONTRACTDEX_TRADE,         MP_TX_PKT_V0,  false,         MSC_CONTRACTDEX_BLOCK },
        { MSC_TYPE_CONTRACTDEX_CANCEL_ECOSYSTEM, MP_TX_PKT_V0,  false,      MSC_CONTRACTDEX_BLOCK },
        { MSC_TYPE_PEGGED_CURRENCY,           MP_TX_PKT_V0,  false,         MSC_CONTRACTDEX_BLOCK },
        { MSC_TYPE_SEND_PEGGED_CURRENCY,           MP_TX_PKT_V0,  false,    MSC_CONTRACTDEX_BLOCK },
        { MSC_TYPE_SEND_ALL,                  MP_TX_PKT_V0,  false,            MSC_SEND_ALL_BLOCK },
        { MSC_TYPE_CONTRACTDEX_CLOSE_POSITION,MP_TX_PKT_V0,  false,         MSC_CONTRACTDEX_BLOCK },
        {MSC_TYPE_CONTRACTDEX_CANCEL_ORDERS_BY_BLOCK,MP_TX_PKT_V0,  false,   MSC_CONTRACTDEX_BLOCK},
        {MSC_TYPE_METADEX_TRADE,              MP_TX_PKT_V0,  false,          MSC_CONTRACTDEX_BLOCK},
        {MSC_TYPE_TRADE_OFFER,                MP_TX_PKT_V1,  false,          MSC_CONTRACTDEX_BLOCK},
        {MSC_TYPE_ACCEPT_OFFER_BTC,           MP_TX_PKT_V1,  false,          MSC_CONTRACTDEX_BLOCK},
        {MSC_TYPE_DEX_BUY_OFFER,              MP_TX_PKT_V1,  false,          MSC_CONTRACTDEX_BLOCK}

    };

    const size_t nSize = sizeof(vTxRestrictions) / sizeof(vTxRestrictions[0]);

    return std::vector<TransactionRestriction>(vTxRestrictions, vTxRestrictions + nSize);
}

/**
 * Returns an empty vector of consensus checkpoints.
 *
 * This method should be overwriten by the child classes, if needed.
 */
std::vector<ConsensusCheckpoint> CConsensusParams::GetCheckpoints() const
{
    return std::vector<ConsensusCheckpoint>();
}

/**
 * Returns consensus checkpoints for mainnet, used to verify transaction processing.
 */
std::vector<ConsensusCheckpoint> CMainConsensusParams::GetCheckpoints() const
{
    // block height, block hash and consensus hash
    const ConsensusCheckpoint vCheckpoints[] = {
/**
TODO : New chain checkpoints
        { 250000, uint256S("000000000000003887df1f29024b06fc2200b55f8af8f35453d7be294df2d214"),
                  uint256S("c2e1e0f3cf3c49d8ee08bd45ad39be27eb400041d6288864ee144892449c97df") },
        { 440000, uint256S("0000000000000000038cc0f7bcdbb451ad34a458e2d535764f835fdeb896f29b"),
                  uint256S("94e3e045b846b35226c1b7c9399992515094e227fd195626e3875ad812b44e7a") },
**/
    };

    const size_t nSize = sizeof(vCheckpoints) / sizeof(vCheckpoints[0]);

    return std::vector<ConsensusCheckpoint>(vCheckpoints, vCheckpoints + nSize);
}

/**
 * Constructor for mainnet consensus parameters.
 */
CMainConsensusParams::CMainConsensusParams()
{
    GENESIS_BLOCK = 1171000;
    // Notice range for feature activations:
    MIN_ACTIVATION_BLOCKS = 2048;  // ~2 weeks
    MAX_ACTIVATION_BLOCKS = 12288; // ~12 weeks
    // Script related:
    PUBKEYHASH_BLOCK = 0;
    SCRIPTHASH_BLOCK = 0;
    NULLDATA_BLOCK = 0;
    // Transaction restrictions:
    MSC_ALERT_BLOCK = 0;
    MSC_SEND_BLOCK = 9999999;
    MSC_SP_BLOCK = 9999999;
    MSC_MANUALSP_BLOCK = 9999999;
    MSC_SEND_ALL_BLOCK = 9999999;
    ///////////////////////////////
    /** New things for Contract */
    MSC_CONTRACTDEX_BLOCK = 999999;
    ///////////////////////////////
}

/**
 * Constructor for testnet consensus parameters.
 */
CTestNetConsensusParams::CTestNetConsensusParams()
{
    GENESIS_BLOCK = 620000;
    // Notice range for feature activations:
    MIN_ACTIVATION_BLOCKS = 0;
    MAX_ACTIVATION_BLOCKS = 999999;
    // Script related:
    PUBKEYHASH_BLOCK = 0;
    SCRIPTHASH_BLOCK = 0;
    NULLDATA_BLOCK = 0;
    // Transaction restrictions:
    MSC_ALERT_BLOCK = 0;
    MSC_SEND_BLOCK = 0;
    MSC_SP_BLOCK = 0;
    MSC_MANUALSP_BLOCK = 0;
    MSC_SEND_ALL_BLOCK = 0;
    ///////////////////////////////
    /** New things for Contract */
    MSC_CONTRACTDEX_BLOCK = 0;
    ///////////////////////////////
}

/**
 * Constructor for regtest consensus parameters.
 */
CRegTestConsensusParams::CRegTestConsensusParams()
{
    GENESIS_BLOCK = 0;
    // Notice range for feature activations:
    MIN_ACTIVATION_BLOCKS = 5;
    MAX_ACTIVATION_BLOCKS = 10;
    // Script related:
    PUBKEYHASH_BLOCK = 0;
    SCRIPTHASH_BLOCK = 0;
    NULLDATA_BLOCK = 0;
    // Transaction restrictions:
    MSC_ALERT_BLOCK = 0;
    MSC_SEND_BLOCK = 0;
    MSC_SP_BLOCK = 0;
    MSC_MANUALSP_BLOCK = 0;
    MSC_SEND_ALL_BLOCK = 0;
    ///////////////////////////////
    /** New things for Contract */
    MSC_CONTRACTDEX_BLOCK = 0;
    ///////////////////////////////
}

//! Consensus parameters for mainnet
static CMainConsensusParams mainConsensusParams;
//! Consensus parameters for testnet
static CTestNetConsensusParams testNetConsensusParams;
//! Consensus parameters for regtest mode
static CRegTestConsensusParams regTestConsensusParams;

/**
 * Returns consensus parameters for the given network.
 */
CConsensusParams& ConsensusParams(const std::string& network)
{
    if (network == "main") {
        return mainConsensusParams;
    }
    if (network == "test") {
        return testNetConsensusParams;
    }
    if (network == "regtest") {
        return regTestConsensusParams;
    }
    // Fallback:
    return mainConsensusParams;
}

/**
 * Returns currently active consensus parameter.
 */
const CConsensusParams& ConsensusParams()
{
    const std::string& network = Params().NetworkIDString();

    return ConsensusParams(network);
}

/**
 * Returns currently active mutable consensus parameter.
 */
CConsensusParams& MutableConsensusParams()
{
    const std::string& network = Params().NetworkIDString();

    return ConsensusParams(network);
}

/**
 * Resets consensus paramters.
 */
void ResetConsensusParams()
{
    mainConsensusParams = CMainConsensusParams();
    testNetConsensusParams = CTestNetConsensusParams();
    regTestConsensusParams = CRegTestConsensusParams();
}

/**
 * Checks, if the script type is allowed as input.
 */
bool IsAllowedInputType(int whichType, int nBlock)
{
    const CConsensusParams& params = ConsensusParams();

    switch (whichType)
    {
        case TX_PUBKEYHASH:
            return (params.PUBKEYHASH_BLOCK <= nBlock);

        case TX_SCRIPTHASH:
            return (params.SCRIPTHASH_BLOCK <= nBlock);
    }

    return false;
}

/**
 * Checks, if the script type qualifies as output.
 */
bool IsAllowedOutputType(int whichType, int nBlock)
{
    const CConsensusParams& params = ConsensusParams();

    switch (whichType)
    {
        case TX_PUBKEYHASH:
            return (params.PUBKEYHASH_BLOCK <= nBlock);

        case TX_SCRIPTHASH:
            return (params.SCRIPTHASH_BLOCK <= nBlock);

        case TX_NULL_DATA:
            return (params.NULLDATA_BLOCK <= nBlock);
    }

    return false;
}

/**
 * Activates a feature at a specific block height, authorization has already been validated.
 *
 * Note: Feature activations are consensus breaking.  It is not permitted to activate a feature within
 *       the next 2048 blocks (roughly 2 weeks), nor is it permitted to activate a feature further out
 *       than 12288 blocks (roughly 12 weeks) to ensure sufficient notice.
 *       This does not apply for activation during initialization (where loadingActivations is set true).
 */
bool ActivateFeature(uint16_t featureId, int activationBlock, uint32_t minClientVersion, int transactionBlock)
{
    PrintToLog("Feature activation requested (ID %d to go active as of block: %d)\n", featureId, activationBlock);

    const CConsensusParams& params = ConsensusParams();

    // check activation block is allowed
    if ((activationBlock < (transactionBlock + params.MIN_ACTIVATION_BLOCKS)) ||
        (activationBlock > (transactionBlock + params.MAX_ACTIVATION_BLOCKS))) {
            PrintToLog("Feature activation of ID %d refused due to notice checks\n", featureId);
            return false;
    }

    // check whether the feature is already active
    if (IsFeatureActivated(featureId, transactionBlock)) {
        PrintToLog("Feature activation of ID %d refused as the feature is already live\n", featureId);
        return false;
    }

    // check feature is recognized and activation is successful
    std::string featureName = GetFeatureName(featureId);
    bool supported = OMNICORE_VERSION >= minClientVersion;
    switch (featureId) {
        // No currently outstanding features
    }

    PrintToLog("Feature activation of ID %d processed. %s will be enabled at block %d.\n", featureId, featureName, activationBlock);
    AddPendingActivation(featureId, activationBlock, minClientVersion, featureName);

    if (!supported) {
        PrintToLog("WARNING!!! AS OF BLOCK %d THIS CLIENT WILL BE OUT OF CONSENSUS AND WILL AUTOMATICALLY SHUTDOWN.\n", activationBlock);
        std::string alertText = strprintf("Your client must be updated and will shutdown at block %d (unsupported feature %d ('%s') activated)\n",
                                          activationBlock, featureId, featureName);
        AddAlert("omnicore", ALERT_BLOCK_EXPIRY, activationBlock, alertText);
        //TODO AlertNotify(alertText);
    }

    return true;
}

/**
 * Deactivates a feature immediately, authorization has already been validated.
 *
 * Note: There is no notice period for feature deactivation as:
 *       # It is reserved for emergency use in the event an exploit is found
 *       # No client upgrade is required
 *       # No action is required by users
 */
bool DeactivateFeature(uint16_t featureId, int transactionBlock)
{
    PrintToLog("Immediate feature deactivation requested (ID %d)\n", featureId);

    if (!IsFeatureActivated(featureId, transactionBlock)) {
        PrintToLog("Feature deactivation of ID %d refused as the feature is not yet live\n", featureId);
        return false;
    }

    std::string featureName = GetFeatureName(featureId);
    switch (featureId) {
        // No currently outstanding features
        default:
            return false;
        break;
    }

    PrintToLog("Feature deactivation of ID %d processed. %s has been disabled.\n", featureId, featureName);

    std::string alertText = strprintf("An emergency deactivation of feature ID %d (%s) has occurred.", featureId, featureName);
    AddAlert("omnicore", ALERT_BLOCK_EXPIRY, transactionBlock + 1024, alertText);
    // TODO AlertNotify(alertText);

    return true;
}

/**
 * Returns the display name of a feature ID
 */
std::string GetFeatureName(uint16_t featureId)
{
    switch (featureId) {
        // No currently outstanding features
        default: return "Unknown feature";
    }
}

/**
 * Checks, whether a feature is activated at the given block.
 */
bool IsFeatureActivated(uint16_t featureId, int transactionBlock)
{
    // const CConsensusParams& params = ConsensusParams();
    int activationBlock = std::numeric_limits<int>::max();

    switch (featureId) {
        // No currently outstanding features
        default:
            return false;
    }

    return (transactionBlock >= activationBlock);
}

/**
 * Checks, if the transaction type and version is supported and enabled.
 *
 * In the test ecosystem, transactions, which are known to the client are allowed
 * without height restriction.
 *
 * Certain transactions use a property identifier of 0 (= BTC) as wildcard, which
 * must explicitly be allowed.
 */
bool IsTransactionTypeAllowed(int txBlock, uint32_t txProperty, uint16_t txType, uint16_t version)
{
    const std::vector<TransactionRestriction>& vTxRestrictions = ConsensusParams().GetRestrictions();

    for (std::vector<TransactionRestriction>::const_iterator it = vTxRestrictions.begin(); it != vTxRestrictions.end(); ++it)
    {
        const TransactionRestriction& entry = *it;
        if (entry.txType != txType || entry.txVersion != version) {
            continue;
        }
        // a property identifier of 0 (= BTC) may be used as wildcard
        if (OMNI_PROPERTY_BTC == txProperty && !entry.allowWildcard) {
            continue;
        }
        // transactions are not restricted in the test ecosystem
        if (isTestEcosystemProperty(txProperty)) {
            return true;
        }
        if (txBlock >= entry.activationBlock) {
            return true;
        }
    }

    return false;
}

/**
 * Compares a supplied block, block hash and consensus hash against a hardcoded list of checkpoints.
 */
bool VerifyCheckpoint(int block, const uint256& blockHash)
{
    // optimization; we only checkpoint every 10,000 blocks - skip any further work if block not a multiple of 10K
    if (block % 10000 != 0) return true;

    const std::vector<ConsensusCheckpoint>& vCheckpoints = ConsensusParams().GetCheckpoints();

    for (std::vector<ConsensusCheckpoint>::const_iterator it = vCheckpoints.begin(); it != vCheckpoints.end(); ++it) {
        const ConsensusCheckpoint& checkpoint = *it;
        if (block != checkpoint.blockHeight) {
            continue;
        }

        if (blockHash != checkpoint.blockHash) {
            PrintToLog("%s(): block hash mismatch - expected %s, received %s\n", __func__, checkpoint.blockHash.GetHex(), blockHash.GetHex());
            return false;
        }

        // only verify if there is a checkpoint to verify against
        uint256 consensusHash = GetConsensusHash();
        if (consensusHash != checkpoint.consensusHash) {
            PrintToLog("%s(): consensus hash mismatch - expected %s, received %s\n", __func__, checkpoint.consensusHash.GetHex(), consensusHash.GetHex());
            return false;
        } else {
            break;
        }
    }

    // either checkpoint matched or we don't have a checkpoint for this block
    return true;
}

} // namespace mastercore
