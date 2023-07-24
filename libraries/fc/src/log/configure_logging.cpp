#include <fc/log/logger_config.hpp>
#include <fc/log/appender.hpp>

#include <fc/log/console_appender.hpp>
#include <fc/log/file_appender.hpp>

namespace fc {

bool configure_logging()
{
  static bool reg_console_appender = appender::register_appender<console_appender>( "console" );
  static bool reg_file_appender = appender::register_appender<file_appender>( "file" );

  return reg_console_appender || reg_file_appender;
}

} // namespace fc
