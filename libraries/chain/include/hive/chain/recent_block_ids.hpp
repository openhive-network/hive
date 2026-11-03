#pragma once

#include <hive/protocol/block_header.hpp>

#include <fc/exception/exception.hpp>
#include <fc/optional.hpp>

#include <algorithm>
#include <mutex>
#include <vector>

namespace hive { namespace chain {

  using hive::protocol::block_id_type;

  /**
   * @brief Ids of the most recent irreversible blocks, for block storage that doesn't keep the blocks.
   *
   * The id of block N is kept at position N % capacity, so the ids of the newest `capacity` blocks
   * are remembered. A block number in an id identifies the block an entry belongs to. Safe to use
   * from many threads.
   */
  class recent_block_ids
  {
  public:
    explicit recent_block_ids( uint32_t capacity ) : _ids( capacity )
    {
      FC_ASSERT( capacity > 0 );
    }

    /// Remembers the id of a block that has become irreversible.
    void remember( const block_id_type& id )
    {
      const uint32_t block_num = protocol::block_header::num_from_id( id );
      std::lock_guard<std::mutex> guard( _mutex );
      _ids[ block_num % _ids.size() ] = id;
    }

    /// Id of the block with given number, if it is still remembered.
    fc::optional<block_id_type> find( uint32_t block_num ) const
    {
      if( block_num == 0 )
        return fc::optional<block_id_type>();
      std::lock_guard<std::mutex> guard( _mutex );
      const block_id_type& id = _ids[ block_num % _ids.size() ];
      // an empty entry has block number 0, an outdated one belongs to an older block
      if( protocol::block_header::num_from_id( id ) != block_num )
        return fc::optional<block_id_type>();
      return id;
    }

    void clear()
    {
      std::lock_guard<std::mutex> guard( _mutex );
      std::fill( _ids.begin(), _ids.end(), block_id_type() );
    }

  private:
    mutable std::mutex          _mutex;
    std::vector<block_id_type>  _ids;
  };

} }
