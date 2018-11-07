#ifndef OMNICORE_SP_H
#define OMNICORE_SP_H

#include "omnicore/log.h"
#include "omnicore/omnicore.h"
#include "omnicore/persistence.h"

class CBlockIndex;
class uint256;

#include "serialize.h"

#include <boost/filesystem.hpp>

#include <openssl/sha.h>

#include <stdint.h>
#include <stdio.h>

#include <fstream>
#include <map>
#include <string>
#include <utility>
#include <vector>

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

        ////////////////////////////
        /** New things for Contracts */
        uint32_t blocks_until_expiration;
        uint32_t notional_size;
        uint32_t collateral_currency;
        uint32_t margin_requirement;
        int64_t contracts_needed;
        int init_block;
        /*uint32_t numerator; */
        uint32_t denomination;
        /* int64_t ticksize; */
        std::string series;

        // for pegged currency
        uint32_t contract_associated;

        // For crowdsale properties:
        //   txid -> amount invested, crowdsale deadline, user issued tokens, issuer issued tokens
        // For managed properties:
        //   txid -> granted amount, revoked amount
        std::map<uint256, std::vector<int64_t> > historicalData;

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
            ////////////////////////////
            /** New things for Contracts */
            READWRITE(blocks_until_expiration);
            READWRITE(notional_size);
            READWRITE(collateral_currency);
            READWRITE(margin_requirement);
            READWRITE(init_block);
            READWRITE(contract_associated);
            READWRITE(denomination);
            READWRITE(series);
            ////////////////////////////
        }

        bool isDivisible() const;
        void print() const;
      	bool isContract() const;
    };

private:
    // implied version of OMNI and TOMNI so they don't hit the leveldb
    Entry implied_omni;
    Entry implied_tomni;

    uint32_t next_spid;
    uint32_t next_test_spid;

public:
    CMPSPInfo(const boost::filesystem::path& path, bool fWipe);
    virtual ~CMPSPInfo();

    /** Extends clearing of CDBBase. */
    void Clear();

    void init(uint32_t nextSPID = 0x3UL, uint32_t nextTestSPID = TEST_ECO_PROPERTY_1);

    uint32_t peekNextSPID(uint8_t ecosystem) const;
    bool updateSP(uint32_t propertyId, const Entry& info);
    uint32_t putSP(uint8_t ecosystem, const Entry& info);
    bool getSP(uint32_t propertyId, Entry& info) const;
    bool hasSP(uint32_t propertyId) const;
    uint32_t findSPByTX(const uint256& txid) const;

    int64_t popBlock(const uint256& block_hash);

    void setWatermark(const uint256& watermark);
    bool getWatermark(uint256& watermark) const;

    void printAll() const;

    int rollingContractsBlock(const CBlockIndex* pBlockIndex);
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
    void saveCrowdSale(std::ofstream& file, SHA256_CTX* shaCtx, const std::string& addr) const;
};

/**  NOTE: May be we can create contract type class, finding data in memory instead of db */
//
// class ContractSP
// {
// private:
//     uint32_t numeration;
//     uint32_t denomination;
//     uint32_t blocks_until_expiration;
//     uint32_t notional_size;
//     uint32_t collateral_currency;
//     uint32_t margin_requirement;
//     /* int64_t ticksize; */
//     uint32_t contractId;
//     int init_block;
//
// public:
//     ContractSP();
//     ContractSP(uint32_t num, uint32_t den, uint32_t buex, uint32_t ns, uint32_t col, uint32_t mar, int blk, int64_t tick, uint32_t id);
//
//     uint32_t getNumeration () const { return numeration; }
//     uint32_t getContractId() const { return contractId; }
//     uint32_t getDenomination() const { return denomination; }
//     int64_t getDeadline() const { return (init_block + static_cast<int>(blocks_until_expiration)); }
//     uint32_t getNotionalSize () const { return notional_size; }
//     uint32_t getMarginRequirement () const { return margin_requirement; }
//     int getInitBlock () const { return init_block; }
//     /* int64_t getTickSize () const { return ticksize; } */
// };

namespace mastercore
{
typedef std::map<std::string, CMPCrowd> CrowdMap;
// typedef std::map<std::string, ContractSP> ContractMap;

extern CMPSPInfo* _my_sps;
extern CrowdMap my_crowds;

std::string strPropertyType(uint16_t propertyType);
std::string strEcosystem(uint8_t ecosystem);
//////////////////////////////////////
/** New things for Contracts */
bool isPropertyContract(uint32_t propertyId);
int addInterestPegged(int nBlockPrev, const CBlockIndex* pBlockIndex);
uint64_t edgeOrderbook(uint32_t contractId, uint8_t tradingAction);
//////////////////////////////////////
std::string getPropertyName(uint32_t propertyId);
bool isPropertyDivisible(uint32_t propertyId);
bool IsPropertyIdValid(uint32_t propertyId);

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
bool isPropertyContract(uint32_t propertyId);

}




#endif // OMNICORE_SP_H
