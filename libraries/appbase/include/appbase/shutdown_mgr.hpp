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

      appbase::application& theApp;
      
      plugins::chain::chain_plugin& chain;

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
            FC_ASSERT( ++cnt < time_maximum, "Closing the ${name} is terminated", ( "name", name ) );
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

      shutdown_mgr( std::string _name, appbase::application& app, plugins::chain::chain_plugin& chain )
        : name( _name ), running( true ), theApp( app ), chain( chain )
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

        // this comment doesn't really fit well anywhere, but it needs to be somewhere.
        // note: it seems that the original intent of the shutdown manager was primarily to 
        // introduce a pause in the shutdown process, which would allow the write_processor_thread
        // to process any remaining work, before continuing shutdown.
        //
        // The write_processor_thread (in chain_plugin.cpp) is responsible for taking blocks & 
        // transactions from the queue and pushing them to the chain database for processing,
        // and then notifying the p2p code whether the block/transaction was accepted or not.
        // If this doesn't happen, the write_processor_thread could notify the p2p plugin that
        // it finished processing a block/transaction while the p2p plugin is in the process of
        // being shutdown.  If the p2p plugin code tries to do something with that notification
        // when its data structures are being destroyed, crashes are likely.
        //
        // The intent here was to prevent the write queue from accepting any new work, and 
        // process any existing work, before allowing the p2p plugin to begin shutdown in 
        // earnest.
        //
        // That said, at the moment, this doesn't work very well.  As soon as a shutdown
        // request is received (SIGINT), the write_processor_thread exits.  Anything that was
        // on the write queue at the time will stay on the write queue forever, and never
        // have a chance to be pushed to the database.  If there are 10 blocks on the queue,
        // you could wait forever and they will never be processed.
        // It's not entirely useless though.  If the write_processor_thread is actively
        // working on a single block (meaning that block has been removed from the queue
        // and the queue is now empty, and the thread is now in the middle of a 
        // push_block()/push_transaction() call), this code should successfully wait for
        // that block or transaction to be processed before it allows the p2p plugin to 
        // continue shutdown.
        for( auto& state : states )
          wait( state.second );
      }

      bool is_running() const
      {
        /*
          Explanation of `is_finished_write_processing`.

          Sometimes stopping of the working thread is triggered by SIGINT, but sometimes by CLI parameteres like `stop-at-block`.
          We have to have certanity that the working thread is closed regardless of a reason,
          so the best option is to wait for SIGINT and a real finish of the working thread.
        */
        return running.load() && !theApp.is_interrupt_request() && !chain.is_finished_write_processing();
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
