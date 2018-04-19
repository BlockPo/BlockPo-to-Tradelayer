// Copyright (c) 2011-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sendmpdialog.h"
#include "ui_sendmpdialog.h"

#include "omnicore_qtutils.h"

#include "clientmodel.h"
#include "walletmodel.h"
#include "platformstyle.h"

#include "omnicore/createpayload.h"
#include "omnicore/errors.h"
#include "omnicore/omnicore.h"
#include "omnicore/parse_string.h"
#include "omnicore/pending.h"
#include "omnicore/sp.h"
#include "omnicore/tally.h"
#include "omnicore/utilsbitcoin.h"
#include "omnicore/wallettxs.h"

#include "amount.h"
#include "base58.h"
#include "bitcoinunits.h"
#include "main.h"
#include "optionsmodel.h"
#include "sync.h"
#include "txmempool.h"
#include "uint256.h"
#include "wallet/wallet.h"

#include "qt/sendcoinsdialog.h"

#include <stdint.h>

#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <QDateTime>
#include <QDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QSettings>
#include <QString>
#include <QWidget>

#include <boost/lexical_cast.hpp>

using std::ostringstream;
using std::string;

using namespace mastercore;

bool bShowFeeHint = false;
bool bShowEmptyHint = false;

#define SEND_CONFIRM_DELAY   3

SendMPDialog::SendMPDialog(const PlatformStyle *platformStyle, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SendMPDialog),
    clientModel(0),
    walletModel(0),
    platformStyle(platformStyle)
{
    ui->setupUi(this);

    if (!platformStyle->getImagesOnButtons()) {
        ui->clearButton->setIcon(QIcon());
        ui->sendButton->setIcon(QIcon());
    } else {
        ui->clearButton->setIcon(platformStyle->SingleColorIcon(":/icons/remove"));
        ui->sendButton->setIcon(platformStyle->SingleColorIcon(":/icons/send"));
    }

#if QT_VERSION >= 0x040700
    ui->sendToLineEdit->setPlaceholderText("Enter an Omni Lite address (e.g. LLomniqvhCKdMEQNkezXH71tmrvfxCL5Ju)");
    ui->amountLineEdit->setPlaceholderText("Enter Amount");
#endif

    ui->hintLabel->setVisible(false);

    ui->typeDescLabel->setText(QString::fromStdString(GetLongDescription(MSC_TYPE_SIMPLE_SEND)));
    ui->typeCombo->addItem("Simple Send","0");
    ui->typeCombo->addItem("Issuance (Fixed)","50");
    ui->typeCombo->addItem("Issuance (Managed)","54");
    ui->typeCombo->addItem("Grant Tokens","55");
    ui->typeCombo->addItem("Revoke Tokens","56");
    ui->typeCombo->addItem("Issuance (Crowdsale)","51");
    ui->typeCombo->addItem("Close Crowdsale","53");
    typeComboBoxChanged(0);

    /**
    !! This is just a prototype at the moment, and the genesis block will likely be reset if this is taken forward.
    !! Main ecosystem transactions are thus invalidated, so only allow the UI to send transactions within the test ecosystem
    */
    ui->chkTestEco->setCheckState(Qt::Checked);
    ui->chkTestEco->setEnabled(false);

    // Default new properties to divisible
    ui->chkDivisible->setCheckState(Qt::Checked);

    // connect actions
    connect(ui->propertyComboBox, SIGNAL(activated(int)), this, SLOT(propertyComboBoxChanged(int)));
    connect(ui->sendFromComboBox, SIGNAL(activated(int)), this, SLOT(sendFromComboBoxChanged(int)));
    connect(ui->clearButton, SIGNAL(clicked()), this, SLOT(clearFields()));
    connect(ui->sendButton, SIGNAL(clicked()), this, SLOT(sendOmniTransaction()));
    connect(ui->typeCombo, SIGNAL(activated(int)), this, SLOT(typeComboBoxChanged(int)));
    connect(ui->chkDivisible, SIGNAL(clicked()), this, SLOT(divisChkClicked()));

    // set up fees group - for now this is cloned from sendcoinsdialog
    QSettings settings;
    if (!settings.contains("fFeeSectionMinimized"))
        settings.setValue("fFeeSectionMinimized", true);
    if (!settings.contains("nFeeRadio") && settings.contains("nTransactionFee") && settings.value("nTransactionFee").toLongLong() > 0) // compatibility
        settings.setValue("nFeeRadio", 1); // custom
    if (!settings.contains("nFeeRadio"))
        settings.setValue("nFeeRadio", 0); // recommended
    if (!settings.contains("nCustomFeeRadio") && settings.contains("nTransactionFee") && settings.value("nTransactionFee").toLongLong() > 0) // compatibility
        settings.setValue("nCustomFeeRadio", 1); // total at least
    if (!settings.contains("nCustomFeeRadio"))
        settings.setValue("nCustomFeeRadio", 0); // per kilobyte
    if (!settings.contains("nSmartFeeSliderPosition"))
        settings.setValue("nSmartFeeSliderPosition", 0);
    if (!settings.contains("nTransactionFee"))
        settings.setValue("nTransactionFee", (qint64)DEFAULT_TRANSACTION_FEE);
    if (!settings.contains("fPayOnlyMinFee"))
        settings.setValue("fPayOnlyMinFee", false);
    ui->groupFee->setId(ui->radioSmartFee, 0);
    ui->groupFee->setId(ui->radioCustomFee, 1);
    ui->groupFee->button((int)std::max(0, std::min(1, settings.value("nFeeRadio").toInt())))->setChecked(true);
    ui->groupCustomFee->setId(ui->radioCustomPerKilobyte, 0);
    ui->groupCustomFee->setId(ui->radioCustomAtLeast, 1);
    ui->groupCustomFee->button((int)std::max(0, std::min(1, settings.value("nCustomFeeRadio").toInt())))->setChecked(true);
    ui->sliderSmartFee->setValue(settings.value("nSmartFeeSliderPosition").toInt());
    ui->customFee->setValue(settings.value("nTransactionFee").toLongLong());
    ui->checkBoxMinimumFee->setChecked(settings.value("fPayOnlyMinFee").toBool());
    minimizeFeeSection(settings.value("fFeeSectionMinimized").toBool());

    // initial update
    balancesUpdated();
}

SendMPDialog::~SendMPDialog()
{
    delete ui;
}

void SendMPDialog::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if (model != NULL) {
        connect(model, SIGNAL(refreshOmniBalance()), this, SLOT(balancesUpdated()));
        connect(model, SIGNAL(reinitOmniState()), this, SLOT(balancesUpdated()));
    }
}

void SendMPDialog::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
    if (model != NULL) {
        connect(model, SIGNAL(balanceChanged(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)), this, SLOT(updateFrom()));

        // fee section
        connect(ui->sliderSmartFee, SIGNAL(valueChanged(int)), this, SLOT(updateSmartFeeLabel()));
        connect(ui->sliderSmartFee, SIGNAL(valueChanged(int)), this, SLOT(updateGlobalFeeVariables()));
        connect(ui->groupFee, SIGNAL(buttonClicked(int)), this, SLOT(updateFeeSectionControls()));
        connect(ui->groupFee, SIGNAL(buttonClicked(int)), this, SLOT(updateGlobalFeeVariables()));
        connect(ui->groupCustomFee, SIGNAL(buttonClicked(int)), this, SLOT(updateGlobalFeeVariables()));
        connect(ui->customFee, SIGNAL(valueChanged()), this, SLOT(updateGlobalFeeVariables()));
        connect(ui->checkBoxMinimumFee, SIGNAL(stateChanged(int)), this, SLOT(setMinimumFee()));
        connect(ui->checkBoxMinimumFee, SIGNAL(stateChanged(int)), this, SLOT(updateFeeSectionControls()));
        connect(ui->checkBoxMinimumFee, SIGNAL(stateChanged(int)), this, SLOT(updateGlobalFeeVariables()));

        ui->customFee->setSingleStep(CWallet::GetRequiredFee(1000));
        updateFeeSectionControls();
        updateMinFeeLabel();
        updateSmartFeeLabel();
        updateGlobalFeeVariables();
    }
}

/**
  *  Updates the property selection combo box based on the type of transaction selected
  *
  *  Note: uses the state to filter out options that would result in invalid transactions
  */
void SendMPDialog::updatePropSelector()
{
    LOCK2(cs_main, cs_tally);

    uint16_t typeId = GetCurrentType();
    if (typeId == 9999) {
        QMessageBox::critical( this, "User interface error",
        "The transaction type could not be determined from the transaction selector." );
        return;
    }

    QString spId = ui->propertyComboBox->itemData(ui->propertyComboBox->currentIndex()).toString();
    ui->propertyComboBox->clear();

    uint32_t nextPropIdMainEco = GetNextPropertyId(true);
    uint32_t nextPropIdTestEco = GetNextPropertyId(false);

    for (unsigned int propertyId = 3; propertyId < nextPropIdMainEco; propertyId++) {
        if (FilterProperty(typeId, propertyId)) continue;
        ui->propertyComboBox->addItem(propToStr(propertyId).c_str(), strprintf("%d",propertyId).c_str());
    }
    for (unsigned int propertyId = 2147483651; propertyId < nextPropIdTestEco; propertyId++) {
        if (FilterProperty(typeId, propertyId)) continue;
        ui->propertyComboBox->addItem(propToStr(propertyId).c_str(), strprintf("%d",propertyId).c_str());
    }

    int propIdx = ui->propertyComboBox->findData(spId);
    if (propIdx != -1) { ui->propertyComboBox->setCurrentIndex(propIdx); }
}

/**
  * Updates the interface when the 'send from' address selection combo is changed
  */
void SendMPDialog::updateFrom()
{
    std::string currentSetFromAddress = ui->sendFromComboBox->currentText().toStdString();

    if (currentSetFromAddress.empty()) {
        ui->balanceLabel->setText(QString::fromStdString("N/A"));
        bShowFeeHint = false;
        hintHelper();
        return;
    }

    // strip out the extra balance data from the combo text to leave just the address remaining
    size_t spacer = currentSetFromAddress.find(" ");
    if (spacer != std::string::npos) {
        currentSetFromAddress = currentSetFromAddress.substr(0,spacer);
        ui->sendFromComboBox->setEditable(true);
        QLineEdit *comboDisplay = ui->sendFromComboBox->lineEdit();
        comboDisplay->setText(QString::fromStdString(currentSetFromAddress));
        comboDisplay->setReadOnly(true);
    }

    // update the balance for the selected address
    uint32_t propertyId = GetCurrentPropertyId();
    if (propertyId > 0) {
        ui->balanceLabel->setText(QString::fromStdString(FormatMP(propertyId, getUserAvailableMPbalance(currentSetFromAddress, propertyId)) + getTokenLabel(propertyId)));
    } else {
        ui->balanceLabel->setText(QString::fromStdString("N/A"));
    }

    // provide a hint if it looks like there are insufficient fees for a transaction
    if (CheckFee(currentSetFromAddress, 80)) {
        bShowFeeHint = false;
        hintHelper();
    } else {
        bShowFeeHint = true;
        hintHelper("Hint: To use the selected sending address you'll need to fund it with some Litecoin for transaction fees.");
    }
}

/**
  * Updates the interface when the property selection combo is changed
  */
void SendMPDialog::updateProperty()
{
    uint32_t propertyId = GetCurrentPropertyId();
    uint16_t typeId = GetCurrentType();

    std::string currentSetFromAddress = ui->sendFromComboBox->currentText().toStdString();
    ui->sendFromComboBox->clear();

    // Repopulate the from address selector to include any spendable address with tokens in the selected property
    if (typeId == MSC_TYPE_SIMPLE_SEND || typeId == MSC_TYPE_REVOKE_PROPERTY_TOKENS) {
        LOCK(cs_tally);
        for (std::unordered_map<string, CMPTally>::iterator my_it = mp_tally_map.begin(); my_it != mp_tally_map.end(); ++my_it) {
            std::string address = (my_it->first).c_str();
            if (!getUserAvailableMPbalance(address, propertyId)) continue;
            if (IsMyAddress(address) != ISMINE_SPENDABLE) continue;
            ui->sendFromComboBox->addItem(QString::fromStdString(address + " \t" + FormatMP(propertyId, getUserAvailableMPbalance(address, propertyId)) + getTokenLabel(propertyId)));
        }
    }

    // Repopulate the from address selector to include any address with spendable utxos
    if (typeId == MSC_TYPE_CREATE_PROPERTY_FIXED || typeId == MSC_TYPE_CREATE_PROPERTY_VARIABLE || typeId == MSC_TYPE_CREATE_PROPERTY_MANUAL) {
        std::set<std::string> setAddresses;
        std::vector<COutput> vecOutputs;
        assert(pwalletMain != NULL);
        LOCK2(cs_main, pwalletMain->cs_wallet);
        pwalletMain->AvailableCoins(vecOutputs, false, NULL, true);
        BOOST_FOREACH(const COutput& out, vecOutputs) {
            CTxDestination address;
            const CScript& scriptPubKey = out.tx->vout[out.i].scriptPubKey;
            bool fValid = ExtractDestination(scriptPubKey, address);
            std::string strAddress = CBitcoinAddress(address).ToString();

            if (fValid && !setAddresses.count(strAddress)) {
                setAddresses.insert(strAddress);
            }
        }
        for (std::set<std::string>::iterator it = setAddresses.begin(); it != setAddresses.end(); ++it) {
            ui->sendFromComboBox->addItem(QString::fromStdString(*it));
        }
    }

    // Repopulate the from address selector to include the issuer address for the selected property if it's spendable
    if (typeId == MSC_TYPE_CLOSE_CROWDSALE || typeId == MSC_TYPE_GRANT_PROPERTY_TOKENS) {
        LOCK(cs_tally);
        CMPSPInfo::Entry sp;
        if (_my_sps->getSP(propertyId, sp)) {
            if (IsMyAddress(sp.issuer) == ISMINE_SPENDABLE) {
                ui->sendFromComboBox->addItem(QString::fromStdString(sp.issuer));
            }
        }
    }

    int fromIdx = ui->sendFromComboBox->findText(QString::fromStdString(currentSetFromAddress), Qt::MatchContains);
    if (fromIdx != -1) {
        ui->sendFromComboBox->setCurrentIndex(fromIdx);
    }

    // populate balance for global wallet
    if (typeId == MSC_TYPE_SIMPLE_SEND || typeId == MSC_TYPE_REVOKE_PROPERTY_TOKENS) {
        ui->globalBalanceLabel->setText(QString::fromStdString("Wallet Balance (Available): " + FormatMP(propertyId, global_balance_money[propertyId])));
    } else {
        ui->globalBalanceLabel->setText(QString::fromStdString("Wallet Balance (Available): N/A"));
    }

    // provide hints if there are no populated addresses
    if (ui->sendFromComboBox->count() == 0) {
        bShowEmptyHint = true;
        if (typeId == MSC_TYPE_CREATE_PROPERTY_FIXED || typeId == MSC_TYPE_CREATE_PROPERTY_VARIABLE || typeId == MSC_TYPE_CREATE_PROPERTY_MANUAL) {
            hintHelper("Hint: The wallet is empty.  To send a transaction you'll need some Litecoin for fees.");
        } else if (typeId == MSC_TYPE_SIMPLE_SEND || typeId == MSC_TYPE_REVOKE_PROPERTY_TOKENS) {
            hintHelper("Hint: You don't have any tokens in the wallet.  To send a transaction you'll need to create or obtain some tokens.");
        } else if (typeId == MSC_TYPE_GRANT_PROPERTY_TOKENS) {
            hintHelper("Hint: You can't select any options here because there are no addresses in the wallet that control a managed property.");
        } else if (typeId == MSC_TYPE_CLOSE_CROWDSALE) {
            hintHelper("Hint: You can't select any options here because there are no addresses in the wallet with an active crowdsale.");
        } else {
            hintHelper("Hint: Unknown.");
        }
    } else {
        bShowEmptyHint = false;
        hintHelper();
    }

#if QT_VERSION >= 0x040700
    // update placeholder text
    bool displayDivis = false;
    if (typeId == MSC_TYPE_SIMPLE_SEND || typeId == MSC_TYPE_REVOKE_PROPERTY_TOKENS || typeId == MSC_TYPE_GRANT_PROPERTY_TOKENS) {
        if (isPropertyDivisible(propertyId)) {
            displayDivis = true;
        }
    } else {
        if (ui->chkDivisible->isChecked()) {
            displayDivis = true;
        }
    }
    if (displayDivis) {
        ui->amountLineEdit->setPlaceholderText("Enter divisible amount");
    } else {
        ui->amountLineEdit->setPlaceholderText("Enter indivisible amount");
    }
#endif
}

/**
 * Creates a transaction with the parameters selected in the UI and broadcasts it
 *
 */
void SendMPDialog::sendOmniTransaction()
{
    // obtain the type of tx we are sending
    uint16_t typeId = GetCurrentType();

    // obtain the selected sender address
    string strFromAddress = ui->sendFromComboBox->currentText().toStdString();
    CBitcoinAddress fromAddress;
    if (false == strFromAddress.empty()) fromAddress.SetString(strFromAddress);
    if (!fromAddress.IsValid()) {
        QMessageBox::critical( this, "Unable to send transaction",
        "The sender address selected is not valid.\n\nPlease double-check the transction details thoroughly before retrying your send transaction." );
        return;
    }

    // obtain the reference address if appropriate
    string strRefAddress = ui->sendToLineEdit->text().toStdString();
    CBitcoinAddress refAddress;
    if (typeId == MSC_TYPE_SIMPLE_SEND || typeId == MSC_TYPE_GRANT_PROPERTY_TOKENS) {
        if (typeId == MSC_TYPE_GRANT_PROPERTY_TOKENS && strRefAddress.empty()) {
            /** grants are permitted without a recipient **/
        } else {
            if (false == strRefAddress.empty()) refAddress.SetString(strRefAddress);
            if (!refAddress.IsValid()) {
                QMessageBox::critical( this, "Unable to send transaction",
                "The recipient address entered is not valid.\n\nPlease double-check the transction details thoroughly before retrying your send transaction." );
                return;
            }
        }
    }

    // obtain the property being sent
    uint32_t propertyId = GetCurrentPropertyId();
    if (propertyId == 0) {
        if (typeId == MSC_TYPE_SIMPLE_SEND || typeId == MSC_TYPE_CREATE_PROPERTY_VARIABLE || typeId == MSC_TYPE_CLOSE_CROWDSALE ||
            typeId == MSC_TYPE_GRANT_PROPERTY_TOKENS || typeId == MSC_TYPE_REVOKE_PROPERTY_TOKENS) {
                QMessageBox::critical( this, "Unable to send transaction",
                "The property selected is not valid.\n\nPlease double-check the transction details thoroughly before retrying your send transaction." );
                return;
        }
    }

    // obtain the metadata
    std::string strPropName = ui->nameLE->text().toStdString();
    std::string strPropURL = ui->urlLE->text().toStdString();
    if (typeId == MSC_TYPE_CREATE_PROPERTY_FIXED || typeId == MSC_TYPE_CREATE_PROPERTY_VARIABLE || typeId == MSC_TYPE_CREATE_PROPERTY_MANUAL) {
        if (strPropName.empty()) {
            QMessageBox::critical( this, "Unable to send transaction",
            "The property name cannot be empty.\n\nPlease choose a name for your property and try your transaction again." );
            return;
        }
    }

    // obtain the ecosystem
    uint8_t ecosystem = 2;
    std::string strEco = "Test Ecosystem";
    if (!ui->chkTestEco->isChecked()) {
        ecosystem = 1;
        strEco = "Main Ecosystem";
    }

    // obtain the divisibility
    bool divisible = false;
    if (typeId == MSC_TYPE_CREATE_PROPERTY_FIXED || typeId == MSC_TYPE_CREATE_PROPERTY_VARIABLE || typeId == MSC_TYPE_CREATE_PROPERTY_MANUAL) {
        if (ui->chkDivisible->isChecked()) divisible = true;
    } else {
        divisible = isPropertyDivisible(propertyId);
    }
    uint16_t propertyType = divisible ? 2 : 1;
    std::string strPropType = divisible ? "Divisible" : "Indivisible";

    // obtain the amount
    int64_t sendAmount = 0;
    string strAmount = ui->amountLineEdit->text().toStdString();
    if (typeId == MSC_TYPE_SIMPLE_SEND || typeId == MSC_TYPE_CREATE_PROPERTY_FIXED || typeId == MSC_TYPE_CREATE_PROPERTY_VARIABLE ||
        typeId == MSC_TYPE_GRANT_PROPERTY_TOKENS || typeId == MSC_TYPE_REVOKE_PROPERTY_TOKENS) {
        if (!divisible) {
            // warn if we have to truncate the amount due to a decimal amount for an indivisible property, but allow to continue
            size_t pos = strAmount.find(".");
            if (pos != std::string::npos) {
                std::string tmpStrAmount = strAmount.substr(0,pos);
                std::string strMsgText = "The amount entered contains a decimal however the property being transacted is indivisible.\n\n"
                                         "The amount entered will be truncated as follows:\n"
                                         "Original amount entered: " + strAmount + "\n"
                                         "Amount that will be transacted: " + tmpStrAmount + "\n\n"
                                         "Do you still wish to proceed with the transaction?";
                QMessageBox::StandardButton responseClick = QMessageBox::question(this, "Amount truncation warning", QString::fromStdString(strMsgText), QMessageBox::Yes|QMessageBox::No);
                if (responseClick == QMessageBox::No) {
                    QMessageBox::critical( this, "Transaction cancelled",
                    "The transaction has been cancelled.\n\nPlease double-check the transction details thoroughly before retrying your send transaction." );
                    return;
                }
                strAmount = tmpStrAmount;
                ui->amountLineEdit->setText(QString::fromStdString(strAmount));
            }
        }
        sendAmount = StrToInt64(strAmount, divisible);
        if (0>=sendAmount) {
            QMessageBox::critical( this, "Unable to send transaction",
            "The amount entered is not valid.\n\nPlease double-check the transction details thoroughly before retrying your send transaction." );
            return;
        }
    }

    // check if wallet is still syncing, as this will currently cause a lockup if we try to send - compare our chain to peers to see if we're up to date
    uint32_t intBlockDate = GetLatestBlockTime();  // uint32, not using time_t for portability
    QDateTime currentDate = QDateTime::currentDateTime();
    int secs = QDateTime::fromTime_t(intBlockDate).secsTo(currentDate);
    if (secs > 90*60) {
        QMessageBox::critical( this, "Unable to send transaction",
        "The client is still synchronizing.  Sending transactions can currently be performed only when the client has completed synchronizing." );
        return;
    }

    // check if it's a property creation, and if so evaluate the payload length against nMaxDatacarrierBytes and warn if metadata is too long
    if (typeId == MSC_TYPE_CREATE_PROPERTY_FIXED || typeId == MSC_TYPE_CREATE_PROPERTY_VARIABLE || typeId == MSC_TYPE_CREATE_PROPERTY_MANUAL) {
        std::vector<unsigned char> tmpPayload;
        if (typeId == MSC_TYPE_CREATE_PROPERTY_FIXED) {
            tmpPayload = CreatePayload_IssuanceFixed(ecosystem, propertyType, 0, strPropName, strPropURL, "", sendAmount);
        } else if (typeId == MSC_TYPE_CREATE_PROPERTY_VARIABLE) {
            tmpPayload = CreatePayload_IssuanceVariable(ecosystem, propertyType, 0, strPropName, strPropURL, "", propertyId, sendAmount, 255, 255, 255);
        } else if (typeId == MSC_TYPE_CREATE_PROPERTY_MANUAL) {
            tmpPayload = CreatePayload_IssuanceManaged(ecosystem, propertyType, 0, strPropName, strPropURL, "");
        }
        if (tmpPayload.size()+GetOmMarker().size() > nMaxDatacarrierBytes) {
            QMessageBox::critical( this, "Unable to send transaction",
            "The metadata entered is too long for the maximum payload size.  Please reduce the length of the URL and/or name and try your transaction again.");
            return;
        }
    }

    // ##### PER TX CHECKS & PAYLOAD CREATION #####
    std::vector<unsigned char> payload;
    std::string strMsgText = "You are about to send the following transaction, please check the details thoroughly:\n\n";
    if (typeId == MSC_TYPE_SIMPLE_SEND) {
        if (!sufficientBalance(fromAddress.ToString(), propertyId, sendAmount)) return;
        std::string propDetails = getPropertyName(propertyId) + getTokenLabel(propertyId);
        strMsgText += "Type: Simple Send\n\n";
        strMsgText += "From: " + fromAddress.ToString() + "\n";
        strMsgText += "To: " + refAddress.ToString() + "\n";
        strMsgText += "Property: " + propDetails + "\n";
        strMsgText += "Amount that will be sent: " + FormatByDivisibility(sendAmount, divisible) + "\n";
        payload = CreatePayload_SimpleSend(propertyId, sendAmount);
    }
    if (typeId == MSC_TYPE_CREATE_PROPERTY_FIXED) {
        strMsgText += "Type: Create Property (Fixed)\n\n";
        strMsgText += "From: " + fromAddress.ToString() + "\n";
        strMsgText += "Ecosystem: " + strEco + "\n";
        strMsgText += "Property Type: " + strPropType + "\n";
        strMsgText += "Property Name: " + strPropName + "\n";
        strMsgText += "Property URL: " + strPropURL + "\n";
        strMsgText += "Amout of Tokens: " + FormatByType(sendAmount, propertyType) + "\n";
        payload = CreatePayload_IssuanceFixed(ecosystem, propertyType, 0, strPropName, strPropURL, "", sendAmount);
    }
    if (typeId == MSC_TYPE_CREATE_PROPERTY_VARIABLE) {
        std::string strDeadline = ui->deadlineLE->text().toStdString();
        std::string strEarlyBonus = ui->earlybirdLE->text().toStdString();
        std::string strIssuerPercentage = ui->issuerLE->text().toStdString();
        int64_t deadline = StrToInt64(strDeadline, false);
        int64_t earlyBonus = StrToInt64(strEarlyBonus, false);
        int64_t issuerPercentage = StrToInt64(strIssuerPercentage, false);
        if (earlyBonus > 255 || issuerPercentage > 255) {
            QMessageBox::critical(this, "Unable to send transaction",
            "The deadline, early bonus and issuer percentage fields must be numeric.\n\nPlease double-check the transction details thoroughly before retrying your send transaction." );
            return;
        }
        std::string propDetails = getPropertyName(propertyId) + getTokenLabel(propertyId);
        strMsgText += "Type: Create Property (Variable)\n\n";
        strMsgText += "From: " + fromAddress.ToString() + "\n";
        strMsgText += "Ecosystem: " + strEco + "\n";
        strMsgText += "Property Type: " + strPropType + "\n";
        strMsgText += "Property Name: " + strPropName + "\n";
        strMsgText += "Property URL: " + strPropURL + "\n";
        strMsgText += "Desired Property: " + propDetails + "\n";
        strMsgText += "Deadline: " + strprintf("%d",deadline) + "\n";
        strMsgText += "Early Bonus %: " + strprintf("%d",earlyBonus) + "\n";
        strMsgText += "Issuer %: " + strprintf("%d",issuerPercentage) + "\n";
        strMsgText += "Amout Per Unit: " + FormatByType(sendAmount, propertyType) + "\n";
        payload = CreatePayload_IssuanceVariable(ecosystem, propertyType, 0, strPropName, strPropURL, "", propertyId, sendAmount, deadline, earlyBonus, issuerPercentage);
    }
    if (typeId == MSC_TYPE_CLOSE_CROWDSALE) {
        std::string propDetails = getPropertyName(propertyId) + getTokenLabel(propertyId);
        strMsgText += "Type: Close Crowdsale\n\n";
        strMsgText += "From: " + fromAddress.ToString() + "\n";
        strMsgText += "Property: " + propDetails + "\n";
        payload = CreatePayload_CloseCrowdsale(propertyId);
    }
    if (typeId == MSC_TYPE_CREATE_PROPERTY_MANUAL) {
        strMsgText += "Type: Create Property (Managed)\n\n";
        strMsgText += "From: " + fromAddress.ToString() + "\n";
        strMsgText += "Ecosystem: " + strEco + "\n";
        strMsgText += "Property Type: " + strPropType + "\n";
        strMsgText += "Property Name: " + strPropName + "\n";
        strMsgText += "Property URL: " + strPropURL + "\n";
        payload = CreatePayload_IssuanceManaged(ecosystem, propertyType, 0, strPropName, strPropURL, "");
    }
    if (typeId == MSC_TYPE_GRANT_PROPERTY_TOKENS) {
        std::string propDetails = getPropertyName(propertyId) + getTokenLabel(propertyId);
        strMsgText += "Type: Grant Tokens\n\n";
        strMsgText += "From: " + fromAddress.ToString() + "\n";
        if (!strRefAddress.empty()) strMsgText += "To: " + refAddress.ToString() + "\n";
        strMsgText += "Property: " + propDetails + "\n";
        strMsgText += "Amount that will be granted: " + FormatByDivisibility(sendAmount, divisible) + "\n";
        payload = CreatePayload_Grant(propertyId, sendAmount);
    }
    if (typeId == MSC_TYPE_REVOKE_PROPERTY_TOKENS) {
        if (!sufficientBalance(fromAddress.ToString(), propertyId, sendAmount)) return;
        std::string propDetails = getPropertyName(propertyId) + getTokenLabel(propertyId);
        strMsgText += "Type: Revoke Tokens\n\n";
        strMsgText += "From: " + fromAddress.ToString() + "\n";
        strMsgText += "Property: " + propDetails + "\n";
        strMsgText += "Amount that will be revoked: " + FormatByDivisibility(sendAmount, divisible) + "\n";
        payload = CreatePayload_Revoke(propertyId, sendAmount);
    }
    // ##### END PER TX CHECKS & PAYLOAD CREATION #####

    // request confirmation to send the transaction
    SendConfirmationDialog confirmationDialog(tr("Confirm Transaction"), QString::fromStdString(strMsgText), SEND_CONFIRM_DELAY, this);
    confirmationDialog.exec();
    QMessageBox::StandardButton retval = (QMessageBox::StandardButton)confirmationDialog.result();
    if (retval != QMessageBox::Yes) return;

    // unlock the wallet
    WalletModel::UnlockContext ctx(walletModel->requestUnlock());
    if(!ctx.isValid()) {
        QMessageBox::critical( this, "Send transaction failed",
        "The send transaction has been cancelled.\n\nThe wallet unlock process must be completed to send a transaction." );
        return; // unlock wallet was cancelled/failed
    }

    // request the wallet build the transaction (and if needed commit it) - note UI does not support added reference amounts currently
    uint256 txid;
    std::string rawHex;
    int result = -1;
    if (typeId == MSC_TYPE_SIMPLE_SEND || (typeId == MSC_TYPE_GRANT_PROPERTY_TOKENS && !strRefAddress.empty())) {
        result = WalletTxBuilder(fromAddress.ToString(), refAddress.ToString(), 0, payload, txid, rawHex, autoCommit);
    } else {
        result = WalletTxBuilder(fromAddress.ToString(), "", 0, payload, txid, rawHex, autoCommit);
    }

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        QMessageBox::critical( this, "Send transaction failed",
        "The send transaction has failed.\n\nThe error code was: " + QString::number(result) + "\nThe error message was:\n" + QString::fromStdString(error_str(result)));
        return;
    } else {
        if (!autoCommit) {
            PopulateSimpleDialog(rawHex, "Raw Hex (auto commit is disabled)", "Raw transaction hex");
        } else {
            if (typeId == MSC_TYPE_SIMPLE_SEND) {
                PendingAdd(txid, fromAddress.ToString(), MSC_TYPE_SIMPLE_SEND, propertyId, sendAmount);
            } else {
                PendingAdd(txid, fromAddress.ToString(), typeId, 0, sendAmount, false); // TODO - better pending support needed
            }
            PopulateTXSentDialog(txid.GetHex());
        }
    }

    clearFields();
}

void SendMPDialog::sendFromComboBoxChanged(int idx)
{
    updateFrom();
}

void SendMPDialog::propertyComboBoxChanged(int idx)
{
    updateProperty();
    updateFrom();
}

void SendMPDialog::hideAll()
{
    ui->amountLineEdit->hide();
    ui->balanceLabel->hide();
    ui->chkDivisible->hide();
    ui->propertyComboBox->hide();
    ui->wgtAmount->hide();
    ui->wgtCrowd->hide();
    ui->wgtFrom->hide();
    ui->wgtProp->hide();
    ui->wgtPropOptions->hide();
    ui->wgtTo->hide();
    ui->sendFromLabel->setText("Send From:");
    ui->sendToLabel->setText("Send To:");
    ui->amountLabel->setText("Amount:");
    #if QT_VERSION >= 0x040700
        ui->urlLE->setPlaceholderText("");
        ui->nameLE->setPlaceholderText("");
    #endif
}

void SendMPDialog::typeComboBoxChanged(int idx)
{
    QString typeIdStr = ui->typeCombo->itemData(ui->typeCombo->currentIndex()).toString();
    if (typeIdStr.toStdString().empty()) return;
    uint32_t typeId = typeIdStr.toUInt();

    hideAll();
    updatePropSelector();
    updateProperty();
    updateFrom();

    ui->typeDescLabel->setText(QString::fromStdString(GetLongDescription(typeId)));

    switch (typeId)
    {
        case MSC_TYPE_SIMPLE_SEND:
            ui->amountLineEdit->show();
            ui->wgtFrom->show();
            ui->wgtTo->show();
            ui->wgtAmount->show();
            ui->balanceLabel->show();
            ui->propertyComboBox->show();
            break;
        case MSC_TYPE_CREATE_PROPERTY_FIXED:
            ui->amountLineEdit->show();
            ui->wgtProp->show();
            ui->wgtPropOptions->show();
            ui->wgtAmount->show();
            ui->wgtFrom->show();
            ui->chkDivisible->show();
            #if QT_VERSION >= 0x040700
                ui->urlLE->setPlaceholderText("Optional");
                ui->nameLE->setPlaceholderText("Enter new property name");
            #endif
            break;
        case MSC_TYPE_CREATE_PROPERTY_VARIABLE:
            ui->wgtCrowd->show();
            ui->wgtProp->show();
            ui->wgtPropOptions->show();
            ui->wgtAmount->show();
            ui->amountLineEdit->show();
            ui->wgtFrom->show();
            ui->propertyComboBox->show();
            ui->chkDivisible->show();
            #if QT_VERSION >= 0x040700
                ui->urlLE->setPlaceholderText("Optional");
                ui->nameLE->setPlaceholderText("Enter new property name");
            #endif
            break;
        case MSC_TYPE_CLOSE_CROWDSALE:
            ui->wgtAmount->show();
            ui->amountLabel->setText("Property:");
            ui->wgtFrom->show();
            ui->propertyComboBox->show();
            break;
        case MSC_TYPE_CREATE_PROPERTY_MANUAL:
            ui->wgtProp->show();
            ui->wgtPropOptions->show();
            ui->wgtFrom->show();
            ui->chkDivisible->show();
            #if QT_VERSION >= 0x040700
                ui->urlLE->setPlaceholderText("Optional");
                ui->nameLE->setPlaceholderText("Enter new property name");
            #endif
            break;
        case MSC_TYPE_GRANT_PROPERTY_TOKENS:
            ui->sendFromLabel->setText("Grant From:");
            ui->sendToLabel->setText("Grant To:");
            ui->amountLineEdit->show();
            ui->wgtAmount->show();
            ui->wgtFrom->show();
            ui->wgtTo->show();
            ui->propertyComboBox->show();
            break;
        case MSC_TYPE_REVOKE_PROPERTY_TOKENS:
            ui->sendFromLabel->setText("Revoke From:");
            ui->amountLineEdit->show();
            ui->wgtAmount->show();
            ui->balanceLabel->show();
            ui->wgtFrom->show();
            ui->propertyComboBox->show();
            break;
        default:
            break;
    }
}

void SendMPDialog::divisChkClicked()
{
#if QT_VERSION >= 0x040700
    if (ui->chkDivisible->isChecked()) {
        ui->amountLineEdit->setPlaceholderText("Enter divisible Amount");
    } else {
        ui->amountLineEdit->setPlaceholderText("Enter indivisible Amount");
    }
#endif
}

void SendMPDialog::balancesUpdated()
{
    updatePropSelector();
    updateProperty();
    updateFrom();
}

bool SendMPDialog::txConfirmation(std::string strMsgText)
{
    strMsgText += "\n\nAre you sure you wish to send this transaction?";
    QString msgText = QString::fromStdString(strMsgText);

    QMessageBox::StandardButton responseClick;
    responseClick = QMessageBox::question(this, "Confirm transaction", msgText, QMessageBox::Yes|QMessageBox::No);

    if (responseClick == QMessageBox::Yes) {
        return true;
    }

    QMessageBox::critical(this, "Transaction cancelled",
    "The transaction has been cancelled.\n\nPlease double-check the transction details thoroughly before retrying your transaction." );
    return false;
}

bool SendMPDialog::sufficientBalance(std::string address, uint32_t propertyId, int64_t amount)
{
    int64_t balanceAvailable = getUserAvailableMPbalance(address, propertyId);

    if (amount <= balanceAvailable) {
        return true;
    }

    QMessageBox::critical(this, "Unable to send transaction",
    "The selected sending address does not have a sufficient balance to cover the amount entered.\n\nPlease double-check the transction details thoroughly before retrying your send transaction." );
    return false;
}

// cloned from sendcoinsdialog.cpp
void SendMPDialog::minimizeFeeSection(bool fMinimize)
{
    ui->labelFeeMinimized->setVisible(fMinimize);
    ui->buttonChooseFee  ->setVisible(fMinimize);
    ui->buttonMinimizeFee->setVisible(!fMinimize);
    ui->frameFeeSelection->setVisible(!fMinimize);
    ui->horizontalLayoutSmartFee->setContentsMargins(0, (fMinimize ? 0 : 6), 0, 0);
    fFeeMinimized = fMinimize;
}

void SendMPDialog::on_buttonChooseFee_clicked()
{
    minimizeFeeSection(false);
}

void SendMPDialog::on_buttonMinimizeFee_clicked()
{
    updateFeeMinimizedLabel();
    minimizeFeeSection(true);
}

void SendMPDialog::setMinimumFee()
{
    ui->radioCustomPerKilobyte->setChecked(true);
    ui->customFee->setValue(CWallet::GetRequiredFee(1000));
}

void SendMPDialog::updateFeeSectionControls()
{
    ui->sliderSmartFee          ->setEnabled(ui->radioSmartFee->isChecked());
    ui->labelSmartFee           ->setEnabled(ui->radioSmartFee->isChecked());
    ui->labelSmartFee2          ->setEnabled(ui->radioSmartFee->isChecked());
    ui->labelSmartFee3          ->setEnabled(ui->radioSmartFee->isChecked());
    ui->labelFeeEstimation      ->setEnabled(ui->radioSmartFee->isChecked());
    ui->labelSmartFeeNormal     ->setEnabled(ui->radioSmartFee->isChecked());
    ui->labelSmartFeeFast       ->setEnabled(ui->radioSmartFee->isChecked());
    ui->checkBoxMinimumFee      ->setEnabled(ui->radioCustomFee->isChecked());
    ui->labelMinFeeWarning      ->setEnabled(ui->radioCustomFee->isChecked());
    ui->radioCustomPerKilobyte  ->setEnabled(ui->radioCustomFee->isChecked() && !ui->checkBoxMinimumFee->isChecked());
    ui->radioCustomAtLeast      ->setEnabled(ui->radioCustomFee->isChecked() && !ui->checkBoxMinimumFee->isChecked());
    ui->customFee               ->setEnabled(ui->radioCustomFee->isChecked() && !ui->checkBoxMinimumFee->isChecked());
    //Q_EMIT feesUpdated();
}

void SendMPDialog::updateGlobalFeeVariables()
{
    if (ui->radioSmartFee->isChecked())
    {
        nTxConfirmTarget = defaultConfirmTarget - ui->sliderSmartFee->value();
        payTxFee = CFeeRate(0);
    }
    else
    {
        nTxConfirmTarget = defaultConfirmTarget;
        payTxFee = CFeeRate(ui->customFee->value());
    }
}

void SendMPDialog::updateFeeMinimizedLabel()
{
    if(!walletModel || !walletModel->getOptionsModel())
        return;

    if (ui->radioSmartFee->isChecked())
        ui->labelFeeMinimized->setText(ui->labelSmartFee->text());
    else {
        ui->labelFeeMinimized->setText(BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), ui->customFee->value()) +
            ((ui->radioCustomPerKilobyte->isChecked()) ? "/kB" : ""));
    }
}

void SendMPDialog::updateMinFeeLabel()
{
    if (walletModel && walletModel->getOptionsModel())
        ui->checkBoxMinimumFee->setText(tr("Pay only the required fee of %1").arg(
            BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), CWallet::GetRequiredFee(1000)) + "/kB")
        );
}

void SendMPDialog::updateSmartFeeLabel()
{
    if(!walletModel || !walletModel->getOptionsModel())
        return;

    int nBlocksToConfirm = defaultConfirmTarget - ui->sliderSmartFee->value();
    int estimateFoundAtBlocks = nBlocksToConfirm;
    CFeeRate feeRate = mempool.estimateSmartFee(nBlocksToConfirm, &estimateFoundAtBlocks);
    if (feeRate <= CFeeRate(0)) // not enough data => minfee
    {
        ui->labelSmartFee->setText(BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(),
                                                                std::max(CWallet::fallbackFee.GetFeePerK(), CWallet::GetRequiredFee(1000))) + "/kB");
        ui->labelSmartFee2->show(); // (Smart fee not initialized yet. This usually takes a few blocks...)
        ui->labelFeeEstimation->setText("");
    }
    else
    {
        ui->labelSmartFee->setText(BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(),
                                                                std::max(feeRate.GetFeePerK(), CWallet::GetRequiredFee(1000))) + "/kB");
        ui->labelSmartFee2->hide();
        ui->labelFeeEstimation->setText(tr("Estimated to begin confirmation within %n block(s).", "", estimateFoundAtBlocks));
    }

    updateFeeMinimizedLabel();

    //Q_EMIT feesUpdated();
}

/**
  * Helper to determine whether to show a property in the property selector combo
  */
bool SendMPDialog::FilterProperty(uint16_t typeId, uint32_t propertyId)
{
    if (typeId == MSC_TYPE_CREATE_PROPERTY_FIXED || typeId == MSC_TYPE_CREATE_PROPERTY_MANUAL) {
        return true;
    }

    if (typeId == MSC_TYPE_GRANT_PROPERTY_TOKENS || typeId == MSC_TYPE_REVOKE_PROPERTY_TOKENS) {
        CMPSPInfo::Entry sp;
        if (!_my_sps->getSP(propertyId, sp)) {
            return true;
        }
        if (!sp.manual) {
            return true;
        }
        if (typeId == MSC_TYPE_REVOKE_PROPERTY_TOKENS) {
            if (global_balance_money[propertyId] == 0) {
                return true;
            }
        }
        if (typeId == MSC_TYPE_GRANT_PROPERTY_TOKENS) {
            if (IsMyAddress(sp.issuer) != ISMINE_SPENDABLE) {
                return true;
            }
        }
    }

    if (typeId == MSC_TYPE_CLOSE_CROWDSALE) {
        CMPSPInfo::Entry sp;
        if (!_my_sps->getSP(propertyId, sp)) {
            return true;
        }
        if (sp.manual || sp.fixed) {
            return true;
        }
        if (!isCrowdsaleActive(propertyId)) {
            return true;
        }
        if (IsMyAddress(sp.issuer) != ISMINE_SPENDABLE) {
            return true;
        }
    }

    if (typeId == MSC_TYPE_SIMPLE_SEND) {
        if (global_balance_money[propertyId] == 0) {
            return true;
        }
    }

    return false;
}

/**
  * Helper to format a property for use in a combo box
  */
std::string SendMPDialog::propToStr(uint32_t propertyId)
{
    std::string spName = getPropertyName(propertyId);
    std::string spId = strprintf("%d", propertyId);

    if (spName.size()>23) {
        spName = spName.substr(0,23) + "...";
    }

    spName += " (#" + spId + ")";

    if (isPropertyDivisible(propertyId)) {
        spName += " [D]";
    } else {
        spName += " [I]";
    }

    return spName;
}

/**
  * Helper to show/hide the hint text
  */
void SendMPDialog::hintHelper(std::string text)
{
    if (!text.empty()) {
        ui->hintLabel->setText(QString::fromStdString(text));
    }
    if (bShowFeeHint || bShowEmptyHint) {
        ui->hintLabel->setVisible(true);
    } else {
        ui->hintLabel->setVisible(false);
    }
}

/**
  * Helper to clear text fields
  */
void SendMPDialog::clearFields()
{
    ui->sendToLineEdit->setText("");
    ui->amountLineEdit->setText("");
    ui->nameLE->setText("");
    ui->urlLE->setText("");
    ui->deadlineLE->setText("");
    ui->earlybirdLE->setText("");
    ui->issuerLE->setText("");
}

/**
  * Helper to determine if an address holds any tokens
  */
bool SendMPDialog::HasTokens(std::string address)
{
    LOCK(cs_tally);

    CMPTally* addressTally = getTally(address);

    if (NULL == addressTally) {
        return false;
    }

    addressTally->init();

    uint32_t propertyId = 0;
    while (0 != (propertyId = addressTally->next())) {
        if (addressTally->getMoney(propertyId, BALANCE) > 0) {
            return true;
        }
    }

    return false;
}

/**
  * Helper to determine the currently selected transaction type
  */
uint16_t SendMPDialog::GetCurrentType()
{
    QString typeIdStr = ui->typeCombo->itemData(ui->typeCombo->currentIndex()).toString();
    if (typeIdStr.toStdString().empty()) {
        return 9999; // non existent transaction type used as magic number, avoid
    }
    uint16_t typeId = typeIdStr.toUInt();
    return typeId;
}

/**
  * Helper to determine the currently selected property id
  */
uint32_t SendMPDialog::GetCurrentPropertyId()
{
    QString spId = ui->propertyComboBox->itemData(ui->propertyComboBox->currentIndex()).toString();
    if (spId.toStdString().empty()) {
        return 0;
    }
    uint32_t propertyId = spId.toUInt();

    return propertyId;
}

/**
  * Helper to update the fee display when the settings have been changed in another tab
  */
void SendMPDialog::refreshFeeDetails()
{
    updateSmartFeeLabel();
    updateFeeSectionControls();
}
