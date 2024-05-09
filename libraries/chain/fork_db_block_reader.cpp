#include <hive/chain/fork_db_block_reader.hpp>

#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/fork_database.hpp>

#include <boost/range/adaptor/reversed.hpp>

namespace hive { namespace chain {

fork_db_block_reader::fork_db_block_reader( fork_database& fork_db, 
                                            const block_log_wrapper& log_reader )
  : _fork_db( fork_db ), _log_reader( log_reader )
{}

std::shared_ptr<full_block_type> fork_db_block_reader::head_block() const
{
  auto head = _fork_db.head();
  return head ? head->full_block : _log_reader.head_block();
}

uint32_t fork_db_block_reader::head_block_num(
  fc::microseconds wait_for_microseconds /*= fc::microseconds()*/ ) const
{
  auto head = _fork_db.head();
  return head ? head->get_block_num() : _log_reader.head_block_num( wait_for_microseconds );
}

block_id_type fork_db_block_reader::head_block_id( 
  fc::microseconds wait_for_microseconds /*= fc::microseconds()*/ ) const
{
  auto head = _fork_db.head();
  return head ? head->get_block_id() : _log_reader.head_block_id( wait_for_microseconds );
}

std::shared_ptr<full_block_type> fork_db_block_reader::read_block_by_num( uint32_t block_num ) const
{
  return _log_reader.read_block_by_num( block_num );
}

void fork_db_block_reader::process_blocks( uint32_t starting_block_number,
  uint32_t ending_block_number, block_processor_t processor,
  blockchain_worker_thread_pool& thread_pool ) const
{
  return _log_reader.process_blocks( starting_block_number, ending_block_number, processor,
                                     thread_pool );
}

std::shared_ptr<full_block_type> fork_db_block_reader::get_block_by_number( uint32_t block_num,
  fc::microseconds wait_for_microseconds ) const
{
  // For the time being we'll silently return empty pointer for requests of future blocks.
  // FC_ASSERT( block_num <= head_block_num(), "Got no block with number greater than ${num}.", ("num", head_block_num()) );

  std::shared_ptr<full_block_type> blk;

  shared_ptr<fork_item> forkdb_item = 
    _fork_db.fetch_block_on_main_branch_by_number(block_num, wait_for_microseconds);
  if (forkdb_item)
    blk = forkdb_item->full_block;
  else
    blk = _log_reader.get_block_by_number(block_num);

  return blk;
}

std::shared_ptr<full_block_type> fork_db_block_reader::fetch_block_by_id( 
  const block_id_type& id ) const
{
  try {
    shared_ptr<fork_item> fork_item = _fork_db.fetch_block( id );
    if (fork_item)
      return fork_item->full_block;

    return _log_reader.fetch_block_by_id( id );
  } FC_CAPTURE_AND_RETHROW()
}


bool fork_db_block_reader::is_known_block( const block_id_type& id ) const
{
  try {
    if( _fork_db.fetch_block( id ) )
      return true;

    return _log_reader.is_known_block( id );
  } FC_CAPTURE_AND_RETHROW()
}

bool fork_db_block_reader::is_known_block_unlocked( const block_id_type& id ) const
{ 
  try {
    if (_fork_db.fetch_block_unlocked(id, true /* only search linked blocks */))
      return true;

    return _log_reader.is_known_block( id );
  } FC_CAPTURE_AND_RETHROW()
}

std::deque<block_id_type>::const_iterator fork_db_block_reader::find_first_item_not_in_blockchain(
  const std::deque<block_id_type>& item_hashes_received ) const
{
  return _fork_db.with_read_lock([&](){
    return std::partition_point(item_hashes_received.begin(), item_hashes_received.end(), [&](const block_id_type& block_id) {
      return is_known_block_unlocked(block_id);
    });
  });
}

block_id_type fork_db_block_reader::find_block_id_for_num( uint32_t block_num )const
{
  block_id_type result;

  try
  {
    if( block_num != 0 )
    {
      // See if fork DB has the item
      shared_ptr<fork_item> fitem = _fork_db.fetch_block_on_main_branch_by_number( block_num );
      if( fitem )
      {
        result = fitem->get_block_id();
      }
      else
      {
        // Next we check if block_log has it. Irreversible blocks are there.
        result = _log_reader.find_block_id_for_num( block_num );
      }
    }
  }
  FC_CAPTURE_AND_RETHROW( (block_num) )

  if( result == block_id_type() )
    FC_THROW_EXCEPTION(fc::key_not_found_exception, "block number not found");

  return result;
}

std::vector<std::shared_ptr<full_block_type>> fork_db_block_reader::fetch_block_range( 
  const uint32_t starting_block_num, const uint32_t count, 
  fc::microseconds wait_for_microseconds /*= fc::microseconds()*/ ) const
{ 
  try {
    // for debugging, put the head block back so it should straddle the last irreversible
    // const uint32_t starting_block_num = head_block_num() - 30;
    FC_ASSERT(starting_block_num > 0, "Invalid starting block number");
    FC_ASSERT(count > 0, "Why ask for zero blocks?");
    FC_ASSERT(count <= 1000, "You can only ask for 1000 blocks at a time");
    idump((starting_block_num)(count));

    vector<fork_item> fork_items = _fork_db.fetch_block_range_on_main_branch_by_number( starting_block_num, count, wait_for_microseconds );
    idump((fork_items.size()));
    if (!fork_items.empty())
      idump((fork_items.front().get_block_num()));

    // if the fork database returns some blocks, it means:
    // - that the last block in the range [starting_block_num, starting_block_num + count - 1]
    // - any block before the first block it returned should be in the block log
    uint32_t remaining_count = fork_items.empty() ? count : fork_items.front().get_block_num() - starting_block_num;
    idump((remaining_count));
    std::vector<std::shared_ptr<full_block_type>> result;

    if (remaining_count)
      result = _log_reader.fetch_block_range(starting_block_num, remaining_count, wait_for_microseconds);

    result.reserve(result.size() + fork_items.size());
    for (fork_item& item : fork_items)
      result.push_back(item.full_block);

    return result;
  } FC_LOG_AND_RETHROW()
}

std::vector<block_id_type> fork_db_block_reader::get_blockchain_synopsis( 
  const block_id_type& reference_point, uint32_t number_of_blocks_after_reference_point ) const
{
  fc::optional<uint32_t> block_number_needed_from_block_log;
  std::vector<block_id_type> synopsis = 
    _fork_db.get_blockchain_synopsis(reference_point, number_of_blocks_after_reference_point,
                                     block_number_needed_from_block_log);

  if (block_number_needed_from_block_log)
  {
    uint32_t reference_point_block_num = protocol::block_header::num_from_id(reference_point);
    block_id_type read_block_id = get_block_id_for_num( *block_number_needed_from_block_log );

    if (reference_point_block_num == *block_number_needed_from_block_log)
    {
      // we're getting this block from the database because it's the reference point,
      // not because it's the last irreversible.
      // We can only do this if the reference point really is in the blockchain
      if (read_block_id == reference_point)
        synopsis.insert(synopsis.begin(), reference_point);
      else
      {
        wlog("Unable to generate a usable synopsis because the peer we're generating it for forked too long ago "
             "(our chains diverge before block #${reference_point_block_num}",
             (reference_point_block_num));
        // TODO: get the right type of exception here
        //FC_THROW_EXCEPTION(graphene::net::block_older_than_undo_history, "Peer is on a fork I'm unable to switch to");
        FC_THROW("Peer is on a fork I'm unable to switch to");
      }
    }
    else
    {
      synopsis.insert(synopsis.begin(), read_block_id);
    }
  }
  return synopsis;
}

std::vector<block_id_type> fork_db_block_reader::get_block_ids(
  const std::vector<block_id_type>& blockchain_synopsis, uint32_t& remaining_item_count,
  uint32_t limit) const
{
  uint32_t first_block_num_in_reply;
  uint32_t last_block_num_in_reply;
  uint32_t last_block_from_block_log_in_reply;
  shared_ptr<fork_item> head;
  uint32_t head_block_num;
  vector<block_id_type> result;

  // get and hold a fork database lock so a fork switch can't happen while we're in the middle of creating
  // this list of block ids
  _fork_db.with_read_lock([&]() {
    remaining_item_count = 0;
    head = _fork_db.head_unlocked();
    if (!head)
      return;
    head_block_num = head->get_block_num();

    block_id_type last_known_block_id;
    if (blockchain_synopsis.empty() ||
        (blockchain_synopsis.size() == 1 && blockchain_synopsis[0] == block_id_type()))
    {
      // peer has sent us an empty synopsis meaning they have no blocks.
      // A bug in old versions would cause them to send a synopsis containing block 000000000
      // when they had an empty blockchain, so pretend they sent the right thing here.
      // do nothing, leave last_known_block_id set to zero
    }
    else
    {
      bool found_a_block_in_synopsis = false;
      for (const block_id_type& block_id_in_synopsis : boost::adaptors::reverse(blockchain_synopsis))
        if (block_id_in_synopsis == block_id_type() || is_included_block_unlocked(block_id_in_synopsis))
        {
          last_known_block_id = block_id_in_synopsis;
          found_a_block_in_synopsis = true;
          break;
        }

      if (!found_a_block_in_synopsis)
        FC_THROW_EXCEPTION(internal_peer_is_on_an_unreachable_fork, "Unable to provide a list of blocks starting at any of the blocks in peer's synopsis");
    }

    // the list will be composed of block ids from the block_log first, followed by ones from the fork database.
    // when building our reply, we'll fill in the ones from the fork_database first, so we can release the
    // fork_db lock, then we'll grab the ids from the block_log at our leisure.
    first_block_num_in_reply = block_header::num_from_id(last_known_block_id);
    if (first_block_num_in_reply == 0)
      ++first_block_num_in_reply;
    last_block_num_in_reply = std::min(head_block_num, first_block_num_in_reply + limit - 1);
    uint32_t result_size = last_block_num_in_reply - first_block_num_in_reply + 1;

    result.resize(result_size);

    uint32_t oldest_block_num_in_forkdb = _fork_db.get_oldest_block_num_unlocked();
    last_block_from_block_log_in_reply = std::min(oldest_block_num_in_forkdb - 1, last_block_num_in_reply);

    uint32_t first_block_num_from_fork_db_in_reply = std::max(oldest_block_num_in_forkdb, first_block_num_in_reply);
    //idump((first_block_num_in_reply)(last_block_from_block_log_in_reply)(first_block_num_from_fork_db_in_reply)(last_block_num_in_reply));

    for (uint32_t block_num = first_block_num_from_fork_db_in_reply;
         block_num <= last_block_num_in_reply;
         ++block_num)
    {
      shared_ptr<fork_item> item_from_forkdb = _fork_db.fetch_block_on_main_branch_by_number_unlocked(block_num);
      assert(item_from_forkdb);
      uint32_t index_in_result = block_num - first_block_num_in_reply;
      result[index_in_result] = item_from_forkdb->get_block_id();
    }
  }); // drop the forkdb lock

  if (!head)
  {
    remaining_item_count = 0;
    return result;
  }

  for (uint32_t block_num = first_block_num_in_reply;
       block_num <= last_block_from_block_log_in_reply;
       ++block_num)
  {
    uint32_t index_in_result = block_num - first_block_num_in_reply;
    result[index_in_result] = get_block_id_for_num(block_num);
  }

  if (!result.empty() && block_header::num_from_id(result.back()) < head_block_num)
    remaining_item_count = head_block_num - last_block_num_in_reply;
  else
    remaining_item_count = 0;

  return result;
}

// requires forkdb read lock, does not require chainbase lock
bool fork_db_block_reader::is_included_block_unlocked(const block_id_type& block_id) const
{ 
  try {
    uint32_t block_num = block_header::num_from_id(block_id);
    if (block_num == 0)
      return block_id == block_id_type();

    // See if fork DB has the item
    shared_ptr<fork_item> fitem = _fork_db.fetch_block_on_main_branch_by_number_unlocked(block_num);
    if (fitem)
      return block_id == fitem->get_block_id();


    // Next we check if block_log has it. Irreversible blocks are here.
    auto read_block_id = get_block_id_for_num(block_num);
    return block_id == read_block_id;
  } FC_CAPTURE_AND_RETHROW()
}

block_id_type fork_db_block_reader::get_block_id_for_num( uint32_t block_num ) const
{
  block_id_type read_block_id;
  try
  {
    read_block_id = _log_reader.find_block_id_for_num( block_num );
  }
  catch (fc::key_not_found_exception&)
  { /*leave read_block_id empty*/ }

  return read_block_id;
}

} } //hive::chain
