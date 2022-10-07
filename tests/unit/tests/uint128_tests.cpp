#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include <fc/uint128.hpp>

BOOST_AUTO_TEST_SUITE( uint128_tests )

BOOST_AUTO_TEST_CASE( uint128_t )
{
  try
  {
    BOOST_REQUIRE_EQUAL(std::string(fc::uint128()), std::string("0"));
    BOOST_REQUIRE_EQUAL(std::string(fc::uint128(0)), std::string("0"));
    BOOST_REQUIRE_EQUAL(fc::uint128(8).to_uint64(), 8u);
    BOOST_REQUIRE_EQUAL(fc::uint128(6789).to_uint64(), 6789u);
    BOOST_REQUIRE_EQUAL(fc::uint128(10000).to_uint64(), 10000u);
    BOOST_REQUIRE_EQUAL(std::string(fc::uint128(1)), std::string("1"));
    BOOST_REQUIRE_EQUAL(std::string(fc::uint128(5)), std::string("5"));
    BOOST_REQUIRE_EQUAL(std::string(fc::uint128(12345)), std::string("12345"));
    BOOST_REQUIRE_EQUAL(std::string(fc::uint128(0)), std::string(fc::uint128("0")));

    fc::uint128 ten(10);
    fc::uint128 two(2);
    fc::uint128 twenty(20);
    fc::uint128 pi(31415920000);
    pi /=      10000;

    BOOST_REQUIRE_EQUAL( std::string(ten), "10" );
    BOOST_REQUIRE_EQUAL( std::string(two), "2" );
    BOOST_REQUIRE_EQUAL( std::string(ten+two), "12" );
    BOOST_REQUIRE_EQUAL( std::string(ten-two), "8" );
    BOOST_REQUIRE_EQUAL( std::string(ten*two), "20" );
    BOOST_REQUIRE_EQUAL( std::string(ten/two), "5" );
    BOOST_REQUIRE_EQUAL( std::string(ten/two/two/two*two*two*two), "8" );
    BOOST_REQUIRE_EQUAL( std::string(ten/two/two/two*two*two*two), std::string(two*two*two) );
    BOOST_REQUIRE_EQUAL( std::string(twenty/ten), std::string(two) );
    BOOST_REQUIRE_EQUAL( std::string(pi), "3141592" );
    BOOST_REQUIRE_EQUAL( std::string(pi*10), "31415920" );
    BOOST_REQUIRE_EQUAL( std::string(pi*20), "62831840" );
    BOOST_REQUIRE_EQUAL( std::string(pi*1), "3141592" );
    BOOST_REQUIRE_EQUAL( std::string(pi*0), "0" );

    const auto CONTENT_CONSTANT_HF0                 = (fc::uint128_t(uint64_t(2000000000000ll)));
    const auto HF21_CONVERGENT_LINEAR_RECENT_CLAIMS = (fc::uint128_t(0,503600561838938636ull));
    const auto CONTENT_CONSTANT_HF21                = (fc::uint128_t(0,2000000000000ull));
    const auto VIRTUAL_SCHEDULE_LAP_LENGTH          = (fc::uint128(uint64_t(-1)));

    BOOST_REQUIRE_EQUAL( CONTENT_CONSTANT_HF0.operator std::string().size(), 13 );

    BOOST_REQUIRE_EQUAL( std::string(CONTENT_CONSTANT_HF0), std::string("2000000000000") );
    BOOST_REQUIRE_EQUAL( std::string(HF21_CONVERGENT_LINEAR_RECENT_CLAIMS), std::string("503600561838938636") );
    BOOST_REQUIRE_EQUAL( std::string(CONTENT_CONSTANT_HF21), std::string("2000000000000") );
    BOOST_REQUIRE_EQUAL( std::string(VIRTUAL_SCHEDULE_LAP_LENGTH), std::string("18446744073709551615") );

    const auto VIRTUAL_SCHEDULE_LAP_LENGTH2 = (fc::uint128::max_value());
    const auto lo_hi                        = (fc::uint128(10, 12));

    BOOST_REQUIRE_EQUAL( std::string(VIRTUAL_SCHEDULE_LAP_LENGTH2), std::string("340282366920938463463374607431768211455") );
    BOOST_REQUIRE_EQUAL( std::string(lo_hi), std::string("184467440737095516172") );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
#endif
