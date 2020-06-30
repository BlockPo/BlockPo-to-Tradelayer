#include <tradelayer/dex.h>
#include <test/test_bitcoin.h>
#include <boost/test/unit_test.hpp>
#include <stdint.h>

using namespace mastercore;

BOOST_FIXTURE_TEST_SUITE(tradelayer_dex_functions_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(legacy_calculateDesiredBTC)
{

    // 1 LTC = 1 ALL
    int64_t amountOffered = 400000000; // 4 ALL
    int64_t amountDesired = 400000000; // 4 LTC
    int64_t amountAvailable = 300000000; // 3 ALL (left to offer)

    // checking ((amountDesiredOfLitecoin * amountAvailableOfTokens) / initialAmountOfTokens)
    BOOST_CHECK_EQUAL(300000000, legacy::calculateDesiredLTC(amountOffered, amountDesired, amountAvailable));


    // 10 LTC = 1 ALL
    amountOffered = 500000000; // 5 ALL
    amountDesired = 5000000000; // 50 LTC
    amountAvailable = 200000000; // 2 ALL (left to offer)
    BOOST_CHECK_EQUAL(2000000000, legacy::calculateDesiredLTC(amountOffered, amountDesired, amountAvailable));

    // 1 LTC = 10 ALL
    amountOffered = 5000000000; // 50 ALL
    amountDesired = 500000000; // 5 LTC
    amountAvailable = 2500000000; // 25 ALL (left to offer)
    BOOST_CHECK_EQUAL(250000000, legacy::calculateDesiredLTC(amountOffered, amountDesired, amountAvailable));

    // 1 LTC = 1 ALL
    amountOffered = numeric_limits<int64_t>::max(); // 92,233,720,368.54775807 ALL
    amountDesired = numeric_limits<int64_t>::max(); // 92,233,720,368.54775807 LTC
    amountAvailable = 50000000000; // 500 ALL (left to offer)

    //NOTE: here's an multiplication Overflow
    BOOST_CHECK_EQUAL(2, legacy::calculateDesiredLTC(amountOffered, amountDesired, amountAvailable));
}

BOOST_AUTO_TEST_CASE(calculateDesiredLTC)
{

    // 1 LTC = 1 ALL
    int64_t amountOffered = 400000000; // 4 ALL
    int64_t amountDesired = 400000000; // 4 LTCs
    int64_t amountAvailable = 300000000; // 3 ALL (left to offer)

    // checking ((amountDesiredOfBitcoin * amountAvailableOfTokens) / initialAmountOfTokens)
    BOOST_CHECK_EQUAL(300000000, mastercore::calculateDesiredLTC(amountOffered, amountDesired, amountAvailable));


    // 10 LTC = 1 ALL
    amountOffered = 500000000; // 5 ALL
    amountDesired = 5000000000; // 50 LTCs
    amountAvailable = 200000000; // 2 ALL (left to offer)
    BOOST_CHECK_EQUAL(2000000000, mastercore::calculateDesiredLTC(amountOffered, amountDesired, amountAvailable));

    // 1 LTC = 10 ALL
    amountOffered = 5000000000; // 50 ALL
    amountDesired = 500000000; // 5 LTCs
    amountAvailable = 2500000000; // 25 ALL (left to offer)
    BOOST_CHECK_EQUAL(250000000, mastercore::calculateDesiredLTC(amountOffered, amountDesired, amountAvailable));

    // 1 LTC = 1 ALL
    amountOffered = numeric_limits<int64_t>::max(); // 92,233,720,368.54775807 ALL
    amountDesired = numeric_limits<int64_t>::max(); // 92,233,720,368.54775807 LTC
    amountAvailable = 50000000000; // 500 ALL (left to offer)

    // Correct amount here
    BOOST_CHECK_EQUAL(50000000000, mastercore::calculateDesiredLTC(amountOffered, amountDesired, amountAvailable));

}

BOOST_AUTO_TEST_SUITE_END()
