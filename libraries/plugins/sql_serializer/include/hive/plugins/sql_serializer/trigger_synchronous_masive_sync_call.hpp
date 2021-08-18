#pragma once

#include <functional>
#include <mutex>
#include <unordered_map>

namespace hive::plugins::sql_serializer {

  /**
   *  The class is responsible to call hive.end_massive_sync when given number of threads confirm that they
   *  finish commititing data for the same batch of blocks
   *  - _number_of_threads how many threads need to report about commit the same batch of blocks
   *  - _triggered_function function which needs to be call when number of threads confirms finishing batch of blocks
   */
class trigger_synchronous_masive_sync_call {
public:
  using BLOCK_NUM = uint32_t;
  using NUMBER_OF_COMPLETED_THREADS = uint32_t;
  using TRIGGERRED_FUNCTION = std::function< void(BLOCK_NUM) >;

  trigger_synchronous_masive_sync_call( uint32_t _number_of_threads, TRIGGERRED_FUNCTION _triggered_function );
  trigger_synchronous_masive_sync_call( trigger_synchronous_masive_sync_call&& ) = delete;
  trigger_synchronous_masive_sync_call( trigger_synchronous_masive_sync_call& ) = delete;
  trigger_synchronous_masive_sync_call& operator=( trigger_synchronous_masive_sync_call& ) = delete;
  trigger_synchronous_masive_sync_call& operator=( trigger_synchronous_masive_sync_call&& ) = delete;
  ~trigger_synchronous_masive_sync_call() = default;

  void report_complete_thread_stage( BLOCK_NUM _stage_block_num );
private:
  const NUMBER_OF_COMPLETED_THREADS m_number_of_threads;
  TRIGGERRED_FUNCTION m_triggered_function;
  std::unordered_map< BLOCK_NUM, NUMBER_OF_COMPLETED_THREADS > m_completed_threads;
  std::mutex m_mutex;
  BLOCK_NUM m_already_commited_blocks = 0;
};

} // namespace hive::plugins::sql_serializer
