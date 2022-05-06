#include <tradelayer/mdex.h>
#include <tradelayer/dex.h>
#include <tradelayer/tradelayer.h>
#include <tradelayer/uint256_extensions.h>
#include <test/test_bitcoin.h>
#include <boost/test/unit_test.hpp>
#include <stdint.h>

using namespace mastercore;

BOOST_FIXTURE_TEST_SUITE(tradelayer_mdex_functions_tests, BasicTestingSetup)

void literVolume(int64_t& amount, uint32_t propertyId, const int& fblock, const int& sblock, const std::map<int, std::map<uint32_t,int64_t>>& aMap)
{
    // BOOST_TEST_MESSAGE("iter, fblock:" << fblock);
    // BOOST_TEST_MESSAGE("iter, sblock:" << sblock);
    for(const auto &m : aMap)
    {
        const int& blk = m.first;
        // BOOST_TEST_MESSAGE("iter, block (after):" << blk);
        if(blk < fblock && fblock != 0){
            continue;
        } else if(sblock < blk){
            break;
        }

        // BOOST_TEST_MESSAGE("iter, block:" << blk);

        const auto &blockMap = m.second;
        auto itt = blockMap.find(propertyId);
        // BOOST_TEST_MESSAGE("iter, propertyId:" << propertyId);
        if (itt != blockMap.end()){

            const int64_t& newAmount = itt->second;

            // overflows?
            assert(!isOverflow(amount, newAmount));
            amount += newAmount;
            // BOOST_TEST_MESSAGE("iter, amount (after):" << amount);

        }

    }

}

int64_t lgetVWap(uint32_t propertyId, int aBlock, const std::map<uint32_t,std::map<int,std::vector<std::pair<int64_t,int64_t>>>>& aMap)
{
    int64_t volume = 0;
    arith_uint256 nvwap = 0;
    const int rollback = aBlock - 12;

    // BOOST_TEST_MESSAGE("rollback:" << rollback);

    auto it = aMap.find(propertyId);
    if (it != aMap.end())
    {
        auto &vmap = it->second;
        auto itt = (rollback > 0) ? find_if(vmap.begin(), vmap.end(), [&rollback] (const std::pair<int,std::vector<std::pair<int64_t,int64_t>>>& int_arith_pair) { return (int_arith_pair.first >= rollback);}) : vmap.begin();
        if (itt != vmap.end())
        {
            for ( ; itt != vmap.end(); ++itt)
            {
                auto v = itt->second;
                for_each(v.begin(),v.end(), [&nvwap](const std::pair<int64_t,int64_t>& num){ nvwap += ConvertTo256(num.first * (num.second / COIN));});
            }
        }

    }
    // calculating the volume
    // BOOST_TEST_MESSAGE("rollback:" << rollback);
    // BOOST_TEST_MESSAGE("aBlock:" << aBlock);
    iterVolume(volume, propertyId, rollback, aBlock, MapLTCVolume);

    if (volume == 0) BOOST_TEST_MESSAGE("volume here is 0");

    // BOOST_TEST_MESSAGE("nvwap:" << ConvertTo64(nvwap));
    // BOOST_TEST_MESSAGE("volume:" << volume);
    return ((volume > 0) ? (COIN *(ConvertTo64(nvwap) / volume)) : 0);

}

int64_t lincreaseLTCVolume(uint32_t propertyId, uint32_t propertyDesired, int aBlock)
{
    int64_t total = 0, propertyAmount = 0, propertyDesiredAmount = 0;
    const int rollback = aBlock - 1000;
    iterVolume(propertyAmount, propertyId, rollback, aBlock, MapLTCVolume);
    iterVolume(propertyDesiredAmount, propertyDesired, rollback, aBlock, MapLTCVolume);

    // BOOST_TEST_MESSAGE("propertyAmount:" << propertyAmount);
    // BOOST_TEST_MESSAGE("propertyDesiredAmount:" << propertyDesiredAmount);
    // BOOST_TEST_MESSAGE("aBlock:" << aBlock);
    // BOOST_TEST_MESSAGE("rollback:" << rollback);

    if (1000 * COIN <=  propertyAmount && 1000 * COIN <=  propertyDesiredAmount)
    {

        // look up the 12-block VWAP of the denominator vs. LTC
        const int64_t vwap = getVWap(propertyDesired, aBlock, tokenvwap);
        const arith_uint256 aTotal = (ConvertTo256(propertyDesiredAmount) * ConvertTo256(vwap)) / ConvertTo256(COIN);
        total = ConvertTo64(aTotal);
        // BOOST_TEST_MESSAGE("vwap:" << vwap);
        // BOOST_TEST_MESSAGE("propertyDesiredAmount:" << propertyDesiredAmount);
        // BOOST_TEST_MESSAGE("total:" << total);

        // increment cumulative LTC volume by tokens traded * the 12-block VWAP
        MapLTCVolume[aBlock][propertyDesired] += total;

    }

    return total;

}

BOOST_AUTO_TEST_CASE(getvwap_function)
{
    const uint32_t propertyId = 3;
    // actual block
    const int aBlock = 10250;

    // adding  amount *  price
    tokenvwap[propertyId][aBlock - 1].push_back(std::make_pair(2000 * COIN, 1500 * COIN));
    tokenvwap[propertyId][aBlock - 2].push_back(std::make_pair(2000 * COIN, 1600 * COIN));
    tokenvwap[propertyId][aBlock - 3].push_back(std::make_pair(2000 * COIN, 1700 * COIN));
    tokenvwap[propertyId][aBlock - 3].push_back(std::make_pair(2000 * COIN, 1800 * COIN));
    tokenvwap[propertyId][aBlock - 4].push_back(std::make_pair(2000 * COIN, 1900 * COIN));

    // adding some volume
    MapLTCVolume[aBlock - 1][propertyId] += 2000 * COIN;
    MapLTCVolume[aBlock - 2][propertyId] += 1000 * COIN;
    MapLTCVolume[aBlock - 3][propertyId] += 3000 * COIN;
    MapLTCVolume[aBlock - 4][propertyId] += 4000 * COIN;

    // checking vwap:   17000000 / 10000 = 1700
    BOOST_CHECK_EQUAL(1700 * COIN, lgetVWap(propertyId, aBlock, tokenvwap));

    // cleaning maps
    tokenvwap.clear();
    MapLTCVolume.clear();
}

BOOST_AUTO_TEST_CASE(increase_ltc_volume_function)
{
    const uint32_t propertyId = 1;
    const uint32_t propertyDesired = 2;
    // actual block
    const int aBlock = 10250;

    // adding  amount *  price
    tokenvwap[propertyId][aBlock - 10].push_back(std::make_pair(1000 * COIN, 1500 * COIN));
    tokenvwap[propertyId][aBlock - 10].push_back(std::make_pair(3000 * COIN, 1500 * COIN));

    tokenvwap[propertyDesired][aBlock - 10].push_back(std::make_pair(1000 * COIN, 1500 * COIN));
    tokenvwap[propertyDesired][aBlock - 10].push_back(std::make_pair(3000 * COIN, 1500 * COIN));


    // adding some volume
    MapLTCVolume[aBlock - 10][propertyId] = 2000 * COIN;
    MapLTCVolume[aBlock - 10][propertyDesired] = 2000 * COIN;


    // 12-vwap * amountdesired = 3000 * 2000 = 6000000
    BOOST_CHECK_EQUAL(6000000 * COIN, lincreaseLTCVolume(propertyId, propertyDesired, aBlock));


}



BOOST_AUTO_TEST_SUITE_END()
