// Smart Properties & Crowd Sales

#include <tradelayer/sp.h>

#include <tradelayer/log.h>
#include <tradelayer/mdex.h>
#include <tradelayer/tradelayer.h>
#include <tradelayer/uint256_extensions.h>

#include <arith_uint256.h>
#include <base58.h>
#include <clientversion.h>
#include <hash.h>
#include <serialize.h>
#include <streams.h>
#include <tinyformat.h>
#include <uint256.h>
#include <util/time.h>
#include <validation.h>

#include <leveldb/db.h>
#include <leveldb/write_batch.h>

#include <stdint.h>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

using namespace mastercore;

typedef boost::multiprecision::uint128_t ui128;

CMPSPInfo::Entry::Entry()
  : prop_type(0), prev_prop_id(0), num_tokens(0),
    fixed(false), manual(false) {}

bool CMPSPInfo::Entry::isDivisible() const
{
  switch (prop_type)
    {
    case ALL_PROPERTY_TYPE_DIVISIBLE:
    case ALL_PROPERTY_TYPE_PEGGEDS:
      return true;
    }
  return false;
}

// bool CMPSPInfo::Entry::isNative() const
// {
//   switch (prop_type)
//     {
//     case ALL_PROPERTY_TYPE_NATIVE_CONTRACT:
//       return true;
//     }
//   return false;
// }

// bool CMPSPInfo::Entry::isSwap() const
// {
//   switch (prop_type)
//     {
//       case ALL_PROPERTY_TYPE_PERPETUAL_ORACLE:
//       return true;
//       case ALL_PROPERTY_TYPE_PERPETUAL_CONTRACTS:
//       return true;
//     }
//   return false;
// }

// bool CMPSPInfo::Entry::isOracle() const
// {
//   switch (prop_type)
//     {
//     case ALL_PROPERTY_TYPE_ORACLE_CONTRACT:
//       return true;
//     case ALL_PROPERTY_TYPE_PERPETUAL_ORACLE:
//       return true;
//     }
//   return false;
// }

bool CMPSPInfo::Entry::isPegged() const
{
  switch (prop_type)
    {
    case ALL_PROPERTY_TYPE_PEGGEDS:
      return true;
    }
  return false;
}

// bool CMPSPInfo::Entry::isContract() const
// {
//     switch (prop_type)
//         {
//             case ALL_PROPERTY_TYPE_NATIVE_CONTRACT:
//                 return true;
//
//             case ALL_PROPERTY_TYPE_ORACLE_CONTRACT:
//                 return true;
//
//             case ALL_PROPERTY_TYPE_PERPETUAL_ORACLE:
//                 return true;
//
//             case ALL_PROPERTY_TYPE_PERPETUAL_CONTRACTS:
//                 return true;
//     }
//   return false;
// }

void CMPSPInfo::Entry::print() const
{
  PrintToLog("%s:%s(Fixed=%s,Divisible=%s):%d:%s/%s, %s %s\n",
		 issuer,
		 name,
		 fixed ? "Yes" : "No",
		 isDivisible() ? "Yes" : "No",
		 num_tokens,
		 category, subcategory, url, data);
}

CMPSPInfo::CMPSPInfo(const fs::path& path, bool fWipe)
{
  leveldb::Status status = Open(path, fWipe);
  PrintToLog("Loading smart property database: %s\n", status.ToString());

  // special cases for constant SPs ALL and TALL
  // implied_all.issuer = ExodusAddress().ToString();
  implied_all.prop_type = ALL_PROPERTY_TYPE_DIVISIBLE;
  implied_all.num_tokens = 700000;
  implied_all.category = "N/A";
  implied_all.subcategory = "N/A";
  implied_all.name = "ALL";
  implied_all.url = "";
  implied_all.data = "";
  implied_all.kyc.push_back(0); // kyc 0 as default
  implied_tall.prop_type = ALL_PROPERTY_TYPE_DIVISIBLE;
  implied_tall.num_tokens = 700000;
  implied_tall.category = "N/A";
  implied_tall.subcategory = "N/A";
  implied_tall.name = "sLTC";
  implied_tall.url = "";
  implied_tall.data = "";
  implied_tall.kyc.push_back(0);

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

void CMPSPInfo::init(uint32_t nextSPID)
{
  next_spid = nextSPID;
}

uint32_t CMPSPInfo::peekNextSPID() const
{
    uint32_t nextId = next_spid;

    return nextId;
}

bool CMPSPInfo::updateSP(uint32_t propertyId, const Entry& info)
{
  // cannot update implied SP
  if (ALL == propertyId || sLTC == propertyId) {
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

uint32_t CMPSPInfo::putSP(const Entry& info)
{
    uint32_t propertyId = next_spid++;

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
    // special cases for ALL and sLTC
    if (ALL == propertyId) {
        info = implied_all;
        return true;
    } else if (sLTC == propertyId){
        info = implied_tall;
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
    // Special cases for ALL and sLTC
    if (ALL == propertyId || sLTC == propertyId) {
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
    // print off the hard coded ALL and TALL entries
    for (uint32_t idx = TL_PROPERTY_ALL; idx <= TL_PROPERTY_TALL; idx++) {
        Entry info;
        PrintToLog("%10d => ", idx);
        if (getSP(idx, info)) {
            info.print();
        } else {
            PrintToLog("<Internal Error on implicit SP>\n");
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
            PrintToLog("<Malformed key in DB>\n");
            continue;
        }
        PrintToLog("%10s => ", propertyId);

        // deserialize the persisted data
        leveldb::Slice slSpValue = iter->value();
        Entry info;
        try {
            CDataStream ssSpValue(slSpValue.data(), slSpValue.data() + slSpValue.size(), SER_DISK, CLIENT_VERSION);
            ssSpValue >> info;
        } catch (const std::exception& e) {
            PrintToLog("<Malformed value in DB>\n");
            PrintToLog("%s(): ERROR: %s\n", __func__, e.what());
            continue;
        }
        info.print();
    }

    //clean up the iterator
    delete iter;
}

bool mastercore::IsPropertyIdValid(uint32_t propertyId)
{
  // is true, because we can exchange litecoins too
  if (propertyId == LTC) return true;

  uint32_t nextId = 0;

  if (propertyId < MAX_PROPERTY_N) {
    nextId = _my_sps->peekNextSPID();
  }

  if (propertyId < nextId) {
    return true;
  }

  return false;
}

bool mastercore::isPropertyDivisible(uint32_t propertyId)
{
  // TODO: is a lock here needed
  CMPSPInfo::Entry sp;

  if (_my_sps->getSP(propertyId, sp)) return sp.isDivisible();

  return true;
}


// bool mastercore::isPropertySwap(uint32_t propertyId)
// {
//   CMPSPInfo::Entry sp;
//
//   if (_my_sps->getSP(propertyId, sp)) return sp.isSwap();
//
//   return true;
// }

bool mastercore::isPropertyPegged(uint32_t propertyId)
{
  CMPSPInfo::Entry sp;

  if (_my_sps->getSP(propertyId, sp)) return sp.isPegged();

  return true;
}

std::string mastercore::getPropertyName(uint32_t propertyId)
{
    CMPSPInfo::Entry sp;
    if (_my_sps->getSP(propertyId, sp)) return sp.name;
    return "Property Name Not Found";
}

bool mastercore::getEntryFromName(const std::string& name, uint32_t& propertyId, CMPSPInfo::Entry& sp)
{
    uint32_t nextSPID = _my_sps->peekNextSPID();
    for (propertyId = 1; propertyId < nextSPID; propertyId++)
    {
        if (_my_sps->getSP(propertyId, sp) && name == sp.name) return true;
    }

    return false;
}

int mastercore::addInterestPegged(int nBlock)
{
    int64_t priceIndex = 0;
    int64_t nMarketPrice = 0;

    for (std::unordered_map<std::string, CMPTally>::iterator it = mp_tally_map.begin(); it != mp_tally_map.end(); ++it) {
            uint32_t id = 0;
            std::string address = it->first;
            (it->second).init();

            // searching for pegged currency
            while (0 != (id = (it->second).next())) {
                CMPSPInfo::Entry newSp;
                if (!_my_sps->getSP(id, newSp) || newSp.prop_type != ALL_PROPERTY_TYPE_PEGGEDS) {
                    continue;
                }

                // checking for deadline block
                CDInfo::Entry cd;
                _my_cds->getCD(newSp.contract_associated, cd);

                const int deadline = cd.blocks_until_expiration + cd.init_block;
                if (deadline != nBlock) { continue; }

                // natives or oracles?
                if(cd.isOracle()) {
                    priceIndex = getOracleTwap(newSp.contract_associated, 24);
                    nMarketPrice = getContractTradesVWAP(newSp.contract_associated, 24);
                } else {
                    // we need to include natives here
                }

                const int64_t diff = priceIndex - nMarketPrice;
                arith_uint256 interest = ConvertTo256(diff) / ConvertTo256(nMarketPrice);

                if(msc_debug_interest_pegged) {
                    PrintToLog("%s(): diff: %d, priceIndex: %d, nMarketPrice: %d, diff: %d, interest: %d\n",__func__, diff, priceIndex, nMarketPrice, diff, ConvertTo64(interest));
                }

                //price of ALL expresed in id property
                int64_t allPrice = 0;
                auto it = market_priceMap.find(ALL);
                if (it != market_priceMap.end())
                {
                    const auto &auxMap = it->second;
                    auto itt = auxMap.find(id);
                    if (itt != auxMap.end()){
                       allPrice = itt->second;
                    }

                }

                //adding interest to pegged
                const int64_t nPegged = getMPbalance(address, id, BALANCE);



                if (allPrice > 0)
                {
                    const arith_uint256 all = (ConvertTo256(nPegged) * interest) / ConvertTo256(allPrice);
                    const int64_t intAll = ConvertTo64(all);

                    if(msc_debug_interest_pegged) {
                        PrintToLog("%s(): allPrice: %d, nPegged: %d, intAll: %d\n",__func__, allPrice, nPegged, intAll);
                    }

                    // updating pegged currency interest (id: pegged currency id)
                    assert(update_tally_map(address, id, intAll, BALANCE));
                }


            }

        }

    return 1;
}

std::string mastercore::strPropertyType(uint16_t propertyType)
{
  switch (propertyType)
    {
    case ALL_PROPERTY_TYPE_DIVISIBLE: return "divisible";
    case ALL_PROPERTY_TYPE_INDIVISIBLE: return "indivisible";
    }

  return "unknown";
}
