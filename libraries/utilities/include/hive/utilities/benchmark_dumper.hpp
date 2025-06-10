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
  * Call \see initialize when you're ready to start the measurments.
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
    uint64_t time = 0; // cumulative in ns
    uint64_t avg_time_ns = 0; // only updated on write to report
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

  struct database_object_sizeof_t
  {
    database_object_sizeof_t(std::string&& name, size_t size)
      : object_name(name), object_size(size) {}

    std::string    object_name;
    size_t         object_size = 0;
  };

  typedef std::vector<database_object_sizeof_t> database_object_sizeof_cntr_t;

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
    index_memory_details_cntr_t index_memory_details_cntr;
  };

  typedef std::vector<measurement> TMeasurements;

  struct TAllData
  {
    database_object_sizeof_cntr_t database_object_sizeofs;
    TMeasurements                 measurements;
    measurement                   total_measurement;
  };

  typedef std::function<void(index_memory_details_cntr_t&, comment_archive_details_t&, uint64_t&)> get_stat_details_t;
  typedef std::function<void(database_object_sizeof_cntr_t&)> get_database_objects_sizeofs_t;

  bool is_initialized() const { return _init_sys_time != fc::time_point{}; }

  void initialize(get_database_objects_sizeofs_t get_database_objects_sizeofs, const char* file_name)
  {
    _file_name = file_name;
    _init_sys_time = _last_sys_time = fc::time_point::now();
    _init_cpu_time = _last_cpu_time = clock();
    _pid = getpid();
    get_database_objects_sizeofs(_all_data.database_object_sizeofs);
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
        (count)(avg_time_ns) ) // note: total time (in nanoseconds) is not printed
FC_REFLECT( hive::utilities::benchmark_dumper::comment_archive_details_t,
        (comment_accessed_from_index)(comment_accessed_from_archive)(comment_not_found)
        (comment_cashout_processing)(comment_lib_processing) )

FC_REFLECT( hive::utilities::benchmark_dumper::index_memory_details_t,
        (index_name)(index_size)(item_sizeof)(item_additional_allocation)
        (additional_container_allocation)(total_index_mem_usage) )

FC_REFLECT( hive::utilities::benchmark_dumper::database_object_sizeof_t,
        (object_name)(object_size) )

FC_REFLECT( hive::utilities::benchmark_dumper::measurement,
        (block_number)(real_ms)(cpu_ms)(current_mem)(peak_mem)(shm_free)(comment_archive_stats)(index_memory_details_cntr) )

FC_REFLECT( hive::utilities::benchmark_dumper::TAllData,
        (database_object_sizeofs)(measurements)(total_measurement) )
