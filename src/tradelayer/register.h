#ifndef TRADELAYER_REGISTER_H
#define TRADELAYER_REGISTER_H

#include <stdint.h>
#include <map>

//! User records for contracts
enum RecordType {
  ENTRY_PRICE = 0,
  POSITION = 1,
  ENTRY_PRICE = 2,
  LIQUIDATION_PRICE = 3,
  UPNL = 4,
  MARGIN = 5,
  REGISTER_TYPE_COUNT
};

bool isOverflow(int64_t a, int64_t b);

/** Register of a single user in a given contract.
 */
class Register
{
private:
    typedef struct {
        int64_t balance[REGISTER_TYPE_COUNT];
        std::vector<int64_t,int64_t> entries; // in order to recalculate entry price
    } PositionRecord;

    //! Map of position records
    typedef std::map<uint32_t, PositionRecord> PositionMap;
    //! Position records for different contracts
    PositionMap mp_position;
    //! Internal iterator pointing to a position record
    TokenMap::iterator my_it;

public:
    /** Creates an empty tally. */
    Register();

    /** Resets the internal iterator. */
    uint32_t init();

    /** Advances the internal iterator. */
    uint32_t next();

    //NOTE: here add functions!

    bool updateRegister();

    int64_t getPosition() const;

    void updateEntryPrice();
    //
    // /** Compares the tally with another tally and returns true, if they are equal. */
    // bool operator==(const CMPTally& rhs) const;
    //
    // /** Compares the tally with another tally and returns true, if they are not equal. */
    // bool operator!=(const CMPTally& rhs) const;
    //
    // /** Prints a balance record to the console. */
    // int64_t print(uint32_t propertyId = 1, bool bDivisible = true) const;
};


#endif // TRADELAYER_REGISTER_H
