#include <hive/chain/external_storage/account_rocksdb_objects.hpp>

namespace hive { namespace chain {

hive::utilities::benchmark_dumper::account_archive_details_t accounts_stats::stats;

rocksdb_account_object::rocksdb_account_object( const account_object& obj )
{
  id                          = obj.get_id();
  proxy                       = obj.get_proxy();
  name                        = obj.get_name();
  pending_claimed_accounts    = obj.get_pending_claimed_accounts();
  created                     = obj.get_creation_time();
  block_created               = obj.get_block_creation_time();
  governance_vote_expiration_ts = obj.get_governance_vote_expiration_ts();
  withdraw_routes             = obj.get_withdraw_routes();
  pending_escrow_transfers    = obj.get_pending_escrow_transfers();
  open_recurrent_transfers    = obj.get_open_recurrent_transfers();
  witnesses_voted_for         = obj.get_witnesses_voted_for();
  savings_withdraw_requests   = obj.get_savings_withdraw_requests();
  can_vote_flag               = obj.can_vote();
  mined                       = obj.was_mined();
  memo_key                    = obj.get_memo_key();
  proxied_vsf_votes           = obj.get_proxied_vsf_votes();
}

rocksdb_account_object_by_id::rocksdb_account_object_by_id( const account_object& obj )
{
  id   = obj.get_id();
  name = obj.get_name();
}

} } // hive::chain
