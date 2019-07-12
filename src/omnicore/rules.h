#ifndef OMNICORE_RULES_H
#define OMNICORE_RULES_H

#include "uint256.h"

#include <stdint.h>
#include <string>
#include <vector>

namespace mastercore
{
//! Feature identifier placeholder
const uint16_t FEATURE_NA = 9999;
const uint16_t FEATURE_DEXMATH = 5;
//! Feature identifier to enable the fee cache and strip 0.05% fees from non-Omni pairs
const uint16_t FEATURE_FEES = 9;
//! Feature identifier to enable Send All transactions
const uint16_t FEATURE_CONTRACTDEX = 11;
/** A structure to represent transaction restrictions*/

//! When (propertyTotalTokens / OMNI_FEE_THRESHOLD) is reached fee distribution will occur
const int64_t OMNI_FEE_THRESHOLD = 100000; // 0.001%

struct TransactionRestriction
{
    //! Transaction type
    uint16_t txType;
    //! Transaction version
    uint16_t txVersion;
    //! Whether the property identifier can be 0 (= BTC)
    bool allowWildcard;
    //! Block after which the feature or transaction is enabled
    int activationBlock;
};

/** A structure to represent a verification checkpoint.
 */
struct ConsensusCheckpoint
{
    int blockHeight;
    uint256 blockHash;
    uint256 consensusHash;
};

// TODO: rename allcaps variable names
// TODO: remove remaining global heights
// TODO: add Exodus addresses to params

/** Base class for consensus parameters.
 */
class CConsensusParams
{
public:
    //! Live block of Omni Layer Lite
    int GENESIS_BLOCK;

    //! Minimum number of blocks to use for notice rules on activation
    int MIN_ACTIVATION_BLOCKS;
    //! Maximum number of blocks to use for notice rules on activation
    int MAX_ACTIVATION_BLOCKS;

    //! Block to enable pay-to-pubkey-hash support
    int PUBKEYHASH_BLOCK;
    //! Block to enable pay-to-script-hash support
    int SCRIPTHASH_BLOCK;
    //! Block to enable OP_RETURN based encoding
    int NULLDATA_BLOCK;

    //! Block to enable alerts and notifications
    int MSC_ALERT_BLOCK;
    //! Block to enable simple send transactions
    int MSC_SEND_BLOCK;
    //! Block to enable smart property transactions
    int MSC_SP_BLOCK;
    //! Block to enable managed properties
    int MSC_MANUALSP_BLOCK;
    //! Block to enable "send all" transactions
    int MSC_SEND_ALL_BLOCK;
    ////////////////////////////////////
    /** New things for Contract: ! Block to enable MetaDEx transactions */
    int MSC_CONTRACTDEX_BLOCK;
    int MSC_VESTING_BLOCK;
    int MSC_NODE_REWARD;
    ////////////////////////////////////

    /** Returns a mapping of transaction types, and the blocks at which they are enabled. */
    virtual std::vector<TransactionRestriction> GetRestrictions() const;

    /** Returns an empty vector of consensus checkpoints. */
    virtual std::vector<ConsensusCheckpoint> GetCheckpoints() const;

    /** Destructor. */
    virtual ~CConsensusParams() {}

protected:
    /** Constructor, only to be called from derived classes. */
    CConsensusParams() {}
};

/** Consensus parameters for mainnet.
 */
class CMainConsensusParams: public CConsensusParams
{
public:
    /** Constructor for mainnet consensus parameters. */
    CMainConsensusParams();
    /** Destructor. */
    virtual ~CMainConsensusParams() {}

    /** Returns consensus checkpoints for mainnet, used to verify transaction processing. */
    virtual std::vector<ConsensusCheckpoint> GetCheckpoints() const;
};

/** Consensus parameters for testnet.
 */
class CTestNetConsensusParams: public CConsensusParams
{
public:
    /** Constructor for testnet consensus parameters. */
    CTestNetConsensusParams();
    /** Destructor. */
    virtual ~CTestNetConsensusParams() {}
};

/** Consensus parameters for regtest mode.
 */
class CRegTestConsensusParams: public CConsensusParams
{
public:
    /** Constructor for regtest consensus parameters. */
    CRegTestConsensusParams();
    /** Destructor. */
    virtual ~CRegTestConsensusParams() {}
};

/** Returns consensus parameters for the given network. */
CConsensusParams& ConsensusParams(const std::string& network);
/** Returns currently active consensus parameter. */
const CConsensusParams& ConsensusParams();
/** Returns currently active mutable consensus parameter. */
CConsensusParams& MutableConsensusParams();
/** Resets consensus paramters. */
void ResetConsensusParams();


/** Gets the display name for a feature ID */
std::string GetFeatureName(uint16_t featureId);
/** Activates a feature at a specific block height. */
bool ActivateFeature(uint16_t featureId, int activationBlock, uint32_t minClientVersion, int transactionBlock);
/** Deactivates a feature immediately, authorization has already been validated. */
bool DeactivateFeature(uint16_t featureId, int transactionBlock);
/** Checks, whether a feature is activated at the given block. */
bool IsFeatureActivated(uint16_t featureId, int transactionBlock);
/** Checks, if the script type is allowed as input. */
bool IsAllowedInputType(int whichType, int nBlock);
/** Checks, if the script type qualifies as output. */
bool IsAllowedOutputType(int whichType, int nBlock);
/** Checks, if the transaction type and version is supported and enabled. */
bool IsTransactionTypeAllowed(int txBlock, uint32_t txProperty, uint16_t txType, uint16_t version);

/** Compares a supplied block, block hash and consensus hash against a hardcoded list of checkpoints. */
bool VerifyCheckpoint(int block, const uint256& blockHash);
}

#endif // OMNICORE_RULES_H
