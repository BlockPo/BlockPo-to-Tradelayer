#include <tradelayer/consensushash.h>
#include <tradelayer/errors.h>
#include <tradelayer/log.h>
#include <tradelayer/mdex.h>
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

#include <boost/test/unit_test.hpp>

using namespace mastercore;

BOOST_FIXTURE_TEST_SUITE(x_trade_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(one_trade)
{
  CMPTally tally;
  CMPContractDex seller("1dexX7zmPen1yBz2H9ZF62AK5TGGqGTZH", // address
			172, // block
			1,   // property for sale
			5,   // amount of contracts for sale
			0,   // desired property
			0,
			uint256S("11"), // txid
			1,   // position in block
			1,   // subaction
			0,   // amount remaining
			5,   // effective_price
			2,  // trading_action
 10000000);  // amount reserved

  CMPContractDex buyer("1NNQKWM8mC35pBNPxV1noWFZEw7A5X6zXz", // address
		       172,  // block
		       1,    // property for sale
		       5,    // amount of contracts for trade
		       0,    // desired property
		       0,
		       uint256S("12"), // txid
		       2,    // position in block
		       1,    // subaction
		       0,    // amount remaining
		       5,    // effective_price
		       1,   // trading_action
    10000000);  // amount reserved

  CMPContractDex *s; s = &seller;
  CMPContractDex *b; b = &buyer;

  BOOST_CHECK(mastercore::update_tally_map(seller.getAddr(),seller.getProperty(), 20, NEGATIVE_BALANCE));
  BOOST_CHECK(mastercore::update_tally_map(buyer.getAddr(),buyer.getProperty(), 20, POSITIVE_BALANCE));

  BOOST_CHECK(mastercore::update_tally_map(seller.getAddr(),seller.getProperty(), 100000, CONTRACTDEX_RESERVE));
  BOOST_CHECK(mastercore::update_tally_map(buyer.getAddr(),buyer.getProperty(), 100000, CONTRACTDEX_RESERVE));

  BOOST_CHECK_EQUAL(20, getMPbalance(seller.getAddr(), seller.getProperty(), NEGATIVE_BALANCE));
  BOOST_CHECK_EQUAL(20, getMPbalance(buyer.getAddr(), buyer.getProperty(), POSITIVE_BALANCE));

  BOOST_CHECK_EQUAL(100000, getMPbalance(seller.getAddr(), seller.getProperty(), CONTRACTDEX_RESERVE));
  BOOST_CHECK_EQUAL(100000, getMPbalance(buyer.getAddr(), buyer.getProperty(), CONTRACTDEX_RESERVE));

  BOOST_CHECK_EQUAL(NOTHING, x_Trade(b));
  BOOST_CHECK_EQUAL(NOTHING, x_Trade(s));

}

BOOST_AUTO_TEST_SUITE_END()
