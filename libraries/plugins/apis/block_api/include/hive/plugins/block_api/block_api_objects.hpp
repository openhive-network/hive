#pragma once
#include <hive/chain/account_object.hpp>
#include <hive/chain/block_summary_object.hpp>
#include <hive/chain/comment_object.hpp>
#include <hive/chain/global_property_object.hpp>
#include <hive/chain/history_object.hpp>
#include <hive/chain/hive_objects.hpp>
#include <hive/chain/transaction_object.hpp>
#include <hive/chain/witness_objects.hpp>
#include <hive/chain/database.hpp>

namespace hive { namespace plugins { namespace block_api {

using namespace hive::chain;

struct api_signed_block_object : public signed_block
{
  api_signed_block_object( const signed_block& block, const optional<block_id_type>& block_id, const optional<public_key_type>& signing_key ) : signed_block( block )
  {
    block_id_type _block_id;
    if( block_id.valid() )
      _block_id = *block_id;
    else
      _block_id = id();

    public_key_type _signing_key;
    if( signing_key.valid() )
      _signing_key = *signing_key;
    else
      _signing_key = signee();

    init( _block_id, _signing_key );
  }

  api_signed_block_object( const signed_block& block ) : signed_block( block )
  {
    init( id(), signee() );
  }

  api_signed_block_object() {}

  void init( const block_id_type& _block_id, const public_key_type& _signing_key )
  {
    block_id = _block_id;
    signing_key = _signing_key;
    transaction_ids.reserve( transactions.size() );
    for( const signed_transaction& tx : transactions )
      transaction_ids.push_back( tx.id() );
  }

  block_id_type                 block_id;
  public_key_type               signing_key;
  vector< transaction_id_type > transaction_ids;
};

} } } // hive::plugins::database_api

FC_REFLECT_DERIVED( hive::plugins::block_api::api_signed_block_object, (hive::protocol::signed_block),
              (block_id)
              (signing_key)
              (transaction_ids)
            )
