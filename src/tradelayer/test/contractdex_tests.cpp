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

BOOST_FIXTURE_TEST_SUITE(contractdex_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(edge_orderbook)
{
    CMPContractDex seller1("1dexX7zmPen1yBz2H9ZF62AK5TGGqGTZH", // address
  		  172, // block
  			1,   // property for sale
  			5,   // amount of contracts for sale
  			0,   // desired property
  			0,
  			uint256S("6"), // txid
  			1,   // position in block
  			1,   // subaction
  			0,   // amount remaining
  			500000000,   // effective_price
  			2,  // trading_action
        0);   // amount to reserve

    CMPContractDex seller2("1NNQKWM8mC35pBNPxV1noWFZEw7A5X6zXz", // address
	      172,  // block
	      1,    // property for sale
	      5,    // amount of contracts for trade
	      0,    // desired property
	      0,
	      uint256S("7"), // txid
	      2,    // position in block
	      1,    // subaction
	      0,    // amount remaining
	      600000000,    // effective_price
	      2,    // trading_action
        0);   // amount to reserve

    CMPContractDex seller3("1Nx8KWM8mC35pBNPxV1noWFZEw7A5X6zXz", // address
	      172,  // block
	      1,    // property for sale
	      5,    // amount of contracts for trade
	      0,    // desired property
	      0,
	      uint256S("8"), // txid
	      3,    // position in block
	      1,    // subaction
	      0,    // amount remaining
	      700000000,    // effective_price
	      2,    // trading_action
        0);   // amount to reserve

    CMPContractDex seller4("1Nx8KWM8mC35pBNPxV1noWFZEw7A5X6zXz", // address
	      172,  // block
	      1,    // property for sale
	      5,    // amount of contracts for trade
	      0,    // desired property
	      0,
	      uint256S("9"), // txid
	      3,    // position in block
	      1,    // subaction
	      0,    // amount remaining
	      800000000,    // effective_price
	      2,    // trading_action
        0);   // amount to reserve

    CMPContractDex buyer1("2dexX7zmPen1yBz2H9ZF62AK5TGGqGTZH", // address
   	    172, // block
   			1,   // property for sale
   			5,   // amount of contracts for sale
   			0,   // desired property
   			0,
   			uint256S("10"), // txid
   			4,   // position in block
   			1,   // subaction
   			0,   // amount remaining
   			400000000,   // effective_price
   			1,  // trading_action
        0);   // amount to reserve

    CMPContractDex buyer2("1NNQKWM8mP35pBNPxV1noWFZEw7A5X6zXz", // address
        172,  // block
        1,    // property for sale
        5,    // amount of contracts for trade
        0,    // desired property
        0,
        uint256S("11"), // txid
        5,    // position in block
        1,    // subaction
        0,    // amount remaining
        300000000,    // effective_price
        1,    // trading_action
        0);   // amount to reserve

    CMPContractDex buyer3("19x8KWM8mC35pBNPxV1noWFZEw7A5X6YXz", // address
        172,  // block
        1,    // property for sale
        5,    // amount of contracts for trade
        0,    // desired property
        0,
        uint256S("20"), // txid
        6,    // position in block
        1,    // subaction
        0,    // amount remaining
        200000000,    // effective_price
        1,   // trading_action
        0);   // amount to reserve

    CMPContractDex buyer4("19x8KWM8mC35pBNPxV1noWFZEw7A5X6zXz", // address
        172,  // block
        1,    // property for sale
        5,    // amount of contracts for trade
        0,    // desired property
        0,
        uint256S("12"), // txid
        7,    // position in block
        1,    // subaction
        0,    // amount remaining
        100000000,    // effective_price
        1,   // trading_action
        0);   // amount to reserve

    //inserting in orderbook
    BOOST_CHECK(ContractDex_INSERT(seller1)); // prices: 500000000
    BOOST_CHECK(ContractDex_INSERT(seller2)); //         600000000
    BOOST_CHECK(ContractDex_INSERT(seller3)); //         700000000
    BOOST_CHECK(ContractDex_INSERT(seller4)); //         800000000

    BOOST_CHECK(ContractDex_INSERT(buyer1));  // prices: 400000000
    BOOST_CHECK(ContractDex_INSERT(buyer2));  //         300000000
    BOOST_CHECK(ContractDex_INSERT(buyer3));  //         200000000
    BOOST_CHECK(ContractDex_INSERT(buyer4));  //         100000000

    uint32_t contractId = 1;

    //checking the Ask
    BOOST_CHECK_EQUAL(edgeOrderbook(contractId, buy), 500000000);

    //checking the Bid
    BOOST_CHECK_EQUAL(edgeOrderbook(contractId, sell), 400000000);


}

BOOST_AUTO_TEST_SUITE_END()
