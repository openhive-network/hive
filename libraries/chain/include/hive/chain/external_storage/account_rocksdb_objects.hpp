#pragma once

#include <hive/utilities/benchmark_dumper.hpp>
#include <hive/chain/account_object.hpp>
#include <hive/chain/account_object_multiindex.hpp>

#include <hive/chain/external_storage/allocator_helper.hpp>

namespace hive { namespace chain {

struct accounts_stats
{
  static hive::utilities::benchmark_dumper::account_archive_details_t stats; // note: times should be measured in nanoseconds
};

/**
 * rocksdb_account_object stores the account_object fields for RocksDB persistence.
 * In the split architecture, account_object contains only the "misc" fields:
 * - proxy, name, pending_claimed_accounts, created, block_created
 * - governance_vote_expiration_ts
 * - withdraw_routes, pending_escrow_transfers, open_recurrent_transfers, witnesses_voted_for
 * - savings_withdraw_requests, can_vote_flag, mined, memo_key, proxied_vsf_votes
 *
 * Other account data (assets/recovery, delayed_votes) are stored
 * in separate split objects (assets_object, delayed_votes_object)
 */
class rocksdb_account_object
{
  public:

  rocksdb_account_object(){}

  rocksdb_account_object( const account_object& obj );

  template<typename Return_Type>
  Return_Type build( chainbase::database& db )
  {
    if constexpr ( std::is_same_v<Return_Type, const account_object*> )
    {
      // Use minimal constructor that takes (id, name, created, memo_key)
      // Note: create_no_undo internally adds allocator
      const auto& obj = db.create_no_undo<account_object>(
                      id.get_value(),
                      name,
                      created,
                      memo_key );
      // Populate the rest of the fields via modify_no_undo to avoid undo tracking
      db.modify_no_undo( obj, [this]( account_object& o )
      {
        o.restore_block_created( this->block_created );
        o.restore_mined( this->mined );
        o.set_pending_claimed_accounts( this->pending_claimed_accounts );
        o.set_withdraw_routes( this->withdraw_routes );
        o.set_pending_escrow_transfers( this->pending_escrow_transfers );
        o.set_open_recurrent_transfers( this->open_recurrent_transfers );
        o.set_witnesses_voted_for( this->witnesses_voted_for );
        o.set_savings_withdraw_requests( this->savings_withdraw_requests );
        o.set_can_vote( this->can_vote_flag );
        o.get_proxied_vsf_votes() = this->proxied_vsf_votes;
      });
      return &obj;
    }
    else
    {
      auto obj = std::make_shared<account_object>(
                          allocator_helper::get_allocator<account_object, account_index>( db ),
                          id.get_value(),
                          name,
                          created,
                          memo_key );
      // Populate the rest of the fields for shared_ptr case too
      obj->restore_block_created( this->block_created );
      obj->restore_mined( this->mined );
      obj->set_pending_claimed_accounts( this->pending_claimed_accounts );
      obj->set_withdraw_routes( this->withdraw_routes );
      obj->set_pending_escrow_transfers( this->pending_escrow_transfers );
      obj->set_open_recurrent_transfers( this->open_recurrent_transfers );
      obj->set_witnesses_voted_for( this->witnesses_voted_for );
      obj->set_savings_withdraw_requests( this->savings_withdraw_requests );
      obj->set_can_vote( this->can_vote_flag );
      obj->get_proxied_vsf_votes() = this->proxied_vsf_votes;
      return Return_Type( obj );
    }
  }

  account_id_type                         id;

  // Fields from account_object (split architecture)
  // Note: proxy and governance_vote_expiration_ts are canonical on tiny_account_object
  account_name_type name;
  share_type        pending_claimed_accounts = 0;
  time_point_sec    created;
  time_point_sec    block_created;
  uint16_t          withdraw_routes = 0;
  uint16_t          pending_escrow_transfers = 0;
  uint16_t          open_recurrent_transfers = 0;
  uint16_t          witnesses_voted_for = 0;
  uint8_t           savings_withdraw_requests = 0;
  bool              can_vote_flag = true;
  bool              mined = true;
  public_key_type   memo_key;
  fc::array<share_type, HIVE_MAX_PROXY_RECURSION_DEPTH> proxied_vsf_votes;
};

class rocksdb_account_object_by_id
{
  public:

  rocksdb_account_object_by_id(){}

  rocksdb_account_object_by_id( const account_object& obj );

  account_id_type   id;
  account_name_type name;
};

} } // hive::chain

FC_REFLECT( hive::chain::rocksdb_account_object,
          (id)
          (name)
          (pending_claimed_accounts)
          (created)(block_created)
          (withdraw_routes)(pending_escrow_transfers)(open_recurrent_transfers)(witnesses_voted_for)
          (savings_withdraw_requests)(can_vote_flag)(mined)
          (memo_key)
          (proxied_vsf_votes)
        )

FC_REFLECT( hive::chain::rocksdb_account_object_by_id, (id)(name) )
