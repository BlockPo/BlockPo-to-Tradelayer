// Smart Properties & Crowd Sales

#include "omnicore/sp.h"

#include "omnicore/log.h"
#include "omnicore/omnicore.h"
#include "omnicore/uint256_extensions.h"
#include "omnicore/mdex.h"

#include "arith_uint256.h"
#include "base58.h"
#include "clientversion.h"
#include "validation.h"
#include "serialize.h"
#include "streams.h"
#include "tinyformat.h"
#include "uint256.h"
#include "utiltime.h"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

#include "leveldb/db.h"
#include "leveldb/write_batch.h"

#include <stdint.h>

#include <map>
#include <string>
#include <vector>
#include <utility>

using namespace mastercore;

extern uint64_t ask[10];
extern uint64_t bid[10];

extern int64_t priceIndex;
extern int64_t allPrice;
int64_t nMarketPrice = 888; // NOTE:just for testing
extern volatile uint64_t marketPrice;
extern int64_t factorE;
extern std::map<std::string,uint32_t> peggedIssuers;
typedef boost::multiprecision::uint128_t ui128;

CMPSPInfo::Entry::Entry()
  : prop_type(0), prev_prop_id(0), num_tokens(0), property_desired(0),
    deadline(0), early_bird(0), percentage(0),
    close_early(false), max_tokens(false), missedTokens(0), timeclosed(0),
    fixed(false), manual(false) {}

bool CMPSPInfo::Entry::isDivisible() const
{
    switch (prop_type) {
        case MSC_PROPERTY_TYPE_DIVISIBLE:
        case MSC_PROPERTY_TYPE_INDIVISIBLE:
            return true;
    }
    return false;
}

void CMPSPInfo::Entry::print() const
{
    PrintToConsole("%s:%s(Fixed=%s,Divisible=%s):%d:%s/%s, %s %s\n",
            issuer,
            name,
            fixed ? "Yes" : "No",
            isDivisible() ? "Yes" : "No",
            num_tokens,
            category, subcategory, url, data);
}

CMPSPInfo::CMPSPInfo(const boost::filesystem::path& path, bool fWipe)
{
    leveldb::Status status = Open(path, fWipe);
    PrintToConsole("Loading smart property database: %s\n", status.ToString());

    // special cases for constant SPs OMNI and TOMNI
    // implied_omni.issuer = ExodusAddress().ToString();
    implied_omni.prop_type = MSC_PROPERTY_TYPE_DIVISIBLE;
    implied_omni.num_tokens = 700000;
    implied_omni.category = "N/A";
    implied_omni.subcategory = "N/A";
    implied_omni.name = "property 1";
    implied_omni.url = "";
    implied_omni.data = "";
    implied_tomni.prop_type = MSC_PROPERTY_TYPE_DIVISIBLE;
    implied_tomni.num_tokens = 700000;
    implied_tomni.category = "N/A";
    implied_tomni.subcategory = "N/A";
    implied_tomni.name = "test property 2";
    implied_tomni.url = "";
    implied_tomni.data = "";

    init();
}

CMPSPInfo::~CMPSPInfo()
{
    if (msc_debug_persistence) PrintToLog("CMPSPInfo closed\n");
}

void CMPSPInfo::Clear()
{
    // wipe database via parent class
    CDBBase::Clear();
    // reset "next property identifiers"
    init();
}

void CMPSPInfo::init(uint32_t nextSPID, uint32_t nextTestSPID)
{
    next_spid = nextSPID;
    next_test_spid = nextTestSPID;
}

uint32_t CMPSPInfo::peekNextSPID(uint8_t ecosystem) const
{
    uint32_t nextId = 0;

    switch (ecosystem) {
        case OMNI_PROPERTY_MSC: // Main ecosystem, MSC: 1, TMSC: 2, First available SP = 3
            nextId = next_spid;
            break;
        case OMNI_PROPERTY_TMSC: // Test ecosystem, same as above with high bit set
            nextId = next_test_spid;
            break;
        default: // Non-standard ecosystem, ID's start at 0
            nextId = 0;
    }

    return nextId;
}

bool CMPSPInfo::updateSP(uint32_t propertyId, const Entry& info)
{
    // cannot update implied SP
    if (OMNI_PROPERTY_MSC == propertyId || OMNI_PROPERTY_TMSC == propertyId) {
        return false;
    }

    // DB key for property entry
    CDataStream ssSpKey(SER_DISK, CLIENT_VERSION);
    ssSpKey << std::make_pair('s', propertyId);
    leveldb::Slice slSpKey(&ssSpKey[0], ssSpKey.size());

    // DB value for property entry
    CDataStream ssSpValue(SER_DISK, CLIENT_VERSION);
    // ssSpValue.reserve(GetSerializeSize(info, ssSpValue.GetType(), ssSpValue.GetVersion()));
    ssSpValue << info;
    leveldb::Slice slSpValue(&ssSpValue[0], ssSpValue.size());

    // DB key for historical property entry
    CDataStream ssSpPrevKey(SER_DISK, CLIENT_VERSION);
    ssSpPrevKey << 'b';
    ssSpPrevKey << info.update_block;
    ssSpPrevKey << propertyId;
    leveldb::Slice slSpPrevKey(&ssSpPrevKey[0], ssSpPrevKey.size());

    leveldb::WriteBatch batch;
    std::string strSpPrevValue;

    // if a value exists move it to the old key
    if (!pdb->Get(readoptions, slSpKey, &strSpPrevValue).IsNotFound()) {
        batch.Put(slSpPrevKey, strSpPrevValue);
    }
    batch.Put(slSpKey, slSpValue);
    leveldb::Status status = pdb->Write(syncoptions, &batch);

    if (!status.ok()) {
        PrintToLog("%s(): ERROR for SP %d: %s\n", __func__, propertyId, status.ToString());
        return false;
    }

    PrintToLog("%s(): updated entry for SP %d successfully\n", __func__, propertyId);
    return true;
}

uint32_t CMPSPInfo::putSP(uint8_t ecosystem, const Entry& info)
{
    uint32_t propertyId = 0;
    switch (ecosystem) {
        case OMNI_PROPERTY_MSC: // Main ecosystem, MSC: 1, TMSC: 2, First available SP = 3
            propertyId = next_spid++;
            break;
        case OMNI_PROPERTY_TMSC: // Test ecosystem, same as above with high bit set
            propertyId = next_test_spid++;
            break;
        default: // Non-standard ecosystem, ID's start at 0
            propertyId = 0;
    }

    // DB key for property entry
    CDataStream ssSpKey(SER_DISK, CLIENT_VERSION);
    ssSpKey << std::make_pair('s', propertyId);
    leveldb::Slice slSpKey(&ssSpKey[0], ssSpKey.size());

    // DB value for property entry
    CDataStream ssSpValue(SER_DISK, CLIENT_VERSION);
    ssSpValue.reserve(GetSerializeSize(info, ssSpValue.GetType(), ssSpValue.GetVersion()));
    ssSpValue << info;
    leveldb::Slice slSpValue(&ssSpValue[0], ssSpValue.size());

    // DB key for identifier lookup entry
    CDataStream ssTxIndexKey(SER_DISK, CLIENT_VERSION);
    ssTxIndexKey << std::make_pair('t', info.txid);
    leveldb::Slice slTxIndexKey(&ssTxIndexKey[0], ssTxIndexKey.size());

    // DB value for identifier
    CDataStream ssTxValue(SER_DISK, CLIENT_VERSION);
    ssTxValue.reserve(GetSerializeSize(propertyId, ssSpValue.GetType(), ssSpValue.GetVersion()));
    ssTxValue << propertyId;
    leveldb::Slice slTxValue(&ssTxValue[0], ssTxValue.size());

    // sanity checking
    std::string existingEntry;
    if (!pdb->Get(readoptions, slSpKey, &existingEntry).IsNotFound() && slSpValue.compare(existingEntry) != 0) {
        std::string strError = strprintf("writing SP %d to DB, when a different SP already exists for that identifier", propertyId);
        PrintToLog("%s() ERROR: %s\n", __func__, strError);
    } else if (!pdb->Get(readoptions, slTxIndexKey, &existingEntry).IsNotFound() && slTxValue.compare(existingEntry) != 0) {
        std::string strError = strprintf("writing index txid %s : SP %d is overwriting a different value", info.txid.ToString(), propertyId);
        PrintToLog("%s() ERROR: %s\n", __func__, strError);
    }

    // atomically write both the the SP and the index to the database
    leveldb::WriteBatch batch;
    batch.Put(slSpKey, slSpValue);
    batch.Put(slTxIndexKey, slTxValue);

    leveldb::Status status = pdb->Write(syncoptions, &batch);

    if (!status.ok()) {
        PrintToLog("%s(): ERROR for SP %d: %s\n", __func__, propertyId, status.ToString());
    }

    return propertyId;
}

bool CMPSPInfo::getSP(uint32_t propertyId, Entry& info) const
{
    // special cases for constant SPs MSC and TMSC
    if (OMNI_PROPERTY_MSC == propertyId) {
        info = implied_omni;
        return true;
    } else if (OMNI_PROPERTY_TMSC == propertyId) {
        info = implied_tomni;
        return true;
    }

    // DB key for property entry
    CDataStream ssSpKey(SER_DISK, CLIENT_VERSION);
    ssSpKey << std::make_pair('s', propertyId);
    leveldb::Slice slSpKey(&ssSpKey[0], ssSpKey.size());

    // DB value for property entry
    std::string strSpValue;
    leveldb::Status status = pdb->Get(readoptions, slSpKey, &strSpValue);
    if (!status.ok()) {
        if (!status.IsNotFound()) {
            PrintToLog("%s(): ERROR for SP %d: %s\n", __func__, propertyId, status.ToString());
        }
        return false;
    }

    try {
        CDataStream ssSpValue(strSpValue.data(), strSpValue.data() + strSpValue.size(), SER_DISK, CLIENT_VERSION);
        ssSpValue >> info;
    } catch (const std::exception& e) {
        PrintToLog("%s(): ERROR for SP %d: %s\n", __func__, propertyId, e.what());
        return false;
    }

    return true;
}

bool CMPSPInfo::hasSP(uint32_t propertyId) const
{
    // Special cases for constant SPs MSC and TMSC
    if (OMNI_PROPERTY_MSC == propertyId || OMNI_PROPERTY_TMSC == propertyId) {
        return true;
    }

    // DB key for property entry
    CDataStream ssSpKey(SER_DISK, CLIENT_VERSION);
    ssSpKey << std::make_pair('s', propertyId);
    leveldb::Slice slSpKey(&ssSpKey[0], ssSpKey.size());

    // DB value for property entry
    std::string strSpValue;
    leveldb::Status status = pdb->Get(readoptions, slSpKey, &strSpValue);

    return status.ok();
    return true;
}

uint32_t CMPSPInfo::findSPByTX(const uint256& txid) const
{
    uint32_t propertyId = 0;

    // DB key for identifier lookup entry
    CDataStream ssTxIndexKey(SER_DISK, CLIENT_VERSION);
    ssTxIndexKey << std::make_pair('t', txid);
    leveldb::Slice slTxIndexKey(&ssTxIndexKey[0], ssTxIndexKey.size());

    // DB value for identifier
    std::string strTxIndexValue;
    if (!pdb->Get(readoptions, slTxIndexKey, &strTxIndexValue).ok()) {
        std::string strError = strprintf("failed to find property created with %s", txid.GetHex());
        PrintToLog("%s(): ERROR: %s", __func__, strError);
        return 0;
    }

    try {
        CDataStream ssValue(strTxIndexValue.data(), strTxIndexValue.data() + strTxIndexValue.size(), SER_DISK, CLIENT_VERSION);
        ssValue >> propertyId;
    } catch (const std::exception& e) {
        PrintToLog("%s(): ERROR: %s\n", __func__, e.what());
        return 0;
    }

    return propertyId;
}

int64_t CMPSPInfo::popBlock(const uint256& block_hash)
{
    int64_t remainingSPs = 0;
    leveldb::WriteBatch commitBatch;
    leveldb::Iterator* iter = NewIterator();

    CDataStream ssSpKeyPrefix(SER_DISK, CLIENT_VERSION);
    ssSpKeyPrefix << 's';
    leveldb::Slice slSpKeyPrefix(&ssSpKeyPrefix[0], ssSpKeyPrefix.size());

    for (iter->Seek(slSpKeyPrefix); iter->Valid() && iter->key().starts_with(slSpKeyPrefix); iter->Next()) {
        // deserialize the persisted value
        leveldb::Slice slSpValue = iter->value();
        Entry info;
        try {
            CDataStream ssValue(slSpValue.data(), slSpValue.data() + slSpValue.size(), SER_DISK, CLIENT_VERSION);
            ssValue >> info;
        } catch (const std::exception& e) {
            PrintToLog("%s(): ERROR: %s\n", __func__, e.what());
            return -1;
        }
        // pop the block
        if (info.update_block == block_hash) {
            leveldb::Slice slSpKey = iter->key();

            // need to roll this SP back
            if (info.update_block == info.creation_block) {
                // this is the block that created this SP, so delete the SP and the tx index entry
                CDataStream ssTxIndexKey(SER_DISK, CLIENT_VERSION);
                ssTxIndexKey << std::make_pair('t', info.txid);
                leveldb::Slice slTxIndexKey(&ssTxIndexKey[0], ssTxIndexKey.size());
                commitBatch.Delete(slSpKey);
                commitBatch.Delete(slTxIndexKey);
            } else {
                uint32_t propertyId = 0;
                try {
                    CDataStream ssValue(1+slSpKey.data(), 1+slSpKey.data()+slSpKey.size(), SER_DISK, CLIENT_VERSION);
                    ssValue >> propertyId;
                } catch (const std::exception& e) {
                    PrintToLog("%s(): ERROR: %s\n", __func__, e.what());
                    return -2;
                }

                CDataStream ssSpPrevKey(SER_DISK, CLIENT_VERSION);
                ssSpPrevKey << 'b';
                ssSpPrevKey << info.update_block;
                ssSpPrevKey << propertyId;
                leveldb::Slice slSpPrevKey(&ssSpPrevKey[0], ssSpPrevKey.size());

                std::string strSpPrevValue;
                if (!pdb->Get(readoptions, slSpPrevKey, &strSpPrevValue).IsNotFound()) {
                    // copy the prev state to the current state and delete the old state
                    commitBatch.Put(slSpKey, strSpPrevValue);
                    commitBatch.Delete(slSpPrevKey);
                    ++remainingSPs;
                } else {
                    // failed to find a previous SP entry, trigger reparse
                    PrintToLog("%s(): ERROR: failed to retrieve previous SP entry\n", __func__);
                    return -3;
                }
            }
        } else {
            ++remainingSPs;
        }
    }

    // clean up the iterator
    delete iter;

    leveldb::Status status = pdb->Write(syncoptions, &commitBatch);

    if (!status.ok()) {
        PrintToLog("%s(): ERROR: %s\n", __func__, status.ToString());
        return -4;
    }

    return remainingSPs;
}

void CMPSPInfo::setWatermark(const uint256& watermark)
{
    leveldb::WriteBatch batch;

    CDataStream ssKey(SER_DISK, CLIENT_VERSION);
    ssKey << 'B';
    leveldb::Slice slKey(&ssKey[0], ssKey.size());

    CDataStream ssValue(SER_DISK, CLIENT_VERSION);
    ssValue.reserve(GetSerializeSize(watermark, ssValue.GetType(), ssValue.GetVersion()));
    ssValue << watermark;
    leveldb::Slice slValue(&ssValue[0], ssValue.size());

    batch.Delete(slKey);
    batch.Put(slKey, slValue);

    leveldb::Status status = pdb->Write(syncoptions, &batch);
    if (!status.ok()) {
        PrintToLog("%s(): ERROR: failed to write watermark: %s\n", __func__, status.ToString());
    }
}

bool CMPSPInfo::getWatermark(uint256& watermark) const
{
    CDataStream ssKey(SER_DISK, CLIENT_VERSION);
    ssKey << 'B';
    leveldb::Slice slKey(&ssKey[0], ssKey.size());

    std::string strValue;
    leveldb::Status status = pdb->Get(readoptions, slKey, &strValue);
    if (!status.ok()) {
        if (!status.IsNotFound()) {
            PrintToLog("%s(): ERROR: failed to retrieve watermark: %s\n", __func__, status.ToString());
        }
        return false;
    }

    try {
        CDataStream ssValue(strValue.data(), strValue.data() + strValue.size(), SER_DISK, CLIENT_VERSION);
        ssValue >> watermark;
    } catch (const std::exception& e) {
        PrintToLog("%s(): ERROR: failed to deserialize watermark: %s\n", __func__, e.what());
        return false;
    }

    return true;
}

void CMPSPInfo::printAll() const
{
    // print off the hard coded MSC and TMSC entries
    for (uint32_t idx = OMNI_PROPERTY_MSC; idx <= OMNI_PROPERTY_TMSC; idx++) {
        Entry info;
        PrintToConsole("%10d => ", idx);
        if (getSP(idx, info)) {
            info.print();
        } else {
            PrintToConsole("<Internal Error on implicit SP>\n");
        }
    }

    leveldb::Iterator* iter = NewIterator();

    CDataStream ssSpKeyPrefix(SER_DISK, CLIENT_VERSION);
    ssSpKeyPrefix << 's';
    leveldb::Slice slSpKeyPrefix(&ssSpKeyPrefix[0], ssSpKeyPrefix.size());

    for (iter->Seek(slSpKeyPrefix); iter->Valid() && iter->key().starts_with(slSpKeyPrefix); iter->Next()) {
        leveldb::Slice slSpKey = iter->key();
        uint32_t propertyId = 0;
        try {
            CDataStream ssValue(1+slSpKey.data(), 1+slSpKey.data()+slSpKey.size(), SER_DISK, CLIENT_VERSION);
            ssValue >> propertyId;
        } catch (const std::exception& e) {
            PrintToLog("%s(): ERROR: %s\n", __func__, e.what());
            PrintToConsole("<Malformed key in DB>\n");
            continue;
        }
        PrintToConsole("%10s => ", propertyId);

        // deserialize the persisted data
        leveldb::Slice slSpValue = iter->value();
        Entry info;
        try {
            CDataStream ssSpValue(slSpValue.data(), slSpValue.data() + slSpValue.size(), SER_DISK, CLIENT_VERSION);
            ssSpValue >> info;
        } catch (const std::exception& e) {
            PrintToConsole("<Malformed value in DB>\n");
            PrintToLog("%s(): ERROR: %s\n", __func__, e.what());
            continue;
        }
        info.print();
    }

    //clean up the iterator
    delete iter;
}

CMPCrowd::CMPCrowd()
  : propertyId(0), nValue(0), property_desired(0), deadline(0),
    early_bird(0), percentage(0), u_created(0), i_created(0)
{
}

CMPCrowd::CMPCrowd(uint32_t pid, int64_t nv, uint32_t cd, int64_t dl, uint8_t eb, uint8_t per, int64_t uct, int64_t ict)
  : propertyId(pid), nValue(nv), property_desired(cd), deadline(dl),
    early_bird(eb), percentage(per), u_created(uct), i_created(ict)
{
}

void CMPCrowd::insertDatabase(const uint256& txHash, const std::vector<int64_t>& txData)
{
    txFundraiserData.insert(std::make_pair(txHash, txData));
}

std::string CMPCrowd::toString(const std::string& address) const
{
    return strprintf("%34s : id=%u=%X; prop=%u, value= %li, deadline: %s (%lX)", address, propertyId, propertyId,
        property_desired, nValue, DateTimeStrFormat("%Y-%m-%d %H:%M:%S", deadline), deadline);
}

void CMPCrowd::print(const std::string& address, FILE* fp) const
{
    fprintf(fp, "%s\n", toString(address).c_str());
}

void CMPCrowd::saveCrowdSale(std::ofstream& file, SHA256_CTX* shaCtx, const std::string& addr) const
{
    // compose the outputline
    // addr,propertyId,nValue,property_desired,deadline,early_bird,percentage,created,mined
    std::string lineOut = strprintf("%s,%d,%d,%d,%d,%d,%d,%d,%d",
            addr,
            propertyId,
            nValue,
            property_desired,
            deadline,
            early_bird,
            percentage,
            u_created,
            i_created);

    // append N pairs of address=nValue;blockTime for the database
    std::map<uint256, std::vector<int64_t> >::const_iterator iter;
    for (iter = txFundraiserData.begin(); iter != txFundraiserData.end(); ++iter) {
        lineOut.append(strprintf(",%s=", (*iter).first.GetHex()));
        std::vector<int64_t> const &vals = (*iter).second;

        std::vector<int64_t>::const_iterator valIter;
        for (valIter = vals.begin(); valIter != vals.end(); ++valIter) {
            if (valIter != vals.begin()) {
                lineOut.append(";");
            }

            lineOut.append(strprintf("%d", *valIter));
        }
    }

    // add the line to the hash
    SHA256_Update(shaCtx, lineOut.c_str(), lineOut.length());

    // write the line
    file << lineOut << std::endl;
}

CMPCrowd* mastercore::getCrowd(const std::string& address)
{
    CrowdMap::iterator my_it = my_crowds.find(address);

    if (my_it != my_crowds.end()) return &(my_it->second);

    return (CMPCrowd *)NULL;
}

bool mastercore::IsPropertyIdValid(uint32_t propertyId)
{
    if (propertyId == 0) return false;

    uint32_t nextId = 0;

    if (propertyId < TEST_ECO_PROPERTY_1) {
        nextId = _my_sps->peekNextSPID(1);
    } else {
        nextId = _my_sps->peekNextSPID(2);
    }

    if (propertyId < nextId) {
        return true;
    }

    return false;
}

bool mastercore::isPropertyDivisible(uint32_t propertyId)
{
    // TODO: is a lock here needed
    // CMPSPInfo::Entry sp;
    //
    // if (_my_sps->getSP(propertyId, sp)) return sp.isDivisible();

    return true;
}

std::string mastercore::getPropertyName(uint32_t propertyId)
{
    CMPSPInfo::Entry sp;
    if (_my_sps->getSP(propertyId, sp)) return sp.name;
    return "Property Name Not Found";
}

bool mastercore::isCrowdsaleActive(uint32_t propertyId)
{
    for (CrowdMap::const_iterator it = my_crowds.begin(); it != my_crowds.end(); ++it) {
        const CMPCrowd& crowd = it->second;
        uint32_t foundPropertyId = crowd.getPropertyId();
        if (foundPropertyId == propertyId) return true;
    }
    return false;
}

/**
 * Calculates missing bonus tokens, which are credited to the crowdsale issuer.
 *
 * Due to rounding effects, a crowdsale issuer may not receive the full
 * bonus immediatly. The missing amount is calculated based on the total
 * tokens created and already credited.
 *
 * @param sp        The crowdsale property
 * @param crowdsale The crowdsale
 * @return The number of missed tokens
 */
int64_t mastercore::GetMissedIssuerBonus(const CMPSPInfo::Entry& sp, const CMPCrowd& crowdsale)
{
    // consistency check
    assert(getTotalTokens(crowdsale.getPropertyId())
            == (crowdsale.getIssuerCreated() + crowdsale.getUserCreated()));

    arith_uint256 amountMissing = 0;
    arith_uint256 bonusPercentForIssuer = ConvertTo256(sp.percentage);
    arith_uint256 amountAlreadyCreditedToIssuer = ConvertTo256(crowdsale.getIssuerCreated());
    arith_uint256 amountCreditedToUsers = ConvertTo256(crowdsale.getUserCreated());
    arith_uint256 amountTotal = amountCreditedToUsers + amountAlreadyCreditedToIssuer;

    // calculate theoretical bonus for issuer based on the amount of
    // tokens credited to users
    arith_uint256 exactBonus = amountCreditedToUsers * bonusPercentForIssuer;
    exactBonus /= ConvertTo256(100); // 100 %

    // there shall be no negative missing amount
    if (exactBonus < amountAlreadyCreditedToIssuer) {
        return 0;
    }

    // subtract the amount already credited to the issuer
    if (exactBonus > amountAlreadyCreditedToIssuer) {
        amountMissing = exactBonus - amountAlreadyCreditedToIssuer;
    }

    // calculate theoretical total amount of all tokens
    arith_uint256 newTotal = amountTotal + amountMissing;

    // reduce to max. possible amount
    if (newTotal > uint256_const::max_int64) {
        amountMissing = uint256_const::max_int64 - amountTotal;
    }

    return ConvertTo64(amountMissing);
}

// calculateFundraiser does token calculations per transaction
// calcluateFractional does calculations for missed tokens
void mastercore::calculateFundraiser(bool inflateAmount, int64_t amtTransfer, uint8_t bonusPerc,
        int64_t fundraiserSecs, int64_t currentSecs, int64_t numProps, uint8_t issuerPerc, int64_t totalTokens,
        std::pair<int64_t, int64_t>& tokens, bool& close_crowdsale)
{
    // Weeks in seconds
    arith_uint256 weeks_sec_ = ConvertTo256(604800);

    // Precision for all non-bitcoin values (bonus percentages, for example)
    arith_uint256 precision_ = ConvertTo256(1000000000000LL);

    // Precision for all percentages (10/100 = 10%)
    arith_uint256 percentage_precision = ConvertTo256(100);

    // Calculate the bonus seconds
    arith_uint256 bonusSeconds_ = 0;
    if (currentSecs < fundraiserSecs) {
        bonusSeconds_ = ConvertTo256(fundraiserSecs) - ConvertTo256(currentSecs);
    }

    // Calculate the whole number of weeks to apply bonus
    arith_uint256 weeks_ = (bonusSeconds_ / weeks_sec_) * precision_;
    weeks_ += (Modulo256(bonusSeconds_, weeks_sec_) * precision_) / weeks_sec_;

    // Calculate the earlybird percentage to be applied
    arith_uint256 ebPercentage_ = weeks_ * ConvertTo256(bonusPerc);

    // Calcluate the bonus percentage to apply up to percentage_precision number of digits
    arith_uint256 bonusPercentage_ = (precision_ * percentage_precision);
    bonusPercentage_ += ebPercentage_;
    bonusPercentage_ /= percentage_precision;

    // Calculate the bonus percentage for the issuer
    arith_uint256 issuerPercentage_ = ConvertTo256(issuerPerc);
    issuerPercentage_ *= precision_;
    issuerPercentage_ /= percentage_precision;

    // Precision for bitcoin amounts (satoshi)
    arith_uint256 satoshi_precision_ = ConvertTo256(100000000L);

    // Total tokens including remainders
    arith_uint256 createdTokens = ConvertTo256(amtTransfer);
    if (inflateAmount) {
        createdTokens *= ConvertTo256(100000000L);
    }
    createdTokens *= ConvertTo256(numProps);
    createdTokens *= bonusPercentage_;

    arith_uint256 issuerTokens = createdTokens / satoshi_precision_;
    issuerTokens /= precision_;
    issuerTokens *= (issuerPercentage_ / 100);
    issuerTokens *= precision_;

    arith_uint256 createdTokens_int = createdTokens / precision_;
    createdTokens_int /= satoshi_precision_;

    arith_uint256 issuerTokens_int = issuerTokens / precision_;
    issuerTokens_int /= satoshi_precision_;
    issuerTokens_int /= 100;

    arith_uint256 newTotalCreated = ConvertTo256(totalTokens) + createdTokens_int + issuerTokens_int;

    if (newTotalCreated > uint256_const::max_int64) {
        arith_uint256 maxCreatable = uint256_const::max_int64 - ConvertTo256(totalTokens);
        arith_uint256 created = createdTokens_int + issuerTokens_int;

        // Calcluate the ratio of tokens for what we can create and apply it
        arith_uint256 ratio = created * precision_;
        ratio *= satoshi_precision_;
        ratio /= maxCreatable;

        // The tokens for the issuer
        issuerTokens_int = issuerTokens_int * precision_;
        issuerTokens_int *= satoshi_precision_;
        issuerTokens_int /= ratio;

        assert(issuerTokens_int <= maxCreatable);

        // The tokens for the user
        createdTokens_int = maxCreatable - issuerTokens_int;

        // Close the crowdsale after assigning all tokens
        close_crowdsale = true;
    }

    // The tokens to credit
    tokens = std::make_pair(ConvertTo64(createdTokens_int), ConvertTo64(issuerTokens_int));
}

// go hunting for whether a simple send is a crowdsale purchase
// TODO !!!! horribly inefficient !!!! find a more efficient way to do this
bool mastercore::isCrowdsalePurchase(const uint256& txid, const std::string& address, int64_t* propertyId, int64_t* userTokens, int64_t* issuerTokens)
{
    // 1. loop crowdsales (active/non-active) looking for issuer address
    // 2. loop those crowdsales for that address and check their participant txs in database

    // check for an active crowdsale to this address
    CMPCrowd* pcrowdsale = getCrowd(address);
    if (pcrowdsale) {
        std::map<uint256, std::vector<int64_t> >::const_iterator it;
        const std::map<uint256, std::vector<int64_t> >& database = pcrowdsale->getDatabase();
        for (it = database.begin(); it != database.end(); it++) {
            const uint256& tmpTxid = it->first;
            if (tmpTxid == txid) {
                *propertyId = pcrowdsale->getPropertyId();
                *userTokens = it->second.at(2);
                *issuerTokens = it->second.at(3);
                return true;
            }
        }
    }

    // if we still haven't found txid, check non active crowdsales to this address
    for (uint8_t ecosystem = 1; ecosystem <= 2; ecosystem++) {
        uint32_t startPropertyId = (ecosystem == 1) ? 1 : TEST_ECO_PROPERTY_1;
        for (uint32_t loopPropertyId = startPropertyId; loopPropertyId < _my_sps->peekNextSPID(ecosystem); loopPropertyId++) {
            CMPSPInfo::Entry sp;
            if (!_my_sps->getSP(loopPropertyId, sp)) continue;
            if (sp.issuer != address) continue;
            for (std::map<uint256, std::vector<int64_t> >::const_iterator it = sp.historicalData.begin(); it != sp.historicalData.end(); it++) {
                if (it->first == txid) {
                    *propertyId = loopPropertyId;
                    *userTokens = it->second.at(2);
                    *issuerTokens = it->second.at(3);
                    return true;
                }
            }
        }
    }

    // didn't find anything, not a crowdsale purchase
    return false;
}

void mastercore::eraseMaxedCrowdsale(const std::string& address, int64_t blockTime, int block)
{
    CrowdMap::iterator it = my_crowds.find(address);

    if (it != my_crowds.end()) {
        const CMPCrowd& crowdsale = it->second;

        PrintToLog("%s(): ERASING MAXED OUT CROWDSALE from address=%s, at block %d (timestamp: %d), SP: %d (%s)\n",
            __func__, address, block, blockTime, crowdsale.getPropertyId(), strMPProperty(crowdsale.getPropertyId()));

        if (msc_debug_sp) {
            PrintToLog("%s(): %s\n", __func__, DateTimeStrFormat("%Y-%m-%d %H:%M:%S", blockTime));
            PrintToLog("%s(): %s\n", __func__, crowdsale.toString(address));
        }

        // get sp from data struct
        CMPSPInfo::Entry sp;
        assert(_my_sps->getSP(crowdsale.getPropertyId(), sp));

        // get txdata
        sp.historicalData = crowdsale.getDatabase();
        sp.close_early = true;
        sp.max_tokens = true;
        sp.timeclosed = blockTime;

        // update SP with this data
        sp.update_block = chainActive[block]->GetBlockHash();
        assert(_my_sps->updateSP(crowdsale.getPropertyId(), sp));

        // no calculate fractional calls here, no more tokens (at MAX)
        my_crowds.erase(it);
    }
}


int mastercore::addInterestPegged(int nBlockPrev, const CBlockIndex* pBlockIndex)
{
    allPrice = 888;
    for (std::unordered_map<std::string, CMPTally>::iterator it = mp_tally_map.begin(); it != mp_tally_map.end(); ++it) {
            uint32_t id = 0;
            std::string address = it->first;
            (it->second).init();

            // searching for pegged currency
            while (0 != (id = (it->second).next())) {
                CMPSPInfo::Entry newSp;
                if (!_my_sps->getSP(id, newSp) || newSp.subcategory != "Pegged Currency") {
                    continue;
                }

                // checking for deadline block
                CMPSPInfo::Entry spp;
                _my_sps->getSP(newSp.contract_associated, spp);
                int actualBlock = static_cast<int>(pBlockIndex->nHeight);
                int deadline = static_cast<int>(spp.blocks_until_expiration) + spp.init_block;
                if (deadline != actualBlock) { continue; }

                int64_t diff = priceIndex - nMarketPrice;
                int64_t tokens = static_cast<int64_t>(newSp.num_tokens);
                arith_uint256 num_tokens = ConvertTo256(tokens) / ConvertTo256(factorE);
                arith_uint256 interest = ConvertTo256(diff) / ConvertTo256(nMarketPrice);

                //adding interest to pegged
                int64_t nPegged = getMPbalance(address, id, BALANCE);
                arith_uint256 all = ConvertTo256(nPegged) * interest / ConvertTo256(allPrice);
                int64_t intAll = ConvertTo64(all);
                assert(update_tally_map(address, id, intAll, BALANCE));

            }

        }

    return 1;
}


int CMPSPInfo::rollingContractsBlock(const CBlockIndex* pBlockIndex)
{
    PrintToConsole("_________________________________________________________\n");
    PrintToConsole("Actual block : %d\n",pBlockIndex->nHeight); // Issuer, bid always

    for (std::map<std::string, uint32_t>::const_iterator it = peggedIssuers.begin(); it != peggedIssuers.end(); it++) {
        const std::string owner = it->first;
        const uint32_t propertyId = it->second;

        // NOTE: We need a map of Contracts and Pegged Currency to look for data faster
        Entry sp;
        if (_my_sps->getSP(propertyId, sp) && sp.subcategory == "Pegged Currency") {
            Entry spp;
            _my_sps->getSP(sp.contract_associated, spp);
            int expiration = static_cast<int>(spp.blocks_until_expiration);
            int actualBlock = static_cast<int>(pBlockIndex->nHeight);
            int deadline = expiration + spp.init_block;
            int rollingBlock = static_cast<int>(trunc( 0.8 * deadline ));  //80% of deadline blocks
            if (rollingBlock != actualBlock) { continue; }

            int64_t contractsReserved = getMPbalance(owner,sp.contract_associated, CONTRACTDEX_RESERVE);

            PrintToConsole("Generate the Rolling!\n");

            // then we must rolling the contracts A to contracts B  (selling the contracts and then putting them into RESERVE)
            int64_t bid = edgeOrderbook(sp.contract_associated,2);
            int64_t positiveBalance = getMPbalance(owner, sp.contract_associated, POSSITIVE_BALANCE);
            int64_t negativeBalance = getMPbalance(owner, sp.contract_associated, NEGATIVE_BALANCE);

            // calculating the price of the reserve position
            int64_t notionalSize = static_cast<int64_t>(spp.notional_size);
            ui128 uReservePrice = multiply_int64_t(contractsReserved, notionalSize);
            int64_t reservePrice = static_cast<int64_t>(uReservePrice);

            uint256 txid;
            uint32_t contractId2 = 0;
            int64_t notionalSizeB;
            assert(update_tally_map(owner, sp.contract_associated, -contractsReserved, CONTRACTDEX_RESERVE));

            // TODO: Here is better use ContractDex_INSERT function
            int result = ContractDex_ADD(owner, sp.contract_associated, contractsReserved, actualBlock, txid, 0, bid, 1, 0);

            // Looking for an older contract of the same type
            {
                LOCK(cs_tally);  // TODO: use maps instead search in database
                uint32_t NextSPID = _my_sps->peekNextSPID(1);
                PrintToConsole("NextSPID: %d\n",NextSPID);
                uint32_t begin = sp.contract_associated + 1;
                for (uint32_t propertyId = begin; propertyId < NextSPID; propertyId++) {
                    CMPSPInfo::Entry fc;
                    if (_my_sps->getSP(propertyId, fc)) {
                        PrintToConsole("Property Id: %d\n",propertyId);
                        if (fc.subcategory == "Futures Contracts" && fc.denomination == spp.denomination && fc.name != spp.name){
                            contractId2 = propertyId;
                            notionalSizeB = static_cast<int64_t>(fc.notional_size);
                            PrintToConsole("Property to jump in rolling found: %d\n",propertyId);
                            break;
                        }
                    }
                }
           }

           // If there's no contract to jump to
           if(contractId2 == 0) {
               PrintToConsole("No contract to jump to\n");
               return 0;
           }

           int64_t bid1 = edgeOrderbook(contractId2,2);
           int64_t positiveBalanceB = getMPbalance(owner,contractId2, POSSITIVE_BALANCE);
           int64_t negativeBalanceB = getMPbalance(owner,contractId2, NEGATIVE_BALANCE);

           // making the calculations of new amount of contracts in reserve
           arith_uint256 toReserve = DivideAndRoundUp(ConvertTo256(reservePrice), ConvertTo256(notionalSizeB)*ConvertTo256(factorE));
           int64_t newReserved = ConvertTo64(toReserve) * factorE;  // TODO: use multiply_int64_t

           PrintToConsole("________________________________________________\n");
           PrintToConsole("notional Size Contract B: %d\n",notionalSizeB);
           PrintToConsole("reservePrice: %d\n",reservePrice);
           PrintToConsole("new reserve: %d\n",newReserved);
           PrintToConsole("________________________________________________\n");

           int result2 = ContractDex_ADD(owner,contractId2, newReserved, actualBlock, txid, 0, bid1, 2, 0); // shorting in the future contract B

           if(positiveBalanceB >= 0 && negativeBalanceB == 0)
           {
               assert(update_tally_map(owner, contractId2, newReserved, POSSITIVE_BALANCE));

           } else if (positiveBalanceB == 0 && negativeBalanceB >= 0) {
               int64_t diffn = newReserved - negativeBalanceB;

               if (diffn > 0)
               {
                    assert(update_tally_map(owner, contractId2, diffn, POSSITIVE_BALANCE));
                    assert(update_tally_map(owner, contractId2, -negativeBalanceB, NEGATIVE_BALANCE));

               } else {
                    assert(update_tally_map(owner, contractId2, -newReserved, NEGATIVE_BALANCE));
               }

           }

           assert(update_tally_map(owner, contractId2, newReserved, CONTRACTDEX_RESERVE));

           int64_t contractsReservedB = getMPbalance(owner,contractId2, CONTRACTDEX_RESERVE);

           PrintToConsole("Price of bid: %d\n",bid1);

           if (result == 0){ PrintToConsole("Order Added in Contract A\n"); }
           if (result2 == 0){ PrintToConsole("Order Added in Contract B\n"); }

       }
   }

   return 1;
}

unsigned int mastercore::eraseExpiredCrowdsale(const CBlockIndex* pBlockIndex)
{
    if (pBlockIndex == NULL) return 0;

    const int64_t blockTime = pBlockIndex->GetBlockTime();
    const int blockHeight = pBlockIndex->nHeight;
    unsigned int how_many_erased = 0;
    CrowdMap::iterator my_it = my_crowds.begin();

    while (my_crowds.end() != my_it) {
        const std::string& address = my_it->first;
        const CMPCrowd& crowdsale = my_it->second;

        if (blockTime > crowdsale.getDeadline()) {
            PrintToLog("%s(): ERASING EXPIRED CROWDSALE from address=%s, at block %d (timestamp: %d), SP: %d (%s)\n",
                __func__, address, blockHeight, blockTime, crowdsale.getPropertyId(), strMPProperty(crowdsale.getPropertyId()));

            if (msc_debug_sp) {
                PrintToLog("%s(): %s\n", __func__, DateTimeStrFormat("%Y-%m-%d %H:%M:%S", blockTime));
                PrintToLog("%s(): %s\n", __func__, crowdsale.toString(address));
            }

            // get sp from data struct
            CMPSPInfo::Entry sp;
            assert(_my_sps->getSP(crowdsale.getPropertyId(), sp));

            // find missing tokens
            int64_t missedTokens = GetMissedIssuerBonus(sp, crowdsale);

            // get txdata
            sp.historicalData = crowdsale.getDatabase();
            sp.missedTokens = missedTokens;

            // update SP with this data
            sp.update_block = pBlockIndex->GetBlockHash();
            assert(_my_sps->updateSP(crowdsale.getPropertyId(), sp));

            // update values
            if (missedTokens > 0) {
                assert(update_tally_map(sp.issuer, crowdsale.getPropertyId(), missedTokens, BALANCE));
            }

            my_crowds.erase(my_it++);

            ++how_many_erased;

        } else my_it++;
    }

    return how_many_erased;
}

std::string mastercore::strPropertyType(uint16_t propertyType)
{
    switch (propertyType) {
        case MSC_PROPERTY_TYPE_DIVISIBLE: return "divisible";
        case MSC_PROPERTY_TYPE_INDIVISIBLE: return "indivisible";
    }

    return "unknown";
}

std::string mastercore::strEcosystem(uint8_t ecosystem)
{
    switch (ecosystem) {
        case OMNI_PROPERTY_MSC: return "main";
        case OMNI_PROPERTY_TMSC: return "test";
    }

    return "unknown";
}

/** New things for futures contracts */
bool mastercore::isPropertyContract(uint32_t propertyId)
{
    CMPSPInfo::Entry sp;

    if (_my_sps->getSP(propertyId, sp)  && sp.subcategory == "Futures Contracts") {
        return false;
    }


    return true;
}

uint64_t mastercore::edgeOrderbook(uint32_t contractId, uint8_t tradingAction)
{
    uint64_t price = 0;
    uint64_t result = 0;
    std::vector<uint64_t> vecContractDexPrices;

    cd_PricesMap* const ppriceMap = get_PricesCd(contractId); // checking the ask price of contract A
    for (cd_PricesMap::iterator it = ppriceMap->begin(); it != ppriceMap->end(); ++it) {
        const cd_Set& indexes = it->second;
        for (cd_Set::const_iterator it = indexes.begin(); it != indexes.end(); ++it) {
            const CMPContractDex& obj = *it;
            if (obj.getTradingAction() == tradingAction || obj.getAmountForSale() <= 0) continue;
            price = obj.getEffectivePrice();
            PrintToLog("edgeOrderbook/price: %d\n",price);
            vecContractDexPrices.push_back(price);
        }
    }

    if (tradingAction == BUY && !vecContractDexPrices.empty()){
       result = vecContractDexPrices.front();
    } else if (tradingAction == SELL && !vecContractDexPrices.empty()){
       result = vecContractDexPrices.back();
    }

    return static_cast<uint64_t>(result);
}
