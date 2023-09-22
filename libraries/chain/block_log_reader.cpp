#include <hive/chain/block_log_reader.hpp>

#include <hive/chain/block_log.hpp>

namespace hive { namespace chain {

block_log_reader::block_log_reader( block_log& block_log )
  : _block_log( block_log )
{}

std::shared_ptr<full_block_type> block_log_reader::head() const
{
  return _block_log.head();
}

std::shared_ptr<full_block_type> block_log_reader::read_block_by_num( uint32_t block_num ) const
{
  return _block_log.read_block_by_num( block_num );
}

void block_log_reader::process_blocks(uint32_t starting_block_number, uint32_t ending_block_number,
  block_processor_t processor)
{
  _block_log.for_each_block( starting_block_number, ending_block_number, processor, 
                             block_log::for_each_purpose::replay );
}

bool block_log_reader::is_known_block(const block_id_type& id) const
{
  try {
    auto requested_block_num = protocol::block_header::num_from_id(id);
    auto read_block_id = _block_log.read_block_id_by_num(requested_block_num);

    return read_block_id != block_id_type() && read_block_id == id;
  } FC_CAPTURE_AND_RETHROW()
}

} } //hive::chain

