// Contracts Db Entries

#include <tradelayer/ce.h>

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

CDInfo::Entry::Entry()
  : prop_type(0), blocks_until_expiration(0), notional_size(0), collateral_currency(0),
    margin_requirement(0), attribute_type(0), init_block(0), numerator(0), denominator(1),
    ticksize(1), inverse_quoted(false), expirated(false)   {}


bool CDInfo::Entry::isNative() const
{
  switch (prop_type)
    {
    case ALL_PROPERTY_TYPE_NATIVE_CONTRACT:
      return true;
    }
  return false;
}

bool CDInfo::Entry::isSwap() const
{
  switch (prop_type)
    {
      case ALL_PROPERTY_TYPE_PERPETUAL_ORACLE:
      return true;
      case ALL_PROPERTY_TYPE_PERPETUAL_CONTRACTS:
      return true;
    }
  return false;
}

bool CDInfo::Entry::isOracle() const
{
  switch (prop_type)
    {
    case ALL_PROPERTY_TYPE_ORACLE_CONTRACT:
      return true;
    case ALL_PROPERTY_TYPE_PERPETUAL_ORACLE:
      return true;
    }
  return false;
}


void CDInfo::Entry::print() const
{
  // NOTE: include all contract info
  // PrintToLog("%s:%s(Fixed=%s,Divisible=%s):%d:%s/%s, %s %s\n",
	// 	 issuer,
	// 	 name,
	// 	 fixed ? "Yes" : "No",
	// 	 isDivisible() ? "Yes" : "No",
	// 	 num_tokens,
	// 	 category, subcategory, url, data);
}

CDInfo::CDInfo(const fs::path& path, bool fWipe)
{
  leveldb::Status status = Open(path, fWipe);
  PrintToLog("Loading contracts database: %s\n", status.ToString());
  init();
}

CDInfo::~CDInfo()
{
  if (msc_debug_persistence) PrintToLog("CDInfo closed\n");
}

void CDInfo::Clear()
{
  // wipe database via parent class
  CDBBase::Clear();
  init();
}

void CDInfo::init(uint32_t nextCDID)
{
  next_contract_id = nextCDID;
}

uint32_t CDInfo::peekNextContractID() const
{
    uint32_t nextId = next_contract_id;

    return nextId;
}


bool CDInfo::updateCD(uint32_t contractId, const Entry& info)
{

  // DB key for property entry
  CDataStream ssSpKey(SER_DISK, CLIENT_VERSION);
  ssSpKey << std::make_pair('s', contractId);
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
  ssSpPrevKey << contractId;
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
    PrintToLog("%s(): ERROR for CD %d: %s\n", __func__, contractId, status.ToString());
    return false;
  }

  PrintToLog("%s(): updated entry for CD %d successfully\n", __func__, contractId);
  return true;
}

uint32_t CDInfo::putCD(const Entry& info)
{
    uint32_t contractId = next_contract_id++;

    // DB key for property entry
    CDataStream ssSpKey(SER_DISK, CLIENT_VERSION);
    ssSpKey << std::make_pair('s', contractId);
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
    ssTxValue.reserve(GetSerializeSize(contractId, ssSpValue.GetType(), ssSpValue.GetVersion()));
    ssTxValue << contractId;
    leveldb::Slice slTxValue(&ssTxValue[0], ssTxValue.size());

    // sanity checking
    std::string existingEntry;
    if (!pdb->Get(readoptions, slSpKey, &existingEntry).IsNotFound() && slSpValue.compare(existingEntry) != 0) {
        std::string strError = strprintf("writing CD %d to DB, when a different CD already exists for that identifier", contractId);
        PrintToLog("%s() ERROR: %s\n", __func__, strError);
    } else if (!pdb->Get(readoptions, slTxIndexKey, &existingEntry).IsNotFound() && slTxValue.compare(existingEntry) != 0) {
        std::string strError = strprintf("writing index txid %s : CD %d is overwriting a different value", info.txid.ToString(), contractId);
        PrintToLog("%s() ERROR: %s\n", __func__, strError);
    }

    // atomically write both the the SP and the index to the database
    leveldb::WriteBatch batch;
    batch.Put(slSpKey, slSpValue);
    batch.Put(slTxIndexKey, slTxValue);

    leveldb::Status status = pdb->Write(syncoptions, &batch);

    if (!status.ok()) {
        PrintToLog("%s(): ERROR for CD %d: %s\n", __func__, contractId, status.ToString());
    }

    return contractId;
}

bool CDInfo::getCD(uint32_t contractId, Entry& info) const
{
    // DB key for property entry
    CDataStream ssSpKey(SER_DISK, CLIENT_VERSION);
    ssSpKey << std::make_pair('s', contractId);
    leveldb::Slice slSpKey(&ssSpKey[0], ssSpKey.size());

    // DB value for property entry
    std::string strSpValue;
    leveldb::Status status = pdb->Get(readoptions, slSpKey, &strSpValue);
    if (!status.ok()) {
        if (!status.IsNotFound()) {
            PrintToLog("%s(): ERROR for CD %d: %s\n", __func__, contractId, status.ToString());
        }
        return false;
    }

    try {
        CDataStream ssSpValue(strSpValue.data(), strSpValue.data() + strSpValue.size(), SER_DISK, CLIENT_VERSION);
        ssSpValue >> info;
    } catch (const std::exception& e) {
        PrintToLog("%s(): ERROR for CD %d: %s\n", __func__, contractId, e.what());
        return false;
    }

    return true;
}

bool CDInfo::hasCD(uint32_t contractId) const
{
    // DB key for property entry
    CDataStream ssSpKey(SER_DISK, CLIENT_VERSION);
    ssSpKey << std::make_pair('s', contractId);
    leveldb::Slice slSpKey(&ssSpKey[0], ssSpKey.size());

    // DB value for property entry
    std::string strSpValue;
    leveldb::Status status = pdb->Get(readoptions, slSpKey, &strSpValue);

    return status.ok();
    return true;
}

uint32_t CDInfo::findCDByTX(const uint256& txid) const
{
    uint32_t contractId = 0;

    // DB key for identifier lookup entry
    CDataStream ssTxIndexKey(SER_DISK, CLIENT_VERSION);
    ssTxIndexKey << std::make_pair('t', txid);
    leveldb::Slice slTxIndexKey(&ssTxIndexKey[0], ssTxIndexKey.size());

    // DB value for identifier
    std::string strTxIndexValue;
    if (!pdb->Get(readoptions, slTxIndexKey, &strTxIndexValue).ok()) {
        std::string strError = strprintf("failed to find contract created with %s", txid.GetHex());
        PrintToLog("%s(): ERROR: %s", __func__, strError);
        return 0;
    }

    try {
        CDataStream ssValue(strTxIndexValue.data(), strTxIndexValue.data() + strTxIndexValue.size(), SER_DISK, CLIENT_VERSION);
        ssValue >> contractId;
    } catch (const std::exception& e) {
        PrintToLog("%s(): ERROR: %s\n", __func__, e.what());
        return 0;
    }

    return contractId;
}

int64_t CDInfo::popBlock(const uint256& block_hash)
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
                uint32_t contractId = 0;
                try {
                    CDataStream ssValue(1+slSpKey.data(), 1+slSpKey.data()+slSpKey.size(), SER_DISK, CLIENT_VERSION);
                    ssValue >> contractId;
                } catch (const std::exception& e) {
                    PrintToLog("%s(): ERROR: %s\n", __func__, e.what());
                    return -2;
                }

                CDataStream ssSpPrevKey(SER_DISK, CLIENT_VERSION);
                ssSpPrevKey << 'b';
                ssSpPrevKey << info.update_block;
                ssSpPrevKey << contractId;
                leveldb::Slice slSpPrevKey(&ssSpPrevKey[0], ssSpPrevKey.size());

                std::string strSpPrevValue;
                if (!pdb->Get(readoptions, slSpPrevKey, &strSpPrevValue).IsNotFound()) {
                    // copy the prev state to the current state and delete the old state
                    commitBatch.Put(slSpKey, strSpPrevValue);
                    commitBatch.Delete(slSpPrevKey);
                    ++remainingSPs;
                } else {
                    // failed to find a previous CD entry, trigger reparse
                    PrintToLog("%s(): ERROR: failed to retrieve previous CD entry\n", __func__);
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

void CDInfo::printAll() const
{
    // print off the hard coded ALL and TALL entries
    for (uint32_t idx = 1; idx <= next_contract_id; idx++) {
        Entry info;
        PrintToLog("%10d => ", idx);
        if (getCD(idx, info)) {
            info.print();
        } else {
            PrintToLog("<Internal Error on implicit CD>\n");
        }
    }

    leveldb::Iterator* iter = NewIterator();

    CDataStream ssSpKeyPrefix(SER_DISK, CLIENT_VERSION);
    ssSpKeyPrefix << 's';
    leveldb::Slice slSpKeyPrefix(&ssSpKeyPrefix[0], ssSpKeyPrefix.size());

    for (iter->Seek(slSpKeyPrefix); iter->Valid() && iter->key().starts_with(slSpKeyPrefix); iter->Next()) {
        leveldb::Slice slSpKey = iter->key();
        uint32_t contractId = 0;
        try {
            CDataStream ssValue(1+slSpKey.data(), 1+slSpKey.data()+slSpKey.size(), SER_DISK, CLIENT_VERSION);
            ssValue >> contractId;
        } catch (const std::exception& e) {
            PrintToLog("%s(): ERROR: %s\n", __func__, e.what());
            PrintToLog("<Malformed key in DB>\n");
            continue;
        }
        PrintToLog("%10s => ", contractId);

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



// bool mastercore::isPropertySwap(uint32_t contractId)
// {
//   CDInfo::Entry sp;
//
//   if (_my_cds->getCD(contractId, sp)) return sp.isSwap();
//
//   return true;
// }

bool mastercore::IsContractIdValid(uint32_t contractId)
{

  if (contractId == 0) {
      return false;
  }

  uint32_t nextId = 0;

  nextId = _my_cds->peekNextContractID();

  if (contractId < nextId) {
    return true;
  }

  return false;
}

std::string mastercore::getContractName(uint32_t contractId)
{
    CDInfo::Entry sp;
    if (_my_cds->getCD(contractId, sp)) return sp.name;
    return "Contract Name Not Found";
}

bool mastercore::getContractFromName(const std::string& name, uint32_t& contractId, CDInfo::Entry& sp)
{
    uint32_t nextCDID = _my_cds->peekNextContractID();
    for (contractId = 1; contractId < nextCDID; contractId++)
    {
        if (_my_cds->getCD(contractId, sp) && name == sp.name) return true;
    }

    return false;
}
