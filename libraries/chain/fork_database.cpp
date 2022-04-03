#include <hive/chain/fork_database.hpp>

#include <hive/chain/database_exceptions.hpp>
#include <boost/range/algorithm/reverse.hpp>

namespace hive { namespace chain {

fork_database::fork_database()
{
}
void fork_database::reset()
{
  with_write_lock( [&]() {
    _head.reset();
    _index.clear();
  });
}

void fork_database::pop_block()
{
  with_write_lock( [&]() {
    FC_ASSERT( _head, "cannot pop an empty fork database" );
    auto prev = _head->prev.lock();
    FC_ASSERT( prev, "popping head block would leave fork DB empty" );
    _head = prev;
  });
}

void fork_database::start_block(signed_block b)
{
  auto item = std::make_shared<fork_item>(std::move(b));
  with_write_lock( [&]() {
    _index.insert(item);
    _head = item;
  });
}

/**
  * Pushes the block into the fork database and caches it if it doesn't link
  *
  */
shared_ptr<fork_item> fork_database::push_block(const signed_block& b)
{
  auto item = std::make_shared<fork_item>(b);
  return with_write_lock( [&]() {
    try {
      _push_block(item);
    }
    catch ( const unlinkable_block_exception& e )
    {
      wlog( "Pushing block to fork database that failed to link: ${id}, ${num}", ("id",b.id())("num",b.block_num()) );
      wlog( "Head: ${num}, ${id}", ("num",_head->data.block_num())("id",_head->data.id()) );
      _unlinked_index.insert( item );
      throw;
    }
    return _head;
  });
}

void  fork_database::_push_block(const item_ptr& item)
{
  if( _head ) // make sure the block is within the range that we are caching
  {
    FC_ASSERT( item->num > std::max<int64_t>( 0, int64_t(_head->num) - (_max_size) ),
            "attempting to push a block that is too old",
            ("item->num",item->num)("head",_head->num)("max_size",_max_size));
  }
  //if we can't link the new item all the way back to genesis block,
  // throw an unlinkable block exception
  if( _head && item->previous_id() != block_id_type() )
  {
    auto& index = _index.get<block_id>();
    auto itr = index.find(item->previous_id());
    HIVE_ASSERT(itr != index.end(), unlinkable_block_exception, "block does not link to known chain");
    FC_ASSERT(!(*itr)->invalid);
    item->prev = *itr;
  }
  
  _index.insert(item);
  // if we don't have a head block or this is the next block or on a longer fork than our head block
  //   make this the new head block
  if( !_head || item->num > _head->num ) _head = item;

  _push_next( item ); //check for any unlinked blocks that can now be linked to our fork
}

shared_ptr<fork_item> fork_database::head()const 
{
  return with_read_lock( [&]() { return _head; } );
}

/**
  *  Iterate through the unlinked cache and insert anything that
  *  links to the newly inserted item.  This will start a recursive
  *  set of calls performing a depth-first insertion of pending blocks as
  *  _push_next(..) calls _push_block(...) which will in turn call _push_next
  */
void fork_database::_push_next( const item_ptr& new_item )
{
    auto& prev_idx = _unlinked_index.get<by_previous>();

    auto itr = prev_idx.find( new_item->id );
    while( itr != prev_idx.end() )
    {
      auto tmp = *itr;
      prev_idx.erase( itr );
      try
      {
        _push_block( tmp );
      }
      catch(const fc::assert_exception& e)
      {
        //swallow invalid block exception so we can process other unlinked blocks
        wdump((e.to_detail_string()));
      }

      itr = prev_idx.find( new_item->id );
    }
}

void fork_database::set_max_size( uint32_t s )
{
  with_write_lock( [&]() {
    _max_size = s;
    if( !_head ) return;
    //wlog("set_max_size(${s}), head is ${head}, erasing <= ${thresh}", (s)("head", _head->num)("thresh", _head->num - s));
  
    { /// index
      auto& by_num_idx = _index.get<block_num>();
      auto itr = by_num_idx.begin();
      while( itr != by_num_idx.end() )
      {
        if( (*itr)->num <= std::max(int64_t(0),int64_t(_head->num) - _max_size) )
          by_num_idx.erase(itr);
        else
          break;
        itr = by_num_idx.begin();
      }
    }
    { /// unlinked_index
      auto& by_num_idx = _unlinked_index.get<block_num>();
      auto itr = by_num_idx.begin();
      while( itr != by_num_idx.end() )
      {
        if( (*itr)->num <= std::max(int64_t(0),int64_t(_head->num) - _max_size) )
          by_num_idx.erase(itr);
        else
          break;
        itr = by_num_idx.begin();
      }
    }
  });
}

bool fork_database::is_known_block(const block_id_type& id)const
{
  return with_read_lock( [&]() {
    auto& index = _index.get<block_id>();
    auto itr = index.find(id);
    if( itr != index.end() )
      return true;
    auto& unlinked_index = _unlinked_index.get<block_id>();
    auto unlinked_itr = unlinked_index.find(id);
    return unlinked_itr != unlinked_index.end();
  });
}

item_ptr fork_database::fetch_block_unlocked(const block_id_type& id)const
{
  auto& index = _index.get<block_id>();
  auto itr = index.find(id);
  if( itr != index.end() )
    return *itr;
  auto& unlinked_index = _unlinked_index.get<block_id>();
  auto unlinked_itr = unlinked_index.find(id);
  if( unlinked_itr != unlinked_index.end() )
    return *unlinked_itr;
  return item_ptr();
}

item_ptr fork_database::fetch_block(const block_id_type& id)const
{
  return with_read_lock([&]() { return fetch_block_unlocked(id); });
}

vector<item_ptr> fork_database::fetch_block_by_number_unlocked(uint32_t num)const
{
  try
  {
    vector<item_ptr> result;
    auto const& block_num_idx = _index.get<block_num>();
    auto itr = block_num_idx.lower_bound(num);
    while( itr != block_num_idx.end() && itr->get()->num == num )
    {
      if( (*itr)->num == num )
        result.push_back( *itr );
      else
        break;
      ++itr;
    }
    return result;
  }
  FC_LOG_AND_RETHROW()
}

vector<item_ptr> fork_database::fetch_block_by_number(uint32_t num)const
{
  try
  {
    return with_read_lock( [&]() {
      return fetch_block_by_number_unlocked(num);
    });
  }
  FC_LOG_AND_RETHROW()
}

time_point_sec fork_database::head_block_time(fc::microseconds wait_for_microseconds)const
{ try {
  return with_read_lock( [&]() {
    return _head ? _head->data.timestamp : time_point_sec();
  }, wait_for_microseconds);
} FC_RETHROW_EXCEPTIONS(warn, "") }

uint32_t fork_database::head_block_num(fc::microseconds wait_for_microseconds)const
{ try {
  return with_read_lock( [&]() {
    return _head ? _head->num : 0;
  }, wait_for_microseconds);
} FC_RETHROW_EXCEPTIONS(warn, "") }

block_id_type fork_database::head_block_id(fc::microseconds wait_for_microseconds)const
{ try {
  return with_read_lock( [&]() {
    return _head ? _head->id : block_id_type();
  }, wait_for_microseconds);
} FC_RETHROW_EXCEPTIONS(warn, "") }


pair<fork_database::branch_type,fork_database::branch_type> fork_database::fetch_branch_from(block_id_type first, block_id_type second)const
{ try {
  return with_read_lock( [&]() {
    // This function gets a branch (i.e. vector<fork_item>) leading
    // back to the most recent common ancestor.
    pair<branch_type,branch_type> result;
    auto first_branch_itr = _index.get<block_id>().find(first);
    FC_ASSERT(first_branch_itr != _index.get<block_id>().end());
    auto first_branch = *first_branch_itr;
  
    auto second_branch_itr = _index.get<block_id>().find(second);
    FC_ASSERT(second_branch_itr != _index.get<block_id>().end());
    auto second_branch = *second_branch_itr;
  
  
    while( first_branch->data.block_num() > second_branch->data.block_num() )
    {
      result.first.push_back(first_branch);
      first_branch = first_branch->prev.lock();
      FC_ASSERT(first_branch);
    }
    while( second_branch->data.block_num() > first_branch->data.block_num() )
    {
      result.second.push_back( second_branch );
      second_branch = second_branch->prev.lock();
      FC_ASSERT(second_branch);
    }
    while( first_branch->data.previous != second_branch->data.previous )
    {
      result.first.push_back(first_branch);
      result.second.push_back(second_branch);
      first_branch = first_branch->prev.lock();
      FC_ASSERT(first_branch);
      second_branch = second_branch->prev.lock();
      FC_ASSERT(second_branch);
    }
    if( first_branch && second_branch )
    {
      result.first.push_back(first_branch);
      result.second.push_back(second_branch);
    }
    return result;
  });
} FC_CAPTURE_AND_RETHROW( (first)(second) ) }

shared_ptr<fork_item> fork_database::walk_main_branch_to_num_unlocked( uint32_t block_num )const
{
  shared_ptr<fork_item> next = head();
  if( block_num > next->num )
    return shared_ptr<fork_item>();

  while( next.get() != nullptr && next->num > block_num )
    next = next->prev.lock();
  return next;
}

shared_ptr<fork_item> fork_database::walk_main_branch_to_num( uint32_t block_num )const
{
  return with_read_lock( [&]() {
    return walk_main_branch_to_num_unlocked( block_num );
  });
}

shared_ptr<fork_item> fork_database::fetch_block_on_main_branch_by_number( uint32_t block_num, fc::microseconds wait_for_microseconds )const
{
  return with_read_lock( [&]() {
    vector<item_ptr> blocks = fetch_block_by_number_unlocked(block_num);
    if( blocks.size() == 1 )
      return blocks[0];
    if( blocks.size() == 0 )
      return shared_ptr<fork_item>();
    return walk_main_branch_to_num_unlocked(block_num);
  }, wait_for_microseconds);
}

vector<fork_item> fork_database::fetch_block_range_on_main_branch_by_number( const uint32_t first_block_num, const uint32_t count, fc::microseconds wait_for_microseconds )const
{
  return with_read_lock( [&]() {
    vector<fork_item> results;

    if (!_head ||
        _head->num < first_block_num)
      return results;
  
    // the caller is asking for blocks from first_block_num ... last_desired_block_num
    const uint32_t last_desired_block_num = first_block_num + count - 1;
  
    // but if the head block isn't to last_desired_block_num yet, the latest we can have is the head block
    const uint32_t last_block_num = std::min(last_desired_block_num, _head->num);
  
    // look up that last block and see if we have it
    const auto& block_num_idx = _index.get<block_num>();
    const auto fork_items_for_last_block_num = boost::make_iterator_range(block_num_idx.equal_range(last_block_num));
    // if we don't have it (it has already been moved to the block log), return an empty list
    if (fork_items_for_last_block_num.empty())
      return results;
    
    // otherwise, walk backwards from that block, collecting blocks along the way.  Stop when
    // we either reach the first block the caller asked for, or we run out of blocks in the fork database
    shared_ptr<fork_item> item;
    if (std::next(fork_items_for_last_block_num.begin()) == fork_items_for_last_block_num.end()) // if exactly one block numbered last_block_num
      item = fork_items_for_last_block_num.front();
    else
      item = walk_main_branch_to_num_unlocked(last_block_num);
  
    for (; item && item->num >= first_block_num; item = item->prev.lock())
      results.push_back(*item);
  
    // we collected the blocks in descending order, reverse that order
    boost::reverse(results);
  
    return results;
  }, wait_for_microseconds);
}

void fork_database::set_head(shared_ptr<fork_item> h)
{
  with_write_lock( [&]() {
    _head = std::move( h );
  });
}

void fork_database::remove(block_id_type id)
{
  with_write_lock( [&]() {
    if (_head && _head->id == id)
      _head = _head->prev.lock();
    _index.get<block_id>().erase(id);
  });
}

std::vector<block_id_type> fork_database::get_blockchain_synopsis(block_id_type reference_point, uint32_t number_of_blocks_after_reference_point, 
                                                                  /* out */ fc::optional<uint32_t>& block_number_needed_from_block_log)
{ try {
  return with_read_lock([&]() {
    std::vector<block_id_type> synopsis;

    if (!_head)
      return synopsis; // we have no blocks
    
    // if we have blocks, we expect the index to be useful
    assert(!_index.empty());

    synopsis.reserve(30);

    const auto& block_num_idx = _index.get<block_num>();
    // The oldest block in the fork database is always the last irreversible block
    uint32_t last_irreversible_block_num = (*block_num_idx.begin())->num;


    // there are several cases: 
    // - no reference point given: 
    //     we return a summary of last_irreversible up through the current head block
    // - reference point is at or before the last_irreversible_block
    //     !!! problem: we don't know if the reference point is on the main
    //         chain because we don't have access to the block log !!!
    //     if the reference point is on the main chain:
    //       our summary would just be the reference point
    //     if not
    //       ??? we can't actually construct a synopsis that would be useful ???
    // - reference point is after last_irreversible_block
    //     return a summary of the chain from last_irreversible through reference_point


    // if no reference point specified, summarize the main chain from the last_irreversible_block up to the head_block
    // (same behavior as if the reference point was our head block)
    if (reference_point == block_id_type())
      reference_point = _head->id;

    uint32_t reference_point_block_num = protocol::block_header::num_from_id(reference_point);
    //edump((last_irreversible_block_num)(reference_point_block_num)(_head->num));
    //edump((reference_point_block_num)(last_irreversible_block_num));
    if (reference_point_block_num < last_irreversible_block_num)
    {
      // this is a weird case, the reference point is before anything in the
      // fork database, so we can't put anything in the synopsis.  The caller
      // will need to supply the reference point if it's in the main chain, 
      // or throw if not
      block_number_needed_from_block_log = reference_point_block_num;
      //edump((block_number_needed_from_block_log)(synopsis));
      return synopsis;
    }

    // set low_block_num to the number of the first block in the final synopsis
    // usually it will be last_irreversible, unless there is none
    uint32_t low_block_num = last_irreversible_block_num ? last_irreversible_block_num : 1;


    // the node is asking for a summary of the block chain up to the reference
    // block, and the reference block should be in the fork database
    const auto& block_id_index = _index.get<block_id>();
    auto reference_point_iter = block_id_index.find(reference_point);
    if (reference_point_iter == block_id_index.end())
    {
      // we've got a problem: the block number indicates the block should 
      // be in the fork database, but it's not.  A well-behaved peer
      // shouldn't cause this situation
      // maybe throw here???
      //edump((last_irreversible_block_num)(reference_point_block_num)(_head->num)(reference_point));
      FC_THROW("Unable to construct a useful synopsis because we can't find the reference block in the fork database");
    }

    std::vector<block_id_type> block_ids_on_this_fork;

    item_ptr next = *reference_point_iter;
    while (next.get())
    {
      block_ids_on_this_fork.push_back(next->id);
      next = next->prev.lock();
    }

    // block_ids_on_this_fork now contains
    // [reference_point, ..., first_reversible_block, last_irreversible_block]

    // at this point:
    // low_block_num is the block before the first block we can undo,
    // high_block_num is the block number of the reference block, or the end of the chain if no reference provided

    // true_high_block_num is the ending block number after the network code appends any item ids it
    // knows about that we don't
    uint32_t true_high_block_num = reference_point_block_num + number_of_blocks_after_reference_point;
    //idump((low_block_num)(reference_point_block_num)(true_high_block_num));
    do
    {
      synopsis.push_back(block_ids_on_this_fork[block_ids_on_this_fork.size() - (low_block_num - last_irreversible_block_num) - 1]);
      low_block_num += (true_high_block_num - low_block_num + 2) / 2;
    }
    while (low_block_num <= reference_point_block_num);
    return synopsis;
  });    
} FC_LOG_AND_RETHROW() }


} } // hive::chain
