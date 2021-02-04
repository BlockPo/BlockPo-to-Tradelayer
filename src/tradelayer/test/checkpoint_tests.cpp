#include <test/test_bitcoin.h>
#include <tradelayer/activation.h>
#include <tradelayer/consensushash.h>
#include <tradelayer/dex.h>
#include <tradelayer/mdex.h>
#include <tradelayer/rules.h>
#include <tradelayer/sp.h>
#include <tradelayer/tally.h>
#include <tradelayer/tradelayer.h>

#include <arith_uint256.h>
#include <sync.h>
#include <uint256.h>

#include <stdint.h>
#include <string>

#include <boost/test/unit_test.hpp>

namespace mastercore
{
extern std::string GenerateConsensusString(const CMPTally& tallyObj, const std::string& address, const uint32_t propertyId); // done
extern std::string GenerateConsensusString(const CMPOffer& offerObj, const std::string& address); // half
extern std::string GenerateConsensusString(const CMPAccept& acceptObj, const std::string& address);
extern std::string GenerateConsensusString(const uint32_t propertyId, const std::string& address);
extern std::string GenerateConsensusString(const Channel& chn);
extern std::string kycGenerateConsensusString(const std::vector<std::string>& vstr);
extern std::string attGenerateConsensusString(const std::vector<std::string>& vstr);
extern std::string feeGenerateConsensusString(const uint32_t& propertyId, const int64_t& cache);
extern std::string GenerateConsensusString(const FeatureActivation& feat);

}

extern void clear_all_state();

using namespace mastercore;

BOOST_FIXTURE_TEST_SUITE(tradelayer_checkpoint_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(consensus_string_tally)
{
    CMPTally tally;
    BOOST_CHECK_EQUAL("", GenerateConsensusString(tally, "3CwZ7FiQ4MqBenRdCkjjc41M5bnoKQGC2b", 1));
    BOOST_CHECK_EQUAL("", GenerateConsensusString(tally, "3CwZ7FiQ4MqBenRdCkjjc41M5bnoKQGC2b", 3));

    BOOST_CHECK(tally.updateMoney(3, 7, BALANCE));
    BOOST_CHECK_EQUAL("3CwZ7FiQ4MqBenRdCkjjc41M5bnoKQGC2b|3|7|0|0|0|0|0|0|0|0|0|0",
            GenerateConsensusString(tally, "3CwZ7FiQ4MqBenRdCkjjc41M5bnoKQGC2b", 3));

    BOOST_CHECK(tally.updateMoney(3, 7, BALANCE));
    BOOST_CHECK(tally.updateMoney(3, 100, SELLOFFER_RESERVE));
    BOOST_CHECK(tally.updateMoney(3, int64_t(9223372036854775807LL), ACCEPT_RESERVE));
    BOOST_CHECK(tally.updateMoney(3, (-int64_t(9223372036854775807LL)-1), PENDING)); // ignored
    BOOST_CHECK(tally.updateMoney(3, int64_t(4294967296L), METADEX_RESERVE));
    BOOST_CHECK_EQUAL("3CwZ7FiQ4MqBenRdCkjjc41M5bnoKQGC2b|3|14|100|9223372036854775807|4294967296|0|0|0|0|0|0|0",
            GenerateConsensusString(tally, "3CwZ7FiQ4MqBenRdCkjjc41M5bnoKQGC2b", 3));
}

BOOST_AUTO_TEST_CASE(consensus_string_offer)
{
    CMPOffer offerA;
    BOOST_CHECK_EQUAL("0000000000000000000000000000000000000000000000000000000000000000|3CwZ7FiQ4MqBenRdCkjjc41M5bnoKQGC2b|0|0|0|0|0|0|0|0",
            GenerateConsensusString(offerA, "3CwZ7FiQ4MqBenRdCkjjc41M5bnoKQGC2b"));

    CMPOffer offerB(340000, 3000, 2, 100000, 1000, 10, uint256S("3c9a055899147b03b2c5240a020c1f94d243a834ecc06ab8cfa504ee29d07b7d"), 0, 1);
    BOOST_CHECK_EQUAL("3c9a055899147b03b2c5240a020c1f94d243a834ecc06ab8cfa504ee29d07b7d|1HG3s4Ext3sTqBTHrgftyUzG3cvx5ZbPCj|340000|2|3000|100000|1000|10|0|1",
            GenerateConsensusString(offerB, "1HG3s4Ext3sTqBTHrgftyUzG3cvx5ZbPCj"));
}

BOOST_AUTO_TEST_CASE(consensus_string_accept)
{
    CMPAccept accept(1234, 1000, 350000, 10, 2, 2000, 4000, uint256S("2c9a055899147b03b2c5240a020c1f94d243a834ecc06ab8cfa504ee29d07b7b"));
    BOOST_CHECK_EQUAL("2c9a055899147b03b2c5240a020c1f94d243a834ecc06ab8cfa504ee29d07b7b|3CwZ7FiQ4MqBenRdCkjjc41M5bnoKQGC2b|1234|1000|350000",
            GenerateConsensusString(accept, "3CwZ7FiQ4MqBenRdCkjjc41M5bnoKQGC2b"));
}


BOOST_AUTO_TEST_CASE(consensus_string_mdex)
{
    CMPMetaDEx tradeA;
    BOOST_CHECK_EQUAL("0000000000000000000000000000000000000000000000000000000000000000||0|0|0|0|0",
            tradeA.GenerateConsensusString());

    CMPMetaDEx tradeB("1PxejjeWZc9ZHph7A3SYDo2sk2Up4AcysH", 395000, 31, 1000000, 1, 2000000,
            uint256S("2c9a055899147b03b2c5240a020c1f94d243a834ecc06ab8cfa504ee29d07b7d"), 1, 1, 900000);
    BOOST_CHECK_EQUAL("2c9a055899147b03b2c5240a020c1f94d243a834ecc06ab8cfa504ee29d07b7d|1PxejjeWZc9ZHph7A3SYDo2sk2Up4AcysH|31|1000000|1|2000000|900000",
            tradeB.GenerateConsensusString());
}

BOOST_AUTO_TEST_CASE(consensus_string_property_issuer)
{
    BOOST_CHECK_EQUAL("5|3CwZ7FiQ4MqBenRdCkjjc41M5bnoKQGC2b",
            GenerateConsensusString(5, "3CwZ7FiQ4MqBenRdCkjjc41M5bnoKQGC2b"));
}

BOOST_AUTO_TEST_CASE(consensus_string_cdex)
{
    CMPContractDex tradeA;
    BOOST_CHECK_EQUAL("0000000000000000000000000000000000000000000000000000000000000000||0|0|0|0|0|0",
            tradeA.GenerateConsensusString());

   CMPContractDex tradeB("1PxejjeWZc9ZHph7A3SYDo2sk2Up4AcysH", 395000, 8050, 9999999, 0, 0, uint256S("2c9a055899147b03b2c5240a020c1f94d243a834ecc06ab8cfa504ee29d07b7d"), 1, CMPTransaction::ADD, 16539884, 2, 0);

   BOOST_CHECK_EQUAL("2c9a055899147b03b2c5240a020c1f94d243a834ecc06ab8cfa504ee29d07b7d" , tradeB.getHash().GetHex());
   BOOST_CHECK_EQUAL("1PxejjeWZc9ZHph7A3SYDo2sk2Up4AcysH", tradeB.getAddr());
   BOOST_CHECK_EQUAL(8050, tradeB.getProperty());
   BOOST_CHECK_EQUAL(9999999, tradeB.getAmountForSale());
   BOOST_CHECK_EQUAL(16539884, tradeB.getEffectivePrice());
   BOOST_CHECK_EQUAL(9999999, tradeB.getAmountRemaining());
   BOOST_CHECK_EQUAL(2, tradeB.getTradingAction());
   BOOST_CHECK_EQUAL(0, tradeB.getAmountReserved());

   BOOST_CHECK_EQUAL("2c9a055899147b03b2c5240a020c1f94d243a834ecc06ab8cfa504ee29d07b7d|1PxejjeWZc9ZHph7A3SYDo2sk2Up4AcysH|8050|9999999|9999999|16539884|2|0",
            tradeB.GenerateConsensusString());
}


BOOST_AUTO_TEST_CASE(consensus_channel_address)
{
    const std::string multisig = "3CwZ7FiQ4MqBenRdCkjjc41M5bnoKQGC2b";
    const std::string first = "1PxejjeWZc9ZHph7A3SYDo2sk2Up4AcysH";
    const std::string second = "16Mk17CGqLaz5qBxv93YxcXwFbzV7N7y6G";
    const int last_exchange_block = 200;

    Channel chn(multisig, first, second, last_exchange_block);
    BOOST_CHECK_EQUAL("3CwZ7FiQ4MqBenRdCkjjc41M5bnoKQGC2b|1PxejjeWZc9ZHph7A3SYDo2sk2Up4AcysH|16Mk17CGqLaz5qBxv93YxcXwFbzV7N7y6G|874|200",
            GenerateConsensusString(chn));
}

BOOST_AUTO_TEST_CASE(consensus_kyc_register)
{
    std::vector<std::string> vstr;

    vstr.push_back("blockpo");
    vstr.push_back("3CwZ7FiQ4MqBenRdCkjjc41M5bnoKQGC2b");
    vstr.push_back("www.blockpo.com");
    vstr.push_back("526");
    vstr.push_back("");
    vstr.push_back("1");

    BOOST_CHECK_EQUAL("blockpo|3CwZ7FiQ4MqBenRdCkjjc41M5bnoKQGC2b|www.blockpo.com|526|1",
            kycGenerateConsensusString(vstr));
}

BOOST_AUTO_TEST_CASE(consensus_attestation_register)
{
    std::vector<std::string> vstr;

    vstr.push_back("3CwZ7FiQ4MqBenRdCkjjc41M5bnoKQGC2b");
    vstr.push_back("1PxejjeWZc9ZHph7A3SYDo2sk2Up4AcysH");
    vstr.push_back("1");
    vstr.push_back("");
    vstr.push_back("527");

    BOOST_CHECK_EQUAL("3CwZ7FiQ4MqBenRdCkjjc41M5bnoKQGC2b|1PxejjeWZc9ZHph7A3SYDo2sk2Up4AcysH|1|527",
           attGenerateConsensusString(vstr));
}

BOOST_AUTO_TEST_CASE(consensus_cachefee)
{
    uint32_t propertyId = 9352;
    int64_t cache = 8742639395;

    BOOST_CHECK_EQUAL("9352|8742639395",
           feeGenerateConsensusString(propertyId, cache));
}

BOOST_AUTO_TEST_CASE(consensus_activations)
{
    FeatureActivation feat;
    feat.featureId = 8;
    feat.activationBlock = 145263;
    feat.minClientVersion = 1;
    feat.featureName = "x activation";
    feat.status = true;

    BOOST_CHECK_EQUAL("8|145263|1|x activation|completed",
           GenerateConsensusString(feat));
}

BOOST_AUTO_TEST_CASE(get_checkpoints)
{
    // There aren't yet consensus checkpoints for mainnet:
    BOOST_CHECK(ConsensusParams("main").GetCheckpoints().empty());
    // ... and no checkpoints for regtest mode are defined:
    BOOST_CHECK(ConsensusParams("regtest").GetCheckpoints().empty());
}


BOOST_AUTO_TEST_SUITE_END()
