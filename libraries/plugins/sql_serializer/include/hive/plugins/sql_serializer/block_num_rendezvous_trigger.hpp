#pragma once

#include <functional>
#include <mutex>
#include <unordered_map>

namespace hive::plugins::sql_serializer {

  /**
   *  The class is responsible to call TRIGGERRED_FUNCTION when given number of threads confirm that they
   *  finish commititing data for the same batch of blocks
   *  - _number_of_threads how many threads need to report about commit the same batch of blocks
   *  - _triggered_function function which needs to be call when number of threads confirms finishing batch of blocks
   */
class block_num_rendezvous_trigger {
public:
  using BLOCK_NUM = uint32_t;
  using NUMBER_OF_COMPLETED_THREADS = uint32_t;
  using TRIGGERRED_FUNCTION = std::function< void(BLOCK_NUM) >;

  block_num_rendezvous_trigger( NUMBER_OF_COMPLETED_THREADS _number_of_threads, TRIGGERRED_FUNCTION _triggered_function );
  block_num_rendezvous_trigger( block_num_rendezvous_trigger&& ) = delete;
  block_num_rendezvous_trigger( block_num_rendezvous_trigger& ) = delete;
  block_num_rendezvous_trigger& operator=( block_num_rendezvous_trigger& ) = delete;
  block_num_rendezvous_trigger& operator=( block_num_rendezvous_trigger&& ) = delete;
  ~block_num_rendezvous_trigger() = default;

  void report_complete_thread_stage( BLOCK_NUM _stage_block_num );
private:
  const NUMBER_OF_COMPLETED_THREADS m_number_of_threads;
  TRIGGERRED_FUNCTION m_triggered_function;
  std::unordered_map< BLOCK_NUM, NUMBER_OF_COMPLETED_THREADS > m_completed_threads;
  std::mutex m_mutex;
  BLOCK_NUM m_already_commited_blocks = 0;
};

} // namespace hive::plugins::sql_serializer
