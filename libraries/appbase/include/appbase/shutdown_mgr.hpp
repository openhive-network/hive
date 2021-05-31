#pragma once

#include <fc/log/logger.hpp>

#include <vector>
#include <atomic>
#include <future>

namespace hive {

  struct shutdown_state
  {
    using ptr_shutdown_state = std::shared_ptr< shutdown_state >;

    std::promise<void>        promise;
    std::shared_future<void>  future;
    std::atomic_uint          activity;
    std::string               name;

    shutdown_state( const std::string& _name ) : name( _name ){}
  };

  class shutdown_mgr
  {
    private:

      std::string name;

      std::atomic_bool running;

      std::vector< shutdown_state::ptr_shutdown_state > states;

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

      void wait( const shutdown_state& state )
      {
        FC_ASSERT( !get_running().load(), "Lack of shutdown" );

        std::future_status res;
        uint32_t cnt = 0;
        uint32_t time_maximum = 300;//30 seconds

        ilog("Processing of '${name}' in progress...", ("name", state.name ) );

        do
        {
          if( state.activity.load() != 0 )
          {
            res = state.future.wait_for( std::chrono::milliseconds(100) );
            if( res != std::future_status::ready )
            {
              ilog("finishing: ${s}, future status: ${fs}, attempt: ${attempt} ...", ("s", fStatus( res ) )("fs", std::to_string( state.future.valid() ) )("attempt", cnt + 1) );
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

        ilog("Processing of '${name}' was finished...", ("name", state.name ) );
      }

    public:

      shutdown_mgr( std::string _name, size_t _nr_actions, const std::vector< std::string >& names )
        : name( _name ), running( true )
      {
        FC_ASSERT( _nr_actions == names.size() );

        for( size_t i = 0; i < _nr_actions; ++i )
        {
          shutdown_state::ptr_shutdown_state _state( new shutdown_state( "shutdown-state type: " + names[i] ) );
          _state->future = std::shared_future<void>( _state->promise.get_future() );
          _state->activity.store( 0 );

          states.emplace_back( _state );
        }
      }

      void shutdown()
      {
        running.store( false );

        for( auto& state : states )
        {
          shutdown_state* _state = state.get();
          FC_ASSERT( _state, "State has NULL value" );
          wait( *_state );
        }
      }

      const std::atomic_bool& get_running() const
      {
        return running;
      }

      shutdown_state& get_state( size_t idx )
      {
        FC_ASSERT( idx < states.size(), "Incorrect index - lack of correct state" );

        shutdown_state* _state = states[idx].get();
        FC_ASSERT( _state, "State has NULL value" );

        return *_state;
      }
  };

  class action_catcher
  {
    private:
    
      const std::atomic_bool& running;
      shutdown_state&         state;

    public:

      action_catcher( const std::atomic_bool& _running, shutdown_state& _state ):
        running( _running ), state( _state )
      {
        state.activity.store( state.activity.load() + 1 );
      }

      ~action_catcher()
      {
        state.activity.store( state.activity.load() - 1 );

        if( running.load() == false && state.future.valid() == false )
        {
          ilog("Sending notification to shutdown barrier.");

          try
          {
            state.promise.set_value();
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
