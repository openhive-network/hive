#pragma once

#include<memory>

#include <boost/asio.hpp>

namespace appbase {

  class signals_handler
  {
    public:

      using p_signal_set = std::shared_ptr< boost::asio::signal_set >;

      using empty_function_type               = std::function< void() >;

      using interrupt_request_generation_type = empty_function_type;

    private:

      bool                              closed              = false;
      uint32_t                          last_signal_number  = 0;

      interrupt_request_generation_type interrupt_request_generation;

      p_signal_set                      signals;

      boost::asio::io_service           io_serv;

      void handle_signal( const boost::system::error_code& err, int signal_number );

      void attach_signals();

    public:

      signals_handler( interrupt_request_generation_type&& _interrupt_request_generation );
      ~signals_handler();

      void init( std::promise<void> after_attach_signals );
      void generate_interrupt_request();

      void clear_signals();

      boost::asio::io_service& get_io_service();
  };

  class signals_handler_wrapper
  {
    private:


      bool initialized    = false;
      bool thread_closed  = false;

      std::unique_ptr<std::thread>  handler_thread;
      signals_handler               handler;

      std::promise<void>            after_attach_signals_promise;

      void block_signals();

    public:

      signals_handler_wrapper( signals_handler::interrupt_request_generation_type&& _interrupt_request_generation );

      void init();
      void wait4stop( bool log = false );
      bool is_thread_closed();

      boost::asio::io_service& get_io_service();
  };
}