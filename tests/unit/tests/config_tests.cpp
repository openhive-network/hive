#ifdef IS_TEST_NET

#include <boost/test/unit_test.hpp>

#include "../db_fixture/config_fixture.hpp"

#include <boost/range/combine.hpp>

using namespace hive::chain;

void verify_logging_config_against_pattern( const config_fixture& cf, const std::vector< std::string >& pattern )
{
  const auto logging_config = cf.get_logging_config();

  BOOST_ASSERT( logging_config.valid() && "Logging config should have been filled by now" );
  BOOST_REQUIRE_EQUAL( logging_config->appenders.size(), pattern.size() );
  for( const auto [ appender, appender_pattern ] : boost::combine( logging_config->appenders, pattern ) )
  {
    BOOST_REQUIRE_EQUAL( fc::json::to_string( appender, fc::json::legacy_generator ), appender_pattern );
  }
}

BOOST_FIXTURE_TEST_SUITE( config_tests, config_fixture )

BOOST_AUTO_TEST_CASE( default_logging_config_test )
{ try {

  BOOST_TEST_MESSAGE( "Verifying logging-related setting from default auto-generated config.ini" );

  postponed_init(); // default appender config

  verify_logging_config_against_pattern( *this, std::vector< std::string >( {
    R"~({"name":"stderr","type":"console","args":{"format":"${timestamp} ${context} ${file}:${line} ${method} ${level}]  ${message}","stream":"std_error","level_colors":[{"level":"debug","color":"green"},{"level":"warn","color":"brown"},{"level":"error","color":"red"}],"flush":true,"time_format":"iso_8601_microseconds"},"enabled":true})~",
    R"~({"name":"p2p","type":"file","args":{"format":"${timestamp} ${context} ${file}:${line} ${method} ${level}]  ${message}","filename":"/tmp/hive-tmp/logs/p2p/p2p.log","flush":true,"rotate":true,"rotation_interval":3600000000,"rotation_limit":86400000000,"time_format":"iso_8601_milliseconds","delta_times":false},"enabled":true})~"
    } )
  );

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( custom_logging_config_test )
{ try {

  BOOST_TEST_MESSAGE( "Verifying logging-related settings from custom config.ini" );
  // Use non-default values for every available attribute.
  postponed_init( std::vector< std::string >( {
        R"~({"appender":"stderr","stream":"std_error","time_format":"milliseconds_since_hour"})~",
        R"~({"appender":"p2p","file":"logs/p2p/p2p.log","time_format":"milliseconds_since_epoch","delta_times":true,"flush":false,})~",
        R"~({"appender":"3","file":"logs/p2p/p3p.log","time_format":"iso_8601_seconds","rotate":false})~",
        R"~({"appender":"4","file":"logs/p2p/p4p.log","time_format":"iso_8601_milliseconds","rotation_interval":1})~",
        R"~({"appender":"5","file":"logs/p2p/p5p.log","time_format":"iso_8601_microseconds","rotation_limit":3600})~"
      } ) );

  verify_logging_config_against_pattern( *this, std::vector< std::string >( {
    R"~({"name":"stderr","type":"console","args":{"format":"${timestamp} ${context} ${file}:${line} ${method} ${level}]  ${message}","stream":"std_error","level_colors":[{"level":"debug","color":"green"},{"level":"warn","color":"brown"},{"level":"error","color":"red"}],"flush":true,"time_format":"milliseconds_since_hour"},"enabled":true})~",
    R"~({"name":"p2p","type":"file","args":{"format":"${timestamp} ${context} ${file}:${line} ${method} ${level}]  ${message}","filename":"/tmp/hive-tmp/logs/p2p/p2p.log","flush":false,"rotate":true,"rotation_interval":3600000000,"rotation_limit":86400000000,"time_format":"milliseconds_since_epoch","delta_times":true},"enabled":true})~",
    R"~({"name":"3","type":"file","args":{"format":"${timestamp} ${context} ${file}:${line} ${method} ${level}]  ${message}","filename":"/tmp/hive-tmp/logs/p2p/p3p.log","flush":true,"rotate":false,"rotation_interval":3600000000,"rotation_limit":86400000000,"time_format":"iso_8601_seconds","delta_times":false},"enabled":true})~",
    R"~({"name":"4","type":"file","args":{"format":"${timestamp} ${context} ${file}:${line} ${method} ${level}]  ${message}","filename":"/tmp/hive-tmp/logs/p2p/p4p.log","flush":true,"rotate":true,"rotation_interval":1000000,"rotation_limit":86400000000,"time_format":"iso_8601_milliseconds","delta_times":false},"enabled":true})~",
    R"~({"name":"5","type":"file","args":{"format":"${timestamp} ${context} ${file}:${line} ${method} ${level}]  ${message}","filename":"/tmp/hive-tmp/logs/p2p/p5p.log","flush":true,"rotate":true,"rotation_interval":3600000000,"rotation_limit":3600000000,"time_format":"iso_8601_microseconds","delta_times":false},"enabled":true})~"
    } ) );

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

#endif
