#ifdef IS_TEST_NET

#include <boost/test/unit_test.hpp>

#include "../db_fixture/config_fixture.hpp"

using namespace hive::chain;

BOOST_FIXTURE_TEST_SUITE(config_tests, config_fixture)

BOOST_AUTO_TEST_CASE(default_logging_config_test)
{ try {

  BOOST_TEST_MESSAGE( "Verifying logging-related setting from default auto-generated config.ini" );

  const auto logging_config = get_logging_config();

  BOOST_ASSERT( logging_config.valid() && "Logging config should have been filled by now" );
  BOOST_REQUIRE_EQUAL( logging_config->appenders.size(), 2 );
  BOOST_REQUIRE_EQUAL( fc::json::to_string( logging_config->appenders[0] ),
    R"~({"name":"stderr","type":"console","args":{"format":"${timestamp} ${context} ${file}:${line} ${method} ${level}]  ${message}","stream":"std_error","level_colors":[{"level":"debug","color":"green"},{"level":"warn","color":"brown"},{"level":"error","color":"red"}],"flush":true,"time_format":"iso_8601_microseconds"},"enabled":true})~"
  );
  BOOST_REQUIRE_EQUAL( fc::json::to_string( logging_config->appenders[1] ),
    R"~({"name":"p2p","type":"file","args":{"format":"${timestamp} ${context} ${file}:${line} ${method} ${level}]  ${message}","filename":"/tmp/hive-tmp/logs/p2p/p2p.log","flush":true,"rotate":true,"rotation_interval":3600000000,"rotation_limit":"86400000000","time_format":"iso_8601_milliseconds","delta_times":false},"enabled":true})~"
  );

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

#endif
