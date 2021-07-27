#include <test/test_bitcoin.h>
#include <tradelayer/register.h>

#include <boost/test/unit_test.hpp>
#include <stdint.h>

BOOST_FIXTURE_TEST_SUITE(tradelayer_register_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(empty_register)
{
    Register reg;
    // As general record
    BOOST_CHECK_EQUAL(0, reg.getRecord(0, ENTRY_CPRICE));
    BOOST_CHECK_EQUAL(0, reg.getRecord(0, CONTRACT_POSITION));
    BOOST_CHECK_EQUAL(0, reg.getRecord(0, LIQUIDATION_PRICE));
    BOOST_CHECK_EQUAL(0, reg.getRecord(0, UPNL));
    BOOST_CHECK_EQUAL(0, reg.getRecord(0, MARGIN));

    // RecordType out of range:
    BOOST_CHECK(!reg.updateRecord(0, 1, static_cast<RecordType>(14)));
    BOOST_CHECK(!reg.updateRecord(0, 1, static_cast<RecordType>(15)));

    BOOST_CHECK_EQUAL(0, reg.init());
    BOOST_CHECK_EQUAL(0, reg.next());

}

BOOST_AUTO_TEST_CASE(filled_register)
{
    Register reg;
    BOOST_CHECK(!reg.updateRecord(0, 0, ENTRY_CPRICE));
    BOOST_CHECK(!reg.updateRecord(0, 0, CONTRACT_POSITION));
    BOOST_CHECK(!reg.updateRecord(0, 0, LIQUIDATION_PRICE));
    BOOST_CHECK(!reg.updateRecord(0, 0, UPNL));
    BOOST_CHECK(!reg.updateRecord(0, 0, MARGIN));

    BOOST_CHECK_EQUAL(0, reg.getRecord(0, ENTRY_CPRICE));
    BOOST_CHECK_EQUAL(0, reg.getRecord(0, CONTRACT_POSITION));
    BOOST_CHECK_EQUAL(0, reg.getRecord(0, LIQUIDATION_PRICE));
    BOOST_CHECK_EQUAL(0, reg.getRecord(0, UPNL));
    BOOST_CHECK_EQUAL(0, reg.getRecord(0, MARGIN));

    BOOST_CHECK(reg.updateRecord(0, 1, ENTRY_CPRICE));
    BOOST_CHECK(reg.updateRecord(0, -1000, CONTRACT_POSITION));
    BOOST_CHECK(reg.updateRecord(1, int64_t(9223372036854775807LL), LIQUIDATION_PRICE));
    BOOST_CHECK(reg.updateRecord(2, -233000, UPNL));
    BOOST_CHECK(reg.updateRecord(5, int64_t(4294967296L), MARGIN));


    BOOST_CHECK_EQUAL(reg.getRecord(0, ENTRY_CPRICE), 1);
    BOOST_CHECK_EQUAL(reg.getRecord(0, CONTRACT_POSITION), -1000);
    BOOST_CHECK_EQUAL(reg.getRecord(1, LIQUIDATION_PRICE), int64_t(9223372036854775807LL));
    BOOST_CHECK_EQUAL(reg.getRecord(2, UPNL), -233000);
    BOOST_CHECK_EQUAL(reg.getRecord(5, MARGIN), int64_t(4294967296L));


    /**
     * Note:
     * The internal iterator must be replaced via init(),
     * after inserting a new entry via updateRecord().
     */
    BOOST_CHECK_EQUAL(0, reg.init());
    BOOST_CHECK_EQUAL(0, reg.next());
    BOOST_CHECK_EQUAL(1, reg.next());
    BOOST_CHECK_EQUAL(2, reg.next());
    BOOST_CHECK_EQUAL(5, reg.next());
    BOOST_CHECK_EQUAL(0, reg.init());
    BOOST_CHECK_EQUAL(0, reg.next());
    BOOST_CHECK_EQUAL(1, reg.next());
}

BOOST_AUTO_TEST_CASE(insert_entry)
{
    Register reg;
    // contractId, amount of contracts, price
    BOOST_CHECK(reg.insertEntry(0, 1000, 200000000000));
    BOOST_CHECK(reg.insertEntry(0, 2000, 300000000000));
    BOOST_CHECK(reg.insertEntry(0, 3000, 400000000000));

    // checking entry price for full position
    BOOST_CHECK_EQUAL(333333333334, reg.getPosEntryPrice(0));


}

BOOST_AUTO_TEST_CASE(decrease_entry_same_position)
{
    Register reg;
    // contractId, amount of contracts, price
    BOOST_CHECK(reg.insertEntry(0, 1000, 200000000000));
    BOOST_CHECK(reg.insertEntry(0, 2000, 300000000000));
    BOOST_CHECK(reg.insertEntry(0, 3000, 400000000000));

    BOOST_CHECK(reg.decreasePosRecord(0, 1000));

    //Now we have just 5000 contracts, let's check the new entry price
    BOOST_CHECK_EQUAL(360000000000, reg.getPosEntryPrice(0));

}

BOOST_AUTO_TEST_CASE(increase_entry_same_position)
{
    Register reg;

    BOOST_CHECK(reg.insertEntry(0, 1000, 200000000000));
    BOOST_CHECK(reg.insertEntry(0, 2000, 300000000000));

    // entry price: 2666.666
    BOOST_CHECK_EQUAL(266666666667, reg.getPosEntryPrice(0));

    BOOST_CHECK(reg.insertEntry(0, 3000, 400000000000));
    BOOST_CHECK(reg.insertEntry(0, 4000, 500000000000));

    // entry price: 4000
    BOOST_CHECK_EQUAL(400000000000, reg.getPosEntryPrice(0));

}


BOOST_AUTO_TEST_CASE(decrease_changing_position)
{
    Register reg;

    BOOST_CHECK(reg.insertEntry(0, 1000, 200000000000));
    BOOST_CHECK(reg.insertEntry(0, 2000, 300000000000));

    // entry price: 2666.666
    BOOST_CHECK_EQUAL(266666666667, reg.getPosEntryPrice(0));

    BOOST_CHECK(reg.insertEntry(0, 3000, 400000000000));
    BOOST_CHECK(reg.insertEntry(0, 4000, 500000000000));

    // entry price: 4000
    BOOST_CHECK_EQUAL(400000000000, reg.getPosEntryPrice(0));


    //At this point, we have 10000 contracts, we are gonna sell 11000 at price 6000
    BOOST_CHECK(reg.decreasePosRecord(0, 11000, 600000000000));

    // we should have a position of 1000 (short) at price 6000
    BOOST_CHECK_EQUAL(600000000000, reg.getPosEntryPrice(0));

}


BOOST_AUTO_TEST_SUITE_END()
