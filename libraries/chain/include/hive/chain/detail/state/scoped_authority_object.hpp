#pragma once

#include <hive/chain/hive_object_types.hpp>
#include <hive/protocol/asset.hpp>

namespace hive { namespace chain {

using hive::protocol::asset;

/** Persistent consensus record for one immutable scoped transfer grant. */
class scoped_authority_object : public object< scoped_authority_object_type, scoped_authority_object, std::true_type >
{
  CHAINBASE_OBJECT( scoped_authority_object );
public:
  using t_recipients = chainbase::t_vector< account_name_type >;

  template< typename Allocator >
  scoped_authority_object( allocator< Allocator > a, uint64_t _id,
    const account_name_type& _grantor, const account_name_type& _actor, uint32_t _grant_id,
    const asset& _max_per_transfer, const asset& _max_per_period, const asset& _max_lifetime,
    uint32_t _period_seconds, const time_point_sec& _valid_from, const time_point_sec& _expires_at,
    const std::vector< account_name_type >& _allowed_recipients )
  : id( _id ), grantor( _grantor ), actor( _actor ), grant_id( _grant_id ),
    max_per_transfer( _max_per_transfer ), max_per_period( _max_per_period ), max_lifetime( _max_lifetime ),
    period_seconds( _period_seconds ), valid_from( _valid_from ), expires_at( _expires_at ),
    lifetime_spent( 0, _max_lifetime.symbol ), period_spent( 0, _max_lifetime.symbol ), allowed_recipients( a )
  {
    for( const auto& recipient : _allowed_recipients )
      allowed_recipients.emplace_back( recipient );
  }

  account_name_type grantor;
  account_name_type actor;
  uint32_t grant_id = 0;
  asset max_per_transfer;
  asset max_per_period;
  asset max_lifetime;
  uint32_t period_seconds = 0;
  time_point_sec valid_from;
  time_point_sec expires_at;
  asset lifetime_spent;
  asset period_spent;
  uint64_t current_period_index = 0;
  bool revoked = false;
  t_recipients allowed_recipients;

  size_t get_dynamic_alloc() const
  {
    return allowed_recipients.capacity() * sizeof( typename t_recipients::value_type );
  }

  CHAINBASE_UNPACK_CONSTRUCTOR( scoped_authority_object, (allowed_recipients) );
};

} } // hive::chain

FC_REFLECT( hive::chain::scoped_authority_object,
  (id)(grantor)(actor)(grant_id)(max_per_transfer)(max_per_period)(max_lifetime)(period_seconds)
  (valid_from)(expires_at)(lifetime_spent)(period_spent)(current_period_index)(revoked)(allowed_recipients) )
