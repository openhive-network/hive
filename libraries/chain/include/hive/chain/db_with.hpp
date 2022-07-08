#pragma once

#include <hive/chain/database.hpp>
#include <boost/scope_exit.hpp>

/*
  * This file provides with() functions which modify the database
  * temporarily, then restore it.  These functions are mostly internal
  * implementation detail of the database.
  *
  * Essentially, we want to be able to use "finally" to restore the
  * database regardless of whether an exception is thrown or not, but there
  * is no "finally" in C++.  Instead, C++ requires us to create a struct
  * and put the finally block in a destructor.  Aagh!
  */

namespace hive { namespace chain { namespace detail {
/**
  * Class used to help the with_skip_flags implementation.
  * It must be defined in this header because it must be
  * available to the with_skip_flags implementation,
  * which is a template and therefore must also be defined
  * in this header.
  */
struct skip_flags_restorer
{
  skip_flags_restorer( node_property_object& npo, uint32_t old_skip_flags )
    : _npo( npo ), _old_skip_flags( old_skip_flags )
  {}

  ~skip_flags_restorer()
  {
    _npo.skip_flags = _old_skip_flags;
  }

  node_property_object& _npo;
  uint32_t _old_skip_flags;      // initialized in ctor
};

/**
  * Set the skip_flags to the given value, call callback,
  * then reset skip_flags to their previous value after
  * callback is done.
  */
template< typename Lambda >
void with_skip_flags(
  database& db,
  uint32_t skip_flags,
  Lambda callback )
{
  node_property_object& npo = db.node_properties();
  skip_flags_restorer restorer( npo, npo.skip_flags );
  npo.skip_flags = skip_flags;
  callback();
  return;
}

/**
  * Class used to help the without_pending_transactions
  * implementation.
  *
  * TODO:  Change the name of this class to better reflect the fact
  * that it restores popped transactions as well as pending transactions.
  */
struct pending_transactions_restorer
{
  pending_transactions_restorer( database& db, std::vector<std::shared_ptr<full_transaction_type>>&& pending_transactions )
    : _db(db), _pending_transactions( std::move(pending_transactions) )
  {
    _db.clear_pending();
  }

  ~pending_transactions_restorer()
  {
    auto head_block_time = _db.head_block_time();
    _db._pending_tx.reserve( _db._popped_tx.size() + _pending_transactions.size() );

#if !defined(IS_TEST_NET) || defined NDEBUG //especially during debugging that limit is highly problematic
    auto start = fc::time_point::now();
#endif
    bool apply_trxs = true;
    uint32_t applied_txs = 0;
    uint32_t postponed_txs = 0;
    uint32_t expired_txs = 0;

    auto handle_tx = [&](const std::shared_ptr<full_transaction_type>& full_transaction)
    {
#if !defined IS_TEST_NET || defined NDEBUG //during debugging that limit is highly problematic
      if( apply_trxs && fc::time_point::now() - start > HIVE_PENDING_TRANSACTION_EXECUTION_LIMIT )
        apply_trxs = false;
#endif

      if( apply_trxs )
      {
        const signed_transaction& tx = full_transaction->get_transaction();
        try
        {
          if( tx.expiration < head_block_time )
          {
            ++expired_txs;
          }
          else if (!_db.is_known_transaction(full_transaction->get_transaction_id()))
          {
            // since push_transaction() takes a signed_transaction,
            // the operation_results field will be ignored.
            _db._push_transaction(full_transaction);
            ++applied_txs;
          }
        }
        catch( const transaction_exception& e )
        {
          dlog("Pending transaction became invalid after switching to block ${b} ${n} ${t}",
               ("b", _db.head_block_id())("n", _db.head_block_num())("t", _db.head_block_time()));
          dlog("The invalid transaction caused exception ${e}", ("e", e.to_detail_string()));
          dlog("${tx}", (tx));
        }
        catch( const fc::exception& e )
        {
          /*
          dlog( "Pending transaction became invalid after switching to block ${b} ${n} ${t}",
            ("b", _db.head_block_id())("n", _db.head_block_num())("t", _db.head_block_time()) );
          dlog( "The invalid pending transaction caused exception ${e}", ("e", e.to_detail_string() ) );
          dlog( "${t}", ("t", tx) );
          */
        }
      }
      else
      {
        _db._pending_tx.emplace_back(full_transaction);
        ++postponed_txs;
      }
    };

    uint32_t skip = _db.node_properties().skip_flags
      | database::skip_validate | database::skip_transaction_signatures;
      //1. operations once validated cannot become invalid;
      //2. signatures might become invalid if transaction that changes keys happens to arrive in some
      //block in different order than when pending transaction became pending, however for that super
      //rare case there is no point in wasting time verifying authority all the time - it will be
      //verified by witness during block production;
      //3. there is no point in skipping tapos check - it is cheap and might let us drop transactions
      //from pending in case of fork (maybe we should not skip it only when _popped_tx is not empty?);
      //4. while we have duplication check pulled outside, skip_transaction_dupe_check flag also
      //governs creation of transaction_objects - those contain packed version of transaction that is
      //used by p2p, so we can't skip that

    with_skip_flags( _db, skip, [&]()
    {
      BOOST_SCOPE_EXIT( &_db ) { _db.clear_tx_status(); } BOOST_SCOPE_EXIT_END
      _db.set_tx_status( database::TX_STATUS_PENDING );

      for( auto& tx : _db._popped_tx )
        handle_tx( tx );
      _db._popped_tx.clear();

      for (auto& tx : _pending_transactions)
        handle_tx(tx);
    } );

    if (postponed_txs || expired_txs)
      wlog("Postponed ${postponed_txs} pending transactions. ${applied_txs} were applied. ${expired_txs} expired.",
           (postponed_txs)(applied_txs)(expired_txs));
  }

  database& _db;
  std::vector<std::shared_ptr<full_transaction_type>> _pending_transactions;
};

/**
  * Empty pending_transactions, call callback,
  * then reset pending_transactions after callback is done.
  *
  * Pending transactions which no longer validate will be culled.
  */
template<typename Lambda>
void without_pending_transactions(database& db,
                                  std::vector<std::shared_ptr<full_transaction_type>>&& pending_transactions,
                                  Lambda callback)
{
    pending_transactions_restorer restorer( db, std::move(pending_transactions) );
    callback();
    return;
}

} } } // hive::chain::detail
