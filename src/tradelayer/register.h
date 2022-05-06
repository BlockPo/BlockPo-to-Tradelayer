#ifndef TRADELAYER_REGISTER_H
#define TRADELAYER_REGISTER_H

#include <tradelayer/ce.h>

#include <stdint.h>
#include <sync.h>
#include <map>
#include <queue>
#include <unordered_map>
#include <univalue.h>

//! Global lock for state objects
extern CCriticalSection cs_register;

//! User records for contracts
enum RecordType {
  ENTRY_CPRICE = 0,
  CONTRACT_POSITION = 1,
  BANKRUPTCY_PRICE = 2,
  UPNL = 3,
  MARGIN = 4,
  LEVERAGE = 5,
  CONTRACT_RESERVE = 6, // for synthetic/pegged currency
  RECORD_TYPE_COUNT
};

extern bool isOverflow(int64_t a, int64_t b);

// in order to recalculate entry price (FIFO)
typedef std::vector<std::pair<int64_t,int64_t>> Entries;

/** Register of a single user in a given contract.
 */
class Register
{
private:
    typedef struct {
        int64_t balance[RECORD_TYPE_COUNT];
        Entries entries;
    } PositionRecord;

    //! Map of position records
    typedef std::map<uint32_t, PositionRecord> RecordMap;
    //! Position records for different contracts
    RecordMap mp_record;
    //! Internal iterator pointing to a position record
    RecordMap::iterator my_it;


public:
    /** Creates an empty register. */
    Register();

    /** Resets the internal iterator. */
    uint32_t init();

    /** Advances the internal iterator. */
    uint32_t next();

    /** Check if it's the first element of record. */
    bool isBegin();

    bool updateRecord(uint32_t contractId, int64_t amount, RecordType ttype);

    bool insertEntry(uint32_t contractId, int64_t amount, int64_t price);

    bool decreasePosRecord(uint32_t contractId, int64_t amount, int64_t price = 0);

    int64_t getEntryPrice(uint32_t contractId, int64_t amount) const;

    int64_t getPosEntryPrice(uint32_t contractId) const;

    int64_t getRecord(uint32_t contractId, RecordType ttype) const;

    int64_t getPosExitPrice(const uint32_t contractId, bool isOracle) const;

    int64_t getUPNL(const uint32_t contractId, const uint32_t notionalSize, bool isOracle = false, bool quoted = false) const;

    bool setBankruptcyPrice(const uint32_t contractId, const uint32_t notionalSize, int64_t initMargin, bool isOracle = false, bool quoted = false);

    int64_t getLiquidationPrice(const uint32_t contractId, const uint32_t notionalSize, const uint64_t marginRequirement) const;

    const Entries* getEntries(const uint32_t contractId) const;

    /** Compares the tally with another tally and returns true, if they are equal. */
    bool operator==(const Register& rhs) const;

};


namespace mastercore
{
  extern std::unordered_map<std::string, Register> mp_register_map;

  int64_t getContractRecord(const std::string& address, uint32_t contractId, RecordType ttype);

  bool update_register_map(const std::string& who, uint32_t contractId, int64_t amount, RecordType ttype);

  bool insert_entry(const std::string& who, uint32_t contractId, int64_t amount, int64_t price);

  bool decrease_entry(const std::string& who, uint32_t contractId, int64_t amount, int64_t price = 0);

  bool getFullContractRecord(const std::string& address, uint32_t contractId, UniValue& position_obj, const CDInfo::Entry& cd);

  bool reset_leverage_register(const std::string& who, uint32_t contractId);

  bool realize_pnl(uint32_t contractId, uint32_t notional_size, bool isOracle, bool isInverseQuoted);

  bool set_bankruptcy_price_onmap(const std::string& who, const uint32_t& contractId, const uint32_t& notionalSize, const int64_t& initMargin);
}

#endif // TRADELAYER_REGISTER_H
