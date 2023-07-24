#include <fc/log/logger_config.hpp>
#include <fc/log/log_message.hpp>

#include <fc/log/appender.hpp>
#include <fc/log/console_appender.hpp>

namespace fc {

bool configure_logging()
{
  static bool reg_console_appender = appender::register_appender<console_appender>( "console" );

  return reg_console_appender;
}

const char* log_context::get_current_task_desc()const
{
  return nullptr;
}

} // namespace fc
