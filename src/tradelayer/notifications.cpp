#include <tradelayer/notifications.h>

#include <tradelayer/log.h>
#include <tradelayer/rules.h>
#include <tradelayer/tradelayer.h>
#include <tradelayer/utilsbitcoin.h>
#include <tradelayer/version.h>

#include <ui_interface.h>

#include <util/system.h>
#include <validation.h>

#include <stdint.h>
#include <string>
#include <vector>

namespace mastercore
{

//! Vector of currently active Trade Layer alerts
std::vector<AlertData> currentTlAlerts;

/**
 * Deletes previously broadcast alerts from sender from the alerts vector
 *
 * Note cannot be used to delete alerts from other addresses, nor to delete system generated feature alerts
 */
void DeleteAlerts(const std::string& sender)
{
    for (std::vector<AlertData>::iterator it = currentTlAlerts.begin(); it != currentTlAlerts.end(); )
    {
        AlertData alert = *it;
        if (sender == alert.alert_sender) {
            PrintToLog("Removing deleted alert (from:%s type:%d expiry:%d message:%s)\n", alert.alert_sender,
                alert.alert_type, alert.alert_expiry, alert.alert_message);
            it = currentTlAlerts.erase(it);
            //uiInterface.TLStateChanged();
        } else {
            it++;
        }
    }
}

/**
 * Removes all active alerts.
 *
 * A signal is fired to notify the UI about the status update.
 */
void ClearAlerts()
{
    currentTlAlerts.clear();
    //uiInterface.TLStateChanged();
}

/**
 * Adds a new alert to the alerts vector
 *
 */
void AddAlert(const std::string& sender, uint16_t alertType, uint32_t alertExpiry, const std::string& alertMessage)
{
    AlertData newAlert;
    newAlert.alert_sender = sender;
    newAlert.alert_type = alertType;
    newAlert.alert_expiry = alertExpiry;
    newAlert.alert_message = alertMessage;

    // very basic sanity checks for broadcast alerts to catch malformed packets
    if (sender != "tradelayer" && (alertType < ALERT_BLOCK_EXPIRY || alertType > ALERT_CLIENT_VERSION_EXPIRY)) {
        PrintToLog("New alert REJECTED (alert type not recognized): %s, %d, %d, %s\n", sender, alertType, alertExpiry, alertMessage);
        return;
    }

    currentTlAlerts.push_back(newAlert);
    PrintToLog("New alert added: %s, %d, %d, %s\n", sender, alertType, alertExpiry, alertMessage);
}

/**
 * Determines whether the sender is an authorized source for Trade Layer alerts.
 *
 * The option "-tlalertallowsender=source" can be used to whitelist additional sources,
 * and the option "-tlalertignoresender=source" can be used to ignore a source.
 *
 * To consider any alert as authorized, "-tlalertallowsender=any" can be used. This
 * should only be done for testing purposes!
 */
bool CheckAlertAuthorization(const std::string& sender)
{
    std::set<std::string> whitelisted;

    // TODO : NEW ALERT USERS
    // Add manually whitelisted sources
    if (gArgs.IsArgSet("-tlalertallowsender")) {
        const std::vector<std::string>& sources = gArgs.GetArgs("-tlalertallowsender");
        for (std::vector<std::string>::const_iterator it = sources.begin(); it != sources.end(); ++it) {
            whitelisted.insert(*it);
        }
    }

    // Remove manually ignored sources
    if (gArgs.IsArgSet("-tlalertignoresender")) {
        const std::vector<std::string>& sources = gArgs.GetArgs("-tlalertignoresender");
        for (std::vector<std::string>::const_iterator it = sources.begin(); it != sources.end(); ++it) {
            whitelisted.erase(*it);
        }
    }

    return (whitelisted.count(sender) || whitelisted.count("any"));
}

/**
 * Alerts including meta data.
 */
std::vector<AlertData> GetTradeLayerAlerts()
{
    return currentTlAlerts;
}

/**
 * Human readable alert messages.
 */
std::vector<std::string> GetTradeLayerAlertMessages()
{
    std::vector<std::string> vstr;
    for (std::vector<AlertData>::iterator it = currentTlAlerts.begin(); it != currentTlAlerts.end(); it++) {
        vstr.push_back((*it).alert_message);
    }
    
    return vstr;
}

/**
 * Expires any alerts that need expiring.
 */
bool CheckExpiredAlerts(unsigned int curBlock, uint64_t curTime)
{
    for (std::vector<AlertData>::iterator it = currentTlAlerts.begin(); it != currentTlAlerts.end(); ) {
        AlertData alert = *it;
        switch (alert.alert_type) {
            case ALERT_BLOCK_EXPIRY:
                if (curBlock >= alert.alert_expiry) {
                    PrintToLog("Expiring alert (from %s: type:%d expiry:%d message:%s)\n", alert.alert_sender,
                        alert.alert_type, alert.alert_expiry, alert.alert_message);
                    it = currentTlAlerts.erase(it);
                    //uiInterface.TLStateChanged();
                } else {
                    it++;
                }
            break;
            case ALERT_BLOCKTIME_EXPIRY:
                if (curTime > alert.alert_expiry) {
                    PrintToLog("Expiring alert (from %s: type:%d expiry:%d message:%s)\n", alert.alert_sender,
                        alert.alert_type, alert.alert_expiry, alert.alert_message);
                    it = currentTlAlerts.erase(it);
                    //uiInterface.TLStateChanged();
                } else {
                    it++;
                }
            break;
            case ALERT_CLIENT_VERSION_EXPIRY:
                if (TL_VERSION > alert.alert_expiry) {
                    PrintToLog("Expiring alert (form: %s type:%d expiry:%d message:%s)\n", alert.alert_sender,
                        alert.alert_type, alert.alert_expiry, alert.alert_message);
                    it = currentTlAlerts.erase(it);
                    //uiInterface.TLStateChanged();
                } else {
                    it++;
                }
            break;
            default: // unrecognized alert type
                    PrintToLog("Removing invalid alert (from:%s type:%d expiry:%d message:%s)\n", alert.alert_sender,
                        alert.alert_type, alert.alert_expiry, alert.alert_message);
                    it = currentTlAlerts.erase(it);
                    //uiInterface.TLStateChanged();
            break;
        }
    }
    return true;
}

} // namespace mastercore
