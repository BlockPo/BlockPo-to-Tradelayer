#include <tradelayer/wallettxs.h>

#include <tradelayer/log.h>
#include <tradelayer/rules.h>
#include <tradelayer/script.h>
#include <tradelayer/tradelayer.h>
#include <tradelayer/utilsbitcoin.h>

#include <amount.h>
#include <base58.h>
#include <init.h>
#include <policy/policy.h>
#include <pubkey.h>
#include <script/standard.h>
#include <sync.h>
#include <txmempool.h>
#include <uint256.h>
#include <util/strencodings.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#ifdef ENABLE_WALLET
#include <script/ismine.h>
#include <wallet/fees.h>
#include <wallet/wallet.h>
#endif

#include <map>
#include <stdint.h>
#include <string>

namespace mastercore
{
/**
 * Retrieves a public key from the wallet, or converts a hex-string to a public key.
 */
bool AddressToPubKey(const std::string& key, CPubKey& pubKey)
{
#ifdef ENABLE_WALLET
    // Case 1: Bitcoin address and the key is in the wallet
    CTxDestination dest = DecodeDestination(key);
    CWalletRef pwalletMain = nullptr;
    if (vpwallets.size() > 0){
        pwalletMain = vpwallets[0];
    }

    if (pwalletMain && IsValidDestination(dest)) {
        CKey keyOut;
        auto keyID = GetKeyForDestination(*pwalletMain, dest);
        if (keyID.IsNull()) {
            PrintToLog("%s: ERROR: redemption address %s does not refer to a public key\n", __func__, key);
            return false;
        }
        if (!pwalletMain->GetPubKey(keyID, pubKey)) {
            PrintToLog("%s() ERROR: no public key in wallet for redemption address %s\n", __func__, key);
            return false;
        }
    }
    // Case 2: Hex-encoded public key
    else
#endif
    if (IsHex(key)) {
        pubKey = CPubKey(ParseHex(key));
    }

    if (!pubKey.IsFullyValid()) {
        PrintToLog("%s(): ERROR: invalid redemption key %s\n", __func__, key);
        return false;
    }

    return true;
}

/**
 * Checks, whether enough spendable outputs are available to pay for transaction fees.
 */
bool CheckFee(const std::string& fromAddress, size_t nDataSize)
{
    int64_t minFee = 0;
    int64_t feePerKB = 0;
    int64_t inputTotal = 0;
#ifdef ENABLE_WALLET
    bool fUseClassD = UseEncodingClassD(nDataSize);

    CCoinControl coinControl;
    inputTotal = SelectCoins(fromAddress, coinControl, 0);

    // calculate the estimated fee per KB based on the currently set confirm target
    // CFeeRate feeRate = mempool.estimateFee(nTxConfirmTarget);
    CFeeRate feeRate = CFeeRate(0); // NOTE: just for testing

    // if there is not enough data (and zero is estimated) then base minimum on a fairly high/safe 50,000 satoshi fee per KB
    if (feeRate == CFeeRate(0)) {
        feePerKB = 50000;
    } else {
        feePerKB = feeRate.GetFeePerK();
    }

    // we do not know the size of the transaction at this point.  Warning calculation employs some guesswork
    if (!fUseClassD) {
        // Calculation based on a 3KB transaction due to:
        //   - the average size across all Class B transactions ever sent to date is 909 bytes.
        //   - under 2% of Class B transactions are over 2KB, under 0.6% of transactions are over 3KB.
        // Thus if created transaction will be over 3KB (rare as per above) warning may not be sufficient.
        minFee = feePerKB * 3;
    } else {
        // Averages for Class D transactions are not yet available, Calculation based on a 2KB transaction due to:
        //   - Class B values but considering Class D removes outputs for both data and Exodus (reduces size).
        minFee = feePerKB * 2;
    }
#endif
    return inputTotal >= minFee;
}

static int64_t GetEstimatedFeePerKb()
{
    int64_t nFee = 50000; // 0.0005 BTC;

#ifdef ENABLE_WALLET
    CWalletRef pwalletMain = nullptr;
    if (vpwallets.size() > 0){
        pwalletMain = vpwallets[0];
    }

    if (pwalletMain) {
       CCoinControl coin_control;
       nFee = GetMinimumFee(1000, coin_control, mempool, ::feeEstimator, nullptr);
    }
#endif
    return nFee;
}

static CAmount GetEconomicThreshold(const CTxOut& txOut)
{
    // Minimum value needed to relay the transaction
    CAmount nThresholdDust = GetDustThreshold(txOut,minRelayTxFee);

    // Use the estimated fee that is also used to contruct transactions.
    // We use the absolute minimum, so we divide by 3, to get rid of the
    // safety margin used for the dust threshold used for relaying.
    CFeeRate estimatedFeeRate(GetEstimatedFeePerKb());
    CAmount nThresholdFees = GetDustThreshold(txOut,estimatedFeeRate) / 3;

    if (msc_debug_wallettxs) PrintToLog("%s(): nThresholdDust: %d, nThresholdFees: %d\n",__func__, nThresholdDust, nThresholdFees);

    return std::max(nThresholdDust, nThresholdFees);
}

/**
 * Checks, whether the output qualifies as input for a transaction.
 */
bool CheckInput(const CTxOut& txOut, int nHeight, CTxDestination& dest)
{
    txnouttype whichType;

    if (!GetOutputType(txOut.scriptPubKey, whichType)) {
       return false;
    }
    if (!IsAllowedInputType(whichType, nHeight)) {
       return false;
    }
    if (!ExtractDestination(txOut.scriptPubKey, dest)) {
       return false;
    }

    return true;
}

/**
 * IsMine wrapper to determine whether the address is in the wallet.
 */
int IsMyAddress(const std::string& address)
{
#ifdef ENABLE_WALLET
    CWalletRef pwalletMain = nullptr;
    if (vpwallets.size() > 0){
        pwalletMain = vpwallets[0];
    }

    if (pwalletMain) {
        // TODO: resolve deadlock caused cs_tally, cs_wallet
        // LOCK(pwalletMain->cs_wallet);
        // CBitcoinAddress parsedAddress(address);
        isminetype isMine = IsMine(*pwalletMain, DecodeDestination(address));

        return static_cast<int>(isMine);
    }
#endif
    return 0;
}

/**
 * Selects spendable outputs to create a transaction.
 */
int64_t SelectCoins(const std::string& fromAddress, CCoinControl& coinControl, int64_t additional, unsigned int minOutputs)
{
    // total output funds collected
    int64_t nTotal = 0;

#ifdef ENABLE_WALLET
    CWalletRef pwalletMain = nullptr;
    if (vpwallets.size() > 0){
        pwalletMain = vpwallets[0];
    }

    if (nullptr == pwalletMain) {
        return 0;
    }

    // assume 20 KB max. transaction size at 0.0001 per kilobyte
    int64_t nMax = (COIN * (20 * (0.0001)));

    // if referenceamount is set it is needed to be accounted for here too
    if (0 < additional) nMax += additional;

    int nHeight = GetHeight();
    LOCK2(cs_main, pwalletMain->cs_wallet);

    // iterate over the wallet
    for (std::map<uint256, CWalletTx>::const_iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it) {
        const uint256& txid = it->first;
        const CWalletTx& wtx = it->second;

        if (!wtx.IsTrusted()) {
            continue;
        }
        if (!wtx.GetAvailableCredit()) {
            continue;
        }

        for (unsigned int n = 0; n < wtx.tx->vout.size(); n++) {
            const CTxOut& txOut = wtx.tx->vout[n];

            CTxDestination dest;
            if (!CheckInput(txOut, nHeight, dest)) {
                continue;
            }

            if (!IsMine(*pwalletMain, dest)) {
                continue;
            }

            if (pwalletMain->IsSpent(txid, n)) {
               continue;
            }

            if (txOut.nValue < GetEconomicThreshold(txOut)) {
                if (msc_debug_wallettxs) PrintToLog("%s(): output value below economic threshold: %s:%d, value: %d\n",
                        __func__, txid.GetHex(), n, txOut.nValue);
                continue;
            }

            const std::string sAddress = EncodeDestination(dest);

            // only use funds from the sender's address
            if (fromAddress == sAddress)
            {
                COutPoint outpoint(txid, n);
                coinControl.Select(outpoint);

                nTotal += txOut.nValue;

                if (nMax <= nTotal) break;
            }
        }

        if (nMax <= nTotal) break;
    }
#endif
    return nTotal;
}


} // namespace mastercore
