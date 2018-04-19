// Copyright (c) 2011-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "overviewpage.h"
#include "ui_overviewpage.h"

#include "bitcoinunits.h"
#include "clientmodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "platformstyle.h"
#include "transactionfilterproxy.h"
#include "transactiontablemodel.h"
#include "walletmodel.h"

#include "omnicore/activation.h"
#include "omnicore/notifications.h"
#include "omnicore/omnicore.h"
#include "omnicore/rules.h"
#include "omnicore/sp.h"
#include "omnicore/tx.h"
#include "omnicore/pending.h"
#include "omnicore/utilsbitcoin.h"
#include "omnicore/wallettxs.h"

#include "main.h"
#include "sync.h"

#include <sstream>
#include <string>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include <QAbstractItemDelegate>
#include <QBrush>
#include <QColor>
#include <QDateTime>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidgetItem>
#include <QPainter>
#include <QRect>
#include <QString>
#include <QStyleOptionViewItem>
#include <QVariant>
#include <QVBoxLayout>
#include <QWidget>

//TODO - Add icons to buttons

using std::ostringstream;
using std::string;

using namespace mastercore;

#define DECORATION_SIZE 64
#define NUM_ITEMS 6

struct OverviewCacheEntry
{
    OverviewCacheEntry()
      : address("unknown"), amount("0.0000000"), valid(false), sendToSelf(false), outbound(false)
    {}

    OverviewCacheEntry(const QString& addressIn, const QString& amountIn, bool validIn, bool sendToSelfIn, bool outboundIn)
      : address(addressIn), amount(amountIn), valid(validIn), sendToSelf(sendToSelfIn), outbound(outboundIn)
    {}

    QString address;
    QString amount;
    bool valid;
    bool sendToSelf;
    bool outbound;
};


std::map<uint256, OverviewCacheEntry> recentCache;

class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    TxViewDelegate(const PlatformStyle *platformStyle, QObject *parent=nullptr):
        QAbstractItemDelegate(parent), unit(BitcoinUnits::BTC),
        platformStyle(platformStyle)
    {

    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const
    {
        painter->save();

        QIcon icon = qvariant_cast<QIcon>(index.data(TransactionTableModel::RawDecorationRole));
        QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
        QString address = index.data(Qt::DisplayRole).toString();
        qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
        bool confirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();
        QVariant value = index.data(Qt::ForegroundRole);

        QRect mainRect = option.rect;
        QRect decorationRect(mainRect.topLeft(), QSize(DECORATION_SIZE, DECORATION_SIZE));
        int xspace = DECORATION_SIZE + 8;
        int ypad = 6;
        int halfheight = (mainRect.height() - 2*ypad)/2;
        QRect amountRect(mainRect.left() + xspace, mainRect.top()+ypad, mainRect.width() - xspace, halfheight);
        QRect addressRect(mainRect.left() + xspace, mainRect.top()+ypad+halfheight, mainRect.width() - xspace, halfheight);

            /** Rather ugly way to provide recent transaction display support - each time we paint a transaction we will check if it's Omni and override
                the values if so.  Data is cached to improve performance.  Short-term hack until a better appraoch is devised. **/
            uint256 hash;
            hash.SetHex(index.data(TransactionTableModel::TxIDRole).toString().toStdString());
            bool omniOverride = false, omniSendToSelf = false, valid = false, omniOutbound = true;
            QString omniAmountStr;
            {
                LOCK(cs_pending);
                PendingMap::iterator it = my_pending.find(hash);
                if (it != my_pending.end()) {
                    omniOverride = true;
                    valid = true; // assume all outbound pending are valid prior to confirmation
                    CMPPending *p_pending = &(it->second);
                    address = QString::fromStdString(p_pending->src);
                    if (p_pending->type == MSC_TYPE_CREATE_PROPERTY_FIXED) {
                        omniAmountStr = QString::fromStdString(FormatByType(p_pending->amount, 1) +  " NEW TOKENS");
                    } else if (p_pending->type == MSC_TYPE_CREATE_PROPERTY_MANUAL || p_pending->type == MSC_TYPE_CLOSE_CROWDSALE) {
                        omniAmountStr = QString::fromStdString("N/A");
                    } else {
                        omniAmountStr = QString::fromStdString(FormatMP(p_pending->prop, p_pending->amount) + getTokenLabel(p_pending->prop));
                    }
                }
            }
            std::map<uint256, OverviewCacheEntry>::iterator cacheIt = recentCache.find(hash);
            if (cacheIt != recentCache.end()) {
                OverviewCacheEntry txEntry = cacheIt->second;
                address = txEntry.address;
                valid = txEntry.valid;
                omniSendToSelf = txEntry.sendToSelf;
                omniOutbound = txEntry.outbound;
                omniAmountStr = txEntry.amount;
                omniOverride = true;
                amount = 0;
            } else if (p_txlistdb->exists(hash)) {
                omniOverride = true;
                amount = 0;
                CTransaction wtx;
                uint256 blockHash;
                if (GetTransaction(hash, wtx, Params().GetConsensus(), blockHash, true)) {
                    if (!blockHash.IsNull() || NULL == GetBlockIndex(blockHash)) {
                        CBlockIndex* pBlockIndex = GetBlockIndex(blockHash);
                        if (NULL != pBlockIndex) {
                            int blockHeight = pBlockIndex->nHeight;
                            CMPTransaction mp_obj;
                            int parseRC = ParseTransaction(wtx, blockHeight, 0, mp_obj);
                            if (0 == parseRC) {
                                if (mp_obj.interpret_Transaction()) {
                                    valid = getValidMPTX(hash);
                                    if (mp_obj.getType() == MSC_TYPE_CLOSE_CROWDSALE || mp_obj.getType() == MSC_TYPE_CREATE_PROPERTY_MANUAL || mp_obj.getType() == MSC_TYPE_CREATE_PROPERTY_VARIABLE) {
                                        omniAmountStr = "N/A";
                                    } else {
                                        std::string tokenLabel = (mp_obj.getProperty() == 0) ? " TOKENS" : getTokenLabel(mp_obj.getProperty());
                                        if (mp_obj.getType() == MSC_TYPE_CREATE_PROPERTY_FIXED) {
                                            omniAmountStr = QString::fromStdString(FormatByType(mp_obj.getAmount(), mp_obj.getPropertyType()) + tokenLabel);
                                        } else {
                                            omniAmountStr = QString::fromStdString(FormatMP(mp_obj.getProperty(), mp_obj.getAmount()) + tokenLabel);
                                        }
                                    }
                                    address = QString::fromStdString(mp_obj.getSender());
                                    if (!mp_obj.getReceiver().empty()) {
                                        if (IsMyAddress(mp_obj.getReceiver())) {
                                            address = QString::fromStdString(mp_obj.getReceiver());
                                            omniOutbound = false;
                                            if (IsMyAddress(mp_obj.getSender())) {
                                                omniSendToSelf = true;
                                            }
                                        }
                                    }
                                }
                                OverviewCacheEntry newEntry;
                                newEntry.valid = valid;
                                newEntry.sendToSelf = omniSendToSelf;
                                newEntry.outbound = omniOutbound;
                                newEntry.address = address;
                                newEntry.amount = omniAmountStr;
                                recentCache.insert(std::make_pair(hash, newEntry));
                            }
                        }
                    }
                }
            }
            if (omniOverride) {
                if (!valid) {
                    icon = QIcon(":/icons/omnitxinvalid");
                } else {
                    icon = QIcon(":/icons/omnitxout");
                    if (!omniOutbound) icon = QIcon(":/icons/omnitxin");
                    if (omniSendToSelf) icon = QIcon(":/icons/omnitxinout");
                }
            }
            /** End temp override for Omni transactions **/


        icon = platformStyle->SingleColorIcon(icon);
        icon.paint(painter, decorationRect);

        QColor foreground = option.palette.color(QPalette::Text);
        if(value.canConvert<QBrush>())
        {
            QBrush brush = qvariant_cast<QBrush>(value);
            foreground = brush.color();
        }

        painter->setPen(foreground);
        QRect boundingRect;
        painter->drawText(addressRect, Qt::AlignLeft|Qt::AlignVCenter, address, &boundingRect);

        if (index.data(TransactionTableModel::WatchonlyRole).toBool())
        {
            QIcon iconWatchonly = qvariant_cast<QIcon>(index.data(TransactionTableModel::WatchonlyDecorationRole));
            QRect watchonlyRect(boundingRect.right() + 5, mainRect.top()+ypad+halfheight, 16, halfheight);
            iconWatchonly.paint(painter, watchonlyRect);
        }

        if(amount < 0)
        {
            foreground = COLOR_NEGATIVE;
        }
        else if(!confirmed)
        {
            foreground = COLOR_UNCONFIRMED;
        }
        else
        {
            foreground = option.palette.color(QPalette::Text);
        }
        painter->setPen(foreground);
        QString amountText;
        if (!omniOverride) {
            amountText = BitcoinUnits::formatWithUnit(unit, amount, true, BitcoinUnits::separatorAlways);
        } else {
            amountText = omniAmountStr;
        }
        if(!confirmed)
        {
            amountText = QString("[") + amountText + QString("]");
        }
        painter->drawText(amountRect, Qt::AlignRight|Qt::AlignVCenter, amountText);

        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(amountRect, Qt::AlignLeft|Qt::AlignVCenter, GUIUtil::dateTimeStr(date));

        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        return QSize(DECORATION_SIZE, DECORATION_SIZE);
    }

    int unit;
    const PlatformStyle *platformStyle;

};
#include "overviewpage.moc"

OverviewPage::OverviewPage(const PlatformStyle *platformStyle, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::OverviewPage),
    clientModel(0),
    walletModel(0),
    currentBalance(-1),
    currentUnconfirmedBalance(-1),
    currentImmatureBalance(-1),
    currentWatchOnlyBalance(-1),
    currentWatchUnconfBalance(-1),
    currentWatchImmatureBalance(-1),
    txdelegate(new TxViewDelegate(platformStyle, this))
{
    ui->setupUi(this);

    // use a SingleColorIcon for the "out of sync warning" icon
    QIcon icon = platformStyle->SingleColorIcon(":/icons/warning");
    icon.addPixmap(icon.pixmap(QSize(64,64), QIcon::Normal), QIcon::Disabled); // also set the disabled icon because we are using a disabled QPushButton to work around missing HiDPI support of QLabel (https://bugreports.qt.io/browse/QTBUG-42503)
    ui->labelTransactionsStatus->setIcon(icon);
    ui->labelWalletStatus->setIcon(icon);
    ui->labelSPStatus->setIcon(icon);

    // Recent transactions
    ui->listTransactions->setItemDelegate(txdelegate);
    ui->listTransactions->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listTransactions->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);

    connect(ui->listTransactions, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTransactionClicked(QModelIndex)));

    // init "out of sync" warning labels
    ui->labelSPStatus->setText("(" + tr("out of sync") + ")");
    ui->labelWalletStatus->setText("(" + tr("out of sync") + ")");
    ui->labelTransactionsStatus->setText("(" + tr("out of sync") + ")");

    // make sure LTC and MSC are always first in the list by adding them first
    UpdatePropertyBalance(0,0);
    UpdatePropertyBalance(1,0);

    updateOmni();

    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);
}

void OverviewPage::handleTransactionClicked(const QModelIndex &index)
{
    // is this an Omni transaction that has been clicked?  Use pending & cache to find out quickly
    uint256 hash;
    hash.SetHex(index.data(TransactionTableModel::TxIDRole).toString().toStdString());
    bool omniTx = false;
    {
        LOCK(cs_pending);

        PendingMap::iterator it = my_pending.find(hash);
        if (it != my_pending.end()) omniTx = true;
    }
    std::map<uint256, OverviewCacheEntry>::iterator cacheIt = recentCache.find(hash);
    if (cacheIt != recentCache.end()) omniTx = true;

    // override if it's an Omni transaction
    if (omniTx) {
        // TODO emit omniTransactionClicked(hash);
    } else {
        // TODO if (filter) emit transactionClicked(filter->mapToSource(index));
    }
}

OverviewPage::~OverviewPage()
{
    delete ui;
}

void OverviewPage::UpdatePropertyBalance(unsigned int propertyId, uint64_t available, uint64_t reserved)
{
    if (propertyId < 2) return;

    // look for this property, does it already exist in overview and if so are the balances correct?
    int existingItem = -1;
    for(int i=0; i < ui->overviewLW->count(); i++) {
        uint64_t itemPropertyId = ui->overviewLW->item(i)->data(Qt::UserRole + 1).value<uint64_t>();
        if (itemPropertyId == propertyId) {
            uint64_t itemAvailableBalance = ui->overviewLW->item(i)->data(Qt::UserRole + 2).value<uint64_t>();
            if (available == itemAvailableBalance) {
                return; // norhing more to do, balance exists and is up to date
            } else {
                existingItem = i;
                break;
            }
        }
    }

    // this property doesn't exist in overview, create an entry for it
    QWidget *listItem = new QWidget();
    QVBoxLayout *vlayout = new QVBoxLayout();
    QHBoxLayout *hlayout = new QHBoxLayout();

    // property label
    string spName = getPropertyName(propertyId).c_str();
    if(spName.size()>22) spName=spName.substr(0,22)+"...";
    spName += strprintf(" (#%d)", propertyId);

    // property details
    bool divisible = isPropertyDivisible(propertyId);
    string tokenStr = getTokenLabel(propertyId);

    QVBoxLayout *vlayoutleft = new QVBoxLayout();
    QVBoxLayout *vlayoutright = new QVBoxLayout();

    // Left Panel
    QLabel *balTotalLabel = new QLabel(QString::fromStdString(spName));
    vlayoutleft->addWidget(balTotalLabel);

    // Right panel
    QLabel *balTotalLabelAmount = new QLabel();
    if (divisible) {
        balTotalLabelAmount->setText(QString::fromStdString(FormatDivisibleMP(available) + " SPT"));
    } else {
        balTotalLabelAmount->setText(QString::fromStdString(FormatIndivisibleMP(available)) + " SPT");
    }
    balTotalLabelAmount->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
//    balTotalLabelAmount->setStyleSheet("QLabel { padding-right:2px; }");
    vlayoutright->addWidget(balTotalLabelAmount);

    // put together
    vlayoutleft->addSpacerItem(new QSpacerItem(1,1,QSizePolicy::Fixed,QSizePolicy::Expanding));
    vlayoutright->addSpacerItem(new QSpacerItem(1,1,QSizePolicy::Fixed,QSizePolicy::Expanding));
    vlayoutleft->setContentsMargins(0,0,0,0);
    vlayoutright->setContentsMargins(0,0,0,0);
    vlayoutleft->setMargin(0);
    vlayoutright->setMargin(0);
    vlayoutleft->setSpacing(3);
    vlayoutright->setSpacing(3);
    hlayout->addLayout(vlayoutleft);
    hlayout->addSpacerItem(new QSpacerItem(1,1,QSizePolicy::Expanding,QSizePolicy::Fixed));
    hlayout->addLayout(vlayoutright);
    hlayout->setContentsMargins(0,0,0,0);
    vlayout->addLayout(hlayout);
    vlayout->addSpacerItem(new QSpacerItem(1,10,QSizePolicy::Fixed,QSizePolicy::Fixed));
    vlayout->setMargin(0);
    vlayout->setSpacing(3);
    listItem->setLayout(vlayout);
    listItem->setContentsMargins(0,0,0,0);
    listItem->layout()->setContentsMargins(0,0,0,0);

    // set data
    if(existingItem == -1) { // new
        QListWidgetItem *item = new QListWidgetItem();
        item->setData(Qt::UserRole + 1, QVariant::fromValue<qulonglong>(propertyId));
        item->setData(Qt::UserRole + 2, QVariant::fromValue<qulonglong>(available));
        item->setSizeHint(QSize(0,listItem->sizeHint().height())); // resize
        ui->overviewLW->addItem(item);
        ui->overviewLW->setItemWidget(item, listItem);
    } else {
        ui->overviewLW->item(existingItem)->setData(Qt::UserRole + 2, QVariant::fromValue<qulonglong>(available));
        ui->overviewLW->setItemWidget(ui->overviewLW->item(existingItem), listItem);
    }
}

void OverviewPage::reinitOmni()
{
    recentCache.clear();
    ui->overviewLW->clear();
    if (walletModel != NULL) {
        UpdatePropertyBalance(0, walletModel->getBalance()); //, walletModel->getUnconfirmedBalance());
    }
    UpdatePropertyBalance(1, 0);
    updateOmni();
}

void OverviewPage::updateOmni()
{
    LOCK(cs_tally);

    // always show MSC
    UpdatePropertyBalance(1,global_balance_money[1]);
    // loop properties and update overview
    unsigned int propertyId;
    unsigned int maxPropIdMainEco = GetNextPropertyId(true);  // these allow us to end the for loop at the highest existing
    unsigned int maxPropIdTestEco = GetNextPropertyId(false); // property ID rather than a fixed value like 100000 (optimization)
    // main eco
    for (propertyId = 2; propertyId < maxPropIdMainEco; propertyId++) {
        if (global_balance_money[propertyId] > 0) {
            UpdatePropertyBalance(propertyId,global_balance_money[propertyId]);
        }
    }
    // test eco
    for (propertyId = 2147483647; propertyId < maxPropIdTestEco; propertyId++) {
        if (global_balance_money[propertyId] > 0) {
            UpdatePropertyBalance(propertyId,global_balance_money[propertyId]);
        }
    }
}

void OverviewPage::setBalance(const CAmount& balance, const CAmount& unconfirmedBalance, const CAmount& immatureBalance, const CAmount& watchOnlyBalance, const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance)
{
    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    currentBalance = balance;
    currentUnconfirmedBalance = unconfirmedBalance;
    ui->labelBalance->setText(BitcoinUnits::formatWithUnit(unit, balance, false, BitcoinUnits::separatorAlways));
    ui->labelUnconfirmed->setText(BitcoinUnits::formatWithUnit(unit, unconfirmedBalance, false, BitcoinUnits::separatorAlways));
    ui->labelTotal->setText(BitcoinUnits::formatWithUnit(unit, balance + unconfirmedBalance + immatureBalance, false, BitcoinUnits::separatorAlways));
}

// show/hide watch-only labels
void OverviewPage::updateWatchOnlyLabels(bool showWatchOnly)
{
    // Omni Core does not currently fully support watch only
}

void OverviewPage::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if(model)
    {
        // Show warning if this is a prerelease version
        connect(model, SIGNAL(alertsChanged(QString)), this, SLOT(updateAlerts(QString)));
        updateAlerts(model->getStatusBarWarnings());

        // Refresh Omni info if there have been Omni layer transactions with balances affecting wallet
        connect(model, SIGNAL(refreshOmniBalance()), this, SLOT(updateOmni()));

        // Reinit Omni info if there has been a chain reorg
        connect(model, SIGNAL(reinitOmniState()), this, SLOT(reinitOmni()));

        // Refresh alerts when there has been a change to the Omni State
        connect(model, SIGNAL(refreshOmniState()), this, SLOT(updateAlerts()));
    }
}

void OverviewPage::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
    if(model && model->getOptionsModel())
    {
        // Set up transaction list
        filter.reset(new TransactionFilterProxy());
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setLimit(NUM_ITEMS);
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->setShowInactive(false);
        filter->sort(TransactionTableModel::Date, Qt::DescendingOrder);

        ui->listTransactions->setModel(filter.get());
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        // Keep up to date with wallet
        setBalance(model->getBalance(), model->getUnconfirmedBalance(), model->getImmatureBalance(),
                   model->getWatchBalance(), model->getWatchUnconfirmedBalance(), model->getWatchImmatureBalance());
        connect(model, SIGNAL(balanceChanged(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)), this, SLOT(setBalance(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)));

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));

        updateWatchOnlyLabels(model->haveWatchOnly());
        connect(model, SIGNAL(notifyWatchonlyChanged(bool)), this, SLOT(updateWatchOnlyLabels(bool)));
    }

    // update the display unit, to not use the default ("BTC")
    updateDisplayUnit();
}

void OverviewPage::updateDisplayUnit()
{
    if(walletModel && walletModel->getOptionsModel())
    {
        if(currentBalance != -1)
            setBalance(currentBalance, currentUnconfirmedBalance, currentImmatureBalance,
                       currentWatchOnlyBalance, currentWatchUnconfBalance, currentWatchImmatureBalance);

        // Update txdelegate->unit with the current unit
        txdelegate->unit = walletModel->getOptionsModel()->getDisplayUnit();

        ui->listTransactions->update();
    }
}

void OverviewPage::updateAlerts(const QString &warnings)
{
    QString alertString = warnings; // get current bitcoin alert/warning directly

    // get alert messages
    std::vector<std::string> omniAlerts = GetOmniCoreAlertMessages();
    for (std::vector<std::string>::iterator it = omniAlerts.begin(); it != omniAlerts.end(); it++) {
        if (!alertString.isEmpty()) alertString += "\n";
        alertString += QString::fromStdString(*it);
    }

    // get activations
    std::vector<FeatureActivation> vecPendingActivations = GetPendingActivations();
    for (std::vector<FeatureActivation>::iterator it = vecPendingActivations.begin(); it != vecPendingActivations.end(); ++it) {
        if (!alertString.isEmpty()) alertString += "\n";
        FeatureActivation pendingAct = *it;
        alertString += QString::fromStdString(strprintf("Feature %d ('%s') will go live at block %d",
                                                  pendingAct.featureId, pendingAct.featureName, pendingAct.activationBlock));
    }
    int currentHeight = GetHeight();
    std::vector<FeatureActivation> vecCompletedActivations = GetCompletedActivations();
    for (std::vector<FeatureActivation>::iterator it = vecCompletedActivations.begin(); it != vecCompletedActivations.end(); ++it) {
        if (currentHeight > (*it).activationBlock+1024) continue; // don't include after live+1024 blocks
        if (!alertString.isEmpty()) alertString += "\n";
        FeatureActivation completedAct = *it;
        alertString += QString::fromStdString(strprintf("Feature %d ('%s') is now live.", completedAct.featureId, completedAct.featureName));
    }

    if (!alertString.isEmpty()) {
        this->ui->labelAlerts->setVisible(true);
        this->ui->labelAlerts->setText(alertString);
    } else {
        this->ui->labelAlerts->setVisible(false);
        this->ui->labelAlerts->setText("");
    }
}

void OverviewPage::showOutOfSyncWarning(bool fShow)
{
    ui->labelSPStatus->setVisible(fShow);
    ui->labelWalletStatus->setVisible(fShow);
    ui->labelTransactionsStatus->setVisible(fShow);
}
