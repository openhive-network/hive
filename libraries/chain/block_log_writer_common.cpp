#include <hive/chain/block_log_writer_common.hpp>

#include <hive/chain/block_log.hpp>
#include <hive/chain/database.hpp> // open_args needed

namespace hive { namespace chain {

uint64_t block_log_writer_common::append(const std::shared_ptr<full_block_type>& full_block, const bool is_at_live_sync)
{
  return get_log_for_new_block().append( full_block, is_at_live_sync );
}

void block_log_writer_common::flush()
{
  get_head_block_log().flush();
}

void block_log_writer_common::open_and_init( const open_args& db_open_args,
  blockchain_worker_thread_pool& thread_pool )
{
  get_head_block_log().open_and_init( db_open_args.data_dir / "block_log",
                                      db_open_args.enable_block_log_compression,
                                      db_open_args.block_log_compression_level,
                                      db_open_args.enable_block_log_auto_fixing,
                                      thread_pool );
}

void block_log_writer_common::close()
{
  get_head_block_log().close();
}

} } //hive::chain
