#pragma once

#include <fc/log/logger.hpp>

#include <vector>
#include <atomic>
#include <future>

namespace hive {

  enum class hive_p2p_handler_type { HIVE_P2P_BLOCK_HANDLER, HIVE_P2P_TRANSACTION_HANDLER };

  class shutdown_state
  {
    std::promise<void>        promise;
    std::shared_future<void>  future;
    std::atomic_uint32_t      activity;
    std::string               name;

    public:

      using ptr_shutdown_state = std::shared_ptr< shutdown_state >;

      shutdown_state( const std::string& name ) : name( name )
      {
        future = std::shared_future<void>( promise.get_future() );
        activity.store( 0 );
      }

      void change_activity( int32_t val )
      {
        FC_ASSERT( activity.load() + val >= 0 );
        activity.store( activity.load() + val );
      }

      uint32_t get_activity() const
      {
        return activity.load();
      }

      bool is_future_valid() const
      {
        return future.valid();
      }

      std::future_status future_wait_for( uint32_t milliseconds ) const
      {
        return future.wait_for( std::chrono::milliseconds( milliseconds ) );
      }

      void set_promise()
      {
        promise.set_value();
      }

      const std::string& get_name() const
      {
        return name;
      }
  };

  class shutdown_mgr
  {
    private:

      std::string name;

      std::atomic_bool running;

      std::map< hive_p2p_handler_type, shutdown_state::ptr_shutdown_state > states;

      const char* fStatus(std::future_status s)
      {
        switch(s)
        {
          case std::future_status::ready:
          return "ready";
          case std::future_status::deferred:
          return "deferred";
          case std::future_status::timeout:
          return "timeout";
          default:
          return "unknown";
        }
      }

      void wait( const shutdown_state::ptr_shutdown_state& state )
      {
        FC_ASSERT( state, "State doesn't exist" );
        FC_ASSERT( !is_running(), "Lack of shutdown" );

        std::future_status res;
        uint32_t cnt = 0;
        uint32_t time_maximum = 300;//30 seconds

        ilog("Processing of '${name}' in progress...", ("name", state->get_name() ) );

        do
        {
          if( state->get_activity() != 0 )
          {
            res = state->future_wait_for( 100 );
            if( res != std::future_status::ready )
            {
              ilog( "attempt: ${attempt}/${total}, reason: ${s}, future status(internal): ${fs} ...", ("attempt", cnt + 1)("total", time_maximum)("s", fStatus( res ) )("fs", std::to_string( state->is_future_valid() ) ) );
              ilog( "Details: Wait for: ${name}. Currently ${number} of '${name}' items are processed...\n", ("name", state->get_name())("number", state->get_activity())("name", state->get_name()) );
            }
            FC_ASSERT( ++cnt <= time_maximum, "Closing the ${name} is terminated", ( "name", name ) );
          }
          else
          {
            res = std::future_status::ready;
            ilog("A value from a different thread is read..." );
          }
        }
        while( res != std::future_status::ready );

        ilog("Processing of '${name}' was finished...", ("name", state->get_name() ) );
      }

    public:

      shutdown_mgr( std::string _name )
        : name( _name ), running( true )
      {
        std::map< hive_p2p_handler_type, std::string > input = {
                    { hive_p2p_handler_type::HIVE_P2P_BLOCK_HANDLER, "P2P_BLOCK" },
                    { hive_p2p_handler_type::HIVE_P2P_TRANSACTION_HANDLER, "P2P_TRANSACTION" } };

        for( auto& item : input )
          states[ item.first ] = std::make_shared<shutdown_state>( item.second );
      }

      void shutdown()
      {
        running.store( false );

        for( auto& state : states )
          wait( state.second );
      }

      bool is_running() const
      {
        return running.load() && !appbase::app().is_interrupt_request();
      }

      shutdown_state& get_state( hive_p2p_handler_type handler )
      {
        auto _found = states.find( handler );
        FC_ASSERT( _found != states.end(), "Incorrect index - lack of correct state" );

        shutdown_state* _state = ( _found->second ).get();
        FC_ASSERT( _state, "State has NULL value" );

        return *_state;
      }
  };

  class action_catcher
  {
    private:
    
      shutdown_mgr& mgr;
      shutdown_state& state;

    public:

      action_catcher( shutdown_mgr& mgr, hive_p2p_handler_type handler ):
        mgr( mgr ), state( mgr.get_state( handler ) )
      {
        state.change_activity( 1 );
      }

      ~action_catcher()
      {
        state.change_activity( -1 );

        if( !mgr.is_running() && !state.is_future_valid() )
        {
          ilog("Sending notification to shutdown barrier.");

          try
          {
            state.set_promise();
          }
          catch( const std::future_error& e )
          {
            ilog("action_catcher: future error exception. ( Code: ${c} )( Message: ${m} )", ( "c", e.code().value() )( "m", e.what() ) );
          }
          catch(...)
          {
            ilog("action_catcher: unknown error exception." );
          }
        }
      }

  };

}
