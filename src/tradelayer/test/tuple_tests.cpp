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
    auto t = NC::Parse<int,std::string,bool,char,short,A,int64_t,float>("10,20,1,65,-2;A::s_; -9223372036854775807;1.1754e-38");

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
    // lexical conversion, i.e. char conversion might throw
    using LC = Convertor<ConversionPolicy<std::false_type, std::true_type>>;
    
    auto pt = LC::ParseEx<char,uint32_t,int64_t>("A;1,10");

    BOOST_CHECK_EQUAL(3, pt.first);
    BOOST_CHECK_EQUAL('A',  std::get<0>(pt.second));
    BOOST_CHECK_EQUAL(1, std::get<1>(pt.second));
    BOOST_CHECK_EQUAL(10, std::get<2>(pt.second));
}

BOOST_AUTO_TEST_SUITE_END()