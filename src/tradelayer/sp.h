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

        // crowdsale generated SP
        uint32_t property_desired;
        int64_t deadline;
        uint8_t early_bird;
        uint8_t percentage;

        // closedearly states, if the SP was a crowdsale and closed due to MAXTOKENS or CLOSE command
        bool close_early;
        bool max_tokens;
        int64_t missedTokens;
        int64_t timeclosed;
        uint256 txid_close;

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
            READWRITE(property_desired);
            READWRITE(deadline);
            READWRITE(early_bird);
            READWRITE(percentage);
            READWRITE(close_early);
            READWRITE(max_tokens);
            READWRITE(missedTokens);
            READWRITE(timeclosed);
            READWRITE(txid_close);
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

/** A live crowdsale.
 */
class CMPCrowd
{
private:
    uint32_t propertyId;
    int64_t nValue;

    uint32_t property_desired;
    int64_t deadline;
    uint8_t early_bird;
    uint8_t percentage;

    int64_t u_created;
    int64_t i_created;

    uint256 txid; // NOTE: not persisted as it doesnt seem used

    // Schema:
    //   txid -> amount invested, crowdsale deadline, user issued tokens, issuer issued tokens
    std::map<uint256, std::vector<int64_t> > txFundraiserData;

public:
    CMPCrowd();
    CMPCrowd(uint32_t pid, int64_t nv, uint32_t cd, int64_t dl, uint8_t eb, uint8_t per, int64_t uct, int64_t ict);

    uint32_t getPropertyId() const { return propertyId; }

    int64_t getDeadline() const { return deadline; }
    uint32_t getCurrDes() const { return property_desired; }

    void incTokensUserCreated(int64_t amount) { u_created += amount; }
    void incTokensIssuerCreated(int64_t amount) { i_created += amount; }

    int64_t getUserCreated() const { return u_created; }
    int64_t getIssuerCreated() const { return i_created; }

    void insertDatabase(const uint256& txHash, const std::vector<int64_t>& txData);
    std::map<uint256, std::vector<int64_t> > getDatabase() const { return txFundraiserData; }

    std::string toString(const std::string& address) const;
    void print(const std::string& address, FILE* fp = stdout) const;
    void saveCrowdSale(std::ofstream& file, CHash256& hasher, const std::string& addr) const;
};

namespace mastercore
{
typedef std::map<std::string, CMPCrowd> CrowdMap;

extern CMPSPInfo* _my_sps;
extern CrowdMap my_crowds;

std::string strPropertyType(uint16_t propertyType);

// bool isPropertyContract(uint32_t propertyId);
// bool isPropertyNativeContract(uint32_t propertyId);
// bool isPropertySwap(uint32_t propertyId);

bool isPropertyDivisible(uint32_t propertyId);
bool IsPropertyIdValid(uint32_t propertyId);
bool isPropertyPegged(uint32_t propertyId);

std::string getPropertyName(uint32_t propertyId);
bool getEntryFromName(const std::string& name, uint32_t& propertyId, CMPSPInfo::Entry& sp);

CMPCrowd* getCrowd(const std::string& address);

bool isCrowdsaleActive(uint32_t propertyId);
bool isCrowdsalePurchase(const uint256& txid, const std::string& address, int64_t* propertyId, int64_t* userTokens, int64_t* issuerTokens);

/** Calculates missing bonus tokens, which are credited to the crowdsale issuer. */
int64_t GetMissedIssuerBonus(const CMPSPInfo::Entry& sp, const CMPCrowd& crowdsale);

/** Calculates amounts credited for a crowdsale purchase. */
void calculateFundraiser(bool inflateAmount, int64_t amtTransfer, uint8_t bonusPerc,
        int64_t fundraiserSecs, int64_t currentSecs, int64_t numProps, uint8_t issuerPerc, int64_t totalTokens,
        std::pair<int64_t, int64_t>& tokens, bool& close_crowdsale);

void eraseMaxedCrowdsale(const std::string& address, int64_t blockTime, int block);

unsigned int eraseExpiredCrowdsale(const CBlockIndex* pBlockIndex);
}

#endif // TRADELAYER_SP_H
