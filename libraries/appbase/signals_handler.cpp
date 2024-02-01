#include <appbase/signals_handler.hpp>

#include <fc/log/logger.hpp>

#include <boost/bind/bind.hpp>

namespace appbase {

signals_handler::signals_handler( interrupt_request_generation_type&& _interrupt_request_generation, final_action_type&& _final_action )
      : interrupt_request_generation( _interrupt_request_generation ), final_action( _final_action )
{
}

signals_handler::~signals_handler()
{
  /*
    The best moment for clearing signals is time of destroying an object then for sure we don't need signals.
    Otherwise can be a situation that signals were removed, but an application gets another SIGINT and that signal won't be able to be process correctly.
  */
  clear_signals();
}

void signals_handler::init( std::promise<void> after_attach_signals )
{
  attach_signals();

  after_attach_signals.set_value();

  io_serv.run();
}

boost::asio::io_service& signals_handler::get_io_service()
{
  return io_serv;
}

void signals_handler::close()
{
  /** Since signals can be processed asynchronously, it would be possible to call this twice
  *    concurrently while op service is stopping.
  */
  std::lock_guard<std::mutex> guard( close_mtx );

  if( !closed )
  {
    interrupt_request_generation();

    final_action();

    io_serv.stop();

    closed = true;
  }
}

void signals_handler::clear_signals()
{
  if( !signals )
    return;

  boost::system::error_code ec;

  signals->cancel( ec );

  signals.reset();

  if( ec.value() != 0 )
  {
    ilog("Error during cancelling signal: " + ec.message());
  }
}

void signals_handler::handle_signal( const boost::system::error_code& err, int signal_number )
{
  /// Handle signal only if it was really present (it is possible to get error_code for 'Operation cancelled' together with signal_number == 0)
  if( signal_number == 0 )
    return;

  static uint32_t _handling_signals_counter = 0;

  signals->async_wait( boost::bind( &signals_handler::handle_signal, this, boost::placeholders::_1, boost::placeholders::_2 ) );

  ++_handling_signals_counter;

  last_signal_number = signal_number;

  if( last_signal_number == SIGINT || last_signal_number == SIGTERM )
  {
    close();
  }
}

void signals_handler::attach_signals()
{
  /** To avoid killing process by broken pipe and continue regular app shutdown.
    *  Useful for usecase: `hived | tee hived.ilog` and pressing Ctrl+C
    **/
  signal(SIGPIPE, SIG_IGN);

  signals = p_signal_set( new boost::asio::signal_set( io_serv, SIGINT, SIGTERM ) );

  signals->async_wait( boost::bind( &signals_handler::handle_signal, this, boost::placeholders::_1, boost::placeholders::_2 ) );
}

signals_handler_wrapper::signals_handler_wrapper( signals_handler::interrupt_request_generation_type&& _interrupt_request_generation, signals_handler::final_action_type&& _final_action )
                        : handler( std::move( _interrupt_request_generation ), std::move( _final_action ) )
{

}

void signals_handler_wrapper::init()
{
  std::future<void> after_attach_signals_future = after_attach_signals_promise.get_future();

  handler_thread = std::make_unique<std::thread>(
    [this]()
    {
      handler.init( std::move( after_attach_signals_promise ) );
    }
  );

  after_attach_signals_future.get();

  block_signals();

  initialized = true;
}

void signals_handler_wrapper::wait()
{
  if( initialized )
  {
    if( handler_thread->joinable() )
      handler_thread->join();
  }
  thread_closed = true;
}

void signals_handler_wrapper::force_stop()
{
  /*
    when application gets SIGINT, it won't be processed correctly, so it is only viable
    for unit tests, where signals are captured by boost anyway;
  */
  handler.close();
}

bool signals_handler_wrapper::is_thread_closed()
{
  return thread_closed;
}

void signals_handler_wrapper::block_signals()
{
  sigset_t blockset;

  sigemptyset(&blockset);
  sigaddset(&blockset, SIGINT);
  sigaddset(&blockset, SIGTERM);
  sigprocmask(SIG_BLOCK, &blockset, NULL);
}

boost::asio::io_service& signals_handler_wrapper::get_io_service()
{
  return handler.get_io_service();
}

}