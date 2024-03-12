#include <hive/chain/block_log_writer_common.hpp>

#include <hive/chain/block_log.hpp>

namespace hive { namespace chain {

uint64_t block_log_writer_common::append(const std::shared_ptr<full_block_type>& full_block, const bool is_at_live_sync)
{
  return get_log_for_new_block().append( full_block, is_at_live_sync );
}

void block_log_writer_common::flush()
{
  get_head_block_log().flush();
}

void block_log_writer_common::close()
{
  get_head_block_log().close();
}

} } //hive::chain
