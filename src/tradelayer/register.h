#ifndef TRADELAYER_REGISTER_H
#define TRADELAYER_REGISTER_H

#include <stdint.h>
#include <map>
#include <vector>

//! User records for contracts
enum RecordType {
  ENTRY_CPRICE = 0,
  CONTRACT_POSITION = 1,
  LIQUIDATION_PRICE = 2,
  UPNL = 3,
  MARGIN = 4,
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

    bool updateRegister(uint32_t contractId, int64_t amount, RecordType ttype);

    void insertEntry(uint32_t contractId, int64_t amount, int64_t price);

    bool updateEntryPrice(uint32_t contractId, int64_t amount);

    int64_t getRecord(uint32_t contractId, RecordType ttype) const;

    /** Compares the tally with another tally and returns true, if they are equal. */
    bool operator==(const Register& rhs) const;

};


#endif // TRADELAYER_REGISTER_H
