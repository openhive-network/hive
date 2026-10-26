#pragma once

#include <fc/time.hpp>
#include <fc/optional.hpp>
#include <fc/variant.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/exception/exception.hpp>
#include <fc/io/json.hpp>

#include <fstream>

#include <sys/time.h>

namespace hive { namespace utilities {

/**
  * Time and memory usage measuring tool.
  * Call \see initialize when you're ready to start the measurements.
  * Call \see measure as many times you need.
  * Call \see dump to collect the summary of the phase of work that has just ended.
  * When file name is passed to \see initialize, every measurement made by \see measure or \see dump is immediately
  * written into that file as single line of json (jsonl style), so nothing has to be accumulated in memory.
  * The values of \see measure and \see dump are returned if you need further processing (e.g. pretty print to console).
  */
class benchmark_dumper
{
public:
  /// Phase of work that summary closes (initial covers everything up to the moment the first block is about to be applied)
  enum class summary_type { initial, replay, sync, live };

  struct counter_t
  {
    uint64_t count = 0;
    uint64_t time_ns = 0; // cumulative time in ns
  };
  struct comment_archive_details_t
  {
    counter_t comment_accessed_from_index;
    counter_t comment_accessed_from_archive;
    counter_t comment_not_found;
    counter_t comment_cashout_processing;
    counter_t comment_lib_processing;
  };

  struct index_memory_details_t
  {
    index_memory_details_t(std::string&& name, size_t size, size_t i_sizeof,
      size_t item_add_allocation, size_t add_container_allocation)
      : index_name(name), index_size(size), item_sizeof(i_sizeof),
        item_additional_allocation(item_add_allocation),
        additional_container_allocation(add_container_allocation)
    {
      total_index_mem_usage = additional_container_allocation;
      total_index_mem_usage += item_additional_allocation;
      total_index_mem_usage += index_size*item_sizeof;
    }

    std::string    index_name;
    size_t         index_size = 0;
    size_t         item_sizeof = 0;
    /// Additional (ie dynamic container) allocations held in all stored items 
    size_t         item_additional_allocation = 0;
    /// Additional memory used for container internal structures (like tree nodes).
    size_t         additional_container_allocation = 0;
    size_t         total_index_mem_usage = 0;
  };

  typedef std::vector<index_memory_details_t> index_memory_details_cntr_t;

  class measurement
  {
  public:
    void set( uint32_t bn, int64_t rm, int32_t cs, uint64_t cm, uint64_t pm, uint64_t sf )
    {
      block_number = bn;
      real_ms = rm;
      cpu_ms = cs;
      current_mem = cm;
      peak_mem = pm;
      shm_free = sf;
    }

  public:
    /// Set only in summaries made by \see dump - their times and comment archive stats cover whole phase (since previous
    /// summary), while in regular measurements made by \see measure they cover time since previous measurement of any kind.
    fc::optional< summary_type > summary;
    uint32_t block_number = 0;
    int64_t  real_ms = 0;
    int32_t  cpu_ms = 0;
    uint64_t current_mem = 0; // in kB
    uint64_t peak_mem = 0; // in kB
    uint64_t shm_free = 0; // in kB

    comment_archive_details_t   comment_archive_stats;
    index_memory_details_cntr_t index_memory_details_cntr;
  };

  typedef std::function<void(index_memory_details_cntr_t&, comment_archive_details_t&, uint64_t&)> get_stat_details_t;

  bool is_initialized() const { return _last_summary_point.sys_time != fc::time_point{}; }

  /// When file name is given, the file is truncated and all subsequent measurements are written into it.
  void initialize( const fc::string& file_name = fc::string() );

  const measurement& measure( uint32_t block_number, get_stat_details_t get_stat_details );
  const measurement& dump( summary_type summary, uint32_t block_number, get_stat_details_t get_stat_details );

private:
  /// Moment measurements are counted from
  struct starting_point_t
  {
    fc::time_point            sys_time;
    clock_t                   cpu_time = 0;
    comment_archive_details_t comment_archive_stats;
  };

  /// Fills data with values since given point, returns current point
  starting_point_t collect( measurement& data, uint32_t block_number, const get_stat_details_t& get_stat_details,
    const starting_point_t& since );
  bool read_mem(pid_t pid, uint64_t* current_virtual, uint64_t* peak_virtual);
  void write_to_file( const measurement& data );
  void validate_is_initialized() const { FC_ASSERT( is_initialized(), "dumper is not initialized, first call initialize()" ); }

private:
  fc::string       _file_name{};
  std::ofstream    _file;
  pid_t            _pid = 0;
  starting_point_t _last_measurement_point; // previous measurement of any kind
  starting_point_t _last_summary_point;
  measurement      _last_measurement;
  measurement      _last_summary;
};

} } // hive::utilities

FC_REFLECT_ENUM( hive::utilities::benchmark_dumper::summary_type, (initial)(replay)(sync)(live) )

FC_REFLECT( hive::utilities::benchmark_dumper::counter_t,
        (count)(time_ns) )
FC_REFLECT( hive::utilities::benchmark_dumper::comment_archive_details_t,
        (comment_accessed_from_index)(comment_accessed_from_archive)(comment_not_found)
        (comment_cashout_processing)(comment_lib_processing) )

FC_REFLECT( hive::utilities::benchmark_dumper::index_memory_details_t,
        (index_name)(index_size)(item_sizeof)(item_additional_allocation)
        (additional_container_allocation)(total_index_mem_usage) )

FC_REFLECT( hive::utilities::benchmark_dumper::measurement,
        (summary)(block_number)(real_ms)(cpu_ms)(current_mem)(peak_mem)(shm_free)(comment_archive_stats)(index_memory_details_cntr) )
