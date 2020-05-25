#include "tradelayer/uint256_extensions.h"
#include "test/test_bitcoin.h"

#include <arith_uint256.h>
#include <boost/test/unit_test.hpp>

#include <stdint.h>
#include <limits>

using namespace mastercore;

BOOST_FIXTURE_TEST_SUITE(tradelayer_uint256_extensions_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(uint256_from_uint64_t)
{
    uint64_t number = 103242;

    arith_uint256 a(number);

    BOOST_CHECK_EQUAL(number, a.GetLow64());
}

BOOST_AUTO_TEST_CASE(uint256_add)
{
    uint64_t number_a = 103242;
    uint64_t number_b = 234324;
    uint64_t number_c = number_a + number_b;

    arith_uint256 a(number_a);
    arith_uint256 b(number_b);
    arith_uint256 c = a + b;

    BOOST_CHECK_EQUAL(number_c, c.GetLow64());
}

BOOST_AUTO_TEST_CASE(uint256_sub)
{
    uint64_t number_a = 503242;
    uint64_t number_b = 234324;
    uint64_t number_c = number_a - number_b;

    arith_uint256 a(number_a);
    arith_uint256 b(number_b);
    arith_uint256 c = a - b;

    BOOST_CHECK_EQUAL(number_c, c.GetLow64());
}

BOOST_AUTO_TEST_CASE(uint256_mul)
{
    uint64_t number_a = 503242;
    uint64_t number_b = 234324;
    uint64_t number_c = number_a * number_b;

    arith_uint256 a(number_a);
    arith_uint256 b(number_b);
    arith_uint256 c = a * b;

    BOOST_CHECK_EQUAL(number_c, c.GetLow64());
}

BOOST_AUTO_TEST_CASE(uint256_div)
{
    uint64_t number_a = 9;
    uint64_t number_b = 4;
    uint64_t number_c = number_a / number_b;

    arith_uint256 a(number_a);
    arith_uint256 b(number_b);
    arith_uint256 c = a / b;

    BOOST_CHECK_EQUAL(number_c, c.GetLow64());
}

BOOST_AUTO_TEST_CASE(uint256_conversion)
{
    BOOST_CHECK_EQUAL(0, ConvertTo64(ConvertTo256(0)));
    BOOST_CHECK_EQUAL(1, ConvertTo64(ConvertTo256(1)));
    BOOST_CHECK_EQUAL(1000000000000000000LL, ConvertTo64(ConvertTo256(1000000000000000000LL)));
    BOOST_CHECK_EQUAL(9223372036854775807LL, ConvertTo64(ConvertTo256(9223372036854775807LL)));
}

BOOST_AUTO_TEST_CASE(uint256_modulo)
{
    BOOST_CHECK_EQUAL(1, ConvertTo64(Modulo256(ConvertTo256(9), ConvertTo256(4))));
}

BOOST_AUTO_TEST_CASE(uint256_modulo_auto_conversion)
{
    BOOST_CHECK_EQUAL(2, ConvertTo64(Modulo256(17, 3)));
}

BOOST_AUTO_TEST_CASE(uint256_divide_and_round_up)
{
    BOOST_CHECK_EQUAL(3, ConvertTo64(DivideAndRoundUp(ConvertTo256(5), ConvertTo256(2))));
}

BOOST_AUTO_TEST_CASE(uint256_const)
{
    BOOST_CHECK_EQUAL(1, ConvertTo64(mastercore::uint256_const::one));
    BOOST_CHECK_EQUAL(std::numeric_limits<int64_t>::max(), ConvertTo64(mastercore::uint256_const::max_int64));
}

arith_uint256 amountReserved(int64_t amount, uint32_t margin_requirement, uint64_t leverage, int64_t uPrice)
{
    return (ConvertTo256(COIN) * ConvertTo256(amount) * ConvertTo256(margin_requirement)) / (ConvertTo256(leverage) * ConvertTo256(uPrice));
}

BOOST_AUTO_TEST_CASE(contractdex_trade_amount)
{
   int64_t uPrice = 1000000000 * COIN;      // price = 1 billion of dUSD
   uint64_t leverage = 10 * COIN;           // max leverage
   uint32_t marginRequirement = 42 * COIN;  // 42 tokens per contract (almost the maximum)

   // 1 contract
   BOOST_CHECK_EQUAL(ConvertTo64(amountReserved(1 * COIN, marginRequirement, leverage, uPrice)),  ConvertTo64(ConvertTo256(0)));  // 0.42 (in practice it's 0)

   // 1000 contracts
   BOOST_CHECK_EQUAL(ConvertTo64(amountReserved(1000 * COIN, marginRequirement, leverage, uPrice)), ConvertTo64(ConvertTo256(420)));  // 420  (in practice it's 0.0000042)

   // 1000 000 contracts
   BOOST_CHECK_EQUAL(ConvertTo64(amountReserved(1000000 * COIN, marginRequirement, leverage, uPrice)), ConvertTo64(ConvertTo256(420000)));  // 420 000  (in practice it's 0.0042)

   // 1000 000 000 contracts
   BOOST_CHECK_EQUAL(ConvertTo64(amountReserved(1000000000 * COIN, marginRequirement, leverage, uPrice)), ConvertTo64(ConvertTo256(420000000)));  // 420 000 000  (in practice it's 4.2)

}

BOOST_AUTO_TEST_CASE(contractdex_trade_leverage)
{
   int64_t amount = 1000000000 * COIN;      // price = 1 billion contracts
   int64_t uPrice = 1000000000 * COIN;      // price = 1 billion of dUSD
   uint32_t marginRequirement = 42 * COIN;  // 42 tokens per contract (almost the maximum)

   // leverage 1
   BOOST_CHECK_EQUAL(ConvertTo64(amountReserved(amount, marginRequirement, 1 * COIN, uPrice)),  ConvertTo64(ConvertTo256(4200000000))); // (in practice it's 4.2)

   // leverage 2
   BOOST_CHECK_EQUAL(ConvertTo64(amountReserved(amount, marginRequirement, 2 * COIN, uPrice)), ConvertTo64(ConvertTo256(2100000000)));  // (in practice it's 2.1)

   // leverage 10
   BOOST_CHECK_EQUAL(ConvertTo64(amountReserved(amount, marginRequirement, 10 * COIN, uPrice)), ConvertTo64(ConvertTo256(420000000)));  //(in practice it's 0.42)


}

BOOST_AUTO_TEST_SUITE_END()
