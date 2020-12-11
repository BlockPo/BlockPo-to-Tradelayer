#include <test/test_bitcoin.h>
#include <tradelayer/consensushash.h>
#include <tradelayer/rpcrequirements.h>
#include <tradelayer/rules.h>
#include <tradelayer/sp.h>
#include <tradelayer/tally.h>
#include <tradelayer/tradelayer.h>
#include <tradelayer/tx.h>
#include <tradelayer/uint256_extensions.h>
#include <util/strencodings.h>

#include <univalue.h>

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

BOOST_FIXTURE_TEST_SUITE(tradelayer_rpcrequirements, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(requirebalance)
{
    const uint32_t propertyId = 1;
    const int64_t amount = 99999;
    const std::string address = "moCYruRphhYgejzH75bxWD49qRFan8eGES";

    BOOST_CHECK(mastercore::update_tally_map(address,propertyId, amount, BALANCE));

    BOOST_CHECK_EQUAL(99999, getMPbalance(address, propertyId, BALANCE));

    const int64_t balanceUnconfirmed = getUserAvailableMPbalance(address, propertyId);

    BOOST_CHECK_EQUAL(99999,balanceUnconfirmed);
}

// no inverse quoted
BOOST_AUTO_TEST_CASE(collateral)
{
    const int64_t uPrice = 100000000; // 1
    const uint32_t propertyId = 4;
    const int64_t contract_amount = 2000;
    const int64_t amount = 400000000000; // 4000 units
    const uint64_t leverage = 2;
    const std::string address = "moCYruRphhYgejzH75bxWD49qRFan8eGES";
    const uint64_t margin_requirement = 100000000;  // 1

    BOOST_CHECK(mastercore::update_tally_map(address, propertyId, amount, BALANCE));

    const arith_uint256 amountTR = (ConvertTo256(COIN) * mastercore::ConvertTo256(contract_amount) * mastercore::ConvertTo256(margin_requirement)) / (mastercore::ConvertTo256(leverage) * mastercore::ConvertTo256(uPrice));
    const int64_t amountToReserve = mastercore::ConvertTo64(amountTR);

    const int64_t nBalance = getMPbalance(address, propertyId, BALANCE);

    BOOST_CHECK_EQUAL(400000000000,nBalance);

    BOOST_CHECK_EQUAL(100000000000,amountToReserve);

    BOOST_CHECK(nBalance >= amountToReserve && nBalance > 0);

    const int64_t balanceUnconfirmed = getUserAvailableMPbalance(address, propertyId);

    BOOST_CHECK_EQUAL(400000000000,balanceUnconfirmed);

}

// no inverse quoted
BOOST_AUTO_TEST_CASE(collateral_inverse_quoted)
{
    const uint32_t propertyId = 4;
    const int64_t contract_amount = 2000;
    const int64_t amount = 400000000000; // 4000 units
    const uint64_t leverage = 2;
    const std::string address = "moCYruRphhYgejzH75bxWD49qRFan8eGEx";
    // rational_t conv = rational_t(1,1);

    const uint64_t margin_requirement = 100000000;  // 1

    // BTC/dUSD , 1 BTC = $10000
    const int64_t uPrice = 1000000000000;

    BOOST_CHECK(mastercore::update_tally_map(address, propertyId, amount, BALANCE));

    const arith_uint256 amountTR = (ConvertTo256(COIN) * mastercore::ConvertTo256(contract_amount) * mastercore::ConvertTo256(margin_requirement)) / (mastercore::ConvertTo256(leverage) * mastercore::ConvertTo256(uPrice));
    const int64_t amountToReserve = mastercore::ConvertTo64(amountTR);

    const int64_t nBalance = getMPbalance(address, propertyId, BALANCE);

    BOOST_CHECK_EQUAL(400000000000,nBalance);

    BOOST_CHECK_EQUAL(10000000,amountToReserve);

    BOOST_CHECK(nBalance >= amountToReserve && nBalance > 0);

    const int64_t balanceUnconfirmed = getUserAvailableMPbalance(address, propertyId);

    BOOST_CHECK_EQUAL(400000000000,balanceUnconfirmed);

}

BOOST_AUTO_TEST_SUITE_END()
