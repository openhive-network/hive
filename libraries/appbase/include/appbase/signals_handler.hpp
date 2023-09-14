#pragma once

#include<memory>

#include <boost/asio.hpp>

namespace appbase {

  class signals_handler
  {
    public:

      using p_signal_set = std::shared_ptr< boost::asio::signal_set >;

      using empty_function_type               = std::function< void() >;

      using final_action_type                 = empty_function_type;
      using interrupt_request_generation_type = empty_function_type;

    private:

      bool                              closed              = false;
      uint32_t                          last_signal_number  = 0;

      interrupt_request_generation_type interrupt_request_generation;
      final_action_type                 final_action;

      p_signal_set                      signals;

      std::mutex                        close_mtx;

      boost::asio::io_service           io_serv;

      void handle_signal( const boost::system::error_code& err, int signal_number );
      void clear_signals();

      void attach_signals();
      void close();

    public:

      signals_handler( interrupt_request_generation_type&& _interrupt_request_generation, final_action_type&& _final_action );
      ~signals_handler();

      void init( std::promise<void> after_attach_signals );

      boost::asio::io_service& get_io_service();
  };

  class signals_handler_wrapper
  {
    private:

      std::unique_ptr<std::thread>  handler_thread;
      signals_handler               handler;

      std::promise<void>            after_attach_signals_promise;

      void block_signals();

    public:

      signals_handler_wrapper( signals_handler::interrupt_request_generation_type&& _interrupt_request_generation, signals_handler::final_action_type&& _final_action );

      void init();
      void wait();

      boost::asio::io_service& get_io_service();
  };
}