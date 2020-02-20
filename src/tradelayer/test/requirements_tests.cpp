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

// BOOST_AUTO_TEST_CASE(payload_send_all)
// {
//     // Send to owners [type 4, version 0]
//     std::vector<unsigned char> vch = CreatePayload_SendAll(
//         static_cast<uint8_t>(2));          // ecosystem: Test
//
//     BOOST_CHECK_EQUAL(HexStr(vch), "000402");
// }



BOOST_AUTO_TEST_SUITE_END()
