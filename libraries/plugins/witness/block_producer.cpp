#include <hive/plugins/witness/block_producer.hpp>

#include <hive/protocol/base.hpp>
#include <hive/protocol/config.hpp>
#include <hive/protocol/version.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/db_with.hpp>
#include <hive/chain/witness_objects.hpp>

#include <fc/macros.hpp>

namespace hive { namespace plugins { namespace witness {

void block_producer::generate_block( chain::generate_block_flow_control* generate_block_ctrl )
{
  hive::chain::detail::with_skip_flags( _db, generate_block_ctrl->get_skip_flags(), [&]()
  {
    try
    {
      _generate_block( generate_block_ctrl, generate_block_ctrl->get_block_timestamp(), generate_block_ctrl->get_witness_owner(),
                       generate_block_ctrl->get_block_signing_private_key() );
    }
    FC_CAPTURE_AND_RETHROW( ( generate_block_ctrl->get_block_timestamp() )( generate_block_ctrl->get_witness_owner() ) )
  } );
}

void block_producer::_generate_block( chain::generate_block_flow_control* generate_block_ctrl,
                                      fc::time_point_sec when, const chain::account_name_type& witness_owner,
                                      const fc::ecc::private_key& block_signing_private_key )
{
  uint32_t skip = _db.get_node_skip_flags();
  uint32_t slot_num = _db.get_slot_at_time( when );
  FC_ASSERT( slot_num > 0 );
  string scheduled_witness = _db.get_scheduled_witness( slot_num );
  FC_ASSERT( scheduled_witness == witness_owner );

  const auto& witness_obj = _db.get_witness( witness_owner );

  if( !(skip & chain::database::skip_witness_signature) )
    FC_ASSERT( witness_obj.signing_key == block_signing_private_key.get_public_key() );

  chain::signed_block_header pending_block_header;

  pending_block_header.previous = _db.head_block_id();
  pending_block_header.timestamp = when;
  pending_block_header.witness = witness_owner;

  adjust_hardfork_version_vote( _db.get_witness( witness_owner ), pending_block_header );

  std::vector<std::shared_ptr<hive::chain::full_transaction_type>> full_transactions;
  if( generate_block_ctrl->skip_transaction_reapplication() )
    fill_block_with_transactions( witness_owner, when, pending_block_header, full_transactions );
  else
    apply_pending_transactions( witness_owner, when, pending_block_header, full_transactions );

  // We have temporarily broken the invariant that
  // _pending_tx_session is the result of applying _pending_tx.
  // However, the push_block() call below will re-create the
  // _pending_tx_session.

  const fc::ecc::private_key* signer = (skip & chain::database::skip_witness_signature) ? nullptr : &block_signing_private_key;

  std::shared_ptr<hive::chain::full_block_type> full_pending_block = 
    hive::chain::full_block_type::create_from_block_header_and_transactions(pending_block_header, full_transactions, signer);

  // TODO:  Move this to _push_block() so session is restored.
  if( !(skip & chain::database::skip_block_size_check) )
    FC_ASSERT(full_pending_block->get_uncompressed_block_size() <= HIVE_MAX_BLOCK_SIZE );
  generate_block_ctrl->store_produced_block( full_pending_block );

  try
  {
    _chain.push_block( *generate_block_ctrl, skip );
  }
  catch (const fc::exception& e)
  {
    elog("NOTIFYALERT! Failed to apply newly produced block ${block_num} (${block_id}) with exception ${e}", 
         ("block_num", full_pending_block->get_block_num())("block_id", full_pending_block->get_block_id())(e));
    throw;
  }
}

void block_producer::adjust_hardfork_version_vote(const chain::witness_object& witness, chain::signed_block_header& pending_block_header)
{
  using namespace hive::protocol;

  if( witness.running_version != HIVE_BLOCKCHAIN_VERSION )
    pending_block_header.extensions.insert( block_header_extensions( HIVE_BLOCKCHAIN_VERSION ) );

  const auto& hfp = _db.get_hardfork_property_object();
  const auto& hf_versions = _db.get_hardfork_versions();

  if( hfp.current_hardfork_version < HIVE_BLOCKCHAIN_VERSION // Binary is newer hardfork than has been applied
    && ( witness.hardfork_version_vote != hf_versions.versions[ hfp.last_hardfork + 1 ] || witness.hardfork_time_vote != hf_versions.times[ hfp.last_hardfork + 1 ] ) ) // Witness vote does not match binary configuration
  {
    // Make vote match binary configuration
    pending_block_header.extensions.insert( block_header_extensions( hardfork_version_vote( hf_versions.versions[ hfp.last_hardfork + 1 ], hf_versions.times[ hfp.last_hardfork + 1 ] ) ) );
  }
  else if( hfp.current_hardfork_version == HIVE_BLOCKCHAIN_VERSION // Binary does not know of a new hardfork
        && witness.hardfork_version_vote > HIVE_BLOCKCHAIN_VERSION ) // Voting for hardfork in the future, that we do not know of...
  {
    // Make vote match binary configuration. This is vote to not apply the new hardfork.
    pending_block_header.extensions.insert( block_header_extensions( hardfork_version_vote( hf_versions.versions[ hfp.last_hardfork ], hf_versions.times[ hfp.last_hardfork ] ) ) );
  }
}

void block_producer::apply_pending_transactions(const chain::account_name_type& witness_owner,
                                                fc::time_point_sec when,
                                                chain::signed_block_header& pending_block_header, 
                                                std::vector<std::shared_ptr<hive::chain::full_transaction_type>>& full_transactions)
{
  // The 4 is for the max size of the transaction vector length
  //ABW: size of vector can take between 1 and 5 packed bytes, therefore +4 covers potential max (size of empty is already
  //included in uncorrected pack_size; in practice total_block_size will overshoot actual pack_size by 4 or 3 bytes, maybe
  //2 bytes if we ever allow blocks big enough to accomodate over 16k transactions (15 or more bits needed for size)
  size_t total_block_size = fc::raw::pack_size(pending_block_header) + 4;
  const auto& gpo = _db.get_dynamic_global_properties();
  uint64_t maximum_block_size = gpo.maximum_block_size;

  //
  // The following code throws away existing _pending_tx_session and
  // rebuilds it by re-applying pending transactions.
  //
  // This rebuild is necessary because pending transactions' validity
  // and semantics may have changed since they were received, because
  // time-based semantics are evaluated based on the current block
  // time.  These changes can only be reflected in the database when
  // the value of the "when" variable is known, which means we need to
  // re-apply pending transactions in this method.
  //
  auto& _pending_tx_session = _db.pending_transaction_session();
  _pending_tx_session.reset();
  _pending_tx_session.emplace( _db.start_undo_session(), _db.start_undo_session() );

  /// modify current witness so transaction evaluators can know who included the transaction
  _db.modify(_db.get_dynamic_global_properties(), [&]( chain::dynamic_global_property_object& dgp )
  {
    dgp.current_witness = witness_owner;
  } );

  BOOST_SCOPE_EXIT( &_db ) { _db.clear_tx_status(); } BOOST_SCOPE_EXIT_END
  // the flag also covers time of processing of required and optional actions
  _db.set_tx_status( chain::database::TX_STATUS_GEN_BLOCK );

  // since due to postponed we can't make sure all pending transactions are truly unique, we still
  // need to make that check here, but we can use _pending_tx_index instead of transaction_index
  _db._pending_tx_index.clear();
  auto skip = _db.get_node_skip_flags();
  skip |= chain::database::skip_transaction_dupe_check;

  uint32_t unused_tx_count = 0;
  uint32_t failed_tx_count = 0;
  for( const std::shared_ptr<hive::chain::full_transaction_type>& full_transaction : _db._pending_tx )
  {
    if( unused_tx_count > HIVE_BLOCK_GENERATION_POSTPONED_TX_LIMIT )
      break;

    // Only include transactions that have not expired yet for currently generating block,
    // this should clear problem transactions and allow block production to continue
    if( full_transaction->get_runtime_expiration() < when )
    {
      ++failed_tx_count;
      continue;
    }

    uint64_t new_total_size = total_block_size + full_transaction->get_transaction_size();

    // ignore transaction if it would make block too big
    if( new_total_size >= maximum_block_size )
    {
      ++unused_tx_count;
      continue;
    }

    try
    {
      const auto& trx_id = full_transaction->get_transaction_id();
      if( _db.is_known_transaction( trx_id ) )
      {
        ++failed_tx_count;
        continue;
      }
      _db.apply_transaction( full_transaction, skip );
      _pending_tx_session->second.squash( true );
      _db._pending_tx_index.emplace( trx_id );

      total_block_size = new_total_size;
      full_transactions.push_back(full_transaction);
    }
    catch( const fc::exception& e )
    {
      ++failed_tx_count;
      _pending_tx_session->second.undo( true );
      // Do nothing, transaction will be re-applied after this block is reapplied (and possibly
      // after processing further blocks) until it expires or repeats the exception during that time
      //wlog( "Transaction was not processed while generating block due to ${e}", ("e", e) );
      //wlog( "The transaction was ${t}", ("t", tx) );
    }
  }
  if( unused_tx_count > 0 || failed_tx_count > 0 )
  {
    wlog( "${x} transactions could not fit in newly produced block (${failed_tx_count} failed/expired)",
      ( "x", _db._pending_tx.size() - full_transactions.size() )( failed_tx_count ) );
  }

  _pending_tx_session.reset();
}

void block_producer::fill_block_with_transactions( const chain::account_name_type& witness_owner,
  fc::time_point_sec when,
  chain::signed_block_header& pending_block_header,
  std::vector<std::shared_ptr<hive::chain::full_transaction_type>>& full_transactions )
{
  //simplified version of apply_pending_transactions used by queen_plugin, when under certain assumptions
  //(no source of transactions that could change authorization after it was checked when adding to pending,
  //no edge case expiration, no deep forks that could fail to reapply all transactions), that are
  //fulfilled when queen is active (unless broken by external force, like debug_plugin), we can skip
  //reapplying transactions and just put them in the block in the same order as in pending;
  //that saves time on undo sessions and transaction reapplications

  size_t total_block_size = fc::raw::pack_size( pending_block_header ) + 4;
  const auto& gpo = _db.get_dynamic_global_properties();
  uint64_t maximum_block_size = gpo.maximum_block_size;

  for( const std::shared_ptr<hive::chain::full_transaction_type>& full_transaction : _db._pending_tx )
  {
    if( full_transaction->get_runtime_expiration() < when )
    {
      wlog( "Detected expired pending transaction during simplified block production at timestamp ${when}. "
        "That might cause error in block application.", ( when ) );
      wlog( "The transaction was ${t}", ( "t", full_transaction->get_transaction() ) );
      continue;
    }

    uint64_t new_total_size = total_block_size + full_transaction->get_transaction_size();

    // stop the process if it would make block too big
    if( new_total_size >= maximum_block_size )
      break;

    total_block_size = new_total_size;
    full_transactions.push_back( full_transaction );
  }
}

} } } // hive::plugins::witness
