
#include <hive/chain/hive_object_types.hpp>

#include <hive/chain/index.hpp>

#include <hive/chain/block_summary_object.hpp>
#include <hive/chain/pending_required_action_object.hpp>
#include <hive/chain/pending_optional_action_object.hpp>
#include <hive/chain/smt_objects.hpp>
#include <hive/chain/hive_objects.hpp>
#include <hive/chain/transaction_object.hpp>
#include <hive/chain/witness_schedule.hpp>

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

void initialize_core_indexes( database& db )
{
  wlog("mtlk revision()= ${rev}", ("rev", db.revision()));
  initialize_core_indexes_01( db );
  wlog("mtlk revision()= ${rev}", ("rev", db.revision()));
  initialize_core_indexes_02( db );
  wlog("mtlk revision()= ${rev}", ("rev", db.revision()));
  initialize_core_indexes_03( db );
  wlog("mtlk revision()= ${rev}", ("rev", db.revision()));
  initialize_core_indexes_04( db );
  wlog("mtlk revision()= ${rev}", ("rev", db.revision()));
  initialize_core_indexes_05( db );
  wlog("mtlk revision()= ${rev}", ("rev", db.revision()));
  initialize_core_indexes_06( db );
  wlog("mtlk revision()= ${rev}", ("rev", db.revision()));
  initialize_core_indexes_07( db );
  wlog("mtlk revision()= ${rev}", ("rev", db.revision()));
  initialize_core_indexes_08( db );
  wlog("mtlk revision()= ${rev}", ("rev", db.revision()));
  initialize_core_indexes_09( db );
  wlog("mtlk revision()= ${rev}", ("rev", db.revision()));
  initialize_core_indexes_10( db );
  wlog("mtlk revision()= ${rev}", ("rev", db.revision()));
  initialize_core_indexes_11( db );
  wlog("mtlk revision()= ${rev}", ("rev", db.revision()));
}

index_info::index_info() {}
index_info::~index_info() {}

} }
