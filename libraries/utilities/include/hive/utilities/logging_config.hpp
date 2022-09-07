#pragma once

#include <fc/log/console_appender.hpp>
#include <fc/log/file_appender.hpp>
#include <fc/log/logger.hpp>
#include <fc/log/logger_config.hpp>
#include <fc/reflect/reflect.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/io/json.hpp>

#include <boost/program_options.hpp>

namespace hive { namespace utilities {

struct appender_args
{
  std::string appender;
  std::string file;
  std::string stream;
  std::string time_format;
  fc::optional<bool> delta_times;

  void validate();
};

struct logger_args
{
  std::string name;
  std::string level;
  std::string appender;

  void validate();
};

void set_logging_program_options( boost::program_options::options_description& options );

fc::optional<fc::logging_config> load_logging_config( const boost::program_options::variables_map& args, const boost::filesystem::path& pwd );

} } // hive::utilities

FC_REFLECT( hive::utilities::appender_args, (appender)(file)(stream)(time_format)(delta_times) )
FC_REFLECT( hive::utilities::logger_args, (name)(level)(appender) )
