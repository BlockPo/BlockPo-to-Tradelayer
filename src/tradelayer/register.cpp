#include <tradelayer/register.h>
#include <tradelayer/ce.h>
#include <tradelayer/log.h>
#include <tradelayer/tradelayer.h>
#include <tradelayer/uint256_extensions.h>
#include <tradelayer/externfns.h>

#include <stdint.h>
#include <map>

using namespace mastercore;

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

bool Register::isBegin()
{
    if (my_it == mp_record.begin()) {
        return true;
    } else {
        return false;
    }
}

int64_t Register::getPosExitPrice(const uint32_t contractId, bool isOracle) const
{
    if(isOracle)
    {
        return getOracleTwap(contractId, OL_BLOCKS);
    }

    // refine this: native contracts mark price
    return 0;
}

int64_t Register::getLiquidationPrice(const uint32_t contractId, const uint32_t notionalSize, const uint64_t marginRequirement) const
{
    int64_t liqPrice  = 0;

    const int64_t position = getRecord(contractId, CONTRACT_POSITION);
    const int64_t entryPrice = getPosEntryPrice(contractId);

    const int64_t marginNeeded = (int64_t) position *  marginRequirement;
    const double firstFactor = (double) marginNeeded / (position * notionalSize);
    const double secondFactor = 0;
    if(entryPrice != 0){
      const double secondFactor = (double) COIN / entryPrice;
    }
    const double denominator =  firstFactor + secondFactor;

    const double dliqPrice = (denominator != 0) ? 1 / denominator : 0;

    if (0 < dliqPrice)
    {
        PrintToLog("%s(): liqPrice is less than zero: %lf, liquidation price = 0 !\n",__func__, dliqPrice);
    } else {
        liqPrice = (int64_t) dliqPrice * COIN;
    }


    PrintToLog("%s(): liqPrice (double) : %d, liqPrice (int) : %d\n",__func__, dliqPrice, liqPrice);

    return liqPrice;

}

int64_t Register::getUPNL(const uint32_t contractId, const uint32_t notionalSize, bool isOracle, bool quoted)
{

    const int64_t position = getRecord(contractId, CONTRACT_POSITION);
    const int64_t entryPrice = getPosEntryPrice(contractId);
    const int64_t exitPrice  = getPosExitPrice(contractId, isOracle);

    if(msc_debug_liquidation_enginee)
    {
         PrintToLog("%s(): inside getUPNL, position: %d. entryPrice: %d, exitPrice: %d, notionalSize: %d\n",__func__, position, entryPrice, exitPrice, notionalSize);
    }

    const arith_uint256 factor = ConvertTo256((abs(position) * notionalSize) / COIN );
    const arith_uint256 dEntryPrice = ConvertTo256(entryPrice / COIN);
    const arith_uint256 dExitPrice = ConvertTo256(exitPrice / COIN);
    const int64_t diff = exitPrice - entryPrice;
    const arith_uint256 dDiff = ConvertTo256(abs(diff));

    PrintToLog("%s(): dEntryPrice: %d, dExitPrice: %d, factor: %d, diff: %d\n",__func__, ConvertTo64(dEntryPrice), ConvertTo64(dExitPrice), ConvertTo64(factor), diff);

    arith_uint256 aUPNL = 0;

    if(quoted)
    {

        aUPNL = (entryPrice != 0 && exitPrice != 0) ?  (dDiff * (factor / dEntryPrice)) / dExitPrice : 0;

    } else {
        aUPNL  = (factor * dDiff);
    }

    // re-converting to int64_t
    const uint64_t uiUPNL = ConvertTo64(aUPNL);
    int64_t iUPNL = 0;

    if ((diff > 0 && position < 0) || (diff < 0 && position > 0)) {
        iUPNL = (int64_t) -uiUPNL;
    } else {
        iUPNL = (int64_t) uiUPNL;
    }

    // updating UPNL Register
    const int64_t oldUPNL = getRecord(contractId, UPNL);
    PrintToLog("%s():oldUPNL: %d, iUPNL: %d\n",__func__, oldUPNL, iUPNL);

    if(oldUPNL != iUPNL) {
        updateRecord(contractId, iUPNL, UPNL);
        PrintToLog("%s():updating record, contractId: %d\n",__func__, contractId);

    }

    if(msc_debug_liquidation_enginee)
    {
       PrintToLog("%s(): entryPrice(int64_t): %d, exitPrice(int64_t): %d, notionalsize: %d\n",__func__, entryPrice, exitPrice, notionalSize);
       PrintToLog("%s():UPNL(int): %d, quoted?: %d\n",__func__, iUPNL, quoted);
    }


    return iUPNL;
}

bool Register::setBankruptcyPrice(const uint32_t contractId, const uint32_t notionalSize, int64_t initMargin, bool isOracle, bool quoted)
{
    const int64_t position = getRecord(contractId, CONTRACT_POSITION);
    const int64_t entryPrice = getPosEntryPrice(contractId);
    const double dBankruptcyPrice  =  (double) 1 / (initMargin / (position * notionalSize) + (1/entryPrice));

    PrintToLog("%s(): bankruptcyPrice: %d\n",__func__, dBankruptcyPrice);

    const int bankruptcyPrice = DoubleToInt64(dBankruptcyPrice);

    return updateRecord(contractId, bankruptcyPrice, BANKRUPTCY_PRICE);
}


/**
 * Updates amount for the given record type.
 *
 * Negative balances are only permitted for contracts amount, PNL and UPNL.
 *
 */
bool Register::updateRecord(uint32_t contractId, int64_t amount, RecordType ttype)
{
    bool fPNL = (ttype == PNL)? true : false;

    if (RECORD_TYPE_COUNT <= ttype || (amount == 0  && !fPNL) ) {
        PrintToLog("%s(): ERROR: ttype (%d) is wrong, or updating amount is zero (%d)\n", __func__, ttype, amount);
        return false;
    }

    bool fUpdated = false;
    int64_t now64 = mp_record[contractId].balance[ttype];

    PrintToLog("%s(): now64: %d\n", __func__, now64);

    if (isOverflow(now64, amount) && fPNL) {
        PrintToLog("%s(): ERROR: arithmetic overflow [%d + %d]\n", __func__, now64, amount);
        return false;
    }

    if ((CONTRACT_POSITION != ttype) && (UPNL != ttype) && (PNL != ttype) && fPNL && ((now64 + amount) < 0)) {
        // NOTE:
        PrintToLog("%s(): ERROR: Negative balances are only permitted for contracts amount, or UPNL\n",__func__);
        return false;
    } else {

        now64 += amount;
        mp_record[contractId].balance[ttype] = (fPNL) ? amount : now64;
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

    // extra protection
    if(amount == 0 || price == 0) {
        return bRet;
    }

    RecordMap::iterator it = mp_record.find(contractId);
    std::pair<int64_t,int64_t> p (amount, price);

    if (it != mp_record.end()) {
        PositionRecord& record = it->second;
        Entries& entries = record.entries;
        entries.push_back(p);
        bRet = true;
    } else {
         PositionRecord posRec;
         Entries& entries = posRec.entries;
         entries.push_back(p);
         mp_record.insert(std::make_pair(contractId, posRec));
         bRet = true;
    }


    return bRet;

}

// Entry price for liquidations
int64_t Register::getEntryPrice(uint32_t contractId, int64_t amount) const
{
    RecordMap::const_iterator it = mp_record.find(contractId);

    if (it != mp_record.end())
    {
        const PositionRecord& record = it->second;
        const Entries& entries = record.entries;

        arith_uint256 total = 0;

        // setting remaining
        int64_t remaining = amount;

        auto itt = entries.begin();

        while(remaining > 0 && itt != entries.end()) {
          // process to calculate it
          const std::pair<int64_t,int64_t>& p = *itt;

          const int64_t& ramount = p.first;
          const int64_t& rprice = p.second;

          int64_t part = 0;

          if(remaining - ramount >= 0) {
              part = ramount;
          } else {
              part = remaining;
          }

          total += ConvertTo256(part * rprice);
          PrintToLog("%s(): part: %d, total: %d\n",__func__, part, ConvertTo64(total));
          remaining -= part;
          PrintToLog("%s(): remaining after loop: %d\n",__func__, remaining);
          PrintToLog("-------------------------------------\n\n");
          ++itt;
        }

        // using 256bits for big calculations
        const arith_uint256 aAmount = ConvertTo256(amount);
        const arith_uint256 aNewPrice = (amount != 0) ? DivideAndRoundUp(total, aAmount) : arith_uint256(0);
        const int64_t newPrice = ConvertTo64(aNewPrice);

        PrintToLog("%s(): total sum: %d, amount: %d, newPrice: %d\n",__func__, ConvertTo64(total), amount, newPrice);

        return newPrice;
    }


    return false;
}


// Decrease Position Record
bool Register::decreasePosRecord(uint32_t contractId, int64_t amount, int64_t price)
{
    bool bRet = false;
    RecordMap::iterator it = mp_record.find(contractId);

    if (it != mp_record.end())
    {
        PositionRecord& record = it->second;
        Entries& entries = record.entries;

        // setting remaining
        int64_t& remaining = amount;

        auto itt = entries.begin();

        while(remaining > 0 && itt != entries.end())
        {
            // process to calculate it
            std::pair<int64_t,int64_t>& p = *itt;

            int64_t& ramount = p.first;
            const int64_t& rprice = p.second;

            if(remaining - ramount >= 0)
            {
                // smaller remaining
                remaining -= ramount;
                PrintToLog("%s():deleting full entry: amount: %d, price: %d\n",__func__, ramount, rprice);
                entries.erase(itt);
            } else {
                // there's nothing left
                ramount -= remaining;
                remaining = 0;
                PrintToLog("%s() there's nothing else (remaining == 0), ramount left: %d, at price: %d\n",__func__, ramount, rprice);
            }

        }

        // closing position and then open a new one on the other side
        if (remaining > 0)
        {
            entries.clear();
            PrintToLog("%s(): closing position and then open a new one on the other side, remaining: %d, price: %d\n",__func__, remaining, price);
            std::pair<int64_t,int64_t> p (remaining, price);
            entries.push_back(p);
        }

        bRet = true;

    }


    return bRet;
}

// Entry price for full position
int64_t Register::getPosEntryPrice(uint32_t contractId) const
{
    int64_t price = 0;
    RecordMap::const_iterator it = mp_record.find(contractId);

    if (it != mp_record.end())
    {
        const PositionRecord& record = it->second;
        const Entries& entries = record.entries;
        arith_uint256 total = 0;  // contracts * price
        int64_t amount = 0; // sum of contracts

        for(const auto& i : entries)
        {
            const int64_t& ramount = i.first;
            const int64_t& rprice = i.second;

            PrintToLog("%s(): ramount: %d, rprice: %d\n",__func__, ramount, rprice);

            total += ConvertTo256(ramount * rprice);
            amount += ramount;
        }

        const arith_uint256 aAmount = ConvertTo64(amount);
        if(amount==0){return 0;}
        const arith_uint256 aPrice = (amount != 0) ? DivideAndRoundUp(total, aAmount) : arith_uint256(0);
        price = ConvertTo64(aPrice);

    }

    return price;
}

/**
 * Returns any record.
 *
 */
int64_t Register::getRecord(uint32_t contractId, RecordType ttype) const
{

    int64_t amount = 0;

    if (RECORD_TYPE_COUNT <= ttype) {
        return amount;
    }


    RecordMap::const_iterator it = mp_record.find(contractId);

    if (it != mp_record.end()) {
        const PositionRecord& record = it->second;
        amount = record.balance[ttype];
    }

    return amount;
}


const Entries* Register::getEntries(const uint32_t contractId) const
{
    const auto it = mp_record.find(contractId);

    if (it != mp_record.end()) {
        const PositionRecord& record = it->second;
        return &(record.entries);
    }

    return static_cast<Entries*>(nullptr);
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

    if((pc1 != mp_record.end()) || (pc2 != rhs.mp_record.end())) {
         return false;
    }


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
    std::unordered_map<std::string, Register>::const_iterator my_it = mp_register_map.find(address);
    if (my_it != mp_register_map.end()) {
        balance = (my_it->second).getRecord(contractId, ttype);
    }

    return balance;
}

bool mastercore::getFullContractRecord(const std::string& address, uint32_t contractId, UniValue& position_obj, const CDInfo::Entry& cd)
{
    LOCK(cs_register);
    std::unordered_map<std::string, Register>::iterator my_it = mp_register_map.find(address);
    if (my_it != mp_register_map.end()) {
        Register& reg = my_it->second;
        //entry price
        const int64_t entryPrice = reg.getPosEntryPrice(contractId);
        if(entryPrice==0){return false;}
        position_obj.pushKV("entry_price", FormatDivisibleMP(entryPrice));
        // position
        const int64_t position = reg.getRecord(contractId, CONTRACT_POSITION);
        position_obj.pushKV("position", FormatIndivisibleMP(position));
        // liquidation price
        const int64_t liqPrice = reg.getLiquidationPrice(contractId, cd.notional_size, cd.margin_requirement);
        position_obj.pushKV("BANKRUPTCY_PRICE", FormatDivisibleMP(liqPrice));
        // position margin
        const int64_t posMargin = reg.getRecord(contractId, MARGIN);
        position_obj.pushKV("position_margin", FormatDivisibleMP(posMargin));
        // upnl
        const int64_t upnl = reg.getUPNL(contractId, cd.notional_size, cd.isOracle(), cd.isInverseQuoted());
        position_obj.pushKV("upnl", FormatDivisibleMP(upnl, true));

        return true;
    }

    return false;
}

// return true if everything is ok
bool mastercore::update_register_map(const std::string& who, uint32_t contractId, int64_t amount, RecordType ttype)
{
    if (0 == amount && ttype != PNL) {
        PrintToLog("%s(%s, %u=0x%X, %+d, ttype=%d) ERROR: amount is zero\n", __func__, who, contractId, contractId, amount, ttype);
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
    bRet = reg.updateRecord(contractId, amount, ttype);

    after = getContractRecord(who, contractId, ttype);

    if (!bRet) {
        if(before != after){
            PrintToLog("%s(): ERROR: Positions should be the same (%s), before (%d), after(%d)\n", __func__, who, before, after);
        }

        PrintToLog("%s(): ERROR: no position updated for (%s), before (%d), after(%d)\n", __func__, who, before, after);
    }

    return bRet;
}

// return true if everything is ok
bool mastercore::set_bankruptcy_price_onmap(const std::string& who, const uint32_t& contractId, const uint32_t& notionalSize, const int64_t& initMargin)
{

    LOCK(cs_register);
    std::unordered_map<std::string, Register>::iterator my_it = mp_register_map.find(who);
    if (my_it == mp_register_map.end()) {
        // insert an empty element
        my_it = (mp_register_map.insert(std::make_pair(who, Register()))).first;
    }

    Register& reg = my_it->second;
    return reg.setBankruptcyPrice(contractId, notionalSize, initMargin);

}

bool mastercore::realize_pnl(uint32_t contractId, uint32_t notional_size, bool isOracle, bool isInverseQuoted, uint32_t collateral_currency)
{
    bool bRet = false;

    LOCK(cs_register);

    for(auto my_it = mp_register_map.begin(); my_it != mp_register_map.end(); ++my_it)
    {
        const std::string& who = my_it->first;
        Register& reg = my_it->second;
        const int64_t upnl = reg.getUPNL(contractId, notional_size, isOracle, isInverseQuoted);
        const int64_t oldPNL = reg.getRecord(contractId, PNL);
        const int64_t newUPNL = upnl - oldPNL;

        PrintToLog("%s(): upnl: %d, oldPNL: %d, newUPNL: %d\n",__func__, upnl, oldPNL, newUPNL);

        if (0 != newUPNL)
        {
            PrintToLog("%s(): updating register map (because newUPNL is not zero)\n",__func__);

            update_register_map(who, contractId, newUPNL, MARGIN);
            update_register_map(who, contractId, newUPNL + oldPNL, PNL);
            update_tally_map(who, collateral_currency, newUPNL, BALANCE);
            bRet = true;
        }
    }

    return bRet;
}

// return true if everything is ok
bool mastercore::reset_leverage_register(const std::string& who, uint32_t contractId)
{
    bool bRet = false;

    LOCK(cs_register);

    const int64_t rleverage = mastercore::getContractRecord(who, contractId, LEVERAGE);

    std::unordered_map<std::string, Register>::iterator my_it = mp_register_map.find(who);
    if (my_it == mp_register_map.end()) {
        PrintToLog("%s(): address (%s) not found for this contract(%d)\n",__func__, who, contractId);
        return bRet;
    }

    Register& reg = my_it->second;

    // cleaning
    bRet = reg.updateRecord(contractId, -rleverage, LEVERAGE);

    // // default leverage : 1
    // bRet2 = reg.updateRecord(contractId, 1, LEVERAGE);

    return bRet;
}

bool mastercore::insert_entry(const std::string& who, uint32_t contractId, int64_t amount, int64_t price)
{
    bool bRet = false;
    if (0 == amount) {
        PrintToLog("%s(%s, %u=0x%X, %+d) ERROR: amount of contracts is zero\n", __func__, who, contractId, contractId, amount);
        return bRet;
    }

    if (0 >= price) {
        PrintToLog("%s(%s, %u=0x%X, %+d) ERROR: price of contracts is zero or less\n", __func__, who, contractId, contractId, amount);
        return bRet;
    }

    LOCK(cs_register);

    std::unordered_map<std::string, Register>::iterator my_it = mp_register_map.find(who);
    if (my_it == mp_register_map.end()) {
        // insert an empty element
        my_it = (mp_register_map.insert(std::make_pair(who, Register()))).first;
    }

    Register& reg = my_it->second;

    bRet = reg.insertEntry(contractId, amount, price);

    // entry price of full position
    reg.getPosEntryPrice(contractId);

    //PrintToLog("%s(): entryPrice(full position): %d, contractId: %d, address: %s\n",__func__, entryPrice, contractId, who);

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

    bRet = reg.decreasePosRecord(contractId, amount, price);

    return bRet;
}
