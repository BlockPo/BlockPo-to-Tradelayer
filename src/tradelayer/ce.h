#ifndef TRADELAYER_CE_H
#define TRADELAYER_CE_H

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

/** LevelDB based storage for contracts
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
 *      uint32_t contractId
 *  Value:
 *      CDInfo::Entry info
 *
 *  Key:
 *      char 't'
 *      uint256 hashTxid
 *  Value:
 *      uint32_t contractId
 *
 *  Key:
 *      char 'b'
 *      uint256 hashBlock
 *      uint32_t contractId
 *  Value:
 *      CDInfo::Entry info
 */
class CDInfo : public CDBBase
{
public:
    struct Entry {
        // basic data
        std::string issuer;
        uint16_t prop_type;
        std::string name;
        std::string url;
        std::string data;

        // other information
        uint256 txid;
        uint256 creation_block;
        uint256 update_block;

        uint32_t blocks_until_expiration;
        uint32_t notional_size;
        uint32_t collateral_currency;
        uint64_t margin_requirement;
        uint32_t attribute_type;
        int init_block;

        uint32_t numerator;
        uint32_t denominator;

        int64_t ticksize;
        std::string series;
        std::string backup_address;
        uint64_t oracle_high;
        uint64_t oracle_low;
        uint64_t oracle_close;

        bool inverse_quoted;
        bool expirated;

        //kyc
        std::vector<int64_t> kyc;

        Entry();

        ADD_SERIALIZE_METHODS;

        template <typename Stream, typename Operation>
        inline void SerializationOp(Stream& s, Operation ser_action) {
            READWRITE(issuer);
            READWRITE(prop_type);
            READWRITE(name);
            READWRITE(url);
            READWRITE(data);
            READWRITE(txid);
            READWRITE(creation_block);
            READWRITE(update_block);

            READWRITE(blocks_until_expiration);
            READWRITE(notional_size);
            READWRITE(collateral_currency);
            READWRITE(margin_requirement);
	          READWRITE(attribute_type);
            READWRITE(init_block);
            READWRITE(numerator);
            READWRITE(denominator);
            READWRITE(series);
            READWRITE(backup_address);
            READWRITE(oracle_high);
            READWRITE(oracle_low);
            READWRITE(oracle_close);
            READWRITE(inverse_quoted);
            READWRITE(kyc);
            ////////////////////////////
        }

        void print() const;
      	bool isNative() const;
        bool isSwap() const;
        bool isOracle() const;
        bool isContract() const;
        bool isInverseQuoted() const { return inverse_quoted; };
        uint32_t getCollateral() const { return collateral_currency; }
    };

 private:
    uint32_t next_contract_id;

 public:
    CDInfo(const fs::path& path, bool fWipe);
    virtual ~CDInfo();

    /** Extends clearing of CDBBase. */
    void Clear();

    void init(uint32_t nextCDID = 0x1UL);

    uint32_t peekNextContractID() const;
    bool updateCD(uint32_t contractId, const Entry& info);
    uint32_t putCD(const Entry& info);
    bool getCD(uint32_t contractId, Entry& info) const;
    bool hasCD(uint32_t contractId) const;
    uint32_t findCDByTX(const uint256& txid) const;

    int64_t popBlock(const uint256& block_hash);

    void setWatermark(const uint256& watermark);
    bool getWatermark(uint256& watermark) const;

    void printAll() const;

};


namespace mastercore
{

extern CDInfo* _my_cds;

bool isContractSwap(uint32_t contractId);
bool isNativeContract(uint32_t contractId);

std::string getContractName(uint32_t contractId);
bool IsContractIdValid(uint32_t contractId);

bool getContractFromName(const std::string& name, uint32_t& contractId, CDInfo::Entry& cd);
}

#endif // TRADELAYER_CE_H
