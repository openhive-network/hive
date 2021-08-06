#include "hive/plugins/sql_serializer/end_massive_sync_processor.hpp"

#include "hive/plugins/sql_serializer/data_processor.hpp"

#include <cassert>
#include <string>

using namespace std::string_literals;

namespace hive::plugins::sql_serializer {

end_massive_sync_processor::end_massive_sync_processor( std::string psqlUrl )
{
  auto commiting_function = [this](const data_processor::data_chunk_ptr&, transaction& tx) -> data_processor::data_processing_status {
    tx.exec( "SELECT hive.end_massive_sync("s + std::to_string( _block_number ) + ")"s );

    return data_processor::data_processing_status();
  };

  _data_processor = std::make_unique< data_processor >( psqlUrl, "commiting hive.end_massive_sync", commiting_function, nullptr );
}

void
end_massive_sync_processor::trigger_block_number( uint32_t last_dumped_block ) {
  _block_number = last_dumped_block;
  _data_processor->trigger( nullptr, 0 );
}

void
end_massive_sync_processor::complete_data_processing() {
  _data_processor->complete_data_processing();
}

void
end_massive_sync_processor::join() {
  assert( _data_processor );
  _data_processor->join();
}

} // namespace hive { namespace plugins { namespace sql_serializer {
