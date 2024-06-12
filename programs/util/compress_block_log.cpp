#include <appbase/application.hpp>

#include <fc/log/logger.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/filesystem.hpp>
#include <hive/chain/block_log.hpp>
#include <hive/chain/block_log_compression.hpp>
#include <hive/chain/block_log_wrapper.hpp>
#include <hive/chain/full_block.hpp>
#include <hive/chain/block_compression_dictionaries.hpp>
#include <hive/chain/blockchain_worker_thread_pool.hpp>

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
bool use_compressed_even_when_larger = true;

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

std::atomic<bool> error_detected {false};

void compress_blocks(uint32_t& current_block_num)
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
      while (((pending_queue.empty() && !all_blocks_enqueued) || (completed_queue.size() >= max_completed_queue_size)) && !error_detected.load())
        queue_condition_variable.wait(lock);
      if (pending_queue.empty() && all_blocks_enqueued)
      {
        dlog("No more blocks to compress, exiting worker thread");
        return;
      }
      else if (error_detected.load())
        return;
      
      // remove the block from the pending queue and put a record on the
      // compressed queue where we will put the compressed data
      uncompressed = pending_queue.front();
      pending_queue.pop();
      compressed = new compressed_block;
      current_block_num = uncompressed->block_number;
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
      std::optional<uint8_t> dictionary_number;
    };
    std::vector<compressed_data> compressed_versions;

    std::optional<uint8_t> dictionary_number_to_use = hive::chain::get_best_available_zstd_compression_dictionary_number_for_block(uncompressed->block_number);

    // zstd
    if (enable_zstd)
    {
      compressed_data zstd_compressed_data;
      fc::time_point before = fc::time_point::now();
      //idump((uncompressed->block_number)(uncompressed->uncompressed_block_size));
      std::tie(zstd_compressed_data.data, zstd_compressed_data.size) = 
        hive::chain::block_log_compression::compress_block_zstd(
          uncompressed->uncompressed_block_data.get(),
          uncompressed->uncompressed_block_size,
          dictionary_number_to_use, zstd_level,
          zstd_compression_context);
      //idump((fc::to_hex(zstd_compressed_data.data.get(), zstd_compressed_data.size))(uncompressed->uncompressed_block_size)(zstd_compressed_data.size));
      //idump((zstd_compressed_data.size));

      fc::time_point after_compress = fc::time_point::now();
      if (benchmark_decompression)
        hive::chain::block_log_compression::decompress_block_zstd(zstd_compressed_data.data.get(),
          zstd_compressed_data.size, dictionary_number_to_use, zstd_decompression_context);
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

    // sort by size
    std::sort(compressed_versions.begin(), compressed_versions.end(), 
              [](const compressed_data& lhs, const compressed_data& rhs) { return lhs.size < rhs.size; });

    if (!compressed_versions.empty())
    {
      // if the smallest compressed version is smaller than the uncompressed version, use it
      if (use_compressed_even_when_larger ||
          compressed_versions.front().size < uncompressed->uncompressed_block_size)
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

void drain_completed_queue(const fc::path &output_path, uint32_t &current_block_number, appbase::application& app, hive::chain::blockchain_worker_thread_pool& thread_pool)
{
  auto log_writer = hive::chain::block_log_wrapper::create_opened_wrapper( output_path, app, thread_pool, false /*read_only*/ );
  ilog("Opened output block log");
  if (log_writer->head_block())
    FC_THROW("output block log is not empty");

  while (true)
  {
    compressed_block *compressed = nullptr;
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      while (pending_queue.empty() && completed_queue.empty() && !all_blocks_enqueued && !error_detected.load())
        queue_condition_variable.wait(lock);

      if (error_detected.load())
        return;

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

    current_block_number = compressed->block_number;
    // we've dequeued the next block in the blockchain
    // wait for the compression job to finish
    dlog("writer thread waiting on block ${block_number}", ("block_number", compressed->block_number));
    auto compressed_future = compressed->compression_complete_promise.get_future();
    while (!compressed_future.has_value())
    {
      if (error_detected.load())
        return;
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    dlog("writer thread writing compressed block ${block_number} to the compressed block log", ("block_number", compressed->block_number));

    // write it out
    log_writer->append_raw(compressed->block_number, compressed->compressed_block_data.get(), compressed->compressed_block_size, compressed->attributes, false);

    if (compressed->block_number % 100000 == 0)
    {
      float total_compression_ratio = 100.f * (1.f - (float)(total_compressed_size + size_of_start_positions) / (float)(total_uncompressed_size + size_of_start_positions));
      std::ostringstream total_compression_ratio_string;
      total_compression_ratio_string << std::fixed << std::setprecision(2) << total_compression_ratio;

      ilog("at block ${block_number}: total uncompressed ${input_size} compressed to ${output_size} (${total_compression_ratio}%)",
           ("block_number", compressed->block_number)("input_size", total_uncompressed_size + size_of_start_positions)("output_size", total_compressed_size + size_of_start_positions)("total_compression_ratio", total_compression_ratio_string.str()));
    }

    delete compressed;
  }
  if (!error_detected.load())
    ilog("Writer thread done writing compressed blocks to ${output_path}, compressed ${input_size} to ${output_size}",
        (output_path)("input_size", total_uncompressed_size + size_of_start_positions)("output_size", total_compressed_size + size_of_start_positions));

  log_writer->close_storage();
}

void fill_pending_queue(const fc::path &input_path, const bool read_only, uint32_t &current_block_number, appbase::application& app, hive::chain::blockchain_worker_thread_pool& thread_pool)
{
  ilog("Starting fill_pending_queue");
  auto log_reader = hive::chain::block_log_wrapper::create_opened_wrapper( input_path, app, thread_pool, read_only );
  ilog("Opened source block log. Readonly: ${read_only}", (read_only));

  if (!log_reader->head_block())
    FC_THROW("input block log is empty");

  uint32_t head_block_num = log_reader->head_block_num();
  idump((head_block_num));
  if (blocks_to_compress && *blocks_to_compress > head_block_num)
    FC_THROW("input block log does not contain ${blocks_to_compress} blocks (it's head block number is ${head_block_num})", (blocks_to_compress)(head_block_num));

  uint32_t stop_at_block = blocks_to_compress ? starting_block_number + *blocks_to_compress - 1 : head_block_num;
  ilog("Compressing blocks ${starting_block_number} to ${stop_at_block}", (starting_block_number)(stop_at_block));

  current_block_number = starting_block_number;

  while (current_block_number <= stop_at_block && !error_detected.load())
  {
    // wait until there is room in the pending queue
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      while (pending_queue.size() >= blocks_to_prefetch && !error_detected.load())
        queue_condition_variable.wait(lock);
    }

    if (error_detected.load())
      return;

    // read a block
    block_to_compress *uncompressed_block = new block_to_compress;
    uncompressed_block->block_number = current_block_number;

    std::tuple<std::unique_ptr<char[]>, size_t, hive::chain::block_log::block_attributes_t> raw_compressed_block_data;
    if (current_block_number == head_block_num)
      raw_compressed_block_data = log_reader->read_raw_head_block();
    else
    {
      std::tuple<std::unique_ptr<char[]>, size_t, hive::chain::block_log_artifacts::artifacts_t> data_with_artifacts = 
        log_reader->read_raw_block_data_by_num(current_block_number);
      raw_compressed_block_data = std::make_tuple(std::get<0>(std::move(data_with_artifacts)), std::get<1>(data_with_artifacts), std::get<2>(data_with_artifacts).attributes);
    }
    std::tuple<std::unique_ptr<char[]>, size_t> raw_block_data = 
      hive::chain::block_log_compression::decompress_raw_block(std::move(raw_compressed_block_data));

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

  if (!error_detected.load())
  {
    dlog("All uncompressed blocks enqueued, exiting the fill_ending_queue() thread");

    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      all_blocks_enqueued = true;
    }

    queue_condition_variable.notify_all();
    log_reader->close_storage();
  }
}

template<typename Func>
void function_wrapper(const Func& function, const std::string function_name)
{
  uint32_t block_number = 0;
  try
  {
    function(block_number);
  }
  catch(const fc::exception& e)
  {
    std::unique_lock<std::mutex> lock(queue_mutex);
    error_detected.store(true);
    queue_condition_variable.notify_all();
    elog("Error in function: ${function_name}. Block number: ${block_number}. FC error details: ${what}", (function_name)(block_number)("what", e.to_detail_string()));
  }
  catch(...)
  {
    std::unique_lock<std::mutex> lock(queue_mutex);
    error_detected.store(true);
    queue_condition_variable.notify_all();
    const auto current_exception = std::current_exception();
    if (current_exception)
    {
      try
      {
        std::rethrow_exception(current_exception);
      }
      catch( const std::exception& e )
      {
        elog("Error in function: ${function_name}. Block number: ${block_number}. Error details: ${what}", (function_name)(block_number)("what", e.what()));
      }
    }
    else
      elog("Error in function: ${function_name}. Block number: ${block_number}. Cannot get error details", (function_name)(block_number));
  }
}

void do_job(const fc::path& input_block_log_path, const fc::path& output_block_log_path, const unsigned compress_jobs, const bool input_read_only, appbase::application& app, hive::chain::blockchain_worker_thread_pool& thread_pool)
{
  FC_ASSERT(compress_jobs);

  auto fpq = [&input_block_log_path, input_read_only, &app, &thread_pool](uint32_t& block_number) {fill_pending_queue((input_block_log_path), input_read_only, block_number, app, thread_pool);};
  auto dcq = [&output_block_log_path, &app, &thread_pool](uint32_t& block_number) { drain_completed_queue((output_block_log_path), block_number, app, thread_pool); };
  auto cb = [](uint32_t& block_number) {compress_blocks(block_number);};

  std::vector<std::thread> workers;
  workers.reserve(2 + compress_jobs);
  workers.push_back(std::thread(function_wrapper<decltype(fpq)>, fpq, "fill_pending_queue"));
  workers.push_back(std::thread(function_wrapper<decltype(dcq)>, dcq, "drain_completed_queue"));

  for (unsigned i = 0; i < compress_jobs; ++i)
    workers.push_back(std::thread(function_wrapper<decltype(cb)>, cb, "compress_blocks_" + std::to_string(i)));

  for(auto& worker : workers)
  {
    if (worker.joinable())
      worker.join();
  }
}

int main(int argc, char** argv)
{
  try
  {
    appbase::application theApp;
    hive::chain::blockchain_worker_thread_pool thread_pool = hive::chain::blockchain_worker_thread_pool( theApp );
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
    options.add_options()("input-block-log,i", boost::program_options::value<std::string>(), "The file (or 1st file when split) containing the input block log. Has rights to read and write.");
    options.add_options()("input-read-only-block-log", boost::program_options::value<std::string>(), "The file (or 1st file when split) containing the input block log. Read only mode.");
    options.add_options()("output-block-log,o", boost::program_options::value<std::string>()->required(), "The file (or 1st file when split) to contain the compressed block log");
    options.add_options()("dump-raw-blocks", boost::program_options::value<std::string>(), "A directory in which to dump raw, uncompressed blocks (one block per file)");
    options.add_options()("starting-block-number,s", boost::program_options::value<uint32_t>()->default_value(1), "Start at the given block number (for benchmarking only, values > 1 will generate an unusable block log)");
    options.add_options()("block-count,n", boost::program_options::value<uint32_t>(), "Stop after this many blocks");
    options.add_options()("use-compressed-even-when-larger", boost::program_options::bool_switch()->default_value(true), "Store the compressed version of the blocks, even when larger than the uncompressed version");

    options.add_options()("help,h", "Print usage instructions");

    boost::program_options::variables_map options_map;
    boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(options).run(), options_map);

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

    if (!options_map.count("output-block-log"))
    {
      std::cerr << "Error: missing parameter output-block-log\n";
      return 1;
    }

    fc::path output_block_log_path = options_map["output-block-log"].as<std::string>();
    if (fc::exists(output_block_log_path) &&
        (not fc::is_regular_file(output_block_log_path) ||
         not hive::chain::block_log_file_name_info::is_block_log_file_name(output_block_log_path)))
    {
      std::cerr << "Error: parameter output-block-log is not a block log file name (single or split)\n";
      return 1;
    }

    if (!options_map.count("input-block-log") && !options_map.count("input-read-only-block-log"))
    {
      std::cerr << "Error: missing parameter for input block_log (input-block-log or input-read-only-block-log)\n";
      return 1;
    }

    if (options_map.count("input-block-log") && options_map.count("input-read-only-block-log"))
    {
      std::cerr << "Error: input-read-only-block-log and input-block-log exclude themselves. You cannot use both.\n";
      return 1;
    }

    fc::path input_block_log_path;
    bool input_readonly = false;
    if (options_map.count("input-block-log"))
      input_block_log_path = options_map["input-block-log"].as<std::string>();
    else
    {
      input_readonly = true;
      input_block_log_path = options_map["input-read-only-block-log"].as<std::string>();
    }

    if (not fc::exists(input_block_log_path) ||
        not fc::is_regular_file(input_block_log_path) ||
        not hive::chain::block_log_file_name_info::is_block_log_file_name(input_block_log_path))
    {
      std::cerr << "Error: parameter input-[read-only-]block-log is not a block log file name (single or split)\n";
      return 1;
    }

    if (options_map.count("dump-raw-blocks"))
      raw_block_output_path = options_map["dump-raw-blocks"].as<std::string>();

    // store the block compressed with zstd even when the uncompressed version is smaller.
    // There are a few thousand small blocks near the beginning of the chain which are uncompressable,
    // and the zstd version comes out slightly larger than the uncompressed version.
    // By default, we'll still store the compressed version in the block log.  Overall, this
    // makes the block log about 50k bytes larger than if we didn't, but it simplifies matters.
    // 
    // When serving a block to a peer that accepts compressed blocks, we'll try to compress any
    // uncompressed blocks loaded from the block log.  If we stored those blocks uncompressed,
    // we would have to attempt to compress the blocks each time we sent them to a peer.
    use_compressed_even_when_larger = options_map["use-compressed-even-when-larger"].as<bool>();

    do_job(input_block_log_path, output_block_log_path, jobs, input_readonly, theApp, thread_pool);

    if (error_detected.load())
    {
      elog("Work was interrupted by an error.");
      return 0;
    }

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
  catch (const fc::exception& e)
  {
    edump((e));
  }
  catch (const std::exception& e)
  {
    edump((std::string(e.what())));
  }

  return 0;
}
