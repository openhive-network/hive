#pragma once
#include <hive/chain/hive_object_types.hpp>

#include <hive/protocol/merkle.hpp>

#include <vector>

namespace hive { namespace chain {

using chainbase::t_vector;

using hive::protocol::digest_type;
using hive::protocol::checksum_type;

/**
  * @class state_stamp_data_object
  * @brief Singleton collecting digests ("stamps") of arbitrary consensus state.
  *
  * Any part of the node can @ref publish a digest of some piece of consensus state during block
  * processing. All digests accumulated between blocks are combined by @ref finalize into a single
  * merkle root that (since HF29) the block producer writes into a block header extension. Every node
  * recomputes the same root while applying the block and rejects the block on mismatch - this way any
  * node that diverged on the stamped state is quickly forced out of consensus. After the block is
  * accepted the collection is @ref reset, so each block covers only the stamps produced since the
  * previous one.
  *
  * The mechanism is intentionally generic; its first user is RC (publishing the rolling hash of payer
  * RC mana), but it can later pin down other internal, otherwise-virtual state (e.g. transfers from
  * recurrent transfer execution) as an immutable part of consensus.
  */
class state_stamp_data_object : public object< state_stamp_data_object_type, state_stamp_data_object, std::true_type >
{
  CHAINBASE_OBJECT( state_stamp_data_object );
public:
  template< typename Allocator >
  state_stamp_data_object( allocator< Allocator > a, uint64_t _id )
    : id( _id ), stamps( a )
  {}

  //adds a new digest to the collection
  void publish( const digest_type& stamp ) { stamps.push_back( stamp ); }
  //combines all collected digests into a single merkle root
  checksum_type finalize() const { return hive::protocol::calculate_merkle_root( std::vector< digest_type >( stamps.begin(), stamps.end() ) ); }
  //removes all collected digests (call after the stamp made it into a block)
  void reset() { stamps.clear(); }
  //tells whether any digest has been collected since the last reset
  bool is_empty() const { return stamps.empty(); }

  size_t get_dynamic_alloc() const
  {
    size_t size = 0;
    size += stamps.capacity() * sizeof( decltype( stamps )::value_type );
    return size;
  }

private:
  using t_stamps = t_vector< digest_type >;

  t_stamps stamps;

  CHAINBASE_UNPACK_CONSTRUCTOR( state_stamp_data_object, (stamps) );
};

} } // hive::chain

FC_REFLECT( hive::chain::state_stamp_data_object, (id)(stamps) )
