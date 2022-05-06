#include <test/test_bitcoin.h>
#include <tradelayer/tupleutils.hpp>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(tradelayer_tuple_tests, BasicTestingSetup)

// *boost::unit_test::tolerance(0.0001)
BOOST_AUTO_TEST_CASE(mighty_tuple)
{
    struct A
    {
        std::string _s;
        A() {}
        A(const std::string& s) : _s(s) {}
    };

    // max:int64_t 9223372036854775807
    // max:int32_t 2147483647
    auto t = NC::Parse<int,std::string,bool,uint8_t,short,A,int64_t,float>("10,20,-3,65,-2;A::s_; -9223372036854775807;1.1754e-38");

    BOOST_CHECK_EQUAL(10, std::get<0>(t));
    BOOST_CHECK_EQUAL("20", std::get<1>(t));
    BOOST_CHECK_EQUAL(true, std::get<2>(t));
    BOOST_CHECK_EQUAL('A', std::get<3>(t));
    BOOST_CHECK_EQUAL(-2, std::get<4>(t));
    BOOST_CHECK_EQUAL("A::s_", std::get<5>(t)._s);
    BOOST_CHECK_EQUAL(-9223372036854775807, std::get<6>(t));
    BOOST_TEST(1.1754e-38, std::get<7>(t));
}

BOOST_AUTO_TEST_CASE(lexical_conversion)
{
    // lexical conversion nothrow policy
    using LC = Convertor<NoThrowPolicy, LexicalTraits>;
    auto t = LC::Parse<char,uint8_t,bool>("65;66,3");

    BOOST_CHECK_EQUAL(0, std::get<0>(t));
    BOOST_CHECK_EQUAL(0, std::get<1>(t));
    BOOST_CHECK_EQUAL(false, std::get<2>(t));
}

BOOST_AUTO_TEST_CASE(fringe_conversion)
{
    auto p1 = NC::ParseEx<char,bool,int>("66");
    auto tuple_size1 = std::tuple_size<decltype(p1.second)>::value;
    auto tuple_size2 = p1.first;
    
    BOOST_CHECK_NE(tuple_size1, tuple_size2);
    BOOST_CHECK_EQUAL(3, tuple_size1);
    BOOST_CHECK_EQUAL(1, tuple_size2);

    BOOST_CHECK_EQUAL('B', std::get<0>(p1.second));
    BOOST_CHECK_EQUAL(false, std::get<1>(p1.second));
    BOOST_CHECK_EQUAL(0, std::get<2>(p1.second));

    auto p2 = NC::ParseEx<char>("67,20,30");
    
    BOOST_CHECK_EQUAL(p2.first, std::tuple_size<decltype(p2.second)>::value);
    BOOST_CHECK_EQUAL(1, p2.first);
    BOOST_CHECK_EQUAL('C', std::get<0>(p2.second));
}

BOOST_AUTO_TEST_SUITE_END()