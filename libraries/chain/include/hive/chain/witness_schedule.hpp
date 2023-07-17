#pragma once

namespace hive { namespace chain {

class database;

void update_witness_schedule( database_i& db );
void reset_virtual_schedule_time( database_i& db );

} }
