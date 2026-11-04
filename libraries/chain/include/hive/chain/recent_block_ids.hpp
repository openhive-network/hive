#pragma once

#include <hive/protocol/block_header.hpp>

#include <fc/exception/exception.hpp>
#include <fc/optional.hpp>
#include <fc/time.hpp>

#include <algorithm>
#include <mutex>
#include <vector>

namespace hive { namespace chain {

  using hive::protocol::block_id_type;

  /**
   * @brief Ids and timestamps of the most recent irreversible blocks, for block storage that
   * doesn't keep the blocks.
   *
   * Block N is kept at position N % capacity, so the newest `capacity` blocks are remembered.
   * A block number in an id identifies the block an entry belongs to. Safe to use from many
   * threads.
   */
  class recent_block_ids
  {
  public:
    struct entry
    {
      block_id_type       id;
      fc::time_point_sec  timestamp;
    };

    explicit recent_block_ids( uint32_t capacity ) : _entries( capacity )
    {
      FC_ASSERT( capacity > 0 );
    }

    /// Remembers a block that has become irreversible.
    void remember( const block_id_type& id, const fc::time_point_sec& timestamp )
    {
      const uint32_t block_num = protocol::block_header::num_from_id( id );
      std::lock_guard<std::mutex> guard( _mutex );
      _entries[ block_num % _entries.size() ] = entry{ id, timestamp };
    }

    /// The block with given number, if it is still remembered.
    fc::optional<entry> find( uint32_t block_num ) const
    {
      if( block_num == 0 )
        return fc::optional<entry>();
      std::lock_guard<std::mutex> guard( _mutex );
      const entry& found = _entries[ block_num % _entries.size() ];
      // an empty entry has block number 0, an outdated one belongs to an older block
      if( protocol::block_header::num_from_id( found.id ) != block_num )
        return fc::optional<entry>();
      return found;
    }

    void clear()
    {
      std::lock_guard<std::mutex> guard( _mutex );
      std::fill( _entries.begin(), _entries.end(), entry() );
    }

  private:
    mutable std::mutex  _mutex;
    std::vector<entry>  _entries;
  };

} }
