#include <base58.h>
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

bool isWinnerAddress(const std::string& consensusHash, const std::string& address)
{
    const std::string lastFourCharConsensus = consensusHash.substr(consensusHash.size() - 4);

    BOOST_TEST_MESSAGE("LastFourCharConsensus:" << lastFourCharConsensus);
    // BOOST_TEST_MESSAGE("lastTwoCharAddr:" << lastTwoCharAddr);

    std::vector<unsigned char> vch;

    DecodeBase58(address, vch);

    BOOST_TEST_MESSAGE("vch:" << HexStr(vch));

    const std::string vchStr = HexStr(vch);

    const std::string LastTwoCharAddr = vchStr.substr(vchStr.size() - 4);

    BOOST_TEST_MESSAGE("LastTwoCharAddr:" << LastTwoCharAddr);


    return (LastTwoCharAddr == lastFourCharConsensus) ? true : false;

}

BOOST_AUTO_TEST_CASE(is_valid_address)
{
  const string address1 = "n1Rovq61fKJosCVAnS31QtvGTR5qXcgSpy";
  const string address2 = "mi65kbzjRyUR2dTKewLpzrmCUQQ2jx1WRp";
  const string address3 = "mitLEfdzC9RMhTccFvhmYiyRp6VzpakM75";
  const string address4 = "mtgPgfWGaAKKzWzG26UrBVqtDFxJbWt6T7";
  const string address5 = "n4HxBFptuw3dAFbHMYFb5Kamu56tzFBpeP";
  const string address6 = "min15ZBzsU8PXLZKNejsb7XfdVDLGkC3uS";

  const string consensusHash1 = "cfc1a5fe9c43f36a468d9267a499171fb14710be4d4284a700b99dec276a";
  const string consensusHash2 = "1b90b0385bd68c07446287e49e649d984b7657c46928a94baa81968fcfa52623";
  const string consensusHash3 = "685f598ea96ea8e58c6d530d567589f0d4439e20fec3beea1b3ab297209cf998";

  BOOST_CHECK(isWinnerAddress(consensusHash1, address1));
  BOOST_CHECK(isWinnerAddress(consensusHash2, address2));
  BOOST_CHECK(isWinnerAddress(consensusHash3, address3));
  BOOST_CHECK(!isWinnerAddress(consensusHash1, address3));
  BOOST_CHECK(!isWinnerAddress(consensusHash1, address4));
  BOOST_CHECK(!isWinnerAddress(consensusHash1, address5));
  BOOST_CHECK(!isWinnerAddress(consensusHash1, address6));

}

// Node Reward Object
BOOST_AUTO_TEST_CASE(node_reward_object)
{
    nodeReward nR;
    const string consensusHash = "cfc1a5fe9c43f36a468d9267a499171fb14710be4d4284a700b99dec276a";
    const string address1 = "n1Rovq61fKJosCVAnS31QtvGTR5qXcgSpy";
    const string address2 = "QMt8L9dYuqTpFPRV7ESwvJGCw9XoiBAvu7";

    const string address3 = "mitLEfdzC9RMhTccFvhmYiyRp6VzpakM75";

    BOOST_CHECK_EQUAL(nR.getNextReward(), 5000000);
    BOOST_CHECK(nR.isWinnerAddress(consensusHash, address1, true));
    BOOST_CHECK(nR.isWinnerAddress(consensusHash, address2, true));
    BOOST_CHECK(!nR.isWinnerAddress(consensusHash, address3, true));

    nR.setLastBlock(25689);
    BOOST_CHECK_EQUAL(nR.getLastBlock(), 25689);

    //adding addresses
    nR.updateAddressStatus(address1, true);
    nR.updateAddressStatus(address2, true);

    //sending node reward
    nR.sendNodeReward(consensusHash, 25690, true);

    //checking balances:
    BOOST_CHECK_EQUAL(getMPbalance(address1, ALL, BALANCE), 2500000);
    BOOST_CHECK_EQUAL(getMPbalance(address2, ALL, BALANCE), 2500000);


}

BOOST_AUTO_TEST_SUITE_END()
