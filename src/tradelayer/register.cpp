#include <tradelayer/register.h>

#include <tradelayer/log.h>
#include <tradelayer/tradelayer.h>

#include <stdint.h>
#include <map>

/**
 * Creates an empty register.
 */
Register::Register()
{
    my_it = mp_record.begin();
}

/**
 * Resets the internal iterator.
 *
 * @return Identifier of the first register element.
 */
uint32_t Register::init()
{
    uint32_t contractId = 0;
    my_it = mp_record.begin();
    if (my_it != mp_record.end()) {
        contractId = my_it->first;
    }

    return contractId;
}

/**
 * Advances the internal iterator.
 *
 * @return Identifier of the register element before the update.
 */
uint32_t Register::next()
{
    uint32_t ret = 0;
    if (my_it != mp_record.end()) {
        ret = my_it->first;
        ++my_it;
    }
    return ret;
}

/**
 * Updates amount for the given tally type.
 *
 * Negative balances are only permitted for contracts amount.
 *
 *
 */
bool Register::updateRegister(uint32_t contractId, int64_t amount, RecordType ttype)
{
    if (RECORD_TYPE_COUNT <= ttype || amount == 0) {
        return false;
    }

    bool fUpdated = false;
    int64_t now64 = mp_record[contractId].balance[ttype];

    if (isOverflow(now64, amount)) {
        PrintToLog("%s(): ERROR: arithmetic overflow [%d + %d]\n", __func__, now64, amount);
        return false;
    }

    if (CONTRACT_POSITION != ttype &&(now64 + amount) < 0) {
        // NOTE:
        PrintToLog("%s(): ERROR: Negative balances are only permitted for contracts amount\n",__func__);
        return false;
    } else {

        now64 += amount;
        mp_record[contractId].balance[ttype] = now64;

        fUpdated = true;
    }

    return fUpdated;
}

void Register::insertEntry(uint32_t contractId, int64_t amount, int64_t price)
{
    RecordMap::iterator it = mp_record.find(contractId);

    if (it != mp_record.end()) {
        std::pair<int64_t,int64_t> p (amount, price);
        PositionRecord& record = it->second;
        Entries& entries = record.entries;
        entries.push_back(p);
    }

}


bool Register::updateEntryPrice(uint32_t contractId, int64_t amount)
{
    RecordMap::iterator it = mp_record.find(contractId);

    if (it != mp_record.end())
    {
        PositionRecord& record = it->second;
        Entries& entries = record.entries;

        int64_t remaining = 0; // contracts remaining in count
        int64_t total = 0;  // contracts * price counted

        // setting remaining
        remaining = amount;
        auto it = entries.begin();

        while(remaining > 0) {
          // process to calculate it
          const int64_t& ramount = it->first;
          const int64_t& rprice = it->second;

          if (0 == ramount || 0 == rprice ) {
              ++it;
          }

          int64_t part = (remaining - ramount >= 0) ? ramount : remaining;

          total += part * rprice;
          PrintToLog("%s(): part: %d, total: %d\n",__func__, part, total);
          remaining -= part;
          PrintToLog("%s(): remaining after loop: %d\n",__func__, remaining);
          PrintToLog("-------------------------------------\n\n");
          ++it;
        }

        const int64_t newPrice = total / amount;

        PrintToLog("%s(): total sum: %d, amount: %d, newPrice: %d\n",__func__, total, amount, newPrice);

        updateRegister(contractId, newPrice, ENTRY_CPRICE);

        return true;
    }


    return false;
}

/**
 * Returns any record.
 *
 */
int64_t Register::getRecord(uint32_t contractId, RecordType ttype) const
{
    if (RECORD_TYPE_COUNT <= ttype) {
        return 0;
    }

    int64_t amount = 0;
    RecordMap::const_iterator it = mp_record.find(contractId);

    if (it != mp_record.end()) {
        const PositionRecord& record = it->second;
        amount = record.balance[ttype];
    }

    return amount;
}

/**
 * Compares the Register with another Register and returns true, if they are equal.
 *
 * @param rhs  The other Register
 * @return True, if both Registers are equal
 */
bool Register::operator==(const Register& rhs) const
{
    if (mp_record.size() != rhs.mp_record.size()) {
        return false;
    }
    RecordMap::const_iterator pc1 = mp_record.begin();
    RecordMap::const_iterator pc2 = rhs.mp_record.begin();

    for (unsigned int i = 0; i < mp_record.size(); ++i) {
        if (pc1->first != pc2->first) {
            return false;
        }
        const PositionRecord& record1 = pc1->second;
        const PositionRecord& record2 = pc2->second;

        for (int ttype = 0; ttype < RECORD_TYPE_COUNT; ++ttype) {
            if (record1.balance[ttype] != record2.balance[ttype]) {
                return false;
            }
        }
        ++pc1;
        ++pc2;
    }

    assert(pc1 == mp_record.end());
    assert(pc2 == rhs.mp_record.end());

    return true;
}
