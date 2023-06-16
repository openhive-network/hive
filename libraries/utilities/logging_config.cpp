#include <hive/utilities/logging_config.hpp>

#include <fc/exception/exception.hpp>

#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#include <vector>

namespace hive { namespace utilities {

using std::string;
using std::vector;

void appender_args::validate()
{
  FC_ASSERT( appender.length(), "Must specify an appender name" );
  FC_ASSERT( ( file.length() > 0 ) ^ ( stream.length() > 0 ), "Must specify either a file or a stream" );
}

void logger_args::validate()
{
  FC_ASSERT( name.length(), "Must specify a logger name" );
  FC_ASSERT( level.length(), "Must specify a logger level" );
  FC_ASSERT( appender.length(), "Must specify an appender name" );
}

void set_logging_program_options( boost::program_options::options_description& options )
{
  std::vector< std::string > default_appender(
    { "{\"appender\":\"stderr\",\"stream\":\"std_error\",\"time_format\":\"iso_8601_microseconds\"}",
      "{\"appender\":\"p2p\",\"file\":\"logs/p2p/p2p.log\",\"time_format\":\"iso_8601_milliseconds\"}" } );
  std::string str_default_appender = boost::algorithm::join( default_appender, " " );

  std::vector< std::string > default_logger(
    { "{\"name\":\"default\",\"level\":\"info\",\"appender\":\"stderr\"}",
      "{\"name\":\"user\",\"level\":\"debug\",\"appender\":\"stderr\"}",
      "{\"name\":\"p2p\",\"level\":\"warn\",\"appender\":\"p2p\"}" } );
  std::string str_default_logger = boost::algorithm::join( default_logger, " " );

  options.add_options()
    ("log-appender", boost::program_options::value< std::vector< std::string > >()->composing()->default_value( default_appender, str_default_appender ),
      "Appender definition JSON. Obligatory attributes:\n"
      "\"appender\" - name of appender\n"
      "\"stream\" - target stream, mutually exclusive with \"file\"\n"
      "\"file\" - target filename (including path), mutually exclusive with \"stream\"\n"
      "Optional attributes:\n"
      "\"time_format\" - see time_format enum values (default: \"iso_8601_seconds\")\n"
      "Optional attributes (applicable to file appender only):\n"
      "\"delta_times\" - whether times should be printed as deltas since previous message (default: false)\n"
      "\"flush\" - whether each log write should end with flush (default: true)\n"
      "\"rotate\" - whether the log files should be rotated (default: true)\n"
      "\"rotation_interval\" - seconds between file rotation (default: 3600)\n"
      "\"rotation_limit\" - seconds before rotated file is removed (default: 86400)"
      )
    ("log-console-appender", boost::program_options::value< std::vector< std::string > >()->composing() )
    ("log-file-appender", boost::program_options::value< std::vector< std::string > >()->composing() )
    ("log-logger", boost::program_options::value< std::vector< std::string > >()->composing()->default_value( default_logger, str_default_logger ),
      "Logger definition json: {\"name\", \"level\", \"appender\"}" )
    ;
}

fc::optional<fc::logging_config> load_logging_config( const boost::program_options::variables_map& args, const boost::filesystem::path& pwd )
{
  try
  {
    fc::logging_config logging_config;
    bool found_logging_config = false;

    std::vector< string > all_appenders;

    if( args.count( "log-appender" ) )
      {
      std::vector< string > appenders = args["log-appender"].as< vector< string > >();
      all_appenders.insert( all_appenders.end(), appenders.begin(), appenders.end() );
      }

    if( args.count( "log-console-appender" ) )
      {
      std::vector< string > console_appenders = args["log-console-appender"].as< vector< string > >();
      all_appenders.insert( all_appenders.end(), console_appenders.begin(), console_appenders.end() );
      }

    if( args.count( "log-file-appender" ) )
    {
      std::vector< string > file_appenders = args["log-file-appender"].as< vector< string > >();
      all_appenders.insert( all_appenders.end(), file_appenders.begin(), file_appenders.end() );
    }

    for( string& s : all_appenders )
    {
      std::size_t pos = 0;
      while ((pos = s.find('{', pos)) != std::string::npos)
      {
        auto appender = fc::json::from_string( s.substr( pos++ ) ).as< appender_args >();
        appender.validate();
        
        if( appender.stream.length() )
        {
          fc::console_appender::config console_appender_config;
          console_appender_config.level_colors.emplace_back(
                                            fc::console_appender::level_color( fc::log_level::debug,
                                                                  fc::console_appender::color::green));
          console_appender_config.level_colors.emplace_back(
                                            fc::console_appender::level_color( fc::log_level::warn,
                                                                  fc::console_appender::color::brown));
          console_appender_config.level_colors.emplace_back(
                                            fc::console_appender::level_color( fc::log_level::error,
                                                                  fc::console_appender::color::red));
          console_appender_config.stream = fc::variant( appender.stream ).as< fc::console_appender::stream::type >();
          if (appender.time_format.length())
            console_appender_config.time_format = fc::variant( appender.time_format ).as<fc::appender::time_format>();
          logging_config.appenders.push_back(
                                  fc::appender_config( appender.appender, "console", fc::variant( console_appender_config ) ) );
          found_logging_config = true;
        }
        else // validate ensures the is either a stream or file configured
        {
          fc::path file_name = appender.file;
          if( file_name.is_relative() )
            file_name = fc::absolute( pwd ) / file_name;
          
          // construct file appender config using default values that may differ
          // from the ones in file_appender::config definition
          fc::file_appender::config file_appender_config;
          file_appender_config.filename = file_name;
          file_appender_config.flush = appender.flush.value_or( true );
          file_appender_config.rotate = appender.rotate.value_or( true );
          file_appender_config.rotation_interval = 
            fc::seconds(
              fc::variant( appender.rotation_interval.value_or( 3600ll /*1 hour*/ ) ).as<int64_t>()
            );
          file_appender_config.rotation_limit = 
            fc::seconds(
              fc::variant( appender.rotation_limit.value_or( 86400ll /*1 day*/ ) ).as<int64_t>()
            );
          file_appender_config.time_format = appender.time_format.length() ?
            fc::variant( appender.time_format ).as<fc::appender::time_format>() :
            fc::appender::time_format::iso_8601_seconds;
          file_appender_config.delta_times = appender.delta_times.value_or(false);
          logging_config.appenders.push_back(
                                  fc::appender_config( appender.appender, "file", fc::variant( file_appender_config ) ) );
          found_logging_config = true;
        }
      }
    }

    if( args.count( "log-logger" ) )
    {
      std::vector< string > loggers = args[ "log-logger" ].as< std::vector< std::string > >();

      for( string& s : loggers )
      {
        std::size_t pos = 0;
        while ((pos = s.find('{', pos)) != std::string::npos)
        {
          auto logger = fc::json::from_string( s.substr( pos++ ) ).as< logger_args >();

          fc::logger_config logger_config( logger.name );
          logger_config.level = fc::variant( logger.level ).as< fc::log_level >();
          boost::split( logger_config.appenders, logger.appender,
                    boost::is_any_of(" ,"),
                    boost::token_compress_on );
          logging_config.loggers.push_back( logger_config );
          found_logging_config = true;
        }
      }
    }

    if( found_logging_config )
      return logging_config;
    else
      return fc::optional< fc::logging_config >();
  }
  FC_RETHROW_EXCEPTIONS(warn, "")
}

} }
