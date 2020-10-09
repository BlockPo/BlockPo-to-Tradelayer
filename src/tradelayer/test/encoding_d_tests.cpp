#include <tradelayer/encoding.h>

#include <tradelayer/script.h>

#include <script/script.h>
#include <test/test_bitcoin.h>
#include <util/strencodings.h>

#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include <boost/test/unit_test.hpp>

// is reset to a norm value in each test
extern unsigned nMaxDatacarrierBytes;

BOOST_FIXTURE_TEST_SUITE(tradelayer_encoding_d_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(class_d_marker)
{
    // Store initial data carrier size
    unsigned nMaxDatacarrierBytesOriginal = nMaxDatacarrierBytes;

    nMaxDatacarrierBytes = 40; // byte

    std::vector<unsigned char> vchMarker;
    vchMarker.push_back(0x77); // "p"
    vchMarker.push_back(0x77); // "q"

    std::vector<unsigned char> vchPayload = ParseHex("0000101");

    std::vector<std::pair<CScript, int64_t> > vecOutputs;
    BOOST_CHECK(TradeLayer_Encode_ClassD(vchPayload, vecOutputs));

    // One output was created
    BOOST_CHECK_EQUAL(vecOutputs.size(), 1);

    // Extract the embedded data
    std::pair<CScript, int64_t> pairOutput = vecOutputs.front();
    CScript scriptData = pairOutput.first;

    std::vector<std::string> vstrEmbeddedData;
    BOOST_CHECK(GetScriptPushes(scriptData, vstrEmbeddedData));

    BOOST_CHECK_EQUAL(vstrEmbeddedData.size(), 1);
    std::string strEmbeddedData = vstrEmbeddedData.front();
    std::vector<unsigned char> vchEmbeddedData = ParseHex(strEmbeddedData);

    // The embedded data has a size of the payload plus marker
    BOOST_CHECK_EQUAL(
            vchEmbeddedData.size(),
            vchMarker.size() + vchPayload.size());

    // The output script really starts with the marker
    for (size_t n = 0; n < vchMarker.size(); ++n) {
        BOOST_CHECK_EQUAL(vchMarker[n], vchEmbeddedData[n]);
    }

    // The output script really ends with the payload
    std::vector<unsigned char> vchEmbeddedPayload(
        vchEmbeddedData.begin() + vchMarker.size(),
        vchEmbeddedData.end());

    BOOST_CHECK_EQUAL(HexStr(vchEmbeddedPayload), HexStr(vchPayload));

    // Restore original data carrier size settings
    nMaxDatacarrierBytes = nMaxDatacarrierBytesOriginal;
}

BOOST_AUTO_TEST_CASE(class_d_with_empty_payload)
{
    // Store initial data carrier size
    unsigned nMaxDatacarrierBytesOriginal = nMaxDatacarrierBytes;

    const std::vector<unsigned char> vchEmptyPayload;
    BOOST_CHECK_EQUAL(vchEmptyPayload.size(), 0);

    // Even less than the size of the marker
    nMaxDatacarrierBytes = 0; // byte

    std::vector<std::pair<CScript, int64_t> > vecOutputs;
    BOOST_CHECK(!TradeLayer_Encode_ClassD(vchEmptyPayload, vecOutputs));
    BOOST_CHECK_EQUAL(vecOutputs.size(), 0);

    // Exactly the size of the marker
    nMaxDatacarrierBytes = 2; // byte

    BOOST_CHECK(TradeLayer_Encode_ClassD(vchEmptyPayload, vecOutputs));
    BOOST_CHECK_EQUAL(vecOutputs.size(), 1);

    // Restore original data carrier size settings
    nMaxDatacarrierBytes = nMaxDatacarrierBytesOriginal;
}


BOOST_AUTO_TEST_SUITE_END()
