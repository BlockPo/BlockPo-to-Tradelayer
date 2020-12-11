#include <tradelayer/consensushash.h>
#include <tradelayer/errors.h>
#include <tradelayer/log.h>
#include <tradelayer/rules.h>
#include <tradelayer/sp.h>
#include <tradelayer/tally.h>
#include <tradelayer/tradelayer.h>
#include <tradelayer/tx.h>
#include <tradelayer/uint256_extensions.h>

#include <arith_uint256.h>
#include <chain.h>
#include <test/test_bitcoin.h>
#include <tinyformat.h>
#include <uint256.h>
#include <univalue.h>
#include <util/strencodings.h>

#include <assert.h>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <stdint.h>
#include <string>
#include <vector>

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/rational.hpp>
#include <boost/test/unit_test.hpp>

using namespace mastercore;

BOOST_FIXTURE_TEST_SUITE(functions_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(one_trade)
{
    int64_t oldPos = 100000000;
    int64_t newPos = 100000000;
    std::string result = updateStatus(oldPos, newPos);
    // No change in position
    BOOST_CHECK_EQUAL(result, "None");


    oldPos = 0;
    newPos = 0;
    result = updateStatus(oldPos, newPos);
    BOOST_CHECK_EQUAL(result, "None");


    oldPos = 0;
    newPos = 100000000;

    int signOld = boost::math::sign(oldPos);
    int signNew = boost::math::sign(newPos);

    BOOST_CHECK_EQUAL(signOld, 0);
    BOOST_CHECK_EQUAL(signNew, 1);


    result = updateStatus(oldPos, newPos);
    // Open long position
    BOOST_CHECK_EQUAL(result, "OpenLongPosition");


    oldPos = 0;
    newPos = -100000000;
    result = updateStatus(oldPos, newPos);
    // Open short position
    BOOST_CHECK_EQUAL(result, "OpenShortPosition");


    oldPos = 300000000;
    newPos = 400000000;
    result = updateStatus(oldPos, newPos);
    // Increasing long position
    BOOST_CHECK_EQUAL(result, "LongPosIncreased");


    oldPos = -300000000;
    newPos = -400000000;
    result = updateStatus(oldPos, newPos);
    // Increasing short position
    BOOST_CHECK_EQUAL(result, "ShortPosIncreased");


    oldPos = 500000000;
    newPos = 200000000;
    result = updateStatus(oldPos, newPos);
    BOOST_CHECK_EQUAL(result, "LongPosNettedPartly");


    oldPos = -600000000;
    newPos = -200000000;
    result = updateStatus(oldPos, newPos);
    BOOST_CHECK_EQUAL(result, "ShortPosNettedPartly");


    oldPos = -600000000;
    newPos =  0;
    result = updateStatus(oldPos, newPos);
    BOOST_CHECK_EQUAL(result, "ShortPosNetted");


    oldPos =  700000000;
    newPos =  0;
    result = updateStatus(oldPos, newPos);
    BOOST_CHECK_EQUAL(result, "LongPosNetted");


    oldPos =  700000000;
    newPos = -900000000;
    result = updateStatus(oldPos, newPos);
    BOOST_CHECK_EQUAL(result, "OpenShortPosByLongPosNetted");

    oldPos =  -700000000;
    newPos =   900000000;
    result = updateStatus(oldPos, newPos);
    BOOST_CHECK_EQUAL(result, "OpenLongPosByShortPosNetted");

}

BOOST_AUTO_TEST_SUITE_END()
