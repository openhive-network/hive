#include <hive/chain/hive_fwd.hpp>
#include <hive/chain/comment_object.hpp>
#include <hive/chain/account_object.hpp>

#include <fc/uint128.hpp>

namespace hive { namespace chain {

// comment_object helper
comment_object::author_and_permlink_hash_type comment_object::compute_author_and_permlink_hash(
  const account_object& author, const std::string& permlink )
{
  return compute_author_and_permlink_hash( author.get_id(), permlink );
}

// comment_cashout_object init helper
void comment_cashout_object::init( const account_object& _author, const std::string& _permlink,
  const time_point_sec& _creation_time, const time_point_sec& _cashout_time )
{
  author_id = _author.get_id();
  from_string( permlink, _permlink );
  FC_ASSERT( _creation_time <= _cashout_time );
}

void comment_cashout_object::add_beneficiary( const account_object& beneficiary, uint16_t weight )
{
  beneficiaries.emplace_back( beneficiary.get_id(), weight );
}

comment_cashout_object::stored_beneficiary_route_type::stored_beneficiary_route_type(
  const account_object& a, const uint16_t& w )
  : account_id( a.get_id() ), weight( w )
{}

// comment_vote_object private constructor
comment_vote_object::comment_vote_object( uint64_t _id, const account_object& _voter, const comment_object& _comment,
  const time_point_sec& _creation_time, int16_t _vote_percent, uint64_t _weight, int64_t _rshares )
: id( _id ), voter( _voter.get_id() ), comment( _comment.get_id() ), weight( _weight ),
  rshares( _rshares ), vote_percent( _vote_percent ), last_update( _creation_time )
{}

} } // hive::chain
