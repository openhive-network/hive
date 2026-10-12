#include <hive/plugins/network_broadcast_api/network_broadcast_api.hpp>
#include <hive/plugins/network_broadcast_api/network_broadcast_api_plugin.hpp>

#include <hive/chain/notifications.hpp>

#include <appbase/application.hpp>

#include <boost/scope_exit.hpp>
#include <boost/thread/future.hpp>
#include <boost/thread/lock_guard.hpp>

namespace hive { namespace plugins { namespace network_broadcast_api {

namespace detail
{
  class network_broadcast_api_impl
  {
    public:
      network_broadcast_api_impl( appbase::application& app ) :
        _p2p( &app.get_plugin< hive::plugins::p2p::p2p_plugin >() ),
        _chain( app.get_plugin< hive::plugins::chain::chain_plugin >() )
      {
        _on_post_apply_block_conn = _chain.db().add_post_apply_block_handler(
          [&]( const hive::chain::block_notification& note ){ on_post_apply_block( note ); },
          app.get_plugin< network_broadcast_api_plugin >(), 0 );
      }

      DECLARE_API_IMPL(
        (broadcast_transaction)
      )

      bool check_max_block_age( int32_t max_block_age ) const;

      broadcast_transaction_synchronous_return broadcast_transaction_synchronous( const signed_transaction& trx );

      hive::plugins::p2p::p2p_plugin*                      _p2p = nullptr;
      hive::plugins::chain::chain_plugin&                  _chain;

    private:
      typedef boost::promise< broadcast_transaction_synchronous_return > result_promise;

      void on_post_apply_block( const hive::chain::block_notification& note );

      // Promises of the broadcast_transaction_synchronous calls that wait for their transactions. A call removes
      // its entry under the mutex before it returns, so on_post_apply_block never fulfils a promise that is gone.
      std::map< transaction_id_type, result_promise* >                   _waiting_calls;
      std::map< fc::time_point_sec, std::vector< transaction_id_type > > _waiting_call_expirations;
      boost::mutex                                                       _waiting_calls_mtx;
      hive::chain::database::signal_connection_ptr                       _on_post_apply_block_conn;
  };

  DEFINE_API_IMPL( network_broadcast_api_impl, broadcast_transaction )
  {
    FC_ASSERT( !check_max_block_age( args.max_block_age ) );

    hive::chain::full_transaction_ptr full_transaction;
    _chain.determine_encoding_and_accept_transaction( full_transaction, args.trx );
    if( _p2p ) // can be used to feed solo test node
      _p2p->broadcast_transaction( full_transaction );

    return broadcast_transaction_return();
  }

  bool network_broadcast_api_impl::check_max_block_age( int32_t max_block_age ) const
  {
    if( max_block_age < 0 )
      return false;

    return _chain.db().with_read_lock( [&]()
    {
      fc::time_point_sec now = fc::time_point::now();
      const auto& dgpo = _chain.db().get_dynamic_global_properties();

      return ( dgpo.get_head_block_time() < now - fc::seconds( max_block_age ) );
    }, fc::seconds(1));
  }

  broadcast_transaction_synchronous_return network_broadcast_api_impl::broadcast_transaction_synchronous( const signed_transaction& trx )
  {
    fc::time_point api_start_time = fc::time_point::now();
    fc::time_point callback_setup_time;
    result_promise p;
    auto f = p.get_future();
    hive::chain::full_transaction_ptr full_transaction;
    optional< transaction_id_type > waiting_id; // set while `p` is in `_waiting_calls`

    auto stop_waiting = [&]()
    {
      if( !waiting_id )
        return;

      boost::lock_guard< boost::mutex > guard( _waiting_calls_mtx );
      auto itr = _waiting_calls.find( *waiting_id );
      // on_post_apply_block erases the entry when it fulfils `p`, and another call may have added the id since
      if( itr != _waiting_calls.end() && itr->second == &p )
        _waiting_calls.erase( itr );
      // We do not need to clean up _waiting_call_expirations because on_post_apply_block handles this case.
      waiting_id.reset();
    };
    BOOST_SCOPE_EXIT( &stop_waiting ) { stop_waiting(); } BOOST_SCOPE_EXIT_END

    auto start_waiting = [&]( bool hf26_auth_fail )
    {
      // the transaction is retried in legacy serialization, which gives it a different id
      if( hf26_auth_fail )
      {
        stop_waiting();
        return;
      }

      const auto& txid = full_transaction->get_transaction_id();
      boost::lock_guard< boost::mutex > guard( _waiting_calls_mtx );
      FC_ASSERT( _waiting_calls.find( txid ) == _waiting_calls.end(), "Transaction is a duplicate" );
      _waiting_calls[ txid ] = &p;
      _waiting_call_expirations[ trx.expiration ].push_back( txid );
      waiting_id = txid;
      LOG_DELAY( api_start_time, fc::seconds( 1 ), "Excessive delay to setup callback" );
      if( callback_setup_time == fc::time_point() )
        callback_setup_time = fc::time_point::now();
    };

    try
    {
      /* start_waiting takes the lock only for its own duration, and the lock is not held while the transaction is
       * accepted. accept_transaction is queued along with all other writes, including pushing blocks, and the write
       * thread takes the lock in on_post_apply_block. Holding it here while waiting for the write thread to accept
       * the transaction would deadlock.
       */
      _chain.determine_encoding_and_accept_transaction( full_transaction, trx, start_waiting );
      if( _p2p ) // can be used to feed solo test node
        _p2p->broadcast_transaction( full_transaction );
    }
    catch( fc::exception& e )
    {
      if( callback_setup_time != fc::time_point() )
        LOG_DELAY_EX( callback_setup_time, fc::seconds( 1 ), "Excessive delay to validate & broadcast trx ${e}", (e) );
      throw;
    }
    catch( ... )
    {
      ulog( "unknown exception thrown while processing trx: ${trx}", (trx) );
      throw fc::unhandled_exception( FC_LOG_MESSAGE( warn, "Unknown error occured when pushing transaction" ), std::current_exception() );
    }

    LOG_DELAY( callback_setup_time, fc::seconds( 1 ), "Excessive delay to validate & broadcast trx" );

    /* Once write processing has finished (shutdown, or --stop-at-block), no block will be applied, and on shutdown
     * the webserver waits for this call before the node can close. The flag is set only after the block being
     * applied, if any, has run its post-apply notifications, so such a block still reports the transaction.
     */
    while( f.wait_for( boost::chrono::milliseconds( 100 ) ) != boost::future_status::ready )
    {
      if( _chain.is_finished_write_processing() )
      {
        stop_waiting();
        if( f.is_ready() ) // reported just before stop_waiting() took the lock
          break;
        FC_THROW_EXCEPTION( fc::canceled_exception,
          "Node stopped applying blocks before transaction ${id} was included in a block or expired. "
          "It has been accepted, so a block may still include it.",
          ( "id", full_transaction->get_transaction_id() ) );
      }
    }

    return f.get();
  }

  void network_broadcast_api_impl::on_post_apply_block( const hive::chain::block_notification& note )
  { try {
    boost::lock_guard< boost::mutex > guard( _waiting_calls_mtx );
    int32_t block_num = int32_t( note.block_num );
    if( _waiting_calls.size() )
    {
      int32_t trx_num = 0;
      for( const auto& trx : note.full_block->get_full_transactions() )
      {
        const auto& id = trx->get_transaction_id();
        auto itr = _waiting_calls.find( id );
        if( itr != _waiting_calls.end() )
        {
          itr->second->set_value( { id, block_num, trx_num, trx->get_rc_cost(), false } );
          _waiting_calls.erase( itr );
        }
        ++trx_num;
      }
    }

    /// clear all expirations
    auto block_ts = note.get_block_timestamp();
    while( true )
    {
      auto exp_it = _waiting_call_expirations.begin();
      if( exp_it == _waiting_call_expirations.end() )
        break;
      if( exp_it->first >= block_ts )
        break;
      for( const transaction_id_type& txid : exp_it->second )
      {
        auto itr = _waiting_calls.find( txid );
        // Not found when the transaction has been confirmed by the above check, or its call stopped waiting.
        if( itr == _waiting_calls.end() )
          continue;

        itr->second->set_value( { txid, block_num, -1, optional< int64_t >(), true } );
        _waiting_calls.erase( itr );
      }
      _waiting_call_expirations.erase( exp_it );
    }
  } FC_LOG_AND_RETHROW() }

} // detail

network_broadcast_api::network_broadcast_api( appbase::application& app ) : my( new detail::network_broadcast_api_impl( app ) )
{
  JSON_RPC_REGISTER_API( HIVE_NETWORK_BROADCAST_API_PLUGIN_NAME );
}

network_broadcast_api::~network_broadcast_api() {}

void network_broadcast_api::check_p2p()
{
  if( not my->_chain.is_p2p_enabled() )
    my->_p2p = nullptr;
  else
    FC_ASSERT( my->_p2p != nullptr ); // constructor should fill it properly
}

broadcast_transaction_synchronous_return network_broadcast_api::broadcast_transaction_synchronous( const signed_transaction& trx )
{
  return my->broadcast_transaction_synchronous( trx );
}

DEFINE_LOCKLESS_APIS( network_broadcast_api,
  (broadcast_transaction)
)

} } } // hive::plugins::network_broadcast_api
