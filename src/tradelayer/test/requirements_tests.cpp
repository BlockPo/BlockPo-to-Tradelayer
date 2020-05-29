#include "tradelayer/rpcrequirements.h"
#include "tradelayer/tally.h"
#include "tradelayer/tradelayer.h"
#include "utilstrencodings.h"
#include "tradelayer/rules.h"
#include "tradelayer/sp.h"
#include "tradelayer/tx.h"
#include "tradelayer/uint256_extensions.h"
#include "tradelayer/consensushash.h"
#include "test/test_bitcoin.h"

#include <univalue.h>

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/rational.hpp>
#include <boost/test/unit_test.hpp>

#include <assert.h>
#include <stdint.h>

#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace mastercore;

BOOST_FIXTURE_TEST_SUITE(tradelayer_rpcrequirements, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(requirebalance)
{
    uint32_t propertyId = 1;
    int64_t amount = 99999;
    std::string address = "moCYruRphhYgejzH75bxWD49qRFan8eGES";

    BOOST_CHECK(mastercore::update_tally_map(address,propertyId, amount, BALANCE));

    BOOST_CHECK_EQUAL(99999, getMPbalance(address, propertyId, BALANCE));

    int64_t balanceUnconfirmed = getUserAvailableMPbalance(address, propertyId);

    BOOST_CHECK_EQUAL(99999,balanceUnconfirmed);
}

// no inverse quoted
BOOST_AUTO_TEST_CASE(collateral)
{
    int64_t uPrice = 100000000; // 1
    uint32_t propertyId = 4;
    int64_t contract_amount = 2000;
    int64_t amount = 400000000000; // 4000 units
    uint64_t leverage = 2;
    std::string address = "moCYruRphhYgejzH75bxWD49qRFan8eGES";
    uint32_t margin_requirement = 100000000;  // 1

    BOOST_CHECK(mastercore::update_tally_map(address, propertyId, amount, BALANCE));

    arith_uint256 amountTR = (ConvertTo256(COIN) * mastercore::ConvertTo256(contract_amount) * mastercore::ConvertTo256(margin_requirement)) / (mastercore::ConvertTo256(leverage) * mastercore::ConvertTo256(uPrice));
    int64_t amountToReserve = mastercore::ConvertTo64(amountTR);

    int64_t nBalance = getMPbalance(address, propertyId, BALANCE);

    BOOST_CHECK_EQUAL(400000000000,nBalance);

    BOOST_CHECK_EQUAL(100000000000,amountToReserve);

    BOOST_CHECK(nBalance >= amountToReserve && nBalance > 0);

    int64_t balanceUnconfirmed = getUserAvailableMPbalance(address, propertyId);

    BOOST_CHECK_EQUAL(400000000000,balanceUnconfirmed);

}

// no inverse quoted
BOOST_AUTO_TEST_CASE(collateral_inverse_quoted)
{
    uint32_t propertyId = 4;
    int64_t contract_amount = 2000;
    int64_t amount = 400000000000; // 4000 units
    uint64_t leverage = 2;
    std::string address = "moCYruRphhYgejzH75bxWD49qRFan8eGES";
    // rational_t conv = rational_t(1,1);

    uint32_t margin_requirement = 100000000;  // 1

    // BTC/dUSD , 1 BTC = $10000
    int64_t uPrice = 1000000000000;

    BOOST_CHECK(mastercore::update_tally_map(address, propertyId, amount, BALANCE));

    arith_uint256 amountTR = (ConvertTo256(COIN) * mastercore::ConvertTo256(contract_amount) * mastercore::ConvertTo256(margin_requirement)) / (mastercore::ConvertTo256(leverage) * mastercore::ConvertTo256(uPrice));
    int64_t amountToReserve = mastercore::ConvertTo64(amountTR);

    int64_t nBalance = getMPbalance(address, propertyId, BALANCE);

    BOOST_CHECK_EQUAL(400000000000,nBalance);

    BOOST_CHECK_EQUAL(10000000,amountToReserve);

    BOOST_CHECK(nBalance >= amountToReserve && nBalance > 0);

    int64_t balanceUnconfirmed = getUserAvailableMPbalance(address, propertyId);

    BOOST_CHECK_EQUAL(400000000000,balanceUnconfirmed);

}

BOOST_AUTO_TEST_SUITE_END()
