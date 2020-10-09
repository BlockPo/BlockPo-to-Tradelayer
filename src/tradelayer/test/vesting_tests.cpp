#include <tradelayer/consensushash.h>
#include <tradelayer/errors.h>
#include <tradelayer/externfns.h>
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

BOOST_FIXTURE_TEST_SUITE(vesting_tests, BasicTestingSetup)

/** Proof of concept function **/
int VestingTokens(double& lastVesting, int64_t globalVolumeALL_LTC, int64_t& nAmount, std::vector<std::string>& vestingAddresses)
{
    bool deactivation = false;

    if (vestingAddresses.empty()){
        return -1;
    }

    if (globalVolumeALL_LTC <= 10000 * COIN){
        return -2;
    }

    if(100000000 * COIN <= globalVolumeALL_LTC){
        deactivation = true;
    }

    // accumulated vesting
    const double accumVesting = getAccumVesting(globalVolumeALL_LTC);

    // vesting fraction on this block
    const double realVesting = (accumVesting > lastVesting) ?  accumVesting - lastVesting : 0;

    if (realVesting == 0){
        return -4;
    }

    for (auto &addr : vestingAddresses)
    {

        const int64_t vestingBalance = getMPbalance(addr, TL_PROPERTY_VESTING, BALANCE);
        const int64_t unvestedALLBal = getMPbalance(addr, ALL, UNVESTED);

        if (vestingBalance != 0 && unvestedALLBal != 0 && globalVolumeALL_LTC != 0)
        {

            const int64_t iRealVesting = mastercore::DoubleToInt64(realVesting);
            const arith_uint256 uVestingBalance = mastercore::ConvertTo256(vestingBalance);
            const arith_uint256 uCOIN  = mastercore::ConvertTo256(COIN);
            const arith_uint256 iAmount = mastercore::ConvertTo256(iRealVesting) * DivideAndRoundUp(uVestingBalance, uCOIN);
            nAmount = mastercore::ConvertTo64(iAmount);

            if(deactivation){
                BOOST_TEST_MESSAGE("Passing ALL left unvested to balance");
                nAmount = unvestedALLBal;
            }

            // BOOST_TEST_MESSAGE("vestingBalance" << vestingBalance << "\n");
            // BOOST_TEST_MESSAGE("unvestedALLBal" << unvestedALLBal);
            // BOOST_TEST_MESSAGE("nAmount" << nAmount);
            // BOOST_TEST_MESSAGE("iRealVesting" << iRealVesting);
            assert(update_tally_map(addr, ALL, -nAmount, UNVESTED));
            assert(update_tally_map(addr, ALL, nAmount, BALANCE));
        }
    }

    lastVesting = accumVesting;

    return 0;
}

BOOST_AUTO_TEST_CASE(empty_address)
{
    int64_t nAmount = 0;
    double lastVesting = 0;
    std::vector<std::string> vestingAddresses;
    BOOST_CHECK_EQUAL(-1, VestingTokens(lastVesting, 0, nAmount, vestingAddresses));

}

BOOST_AUTO_TEST_CASE(out_of_range_volume)
{
    int64_t nAmount = 0;
    double lastVesting = 0;
    std::vector<std::string> vestingAddresses;
    // adding first vesting address
    vestingAddresses.push_back("QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPb");

    // putting 1000 vesting tokens
    BOOST_CHECK(mastercore::update_tally_map("QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPb", TL_PROPERTY_VESTING, 1000 * COIN, BALANCE));
    BOOST_CHECK(mastercore::update_tally_map("QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPb", ALL, 1000 * COIN, UNVESTED));
    //checking ALL balance is zero
    BOOST_CHECK_EQUAL(0, getMPbalance("QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPb", ALL, BALANCE));

    BOOST_CHECK_EQUAL(-2, VestingTokens(lastVesting, 0, nAmount, vestingAddresses));
    BOOST_CHECK_EQUAL(-2, VestingTokens(lastVesting, COIN, nAmount, vestingAddresses));
    BOOST_CHECK_EQUAL(-2, VestingTokens(lastVesting, 10 * COIN, nAmount, vestingAddresses));
    BOOST_CHECK_EQUAL(-2, VestingTokens(lastVesting, 100 * COIN, nAmount, vestingAddresses));
    BOOST_CHECK_EQUAL(-2, VestingTokens(lastVesting, 1000 * COIN, nAmount, vestingAddresses));
    BOOST_CHECK_EQUAL(-2, VestingTokens(lastVesting, 10000 * COIN, nAmount, vestingAddresses));

    // last vesting batch
    BOOST_CHECK_EQUAL(0, VestingTokens(lastVesting, 100000010 * COIN, nAmount, vestingAddresses));

    // amount bigger than max volume
    BOOST_CHECK_EQUAL(-4, VestingTokens(lastVesting, 100000100 * COIN, nAmount, vestingAddresses));
    BOOST_CHECK_EQUAL(-4, VestingTokens(lastVesting, 100001000 * COIN, nAmount, vestingAddresses));
    BOOST_CHECK_EQUAL(-4, VestingTokens(lastVesting, 100010000 * COIN, nAmount, vestingAddresses));
    BOOST_CHECK_EQUAL(-4, VestingTokens(lastVesting, 100100000 * COIN, nAmount, vestingAddresses));

    // must have entire volume
    BOOST_CHECK_EQUAL(100000000000, getMPbalance("QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPb", ALL, BALANCE));
}



BOOST_AUTO_TEST_CASE(vesting_process)
{
  int64_t nAmount = 0;
  double lastVesting = 0;
  std::vector<std::string> vestingAddresses;
  // adding first vesting address
  vestingAddresses.push_back("QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPx");
  // putting 1000 vesting tokens
  BOOST_CHECK(mastercore::update_tally_map("QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPx", TL_PROPERTY_VESTING, 1000 * COIN, BALANCE));
  BOOST_CHECK(mastercore::update_tally_map("QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPx", ALL, 1000 * COIN, UNVESTED));
  //checking ALL balance is zero
  BOOST_CHECK_EQUAL(0, getMPbalance("QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPx", ALL, BALANCE));
  // checking vestingAddresses size
  BOOST_CHECK_EQUAL(1, vestingAddresses.size());

  // checking 7.52% vesting output
  BOOST_CHECK_EQUAL(0, VestingTokens(lastVesting, 20000 * COIN, nAmount, vestingAddresses));
  //checking ALL balance
  BOOST_CHECK_EQUAL(7525749000, getMPbalance("QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPx", ALL, BALANCE));
  //checking unvested ALL balance
  BOOST_CHECK_EQUAL(92474251000, getMPbalance("QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPx", ALL, UNVESTED));
  // all unvested + balance = 1000
  BOOST_CHECK_EQUAL(1000 * COIN, getMPbalance("QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPx", ALL, UNVESTED) + getMPbalance("QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPx", ALL, BALANCE));
  // lastVesting: 7.52%
  BOOST_CHECK_EQUAL(0.075257498915995313, lastVesting);
  // amount vested in this block (7.52%)
  BOOST_CHECK_EQUAL(7525749000, nAmount);

  // checking 15.05% vesting output
  BOOST_CHECK_EQUAL(0, VestingTokens(lastVesting, 40000 * COIN, nAmount, vestingAddresses));
  //checking ALL balance
  BOOST_CHECK_EQUAL(15051498000, getMPbalance("QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPx", ALL, BALANCE));
  // //checking unvested ALL balance
  BOOST_CHECK_EQUAL(84948502000, getMPbalance("QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPx", ALL, UNVESTED));
  // all unvested + balance = 1000
  BOOST_CHECK_EQUAL(1000 * COIN, getMPbalance("QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPx", ALL, UNVESTED) + getMPbalance("QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPx", ALL, BALANCE));
  // lastVesting: 15.05%
  BOOST_CHECK_EQUAL(0.15051499783199063, lastVesting);
  // amount vested in this block (7.53%)
  BOOST_CHECK_EQUAL(7525749000, nAmount);

  // checking 25% vesting output
  BOOST_CHECK_EQUAL(0, VestingTokens(lastVesting, 100000 * COIN, nAmount, vestingAddresses));
  //checking ALL balance
  BOOST_CHECK_EQUAL(24999998000, getMPbalance("QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPx", ALL, BALANCE));
  // //checking unvested ALL balance
  BOOST_CHECK_EQUAL(75000002000, getMPbalance("QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPx", ALL, UNVESTED));
  // all unvested + balance = 1000
  BOOST_CHECK_EQUAL(1000 * COIN, getMPbalance("QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPx", ALL, UNVESTED) + getMPbalance("QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPx", ALL, BALANCE));
  // lastVesting: 25%
  BOOST_CHECK_EQUAL(0.25000000000000000, lastVesting);
  // amount vested in this block (9.95%)
  BOOST_CHECK_EQUAL(9948500000, nAmount);

  // checking 50% vesting output
  BOOST_CHECK_EQUAL(0, VestingTokens(lastVesting, 1000000 * COIN, nAmount, vestingAddresses));
  //checking ALL balance
  BOOST_CHECK_EQUAL(49999998000, getMPbalance("QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPx", ALL, BALANCE));
  // //checking unvested ALL balance
  BOOST_CHECK_EQUAL(50000002000, getMPbalance("QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPx", ALL, UNVESTED));
  // all unvested + balance = 1000
  BOOST_CHECK_EQUAL(1000 * COIN, getMPbalance("QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPx", ALL, UNVESTED) + getMPbalance("QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPx", ALL, BALANCE));
  // lastVesting: 50%
  BOOST_CHECK_EQUAL(0.50000000000000000, lastVesting);
  // amount vested in this block (25%)
  BOOST_CHECK_EQUAL(25000000000, nAmount);


  // checking 75% vesting output
  BOOST_CHECK_EQUAL(0, VestingTokens(lastVesting, 10000000 * COIN, nAmount, vestingAddresses));
  //checking ALL balance
  BOOST_CHECK_EQUAL(74999998000, getMPbalance("QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPx", ALL, BALANCE));
  // //checking unvested ALL balance
  BOOST_CHECK_EQUAL(25000002000, getMPbalance("QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPx", ALL, UNVESTED));
  // all unvested + balance = 1000
  BOOST_CHECK_EQUAL(1000 * COIN, getMPbalance("QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPx", ALL, UNVESTED) + getMPbalance("QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPx", ALL, BALANCE));
  // lastVesting: 75%
  BOOST_CHECK_EQUAL(0.75000000000000000, lastVesting);
  // amount vested in this block (25%)
  BOOST_CHECK_EQUAL(25000000000, nAmount);


  // checking 100% vesting output
  BOOST_CHECK_EQUAL(0, VestingTokens(lastVesting, 1000000000 * COIN, nAmount, vestingAddresses));
  // checking ALL balance
  BOOST_CHECK_EQUAL(100000000000, getMPbalance("QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPx", ALL, BALANCE));
  // //checking unvested ALL balance
  BOOST_CHECK_EQUAL(0, getMPbalance("QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPx", ALL, UNVESTED));
  // all unvested + balance = 1000
  BOOST_CHECK_EQUAL(1000 * COIN, getMPbalance("QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPx", ALL, UNVESTED) + getMPbalance("QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPx", ALL, BALANCE));
  // lastVesting: 75%
  BOOST_CHECK_EQUAL(1, lastVesting);
  // amount vested in this block (25%)
  BOOST_CHECK_EQUAL(25000002000, nAmount);


}

BOOST_AUTO_TEST_SUITE_END()
