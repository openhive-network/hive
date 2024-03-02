#ifdef IS_TEST_NET

#include <boost/test/unit_test.hpp>

#include "../db_fixture/hived_fixture.hpp"

#include <boost/range/combine.hpp>

using namespace hive::chain;

typedef std::vector< std::string > pattern_t;
void verify_logging_config_against_pattern( const hived_fixture& cf, 
  const pattern_t& appender_pattern,
  const pattern_t& logger_pattern )
{
  const auto logging_config = cf.get_logging_config();

  FC_ASSERT( logging_config.valid() && "Logging config should have been filled by now" );
  if( not appender_pattern.empty() )
  {
    BOOST_REQUIRE_EQUAL( logging_config->appenders.size(), appender_pattern.size() );
    for( const auto [ appender, appender_pattern ] : boost::combine( logging_config->appenders, appender_pattern ) )
    {
      BOOST_REQUIRE_EQUAL( fc::json::to_string( appender, fc::json::legacy_generator ), appender_pattern );
    }
  }

  if( not logger_pattern.empty() )
  {
    BOOST_REQUIRE_EQUAL( logging_config->loggers.size(), logger_pattern.size() );
    for( const auto [ logger, logger_pattern ] : boost::combine( logging_config->loggers, logger_pattern ) )
    {
      BOOST_REQUIRE_EQUAL( fc::json::to_string( logger, fc::json::legacy_generator ), logger_pattern );
    }
  }
}

BOOST_FIXTURE_TEST_SUITE( config_tests, hived_fixture )

BOOST_AUTO_TEST_CASE( default_logging_config_test )
{ try {

  BOOST_TEST_MESSAGE( "Verifying logging-related setting from default auto-generated config.ini" );

  postponed_init(); // default appender config

  verify_logging_config_against_pattern( *this, 
    pattern_t( {
      R"~({"name":"stderr","type":"console","args":{"format":"${timestamp} ${context} ${file}:${line} ${method} ${level}]  ${message}","stream":"std_error","level_colors":[{"level":"debug","color":"green"},{"level":"warn","color":"brown"},{"level":"error","color":"red"}],"flush":true,"time_format":"iso_8601_microseconds"},"enabled":true})~",
      R"~({"name":"p2p","type":"file","args":{"format":"${timestamp} ${context} ${file}:${line} ${method} ${level}]  ${message}","filename":"/tmp/hive-tmp/logs/p2p/p2p.log","flush":true,"truncate":true,"rotate":true,"rotation_interval":3600000000,"rotation_limit":86400000000,"time_format":"iso_8601_milliseconds","delta_times":false},"enabled":true})~"
    } ),
    pattern_t( {
      R"~({"name":"default","level":"info","enabled":true,"additivity":false,"appenders":["stderr"]})~",
      R"~({"name":"user","level":"debug","enabled":true,"additivity":false,"appenders":["stderr"]})~",
      R"~({"name":"p2p","level":"warn","enabled":true,"additivity":false,"appenders":["p2p"]})~"
    } )
  );

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( custom_logging_config_test )
{ try {

  BOOST_TEST_MESSAGE( "Verifying logging-related settings from custom config.ini" );
  // Use non-default values for every available attribute.
  postponed_init( { config_line_t( {"log-appender", {
    R"~({"appender":"stderr","stream":"std_error","time_format":"milliseconds_since_hour"})~",
    R"~({"appender":"p2p","file":"logs/p2p/p2p.log","time_format":"milliseconds_since_epoch","delta_times":true,"flush":false,"truncate":false})~",
    R"~({"appender":"3","file":"logs/p2p/p3p.log","time_format":"iso_8601_seconds","rotate":false})~",
    R"~({"appender":"4","file":"logs/p2p/p4p.log","time_format":"iso_8601_milliseconds","rotation_interval":1})~",
    R"~({"appender":"5","file":"logs/p2p/p5p.log","time_format":"iso_8601_microseconds","rotation_limit":3600})~"
    } } ) } );

  verify_logging_config_against_pattern( *this, 
    pattern_t( {
      R"~({"name":"stderr","type":"console","args":{"format":"${timestamp} ${context} ${file}:${line} ${method} ${level}]  ${message}","stream":"std_error","level_colors":[{"level":"debug","color":"green"},{"level":"warn","color":"brown"},{"level":"error","color":"red"}],"flush":true,"time_format":"milliseconds_since_hour"},"enabled":true})~",
      R"~({"name":"p2p","type":"file","args":{"format":"${timestamp} ${context} ${file}:${line} ${method} ${level}]  ${message}","filename":"/tmp/hive-tmp/logs/p2p/p2p.log","flush":false,"truncate":false,"rotate":true,"rotation_interval":3600000000,"rotation_limit":86400000000,"time_format":"milliseconds_since_epoch","delta_times":true},"enabled":true})~",
      R"~({"name":"3","type":"file","args":{"format":"${timestamp} ${context} ${file}:${line} ${method} ${level}]  ${message}","filename":"/tmp/hive-tmp/logs/p2p/p3p.log","flush":true,"truncate":true,"rotate":false,"rotation_interval":3600000000,"rotation_limit":86400000000,"time_format":"iso_8601_seconds","delta_times":false},"enabled":true})~",
      R"~({"name":"4","type":"file","args":{"format":"${timestamp} ${context} ${file}:${line} ${method} ${level}]  ${message}","filename":"/tmp/hive-tmp/logs/p2p/p4p.log","flush":true,"truncate":true,"rotate":true,"rotation_interval":1000000,"rotation_limit":86400000000,"time_format":"iso_8601_milliseconds","delta_times":false},"enabled":true})~",
      R"~({"name":"5","type":"file","args":{"format":"${timestamp} ${context} ${file}:${line} ${method} ${level}]  ${message}","filename":"/tmp/hive-tmp/logs/p2p/p5p.log","flush":true,"truncate":true,"rotate":true,"rotation_interval":3600000000,"rotation_limit":3600000000,"time_format":"iso_8601_microseconds","delta_times":false},"enabled":true})~"
    } ),
    pattern_t()
  );

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( rotation_test )
{ try {

  BOOST_TEST_MESSAGE( "Verifying that file rotation actually works, generating multiple log files" );

  postponed_init( { 
    // Set extremely low rotation values (1s/5s).
    config_line_t( {"log-appender", {
      R"~({"appender":"stderr","stream":"std_error","time_format":"iso_8601_microseconds"})~",
      R"~({"appender":"p2p","file":"logs/p2p/p2p.log","time_format":"iso_8601_milliseconds","rotation_interval":1,"rotation_limit":5})~"
    } } ),
    // Add p2p appender to default logger's appenders list.
    config_line_t( {"log-logger", {
      R"~({"name":"default","level":"info","appenders":["stderr","p2p"]})~",
      R"~({"name":"user","level":"debug","appender":"stderr"})~", // checking handling of obsolete form.
      R"~({"name":"p2p","level":"warn","appenders":[]})~" // checking handling of empty list of appenders.
    } } )
  } );

  verify_logging_config_against_pattern( *this, 
    pattern_t( {
      R"~({"name":"stderr","type":"console","args":{"format":"${timestamp} ${context} ${file}:${line} ${method} ${level}]  ${message}","stream":"std_error","level_colors":[{"level":"debug","color":"green"},{"level":"warn","color":"brown"},{"level":"error","color":"red"}],"flush":true,"time_format":"iso_8601_microseconds"},"enabled":true})~",
      R"~({"name":"p2p","type":"file","args":{"format":"${timestamp} ${context} ${file}:${line} ${method} ${level}]  ${message}","filename":"/tmp/hive-tmp/logs/p2p/p2p.log","flush":true,"truncate":true,"rotate":true,"rotation_interval":1000000,"rotation_limit":5000000,"time_format":"iso_8601_milliseconds","delta_times":false},"enabled":true})~"
    } ),
    pattern_t( {
      R"~({"name":"default","level":"info","enabled":true,"additivity":false,"appenders":["stderr","p2p"]})~",
      R"~({"name":"user","level":"debug","enabled":true,"additivity":false,"appenders":["stderr"]})~",
      R"~({"name":"p2p","level":"warn","enabled":true,"additivity":false,"appenders":[]})~"
    } )
  );

  // Use ilog to write into p2p logs as p2p appender is used by default logger.
  for( int i = 0; i<5; ++i )
  {
    // Each "message" should go into separate log file as they are generated
    // in intervals matching rotation_interval.
    sleep(1);
    ilog("i${i}", (i));
  }

  // Now let's check the number and names of log files.
  auto count_log_files = [this](){
    unsigned log_count = 0;
    std::string log_pattern("p2p.log.");
    const size_t log_pattern_length = log_pattern.size();
    fc::path rotation_dir = get_data_dir() / "logs/p2p";
    fc::directory_iterator end_itr; // Default ctor yields past-the-end
    for( fc::directory_iterator i( rotation_dir ); i != end_itr; ++i )
    {
      std::string filename( i->filename().string() );
      // Don't use ilog here to avoid triggering of another file rotation.
      BOOST_TEST_MESSAGE( filename );
      if( log_pattern == filename.substr( 0, log_pattern_length ) )
        ++log_count;
    }
    return log_count;
  };
  // Due to low rotation values, there should be at least five p2p logs,
  // each with a "message" recorded by ilog.
  unsigned log_count = count_log_files();
  BOOST_REQUIRE_GE( log_count, 5 );
  // Due to rotation_limit value, no more than 6 files are expected.
  BOOST_REQUIRE_LE( log_count, 6 );

  // Additionally check that older files are deleted as new are created
  // (and there are no more than 6 at a time).
  sleep(2);
  log_count = count_log_files();
  BOOST_REQUIRE_LE( log_count, 6 );
  
  // Also "current" files should be deleted because they are empty
  // (nobody writes messages to them).
  sleep(5);
  log_count = count_log_files();
  BOOST_REQUIRE_LE( log_count, 2 );

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

#endif
