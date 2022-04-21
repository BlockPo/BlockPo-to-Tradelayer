#ifndef TRADELAYER_SP_H
#define TRADELAYER_SP_H

#include <tradelayer/log.h>
#include <tradelayer/persistence.h>
#include <tradelayer/tradelayer.h>

class CBlockIndex;
class CHash256;
class uint256;

#include <serialize.h>

#include <stdint.h>
#include <stdio.h>

#include <fstream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <boost/filesystem.hpp>

/** LevelDB based storage for currencies, smart properties and tokens.
 *
 * DB Schema:
 *
 *  Key:
 *      char 'B'
 *  Value:
 *      uint256 hashBlock
 *
 *  Key:
 *      char 's'
 *      uint32_t propertyId
 *  Value:
 *      CMPSPInfo::Entry info
 *
 *  Key:
 *      char 't'
 *      uint256 hashTxid
 *  Value:
 *      uint32_t propertyId
 *
 *  Key:
 *      char 'b'
 *      uint256 hashBlock
 *      uint32_t propertyId
 *  Value:
 *      CMPSPInfo::Entry info
 */
class CMPSPInfo : public CDBBase
{
public:
    struct Entry {
        // common SP data
        std::string issuer;
        uint16_t prop_type;
        uint32_t prev_prop_id;
        std::string category;
        std::string subcategory;
        std::string name;
        std::string url;
        std::string data;
        int64_t num_tokens;

        // other information
        uint256 txid;
        uint256 creation_block;
        uint256 update_block;
        bool fixed;
        bool manual;

        //vesting
        double last_vesting;
        int last_vesting_block;

        uint32_t attribute_type;
        int64_t contracts_needed;
        int init_block;

        // for pegged currency
        uint32_t contract_associated;
        uint32_t currency_associated;

        // For crowdsale properties:
        //   txid -> amount invested, crowdsale deadline, user issued tokens, issuer issued tokens
        // For managed properties:
        //   txid -> granted amount, revoked amount
        std::map<uint256, std::vector<int64_t> > historicalData;

        //kyc
        std::vector<int64_t> kyc;

        Entry();

        ADD_SERIALIZE_METHODS;

        template <typename Stream, typename Operation>
        inline void SerializationOp(Stream& s, Operation ser_action) {
            READWRITE(issuer);
            READWRITE(prop_type);
            READWRITE(prev_prop_id);
            READWRITE(category);
            READWRITE(subcategory);
            READWRITE(name);
            READWRITE(url);
            READWRITE(data);
            READWRITE(num_tokens);
            READWRITE(txid);
            READWRITE(creation_block);
            READWRITE(update_block);
            READWRITE(fixed);
            READWRITE(manual);
            READWRITE(historicalData);
            READWRITE(contracts_needed);
	          READWRITE(attribute_type);
            READWRITE(init_block);
            READWRITE(contract_associated);
            READWRITE(kyc);
            READWRITE(last_vesting);
            READWRITE(last_vesting_block);
        }

        bool isDivisible() const;
        void print() const;
        bool isPegged() const;

    };

 private:
    /** implied version of ALL and TALL so they don't hit the leveldb */
    Entry implied_all;
    Entry implied_tall;
    uint32_t next_spid;

 public:
    CMPSPInfo(const fs::path& path, bool fWipe);
    virtual ~CMPSPInfo();

    /** Extends clearing of CDBBase. */
    void Clear();

    void init(uint32_t nextSPID = 0x3UL);

    uint32_t peekNextSPID() const;
    bool updateSP(uint32_t propertyId, const Entry& info);
    uint32_t putSP(const Entry& info);
    bool getSP(uint32_t propertyId, Entry& info) const;
    bool hasSP(uint32_t propertyId) const;
    uint32_t findSPByTX(const uint256& txid) const;

    int64_t popBlock(const uint256& block_hash);

    void setWatermark(const uint256& watermark);
    bool getWatermark(uint256& watermark) const;

    void printAll() const;

};


namespace mastercore
{

extern CMPSPInfo* _my_sps;

std::string strPropertyType(uint16_t propertyType);

// bool isPropertyContract(uint32_t propertyId);
bool isPropertyPegged(uint32_t propertyId);
bool isPropertySwap(uint32_t propertyId);
bool isPropertyNativeContract(uint32_t propertyId);

std::string getPropertyName(uint32_t propertyId);
bool isPropertyDivisible(uint32_t propertyId);
bool IsPropertyIdValid(uint32_t propertyId);

bool isPropertyContract(uint32_t propertyId);

bool getEntryFromName(const std::string& name, uint32_t& propertyId, CMPSPInfo::Entry& sp);

int addInterestPegged(int nBlock);
}

#endif // TRADELAYER_SP_H
