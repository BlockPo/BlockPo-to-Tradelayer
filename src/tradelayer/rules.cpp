/**
 * @file rules.cpp
 *
 * This file contains consensus rules and restrictions.
 */

#include <tradelayer/rules.h>

#include <tradelayer/activation.h>
#include <tradelayer/consensushash.h>
#include <tradelayer/log.h>
#include <tradelayer/notifications.h>
#include <tradelayer/tradelayer.h>
#include <tradelayer/utilsbitcoin.h>
#include <tradelayer/version.h>

#include <ui_interface.h>

#include <chainparams.h>
#include <script/standard.h>
#include <uint256.h>
#include <validation.h>

#include <limits>
#include <stdint.h>
#include <string>
#include <vector>

namespace mastercore
{

/**
 * Return transaction activation block, & restrictions.
 */
std::vector<TransactionRestriction> CConsensusParams::GetRestrictions() const
{
    std::vector<TransactionRestriction> v;

    // Parameters for the restriction.
    //
    // 1. Transaction type.
    // 2. Version.
    // 3. Allow 0.
    // 4. Activation block.
    //----------------------------------------------------------------//

    v.push_back( { TL_MESSAGE_TYPE_ALERT,                          0xFFFF,            true, MSC_ALERT_BLOCK } );
    v.push_back( { TL_MESSAGE_TYPE_ACTIVATION,                     0xFFFF,            true, MSC_ALERT_BLOCK } );
    v.push_back( { TL_MESSAGE_TYPE_DEACTIVATION,                   0xFFFF,            true, MSC_ALERT_BLOCK } );
    v.push_back( { MSC_TYPE_SIMPLE_SEND,                           MP_TX_PKT_V0,      true, MSC_SEND_BLOCK } );
    v.push_back( { MSC_TYPE_SEND_MANY,                             MP_TX_PKT_V0,      true, MSC_SEND_MANY_BLOCK } );
    v.push_back( { MSC_TYPE_CREATE_PROPERTY_FIXED,                 MP_TX_PKT_V0,      true, MSC_SP_BLOCK } );
    v.push_back( { MSC_TYPE_CREATE_PROPERTY_MANUAL,                MP_TX_PKT_V0,      true, MSC_MANUALSP_BLOCK } );
    v.push_back( { MSC_TYPE_GRANT_PROPERTY_TOKENS,                 MP_TX_PKT_V0,      true, MSC_MANUALSP_BLOCK } );
    v.push_back( { MSC_TYPE_REVOKE_PROPERTY_TOKENS,                MP_TX_PKT_V0,      true, MSC_MANUALSP_BLOCK });
    v.push_back( { MSC_TYPE_CHANGE_ISSUER_ADDRESS,                 MP_TX_PKT_V0,      true, MSC_MANUALSP_BLOCK });
    v.push_back( { MSC_TYPE_CREATE_CONTRACT,                       MP_TX_PKT_V0,      true, MSC_CONTRACTDEX_BLOCK } );
    v.push_back( { MSC_TYPE_CONTRACTDEX_TRADE,                     MP_TX_PKT_V0,      true, MSC_CONTRACTDEX_BLOCK } );
    v.push_back( { MSC_TYPE_CONTRACTDEX_CANCEL_ECOSYSTEM,          MP_TX_PKT_V0,      true, MSC_CONTRACTDEX_BLOCK } );
    v.push_back( { MSC_TYPE_SEND_ALL,                              MP_TX_PKT_V0,      true, MSC_SEND_ALL_BLOCK } );
    v.push_back( { MSC_TYPE_CONTRACTDEX_CLOSE_POSITION,            MP_TX_PKT_V0,      true, MSC_CONTRACTDEX_BLOCK } );
    v.push_back( { MSC_TYPE_CONTRACTDEX_CANCEL_ORDERS_BY_BLOCK,    MP_TX_PKT_V0,      true, MSC_CONTRACTDEX_BLOCK } );
    v.push_back( { MSC_TYPE_METADEX_TRADE,                         MP_TX_PKT_V0,      true, MSC_METADEX_BLOCK } );
    v.push_back( { MSC_TYPE_DEX_SELL_OFFER,                        MP_TX_PKT_V1,      true, MSC_DEXSELL_BLOCK } );
    v.push_back( { MSC_TYPE_DEX_BUY_OFFER,                         MP_TX_PKT_V0,      true, MSC_DEXBUY_BLOCK } );
    v.push_back( { MSC_TYPE_ACCEPT_OFFER_BTC,                      MP_TX_PKT_V0,      true, MSC_DEXSELL_BLOCK } );
    v.push_back( { MSC_TYPE_DEX_PAYMENT,                           MP_TX_PKT_V0,      true, MSC_DEXSELL_BLOCK } );
    v.push_back( { MSC_TYPE_CONTRACTDEX_TRADE,                     MP_TX_PKT_V0,      true, MSC_CONTRACTDEX_BLOCK } );
    v.push_back( { MSC_TYPE_CONTRACTDEX_CANCEL_ECOSYSTEM,          MP_TX_PKT_V0,      true, MSC_CONTRACTDEX_BLOCK } );
    v.push_back( { MSC_TYPE_CREATE_CONTRACT,                       MP_TX_PKT_V0,      true, MSC_CONTRACTDEX_BLOCK } );
    v.push_back( { MSC_TYPE_PEGGED_CURRENCY,                       MP_TX_PKT_V0,      true, MSC_CONTRACTDEX_BLOCK } );
    v.push_back( { MSC_TYPE_REDEMPTION_PEGGED,                     MP_TX_PKT_V0,      true, MSC_PEGGED_CURRENCY_BLOCK } );
    v.push_back( { MSC_TYPE_SEND_PEGGED_CURRENCY,                  MP_TX_PKT_V0,      true, MSC_PEGGED_CURRENCY_BLOCK } );
    v.push_back( { MSC_TYPE_CONTRACTDEX_CLOSE_POSITION,            MP_TX_PKT_V0,      true, MSC_PEGGED_CURRENCY_BLOCK } );
    v.push_back( { MSC_TYPE_CONTRACTDEX_CANCEL_ORDERS_BY_BLOCK,    MP_TX_PKT_V0,      true, MSC_CONTRACTDEX_BLOCK } );
    v.push_back( { MSC_TYPE_CHANGE_ORACLE_REF,                     MP_TX_PKT_V0,      true, MSC_CONTRACTDEX_ORACLES_BLOCK } );
    v.push_back( { MSC_TYPE_SET_ORACLE,                            MP_TX_PKT_V0,      true, MSC_CONTRACTDEX_ORACLES_BLOCK } );
    v.push_back( { MSC_TYPE_CLOSE_ORACLE,                          MP_TX_PKT_V0,      true, MSC_CONTRACTDEX_ORACLES_BLOCK } );
    v.push_back( { MSC_TYPE_ORACLE_BACKUP,                         MP_TX_PKT_V0,      true, MSC_CONTRACTDEX_ORACLES_BLOCK } );
    v.push_back( { MSC_TYPE_COMMIT_CHANNEL,                        MP_TX_PKT_V0,      true, MSC_TRADECHANNEL_TOKENS_BLOCK } );
    v.push_back( { MSC_TYPE_WITHDRAWAL_FROM_CHANNEL,               MP_TX_PKT_V0,      true, MSC_TRADECHANNEL_TOKENS_BLOCK } );
    v.push_back( { MSC_TYPE_INSTANT_TRADE,                         MP_TX_PKT_V0,      true, MSC_TRADECHANNEL_TOKENS_BLOCK } );
    v.push_back( { MSC_TYPE_TRANSFER,                              MP_TX_PKT_V0,      true, MSC_TRADECHANNEL_TOKENS_BLOCK } );
    v.push_back( { MSC_TYPE_CONTRACT_INSTANT,                      MP_TX_PKT_V0,      true, MSC_TRADECHANNEL_CONTRACTS_BLOCK } );
    v.push_back( { MSC_TYPE_NEW_ID_REGISTRATION,                   MP_TX_PKT_V0,      true, MSC_KYC_BLOCK } );
    v.push_back( { MSC_TYPE_UPDATE_ID_REGISTRATION,                MP_TX_PKT_V0,      true, MSC_KYC_BLOCK } );
    v.push_back( { MSC_TYPE_CREATE_ORACLE_CONTRACT,                MP_TX_PKT_V0,      true, MSC_CONTRACTDEX_ORACLES_BLOCK } );
    v.push_back( { MSC_TYPE_SEND_VESTING,                          MP_TX_PKT_V0,      true, MSC_VESTING_BLOCK } );
    v.push_back( { MSC_TYPE_ATTESTATION,                           MP_TX_PKT_V0,      true, MSC_KYC_BLOCK } );
    v.push_back( { MSC_TYPE_REVOKE_ATTESTATION,                    MP_TX_PKT_V0,      true, MSC_KYC_BLOCK } );
    v.push_back( { MSC_TYPE_CONTRACTDEX_CANCEL,                    MP_TX_PKT_V0,      true, MSC_CONTRACTDEX_BLOCK } );
    v.push_back( { MSC_TYPE_INSTANT_LTC_TRADE,                     MP_TX_PKT_V0,      true, MSC_TRADECHANNEL_TOKENS_BLOCK } );
    v.push_back( { MSC_TYPE_METADEX_CANCEL,                        MP_TX_PKT_V0,      true, MSC_METADEX_BLOCK } );
    v.push_back( { MSC_TYPE_METADEX_CANCEL_BY_PAIR,                MP_TX_PKT_V0,      true, MSC_METADEX_BLOCK } );
    v.push_back( { MSC_TYPE_METADEX_CANCEL_ALL,                    MP_TX_PKT_V0,      true, MSC_METADEX_BLOCK } );
    v.push_back( { MSC_TYPE_METADEX_CANCEL_BY_PRICE,               MP_TX_PKT_V0,      true, MSC_METADEX_BLOCK } );
    v.push_back( { MSC_TYPE_CLOSE_CHANNEL,                         MP_TX_PKT_V0,      true, MSC_TRADECHANNEL_TOKENS_BLOCK } );
    v.push_back( { MSC_TYPE_SUBMIT_NODE_ADDRESS,                   MP_TX_PKT_V0,      true, MSC_NODE_REWARD_BLOCK } );
    v.push_back( { MSC_TYPE_CLAIM_NODE_REWARD,                     MP_TX_PKT_V0,      true, MSC_NODE_REWARD_BLOCK } );
    v.push_back( { MSC_TYPE_SEND_DONATION,                         MP_TX_PKT_V0,      true, MSC_SEND_DONATION_BLOCK } );
    //---

    return v;
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
    GENESIS_BLOCK = 2172315;
    // Notice range for feature activations:
    MIN_ACTIVATION_BLOCKS = 0;  // ~2 weeks
    MAX_ACTIVATION_BLOCKS = 99999999; // ~12 weeks
    // Script related:
    PUBKEYHASH_BLOCK = 0;
    SCRIPTHASH_BLOCK = 0;
    NULLDATA_BLOCK = 0;
    MULTISIG_BLOCK = 0;
    // Transaction restrictions:
    MSC_ALERT_BLOCK = 2172315;
    MSC_SEND_BLOCK = 2172315;
    MSC_SEND_MANY_BLOCK = 2172315;
    MSC_SP_BLOCK = 99999999;
    MSC_MANUALSP_BLOCK = 99999999;
    MSC_SEND_ALL_BLOCK = 99999999;
    MSC_CONTRACTDEX_BLOCK = 99999999;
    MSC_CONTRACTDEX_ORACLES_BLOCK = 99999999;
    MSC_VESTING_BLOCK = 99999999;
    MSC_VESTING_CREATION_BLOCK = 2172315;
    MSC_NODE_REWARD_BLOCK = 99999999;
    MSC_KYC_BLOCK = 99999999;
    MSC_DEXSELL_BLOCK = 99999999;
    MSC_DEXBUY_BLOCK = 99999999;
    MSC_METADEX_BLOCK = 99999999;
    MSC_TRADECHANNEL_TOKENS_BLOCK = 99999999;
    MSC_TRADECHANNEL_CONTRACTS_BLOCK = 99999999;

    MSC_TRADECHANNEL_OPTIONS_BLOCK = 99999999;
    MSC_DISPENSERVAULTS_BLOCK = 99999999;
    MSC_PAYMENTBATCHING_BLOCK = 99999999;
    MSC_MARGINLENDING_BLOCK = 99999999;
    MSC_INTEROP_CTV_BLOCK = 99999999;
    MSC_INTEROP_LIGHTNING_BLOCK = 99999999;
    MSC_INTEROP_REPO_BLOCK = 99999999;
    MSC_INTEROP_SIDECHAINS_BLOCK = 99999999;
    MSC_INTEROP_CROSSCHAINATOMICSWAPS_BLOCK = 99999999;
    MSC_GRAPHDEFAULTSWAPS_BLOCK = 99999999;
    MSC_INTERESTRATESWAPS_BLOCK = 99999999;
    MSC_MINERFEECONTRACTS_BLOCK = 99999999;
    MSC_MASSPAYMENT_BLOCK = 99999999;
    MSC_MULTISEND_BLOCK = 99999999;
    MSC_HEDGEDCURRENCY_BLOCK = 99999999;
    MSC_SEND_DONATION_BLOCK = 99999999;
    MSC_PEGGED_CURRENCY_BLOCK = 99999999,
    INFLEXION_BLOCK = 25000;

    ONE_YEAR = 210240;
}

/**
 * Constructor for testnet consensus parameters.
 */
 CTestNetConsensusParams::CTestNetConsensusParams()
 {
     GENESIS_BLOCK = 2121695;
     // Notice range for feature activations:
     MIN_ACTIVATION_BLOCKS = 0;
     MAX_ACTIVATION_BLOCKS = 99999999;
     // Script related:
     PUBKEYHASH_BLOCK = 0;
     SCRIPTHASH_BLOCK = 0;
     NULLDATA_BLOCK = 0;
     MULTISIG_BLOCK = 0;

     // Transaction restrictions:
     MSC_ALERT_BLOCK = 2121695;
     MSC_SEND_BLOCK = 2121695;
     MSC_SEND_MANY_BLOCK = 99999999;
     // MSC_SP_BLOCK = 1491174;
     MSC_SP_BLOCK = 2121695;
     MSC_MANUALSP_BLOCK = 99999999;
     MSC_SEND_ALL_BLOCK = 99999999;
     MSC_CONTRACTDEX_BLOCK = 99999999;
     MSC_CONTRACTDEX_ORACLES_BLOCK = 99999999;
     MSC_VESTING_CREATION_BLOCK = 2121695;
     MSC_VESTING_BLOCK = 99999999;
     MSC_NODE_REWARD_BLOCK = 99999999;
     MSC_KYC_BLOCK = 99999999;
     MSC_DEXSELL_BLOCK = 99999999;
     MSC_DEXBUY_BLOCK = 99999999;
     MSC_METADEX_BLOCK = 99999999;
     MSC_TRADECHANNEL_TOKENS_BLOCK = 99999999;
     MSC_TRADECHANNEL_CONTRACTS_BLOCK = 99999999;

     MSC_TRADECHANNEL_OPTIONS_BLOCK = 99999999;
     MSC_DISPENSERVAULTS_BLOCK = 99999999;
     MSC_PAYMENTBATCHING_BLOCK = 99999999;
     MSC_MARGINLENDING_BLOCK = 99999999;
     MSC_INTEROP_CTV_BLOCK = 99999999;
     MSC_INTEROP_LIGHTNING_BLOCK = 99999999;
     MSC_INTEROP_REPO_BLOCK = 99999999;
     MSC_INTEROP_SIDECHAINS_BLOCK = 99999999;
     MSC_INTEROP_CROSSCHAINATOMICSWAPS_BLOCK = 99999999;
     MSC_GRAPHDEFAULTSWAPS_BLOCK = 99999999;
     MSC_INTERESTRATESWAPS_BLOCK = 99999999;
     MSC_MINERFEECONTRACTS_BLOCK = 99999999;
     MSC_MASSPAYMENT_BLOCK = 99999999;
     MSC_MULTISEND_BLOCK = 99999999;
     MSC_HEDGEDCURRENCY_BLOCK = 99999999;
     MSC_SEND_DONATION_BLOCK = 99999999;
     MSC_PEGGED_CURRENCY_BLOCK = 99999999,
     INFLEXION_BLOCK = 25000;
     ONE_YEAR = 2650;  // just for testing
 }


/**
 * Constructor for regtest consensus parameters.
 */
CRegTestConsensusParams::CRegTestConsensusParams()
{
    GENESIS_BLOCK = 0;
    // Notice range for feature activations:
    MIN_ACTIVATION_BLOCKS = 5;

    //NOTE: testing
    MAX_ACTIVATION_BLOCKS = 1000;
    // Script related:
    PUBKEYHASH_BLOCK = 0;
    SCRIPTHASH_BLOCK = 0;
    NULLDATA_BLOCK = 0;
    MULTISIG_BLOCK = 0;
    // Transaction restrictions:
    MSC_ALERT_BLOCK = 0;
    MSC_SEND_BLOCK = 0;
    MSC_SEND_MANY_BLOCK = 0;

    /** NOTE: this is the value we are changing
     *  (from 999999 to 400) in test tl_activation.py
     */
    MSC_SP_BLOCK = 200;
    MSC_MANUALSP_BLOCK = 0;
    MSC_SEND_ALL_BLOCK = 0;
    MSC_CONTRACTDEX_BLOCK = 0;
    MSC_CONTRACTDEX_ORACLES_BLOCK = 0;
    MSC_VESTING_CREATION_BLOCK = 100;
    MSC_VESTING_BLOCK = 100;  // just for regtest
    MSC_KYC_BLOCK = 0;
    MSC_DEXSELL_BLOCK = 0;
    MSC_DEXBUY_BLOCK = 0;
    MSC_METADEX_BLOCK = 0;
    MSC_NODE_REWARD_BLOCK = 200;
    MSC_TRADECHANNEL_TOKENS_BLOCK = 0;
    MSC_TRADECHANNEL_CONTRACTS_BLOCK = 0;

    MSC_TRADECHANNEL_OPTIONS_BLOCK = 99999999;
    MSC_DISPENSERVAULTS_BLOCK = 99999999;
    MSC_PAYMENTBATCHING_BLOCK = 99999999;
    MSC_MARGINLENDING_BLOCK = 99999999;
    MSC_INTEROP_CTV_BLOCK = 99999999;
    MSC_INTEROP_LIGHTNING_BLOCK = 99999999;
    MSC_INTEROP_REPO_BLOCK = 99999999;
    MSC_INTEROP_SIDECHAINS_BLOCK = 99999999;
    MSC_INTEROP_CROSSCHAINATOMICSWAPS_BLOCK = 99999999;
    MSC_GRAPHDEFAULTSWAPS_BLOCK = 99999999;
    MSC_INTERESTRATESWAPS_BLOCK = 99999999;
    MSC_MINERFEECONTRACTS_BLOCK = 99999999;
    MSC_MASSPAYMENT_BLOCK = 99999999;
    MSC_MULTISEND_BLOCK = 99999999;
    MSC_HEDGEDCURRENCY_BLOCK = 99999999;
    MSC_SEND_DONATION_BLOCK = 99999999;
    MSC_PEGGED_CURRENCY_BLOCK = 200,
    INFLEXION_BLOCK = 1000;

    ONE_YEAR = 930;

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

        case TX_MULTISIG:
            return (params.MULTISIG_BLOCK <= nBlock);
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

        case TX_MULTISIG:
            return (params.MULTISIG_BLOCK <= nBlock);

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
    if(msc_debug_activate_feature) PrintToLog("Feature activation requested (ID %d to go active as of block: %d)\n", featureId, activationBlock);

    CConsensusParams& params = MutableConsensusParams();

    // check activation block is allowed

    if(msc_debug_activate_feature){
        PrintToLog("%s(): activationBlock %d, transactionBlock + params.MIN_ACTIVATION_BLOCKS : %d\n",__func__, activationBlock, (transactionBlock + params.MIN_ACTIVATION_BLOCKS));
        PrintToLog("%s(): transactionBlock + params.MAX_ACTIVATION_BLOCKS : %d\n",__func__, (transactionBlock + params.MAX_ACTIVATION_BLOCKS));
    }

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

    if(msc_debug_activate_feature) PrintToLog("%s(): TL_VERSION : %d, minClientVersion : %d\n",__func__, TL_VERSION, minClientVersion);

    bool supported = TL_VERSION >= minClientVersion;
    switch (featureId) {
      case FEATURE_VESTING:
          params.MSC_VESTING_BLOCK = activationBlock;
          break;

      case FEATURE_KYC:
          params.MSC_KYC_BLOCK = activationBlock;
          break;

      case FEATURE_DEX_SELL:
          params.MSC_DEXSELL_BLOCK = activationBlock;
          break;

      case FEATURE_DEX_BUY:
          params.MSC_DEXBUY_BLOCK = activationBlock;
          break;

      case FEATURE_METADEX:
          params.MSC_METADEX_BLOCK = activationBlock;
          break;

      case FEATURE_TRADECHANNELS_TOKENS:
          params.MSC_TRADECHANNEL_TOKENS_BLOCK = activationBlock;
          break;

      case FEATURE_TRADECHANNELS_CONTRACTS:
          params.MSC_TRADECHANNEL_CONTRACTS_BLOCK = activationBlock;
          break;

      case FEATURE_FIXED:
          params.MSC_SP_BLOCK = activationBlock;
          break;

      case FEATURE_MANAGED:
          params.MSC_MANUALSP_BLOCK = activationBlock;
          break;

      case FEATURE_NODE_REWARD:
          params.MSC_NODE_REWARD_BLOCK = activationBlock;
          break;

      case FEATURE_CONTRACTDEX:
          params.MSC_CONTRACTDEX_BLOCK = activationBlock;
          break;

      case FEATURE_CONTRACTDEX_ORACLES:
          params.MSC_CONTRACTDEX_ORACLES_BLOCK = activationBlock;
          break;

      case FEATURE_TRADECHANNELS_OPTIONS:
          params.MSC_TRADECHANNEL_OPTIONS_BLOCK = activationBlock;
          break;

      case FEATURE_DISPENSERVAULTS:
          params.MSC_DISPENSERVAULTS_BLOCK = activationBlock;
          break;

      case FEATURE_PAYMENTBATCHING:
          params.MSC_PAYMENTBATCHING_BLOCK = activationBlock;
          break;

      case FEATURE_MARGINLENDING:
          params.MSC_MARGINLENDING_BLOCK = activationBlock;
          break;

      case FEATURE_INTEROP_CTV:
          params.MSC_INTEROP_CTV_BLOCK = activationBlock;
          break;

      case FEATURE_INTEROP_LIGHTNING:
          params.MSC_INTEROP_LIGHTNING_BLOCK = activationBlock;
          break;

      case FEATURE_INTEROP_REPO:
          params.MSC_INTEROP_REPO_BLOCK= activationBlock;
          break;

      case FEATURE_INTEROP_SIDECHAINS:
          params.MSC_INTEROP_SIDECHAINS_BLOCK = activationBlock;
          break;

      case FEATURE_INTEROP_CROSSCHAINATOMICSWAPS:
          params.MSC_INTEROP_CROSSCHAINATOMICSWAPS_BLOCK = activationBlock;
          break;

      case FEATURE_GRAPHDEFAULTSWAPS:
          params.MSC_GRAPHDEFAULTSWAPS_BLOCK = activationBlock;
          break;

      case FEATURE_INTERESTRATESWAPS:
          params.MSC_INTERESTRATESWAPS_BLOCK = activationBlock;
          break;

      case FEATURE_MINERFEECONTRACTS:
          params.MSC_MINERFEECONTRACTS_BLOCK = activationBlock;
          break;

      case FEATURE_MASSPAYMENT:
          params.MSC_MASSPAYMENT_BLOCK = activationBlock;
          break;

      case FEATURE_HEDGEDCURRENCY:
          params.MSC_HEDGEDCURRENCY_BLOCK = activationBlock;
          break;

      case FEATURE_SEND_MANY:
          params.MSC_SEND_MANY_BLOCK = activationBlock;
          break;

      case FEATURE_PEGGED_CURRENCY:
          params.MSC_PEGGED_CURRENCY_BLOCK = activationBlock;
          break;

      default:
           supported = false;
           break;

    }

    if(msc_debug_activate_feature) PrintToLog("Feature activation of ID %d processed. %s will be enabled at block %d.\n", featureId, featureName, activationBlock);
    AddPendingActivation(featureId, activationBlock, minClientVersion, featureName);

    if (!supported) {
        PrintToLog("WARNING!!! AS OF BLOCK %d THIS CLIENT WILL BE OUT OF CONSENSUS AND WILL AUTOMATICALLY SHUTDOWN.\n", activationBlock);
        std::string alertText = strprintf("Your client must be updated and will shutdown at block %d (unsupported feature %d ('%s') activated)\n",
                                          activationBlock, featureId, featureName);
        AddAlert("tradelayer", ALERT_BLOCK_EXPIRY, activationBlock, alertText);
        DoWarning(alertText);
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
    if(msc_debug_deactivate_feature) PrintToLog("Immediate feature deactivation requested (ID %d)\n", featureId);

    if (!IsFeatureActivated(featureId, transactionBlock)) {
        PrintToLog("Feature deactivation of ID %d refused as the feature is not yet live\n", featureId);
        return false;
    }

    std::string featureName = GetFeatureName(featureId);
    switch (featureId) {
      case FEATURE_VESTING:
          MutableConsensusParams().MSC_VESTING_BLOCK = 99999999;
          break;

      case FEATURE_KYC:
          MutableConsensusParams().MSC_KYC_BLOCK = 99999999;
          break;

      case FEATURE_DEX_SELL:
          MutableConsensusParams().MSC_DEXSELL_BLOCK = 99999999;
          break;

      case FEATURE_DEX_BUY:
          MutableConsensusParams().MSC_DEXBUY_BLOCK = 99999999;
          break;

      case FEATURE_METADEX:
          MutableConsensusParams().MSC_METADEX_BLOCK = 99999999;
          break;

      case FEATURE_TRADECHANNELS_TOKENS:
          MutableConsensusParams().MSC_TRADECHANNEL_TOKENS_BLOCK = 99999999;
          break;

      case FEATURE_FIXED:
          MutableConsensusParams().MSC_SP_BLOCK = 99999999;
          break;

      case FEATURE_MANAGED:
          MutableConsensusParams().MSC_MANUALSP_BLOCK = 99999999;
          break;

      case FEATURE_NODE_REWARD:
          MutableConsensusParams().MSC_NODE_REWARD_BLOCK = 99999999;
          break;

      case FEATURE_CONTRACTDEX:
          MutableConsensusParams().MSC_CONTRACTDEX_BLOCK = 99999999;
          break;

      case FEATURE_CONTRACTDEX_ORACLES:
          MutableConsensusParams().MSC_CONTRACTDEX_ORACLES_BLOCK = 99999999;
          break;

      case FEATURE_TRADECHANNELS_OPTIONS:
          MutableConsensusParams().MSC_TRADECHANNEL_OPTIONS_BLOCK = 99999999;
          break;

      case FEATURE_TRADECHANNELS_CONTRACTS:
          MutableConsensusParams().MSC_TRADECHANNEL_CONTRACTS_BLOCK = 99999999;
          break;

      case FEATURE_DISPENSERVAULTS:
          MutableConsensusParams().MSC_DISPENSERVAULTS_BLOCK = 99999999;
          break;

      case FEATURE_PAYMENTBATCHING:
          MutableConsensusParams().MSC_PAYMENTBATCHING_BLOCK = 99999999;
          break;

      case FEATURE_MARGINLENDING:
          MutableConsensusParams().MSC_MARGINLENDING_BLOCK= 99999999;
          break;

      case FEATURE_INTEROP_CTV:
          MutableConsensusParams().MSC_INTEROP_CTV_BLOCK = 99999999;
          break;

      case FEATURE_INTEROP_LIGHTNING:
          MutableConsensusParams().MSC_INTEROP_LIGHTNING_BLOCK = 99999999;
          break;

      case FEATURE_INTEROP_REPO:
          MutableConsensusParams().MSC_INTEROP_REPO_BLOCK = 99999999;
          break;

      case FEATURE_INTEROP_SIDECHAINS:
          MutableConsensusParams().MSC_INTEROP_SIDECHAINS_BLOCK = 99999999;
          break;

      case FEATURE_INTEROP_CROSSCHAINATOMICSWAPS:
          MutableConsensusParams().MSC_INTEROP_CROSSCHAINATOMICSWAPS_BLOCK = 99999999;
          break;

      case FEATURE_GRAPHDEFAULTSWAPS:
          MutableConsensusParams().MSC_GRAPHDEFAULTSWAPS_BLOCK = 99999999;
          break;

      case FEATURE_INTERESTRATESWAPS:
          MutableConsensusParams().MSC_INTERESTRATESWAPS_BLOCK = 99999999;
          break;

      case FEATURE_MINERFEECONTRACTS:
          MutableConsensusParams().MSC_MINERFEECONTRACTS_BLOCK = 99999999;
          break;

      case FEATURE_MASSPAYMENT:
          MutableConsensusParams().MSC_MASSPAYMENT_BLOCK = 99999999;
          break;

      case FEATURE_HEDGEDCURRENCY:
          MutableConsensusParams().MSC_HEDGEDCURRENCY_BLOCK = 99999999;
          break;

      case FEATURE_SEND_MANY:
          MutableConsensusParams().MSC_SEND_MANY_BLOCK = 99999999;
          break;

      case FEATURE_PEGGED_CURRENCY:
          MutableConsensusParams().MSC_PEGGED_CURRENCY_BLOCK = 99999999;
          break;

      default:
            return false;

    }

    if(msc_debug_deactivate_feature) PrintToLog("Feature deactivation of ID %d processed. %s has been disabled.\n", featureId, featureName);

    std::string alertText = strprintf("An emergency deactivation of feature ID %d (%s) has occurred.", featureId, featureName);
    AddAlert("tradelayer", ALERT_BLOCK_EXPIRY, transactionBlock + 1024, alertText);
    DoWarning(alertText);

    return true;
}

/**
 * Returns the display name of a feature ID
 */
std::string GetFeatureName(uint16_t featureId)
{
    switch (featureId) {
        case FEATURE_VESTING: return "Vesting Tokens";
        case FEATURE_KYC: return "Know Your Customer";
        case FEATURE_DEX_SELL: return "Sell Offer in DEx Token Exchange";
        case FEATURE_DEX_BUY: return "Buy Offer in DEx Token Exchange";
        case FEATURE_METADEX: return "Distributed Meta Token Exchange";
        case FEATURE_TRADECHANNELS_TOKENS: return "Trade Channels Token Exchange";
        case FEATURE_FIXED : return "Create Fixed Tokens";
        case FEATURE_MANAGED : return "Create Managed Tokens";
        case FEATURE_NODE_REWARD : return "Node Reward activation";
        case FEATURE_CONTRACTDEX: return "Native Contracts Exchange";
        case FEATURE_CONTRACTDEX_ORACLES: return "Oracle Contracts Exchange";
        case FEATURE_TRADECHANNELS_OPTIONS: return "Trade Channels Option Exchange";
        case FEATURE_TRADECHANNELS_CONTRACTS: return "Trade Channels Contracts Exchange";
        case FEATURE_DISPENSERVAULTS: return "DispersenVaults";
        case FEATURE_PAYMENTBATCHING: return "Payment Batching";
        case FEATURE_MARGINLENDING: return "Margin Lending";
        case FEATURE_INTEROP_CTV: return "Interoperability CTV";
        case FEATURE_INTEROP_LIGHTNING: return "Interoperability Lightning";
        case FEATURE_INTEROP_REPO: return "Interoperability Repo";
        case FEATURE_INTEROP_SIDECHAINS: return "Interoperability Side Chains";
        case FEATURE_INTEROP_CROSSCHAINATOMICSWAPS: return "Interoperability Cross Chain Atomic Swapss";
        case FEATURE_GRAPHDEFAULTSWAPS: return "Interoperability Graph Default Swaps";
        case FEATURE_INTERESTRATESWAPS: return "Interoperability Swaps";
        case FEATURE_MINERFEECONTRACTS: return "Interoperability Miner Fee Contracts";
        case FEATURE_MASSPAYMENT: return "Mass Payment";
        case FEATURE_SEND_MANY: return "Send to Many";
        case FEATURE_HEDGEDCURRENCY: return "Hedge Currency";
        case FEATURE_PEGGED_CURRENCY: return "Pegged Currency";
        default: return "Unknown feature";
    }
}

/**
 * Checks, whether a feature is activated at the given block.
 */
bool IsFeatureActivated(uint16_t featureId, int transactionBlock)
{
    const CConsensusParams& params = ConsensusParams();
    int activationBlock = std::numeric_limits<int>::max();

    switch (featureId) {
      case FEATURE_VESTING:
          activationBlock = params.MSC_VESTING_BLOCK;
          break;

      case FEATURE_KYC:
          activationBlock = params.MSC_KYC_BLOCK;
          break;

      case FEATURE_DEX_SELL:
          activationBlock = params.MSC_DEXSELL_BLOCK;
          break;

      case FEATURE_DEX_BUY:
          activationBlock = params.MSC_DEXBUY_BLOCK;
          break;

      case FEATURE_METADEX:
          activationBlock = params.MSC_METADEX_BLOCK;
          break;

      case FEATURE_TRADECHANNELS_TOKENS:
          activationBlock = params.MSC_TRADECHANNEL_TOKENS_BLOCK;
          break;

      case FEATURE_FIXED:
          activationBlock = params.MSC_SP_BLOCK;
          break;

      case FEATURE_MANAGED:
          activationBlock = params.MSC_MANUALSP_BLOCK;
          break;

      case FEATURE_NODE_REWARD:
          activationBlock = params.MSC_NODE_REWARD_BLOCK;
          break;

      case FEATURE_CONTRACTDEX:
          activationBlock = params.MSC_NODE_REWARD_BLOCK;
          break;

      case FEATURE_CONTRACTDEX_ORACLES:
          activationBlock = params.MSC_CONTRACTDEX_ORACLES_BLOCK;
          break;

      case FEATURE_TRADECHANNELS_OPTIONS:
          activationBlock = params.MSC_TRADECHANNEL_OPTIONS_BLOCK;
          break;

      case FEATURE_TRADECHANNELS_CONTRACTS:
          activationBlock = params.MSC_TRADECHANNEL_CONTRACTS_BLOCK;
          break;

      case FEATURE_DISPENSERVAULTS:
          activationBlock = params.MSC_DISPENSERVAULTS_BLOCK;
          break;

      case FEATURE_PAYMENTBATCHING:
          activationBlock = params.MSC_PAYMENTBATCHING_BLOCK;
          break;

      case FEATURE_MARGINLENDING:
          activationBlock = params.MSC_MARGINLENDING_BLOCK;
          break;

      case FEATURE_INTEROP_CTV:
          activationBlock = params.MSC_INTEROP_CTV_BLOCK;
          break;

      case FEATURE_INTEROP_LIGHTNING:
          activationBlock = params.MSC_INTEROP_LIGHTNING_BLOCK;
          break;

      case FEATURE_INTEROP_REPO:
          activationBlock = params.MSC_INTEROP_REPO_BLOCK;
          break;

      case FEATURE_INTEROP_SIDECHAINS:
          activationBlock = params.MSC_INTEROP_SIDECHAINS_BLOCK;
          break;

      case FEATURE_INTEROP_CROSSCHAINATOMICSWAPS:
          activationBlock = params.MSC_INTEROP_CROSSCHAINATOMICSWAPS_BLOCK;
          break;

      case FEATURE_GRAPHDEFAULTSWAPS:
          activationBlock = params.MSC_GRAPHDEFAULTSWAPS_BLOCK;
          break;

      case FEATURE_INTERESTRATESWAPS:
          activationBlock = params.MSC_INTERESTRATESWAPS_BLOCK;
          break;

      case FEATURE_MINERFEECONTRACTS:
          activationBlock = params.MSC_MINERFEECONTRACTS_BLOCK;
          break;

      case FEATURE_MASSPAYMENT:
          activationBlock = params.MSC_MASSPAYMENT_BLOCK;
          break;

      case FEATURE_HEDGEDCURRENCY:
          activationBlock = params.MSC_HEDGEDCURRENCY_BLOCK;
          break;

      case FEATURE_SEND_MANY:
          activationBlock = params.MSC_SEND_MANY_BLOCK;
          break;

      case FEATURE_PEGGED_CURRENCY:
          activationBlock = params.MSC_VESTING_BLOCK;
          break;

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
bool IsTransactionTypeAllowed(int txBlock, uint16_t txType, uint16_t version)
{
    const std::vector<TransactionRestriction>& vTxRestrictions = ConsensusParams().GetRestrictions();

    for (std::vector<TransactionRestriction>::const_iterator it = vTxRestrictions.begin(); it != vTxRestrictions.end(); ++it)
    {
        const TransactionRestriction& entry = *it;

        if (entry.txType != txType || entry.txVersion != version) {
            if(msc_debug_is_transaction_type_allowed) PrintToLog("%s(): entry.txType: %d, entry.txVersion : %d\n",__func__, entry.txType, entry.txVersion);
            continue;
        }

        if (txBlock >= entry.activationBlock) {
            if(msc_debug_is_transaction_type_allowed) PrintToLog("%s(): TRUE!, txBlock: %d; entry.activationBlock: %d\n",__func__, txBlock, entry.activationBlock);
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
