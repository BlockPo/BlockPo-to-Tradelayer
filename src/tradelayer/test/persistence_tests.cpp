#include "tradelayer/tally.h"
#include "tradelayer/tradelayer.h"
#include "test/test_bitcoin.h"

#include <stdint.h>

#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string.hpp>

BOOST_FIXTURE_TEST_SUITE(tradelayer_persistence_tests, BasicTestingSetup)



/**NOTE: make this functions available from tradelayer.h**/
static void write_mp_active_channels(std::string& lineOut)
{
    for (auto it = channels_Map.begin(); it != channels_Map.end(); ++it)
    {
        // decompose the key for address
        const std::string& chnAddr = it->first;
        const channel& chnObj = it->second;

        lineOut = strprintf("%s,%s,%s,%s,%d,%d", chnAddr, chnObj.multisig, chnObj.first, chnObj.second, chnObj.expiry_height, chnObj.last_exchange_block);
        lineOut.append("+");
        addBalances(chnObj.balances, lineOut);
    }

}

int input_activechannels_string(const std::string& s)
{
    std::vector<std::string> vstr;
    boost::split(vstr, s, boost::is_any_of(" ,=+"), boost::token_compress_on);

    channel chn;

    std::string chnAddr = vstr[0];
    chn.multisig = vstr[1];
    chn.first = vstr[2];
    chn.second = vstr[3];
    chn.expiry_height = boost::lexical_cast<int>(vstr[4]);
    chn.last_exchange_block = boost::lexical_cast<int>(vstr[5]);

    // split general data + balances
    std::vector<std::string> vBalance;
    boost::split(vBalance, vstr[6], boost::is_any_of(";"), boost::token_compress_on);


    //address-property:amount;
    for (const auto &v : vBalance)
    {
        //property:amount
        std::vector<std::string> adReg;
        boost::split(adReg, v, boost::is_any_of("-"), boost::token_compress_on);
        const std::string& address = adReg[0];
        std::vector<std::string> reg;
        boost::split(reg, adReg[1], boost::is_any_of(":"), boost::token_compress_on);;
        const uint32_t& property = boost::lexical_cast<uint32_t>(reg[0]);
        const int64_t& amount = boost::lexical_cast<int64_t>(reg[1]);
        chn.balances[address][property] = amount;
    }


    // //inserting chn into map
    if(!channels_Map.insert(std::make_pair(chnAddr,chn)).second) return -1;

    return 0;

}

BOOST_AUTO_TEST_CASE(write_and_input_functions)
{
    // Creating the channel
    channel chn;
    chn.multisig = "Qdj12J6FZgaY34ZNx12pVpTeF9NQdmpGzj";
    chn.first = "mxAsoWQqUupprkj9L3firQ3CmUwyVCAwwY";
    chn.second = "muY24px8kWVHUDc8NmBRjL6UWGbjz8wW5r";
    chn.expiry_height = 1458;
    chn.last_exchange_block = 200;

    // Adding 2563 tokens of property 7 to first address
    const uint32_t propertyId1 = 7;
    const int64_t amount1 = 2563;
    chn.balances["mxAsoWQqUupprkj9L3firQ3CmUwyVCAwwY"][propertyId1] = amount1;


    // Adding 1000 tokens of property 3 to first address
    const uint32_t propertyId2 = 3;
    const int64_t amount2 = 1000;
    chn.balances["mxAsoWQqUupprkj9L3firQ3CmUwyVCAwwY"][propertyId2] = amount2;

    //Adding channel to map
    channels_Map.insert(std::make_pair(chn.multisig,chn));

    std::string lineOut;

    // Testing write function
    write_mp_active_channels(lineOut);
    BOOST_CHECK_EQUAL("Qdj12J6FZgaY34ZNx12pVpTeF9NQdmpGzj,Qdj12J6FZgaY34ZNx12pVpTeF9NQdmpGzj,mxAsoWQqUupprkj9L3firQ3CmUwyVCAwwY,muY24px8kWVHUDc8NmBRjL6UWGbjz8wW5r,1458,200+mxAsoWQqUupprkj9L3firQ3CmUwyVCAwwY-3:1000;7:2563;", lineOut);

    // Cleaning map
    channels_Map.clear();

    // Testing insert function
    BOOST_CHECK_EQUAL(0,input_activechannels_string(lineOut));

    //Finding the channel on the map
    auto it = channels_Map.find("Qdj12J6FZgaY34ZNx12pVpTeF9NQdmpGzj");

    channel chn1;
    int64_t newAmount = 0;
    if (it != channels_Map.end())
    {
        chn1 = it->second;
        const auto &pMap = chn1.balances;
        auto itt = pMap.find(chn.first);
        if (itt != pMap.end()){
            const auto &bMap = itt->second;
            auto ittt = bMap.find(propertyId2);
            if (ittt != bMap.end()){
                newAmount = ittt->second;
            }
        }
    }

    BOOST_CHECK_EQUAL("Qdj12J6FZgaY34ZNx12pVpTeF9NQdmpGzj",chn1.multisig);
    BOOST_CHECK_EQUAL("mxAsoWQqUupprkj9L3firQ3CmUwyVCAwwY",chn1.first);
    BOOST_CHECK_EQUAL("muY24px8kWVHUDc8NmBRjL6UWGbjz8wW5r",chn1.second);
    BOOST_CHECK_EQUAL(1458, chn1.expiry_height);
    BOOST_CHECK_EQUAL(200, chn1.last_exchange_block);
    BOOST_CHECK_EQUAL(1000, newAmount);

}

BOOST_AUTO_TEST_SUITE_END()
