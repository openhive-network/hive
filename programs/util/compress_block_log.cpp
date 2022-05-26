#include <fc/log/logger.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/filesystem.hpp>
#include <hive/chain/block_log.hpp>
#include <hive/chain/block_compression_dictionaries.hpp>

#include <boost/thread/future.hpp>
#include <boost/program_options.hpp>

#include <thread>
#include <chrono>
#include <memory>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <mutex>
#include <condition_variable>
#include <queue>

#ifndef ZSTD_STATIC_LINKING_ONLY
# define ZSTD_STATIC_LINKING_ONLY
#endif
#include <zstd.h>

#undef dlog
#define dlog(...) do {} while(0)

struct block_to_compress
{
  uint32_t block_number;
  size_t uncompressed_block_size = 0;
  std::unique_ptr<char[]> uncompressed_block_data;
};

struct compressed_block
{
  boost::promise<void> compression_complete_promise;

  uint32_t block_number;
  size_t compressed_block_size = 0;
  std::unique_ptr<char[]> compressed_block_data;
  hive::chain::block_log::block_attributes_t attributes;
};

bool enable_zstd = true;
fc::optional<int> zstd_level;

uint32_t starting_block_number = 1;
fc::optional<uint32_t> blocks_to_compress;
fc::optional<fc::path> raw_block_output_path;
bool benchmark_decompression = false;

std::mutex queue_mutex;
std::condition_variable queue_condition_variable;
std::queue<block_to_compress*> pending_queue;
std::queue<compressed_block*> completed_queue;
bool all_blocks_enqueued = false;

uint64_t total_compressed_size = 0;
uint64_t total_uncompressed_size = 0;
uint64_t size_of_start_positions = 0;
uint64_t total_zstd_size = 0;
fc::microseconds total_zstd_compression_time;
fc::microseconds total_zstd_decompression_time;
std::map<hive::chain::block_log::block_flags, uint32_t> total_count_by_method;

const uint32_t blocks_to_prefetch = 100;
const uint32_t max_completed_queue_size = 100;

void compress_blocks()
{
  // each compression thread gets its own context
  ZSTD_CCtx* zstd_compression_context = ZSTD_createCCtx();
  ZSTD_DCtx* zstd_decompression_context = ZSTD_createDCtx();

  while (true)
  {
    // wait until there is work in the pending queue
    block_to_compress* uncompressed = nullptr;
    compressed_block* compressed = nullptr;
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      while ((pending_queue.empty() && !all_blocks_enqueued) || (completed_queue.size() >= max_completed_queue_size))
        queue_condition_variable.wait(lock);
      if (pending_queue.empty() && all_blocks_enqueued)
      {
        dlog("No more blocks to compress, exiting worker thread");
        return;
      }
      
      // remove the block from the pending queue and put a record on the
      // compressed queue where we will put the compressed data
      uncompressed = pending_queue.front();
      pending_queue.pop();
      compressed = new compressed_block;
      compressed->block_number = uncompressed->block_number;
      completed_queue.push(compressed);
    }
    queue_condition_variable.notify_all();

    dlog("compression worker beginning work on block ${block_number}", ("block_number", uncompressed->block_number));

    // we'll compress using every available method, then choose the best
    struct compressed_data
    {
      size_t size;
      std::unique_ptr<char[]> data;
      hive::chain::block_log::block_flags method;
      fc::optional<uint8_t> dictionary_number;
    };
    std::vector<compressed_data> compressed_versions;

    fc::optional<uint8_t> dictionary_number_to_use = hive::chain::get_best_available_zstd_compression_dictionary_number_for_block(uncompressed->block_number);

    // zstd
    if (enable_zstd)
      try
      {
        compressed_data zstd_compressed_data;
        fc::time_point before = fc::time_point::now();
        //idump((uncompressed->block_number)(uncompressed->uncompressed_block_size));
        std::tie(zstd_compressed_data.data, zstd_compressed_data.size) = hive::chain::block_log::compress_block_zstd(uncompressed->uncompressed_block_data.get(), uncompressed->uncompressed_block_size, dictionary_number_to_use, zstd_level, zstd_compression_context);
        //idump((fc::to_hex(zstd_compressed_data.data.get(), zstd_compressed_data.size))(uncompressed->uncompressed_block_size)(zstd_compressed_data.size));
        //idump((zstd_compressed_data.size));

        fc::time_point after_compress = fc::time_point::now();
        if (benchmark_decompression)
          hive::chain::block_log::decompress_block_zstd(zstd_compressed_data.data.get(), zstd_compressed_data.size, dictionary_number_to_use, zstd_decompression_context);
        fc::time_point after_decompress = fc::time_point::now();
        zstd_compressed_data.method = hive::chain::block_log::block_flags::zstd;
        zstd_compressed_data.dictionary_number = dictionary_number_to_use;

        {
          std::unique_lock<std::mutex> lock(queue_mutex);
          total_zstd_size += zstd_compressed_data.size;
          total_zstd_compression_time += after_compress - before;
          total_zstd_decompression_time += after_decompress - after_compress;
        }
        compressed_versions.push_back(std::move(zstd_compressed_data));
      }
      catch (const fc::exception& e)
      {
        elog("Error compressing block ${block_number} with zstd: ${e}", ("block_number", uncompressed->block_number)(e));
      }

    // sort by size
    std::sort(compressed_versions.begin(), compressed_versions.end(), 
              [](const compressed_data& lhs, const compressed_data& rhs) { return lhs.size < rhs.size; });

    if (!compressed_versions.empty())
    {
      // if the smallest compressed version is smaller than the uncompressed version, use it
      if (compressed_versions.front().size < uncompressed->uncompressed_block_size)
      {
        ++total_count_by_method[compressed_versions.front().method];
        compressed->attributes.flags = compressed_versions.front().method;
        compressed->attributes.dictionary_number = compressed_versions.front().dictionary_number;
        compressed->compressed_block_size = compressed_versions.front().size;
        compressed->compressed_block_data = std::move(compressed_versions.front().data);
      }
      else
      {
        ++total_count_by_method[hive::chain::block_log::block_flags::uncompressed];
        compressed->compressed_block_size = uncompressed->uncompressed_block_size;
        compressed->compressed_block_data = std::move(uncompressed->uncompressed_block_data);
        compressed->attributes.flags = hive::chain::block_log::block_flags::uncompressed;
      }
    }
    else
    {
      ++total_count_by_method[hive::chain::block_log::block_flags::uncompressed];
      compressed->compressed_block_size = uncompressed->uncompressed_block_size;
      compressed->compressed_block_data = std::move(uncompressed->uncompressed_block_data);
      compressed->attributes.flags = hive::chain::block_log::block_flags::uncompressed;
    }

    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      total_compressed_size += compressed->compressed_block_size;
      total_uncompressed_size += uncompressed->uncompressed_block_size;
      size_of_start_positions += sizeof(uint64_t);
    }

    dlog("compression worker completed work on block ${block_number}, ${uncompressed} -> ${compressed} bytes", 
         ("block_number", uncompressed->block_number)
         ("compressed", compressed->compressed_block_size)
         ("uncompressed", uncompressed->uncompressed_block_size));

    delete uncompressed;
    compressed->compression_complete_promise.set_value();
  }
}

void drain_completed_queue(const fc::path& block_log)
{
  try
  {
    hive::chain::block_log log;
    log.open(block_log);
    ilog("Opened output block log");
    if (log.head())
    {
      elog("Error: output block log is not empty");
      exit(1);
    }
    while (true)
    {
      compressed_block* compressed = nullptr;
      {
        std::unique_lock<std::mutex> lock(queue_mutex);
        while (pending_queue.empty() && completed_queue.empty() && !all_blocks_enqueued)
          queue_condition_variable.wait(lock);
        if (!completed_queue.empty())
        {
          // the next block we want to write out is at the head of the completed queue
          compressed = completed_queue.front();
          completed_queue.pop();
          queue_condition_variable.notify_all();
        }
        else if (pending_queue.empty() && all_blocks_enqueued)
        {
          ilog("Done draining writing compressed blocks to disk, exiting writer thread");
          break;
        }
        else
        {
          // else the completed queue is empty, but there is still data 
          // in the pending queue or blocks will be added to the pending queue, 
          // so we just wait until it appears in the completed queue
          continue;
        }
      }

      // we've dequeued the next block in the blockchain
      // wait for the compression job to finish
      dlog("writer thread waiting on block ${block_number}", ("block_number", compressed->block_number));
      compressed->compression_complete_promise.get_future().get();

      dlog("writer thread writing compressed block ${block_number} to the compressed block log", ("block_number", compressed->block_number));

      // write it out
      log.append_raw(compressed->compressed_block_data.get(), compressed->compressed_block_size, compressed->attributes);

      if (compressed->block_number % 100000 == 0)
      {
        float total_compression_ratio = 100.f * (1.f - (float)(total_compressed_size + size_of_start_positions) / (float)(total_uncompressed_size + size_of_start_positions));
        std::ostringstream total_compression_ratio_string;
        total_compression_ratio_string << std::fixed << std::setprecision(2) << total_compression_ratio;

        ilog("at block ${block_number}: total uncompressed ${input_size} compressed to ${output_size} (${total_compression_ratio}%)", 
             ("block_number", compressed->block_number)("input_size", total_uncompressed_size + size_of_start_positions)
             ("output_size", total_compressed_size + size_of_start_positions)
             ("total_compression_ratio", total_compression_ratio_string.str()));
      }


      delete compressed;
    }
    ilog("Writer thread done writing compressed blocks to ${block_log}, compressed ${input_size} to ${output_size}", 
         (block_log)("input_size", total_uncompressed_size + size_of_start_positions)
         ("output_size", total_compressed_size + size_of_start_positions));

    log.close();
  }
  FC_LOG_AND_RETHROW()
}

void fill_pending_queue(const fc::path& block_log) 
{
  try
  {
    ilog("Starting fill_pending_queue");
    hive::chain::block_log log;
    log.open(block_log, true);
    ilog("Opened source block log");

    if (!log.head())
    {
      elog("Error: input block log is empty");
      exit(1);
    }
    uint32_t head_block_num = log.head()->block_num();
    idump((head_block_num));
    if (blocks_to_compress && *blocks_to_compress > head_block_num)
    {
      elog("Error: input block log does not contain ${blocks_to_compress} blocks (it's head block number is ${head_block_num})", (blocks_to_compress)(head_block_num));
      exit(1);
    }
    
    uint32_t stop_at_block = blocks_to_compress ? starting_block_number + *blocks_to_compress - 1 : head_block_num;
    ilog("Compressing blocks ${starting_block_number} to ${stop_at_block}", (starting_block_number)(stop_at_block));

    uint32_t current_block_number = starting_block_number;

    while (current_block_number <= stop_at_block)
    {
      // wait until there is room in the pending queue
      {
        std::unique_lock<std::mutex> lock(queue_mutex);
        while (pending_queue.size() >= blocks_to_prefetch)
          queue_condition_variable.wait(lock);
      }

      // read a block
      block_to_compress* uncompressed_block = new block_to_compress;
      uncompressed_block->block_number = current_block_number;

      std::tuple<std::unique_ptr<char[]>, size_t, hive::chain::block_log::block_attributes_t> raw_compressed_block_data = current_block_number == head_block_num ? log.read_raw_head_block() : log.read_raw_block_data_by_num(current_block_number);
      std::tuple<std::unique_ptr<char[]>, size_t> raw_block_data = hive::chain::block_log::decompress_raw_block(std::move(raw_compressed_block_data));

      uncompressed_block->uncompressed_block_size = std::get<1>(raw_block_data);
      uncompressed_block->uncompressed_block_data = std::get<0>(std::move(raw_block_data));

      if (raw_block_output_path)
      {
        std::string first_subdir_name = std::to_string(current_block_number / 1000000 * 1000000);
        std::string second_subdir_name = std::to_string(current_block_number / 100000 * 100000);
        fc::path raw_block_dir = *raw_block_output_path / first_subdir_name / second_subdir_name;
        fc::create_directories(raw_block_dir);
        fc::path raw_block_filename = raw_block_dir / (fc::to_string(current_block_number) + ".bin");
        std::ofstream raw_block_stream(raw_block_filename.generic_string());
        raw_block_stream.write(uncompressed_block->uncompressed_block_data.get(), uncompressed_block->uncompressed_block_size);
      }

      // push it to the queue
      {
        std::unique_lock<std::mutex> lock(queue_mutex);
        pending_queue.push(uncompressed_block);
      }
      queue_condition_variable.notify_all();
      dlog("Pushed uncompressed block ${current_block_number}", (current_block_number));

      ++current_block_number;
    }
    dlog("All uncompressed blocks enqueued, exiting the fill_ending_queue() thread");

    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      all_blocks_enqueued = true;
    }
    queue_condition_variable.notify_all();
    log.close();
  }
  FC_LOG_AND_RETHROW()
}

int main(int argc, char** argv)
{
  try
  {
    // zstd doesn't have well-defined levels, so we get these at runtime
    std::ostringstream zstd_levels_description_stream;
    zstd_levels_description_stream << "The zstd compression level to use";
    zstd_levels_description_stream << " (" << ZSTD_minCLevel() << " - " << ZSTD_maxCLevel() << ")";
    std::string zstd_levels_description = zstd_levels_description_stream.str();

    boost::program_options::options_description options("Allowed options");
    options.add_options()("decompress", boost::program_options::bool_switch()->default_value(false), "Instead of compressing the block log, decompress it");
    options.add_options()("zstd-level", boost::program_options::value<int>()->default_value(15), zstd_levels_description.c_str());
    options.add_options()("benchmark-decompression", "decompress each block and report the decompression times at the end");
    options.add_options()("jobs,j", boost::program_options::value<int>()->default_value(1), "The number of threads to use for compression");
    options.add_options()("input-block-log,i", boost::program_options::value<std::string>()->required(), "The directory containing the input block log");
    options.add_options()("output-block-log,o", boost::program_options::value<std::string>()->required(), "The directory to contain the compressed block log");
    options.add_options()("dump-raw-blocks", boost::program_options::value<std::string>(), "A directory in which to dump raw, uncompressed blocks (one block per file)");
    options.add_options()("starting-block-number,s", boost::program_options::value<uint32_t>()->default_value(1), "Start at the given block number (for benchmarking only, values > 1 will generate an unusable block log)");
    options.add_options()("block-count,n", boost::program_options::value<uint32_t>(), "Stop after this many blocks");
    options.add_options()("help,h", "Print usage instructions");

    boost::program_options::positional_options_description positional_options;
    positional_options.add("input-block-log", 1);
    positional_options.add("output-block-log", 1);

    boost::program_options::variables_map options_map;
    boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(options).positional(positional_options).run(), options_map);

    enable_zstd = !options_map["decompress"].as<bool>();

    zstd_level = options_map["zstd-level"].as<int>();
    ilog("Compressing using zstd level ${zstd_level}", (zstd_level));

    benchmark_decompression = options_map.count("benchmark-decompression") > 0;

    unsigned jobs = options_map["jobs"].as<int>();

    starting_block_number = options_map["starting-block-number"].as<uint32_t>();
    if (options_map.count("block-count"))
      blocks_to_compress = options_map["block-count"].as<uint32_t>();

    if (options_map.count("help"))
    {
      std::cout << options << "\n";
      return 0;
    }

    if (!options_map.count("input-block-log") || !options_map.count("output-block-log"))
    {
      std::cerr << "Error: missing parameter for input-block-log or output-block-log\n";
      return 1;
    }

    fc::path input_block_log_path;
    if (options_map.count("input-block-log"))
      input_block_log_path = options_map["input-block-log"].as<std::string>();
    fc::path output_block_log_path;
    if (options_map.count("output-block-log"))
      output_block_log_path = options_map["output-block-log"].as<std::string>();
    if (options_map.count("dump-raw-blocks"))
      raw_block_output_path = options_map["dump-raw-blocks"].as<std::string>();

    std::shared_ptr<std::thread> fill_queue_thread = std::make_shared<std::thread>([&](){ fill_pending_queue(input_block_log_path / "block_log"); });
    std::shared_ptr<std::thread> drain_queue_thread = std::make_shared<std::thread>([&](){ drain_completed_queue(output_block_log_path / "block_log"); });
    std::vector<std::shared_ptr<std::thread>> compress_blocks_threads;
    for (unsigned i = 0; i < jobs; ++i)
      compress_blocks_threads.push_back(std::make_shared<std::thread>([&](){ compress_blocks(); }));
    fill_queue_thread->join();
    for (std::shared_ptr<std::thread>& compress_blocks_thread : compress_blocks_threads)
      compress_blocks_thread->join();
    drain_queue_thread->join();
    ilog("Total number of blocks by compression method:");
    uint32_t total_blocks_processed = 0;
    for (const auto& value : total_count_by_method)
    {
      ilog("    ${method}: ${count}", ("method", value.first)("count", value.second));
      total_blocks_processed += value.second;
    }
    ilog("Total bytes if all blocks compressed by compression method:");
    if (enable_zstd)
    {
      ilog("    zstd: ${total_zstd_size} bytes, total time: ${total_zstd_compression_time}μs, average time per block: ${average_zstd_time}μs", 
           ("total_zstd_size", total_zstd_size + size_of_start_positions)
           (total_zstd_compression_time)
           ("average_zstd_time", total_zstd_compression_time.count() / total_blocks_processed));
      if (benchmark_decompression)
        ilog("          decompression total time: ${total_zstd_decompression_time}μs, average time per block: ${average_zstd_decompression_time}μs", 
             ("total_zstd_size", total_zstd_size + size_of_start_positions)
             (total_zstd_decompression_time)
             ("average_zstd_decompression_time", total_zstd_decompression_time.count() / total_blocks_processed));
    }
  }
  catch ( const std::exception& e )
  {
    edump( ( std::string( e.what() ) ) );
  }

  return 0;
}
