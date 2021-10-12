#pragma once
#include <fc/io/stdio.hpp>
#include <fc/io/json.hpp>
#include <fc/io/buffered_iostream.hpp>
#include <fc/io/sstream.hpp>
#include <fc/rpc/api_connection.hpp>
#include <fc/thread/thread.hpp>

#include <iostream>
#include <thread>
#include <functional>
#include <atomic>

namespace fc { namespace rpc {

   /**
    *  Provides a simple wrapper for RPC calls to a given interface.
    */
   class cli : public api_connection
   {
      public:
         typedef std::function< void(int) > on_termination_handler;

         cli();
         ~cli();

         virtual variant send_call( api_id_type api_id, string method_name, variants args = variants() );
         virtual variant send_call( string api_name, string method_name, variants args = variants() );
         virtual variant send_callback( uint64_t callback_id, variants args = variants() );
         virtual void    send_notice( uint64_t callback_id, variants args = variants() );

         void start();
         void stop();
         void wait();
         void format_result( const string& method, std::function<string(variant,const variants&)> formatter);
         void set_on_termination_handler( on_termination_handler&& hdl );

         virtual void getline( const fc::string& prompt, fc::string& line );

         void set_prompt( const string& prompt );

      private:
         void run();

         std::map<string,std::function<string(variant,const variants&)> > _result_formatters;

         std::string            _prompt = ">>>";
         std::thread            _run_thread;
         std::atomic< bool >    _run_complete;
         on_termination_handler _termination_hdl;
   };
} }
