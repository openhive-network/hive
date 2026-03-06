#include <hive/chain/dhf_objects_multiindex.hpp>
#include <hive/chain/detail/state/recurrent_transfer_object_multiindex.hpp>
#include <hive/chain/detail/state/decline_voting_rights_request_object_multiindex.hpp>
#include <hive/chain/account_object_multiindex.hpp>
#include <hive/chain/detail/state/tiny_account_object.hpp>

#include <hive/chain/database_virtual_operations.hpp>
#include <hive/chain/util/dhf_helper.hpp>
#include <hive/chain/util/remove_guard.hpp>
#include <hive/chain/index.hpp>
#include <chainbase/chainbase.inl>

#include <hive/chain/util/type_registrar_definition.hpp>

#include <hive/protocol/hive_operations.hpp>
#include <hive/protocol/hive_virtual_operations.hpp>

namespace hive { namespace chain {

using hive::protocol::failed_recurrent_transfer_operation;
using hive::protocol::fill_recurrent_transfer_operation;
using hive::protocol::recurrent_transfer_extensions_type;
using hive::protocol::recurrent_transfer_pair_id;
using hive::protocol::proxy_cleared_operation;
using hive::protocol::declined_voting_rights_operation;

void initialize_core_indexes_11( database& db )
{
  HIVE_ADD_CORE_INDEX(db, proposal_index);
  HIVE_ADD_CORE_INDEX(db, proposal_vote_index);
  HIVE_ADD_CORE_INDEX(db, recurrent_transfer_index);
}

/**
  *  Iterates over all recurrent transfers with a due date date before
  *  the head block time and then executes the transfers
  */
void database::process_recurrent_transfers()
{
  if( !has_hardfork( HIVE_HARDFORK_1_25 ) )
    return;

  auto now = head_block_time();
  const auto& recurrent_transfers_by_date = get_index< recurrent_transfer_index, by_trigger_date >();
  auto itr = recurrent_transfers_by_date.begin();

  // uint16_t is okay because we stop at 1000, if the limit changes, make sure to check if it fits in the integer.
  uint16_t processed_transfers = 0;
  if( _benchmark_dumper.is_enabled() )
    _benchmark_dumper.begin();
  while( itr != recurrent_transfers_by_date.end() && itr->get_trigger_date() <= now )
  {
    // Since this is an intensive process, we don't want to process too many recurrent transfers in a single block
    if (processed_transfers >= HIVE_MAX_RECURRENT_TRANSFERS_PER_BLOCK)
    {
      ilog("Reached max processed recurrent transfers this block");
      break;
    }

    auto &current_recurrent_transfer = *itr;
    ++itr;

    const auto& from_account = get_account(current_recurrent_transfer.from_id);
    const auto& to_account = get_account(current_recurrent_transfer.to_id);
    asset available = get_balance(from_account, current_recurrent_transfer.amount.symbol);
    FC_ASSERT(current_recurrent_transfer.remaining_executions > 0);
    const auto remaining_executions = current_recurrent_transfer.remaining_executions -1;
    bool remove_recurrent_transfer = false;

    recurrent_transfer_extensions_type _extensions;

    //if `current_recurrent_transfer.pair_id` equals to 0, then it's not necessary to create an item in extensions. It's a default value.
    if( current_recurrent_transfer.pair_id )
      _extensions.emplace( recurrent_transfer_pair_id{ current_recurrent_transfer.pair_id } );

    // If we have enough money, we proceed with the transfer
    if (available >= current_recurrent_transfer.amount)
    {
      adjust_balance(from_account, -current_recurrent_transfer.amount);
      adjust_balance(to_account, current_recurrent_transfer.amount);

      // No need to update the object if we know that we will remove it
      if (remaining_executions == 0)
      {
        remove_recurrent_transfer = true;
      }
      else
      {
        modify(current_recurrent_transfer, [&](recurrent_transfer_object &rt)
        {
          rt.consecutive_failures = 0; // reset the consecutive failures counter
          rt.update_next_trigger_date();
          rt.remaining_executions = remaining_executions;
        });
      }

      push_virtual_operation( *this, fill_recurrent_transfer_operation(from_account.get_name(), to_account.get_name(), current_recurrent_transfer.amount, to_string(current_recurrent_transfer.memo), remaining_executions, _extensions));
    }
    else
    {
      uint8_t consecutive_failures = current_recurrent_transfer.consecutive_failures + 1;

      if (consecutive_failures < HIVE_MAX_CONSECUTIVE_RECURRENT_TRANSFER_FAILURES)
      {
        // No need to update the object if we know that we will remove it
        if (remaining_executions == 0)
        {
          remove_recurrent_transfer = true;
        }
        else
        {
          modify(current_recurrent_transfer, [&](recurrent_transfer_object &rt)
          {
            ++rt.consecutive_failures;
            rt.update_next_trigger_date();
            rt.remaining_executions = remaining_executions;
          });
        }
        // false means the recurrent transfer was not deleted
        push_virtual_operation( *this, failed_recurrent_transfer_operation(from_account.get_name(), to_account.get_name(), current_recurrent_transfer.amount, consecutive_failures, to_string(current_recurrent_transfer.memo), remaining_executions, remove_recurrent_transfer, _extensions));
      }
      else
      {
        // if we had too many consecutive failures, remove the recurrent payment object
        remove_recurrent_transfer = true;
        // true means the recurrent transfer was deleted
        push_virtual_operation( *this, failed_recurrent_transfer_operation(from_account.get_name(), to_account.get_name(), current_recurrent_transfer.amount, consecutive_failures, to_string(current_recurrent_transfer.memo), remaining_executions, true, _extensions));
      }
    }

    if (remove_recurrent_transfer)
    {
      remove( current_recurrent_transfer );
      modify( from_account, [&]( account_object& a )
      {
        FC_ASSERT( a.get_open_recurrent_transfers() > 0 );
        a.set_open_recurrent_transfers( a.get_open_recurrent_transfers() - 1 );
      });
    }

    processed_transfers++;
  }
  if( _benchmark_dumper.is_enabled() && processed_transfers )
    _benchmark_dumper.end( "processing", "hive::protocol::recurrent_transfer_operation", processed_transfers );
}

void database::remove_proposal_votes_for_accounts_without_voting_rights()
{
  std::vector<account_name_type> voters;

  const auto& proposal_votes_idx = get_index< proposal_vote_index, by_voter_proposal >();

  auto itr = proposal_votes_idx.begin();
  while( itr != proposal_votes_idx.end() )
  {
    voters.push_back( itr->voter );
    ++itr;
  }

  //Lack of voters.
  if( voters.empty() )
    return;

  std::vector<account_name_type> accounts;

  for( auto& voter : voters )
  {
    const auto& account = get_account( voter );
    if( !account.can_vote() )
      accounts.push_back( account.get_name() );
  }

  //Lack of voters who declined voting rights.
  if( accounts.empty() )
    return;

  /*
    For every account set a request to remove proposal votes.
    Current time is set, because we want to start removing proposal votes as soon as possible.
  */
  const auto& request_idx = get_index< decline_voting_rights_request_index, by_account >();

  for( auto& account : accounts )
  {
    auto found = request_idx.find( account );
    if( found !=request_idx.end() )
    {
      /*
        Before HF28 it was possible to create `decline_voting_rights` operation again, even if an account had `can_vote` set to false.
        In this case `effective_date` must be changed otherwise a new object is created.
      */
      modify( *found, [&]( decline_voting_rights_request_object& req )
      {
        req.effective_date = head_block_time();
      });
    }
    else
    {
      create< decline_voting_rights_request_object >( [&]( decline_voting_rights_request_object& req )
      {
        req.account = account;
        req.effective_date = head_block_time();
      });
    }
  }
}

void database::process_decline_voting_rights()
{
  const auto& request_idx = get_index< decline_voting_rights_request_index >().indices().get< by_effective_date >();
  auto itr = request_idx.begin();

  int count = 0;
  if( _benchmark_dumper.is_enabled() )
    _benchmark_dumper.begin();

  const auto& proposal_votes = get_index< proposal_vote_index, by_voter_proposal >();
  remove_guard obj_perf( get_remove_threshold() );

  while( itr != request_idx.end() && itr->effective_date <= head_block_time() )
  {
    const auto& account = get_account( itr->account );

    if( !has_hardfork( HIVE_HARDFORK_1_28 ) || dhf_helper::remove_proposal_votes( account, proposal_votes, *this, obj_perf ) )
    {
      nullify_proxied_witness_votes( account );
      clear_witness_votes( account );

      const auto& account_tiny = *get_index< tiny_account_index, by_name >().find( account.get_name() );
      if( account_tiny.has_proxy() )
        push_virtual_operation( *this, proxy_cleared_operation( account.get_name(), get_account( account_tiny.get_proxy() ).get_name()) );

      push_virtual_operation( *this, declined_voting_rights_operation( itr->account ) );

      modify( account, [&]( account_object& a )
      {
        a.set_can_vote( false );
      });

      static_cast<chainbase::database&>(*this).modify( account_tiny, [&]( tiny_account_object& t )
      {
        t.clear_proxy();
      });

      remove( *itr );
      itr = request_idx.begin();
      ++count;
    }
    else
    {
      ilog("Threshold exceeded while deleting proposal votes for account ${account}.",
        ("account", account.get_name())); // to be continued in next block
      break;
    }
  }
  if( _benchmark_dumper.is_enabled() && count )
    _benchmark_dumper.end( "processing", "hive::protocol::decline_voting_rights_operation", count );
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::proposal_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::proposal_vote_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::recurrent_transfer_index)

// Explicit template instantiations for chainbase::database methods
template const chainbase::generic_index<hive::chain::proposal_index>& chainbase::database::get_index<hive::chain::proposal_index>() const;
template chainbase::generic_index<hive::chain::proposal_index>& chainbase::database::get_mutable_index<hive::chain::proposal_index>();

template const chainbase::generic_index<hive::chain::proposal_vote_index>& chainbase::database::get_index<hive::chain::proposal_vote_index>() const;
template chainbase::generic_index<hive::chain::proposal_vote_index>& chainbase::database::get_mutable_index<hive::chain::proposal_vote_index>();

template const chainbase::generic_index<hive::chain::recurrent_transfer_index>& chainbase::database::get_index<hive::chain::recurrent_transfer_index>() const;
template chainbase::generic_index<hive::chain::recurrent_transfer_index>& chainbase::database::get_mutable_index<hive::chain::recurrent_transfer_index>();