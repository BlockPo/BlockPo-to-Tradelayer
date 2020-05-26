#include "tradelayer/rpcvalues.h"
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



BOOST_AUTO_TEST_SUITE_END()
