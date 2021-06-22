#include <test/test_bitcoin.h>
#include <tradelayer/dex.h>
#include <tradelayer/mdex.h>
#include <tradelayer/register.h>
#include <tradelayer/tally.h>
#include <tradelayer/tradelayer.h>
#include <tradelayer/tx.h>

#include <boost/algorithm/string.hpp>
#include <boost/test/unit_test.hpp>
#include <stdint.h>

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

        lineOut = strprintf("%s,%s,%s,%s,%d", chnAddr, chnObj.getMultisig(), chnObj.getFirst(), chnObj.getSecond(), chnObj.getLastBlock());
        lineOut.append("+");
        addBalances(chnObj.getBalanceMap(), lineOut);
    }

}

int input_activechannels_string(const std::string& s)
{
    std::vector<std::string> vstr;
    boost::split(vstr, s, boost::is_any_of(" ,=+"), boost::token_compress_on);

    std::string chnAddr = vstr[0];
    const int last_exchange_block = boost::lexical_cast<int>(vstr[4]);
    Channel chn(vstr[1], vstr[2], vstr[3], last_exchange_block);

    // split general data + balances
    std::vector<std::string> vBalance;
    boost::split(vBalance, vstr[5], boost::is_any_of(";"), boost::token_compress_on);

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
            const int64_t realizedProfit = (*iter).second.getMoney(propertyId, REALIZED_PROFIT);
            const int64_t realizedLosses = (*iter).second.getMoney(propertyId, REALIZED_LOSSES);
            const int64_t remaining = (*iter).second.getMoney(propertyId, REMAINING);
            const int64_t unvested = (*iter).second.getMoney(propertyId, UNVESTED);
            // we don't allow 0 balances to read in, so if we don't write them
            // it makes things match up better between persisted state and processed state
            if (0 == balance && 0 == sellReserved && 0 == acceptReserved && 0 == metadexReserved && contractdexReserved == 0 && realizedProfit == 0 && realizedLosses == 0 && remaining == 0 && unvested == 0) {
                continue;
            }

            lineOut.append(strprintf("%d:%d,%d,%d,%d,%d,%d,%d,%d,%d;",
                    propertyId,
                    balance,
                    sellReserved,
                    acceptReserved,
                    metadexReserved,
                    contractdexReserved,
                    realizedProfit,
                    realizedLosses,
                    remaining,
                    unvested));
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
        if (curBalance.size() != 10) return -1;

        const uint32_t propertyId = boost::lexical_cast<uint32_t>(curProperty[0]);
        const int64_t balance = boost::lexical_cast<int64_t>(curBalance[0]);
        const int64_t sellReserved = boost::lexical_cast<int64_t>(curBalance[1]);
        const int64_t acceptReserved = boost::lexical_cast<int64_t>(curBalance[2]);
        const int64_t metadexReserved = boost::lexical_cast<int64_t>(curBalance[3]);
        const int64_t contractdexReserved = boost::lexical_cast<int64_t>(curBalance[4]);
        const int64_t realizeProfit = boost::lexical_cast<int64_t>(curBalance[5]);
        const int64_t realizeLosses = boost::lexical_cast<int64_t>(curBalance[6]);
        const int64_t remaining = boost::lexical_cast<int64_t>(curBalance[7]);
        const int64_t unvested = boost::lexical_cast<int64_t>(curBalance[8]);

        if (balance) update_tally_map(strAddress, propertyId, balance, BALANCE);
        if (sellReserved) update_tally_map(strAddress, propertyId, sellReserved, SELLOFFER_RESERVE);
        if (acceptReserved) update_tally_map(strAddress, propertyId, acceptReserved, ACCEPT_RESERVE);
        if (metadexReserved) update_tally_map(strAddress, propertyId, metadexReserved, METADEX_RESERVE);

        if (contractdexReserved) update_tally_map(strAddress, propertyId, contractdexReserved, CONTRACTDEX_RESERVE);
        if (realizeProfit) update_tally_map(strAddress, propertyId, realizeProfit, REALIZED_PROFIT);
        if (realizeLosses) update_tally_map(strAddress, propertyId, realizeLosses, REALIZED_LOSSES);
        if (remaining) update_tally_map(strAddress, propertyId, remaining, REMAINING);
        if (unvested) update_tally_map(strAddress, propertyId, unvested, UNVESTED);
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

void saveOffer(const CMPContractDex cont, std::string& lineOut)
{

    lineOut = strprintf("%s,%d,%d,%d,%d,%d,%d,%d,%s,%d,%d,%d,%d",
    cont.getAddr(),
    cont.getBlock(),
    cont.getAmountForSale(),
    cont.getProperty(),
    cont.getAmountDesired(),
    cont.getDesProperty(),
    cont.getAction(),
    cont.getIdx(),
    cont.getHash().ToString(),
    cont.getAmountRemaining(),
    cont.getEffectivePrice(),
    cont.getTradingAction(),
    cont.getAmountReserved()
    );

    // BOOST_TEST_MESSAGE("lineOut:" << lineOut << "\n");
}

static int write_mp_contractdex(std::string& lineOut)
{
    for (const auto con : contractdex)
    {
        const cd_PricesMap &prices = con.second;

        for (const auto p : prices)
        {
            const cd_Set &indexes = p.second;

            for (const auto in : indexes)
            {
                const CMPContractDex& contract = in;
                saveOffer(contract, lineOut);
            }
        }
    }

    return 0;
}

int input_mp_contractdexorder_string(const std::string& s)
{
    // BOOST_TEST_MESSAGE("s:" << s << "\n");

    std::vector<std::string> vstr;
    boost::split(vstr, s, boost::is_any_of(" ,="), boost::token_compress_on);

    if (13 != vstr.size()) return -1;

    int i = 0;

    const std::string addr = vstr[i++];
    const int block = boost::lexical_cast<int>(vstr[i++]);
    const int64_t amount_forsale = boost::lexical_cast<int64_t>(vstr[i++]);
    const uint32_t property = boost::lexical_cast<uint32_t>(vstr[i++]);
    const int64_t amount_desired = boost::lexical_cast<int64_t>(vstr[i++]);
    const uint32_t desired_property = boost::lexical_cast<uint32_t>(vstr[i++]);
    const uint8_t subaction = boost::lexical_cast<unsigned int>(vstr[i++]); // lexical_cast can't handle char!
    const unsigned int idx = boost::lexical_cast<unsigned int>(vstr[i++]);
    const uint256 txid = uint256S(vstr[i++]);
    const int64_t amount_remaining = boost::lexical_cast<int64_t>(vstr[i++]);
    const uint64_t effective_price = boost::lexical_cast<uint64_t>(vstr[i++]);
    const uint8_t trading_action = boost::lexical_cast<unsigned int>(vstr[i++]);
    const int64_t amount_reserved = boost::lexical_cast<int64_t>(vstr[i++]);
    //
    // BOOST_TEST_MESSAGE("address:" << addr << "\n");
    // BOOST_TEST_MESSAGE("block:" << block << "\n");
    // BOOST_TEST_MESSAGE("amount for sale:" << amount_forsale << "\n");
    // BOOST_TEST_MESSAGE("property:" << property << "\n");
    // BOOST_TEST_MESSAGE("amount desired:" << amount_desired << "\n");
    // BOOST_TEST_MESSAGE("desired property:" << desired_property << "\n");
    // BOOST_TEST_MESSAGE("subaction:" << subaction << "\n");
    // BOOST_TEST_MESSAGE("idx:" << idx << "\n");
    // BOOST_TEST_MESSAGE("desired property:" << vstr[8] << "\n");
    // BOOST_TEST_MESSAGE("amount remaining:" <<  amount_remaining << "\n");
    // BOOST_TEST_MESSAGE("effective price:" <<  effective_price << "\n");
    // BOOST_TEST_MESSAGE("trading action:" <<  trading_action << "\n");
    // BOOST_TEST_MESSAGE("amount reserved:" <<  amount_reserved << "\n");

    CMPContractDex mdexObj(addr, block, property, amount_forsale, desired_property,
            amount_desired, txid, idx, subaction, amount_remaining, effective_price, trading_action, amount_reserved);

    if (!ContractDex_INSERT(mdexObj)) return -1;

    return 0;
}

void saveOffer(const CMPMetaDEx meta, std::string& lineOut)
{

   lineOut = strprintf("%s,%d,%d,%d,%d,%d,%d,%d,%s,%d",
      meta.getAddr(),
      meta.getBlock(),
      meta.getAmountForSale(),
      meta.getProperty(),
      meta.getAmountDesired(),
      meta.getDesProperty(),
      meta.getAction(),
      meta.getIdx(),
      meta.getHash().ToString(),
      meta.getAmountRemaining()
  );

    // BOOST_TEST_MESSAGE("lineOut:" << lineOut << "\n");
}

static int write_mp_metadex(std::string& lineOut)
{
    for (const auto my_it : metadex)
    {
        const md_PricesMap & prices = my_it.second;

        for (const auto it : prices)
        {
            const md_Set & indexes = it.second;

            for (const auto in : indexes)
            {
                const CMPMetaDEx& meta = in;
                saveOffer(meta, lineOut);
            }
        }
    }

    return 0;
}

int input_mp_mdexorder_string(const std::string& s)
{
    std::vector<std::string> vstr;
    boost::split(vstr, s, boost::is_any_of(" ,="), boost::token_compress_on);

    if (10 != vstr.size()) return -1;

    int i = 0;
    const std::string addr = vstr[i++];
    const int block = boost::lexical_cast<int>(vstr[i++]);
    const int64_t amount_forsale = boost::lexical_cast<int64_t>(vstr[i++]);
    const uint32_t property = boost::lexical_cast<uint32_t>(vstr[i++]);
    const int64_t amount_desired = boost::lexical_cast<int64_t>(vstr[i++]);
    const uint32_t desired_property = boost::lexical_cast<uint32_t>(vstr[i++]);
    const uint8_t subaction = boost::lexical_cast<unsigned int>(vstr[i++]); // lexical_cast can't handle char!
    const unsigned int idx = boost::lexical_cast<unsigned int>(vstr[i++]);
    const uint256 txid = uint256S(vstr[i++]);
    const int64_t amount_remaining = boost::lexical_cast<int64_t>(vstr[i++]);

    CMPMetaDEx mdexObj(addr, block, property, amount_forsale, desired_property,
            amount_desired, txid, idx, subaction, amount_remaining);

    if (!MetaDEx_INSERT(mdexObj)) return -1;

    return 0;
}


int input_tokenvwap_string(const std::string& s)
{
  std::vector<std::string> vstr;
  boost::split(vstr, s, boost::is_any_of("+-"), boost::token_compress_on);

  // BOOST_TEST_MESSAGE("vstr[0]:" << vstr[0] << "\n");
  // BOOST_TEST_MESSAGE("vstr[1]:" << vstr[1] << "\n");
  const uint32_t propertyId = boost::lexical_cast<uint32_t>(vstr[0]);
  const int block = boost::lexical_cast<int64_t>(vstr[1]);

  // BOOST_TEST_MESSAGE("vstr[2]:" << vstr[2] << "\n");
  std::vector<std::string> vpair;
  boost::split(vpair, vstr[2], boost::is_any_of(","), boost::token_compress_on);

  for (const auto& np : vpair)
  {
      // BOOST_TEST_MESSAGE("np:" << np << "\n");
      std::vector<std::string> pAmount;
      boost::split(pAmount, np, boost::is_any_of(":"), boost::token_compress_on);
      const int64_t unitPrice = boost::lexical_cast<int64_t>(pAmount[0]);
      const int64_t amount = boost::lexical_cast<int64_t>(pAmount[1]);
      // BOOST_TEST_MESSAGE("unitPrice:" << unitPrice << "\n");
      // BOOST_TEST_MESSAGE("amount:" << amount << "\n");
      tokenvwap[propertyId][block].push_back(std::make_pair(unitPrice, amount));
  }

  return 0;
}

static int write_mp_tokenvwap(std::string& lineOut)
{
    for (const auto &mp : tokenvwap)
    {
        // decompose the key for address
        const uint32_t& propertyId = mp.first;
        const auto &blcmap = mp.second;

        // BOOST_TEST_MESSAGE("propertyId:" << propertyId << "\n");

        lineOut.append(strprintf("%d+",propertyId));

        for (const auto &vec : blcmap){
            // adding block number
            // BOOST_TEST_MESSAGE("block:" << vec.first);
            lineOut.append(strprintf("%d-",vec.first));

            // vector of pairs
            const auto &vpairs = vec.second;
            for (auto p = vpairs.begin(); p != vpairs.end(); ++p)
            {
                const std::pair<int64_t,int64_t>& vwPair = *p;
                lineOut.append(strprintf("%d:%d", vwPair.first, vwPair.second));
                if (p != std::prev(vpairs.end())) lineOut.append(",");
            }

        }

    }

    return 0;
}


static int write_mp_register(std::string& lineOut)
{
  for (auto &iter : mp_register_map)
  {
      lineOut = iter.first;
      lineOut.append("=");
      Register& reg = iter.second;
      reg.init();
      uint32_t contractId = 0;
      while (0 != (contractId = reg.next()))
      {
          const int64_t entryPrice = reg.getRecord(contractId, ENTRY_CPRICE);
          const int64_t position = reg.getRecord(contractId, CONTRACT_POSITION);
          const int64_t liquidationPrice = reg.getRecord(contractId, LIQUIDATION_PRICE);
          const int64_t upnl = reg.getRecord(contractId, UPNL);
          const int64_t margin = reg.getRecord(contractId, MARGIN);
          const int64_t leverage = reg.getRecord(contractId, LEVERAGE);

          // we don't allow 0 balances to read in, so if we don't write them
          // it makes things match up better between persisted state and processed state
          if (0 == entryPrice && 0 == position && 0 == liquidationPrice && 0 == upnl && 0 == margin && leverage == 0) {
              continue;
          }

          // saving each record
          lineOut.append(strprintf("%d:%d,%d,%d,%d,%d,%d",
                  contractId,
                  entryPrice,
                  position,
                  liquidationPrice,
                  upnl,
                  margin,
                  leverage));

          // saving now the entries (contracts, price)
          const Entries* entry = reg.getEntries(contractId);

          if(entry != nullptr)
          {

              for (Entries::const_iterator it = entry->begin(); it != entry->end(); ++it)
              {
                  if (it == entry->begin()) lineOut.append("-");

                  const std::pair<int64_t,int64_t>& pair = *it;
                  const int64_t& amount = pair.first;
                  const int64_t& price = pair.second;
                  lineOut.append(strprintf("%d,%d", amount, price));

                  if (it != std::prev(entry->end())) {
                      // BOOST_TEST_MESSAGE("appending |");
                      lineOut.append("|");
                  }
                  //
                  // BOOST_TEST_MESSAGE("amount:" <<  amount);
                  // BOOST_TEST_MESSAGE("price:" <<  price);
              }
          }

          lineOut.append(";");

       }

    }

    return 0;
}

static int input_register_string(std::string& s)
{
  // "address=contract_register"
  std::vector<std::string> addrData;
  boost::split(addrData, s, boost::is_any_of("="), boost::token_compress_on);
  if (addrData.size() != 2) return -1;

  std::string strAddress = addrData[0];

  // split the tuples of contract registers
  std::vector<std::string> vRegisters;
  boost::split(vRegisters, addrData[1], boost::is_any_of(";"), boost::token_compress_on);

  // BOOST_TEST_MESSAGE("s:" << s);
  // BOOST_TEST_MESSAGE("addrData[0]:" << addrData[0]);
  // BOOST_TEST_MESSAGE("addrData[1]:" << addrData[1]);

  for (auto iter : vRegisters)
  {
      if (iter.empty()) {
          continue;
      }

     // BOOST_TEST_MESSAGE("Register:" << iter);

     // full register (records + entries)
     std::vector<std::string> fRegister;
     boost::split(fRegister, iter, boost::is_any_of("-"), boost::token_compress_on);

     // contract id + records
     std::vector<std::string> idRecord;
     boost::split(idRecord, fRegister[0], boost::is_any_of(":"), boost::token_compress_on);

     // just records
     std::vector<std::string> vRecord;
     boost::split(vRecord, idRecord[1], boost::is_any_of(","), boost::token_compress_on);

     // BOOST_TEST_MESSAGE("ContractId:" << idRecord[0]);
     const uint32_t contractId = boost::lexical_cast<uint32_t>(idRecord[0]);
     const int64_t entryPrice = boost::lexical_cast<int64_t>(vRecord[0]);
     const int64_t position = boost::lexical_cast<int64_t>(vRecord[1]);
     const int64_t liquidationPrice = boost::lexical_cast<int64_t>(vRecord[2]);
     const int64_t upnl = boost::lexical_cast<int64_t>(vRecord[3]);
     const int64_t margin = boost::lexical_cast<int64_t>(vRecord[4]);
     const int64_t leverage = boost::lexical_cast<int64_t>(vRecord[5]);

     if (entryPrice) update_register_map(strAddress, contractId, entryPrice, ENTRY_CPRICE);
     if (position) update_register_map(strAddress, contractId, position, CONTRACT_POSITION);
     if (liquidationPrice) update_register_map(strAddress, contractId, liquidationPrice, LIQUIDATION_PRICE);
     if (upnl) update_register_map(strAddress, contractId, upnl, UPNL);
     if (margin) update_register_map(strAddress, contractId, margin, MARGIN);
     if (leverage) update_register_map(strAddress, contractId, entryPrice, LEVERAGE);

     // BOOST_TEST_MESSAGE("Record[2]:" << vRecord[2]);
     // BOOST_TEST_MESSAGE("Record[3]:" << vRecord[3]);
     // BOOST_TEST_MESSAGE("Record[4]:" << vRecord[4]);
     // BOOST_TEST_MESSAGE("Record[5]:" << vRecord[5]);

     // if there's no entries, skip
     if (fRegister.size() == 1) {
         continue;
     }

     // just entries
     std::vector<std::string> entries;
     boost::split(entries, fRegister[1], boost::is_any_of("|"), boost::token_compress_on);

     for (auto it : entries)
     {

         std::vector<std::string> entry;
         boost::split(entry, it, boost::is_any_of(","), boost::token_compress_on);
         // BOOST_TEST_MESSAGE("entry[0]:" << entry[0]);
         // BOOST_TEST_MESSAGE("entry[1]:" << entry[1]);
         const int64_t amount = boost::lexical_cast<int64_t>(entry[0]);
         const int64_t price = boost::lexical_cast<int64_t>(entry[1]);
         assert(insert_entry(strAddress, contractId, amount, price));

     }


   }

  return 0;
}



BOOST_AUTO_TEST_CASE(channel_persistence)
{
    // Creating the channel
    const std::string multisig = "Qdj12J6FZgaY34ZNx12pVpTeF9NQdmpGzj";
    const std::string first = "mxAsoWQqUupprkj9L3firQ3CmUwyVCAwwY";
    const std::string second = "muY24px8kWVHUDc8NmBRjL6UWGbjz8wW5r";
    const int last_exchange_block = 200;

    Channel chn(multisig, first, second, last_exchange_block);

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
        const auto &pMap = chn1.getBalanceMap();
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

BOOST_AUTO_TEST_CASE(cdex_persistence)
{
    const std::string address = "1dexX7zmPen1yBz2H9ZF62AK5TGGqGTZH";
    CMPContractDex seller(address,
		    172, // block
			  1,   // property for sale
			  500000000,   // amount of contracts for sale
			  0,   // desired property
			  0,
			  uint256S("11"), // txid
			  1,   // position in block
			  1,   // subaction
		  	0,   // amount remaining
		  	500000000,   // effective_price
		  	2,  // trading_action
        100000000);  // amount reserved

        CMPContractDex *s = &seller;

        BOOST_CHECK(update_tally_map(seller.getAddr(),seller.getProperty(), 100000, CONTRACTDEX_RESERVE));

        BOOST_CHECK_EQUAL(100000, getMPbalance(seller.getAddr(), seller.getProperty(), CONTRACTDEX_RESERVE));

        BOOST_CHECK(ContractDex_INSERT(*s));

        std::string lineOut;

        // writting on persistence
        write_mp_contractdex(lineOut);

        BOOST_CHECK_EQUAL("1dexX7zmPen1yBz2H9ZF62AK5TGGqGTZH,172,500000000,1,0,0,1,1,0000000000000000000000000000000000000000000000000000000000000011,0,500000000,2,100000000",lineOut);

        // clearing map
        contractdex.clear();

        // writting on memory
        input_mp_contractdexorder_string(lineOut);

        CMPContractDex seller1;
        auto it = contractdex.find(1);
        if (it != contractdex.end())
        {
            const auto &cd_pMap = it->second;
            auto itt = cd_pMap.find(500000000);
            if (itt != cd_pMap.end()){
                const auto &pSet = itt->second;
                auto pCont = find_if(pSet.begin(), pSet.end(), [&address] (const CMPContractDex& cont) { return (cont.getAddr() == address);});
                if (pCont != pSet.end()){
                    seller1 = *pCont;
                }

            }
        }

        BOOST_CHECK_EQUAL(address, seller1.getAddr());
        BOOST_CHECK_EQUAL(1, seller1.getProperty());
        BOOST_CHECK_EQUAL(172,seller1.getBlock());
        BOOST_CHECK_EQUAL(500000000, seller1.getAmountForSale());
        BOOST_CHECK_EQUAL(0, seller1.getAmountDesired());
        BOOST_CHECK_EQUAL(0, seller1.getDesProperty());
        BOOST_CHECK_EQUAL(0, seller1.getAmountRemaining());
        BOOST_CHECK_EQUAL(500000000, seller1.getEffectivePrice());
        BOOST_CHECK_EQUAL(100000000, seller1.getAmountReserved());
}

BOOST_AUTO_TEST_CASE(mdex_persistence)
{
    const std::string address = "1dexX7zmPen1yBz2H9ZF62AK5TGGqGTZH";
    CMPMetaDEx seller(address, 200, 1, 1000, 2, 2000, uint256S("11"), 1, CMPTransaction::ADD);


    CMPMetaDEx *s = &seller;

    BOOST_CHECK(update_tally_map(seller.getAddr(),seller.getProperty(), 100000, METADEX_RESERVE));

    BOOST_CHECK_EQUAL(100000, getMPbalance(seller.getAddr(), seller.getProperty(), METADEX_RESERVE));

    BOOST_CHECK(MetaDEx_INSERT(*s));

    std::string lineOut;

    // writting on persistence
    write_mp_metadex(lineOut);

    BOOST_CHECK_EQUAL("1dexX7zmPen1yBz2H9ZF62AK5TGGqGTZH,200,1000,1,2000,2,1,1,0000000000000000000000000000000000000000000000000000000000000011,1000",lineOut);

    // clearing map
    metadex.clear();

    // writting on memory
    input_mp_mdexorder_string(lineOut);

    CMPMetaDEx seller1;
    md_PricesMap* prices = get_Prices(1);
    for (auto my_it = prices->begin(); my_it != prices->end(); ++my_it)
    {
        md_Set* indexes = &(my_it->second);

        auto it = find_if(indexes->begin(), indexes->end(), [&address] (CMPMetaDEx mdexObj) { return (address == mdexObj.getAddr());});

        if (it != indexes->end()) {
           seller1 = *it;
           break;
        }

     }

     BOOST_CHECK_EQUAL(address, seller1.getAddr());
     BOOST_CHECK_EQUAL(1, seller1.getProperty());
     BOOST_CHECK_EQUAL(200,seller1.getBlock());
     BOOST_CHECK_EQUAL(1000, seller1.getAmountForSale());
     BOOST_CHECK_EQUAL(2000, seller1.getAmountDesired());
     BOOST_CHECK_EQUAL(2, seller1.getDesProperty());
     BOOST_CHECK_EQUAL(1000, seller1.getAmountRemaining());
}

BOOST_AUTO_TEST_CASE(tokenvwap_persistence)
{
  const int64_t unitPrice = 1000 * COIN;
  const int64_t amount = 2600 * COIN;
  const uint32_t propertyId = 3;
  const int block = 1006541;

  // adding price and amount in block
  tokenvwap[propertyId][block].push_back(std::make_pair(unitPrice, amount));

  std::string lineOut;
  // writting on persistence
  write_mp_tokenvwap(lineOut);

  BOOST_CHECK_EQUAL("3+1006541-100000000000:260000000000",lineOut);

  // clearing map
  tokenvwap.clear();

  // writting on memory
  input_tokenvwap_string(lineOut);

  std::pair<int64_t,int64_t> test;
  auto it = tokenvwap.find(propertyId);
  if (it != tokenvwap.end())
  {
      // finding block
      const auto &blkmap = it->second;
      auto itt = blkmap.find(block);
      if (itt != blkmap.end()){
          const auto &vpair = itt->second;
          const auto& p =  vpair.begin();
          test = *p;
      }
  }

  BOOST_CHECK_EQUAL(1000 * COIN, test.first);
  BOOST_CHECK_EQUAL(2600 * COIN, test.second);


}


BOOST_AUTO_TEST_CASE(contract_register)
{
  const std::string address = "muY24px8kWVHUDc8NmBRjL6UWGbjz8wW5r";

  BOOST_CHECK(update_register_map(address, 1, 10000, ENTRY_CPRICE));
  BOOST_CHECK(update_register_map(address, 2, 20000, CONTRACT_POSITION));

  std::string lineOut;

  // Testing write function
  write_mp_register(lineOut);

  // %d:%d,%d,%d,%d,%d,%d;
  BOOST_CHECK_EQUAL("muY24px8kWVHUDc8NmBRjL6UWGbjz8wW5r=1:10000,0,0,0,0,0;2:0,20000,0,0,0,0;",lineOut);

  // inserting entry:
  BOOST_CHECK(insert_entry(address, 1, 8000, 2560));
  BOOST_CHECK(insert_entry(address, 2, 4000, 9875));
  BOOST_CHECK(insert_entry(address, 2, 5000, 10000));

  // Testing write function
  write_mp_register(lineOut);

  BOOST_CHECK_EQUAL("muY24px8kWVHUDc8NmBRjL6UWGbjz8wW5r=1:10000,0,0,0,0,0-8000,2560;2:0,20000,0,0,0,0-4000,9875|5000,10000;",lineOut);

  // clearing map
  mp_register_map.clear();

  //saving to memory
  input_register_string(lineOut);

  //Checking register in map:
  std::unordered_map<std::string, Register>::iterator my_it = mp_register_map.find(address);
  Register& reg = my_it->second;

  const Entries* pEnt = reg.getEntries(2);

  std::vector<std::pair<int64_t,int64_t>> entryResult;

  for (Entries::const_iterator it = pEnt->begin(); it != pEnt->end(); ++it)
  {
      const std::pair<int64_t,int64_t>& pair = *it;
      entryResult.push_back(pair);
  }

  BOOST_CHECK(entryResult.size() == 2);

  std::pair<int64_t,int64_t>& pair1  = entryResult[0];
  std::pair<int64_t,int64_t>& pair2  = entryResult[1];

  BOOST_CHECK_EQUAL(pair1.first, 4000);
  BOOST_CHECK_EQUAL(pair1.second, 9875);

  BOOST_CHECK_EQUAL(pair2.first, 5000);
  BOOST_CHECK_EQUAL(pair2.second, 10000);


}

BOOST_AUTO_TEST_SUITE_END()
