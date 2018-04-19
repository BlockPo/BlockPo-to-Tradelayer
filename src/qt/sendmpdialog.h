// Copyright (c) 2011-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SENDMPDIALOG_H
#define SENDMPDIALOG_H

#include "walletmodel.h"

#include <QDialog>
#include <QString>

class ClientModel;

QT_BEGIN_NAMESPACE
class QWidget;
QT_END_NAMESPACE

namespace Ui {
    class SendMPDialog;
}

/** Dialog for sending Master Protocol tokens */
class SendMPDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SendMPDialog(const PlatformStyle *platformStyle, QWidget *parent = 0);
    ~SendMPDialog();

    void setClientModel(ClientModel *model);
    void setWalletModel(WalletModel *model);

    uint16_t GetCurrentType();
    uint32_t GetCurrentPropertyId();
    bool FilterProperty(uint16_t typeId, uint32_t propertyId);
    bool HasTokens(std::string address);
    std::string propToStr(uint32_t propertyId);

    void updateProperty();
    void updatePropSelector();
    void hintHelper(std::string text = "");

    bool txConfirmation(std::string strMsgText);
    bool sufficientBalance(std::string address, uint32_t propertyId, int64_t amount);
    void hideAll();

public Q_SLOTS:
    void typeComboBoxChanged(int idx);
    void propertyComboBoxChanged(int idx);
    void sendFromComboBoxChanged(int idx);
    void sendOmniTransaction();
    void balancesUpdated();
    void updateFrom();
    void divisChkClicked();
    void clearFields();
    void refreshFeeDetails();

private:
    Ui::SendMPDialog *ui;
    ClientModel *clientModel;
    WalletModel *walletModel;
    const PlatformStyle *platformStyle;
    bool fFeeMinimized;
    void minimizeFeeSection(bool fMinimize);
    void updateFeeMinimizedLabel();

private Q_SLOTS:
    void on_buttonChooseFee_clicked();
    void on_buttonMinimizeFee_clicked();
    void setMinimumFee();
    void updateFeeSectionControls();
    void updateMinFeeLabel();
    void updateSmartFeeLabel();
    void updateGlobalFeeVariables();

Q_SIGNALS:
    // Fired when a message should be reported to the user
    void message(const QString &title, const QString &message, unsigned int style);
    void feesUpdated();
};

#endif // SENDMPDIALOG_H





