#include "hive/plugins/sql_serializer/block_num_rendezvous_trigger.hpp"

#include "fc/exception/exception.hpp"

namespace hive { namespace plugins { namespace sql_serializer {
  block_num_rendezvous_trigger::block_num_rendezvous_trigger( uint32_t _number_of_threads, TRIGGERRED_FUNCTION _triggered_function )
  : m_number_of_threads( _number_of_threads )
  , m_triggered_function( std::move( _triggered_function ) )
  {
    if ( m_number_of_threads < 1 ) {
      FC_THROW( "Incorrect number of threads" );
    }

    if ( !m_triggered_function ) {
      FC_THROW( "No trigger function" );
    }
  }


  void
  block_num_rendezvous_trigger::report_complete_thread_stage( BLOCK_NUM _stage_block_num ) {
    std::lock_guard< std::mutex > lock( m_mutex );
    if ( m_already_commited_blocks >= _stage_block_num )
      return;

    if ( m_number_of_threads == 1 )
      m_triggered_function( _stage_block_num );

    auto stage_it = m_completed_threads.find( _stage_block_num );
    if ( stage_it == m_completed_threads.end() ) {
      m_completed_threads.emplace( _stage_block_num, 1 );
      return;
    }

    if ( ( stage_it->second + 1 ) == m_number_of_threads ) {
      m_completed_threads.erase( stage_it );
      m_triggered_function( _stage_block_num );
      m_already_commited_blocks = _stage_block_num;
      ilog( "Dump to blocks ${i}", ("i", _stage_block_num) );
      return;
    }

    ++stage_it->second;
  }
}}} // namespace hive::plugins::sql_serializer
