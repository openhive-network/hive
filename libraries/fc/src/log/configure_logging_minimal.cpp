#include <fc/log/logger_config.hpp>
#include <fc/log/appender.hpp>

#include <fc/log/console_appender.hpp>

namespace fc {

bool configure_logging()
{
  static bool reg_console_appender = appender::register_appender<console_appender>( "console" );

  return reg_console_appender;
}

} // namespace fc
