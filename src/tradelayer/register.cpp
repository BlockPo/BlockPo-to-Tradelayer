#include <tradelayer/register.h>

#include <tradelayer/log.h>
#include <tradelayer/tradelayer.h>

#include <stdint.h>
#include <map>

CCriticalSection cs_register;

// list of all amounts for all addresses for all contracts, map is unsorted
std::unordered_map<std::string, Register> mastercore::mp_register_map;

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
 * Updates amount for the given record type.
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

/**
 *  Inserting price and amount
 *
 */
bool Register::insertEntry(uint32_t contractId, int64_t amount, int64_t price)
{
    bool bRet = false;

    RecordMap::iterator it = mp_record.find(contractId);

    if (it != mp_record.end()) {
        std::pair<int64_t,int64_t> p (amount, price);
        PositionRecord& record = it->second;
        Entries& entries = record.entries;
        entries.push_back(p);
        bRet = true;
    }

    return bRet;

}

// Entry price for liquidations
int64_t Register::getEntryPrice(uint32_t contractId, int64_t amount)
{
    RecordMap::iterator it = mp_record.find(contractId);

    if (it != mp_record.end())
    {
        PositionRecord& record = it->second;
        Entries& entries = record.entries;

        int64_t total = 0;

        // setting remaining
        int64_t& remaining = amount;

        auto it = entries.begin();

        while(remaining > 0) {
          // process to calculate it
          std::pair<int64_t,int64_t>& p = *it;

          const int64_t& ramount = p.first;
          const int64_t& rprice = p.second;

          if (0 == ramount || 0 == rprice ) {
              ++it;
          }

          int64_t part = 0;

          if(remaining - ramount >= 0) {
              part = ramount;
              // entries.erase(it);
          } else {
              part = remaining;
              // ramount -= remaining;
          }

          total += part * rprice;
          PrintToLog("%s(): part: %d, total: %d\n",__func__, part, total);
          remaining -= part;
          PrintToLog("%s(): remaining after loop: %d\n",__func__, remaining);
          PrintToLog("-------------------------------------\n\n");
        }

        const int64_t newPrice = total / amount;

        PrintToLog("%s(): total sum: %d, amount: %d, newPrice: %d\n",__func__, total, amount, newPrice);

        return newPrice;
    }


    return false;
}


// Decrease Position Record
bool Register::decreasePosRecord(uint32_t contractId, int64_t amount)
{
    bool bRet = false;
    RecordMap::iterator it = mp_record.find(contractId);

    if (it != mp_record.end())
    {
        PositionRecord& record = it->second;
        Entries& entries = record.entries;

        // setting remaining
        int64_t& remaining = amount;

        auto it = entries.begin();

        while(amount > 0)
        {
            // process to calculate it
            std::pair<int64_t,int64_t>& p = *it;

            int64_t& ramount = p.first;
            const int64_t& rprice = p.second;

            if (0 == ramount || 0 == rprice ) {
                ++it;
            }

            if(remaining - ramount >= 0)
            {
                entries.erase(it);
                // smaller remaining
                remaining -= ramount;
            } else {
                // there's nothing left
                ramount -= remaining;
                remaining = 0;
            }

        }

        bRet = true;

    }


    return bRet;
}

// Entry price for full position
int64_t Register::getPosEntryPrice(uint32_t contractId)
{
    int64_t price = 0;
    RecordMap::iterator it = mp_record.find(contractId);

    if (it != mp_record.end())
    {
        PositionRecord& record = it->second;
        Entries& entries = record.entries;
        int64_t total = 0;  // contracts * price
        int64_t amount = 0; // sum of contracts

        for(const auto& i : entries)
        {
            const int64_t& ramount = i.first;
            const int64_t& rprice = i.second;

            PrintToLog("%s(): ramount: %d, rprice: %d\n",__func__, ramount, rprice);

            // maybe we need to use arith_uint256 here
            total += ramount * rprice;
            amount += ramount;
        }

        price = total / amount;

    }


    return price;
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


// look at Record for an address, for a given contractId
int64_t mastercore::getContractRecord(const std::string& address, uint32_t contractId, RecordType ttype)
{
    int64_t balance = 0;
    if (RECORD_TYPE_COUNT <= ttype) {
        return 0;
    }

    LOCK(cs_register);
    const std::unordered_map<std::string, Register>::iterator my_it = mp_register_map.find(address);
    if (my_it != mp_register_map.end()) {
        balance = (my_it->second).getRecord(contractId, ttype);
    }

    return balance;
}

// return true if everything is ok
bool mastercore::update_register_map(const std::string& who, uint32_t contractId, int64_t amount, RecordType ttype)
{
    if (0 == amount) {
        PrintToLog("%s(%s, %u=0x%X, %+d, ttype=%d) ERROR: amount of contracts is zero\n", __func__, who, contractId, contractId, amount, ttype);
        return false;
    }
    if (ttype >= RECORD_TYPE_COUNT) {
        PrintToLog("%s(%s, %u=0x%X, %+d, ttype=%d) ERROR: invalid record type\n", __func__, who, contractId, contractId, amount, ttype);
        return false;
    }

    bool bRet = false;
    int64_t before = 0;
    int64_t after = 0;

    LOCK(cs_register);

    before = mastercore::getContractRecord(who, contractId, ttype);

    std::unordered_map<std::string, Register>::iterator my_it = mp_register_map.find(who);
    if (my_it == mp_register_map.end()) {
        // insert an empty element
        my_it = (mp_register_map.insert(std::make_pair(who, Register()))).first;
    }

    Register& reg = my_it->second;
    bRet = reg.updateRegister(contractId, amount, ttype);

    after = getContractRecord(who, contractId, ttype);
    if (!bRet) {
        assert(before == after);
        PrintToLog("%s(%s, %u=0x%X, %+d, ttype=%d) ERROR: insufficient balance (=%d)\n", __func__, who, contractId, contractId, amount, ttype, before);
    }

    return bRet;
}

bool mastercore::insert_entry(const std::string& who, uint32_t contractId, int64_t amount, int64_t price)
{
    bool bRet = false;
    if (0 == amount) {
        PrintToLog("%s(%s, %u=0x%X, %+d) ERROR: amount of contracts is zero\n", __func__, who, contractId, contractId, amount);
        return bRet;
    }

    LOCK(cs_register);

    std::unordered_map<std::string, Register>::iterator my_it = mp_register_map.find(who);
    if (my_it == mp_register_map.end()) {
        // insert an empty element
        my_it = (mp_register_map.insert(std::make_pair(who, Register()))).first;
    }

    Register& reg = my_it->second;

    bRet = reg.insertEntry(contractId, price, amount);

    // entry price of full position
    const int64_t entryPrice = reg.getPosEntryPrice(contractId);

    PrintToLog("%s(): entryPrice(full position): %d\n",__func__, entryPrice);

    return bRet;
}



bool mastercore::decrease_entry(const std::string& who, uint32_t contractId, int64_t amount, int64_t price)
{
    bool bRet = false;
    if (0 == amount) {
        PrintToLog("%s(%s, %u=0x%X, %+d) ERROR: amount of contracts is zero\n", __func__, who, contractId, contractId, amount);
        return bRet;
    }

    LOCK(cs_register);

    std::unordered_map<std::string, Register>::iterator my_it = mp_register_map.find(who);
    if (my_it == mp_register_map.end()) {
        // insert an empty element
        my_it = (mp_register_map.insert(std::make_pair(who, Register()))).first;
    }

    Register& reg = my_it->second;

    bRet = reg.decreasePosRecord(contractId, amount);

    return bRet;
}
