/**
 * @file activation.cpp
 *
 * This file contains feature activation utility code.
 *
 * Note main functions 'ActivateFeature()' and 'DeactivateFeature()' are consensus breaking and reside in rules.cpp
 */

#include <tradelayer/activation.h>
#include <tradelayer/log.h>
#include <tradelayer/utilsbitcoin.h>
#include <tradelayer/version.h>

#include <validation.h>
#include <ui_interface.h>

#include <set>
#include <stdint.h>
#include <string>
#include <vector>

namespace mastercore
{
//! Pending activations
std::vector<FeatureActivation> vecPendingActivations;
//! Completed activations
std::vector<FeatureActivation> vecCompletedActivations;

/**
 * Deletes pending activations with the given identifier.
 *
 * Deletion is not supported on the CompletedActivations vector.
 */
static void DeletePendingActivation(uint16_t featureId)
{
   for (std::vector<FeatureActivation>::iterator it = vecPendingActivations.begin(); it != vecPendingActivations.end(); ) {
       (it->featureId == featureId) ? it = vecPendingActivations.erase(it) : ++it;
   }
}

/**
 * Moves an activation from PendingActivations to CompletedActivations when it is activated.
 *
 * A signal is fired to notify the UI about the status update.
 */
static void PendingActivationCompleted(FeatureActivation activation)
{
     DeletePendingActivation(activation.featureId);

     // status for specific feature: completed
     activation.status = true;
     vecCompletedActivations.push_back(activation);
     // uiInterface.tlStateChanged();
}

/**
 * Adds a feature activation to the PendingActivations vector.
 *
 * If this feature was previously scheduled for activation, then the state pending objects are deleted.
 */
void AddPendingActivation(uint16_t featureId, int activationBlock, uint32_t minClientVersion, const std::string& featureName)
{
    DeletePendingActivation(featureId);

    FeatureActivation featureActivation;
    featureActivation.featureId = featureId;
    featureActivation.featureName = featureName;
    featureActivation.activationBlock = activationBlock;
    featureActivation.minClientVersion = minClientVersion;
    featureActivation.status = false;

    vecPendingActivations.push_back(featureActivation);

    // uiInterface.tlStateChanged();
}

/**
 * Checks if any activations went live in the block.
 */
void CheckLiveActivations(int blockHeight)
{
    std::vector<FeatureActivation> vecPendingActivations = GetPendingActivations();
    for (std::vector<FeatureActivation>::iterator it = vecPendingActivations.begin(); it != vecPendingActivations.end(); ++it) {
        const FeatureActivation& liveActivation = *it;
        if (liveActivation.activationBlock > blockHeight) {
            continue;
        }
        if (TL_VERSION < liveActivation.minClientVersion) {
            std::string msgText = strprintf("Shutting down due to unsupported feature activation (%d: %s)", liveActivation.featureId, liveActivation.featureName);
            PrintToLog(msgText);
            PrintToConsole(msgText);
            if (!gArgs.GetBoolArg("-overrideforcedshutdown", false)) {
                fs::path persistPath = GetDataDir() / "OCL_persist";
                if (fs::exists(persistPath)) fs::remove_all(persistPath); // prevent the node being restarted without a reparse after forced shutdown
                DoAbortNode(msgText, msgText);
            }
        }
        PendingActivationCompleted(liveActivation);
    }
}


/**
 * Returns the vector of pending activations.
 */
std::vector<FeatureActivation> GetPendingActivations()
{
    return vecPendingActivations;
}

/**
 * Returns the vector of completed activations.
 */
std::vector<FeatureActivation> GetCompletedActivations()
{
    return vecCompletedActivations;
}

/**
 * Removes all pending or completed activations.
 *
 * A signal is fired to notify the UI about the status update.
 */
void ClearActivations()
{
    vecPendingActivations.clear();
    vecCompletedActivations.clear();
     // uiInterface.tlStateChanged();
}

/**
 * Determines whether the sender is an authorized source for Trade Layer feature activation.
 *
 * The option "-tlactivationallowsender=source" can be used to whitelist additional sources,
 * and the option "-tlactivationignoresender=source" can be used to ignore a source.
 *
 * To consider any activation as authorized, "-tlactivationallowsender=any" can be used. This
 * should only be done for testing purposes!
 */
bool CheckActivationAuthorization(const std::string& sender)
{
    std::set<std::string> whitelisted;

    // Mainnet - 2 out of 3 signatures required from developers & board members
    whitelisted.insert("MQ4r3yi4jHEHhLSLhzabSBHs1x1g6HdxL3");

    // Testnet - 1 out of 3 signatures required from developers & board members
    whitelisted.insert("QPAjL1rgVzzM5XPkAVgjmt5kHWv44Cf8Aj");

    // Regtest
    // use -tlactivationallowsender for testing

    // Add manually whitelisted sources
    if (gArgs.IsArgSet("-tlactivationallowsender") && RegTest()) {
        const std::vector<std::string>& sources = gArgs.GetArgs("-tlactivationallowsender");

        for (std::vector<std::string>::const_iterator it = sources.begin(); it != sources.end(); ++it) {
            whitelisted.insert(*it);
        }
    }

    // Remove manually ignored sources
    if (gArgs.IsArgSet("-tlactivationignoresender") && RegTest()) {
        const std::vector<std::string>& sources = gArgs.GetArgs("-tlactivationignoresender");

        for (std::vector<std::string>::const_iterator it = sources.begin(); it != sources.end(); ++it) {
            whitelisted.erase(*it);
        }
    }

    return (whitelisted.count(sender) || whitelisted.count("any"));
}

/**
 * Determines whether the sender is an authorized source to deactivate features.
 *
 * The custom options "-tlactivationallowsender=source" and "-tlactivationignoresender=source" are also applied to deactivations.
 */
bool CheckDeactivationAuthorization(const std::string& sender)
{
    std::set<std::string> whitelisted;

    // Mainnet - 2 out of 3 signatures required from developers & board members
    whitelisted.insert("MQ4r3yi4jHEHhLSLhzabSBHs1x1g6HdxL3");

    // Testnet - 1 out of 3 signatures required from developers & board members
    whitelisted.insert("QPAjL1rgVzzM5XPkAVgjmt5kHWv44Cf8Aj");

    // Regtest
    // use -tlactivationallowsender for testing

    // Add manually whitelisted sources - custom sources affect both activation and deactivation
    if (gArgs.IsArgSet("-tlactivationallowsender") && RegTest()) {
        const std::vector<std::string>& sources = gArgs.GetArgs("-tlactivationallowsender");

        for (std::vector<std::string>::const_iterator it = sources.begin(); it != sources.end(); ++it) {
            whitelisted.insert(*it);
        }
    }

    // Remove manually ignored sources - custom sources affect both activation and deactivation
    if (gArgs.IsArgSet("-tlactivationignoresender") && RegTest()) {
        const std::vector<std::string>& sources = gArgs.GetArgs("-tlactivationignoresender");

        for (std::vector<std::string>::const_iterator it = sources.begin(); it != sources.end(); ++it) {
            whitelisted.erase(*it);
        }
    }

    return (whitelisted.count(sender) || whitelisted.count("any"));
}

} // namespace mastercore
