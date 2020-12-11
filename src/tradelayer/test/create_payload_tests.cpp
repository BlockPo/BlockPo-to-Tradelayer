#include <test/test_bitcoin.h>
#include <tradelayer/createpayload.h>
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>
#include <stdint.h>
#include <string>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(tradelayer_create_payload_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(payload_simple_send)
{
    // Simple send [type 0, version 0]
    std::vector<unsigned char> vch = CreatePayload_SimpleSend(
        static_cast<uint32_t>(1),          // property: MSC
        static_cast<int64_t>(100000000));  // amount to transfer: 1.0 MSC (in willets)

    BOOST_CHECK_EQUAL(HexStr(vch), "00000180c2d72f");
}

BOOST_AUTO_TEST_CASE(payload_send_all)
{
    // Send to owners [type 4, version 0]
    std::vector<unsigned char> vch = CreatePayload_SendAll();

    BOOST_CHECK_EQUAL(HexStr(vch), "0004");
}

BOOST_AUTO_TEST_CASE(payload_dex_offer)
{
    // Sell tokens for bitcoins [type 20, version 1]
    std::vector<unsigned char> vch = CreatePayload_DExSell(
        static_cast<uint32_t>(1),         // property: MSC
        static_cast<int64_t>(100000000),  // amount to transfer: 1.0 MSC (in willets)
        static_cast<int64_t>(20000000),   // amount desired: 0.2 BTC (in satoshis)
        static_cast<uint8_t>(10),         // payment window in blocks
        static_cast<int64_t>(10000),      // commitment fee in satoshis
        static_cast<uint8_t>(1));         // sub-action: new offer

    BOOST_CHECK_EQUAL(HexStr(vch),
        "01140180c2d72f80dac4090a904e01");
}

BOOST_AUTO_TEST_CASE(payload_meta_dex_new_trade)
{
    // Trade tokens for tokens [type 25, version 0]
    std::vector<unsigned char> vch = CreatePayload_MetaDExTrade(
        static_cast<uint32_t>(1),          // property: MSC
        static_cast<int64_t>(250000000),   // amount for sale: 2.5 MSC
        static_cast<uint32_t>(31),         // property desired: TetherUS
        static_cast<int64_t>(5000000000)); // amount desired: 50.0 TetherUS

    BOOST_CHECK_EQUAL(HexStr(vch),
        "00190180e59a771f80e497d012");
}

BOOST_AUTO_TEST_CASE(payload_contractdex_cancel_all)
{

    std::vector<unsigned char> vch = CreatePayload_ContractDexCancelAll(
        static_cast<uint8_t>(7)     // contractId
        );

    BOOST_CHECK_EQUAL(HexStr(vch),
        "002007");
}

BOOST_AUTO_TEST_CASE(payload_accept_dex_offer)
{
    // Purchase tokens with bitcoins [type 22, version 0]
    std::vector<unsigned char> vch = CreatePayload_DExAccept(
        static_cast<uint32_t>(1),          // property: MSC
        static_cast<int64_t>(130000000));  // amount to transfer: 1.3 MSC (in willets)

    BOOST_CHECK_EQUAL(HexStr(vch), "00160180c9fe3d");
}

BOOST_AUTO_TEST_CASE(payload_create_property)
{
    // Create property [type 50, version 0]
    std::vector<int> numbers;
    numbers.push_back(0);
    std::string name = "Lihki Coin";
    std::string url = "www.parcero.col";
    std::string data = "";

    std::vector<unsigned char> vch = CreatePayload_IssuanceFixed(
        static_cast<uint16_t>(1),            // property type: indivisible tokens
        static_cast<uint32_t>(0),            // previous property: none
        name,                                // name
        url,                                 // url
        data,                                // data
        static_cast<int64_t>(1000000),       // number of units to create
        numbers);


    BOOST_CHECK_EQUAL(HexStr(vch),
        "003201004c69686b6920436f696e007777772e7061726365726f2e636f6c0000c0843d00");
}

BOOST_AUTO_TEST_CASE(payload_create_property_empty)
{
    // Create property [type 50, version 0]
    std::vector<int> numbers;
    numbers.push_back(0);
    std::string name = "";
    std::string url = "";
    std::string data = "";

    std::vector<unsigned char> vch = CreatePayload_IssuanceFixed(
        static_cast<uint16_t>(1),        // property type: indivisible tokens
        static_cast<uint32_t>(0),        // previous property: none
        name,                 // name
        url,                 // url
        data,                 // data
        static_cast<int64_t>(1000000),  // number of units to create
        numbers);

    BOOST_CHECK_EQUAL(vch.size(), 11);
}

BOOST_AUTO_TEST_CASE(payload_create_property_full)
{
    // Create property [type 50, version 0]
    std::vector<int> numbers;
    numbers.push_back(0);
    std::string name = std::string(700, 'x');
    std::string url = std::string(700, 'x');
    std::string data = std::string(700, 'x');

    std::vector<unsigned char> vch = CreatePayload_IssuanceFixed(
        static_cast<uint16_t>(1),        // property type: indivisible tokens
        static_cast<uint32_t>(0),        // previous property: none
        name,           // name
        url,           // url
        data,           // data
        static_cast<int64_t>(1000000),  // number of units to create
        numbers);

    BOOST_CHECK_EQUAL(vch.size(), 776);
}

BOOST_AUTO_TEST_CASE(payload_create_managed_property)
{
    // create managed property [type 54, version 0]
    std::vector<int> numbers;
    numbers.push_back(0);
    std::string name = "Name";
    std::string url = "www.website.com";
    std::string data = "some data";

    std::vector<unsigned char> vch = CreatePayload_IssuanceManaged(
        static_cast<uint16_t>(1),            // property type: indivisible tokens
        static_cast<uint32_t>(0),            // previous property: none
        name,                                // name
        url,                                 // url
        data,                                // data
        numbers);

    BOOST_CHECK_EQUAL(HexStr(vch),"003601004e616d65007777772e776562736974652e636f6d00736f6d6520646174610000");
}

BOOST_AUTO_TEST_CASE(payload_create_managed_property_empty)
{
    // create managed property [type 54, version 0]
    std::vector<int> numbers;
    numbers.push_back(0);
    std::string name = "";
    std::string url = "";
    std::string data = "";
    std::vector<unsigned char> vch = CreatePayload_IssuanceManaged(
        static_cast<uint16_t>(1),   // property type: indivisible tokens
        static_cast<uint32_t>(0),   // previous property: none
        name,                       // name
        url,                        // url
        data,                       // data
        numbers);

    BOOST_CHECK_EQUAL(vch.size(), 8);

}

BOOST_AUTO_TEST_CASE(payload_create_managed_property_full)
{
    // create managed property [type 54, version 0]
    std::vector<int> numbers;
    numbers.push_back(0);
    numbers.push_back(1);
    std::string name = std::string(700, 'x');
    std::string url = std::string(700, 'x');
    std::string data = std::string(700, 'x');
    std::vector<unsigned char> vch = CreatePayload_IssuanceManaged(
        static_cast<uint16_t>(1),   // property type: indivisible tokens
        static_cast<uint32_t>(0),   // previous property: none
        name,                       // name
        url,                        // url
        data,                       // data
        numbers);
    BOOST_CHECK_EQUAL(vch.size(), 774);

}

BOOST_AUTO_TEST_CASE(payload_grant_tokens)
{
    // Grant tokens [type 55, version 0]
    std::vector<unsigned char> vch = CreatePayload_Grant(
        static_cast<uint32_t>(8),                  // property: SP #8
        static_cast<int64_t>(1000)                // number of units to issue
    );

    BOOST_CHECK_EQUAL(HexStr(vch),"003708e807");
}

BOOST_AUTO_TEST_CASE(payload_revoke_tokens)
{
    // Revoke tokens [type 56, version 0]
    std::vector<unsigned char> vch = CreatePayload_Revoke(
        static_cast<uint32_t>(8),                                   // property: SP #8
        static_cast<int64_t>(1000)                                  // number of units to revoke
    );

    BOOST_CHECK_EQUAL(HexStr(vch),
        "003808e807");
}

BOOST_AUTO_TEST_CASE(payload_revoke_tokens_empty)
{
    // Revoke tokens [type 56, version 0]
    std::vector<unsigned char> vch = CreatePayload_Revoke(
        static_cast<uint32_t>(8),    // property: SP #8
        static_cast<int64_t>(1000)  // number of units to revoke
    );
    BOOST_CHECK_EQUAL(vch.size(), 5);
}

BOOST_AUTO_TEST_CASE(payload_revoke_tokens_full)
{
    // Revoke tokens [type 56, version 0]
    std::vector<unsigned char> vch = CreatePayload_Revoke(
        static_cast<uint32_t>(8),    // property: SP #8
        static_cast<int64_t>(1000)  // number of units to revoke
    );
    BOOST_CHECK_EQUAL(vch.size(), 5);
}

BOOST_AUTO_TEST_CASE(payload_change_property_manager)
{
    // Change property manager [type 70, version 0]
    std::vector<unsigned char> vch = CreatePayload_ChangeIssuer(
        static_cast<uint32_t>(13));  // property: SP #13

    BOOST_CHECK_EQUAL(HexStr(vch), "00460d");
}

// BOOST_AUTO_TEST_CASE(payload_create_contract)
// {
//     std::vector<unsigned char> vch = CreatePayload_CreateContract(
//         static_cast<uint32_t>(1),            // denomType: 1 (dUSD)
//         std::string("ALL/dUSD"),             // name
//         static_cast<uint32_t>(3000),         // blocks until expiration
//         static_cast<uint32_t>(2),            // notional size
//         static_cast<uint32_t>(3),            // collateral currency
//         static_cast<uint64_t>(3)             // margin rpcrequirements
//     );
//     BOOST_CHECK_EQUAL(HexStr(vch),"00280101414c4c2f6455534400b81702030300");
// }

// BOOST_AUTO_TEST_CASE(payload_trade_contract)
// {
//     std::vector<unsigned char> vch = CreatePayload_ContractDexTrade(
//         static_cast<uint32_t>(7),            // contractId
//         static_cast<uint64_t>(1000),         // amount for sale
//         static_cast<uint64_t>(644),          // effective price
//         static_cast<uint8_t>(2)              // trading action
//     );
//         BOOST_CHECK_EQUAL(HexStr(vch),"001d07e807840502");
// }

BOOST_AUTO_TEST_CASE(payload_cancel_orders_by_address)
{
    std::vector<unsigned char> vch = CreatePayload_ContractDexCancelAll(
        static_cast<uint64_t>(7)         // contractId
    );
    BOOST_CHECK_EQUAL(HexStr(vch),"002007");
}

BOOST_AUTO_TEST_CASE(payload_cancel_orders_by_block)
{
    std::vector<unsigned char> vch = CreatePayload_ContractDexCancelOrderByTxId(
        static_cast<int>(1),                  // block
        static_cast<unsigned int>(7)         // idx
    );
    BOOST_CHECK_EQUAL(HexStr(vch),"00220107");}

BOOST_AUTO_TEST_CASE(payload_issuance_pegged)
{
  std::string name = "dUSD";
    std::vector<unsigned char> vch = CreatePayload_IssuancePegged(
        static_cast<uint16_t>(1),            // property type
        static_cast<uint32_t>(0),            // previous propertyId
        name,                                // name
        static_cast<uint32_t>(2),            // propertyId
        static_cast<uint32_t>(3),            // contractId
        static_cast<uint32_t>(78563)         // amount
    );
        BOOST_CHECK_EQUAL(HexStr(vch),"0064010064555344000203e3e504");
}

BOOST_AUTO_TEST_CASE(payload_send_pegged)
{

  std::vector<unsigned char> vch = CreatePayload_SendPeggedCurrency(static_cast<uint32_t>(0),              // propertyId
								    static_cast<uint32_t>(78563));         // amount
  BOOST_CHECK_EQUAL(HexStr(vch),"006600e3e504");
  BOOST_CHECK_EQUAL(vch.size(), 6);
}

BOOST_AUTO_TEST_CASE(payload_redemption_pegged)
{
    std::vector<unsigned char> vch = CreatePayload_RedemptionPegged(
        static_cast<uint32_t>(6),        // propertyId
        static_cast<uint32_t>(5),        // contractId
        static_cast<uint64_t>(0)         // amount
    );

    BOOST_CHECK_EQUAL(HexStr(vch),"0065060500");
    BOOST_CHECK_EQUAL(vch.size(), 5);
}

BOOST_AUTO_TEST_CASE(payload_close_position)
{

    std::vector<unsigned char> vch = CreatePayload_ContractDexClosePosition(
        static_cast<uint32_t>(5)        // contractId
    );

    BOOST_CHECK_EQUAL(HexStr(vch),"002105");
    BOOST_CHECK_EQUAL(vch.size(), 3);
}

BOOST_AUTO_TEST_CASE(payload_send_trade)
{
    std::vector<unsigned char> vch = CreatePayload_MetaDExTrade(
        static_cast<uint32_t>(1),         //propertyId for sale
        static_cast<uint64_t>(1000),      // amount for sale
        static_cast<uint32_t>(4),         // propertyId desired
        static_cast<uint64_t>(200)         // amount desired
    );

    BOOST_CHECK_EQUAL(HexStr(vch),"001901e80704c801");
    BOOST_CHECK_EQUAL(vch.size(), 8);
}

BOOST_AUTO_TEST_CASE(payload_send_dex_offer)
{
    std::vector<unsigned char> vch = CreatePayload_DExSell(
        static_cast<uint32_t>(1),         //propertyId
        static_cast<uint64_t>(600),       // amount for sale
        static_cast<uint64_t>(4),         // amount desired
        static_cast<uint8_t>(2000),       // time limit
        static_cast<uint64_t>(3),         // min fee
        static_cast<uint32_t>(1)          // sub action
    );

    BOOST_CHECK_EQUAL(HexStr(vch),"011401d80404d0010301");
    BOOST_CHECK_EQUAL(vch.size(), 10);
}

BOOST_AUTO_TEST_CASE(payload_send_dex_accept)
{
    std::vector<unsigned char> vch = CreatePayload_DExAccept(
        static_cast<uint32_t>(1),         //propertyId
        static_cast<uint64_t>(600)       // amount accepted
    );

    BOOST_CHECK_EQUAL(HexStr(vch),"001601d804");
    BOOST_CHECK_EQUAL(vch.size(), 5);
}

BOOST_AUTO_TEST_SUITE_END()
