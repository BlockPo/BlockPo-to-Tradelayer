#include "omnicore_qtutils.h"

#include "guiutil.h"

#include "omnicore/omnicore.h"

#include <string>

#include <QDialog>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QObject>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace mastercore
{

/**
 * Displays a 'transaction sent' message box containing the transaction ID and an extra button to copy txid to clipboard
 */
void PopulateTXSentDialog(const std::string& txidStr)
{
    std::string strSentText = "Your Omni Layer transaction has been sent.\n\nThe transaction ID is:\n\n" + txidStr + "\n\n";
    QMessageBox sentDialog;
    sentDialog.setIcon(QMessageBox::Information);
    sentDialog.setWindowTitle("Transaction broadcast successfully");
    sentDialog.setText(QString::fromStdString(strSentText));
    sentDialog.setStandardButtons(QMessageBox::Yes|QMessageBox::Ok);
    sentDialog.setDefaultButton(QMessageBox::Ok);
    sentDialog.setButtonText( QMessageBox::Yes, "OK + Copy TXID" );
    if(sentDialog.exec() == QMessageBox::Yes) GUIUtil::setClipboard(QString::fromStdString(txidStr));
}

/**
 * Displays a simple dialog layout that can be used to provide selectable text to the user
 *
 * Note: used in place of standard dialogs in cases where text selection & copy to clipboard functions are useful
 */
void PopulateSimpleDialog(const std::string& content, const std::string& title, const std::string& tooltip)
{
    QDialog *simpleDlg = new QDialog;
    QLayout *dlgLayout = new QVBoxLayout;
    dlgLayout->setSpacing(12);
    dlgLayout->setMargin(12);
    QTextEdit *dlgTextEdit = new QTextEdit;
    dlgTextEdit->setText(QString::fromStdString(content));
    dlgTextEdit->setStatusTip(QString::fromStdString(tooltip));
    dlgTextEdit->setReadOnly(true);
    dlgTextEdit->setTextInteractionFlags(dlgTextEdit->textInteractionFlags() | Qt::TextSelectableByKeyboard);
    dlgLayout->addWidget(dlgTextEdit);
    QPushButton *closeButton = new QPushButton(QObject::tr("&Close"));
    closeButton->setDefault(true);
    QDialogButtonBox *buttonBox = new QDialogButtonBox;
    buttonBox->addButton(closeButton, QDialogButtonBox::AcceptRole);
    dlgLayout->addWidget(buttonBox);
    QObject::connect(buttonBox, SIGNAL(accepted()), simpleDlg, SLOT(accept()));
    simpleDlg->setAttribute(Qt::WA_DeleteOnClose);
    simpleDlg->setWindowTitle(QString::fromStdString(title));
    simpleDlg->setLayout(dlgLayout);
    simpleDlg->resize(700, 360);
    if (simpleDlg->exec() == QDialog::Accepted) { } //do nothing but close
}

/**
 * Strips trailing zeros from a string containing a divisible value
 */
std::string StripTrailingZeros(const std::string& inputStr)
{
    size_t dot = inputStr.find(".");
    std::string outputStr = inputStr; // make a copy we will manipulate and return
    if (dot==std::string::npos) { // could not find a decimal marker, unsafe - return original input string
        return inputStr;
    }
    size_t lastZero = outputStr.find_last_not_of('0') + 1;
    if (lastZero > dot) { // trailing zeros are after decimal marker, safe to remove
        outputStr.erase ( lastZero, std::string::npos );
        if (outputStr.length() > 0) { std::string::iterator it = outputStr.end() - 1; if (*it == '.') { outputStr.erase(it); } } //get rid of trailing dot if needed
    } else { // last non-zero is before the decimal marker, this is a whole number
        outputStr.erase ( dot, std::string::npos );
    }
    return outputStr;
}

/**
 * Truncates a string at n digits and adds "..." to indicate that display is incomplete
 */
std::string TruncateString(const std::string& inputStr, unsigned int length)
{
    if (inputStr.empty()) return "";
    std::string outputStr = inputStr;
    if (length > 0 && inputStr.length() > length) {
        outputStr = inputStr.substr(0, length) + "...";
    }
    return outputStr;
}

/**
 * Variable length find and replace.  Find all iterations of findText within inputStr and replace them
 * with replaceText.
 */
std::string ReplaceStr(const std::string& findText, const std::string& replaceText, const std::string& inputStr)
{
    size_t start_pos = 0;
    std::string outputStr = inputStr;
    while((start_pos = outputStr.find(findText, start_pos)) != std::string::npos) {
        outputStr.replace(start_pos, findText.length(), replaceText);
        start_pos += replaceText.length();
    }
    return outputStr;
}

/**
 * Long descriptions of transaction types.
 */
std::string GetLongDescription(uint16_t txType)
{
   switch (txType) {
        case MSC_TYPE_SIMPLE_SEND:
            return "The Simple Send transaction type transfers a specific number of tokens of the "
                   "selected property from the sender to the recipient.\n\n"
                   "Please choose the property you wish to send, then select the address you would "
                   "like to send from.  Enter the recipient address in the 'Send To' field and the "
                   "number of tokens you would like to send in the 'Amount' field.";
        case MSC_TYPE_SEND_ALL:
            return "The Send All transaction type transfers ALL available tokens in the selected "
                   "ecosystem from the sender to the recipient.\n\n"
                   "Please choose the address you would like to send from and enter the recipient "
                   "address in the 'Send To' field.\n\n"
                   "Use with caution!";
        case MSC_TYPE_CREATE_PROPERTY_FIXED:
            return "The Fixed Issuance transaction type creates a new property with a fixed number "
                   "of tokens.  The new tokens are credited to the sender.\n\n"
                   "Please choose a name for your property and enter the amount of tokens you would "
                   "like to create.  You will also need to decide whether you would like your new "
                   "tokens to be divisible.  You may also include a short URL to provide interested parties "
                   "with further details on your new property (optional).";
        case MSC_TYPE_CREATE_PROPERTY_VARIABLE:
            return "The Crowdsale Issuance transaction type creates a new property with the number of "
                   "tokens to be determined by a crowdsale.\n\n"
                   "Please choose a name for your property.  You may also include a short URL to provide "
                   "interested parties with further details on your new property (optional).  You will "
                   "also need to decide whether you would like your new property to be divisible.\n\n"
                   "Enter the crowdsale deadline as a unix time stamp, and provide your chosen early "
                   "bonus and issuer percentage values.  Enter the amount of tokens to issue per unit paid "
                   "in the amount field and select the property you would like to receive payment in.";
        case MSC_TYPE_CLOSE_CROWDSALE:
            return "The Close Crowdsale transaction type closes an active crowdsale, finalizing the "
                   "number of tokens for the property and preventing the recognition of further purchases.\n\n"
                   "Please choose the property you would like to close the crowdsale for.";
        case MSC_TYPE_CREATE_PROPERTY_MANUAL:
            return "The Managed Issuance transaction type creates a new property for issuer-managed "
                   "tokens.  No tokens are created.\n\n"
                   "Please choose a name for your property.  You will also need to decide whether you "
                   "would like your new property to be divisible.  You may also include a short URL to provide "
                   "interested parties with further details on your new property (optional).";
        case MSC_TYPE_GRANT_PROPERTY_TOKENS:
            return "The Grant transaction type creates tokens for an issuer-managed property.\n\n"
                   "Please select the property you would like to grant tokens for, then select the sending "
                   "address.  New tokens are credited to the recipient if supplied, otherwise the new tokens "
                   "are credited to the sender.";
        case MSC_TYPE_REVOKE_PROPERTY_TOKENS:
            return "The Revoke transaction type destroys tokens for an issuer-managed property.\n\n"
                   "Please select the property you would like to revoke tokens for, then select the sending "
                   "address.  Tokens are debited from the sender and destroyed.";
        case MSC_TYPE_CHANGE_ISSUER_ADDRESS:
            return "The Change Issuer transaction type changes the issuer on record for a property.\n\n"
                   "Please choose the property you would like to change the issuer for, then enter the "
                   "recipient address.  Changing the issuer on record transfers control of a property from "
                   "the sender to the recipient.\n\n"
                     "Use with caution!";

        default: return "* A description for this transaction cannot be found *";
    }
}

} // end namespace
