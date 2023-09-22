#include <hive/chain/fork_db_block_reader.hpp>

#include <hive/chain/fork_database.hpp>

namespace hive { namespace chain {

fork_db_block_reader::fork_db_block_reader( const fork_database& fork_db, block_log& the_log )
  : block_log_reader( the_log ), _fork_db( fork_db )
{}

bool fork_db_block_reader::is_known_block(const block_id_type& id) const
{
  try {
    if( _fork_db.fetch_block( id ) )
      return true;

    return block_log_reader::is_known_block( id );
  } FC_CAPTURE_AND_RETHROW()
}

} } //hive::chain

