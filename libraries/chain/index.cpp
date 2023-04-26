#include <hive/chain/index.hpp>

namespace hive { namespace chain {

void initialize_core_indexes_01( database& db );
void initialize_core_indexes_02( database& db );
void initialize_core_indexes_03( database& db );
void initialize_core_indexes_04( database& db );
void initialize_core_indexes_05( database& db );
void initialize_core_indexes_06( database& db );
void initialize_core_indexes_07( database& db );
void initialize_core_indexes_08( database& db );
void initialize_core_indexes_09( database& db );
void initialize_core_indexes_10( database& db );
void initialize_core_indexes_11( database& db );
void initialize_core_indexes_12( database& db );
void initialize_core_indexes_13( database& db );

void initialize_core_indexes( database& db )
{
  initialize_core_indexes_01( db );
  initialize_core_indexes_02( db );
  initialize_core_indexes_03( db );
  initialize_core_indexes_04( db );
  initialize_core_indexes_05( db );
  initialize_core_indexes_06( db );
  initialize_core_indexes_07( db );
  initialize_core_indexes_08( db );
  initialize_core_indexes_09( db );
  initialize_core_indexes_10( db );
  initialize_core_indexes_11( db );
  initialize_core_indexes_12( db );
  initialize_core_indexes_13( db );
}

index_info::index_info() {}
index_info::~index_info() {}

} }
