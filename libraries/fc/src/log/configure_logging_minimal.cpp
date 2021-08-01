#include <fc/log/logger_config.hpp>
#include <fc/log/log_message.hpp>
#include <fc/log/console_appender.hpp>

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

const std::string& log_context::get_current_thread_name()const
{
  static std::string unknown{"?unknown?"};
  return unknown;
}

void console_appender::log_impl(const std::string& line, color::type color)
{
  print( line, color );
  print_new_line();
}

} // namespace fc
