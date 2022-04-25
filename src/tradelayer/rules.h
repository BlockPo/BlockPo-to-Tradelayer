#ifndef TRADELAYER_RULES_H
#define TRADELAYER_RULES_H

#include <uint256.h>

#include <stdint.h>
#include <string>
#include <vector>

namespace mastercore
{
//! Feature identifier placeholder
const uint16_t FEATURE_VESTING                  = 1;
const uint16_t FEATURE_KYC                      = 2;
const uint16_t FEATURE_DEX_SELL                 = 3;
const uint16_t FEATURE_DEX_BUY                  = 4;
const uint16_t FEATURE_METADEX                  = 5;
const uint16_t FEATURE_TRADECHANNELS_TOKENS     = 6; //
const uint16_t FEATURE_TRADECHANNELS_CONTRACTS  = 7; //It's important to note that this enables any contract to trade in a channel, but if said contracts aren't activated, still invalid
const uint16_t FEATURE_FIXED                    = 8; //This should include simple sends, send all
const uint16_t FEATURE_MANAGED                  = 9;
const uint16_t FEATURE_NODE_REWARD              = 10;
const uint16_t FEATURE_SEND_MANY                = 11; //enables multiple reference outputs to send different amounts of a property to multiple addresses, useful for fee rebate

const uint16_t FEATURE_CONTRACTDEX              = 12; //Enables native perps/futures for ALL/LTC and LTC/USD, EUR, JPY, CNY, liquidity reward built-in
const uint16_t FEATURE_CONTRACTDEX_ORACLES      = 13; //Enables just oracle contracts with native property id's pre-defined, can be done first
const uint16_t FEATURE_TRADECHANNELS_OPTIONS    = 14;  //we're Trade Channels only for options because, the memory bloat having a crappy orderbook system, bleh
const uint16_t FEATURE_DISPENSERVAULTS          = 15; //One can deposit e.g. sLTC to a time-locked cold address and have the yield remit to a warm address
const uint16_t FEATURE_PAYMENTBATCHING          = 16; //will research composability and scaling of pre-and-post Schnorr use of Taproot or other aggregation methods
const uint16_t FEATURE_MARGINLENDING            = 17; //post tokens as collateral to pay to borrow a lesser value of some other token (e.g. for shorting/spot lvg. long)
const uint16_t FEATURE_INTEROP_CTV              = 18; //propose R&D partnership with CTV author, possible lending mechanism of BTC for token collateral
const uint16_t FEATURE_INTEROP_LIGHTNING        = 19; //OmniBolt style coloring of outputs to cheque in an LN model but carrying TradeLayer tokens
const uint16_t FEATURE_INTEROP_REPO             = 20; //Our dream OP_Code to curry favor for activation by proving this economic model for the greater Bitcoinity
const uint16_t FEATURE_INTEROP_SIDECHAINS       = 21; //Generic and specialized sidechain models can use TradeLayer tokens as bonding collateral and for HFT or payments
const uint16_t FEATURE_INTEROP_CROSSCHAINATOMICSWAPS = 22; //HTLC token-for-token trading, ought to be implicitly composable with BTC/LTC atomic swaps
const uint16_t FEATURE_GRAPHDEFAULTSWAPS        = 23; //activates GDS perpetual swaps underpinning ALL/LTC and LTC/USD, plus a new kind of meta-contract on any derivative
const uint16_t FEATURE_INTERESTRATESWAPS        = 24; //activates term native IR Swaps for ALL/LTC and LTC/USD perpetuals, plus a new kind of oracle contract referring to oracle perp. swaps
const uint16_t FEATURE_MINERFEECONTRACTS        = 25; //activates term native futures for on-chain miner fee averages
const uint16_t FEATURE_MASSPAYMENT              = 26; //unencumbered version of sent-to-owners, can pay n units of property A to all holders of propery B (which can also be A)
const uint16_t FEATURE_HEDGEDCURRENCY           = 27; //Enables 1x short positions against ALL to mint sLTC, and 1x shorts against sLTC or rLTC (if activated) to mint USDL, EURL, JPYL, CNYL
const uint16_t FEATURE_PEGGED_CURRENCY          = 28; // Pegged Currency

//This is the entire roadmap. If we missed anything, well, clearly we tried not to.

struct TransactionRestriction
{
    //! Transaction type
    uint16_t txType;
    //! Transaction version
    uint16_t txVersion;
    //! Whether the property identifier can be 0 (= LTC)
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

/** Base class for consensus parameters.
 */
class CConsensusParams
{
public:
    //! Live block of Trade Layer
    int GENESIS_BLOCK;

    int MULTISIG_BLOCK;

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
    //! Block to enable send to many transactions
    int MSC_SEND_MANY_BLOCK;
    //! Block to enable smart property transactions
    int MSC_SP_BLOCK;
    //! Block to enable managed properties
    int MSC_MANUALSP_BLOCK;
    //! Block to enable "send all" transactions
    int MSC_SEND_ALL_BLOCK;

    //! Block in which we reach decay rate
    int INFLEXION_BLOCK;

    int MSC_VESTING_CREATION_BLOCK;
    int MSC_VESTING_BLOCK;
    int MSC_PEGGED_CURRENCY_BLOCK;
    int MSC_KYC_BLOCK;
    int MSC_METADEX_BLOCK;
    int MSC_DEXSELL_BLOCK;
    int MSC_DEXBUY_BLOCK;
    int MSC_CONTRACTDEX_BLOCK;
    int MSC_CONTRACTDEX_ORACLES_BLOCK;
    int MSC_NODE_REWARD_BLOCK;
    int MSC_TRADECHANNEL_TOKENS_BLOCK;
    int MSC_TRADECHANNEL_CONTRACTS_BLOCK;
    int MSC_TRADECHANNEL_OPTIONS_BLOCK;
    int MSC_DISPENSERVAULTS_BLOCK;
    int MSC_PAYMENTBATCHING_BLOCK;
    int MSC_MARGINLENDING_BLOCK;
    int MSC_INTEROP_CTV_BLOCK;
    int MSC_INTEROP_LIGHTNING_BLOCK;
    int MSC_INTEROP_REPO_BLOCK;
    int MSC_INTEROP_SIDECHAINS_BLOCK;
    int MSC_INTEROP_CROSSCHAINATOMICSWAPS_BLOCK;
    int MSC_GRAPHDEFAULTSWAPS_BLOCK;
    int MSC_INTERESTRATESWAPS_BLOCK;
    int MSC_MINERFEECONTRACTS_BLOCK;
    int MSC_MASSPAYMENT_BLOCK;
    int MSC_MULTISEND_BLOCK;
    int MSC_HEDGEDCURRENCY_BLOCK;
    int MSC_SEND_DONATION_BLOCK;

    /* Vesting Tokens*/
    int ONE_YEAR;


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
bool IsTransactionTypeAllowed(int txBlock, uint16_t txType, uint16_t version);

/** Compares a supplied block, block hash and consensus hash against a hardcoded list of checkpoints. */
bool VerifyCheckpoint(int block, const uint256& blockHash);
}

#endif // TRADELAYER_RULES_H
