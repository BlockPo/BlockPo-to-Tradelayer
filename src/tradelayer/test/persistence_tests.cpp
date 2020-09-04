#include "tradelayer/tally.h"
#include "tradelayer/tradelayer.h"
#include "tradelayer/tx.h"
#include "test/test_bitcoin.h"

#include <stdint.h>

#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string.hpp>

using namespace mastercore;

BOOST_FIXTURE_TEST_SUITE(tradelayer_persistence_tests, BasicTestingSetup)



/**NOTE: make this functions available from tradelayer.h**/
static void write_mp_active_channels(std::string& lineOut)
{
    for (const auto &cm : channels_Map)
    {
        // decompose the key for address
        const std::string& chnAddr = cm.first;
        const Channel& chnObj = cm.second;

        lineOut = strprintf("%s,%s,%s,%s,%d,%d", chnAddr, chnObj.getMultisig(), chnObj.getFirst(), chnObj.getSecond(), chnObj.getExpiry(), chnObj.getLastBlock());
        lineOut.append("+");
        addBalances(chnObj.balances, lineOut);
    }

}

int input_activechannels_string(const std::string& s)
{
    std::vector<std::string> vstr;
    boost::split(vstr, s, boost::is_any_of(" ,=+"), boost::token_compress_on);

    std::string chnAddr = vstr[0];
    const int expiry_height = boost::lexical_cast<int>(vstr[4]);
    const int last_exchange_block = boost::lexical_cast<int>(vstr[5]);
    Channel chn(vstr[1], vstr[2], vstr[3], expiry_height, last_exchange_block);

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
        const uint32_t property = boost::lexical_cast<uint32_t>(reg[0]);
        const int64_t amount = boost::lexical_cast<int64_t>(reg[1]);
        chn.setBalance(address, property, amount);
        // BOOST_TEST_MESSAGE( "(input) property :" << property );
        // BOOST_TEST_MESSAGE( "(input) amount :" << amount );
    }


    // //inserting chn into map
    if(!channels_Map.insert(std::make_pair(chnAddr,chn)).second) return -1;

    return 0;

}

static int write_msc_balances(std::string& lineOut)
{
    std::unordered_map<std::string, CMPTally>::iterator iter;
    for (iter = mp_tally_map.begin(); iter != mp_tally_map.end(); ++iter)
    {
        lineOut = (*iter).first;
        lineOut.append("=");
        CMPTally& curAddr = (*iter).second;
        curAddr.init();
        uint32_t propertyId = 0;
        while (0 != (propertyId = curAddr.next())) {
            const int64_t balance = (*iter).second.getMoney(propertyId, BALANCE);
            const int64_t sellReserved = (*iter).second.getMoney(propertyId, SELLOFFER_RESERVE);
            const int64_t acceptReserved = (*iter).second.getMoney(propertyId, ACCEPT_RESERVE);
            const int64_t metadexReserved = (*iter).second.getMoney(propertyId, METADEX_RESERVE);
            const int64_t contractdexReserved = (*iter).second.getMoney(propertyId, CONTRACTDEX_RESERVE);
            const int64_t positiveBalance = (*iter).second.getMoney(propertyId, POSITIVE_BALANCE);
            const int64_t negativeBalance = (*iter).second.getMoney(propertyId, NEGATIVE_BALANCE);
            const int64_t realizedProfit = (*iter).second.getMoney(propertyId, REALIZED_PROFIT);
            const int64_t realizedLosses = (*iter).second.getMoney(propertyId, REALIZED_LOSSES);
            const int64_t remaining = (*iter).second.getMoney(propertyId, REMAINING);
            const int64_t unvested = (*iter).second.getMoney(propertyId, UNVESTED);
            const int64_t channelReserve = (*iter).second.getMoney(propertyId, CHANNEL_RESERVE);
            // we don't allow 0 balances to read in, so if we don't write them
            // it makes things match up better between persisted state and processed state
            if (0 == balance && 0 == sellReserved && 0 == acceptReserved && 0 == metadexReserved && contractdexReserved == 0 && positiveBalance == 0 && negativeBalance == 0 && realizedProfit == 0 && realizedLosses == 0 && remaining == 0 && unvested == 0 && channelReserve == 0) {
                continue;
            }

            lineOut.append(strprintf("%d:%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d;",
                    propertyId,
                    balance,
                    sellReserved,
                    acceptReserved,
                    metadexReserved,
                    contractdexReserved,
                    positiveBalance,
                    negativeBalance,
                    realizedProfit,
                    realizedLosses,
                    remaining,
                    unvested,
                    channelReserve));
        }

    }

    return 0;
}

int input_msc_balances_string(const std::string& s)
{
    // "address=propertybalancedata"
    std::vector<std::string> addrData;
    boost::split(addrData, s, boost::is_any_of("="), boost::token_compress_on);
    if (addrData.size() != 2) return -1;

    std::string strAddress = addrData[0];

    // split the tuples of properties
    std::vector<std::string> vProperties;
    boost::split(vProperties, addrData[1], boost::is_any_of(";"), boost::token_compress_on);

    std::vector<std::string>::const_iterator iter;
    for (iter = vProperties.begin(); iter != vProperties.end(); ++iter) {
        if ((*iter).empty()) {
            continue;
        }

        // "propertyid:balancedata"
        std::vector<std::string> curProperty;
        boost::split(curProperty, *iter, boost::is_any_of(":"), boost::token_compress_on);
        if (curProperty.size() != 2) return -1;

        std::vector<std::string> curBalance;
        boost::split(curBalance, curProperty[1], boost::is_any_of(","), boost::token_compress_on);
        if (curBalance.size() != 12) return -1;

        const uint32_t propertyId = boost::lexical_cast<uint32_t>(curProperty[0]);
        const int64_t balance = boost::lexical_cast<int64_t>(curBalance[0]);
        const int64_t sellReserved = boost::lexical_cast<int64_t>(curBalance[1]);
        const int64_t acceptReserved = boost::lexical_cast<int64_t>(curBalance[2]);
        const int64_t metadexReserved = boost::lexical_cast<int64_t>(curBalance[3]);
        const int64_t contractdexReserved = boost::lexical_cast<int64_t>(curBalance[4]);
        const int64_t positiveBalance = boost::lexical_cast<int64_t>(curBalance[5]);
        const int64_t negativeBalance = boost::lexical_cast<int64_t>(curBalance[6]);
        const int64_t realizeProfit = boost::lexical_cast<int64_t>(curBalance[7]);
        const int64_t realizeLosses = boost::lexical_cast<int64_t>(curBalance[8]);
        const int64_t remaining = boost::lexical_cast<int64_t>(curBalance[9]);
        const int64_t unvested = boost::lexical_cast<int64_t>(curBalance[10]);
        const int64_t channelReserve = boost::lexical_cast<int64_t>(curBalance[11]);

        if (balance) update_tally_map(strAddress, propertyId, balance, BALANCE);
        if (sellReserved) update_tally_map(strAddress, propertyId, sellReserved, SELLOFFER_RESERVE);
        if (acceptReserved) update_tally_map(strAddress, propertyId, acceptReserved, ACCEPT_RESERVE);
        if (metadexReserved) update_tally_map(strAddress, propertyId, metadexReserved, METADEX_RESERVE);

        if (contractdexReserved) update_tally_map(strAddress, propertyId, contractdexReserved, CONTRACTDEX_RESERVE);
        if (positiveBalance) update_tally_map(strAddress, propertyId, positiveBalance, POSITIVE_BALANCE);
        if (negativeBalance) update_tally_map(strAddress, propertyId, negativeBalance, NEGATIVE_BALANCE);
        if (realizeProfit) update_tally_map(strAddress, propertyId, realizeProfit, REALIZED_PROFIT);
        if (realizeLosses) update_tally_map(strAddress, propertyId, realizeLosses, REALIZED_LOSSES);
        if (remaining) update_tally_map(strAddress, propertyId, remaining, REMAINING);
        if (unvested) update_tally_map(strAddress, propertyId, unvested, UNVESTED);
        if (channelReserve) update_tally_map(strAddress, propertyId, channelReserve, CHANNEL_RESERVE);
    }

    return 0;
}


static int write_mp_cachefees(std::string& lineOut)
{
    for (const auto &ca :  cachefees)
    {
        // decompose the key
        const uint32_t& propertyId = ca.first;
        const int64_t& cache = ca.second;

        lineOut = strprintf("%d,%d",propertyId, cache);

    }



    return 0;
}

int input_cachefees_string(const std::string& s)
{
   std::vector<std::string> vstr;
   boost::split(vstr, s, boost::is_any_of(" ,="), boost::token_compress_on);

   const uint32_t propertyId = boost::lexical_cast<uint32_t>(vstr[0]);
   const int64_t amount = boost::lexical_cast<int64_t>(vstr[1]);;

   if (!cachefees.insert(std::make_pair(propertyId, amount)).second) return -1;

   return 0;
}

static void savingLine(std::string& lineOut, const std::string& chnAddr, const withdrawalAccepted&  w)
{
    lineOut = strprintf("%s,%s,%d,%d,%d,%s", chnAddr, w.address, w.deadline_block, w.propertyId, w.amount, (w.txid).ToString());
}

static int write_mp_withdrawals(std::string& lineOut)
{
    for (const auto w : withdrawal_Map)
    {
        // decompose the key
        const std::string& chnAddr = w.first;
        const vector<withdrawalAccepted>& whd = w.second;

        for_each(whd.begin(), whd.end(), [&lineOut, &chnAddr] (const withdrawalAccepted&  w) { savingLine(lineOut, chnAddr, w);});

    }

    return 0;
}

int input_withdrawals_string(const std::string& s)
{
    std::vector<std::string> vstr;
    boost::split(vstr, s, boost::is_any_of(" ,="), boost::token_compress_on);

    withdrawalAccepted w;

    const std::string& chnAddr = vstr[0];
    w.address = vstr[1];
    w.deadline_block = boost::lexical_cast<int>(vstr[2]);
    w.propertyId = boost::lexical_cast<uint32_t>(vstr[3]);
    w.amount = boost::lexical_cast<int64_t>(vstr[4]);
    w.txid = uint256S(vstr[5]);

    auto it = withdrawal_Map.find(chnAddr);

    // channel found !
    if(it != withdrawal_Map.end())
    {
        vector<withdrawalAccepted>& whAc = it->second;
        whAc.push_back(w);
        return 0;
    }

    // if there's no channel yet
    vector<withdrawalAccepted> whAcc;
    whAcc.push_back(w);
    if(!withdrawal_Map.insert(std::make_pair(chnAddr,whAcc)).second) return -1;

    return 0;

}

BOOST_AUTO_TEST_CASE(channel_persistence)
{
    // Creating the channel
    const std::string multisig = "Qdj12J6FZgaY34ZNx12pVpTeF9NQdmpGzj";
    const std::string first = "mxAsoWQqUupprkj9L3firQ3CmUwyVCAwwY";
    const std::string second = "muY24px8kWVHUDc8NmBRjL6UWGbjz8wW5r";
    const int expiry_height = 1458;
    const int last_exchange_block = 200;

    Channel chn(multisig, first, second, expiry_height, last_exchange_block);

    // Adding 2563 tokens of property 7 to first address
    const uint32_t propertyId1 = 7;
    const int64_t amount1 = 2563;
    chn.setBalance(first, propertyId1, amount1);

    BOOST_CHECK_EQUAL(2563, chn.getRemaining(first, propertyId1));

    // Adding 1000 tokens of property 3 to first address
    const uint32_t propertyId2 = 3;
    const int64_t amount2 = 1000;
    chn.setBalance(first, propertyId2, amount2);

    BOOST_CHECK_EQUAL(1000, chn.getRemaining(first, propertyId2));

    //Adding channel to map
    channels_Map.insert(std::make_pair(chn.getMultisig(),chn));

    std::string lineOut;

    // Testing write function
    write_mp_active_channels(lineOut);
    BOOST_CHECK_EQUAL("Qdj12J6FZgaY34ZNx12pVpTeF9NQdmpGzj,Qdj12J6FZgaY34ZNx12pVpTeF9NQdmpGzj,mxAsoWQqUupprkj9L3firQ3CmUwyVCAwwY,muY24px8kWVHUDc8NmBRjL6UWGbjz8wW5r,1458,200+mxAsoWQqUupprkj9L3firQ3CmUwyVCAwwY-3:1000;mxAsoWQqUupprkj9L3firQ3CmUwyVCAwwY-7:2563", lineOut);

    // Cleaning map
    channels_Map.clear();

    // Testing insert function
    BOOST_CHECK_EQUAL(0,input_activechannels_string(lineOut));

    //Finding the channel on the map
    auto it = channels_Map.find("Qdj12J6FZgaY34ZNx12pVpTeF9NQdmpGzj");

    Channel chn1;
    int64_t newAmount = 0;
    if (it != channels_Map.end())
    {
        // BOOST_TEST_MESSAGE( "channel FOUND!\n");
        chn1 = it->second;
        const auto &pMap = chn1.balances;
        auto itt = pMap.find(chn.getFirst());
        if (itt != pMap.end()){
            // BOOST_TEST_MESSAGE( "address in channel FOUND!\n");
            const auto &bMap = itt->second;
            auto ittt = bMap.find(propertyId2);
            if (ittt != bMap.end()){
                newAmount = ittt->second;
            }
        }
    }

    BOOST_CHECK_EQUAL("Qdj12J6FZgaY34ZNx12pVpTeF9NQdmpGzj",chn1.getMultisig());
    BOOST_CHECK_EQUAL("mxAsoWQqUupprkj9L3firQ3CmUwyVCAwwY",chn1.getFirst());
    BOOST_CHECK_EQUAL("muY24px8kWVHUDc8NmBRjL6UWGbjz8wW5r",chn1.getSecond());
    BOOST_CHECK_EQUAL(1458, chn1.getExpiry());
    BOOST_CHECK_EQUAL(200, chn1.getLastBlock());
    BOOST_CHECK_EQUAL(1000, newAmount);

}

BOOST_AUTO_TEST_CASE(balance_persistence)
{
    const std::string address = "muY24px8kWVHUDc8NmBRjL6UWGbjz8wW5r";

    BOOST_CHECK(update_tally_map(address, 1, 10000, BALANCE));
    BOOST_CHECK(update_tally_map(address, 2, 20000, SELLOFFER_RESERVE));

    std::string lineOut;

    // Testing write function
    write_msc_balances(lineOut);
    BOOST_CHECK_EQUAL("muY24px8kWVHUDc8NmBRjL6UWGbjz8wW5r=1:10000,0,0,0,0,0,0,0,0,0,0,0;2:0,20000,0,0,0,0,0,0,0,0,0,0;",lineOut);

    BOOST_CHECK(update_tally_map(address, 3, 30000, ACCEPT_RESERVE));
    BOOST_CHECK(update_tally_map(address, 4, 40000, METADEX_RESERVE));

    write_msc_balances(lineOut);
    BOOST_CHECK_EQUAL("muY24px8kWVHUDc8NmBRjL6UWGbjz8wW5r=1:10000,0,0,0,0,0,0,0,0,0,0,0;2:0,20000,0,0,0,0,0,0,0,0,0,0;3:0,0,30000,0,0,0,0,0,0,0,0,0;4:0,0,0,40000,0,0,0,0,0,0,0,0;",lineOut);

    // cleaning tally
    mp_tally_map.clear();

    input_msc_balances_string(lineOut);

    BOOST_CHECK_EQUAL(10000, getMPbalance(address, 1, BALANCE));
    BOOST_CHECK_EQUAL(20000, getMPbalance(address, 2, SELLOFFER_RESERVE));
    BOOST_CHECK_EQUAL(30000, getMPbalance(address, 3, ACCEPT_RESERVE));
    BOOST_CHECK_EQUAL(40000, getMPbalance(address, 4, METADEX_RESERVE));

}

BOOST_AUTO_TEST_CASE(cachefees_persistence)
{
    // filling cache
    cachefees[5] = 1986;

    std::string lineOut;

    // Testing write function
    write_mp_cachefees(lineOut);
    BOOST_CHECK_EQUAL("5,1986",lineOut);

    // filling cache
    cachefees[6] = 987456;

    write_mp_cachefees(lineOut);
    BOOST_CHECK_EQUAL("6,987456",lineOut);

    // clearing cache
    cachefees.clear();

    input_cachefees_string(lineOut);

    BOOST_CHECK_EQUAL(987456, cachefees[6]);

}

BOOST_AUTO_TEST_CASE(withdrawals_persistence)
{

    withdrawalAccepted w;
    const std::string chnAddr = "Qdj12J6FZgaY34ZNx12pVpTeF9NQdmpGzj";
    w.address = "muY24px8kWVHUDc8NmBRjL6UWGbjz8wW5r";
    w.deadline_block = 150000;
    w.propertyId = 3;
    w.amount = 600000;
    w.txid = uint256S("749eecf591d04339f63b4514130051a1cd552e3ab9671e857839807109899f35");

    vector<withdrawalAccepted> whAcc;
    whAcc.push_back(w);

    withdrawal_Map.insert(std::make_pair(chnAddr, whAcc));

    std::string lineOut;

    //writting on persistence
    write_mp_withdrawals(lineOut);

    BOOST_CHECK_EQUAL("Qdj12J6FZgaY34ZNx12pVpTeF9NQdmpGzj,muY24px8kWVHUDc8NmBRjL6UWGbjz8wW5r,150000,3,600000,749eecf591d04339f63b4514130051a1cd552e3ab9671e857839807109899f35", lineOut);

    // writting on memory
    input_withdrawals_string(lineOut);

    withdrawalAccepted w1;
    auto it = withdrawal_Map.find("Qdj12J6FZgaY34ZNx12pVpTeF9NQdmpGzj");
    if (it != withdrawal_Map.end()){
        const auto &whd = it->second;
        const std::string sAddress = "muY24px8kWVHUDc8NmBRjL6UWGbjz8wW5r";
        auto itt = find_if(whd.begin(), whd.end(), [&sAddress] (const withdrawalAccepted& w) { return (w.address == sAddress);});
        if (itt != whd.end()){
            w1 = *itt;
        }
    }

    BOOST_CHECK_EQUAL("muY24px8kWVHUDc8NmBRjL6UWGbjz8wW5r", w1.address);
    BOOST_CHECK_EQUAL(150000, w1.deadline_block);
    BOOST_CHECK_EQUAL(3, w1.propertyId);
    BOOST_CHECK_EQUAL(600000, w1.amount);
    BOOST_CHECK_EQUAL("749eecf591d04339f63b4514130051a1cd552e3ab9671e857839807109899f35", (w1.txid).ToString());
}

BOOST_AUTO_TEST_SUITE_END()
