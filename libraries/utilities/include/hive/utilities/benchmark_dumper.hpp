#pragma once

#include <fc/time.hpp>
#include <fc/variant.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/exception/exception.hpp>
#include <fc/io/json.hpp>

#include <sys/time.h>

namespace hive { namespace utilities {

/**
  * Time and memory usage measuring tool.
  * Call \see initialize when you're ready to start the measurements.
  * Call \see measure as many times you need.
  * Call \see dump to print the measurements into the file json style.
  * The values of \see measure and \see dump are returned if you need further processing (e.g. pretty print to console).
  */
class benchmark_dumper
{
public:
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

  struct account_archive_details_t
  {
    counter_t account_metadata_created;
    counter_t account_metadata_modified;
    counter_t account_metadata_accessed_by_name;

    counter_t account_authority_created;
    counter_t account_authority_modified;
    counter_t account_authority_accessed_by_name;
    
    counter_t account_created;
    counter_t account_modified;
    counter_t account_accessed_by_name;
    counter_t account_accessed_by_id;

    counter_t item_moved_to_storage;
    counter_t item_flush_to_storage;

    counter_t compact_storage;
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
    uint32_t block_number = 0;
    int64_t  real_ms = 0;
    int32_t  cpu_ms = 0;
    uint64_t current_mem = 0; // in kB
    uint64_t peak_mem = 0; // in kB
    uint64_t shm_free = 0; // in kB

    comment_archive_details_t   comment_archive_stats;
    account_archive_details_t   account_archive_stats;
    index_memory_details_cntr_t index_memory_details_cntr;
  };

  typedef std::vector<measurement> TMeasurements;

  struct TAllData
  {
    TMeasurements                 measurements;
    measurement                   total_measurement;
  };

  typedef std::function<void(index_memory_details_cntr_t&, comment_archive_details_t&, account_archive_details_t&, uint64_t&)> get_stat_details_t;

  bool is_initialized() const { return _init_sys_time != fc::time_point{}; }

  void initialize( const char* file_name )
  {
    _file_name = file_name;
    _init_sys_time = _last_sys_time = fc::time_point::now();
    _init_cpu_time = _last_cpu_time = clock();
    _pid = getpid();
  }

  const measurement& measure( uint32_t block_number, get_stat_details_t get_stat_details );
  const measurement& dump( bool finalMeasure, uint32_t block_number, get_stat_details_t get_stat_details );

private:
  bool read_mem(pid_t pid, uint64_t* current_virtual, uint64_t* peak_virtual);
  bool is_file_available() const { return !_file_name.empty(); }
  void validate_is_initialized() const { FC_ASSERT( is_initialized(), "dumper is not initialized, first call initialize()" ); }

private:
  fc::string     _file_name{};
  fc::time_point _init_sys_time;
  fc::time_point _last_sys_time;
  clock_t        _init_cpu_time = 0;
  clock_t        _last_cpu_time = 0;
  uint64_t       _total_blocks = 0;
  pid_t          _pid = 0;
  TAllData       _all_data;
};

} } // hive::utilities

FC_REFLECT( hive::utilities::benchmark_dumper::counter_t,
        (count)(time_ns) )
FC_REFLECT( hive::utilities::benchmark_dumper::comment_archive_details_t,
        (comment_accessed_from_index)(comment_accessed_from_archive)(comment_not_found)
        (comment_cashout_processing)(comment_lib_processing) )
FC_REFLECT( hive::utilities::benchmark_dumper::account_archive_details_t,
        (account_metadata_created)(account_metadata_modified)(account_metadata_accessed_by_name)
        (account_authority_created)(account_authority_modified)(account_authority_accessed_by_name)
        (account_created)(account_modified)
        (account_accessed_by_name)(account_accessed_by_id)
        (item_moved_to_storage)
        (item_flush_to_storage)
        (compact_storage)
       )

FC_REFLECT( hive::utilities::benchmark_dumper::index_memory_details_t,
        (index_name)(index_size)(item_sizeof)(item_additional_allocation)
        (additional_container_allocation)(total_index_mem_usage) )

FC_REFLECT( hive::utilities::benchmark_dumper::measurement,
        (block_number)(real_ms)(cpu_ms)(current_mem)(peak_mem)(shm_free)(comment_archive_stats)(account_archive_stats)(index_memory_details_cntr) )

FC_REFLECT( hive::utilities::benchmark_dumper::TAllData,
        (measurements)(total_measurement) )
