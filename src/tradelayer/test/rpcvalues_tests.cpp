#include <test/test_bitcoin.h>
#include <tradelayer/consensushash.h>
#include <tradelayer/parse_string.h>
#include <tradelayer/rpcvalues.h>
#include <tradelayer/rules.h>
#include <tradelayer/sp.h>
#include <tradelayer/tally.h>
#include <tradelayer/tradelayer.h>
#include <tradelayer/tx.h>
#include <tradelayer/uint256_extensions.h>
#include <util/strencodings.h>

#include <assert.h>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <stdint.h>
#include <string>
#include <univalue.h>
#include <vector>

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/rational.hpp>
#include <boost/test/unit_test.hpp>

using namespace mastercore;

BOOST_FIXTURE_TEST_SUITE(tradelayer_rpcvalues, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(contract_amount)
{
    UniValue amount("1.45");
    BOOST_CHECK_EQUAL(1, ParseAmountContract(amount));

    amount = "100";
    BOOST_CHECK_EQUAL(100, ParseAmountContract(amount));

    amount = "158.89";
    BOOST_CHECK_EQUAL(158, ParseAmountContract(amount));

    amount = "123456";
    BOOST_CHECK_EQUAL(123456, ParseAmountContract(amount));

}

// proof of concept function
uint32_t pNameOrId(const UniValue& value)
{
    const int64_t amount = mastercore::StrToInt64(value.get_str(), false);

    if (amount != 0){
        // here, code validate if propertyId is valid
        return static_cast<int64_t>(amount);
    }

    // here code search for a valid propertyId given a contract name.
    const uint32_t propertyId = 9;

    return propertyId;
}

BOOST_AUTO_TEST_CASE(parse_name_id)
{

    UniValue amount("Contract 1");
    BOOST_CHECK_EQUAL(9, pNameOrId(amount));

    amount ="Oracles 3";
    BOOST_CHECK_EQUAL(9, pNameOrId(amount));

    amount = "1";
    BOOST_CHECK_EQUAL(1, pNameOrId(amount));

    amount = "2";
    BOOST_CHECK_EQUAL(2, pNameOrId(amount));

    amount = "3";
    BOOST_CHECK_EQUAL(3, pNameOrId(amount));

    amount = "4.5";
    BOOST_CHECK_EQUAL(4, pNameOrId(amount));

    amount = "8985.6966552121";
    BOOST_CHECK_EQUAL(8985, pNameOrId(amount));

}

BOOST_AUTO_TEST_SUITE_END()
