#include <appbase/application.hpp>

#include <fc/log/logger.hpp>
#include <fc/log/logger_config.hpp>
#include <fc/log/file_appender.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/io/json.hpp>
#include <fc/filesystem.hpp>
#include <fc/thread/thread.hpp>
#include <hive/chain/block_log.hpp>
#include <hive/chain/block_log_wrapper.hpp>
#include <hive/chain/detail/block_attributes.hpp>
#include <hive/chain/block_compression_dictionaries.hpp>
#include <hive/chain/full_block.hpp>
#include <hive/chain/blockchain_worker_thread_pool.hpp>
#include <hive/chain/block_compression_dictionaries.hpp>
#include <hive/chain/split_block_log.hpp>
#include <hive/utilities/io_primitives.hpp>
#include <hive/utilities/git_revision.hpp>

#include <boost/program_options.hpp>
#include <boost/scope_exit.hpp>
#include <boost/thread/barrier.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <thread>
#include <chrono>
#include <memory>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <algorithm>

enum ExitCode
{
  Ok = 0,
  LogFileError = 1,
  InvalidArgumentError = 2,
  BlocksRangeError = 3,
  ArtifactsFileNotValid = 4,
  BlockLogInvalid = 5,
  FailToOpenInWriteMode = 6,
  ExceptionOccured = 7,
  OperationFailed = 8
};

struct shutdown_executor
{
  shutdown_executor(uint32_t threads_num) :
    the_app(),
    thread_pool( hive::chain::blockchain_worker_thread_pool(the_app) )
  {
    thread_pool.set_thread_pool_size(threads_num);
  }
  ~shutdown_executor()
  {
    // Ensure that the thread pool is shut down before its users' destructors are called.
    thread_pool.shutdown();
  }

  appbase::application the_app;
  hive::chain::blockchain_worker_thread_pool thread_pool;
};

using block_log_info = hive::chain::block_log_file_name_info;

void print_version()
{
  std::stringstream ss;
  ss << "git revision: " << hive::utilities::git_revision_sha << "\n";
  ss << "Configured for network type: ";
#ifdef IS_TEST_NET
  ss << "testnet";
#elif HIVE_CONVERTER_BUILD
  ss << "mirrornet";
#else
  ss << "mainnet";
#endif
  std::cout << ss.str() << "\n";
}

std::string generate_json_output(const std::initializer_list<std::pair<std::string, fc::variant>> &elements)
{
  fc::variant_object_builder builder;
  for (const auto& el : elements)
    builder.append(el.first, el.second);
  return fc::json::to_string(builder.get());
}

void print_and_log_error(const std::string err, const bool json_output)
{
  elog("${err}", (err));
  if (json_output)
    std::cerr << generate_json_output({
      {"Error", fc::variant(err)}
    });
  else
    std::cerr << err << "\n";
}

bool artifacts_file_is_not_valid(const fc::path& block_log_file_path, const bool json_output)
{
  const fc::path artifacts_file = fc::path(block_log_file_path.generic_string() + hive::chain::block_log_file_name_info::_artifacts_extension.c_str());
  if (fc::is_directory(artifacts_file))
  {
    print_and_log_error(artifacts_file.string() + " is directory - artifacts should be a file", json_output);
    return true;
  }
  if (!fc::exists(artifacts_file))
  {
    print_and_log_error(artifacts_file.string() + " does not exist - expected to have artifacts file for operation.", json_output);
    return true;
  }
  if (fc::file_size(artifacts_file) == 0)
  {
    print_and_log_error(artifacts_file.string() + " is an empty file!", json_output);
    return true;
  }

  return false;
}

hive::chain::block_log_wrapper::block_log_wrapper_t get_block_log_wrapper(const fc::path &block_log_path, appbase::application &app, hive::chain::blockchain_worker_thread_pool &thread_pool, bool read_only)
{
  if (fc::is_directory(block_log_path))
  {
    auto wrapper = hive::chain::block_log_wrapper::create_limited_wrapper(block_log_path, app, thread_pool);
    hive::chain::block_log_wrapper::block_log_open_args args;
    args.data_dir = block_log_path;
    wrapper->open_and_init(args, read_only, nullptr);
    return wrapper;
  }
  else
    return hive::chain::block_log_wrapper::create_opened_wrapper(block_log_path, app, thread_pool, read_only);
}

void print_simple_error_or_result_message(const std::string message, const fc::path &block_log_path, const bool json_output, const bool is_error)
{
  if (json_output)
    std::cerr << generate_json_output(
      {
        {is_error ? "Error" : "Result", fc::variant(message)},
        {"block_log_path", fc::variant(block_log_path)}
      }
    );
  else if (is_error)
    std::cout << "Error: " << message << "\n";
  else
    std::cout << message << "\n";
}

bool validate_first_and_last_block_and_print_error_if_necessary(
  const fc::path &block_log_path,
  const uint32_t first_block,
  const uint32_t last_block,
  const uint32_t tail_block_num,
  const uint32_t head_block_num,
  const bool json_output
) {
  if (first_block < tail_block_num)
    {
      if (json_output)
        std::cerr << generate_json_output(
          {
            {"Error", fc::variant("first block must be bigger than than tail block number")},
            {"first_block", fc::variant(first_block)},
            {"tail_block_num", fc::variant(tail_block_num)},
            {"block_log_path", fc::variant(block_log_path)}
          }
        );
      else
        std::cout << "Cannot operate on empty block_log\n";
      return false;
    }
    if (head_block_num < last_block)
    {
      if (json_output)
        std::cerr << generate_json_output(
          {
            {"Error", fc::variant("head_block_num must be bigger than than last_block")},
            {"head_block_num", fc::variant(head_block_num)},
            {"last_block", fc::variant(last_block)},
            {"block_log_path", fc::variant(block_log_path)}
          }
        );
      else
        std::cout << "Error: Cannot operate on empty block_log\n";
      return false;
    }

    return true;
}

struct extended_signed_block_header : public hive::protocol::signed_block_header
{
  hive::protocol::block_id_type block_id;
  hive::protocol::public_key_type signing_key;

  extended_signed_block_header(const hive::chain::full_block_type &full_block) : hive::protocol::signed_block_header(full_block.get_block_header()),
                                                                                 block_id(full_block.get_block_id()),
                                                                                 signing_key(full_block.get_signing_key()) {}
};

struct extended_signed_block : public extended_signed_block_header
{
  std::vector<hive::protocol::signed_transaction> transactions;
  extended_signed_block(const hive::chain::full_block_type &full_block) : extended_signed_block_header(full_block),
                                                                          transactions(full_block.get_block().transactions) {}
};

struct block_log_hashes
{
  fc::optional<fc::sha256> final_hash;
  std::map<uint32_t, fc::sha256> checkpoints;
};
typedef std::map<fc::path, block_log_hashes> block_logs_and_hashes_type;

// check if returning values matches block_log.
std::pair<uint32_t, uint32_t> get_effective_range_of_blocks(const int32_t given_first_block, const int32_t given_last_block, const uint32_t given_head_block, const uint32_t given_tail_block)
{
  FC_ASSERT(given_head_block <= static_cast<uint32_t>(std::numeric_limits<int32_t>::max()), "given_head_block: ${given_head_block} reached max int32_t value", (given_head_block));
  const int32_t head_block = static_cast<int32_t>(given_head_block);
  FC_ASSERT(given_tail_block <= static_cast<uint32_t>(std::numeric_limits<int32_t>::max()), "given_tail_block: ${given_tail_block} reached max int32_t value", (given_tail_block));
  const int32_t tail_block = static_cast<int32_t>(given_tail_block);

  uint32_t first_block, last_block;
  if (given_first_block >= 0)
    first_block = std::max(tail_block, given_first_block);
  else
    first_block = std::max(tail_block, head_block + given_first_block + 1);
  if (given_last_block >= 0)
  {
    const uint32_t lb = static_cast<uint32_t>(given_last_block);
    //FC_ASSERT(lb <= given_head_block, "--to ${lb} must be below or equal to head_block: ${given_head_block}",(lb)(given_head_block));
    last_block = lb;
  }
  else
    last_block = std::min(head_block, head_block + given_last_block + 1);

  //FC_ASSERT(first_block <= last_block, "Calculated effective range is: [${first_block}:${last_block}] which is wrong. Parameters: given_first_block: ${given_first_block}, given_last_block:${given_last_block}, head_block: ${head_block}, tail_block: ${tail_block}",
  //          (first_block)(last_block)(given_first_block)(given_last_block)(head_block)(tail_block));

  return std::make_pair(first_block, last_block);
}

void append_full_block_data(const std::shared_ptr<hive::chain::full_block_type> &full_block, uint64_t &start_offset, fc::sha256::encoder &encoder)
{
  const hive::chain::uncompressed_block_data &uncompressed = full_block->get_uncompressed_block();
  // the block log gets the block's raw data followed by the 8-byte offset of the start of the block
  encoder.write(uncompressed.raw_bytes.get(), uncompressed.raw_size);
  encoder.write((const char *)&start_offset, sizeof(start_offset));
  start_offset += uncompressed.raw_size + sizeof(start_offset);
}

ExitCode checksum_block_log(const fc::path &block_log, fc::optional<uint32_t> checkpoint_every_n_blocks, appbase::application &app, hive::chain::blockchain_worker_thread_pool &thread_pool, const bool json_output)
{
  try
  {
    hive::chain::block_log log(app);
    log.open(block_log, thread_pool, true);
    FC_ASSERT(log.head(), "Cannot operate on empty block_log");

    uint64_t uncompressed_block_start_offset = 0; // as we go, keep track of the block's starting offset if it were recorded uncompressed
    fc::sha256::encoder block_log_sha256_encoder;

    const uint32_t head_block_num = log.head()->get_block_num();

    const auto process_block = [&](const std::shared_ptr<hive::chain::full_block_type> full_block)
    {
      if (full_block->get_block_num() % 1000000 == 0)
        dlog("processed block ${current} of ${total}", ("current", full_block->get_block_num())("total", head_block_num));

      append_full_block_data(full_block, uncompressed_block_start_offset, block_log_sha256_encoder);

      if (!json_output && checkpoint_every_n_blocks && full_block->get_block_num() % *checkpoint_every_n_blocks == 0 && full_block->get_block_num() != head_block_num)
      {
        fc::sha256::encoder block_log_encoder_up_to_this_block(block_log_sha256_encoder);
        ilog("${result} ${block_log}@${block_num}", ("result", block_log_encoder_up_to_this_block.result().str())(block_log)("block_num", full_block->get_block_num()));
        std::cout << block_log_encoder_up_to_this_block.result().str() << " " << block_log.generic_string() << "@" << full_block->get_block_num() << "\n";
      }

      return true;
    };
    log.for_each_block(block_log_info::get_first_block_num_for_file_name(block_log),
                        head_block_num, process_block, hive::chain::block_log::for_each_purpose::decompressing,
                        thread_pool);

    fc::sha256 final_hash = block_log_sha256_encoder.result();

    ilog("Final hash: ${final_hash} ${block_log}", ("final_hash", final_hash.str())(block_log));
    if (json_output)
    {
      std::cout << generate_json_output({
        { "final_hash", fc::variant(final_hash) },
        { "block_log", fc::variant("block_log") }
      });
    }
    else
      std::cout << final_hash.str() << " " << block_log.generic_string() << "\n";

    return ExitCode::Ok;
  }
  FC_LOG_AND_RETHROW()
}

bool validate_block_log_checksum(const fc::path &block_log, const block_log_hashes &hashes_to_validate, appbase::application &app, hive::chain::blockchain_worker_thread_pool &thread_pool)
{
  try
  {
    hive::chain::block_log log(app);
    log.open(block_log, thread_pool, true);
    FC_ASSERT(log.head(), "Cannot operate on empty block_log");

    uint64_t uncompressed_block_start_offset = 0; // as we go, keep track of the block's starting offset if it were recorded uncompressed
    fc::sha256::encoder block_log_sha256_encoder;

    fc::optional<uint32_t> last_good_checkpoint_block_number;
    auto next_checkpoint_iter = hashes_to_validate.checkpoints.begin();
    bool all_hashes_matched = true;

    const uint32_t head_block_num = log.head()->get_block_num();
    const auto process_block = [&](const std::shared_ptr<hive::chain::full_block_type> full_block)
    {
      if (full_block->get_block_num() % 1000000 == 0)
        dlog("processed block ${current} of ${total}", ("current", full_block->get_block_num())("total", head_block_num));

      append_full_block_data(full_block, uncompressed_block_start_offset, block_log_sha256_encoder);

      if (next_checkpoint_iter != hashes_to_validate.checkpoints.end())
      {
        const uint32_t checkpoint_block = next_checkpoint_iter->first;
        const fc::sha256 &checkpoint_hash = next_checkpoint_iter->second;
        if (full_block->get_block_num() == checkpoint_block)
        {
          const fc::sha256 block_log_hash_up_to_this_block = fc::sha256::encoder(block_log_sha256_encoder).result();

          if (block_log_hash_up_to_this_block != checkpoint_hash)
          {
            elog("${block_log}@${checkpoint_block}: FAILED (checksum mismatch)", (block_log)(checkpoint_block));
            all_hashes_matched = false;
            for (; next_checkpoint_iter != hashes_to_validate.checkpoints.end(); ++next_checkpoint_iter)
              elog("${block_log}@${block_num} : FAILED (presumed checksum mismatch because of checksum mismatch on earlier block: ${checkpoint_block}", (block_log)("block_num", next_checkpoint_iter->first)(checkpoint_block));
            return false; // stop iterating
          }
          else
          {
            ilog("${block_log}@${checkpoint_block}: OK", (block_log)(checkpoint_block));
            last_good_checkpoint_block_number = checkpoint_block;
            ++next_checkpoint_iter;

            if (next_checkpoint_iter == hashes_to_validate.checkpoints.end() &&
                !hashes_to_validate.final_hash)
              return false; // stop iterating
          }
        }
      }

      return true;
    };
    log.for_each_block(block_log_info::get_first_block_num_for_file_name(block_log),
                        head_block_num, process_block, hive::chain::block_log::for_each_purpose::decompressing,
                        thread_pool);

    fc::sha256 final_hash = block_log_sha256_encoder.result();

    for (; next_checkpoint_iter != hashes_to_validate.checkpoints.end(); ++next_checkpoint_iter)
    {
      elog("${block_log}@${checkpoint_block}: FAILED (block log only contain ${head_block_num} blocks)", (block_log)("checkpoint_block", next_checkpoint_iter->first)(head_block_num));
      all_hashes_matched = false;
    }

    if (hashes_to_validate.final_hash)
    {
      if (final_hash != *hashes_to_validate.final_hash)
      {
        elog("${block_log}: FAILED", (block_log));
        all_hashes_matched = false;
      }
      else
        ilog("${block_log}: OK", (block_log));
    }

    if (all_hashes_matched)
    {
      if (!hashes_to_validate.final_hash &&
          !hashes_to_validate.checkpoints.empty() &&
          hashes_to_validate.checkpoints.rbegin()->first != head_block_num)
        wlog("all checksums matched, but your block log contains ${block_num} blocks past the last checkpoint", ("block_num", head_block_num - *last_good_checkpoint_block_number));
    }

    return all_hashes_matched;
  }
  FC_CAPTURE_AND_RETHROW()
}

block_logs_and_hashes_type parse_checkpoints(const fc::path &checkpoints_file)
{
  try
  {
    block_logs_and_hashes_type result;
    std::ifstream hashes_stream(checkpoints_file.string());
    if (!hashes_stream)
      FC_THROW("Error opening sha256sums file ${checkpoints_file}", (checkpoints_file));
    std::string line;
    while (std::getline(hashes_stream, line))
    {
      size_t space_pos = line.find(' ');
      if (space_pos != std::string::npos)
      {
        fc::sha256 hash = fc::sha256(line.substr(0, space_pos));

        size_t filename_start = line.find_first_not_of(" \t", space_pos);
        if (filename_start != std::string::npos)
        {
          std::string filename_and_block_num = line.substr(filename_start);
          std::string filename;
          fc::optional<uint32_t> block_number;
          size_t asperand_pos = filename_and_block_num.find('@');
          if (asperand_pos != std::string::npos)
          {
            filename = filename_and_block_num.substr(0, asperand_pos);
            block_number = stoul(filename_and_block_num.substr(asperand_pos + 1));
          }
          else
            filename = filename_and_block_num;
          if (block_number)
            result[filename].checkpoints[*block_number] = hash;
          else
            result[filename].final_hash = hash;
        }
      }
    }
    return result;
  }
  FC_CAPTURE_AND_RETHROW()
}

ExitCode validate_block_log_checksums_from_file(const fc::path &checksums_file, appbase::application &app, hive::chain::blockchain_worker_thread_pool &thread_pool, const bool json_output)
{
  try
  {
    block_logs_and_hashes_type hashes_in_checksums_file = parse_checkpoints(checksums_file);
    unsigned fail_count = 0;
    for (const auto &[block_log, hashes_to_validate] : hashes_in_checksums_file)
      if (!validate_block_log_checksum(block_log, hashes_to_validate, app, thread_pool))
        ++fail_count;
    if (fail_count)
    {
      print_simple_error_or_result_message("checksums file had " + std::to_string(fail_count) + " checksums that did NOT match", checksums_file, json_output, true);
      elog("checksums file had ${fail_count} checksums that did NOT match", (fail_count));
    }
    else
    {
      print_simple_error_or_result_message("All checksums from file match", checksums_file, json_output, false);
      ilog("All checksums from file match");
    }
    return ExitCode::Ok;
  }
  FC_CAPTURE_AND_RETHROW()
}

ExitCode compare_block_logs(const fc::path &first_bl_path, const fc::path &second_bl_path,
                        const int32_t first_block_arg, const int32_t last_block_arg, appbase::application &app, hive::chain::blockchain_worker_thread_pool &thread_pool, const bool json_output)
{
  try
  {
    // open both block log files
    auto first_block_log_wrapper = get_block_log_wrapper(first_bl_path, app, thread_pool, true);
    const uint32_t first_head_block_num = first_block_log_wrapper->head_block_num();
    if (!first_head_block_num)
    {
      print_simple_error_or_result_message("First block log is empty. Cannot perform compare operation", first_bl_path, json_output, true);
      return ExitCode::BlockLogInvalid;
    }
    
    const uint32_t first_tail_block_num = first_block_log_wrapper->get_actual_tail_block_num();
    const auto [first_log_first_block_num, first_log_last_block_num] = get_effective_range_of_blocks(first_block_arg, last_block_arg, first_head_block_num, first_tail_block_num);

    auto second_block_log_wrapper = get_block_log_wrapper(second_bl_path, app, thread_pool, true);
    const uint32_t second_head_block_num = second_block_log_wrapper->head_block_num();

    if (!second_head_block_num)
    {
      print_simple_error_or_result_message("Second block log is empty. Cannot perform compare operation", second_bl_path, json_output, true);
      return ExitCode::BlockLogInvalid;
    }

    const uint32_t second_tail_block_num = second_block_log_wrapper->get_actual_tail_block_num();
    const auto [second_log_first_block_num, second_log_last_block_num] = get_effective_range_of_blocks(first_block_arg, last_block_arg, second_head_block_num, second_tail_block_num);

    if (first_log_first_block_num != second_log_first_block_num)
    {
      if (json_output)
        std::cerr << generate_json_output(
            {
              {"Result", fc::variant("Difference found when comparing starting block number in given block logs")},
              {"first_block_log_path", fc::variant(first_bl_path)},
              {"first block number in first block_log", fc::variant(first_tail_block_num)},
              {"first block number to compare in first block_log", fc::variant(first_log_first_block_num)},
              {"second_block_log_path", fc::variant(second_bl_path)},
              {"first block number in second block_log", fc::variant(second_tail_block_num)},
              {"first block number to compare in second block_log", fc::variant(second_log_first_block_num)},
            }
          );
      else
      {
        std::cerr << "Difference found when comparing starting block number in given block logs.\n First block_log: " << first_bl_path.string() << ", first block number in block_log: " << std::to_string(first_tail_block_num)
          << ", first block number to compare: " << std::to_string(first_log_first_block_num) << "\nSecond block_log: " << second_bl_path.string() << ", first block number in block_log: " << std::to_string(second_tail_block_num)
          << ", first block number to compare: " << std::to_string(second_log_first_block_num) << "\n";
      }

      return ExitCode::Ok;
    }

    if (first_log_last_block_num != second_log_last_block_num)
    {
      if (json_output)
        std::cerr << generate_json_output(
            {
              {"Result", fc::variant("Difference found comparing ending block number in given block logs.")},
              {"first_block_log_path", fc::variant(first_bl_path)},
              {"head block number in first block_log", fc::variant(first_head_block_num)},
              {"head block number to compare in first block_log", fc::variant(first_log_last_block_num)},
              {"second_block_log_path", fc::variant(second_bl_path)},
              {"head block number in second block_log", fc::variant(second_head_block_num)},
              {"head block number to compare in second block_log", fc::variant(second_log_last_block_num)},
            }
          );
      else
      {
        std::cerr << "Difference found comparing ending block number in given block logs..\nFirst block_log: " << first_bl_path.string() << ", head block number in block_log: " << std::to_string(first_tail_block_num)
          << ", last block number to compare: " << std::to_string(first_log_first_block_num) << "\nSecond block_log: " << second_bl_path.string() << ", head block number in block_log: " << std::to_string(second_tail_block_num)
          << ", last block number to compare: " << std::to_string(second_log_first_block_num) << "\n";
      }

      return ExitCode::Ok;
    }

    const uint32_t first_block_to_compare = first_log_first_block_num;
    const uint32_t last_block_to_compare = first_log_last_block_num;

    // set up to loop through all the blocks.  we'll be doing two for_each_block calls in parallel,
    // and we don't know whether we'll get the blocks from the first or second block log first.
    // when we get the block, store it in the variable below.  once we have both, compare
    std::shared_ptr<hive::chain::full_block_type> first_full_block;
    std::shared_ptr<hive::chain::full_block_type> second_full_block;

    // if our compare fails, store the fact in these variables
    fc::optional<uint32_t> mismatch_on_block_number;
    std::atomic<bool> mismatch_detected = {false};

    const auto compare_blocks = [&]()
    {
      const hive::chain::uncompressed_block_data &first_uncompressed = first_full_block->get_uncompressed_block();
      const hive::chain::uncompressed_block_data &second_uncompressed = second_full_block->get_uncompressed_block();
      if (first_uncompressed.raw_size != second_uncompressed.raw_size ||
          memcmp(first_uncompressed.raw_bytes.get(), second_uncompressed.raw_bytes.get(), first_uncompressed.raw_size) != 0)
      {
        mismatch_on_block_number = first_full_block->get_block_num();
        mismatch_detected.store(true, std::memory_order_release);
      }
      // not strictly necessary
      first_full_block = nullptr;
      second_full_block = nullptr;
    };

    boost::barrier sync_point(2, compare_blocks);

    const auto generate_block_processor = [&](std::shared_ptr<hive::chain::full_block_type> &destination)
    {
      return [&](const std::shared_ptr<hive::chain::full_block_type> &full_block)
      {
        destination = full_block;
        sync_point.wait();
        return !mismatch_detected.load(std::memory_order_relaxed);
      };
    };

    // walk through the blocks, comparing as we go
    std::thread first_enumerator_thread([&]()
                                        {
      fc::set_thread_name("compare_1"); // tells the OS the thread's name
      fc::thread::current().set_name("compare_1"); // tells fc the thread's name for logging
        first_block_log_wrapper->process_blocks(first_block_to_compare, last_block_to_compare, generate_block_processor(first_full_block), thread_pool); });

    std::thread second_enumerator_thread([&]()
                                         {
      fc::set_thread_name("compare_2"); // tells the OS the thread's name
      fc::thread::current().set_name("compare_2"); // tells fc the thread's name for logging
        second_block_log_wrapper->process_blocks(first_block_to_compare, last_block_to_compare, generate_block_processor(second_full_block), thread_pool); });

    first_enumerator_thread.join();
    second_enumerator_thread.join();

    // check the results
    if (mismatch_detected.load(std::memory_order_relaxed))
    {
      elog("${first_bl_path} and ${second_bl_path} differ at block: ${mismatch_on_block_number}", (first_bl_path)(second_bl_path)(mismatch_on_block_number));
      if (json_output)
        std::cerr << generate_json_output(
          {
            {"Result", fc::variant("Difference detected")},
            {"Block number", fc::variant(mismatch_on_block_number)},
            {"first_block_log_path", fc::variant(first_bl_path)},
            {"second_block_log_path", fc::variant(second_bl_path)}
          }
        );
      else
        std::cerr << "Both block_logs differ at block: " << *mismatch_on_block_number << "\n";
    }
    else
    {
      if (json_output)
        std::cerr << generate_json_output(
          {
            {"Result", fc::variant("No difference detected")},
            {"first_block_log_path", fc::variant(first_bl_path)},
            {"second_block_log_path", fc::variant(second_bl_path)}
          }
        );
      else
        std::cout << "No difference detected between block_logs\n";
    }

    return ExitCode::Ok;
  }
  FC_CAPTURE_AND_RETHROW()
}

ExitCode truncate_block_log(const fc::path &block_log_filename, uint32_t new_head_block_num, bool force, appbase::application &app, hive::chain::blockchain_worker_thread_pool &thread_pool, const bool json_output)
{
  try
  {
    if (!force)
    {
      std::cout << "\nReally truncate " << block_log_filename.string() << " to " << new_head_block_num << " blocks? [yes/no] ";
      std::string response;
      std::cin >> response;
      std::transform(response.begin(), response.end(), response.begin(), tolower);
      if (response != "yes")
      {
        std::cout << "aborting\n";
        return ExitCode::Ok;
      }
    }
    std::cout << "\n";
    hive::chain::block_log log(app);
    log.open(block_log_filename, thread_pool, false);
    if (!log.head())
    {
      print_simple_error_or_result_message("Block log is empty.", block_log_filename, json_output, true);
      return ExitCode::BlockLogInvalid;
    }
    const uint32_t head_block_num = log.read_head()->get_block_num();
    if (head_block_num <= new_head_block_num)
    {
      if (json_output)
        std::cerr << generate_json_output(
          {
            {"Error", fc::variant("Truncation impossible, head block number is smaller than requested")},
            {"block_log_path", fc::variant(block_log_filename)},
            {"head_block_num", fc::variant(head_block_num)},
            {"new_head_block_num", fc::variant(new_head_block_num)}
          }
        );

      else
        std::cerr << "Truncation impossible, head block number is smaller (" << std::to_string(head_block_num) << ") than requested: " << std::to_string(new_head_block_num) << "\n";
      return ExitCode::BlocksRangeError;
    }

    const auto lowest_possible_block = block_log_info::get_first_block_num_for_file_name(block_log_filename);
    if (lowest_possible_block > new_head_block_num)
    {
      if (json_output)
        std::cerr << generate_json_output(
          {
            {"Error", fc::variant("Truncation impossible, new head block number is too small")},
            {"block_log_path", fc::variant(block_log_filename)},
            {"lowest_possible_head_block_num", fc::variant(lowest_possible_block)},
            {"new_head_block_num", fc::variant(new_head_block_num)}
          }
        );
      else
        std::cerr << "Truncation impossible, head block number is smaller (" << std::to_string(head_block_num) << ") than requested: " << std::to_string(new_head_block_num) << "\n";
      return ExitCode::BlocksRangeError;
    }
    ilog("Original block_log head_block_num: ${head_block_num}", (head_block_num));
    log.truncate(new_head_block_num);
    ilog("Truncating finished.");
    if (json_output)
      std::cerr << generate_json_output(
          {
            {"Result", fc::variant("Truncation finished")},
            {"block_log_path", fc::variant(block_log_filename)},
            {"old_head_block_num", fc::variant(head_block_num)},
            {"new_head_block_num", fc::variant(new_head_block_num)}
          }
        );
      else
        std::cerr << "Truncating finished\n";
    return ExitCode::Ok;
  }
  FC_CAPTURE_AND_RETHROW()
}

// helper function.  Reads the 8 bytes at the given location, and determines whether they "look like" the
// flags/offset byte that's written at the end of each block.  Used for when you have a corrupt block log
// and need to find the last complete block.
// if it looked reasonable, returns the start of the block it would point at
fc::optional<uint64_t> is_data_at_file_position_a_plausible_offset_and_flags(int block_log_fd, uint64_t offset_of_pos_and_flags)
{
  uint64_t block_offset_with_flags;
  hive::utilities::perform_read(block_log_fd, (char *)&block_offset_with_flags, sizeof(block_offset_with_flags),
                                offset_of_pos_and_flags, "read block offset");

  const auto [offset, flags] = hive::chain::detail::split_block_start_pos_with_flags(block_offset_with_flags);
  ddump((offset));

  // check that the offset of the start of the block wouldn't mean that it's impossibly large
  const bool offset_is_plausible = offset < offset_of_pos_and_flags && offset >= offset_of_pos_and_flags - HIVE_MAX_BLOCK_SIZE;

  // check that no reserved flags are set, only "zstd/uncompressed" and "has dictionary" are permitted
  const bool flags_are_plausible = (block_offset_with_flags & 0x7e00000000000000ull) == 0;

  bool dictionary_is_plausible;
  if (block_offset_with_flags & 0x0100000000000000ull)
  {
    // if the dictionary flag bit is set, verify that the dictionary number is one that we have.
    dictionary_is_plausible = *flags.dictionary_number <= hive::chain::get_last_available_zstd_compression_dictionary_number().value_or(0);
  }
  else
  {
    // if the dictionary flag bit is not set, expect the dictionary number to be zeroed
    dictionary_is_plausible = (block_offset_with_flags & 0x00ff000000000000ull) == 0;
  }

  return offset_is_plausible && flags_are_plausible && dictionary_is_plausible ? offset : fc::optional<uint64_t>();
}

ExitCode find_end(const fc::path &block_log_filename, bool json_output)
{
  try
  {
    bool possible_end_found = false;

    int block_log_fd = open(block_log_filename.string().c_str(), O_RDONLY | O_CLOEXEC, 0644);
    if (block_log_fd == -1)
    {
      print_simple_error_or_result_message("failed to open block log file: " + std::string(strerror(errno)), block_log_filename, json_output, true);
      return ExitCode::BlockLogInvalid;
    }

    struct stat file_stats;
    if (fstat(block_log_fd, &file_stats) == -1)
    {
      print_simple_error_or_result_message("failed to get block log file size: " + std::string(strerror(errno)), block_log_filename, json_output, true);
      return ExitCode::BlockLogInvalid;
    }
    const uint64_t block_log_size = file_stats.st_size;
    const auto minimum_size = (ssize_t)sizeof(uint64_t);

    if (block_log_size < minimum_size)
    {
      print_simple_error_or_result_message("Block log size should be at least: " + std::to_string(minimum_size), block_log_filename, json_output, true);
      return ExitCode::BlockLogInvalid;
    }

    uint64_t offset_of_pos_and_flags = block_log_size - sizeof(uint64_t);
    for (int i = 0; i < HIVE_MAX_BLOCK_SIZE; ++i)
    {
      fc::optional<uint64_t> possible_start_of_block = is_data_at_file_position_a_plausible_offset_and_flags(block_log_fd, offset_of_pos_and_flags);
      if (possible_start_of_block)
      {
        dlog("found possible end of the block log, double-checking...");
        // if we really found the end of the block log, we should be able to use the offset there to walk back
        // to the offset at the end of the previous block, and on and on until we get to the first block.
        // The odds of random data forming a linked list is vanishingly small, so we'll just try to walk back
        // 10 blocks and call it a day
        bool verified = true;
        for (int j = 0; j < 10; ++j)
        {
          if (!*possible_start_of_block)
            break; // beginning of the block log

          if (*possible_start_of_block < sizeof(uint64_t))
          {
            // we'd have only part of a offset/flags, that would never happen
            verified = false;
            break;
          }

          const uint64_t offset_to_test = *possible_start_of_block - sizeof(uint64_t);
          possible_start_of_block = is_data_at_file_position_a_plausible_offset_and_flags(block_log_fd, offset_to_test);
          if (!possible_start_of_block)
          {
            dlog("verifying: offset ${offset_to_test} is NOT a valid looking position/flags, starting a search for a new end", (offset_to_test));
            verified = false;
            break;
          }
          else
            dlog("verifying: offset ${offset_to_test} is also a valid looking position/flags", (offset_to_test));
        }

        if (verified)
        {
          possible_end_found = true;
          break;
        }
      }
      --offset_of_pos_and_flags;
      if (offset_of_pos_and_flags == 0)
        break; // a block must be at least one byte
    }

    if (possible_end_found)
    {
      const uint64_t new_file_size = offset_of_pos_and_flags + sizeof(uint64_t);
      if (new_file_size == block_log_size)
      {
        ilog("The block log already appears to end in a full block, no truncation is necessary");
        print_simple_error_or_result_message("The block log already appears to end in a full block, no truncation is necessary", block_log_filename, json_output, true);
        return ExitCode::Ok;
      }
      else if (json_output)
      {
        std::cerr << generate_json_output(
          {
            {"Result", fc::variant("Likely end detected, block log size should be reduced.")},
            {"new_block_log_size", fc::variant(new_file_size)},
            {"block_log_path", fc::variant(block_log_filename)}
          }
        );
      }
      else
      {
        wlog("Likely end detected, block log size should be reduced to ${new_file_size}", (new_file_size));
        std::cout << "\n\nLikely end detected, block log size should be reduced to " << new_file_size << ",\n";
        std::cout << "which will reduce the block log size by " << block_log_size - new_file_size << " bytes\n\n";
        std::cout << "Do you want to truncate " << block_log_filename.string() << " now? [yes/no] ";
        std::string response;
        std::cin >> response;
        std::transform(response.begin(), response.end(), response.begin(), tolower);
        if (response == "yes")
        {
          std::cout << "\n";
          close(block_log_fd);
          block_log_fd = open(block_log_filename.string().c_str(), O_RDWR | O_APPEND | O_CREAT | O_CLOEXEC, 0644);
          if (block_log_fd == -1)
          {
            std::cerr << "Unable to reopen the block log file in write mode for truncating: " << strerror(errno) << "\n";
            return ExitCode::OperationFailed;
          }
          if (ftruncate(block_log_fd, new_file_size) != 0)
          {
            std::cerr << "failed to truncate block log: " << strerror(errno) << "\n";
            return ExitCode::OperationFailed;
          }
          ilog("Block log truncated");
          std::cout << "Block log truncated\n";
        }
        else
          std::cout << "Leaving the block log unmolested. You may truncate the file manually later using the command: truncate -s " << new_file_size << " " << block_log_filename.generic_string() << "\n";
      }
    }
    else
    {
      elog("Unable to detect the end of the block log");
      print_simple_error_or_result_message("Unable to detect the end of the block log", block_log_filename, json_output, true);
    }

    close(block_log_fd);
    return ExitCode::Ok;
  }
  FC_CAPTURE_AND_RETHROW()
}

ExitCode get_head_block_number(const fc::path &block_log_path, appbase::application &app, hive::chain::blockchain_worker_thread_pool &thread_pool, const bool json_output)
{
  try
  {
    auto block_log_wrapper = get_block_log_wrapper(block_log_path, app, thread_pool, true);
    const uint32_t head_block_num = block_log_wrapper->head_block_num();
    ilog("${block_log_filename} head block number: ${head_block_num}", (block_log_path)(head_block_num));
    if (json_output)
      std::cerr << generate_json_output(
        {
          {"head_block_number", fc::variant(head_block_num)},
          {"block_log_path", fc::variant(block_log_path)}
        }
      );
    else
      std::cout << head_block_num << "\n";
    block_log_wrapper->close_storage();
    return ExitCode::Ok;
  }
  FC_CAPTURE_AND_RETHROW()
}

ExitCode get_block_ids(const fc::path &block_log_path, const int32_t first_block_arg, const int32_t last_block_arg, bool print_block_numbers, appbase::application &app, hive::chain::blockchain_worker_thread_pool &thread_pool, const bool json_output)
{
  try
  {
    auto block_log_wrapper = get_block_log_wrapper(block_log_path, app, thread_pool, true);
    const uint32_t head_block_num = block_log_wrapper->head_block_num();
    if (!head_block_num)
    {
      print_simple_error_or_result_message("Cannot operate on empty block_log", block_log_path, json_output, true);
      return ExitCode::BlockLogInvalid;
    }
    const uint32_t tail_block_num = block_log_wrapper->get_actual_tail_block_num();
    const auto [first_block, last_block] = get_effective_range_of_blocks(first_block_arg, last_block_arg, head_block_num, tail_block_num);
    if (!validate_first_and_last_block_and_print_error_if_necessary(block_log_path, first_block, last_block, tail_block_num, head_block_num, json_output))
      return ExitCode::BlocksRangeError;

    if (json_output)
    {
        std::vector<fc::variant> ids;
        ids.reserve(last_block - first_block);
        for (unsigned i = first_block; i <= last_block; ++i)
        {
          if (print_block_numbers)
          {
            fc::variant_object vo(std::to_string(i), fc::variant(block_log_wrapper->read_block_id_by_num(i).str()));
            fc::variant v;
            fc::to_variant(vo, v);
            ids.push_back(v);
          }
          else
            ids.push_back(fc::variant(block_log_wrapper->read_block_id_by_num(i).str()));
        }
        std::cout << fc::json::to_string(ids);
    }
    else
    {
      for (unsigned i = first_block; i <= last_block; ++i)
      {
        if (print_block_numbers)
          std::cout << "block num:" << i << ", block_id: " << block_log_wrapper->read_block_id_by_num(i).str() << "\n";
        else
          std::cout << "block_id: " << block_log_wrapper->read_block_id_by_num(i).str() << "\n";
      }
    }
    return ExitCode::Ok;
  }
  FC_CAPTURE_AND_RETHROW()
}

ExitCode get_block_range(const fc::path &block_log_path, const int32_t first_block_arg, const int32_t last_block_arg,
                     fc::optional<fc::path> output_dir, bool header_only, bool pretty_print, bool binary,
                     appbase::application &app, hive::chain::blockchain_worker_thread_pool &thread_pool, const bool json_output)
{
  if (binary)
  {
    if (header_only)
    {
      print_simple_error_or_result_message("unable to write only the header in binary mod", block_log_path, json_output, true);
      return ExitCode::InvalidArgumentError;
    }
    if (!output_dir)
    {
      print_simple_error_or_result_message("If requested block in binary form, output_dir is obligatory", block_log_path, json_output, true);
      return ExitCode::InvalidArgumentError;
    }
  }

  try
  {
    auto block_log_wrapper = get_block_log_wrapper(block_log_path, app, thread_pool, true);
    const uint32_t head_block_num = block_log_wrapper->head_block_num();
    if (!head_block_num)
    {
      print_simple_error_or_result_message("Block log is empty", block_log_path, json_output, true);
      return ExitCode::BlockLogInvalid;
    }

    const uint32_t tail_block_num = block_log_wrapper->get_actual_tail_block_num();
    const auto [first_block, last_block] = get_effective_range_of_blocks(first_block_arg, last_block_arg, head_block_num, tail_block_num);
    if (!validate_first_and_last_block_and_print_error_if_necessary(block_log_path, first_block, last_block, tail_block_num, head_block_num, json_output))
      return ExitCode::BlocksRangeError;

    std::vector<fc::variant> blocks;
    blocks.reserve(last_block - first_block);
    std::stringstream ss;
    std::fstream fs;

    if (output_dir && !fc::exists(*output_dir))
    {
      dlog("Creating directories: ${output_dir}", (output_dir));
      fc::create_directories(*output_dir);
    }

    for (uint32_t current_block_num = first_block; current_block_num <= last_block; ++current_block_num)
    {
      std::shared_ptr<hive::chain::full_block_type> full_block = block_log_wrapper->read_block_by_num(current_block_num);

      if (output_dir)
      {
        static std::string filename_pattern;
        if (filename_pattern.empty())
        {
          if (last_block < 10)
            filename_pattern = "0";
          else
          {
            uint32_t number = last_block;
            while (number)
            {
              number /= 10;
              filename_pattern += "0";
            }
          }
        }
        std::string block_num_as_string = std::to_string(current_block_num);
        size_t block_num_as_string_length = block_num_as_string.length();
        const std::string filename = output_dir->generic_string() + "/block_" + filename_pattern.replace(filename_pattern.length() - block_num_as_string_length, block_num_as_string_length, block_num_as_string) + ".txt";
        fs.open(filename, std::ios::out | std::ios::trunc);
        if (!fs.is_open())
        {
          print_simple_error_or_result_message("Failed to create file: " + filename, block_log_path, json_output, true);
          return ExitCode::OperationFailed;
        }
      }

      if (binary)
      {
        const hive::chain::uncompressed_block_data &uncompressed = full_block->get_uncompressed_block();
        fs.write(uncompressed.raw_bytes.get(), uncompressed.raw_size);
      }
      else
      {
        if (header_only)
        {
          extended_signed_block_header ebh(*full_block);

          if (output_dir)
          {
            if (pretty_print)
              fs << fc::json::to_pretty_string(ebh) << "\n";
            else
              fs << fc::json::to_string(ebh) << "\n";
          }
          else if (json_output)
          {
            fc::variant v;
            fc::to_variant(ebh, v);
            blocks.push_back(v);
          }
          else
          {
            if (pretty_print)
              ss << fc::json::to_pretty_string(ebh) << "\n";
            else
              ss << fc::json::to_string(ebh) << "\n";
          }
        }
        else
        {
          extended_signed_block esb(*full_block);

          if (output_dir)
          {
            if (pretty_print)
              fs << fc::json::to_pretty_string(esb) << "\n";
            else
              fs << fc::json::to_string(esb) << "\n";
          }
          else if (json_output)
          {
            fc::variant v;
            fc::to_variant(esb, v);
            blocks.push_back(v);
          }
          else
          {
            if (pretty_print)
              ss << fc::json::to_pretty_string(esb) << "\n";
            else
              ss << fc::json::to_string(esb) << "\n";
          }
        }
      }

      if (output_dir)
        fs.close();
    }
    if (!output_dir && !json_output)
      std::cout << ss.str() << "\n";
    else if (pretty_print)
      std::cout << fc::json::to_pretty_string(blocks) << "\n";
    else
      std::cout << fc::json::to_string(blocks) << "\n";

    block_log_wrapper->close_storage();
    return ExitCode::Ok;
  }
  FC_CAPTURE_AND_RETHROW()
}

ExitCode get_block_artifacts(const fc::path &block_log_path, const int32_t first_block_arg, const int32_t last_block_arg, const bool header_only, const bool full_match_verification, appbase::application &app, hive::chain::blockchain_worker_thread_pool &thread_pool, const bool json_output)
{
  try
  {
    hive::chain::block_log block_log(app);
    bool block_log_existed = fc::exists(block_log_path);
    block_log.open(block_log_path, thread_pool, true, false);
    if (!block_log.head())
    {
      print_simple_error_or_result_message("Cannot operate on empty block_log", block_log_path, json_output, true);
      return ExitCode::BlockLogInvalid;
    }
    if (full_match_verification)
      ilog("Opening artifacts file with full artifacts match verification ...");

    hive::chain::block_log_artifacts::block_log_artifacts_ptr_t artifacts =
      hive::chain::block_log_artifacts::block_log_artifacts::open(block_log_path, block_log,
        true /*read_only*/, false /*write_fallback*/, full_match_verification,
        not block_log_existed /*new_block_log_created*/, app, thread_pool);

    if (full_match_verification)
      ilog("Artifacts file match verification done");

    const uint32_t artifacts_block_head_num = artifacts->read_head_block_num();
    if (!artifacts_block_head_num)
    {
      print_simple_error_or_result_message(artifacts->get_artifacts_file().string() + " is empty", block_log_path, json_output, true);
      return ExitCode::ArtifactsFileNotValid;
    }

    const uint32_t tail_block_num = block_log_info::get_first_block_num_for_file_name(block_log_path);
    const auto [first_block, last_block] = get_effective_range_of_blocks(first_block_arg, last_block_arg, artifacts_block_head_num - 1 /* it's not possible to read head block artifacts*/, tail_block_num);
    if (!validate_first_and_last_block_and_print_error_if_necessary(block_log_path, first_block, last_block, tail_block_num, artifacts_block_head_num, json_output))
      return ExitCode::BlocksRangeError;

    block_log.close();

    if (json_output)
      std::cout << generate_json_output({
        {"artifacts_file", fc::variant(artifacts->get_artifacts_file().string())},
        {"artifacts", artifacts->get_artifacts_contents(first_block, last_block, header_only)}
    });
    else
      std::cout << fc::json::to_pretty_string(artifacts->get_artifacts_contents(first_block, last_block, header_only)) << "\n";
    return ExitCode::Ok;
  }
  FC_CAPTURE_AND_RETHROW()
}

ExitCode generate_artifacts(const fc::path &block_log_path, appbase::application &app, hive::chain::blockchain_worker_thread_pool &thread_pool, const bool json_output)
{
  try
  {
    auto block_log_reader = get_block_log_wrapper(block_log_path, app, thread_pool, false);
    block_log_reader->close_storage();
    if (json_output)
      print_simple_error_or_result_message("Opened and closed block log file. Artifacts were generated if needed", block_log_path, json_output, false);
    else
      std::cout << "Opened and closed block log file. Artifacts were generated if needed\n";
    return ExitCode::Ok;
  }
  FC_CAPTURE_AND_RETHROW()
}

ExitCode split_block_log(const fc::path &block_log_filename, const int32_t first_block_arg, const int32_t last_block_arg, const fc::path &output_dir, const int32_t files_count_arg, appbase::application &app, hive::chain::blockchain_worker_thread_pool &thread_pool, const bool json_output)
{
  try
  {
    FC_ASSERT(!(first_block_arg != 1 && last_block_arg != -1 && files_count_arg != 0));
    const uint32_t head_block_num = [&block_log_filename, &thread_pool, &app]()
    {
      hive::chain::block_log log(app);
      log.open(block_log_filename, thread_pool, true);
      auto head_block = log.head();
      return head_block ? head_block->get_block_num() : 0;
    }();

    if (!head_block_num)
    {
      print_simple_error_or_result_message("Cannot operate on empty block_log", block_log_filename, json_output, true);
      return ExitCode::BlockLogInvalid;
    }

    if (!fc::exists(output_dir))
    {
      dlog("Creating directories: ${output_dir}", (output_dir));
      fc::create_directories(output_dir);
    }
    size_t head_part_num = 0; // Default value - determine from source log.
    size_t part_count = 0; // Default value - determine from source log.

    const uint32_t tail_block_num = block_log_info::get_first_block_num_for_file_name(block_log_filename);
    const auto [first_block, last_block] = get_effective_range_of_blocks(first_block_arg, last_block_arg, head_block_num, tail_block_num);
    if (!validate_first_and_last_block_and_print_error_if_necessary(block_log_filename, first_block, last_block, tail_block_num, head_block_num, json_output))
      return ExitCode::BlocksRangeError;

    const bool last_block_is_head_block = (last_block == head_block_num);

    if (first_block != 1)
    {
      part_count = ((last_block - first_block) / BLOCKS_IN_SPLIT_BLOCK_LOG_FILE);

      if (last_block_is_head_block || (last_block - first_block != BLOCKS_IN_SPLIT_BLOCK_LOG_FILE))
        ++part_count;
    }

    if (!last_block_is_head_block)
    {
      head_part_num = last_block / BLOCKS_IN_SPLIT_BLOCK_LOG_FILE;
      if (last_block % BLOCKS_IN_SPLIT_BLOCK_LOG_FILE != 0)
        ++head_part_num;
    }

    if (files_count_arg)
    {
      const size_t max_files_limit = head_part_num ? head_part_num : (((head_block_num - first_block + 1) / BLOCKS_IN_SPLIT_BLOCK_LOG_FILE) + 1);

      if (files_count_arg < 0)
      {
        FC_ASSERT(first_block_arg == 1 && last_block_arg == -1);
        part_count = abs(files_count_arg);
      }
      else
      {
        FC_ASSERT(!(first_block_arg != 1 && last_block_arg != -1));
        part_count = static_cast<size_t>(files_count_arg);

        if (last_block_arg == -1)
          head_part_num = static_cast<size_t>(files_count_arg) + (first_block / BLOCKS_IN_SPLIT_BLOCK_LOG_FILE);
      }

      if (max_files_limit < part_count)
      {
        if (json_output)
          std::cout << generate_json_output({
            {"Error", fc::variant("Requested to create more files than possible")},
            {"max_files_limit", fc::variant(max_files_limit)},
            {"requested_files_count", fc::variant(part_count)},
            {"block_log_path", fc::variant(block_log_filename)}
          });
        else
          std::cout << "Requested to create more files: " << std::to_string(part_count) << " than it's possible: " << std::to_string(max_files_limit) << " from given block_log with specific parameters\n";
        return ExitCode::BlocksRangeError;
      }
    }

    uint32_t head_block_number = head_part_num == 0 ? 0 :
      hive::chain::block_log_wrapper::get_number_of_last_block_in_part( head_part_num, MAX_FILES_OF_SPLIT_BLOCK_LOG );
    if (!json_output)
      std::cout << "Splitting block_log ...\n";
    hive::chain::split_block_log(block_log_filename, head_block_number, part_count, app, thread_pool, output_dir);
    if (!json_output)
      std::cout << "Splitting block_log finished\n";
    else
    {
      std::cout << generate_json_output({
        {"Result", fc::variant("Splitting block_log finished")},
        {"output_dir", fc::variant(output_dir)},
        {"block_log_path", fc::variant(block_log_filename)}
      });
    }
    if (!last_block_is_head_block && (last_block % BLOCKS_IN_SPLIT_BLOCK_LOG_FILE != 0))
    {
      const std::string block_log_part_filename_pattern = "block_log_part.0000";
      const std::string last_part_num_as_str = std::to_string(hive::chain::block_log_wrapper::get_part_number_for_block(last_block, MAX_FILES_OF_SPLIT_BLOCK_LOG));
      const size_t lpnas_size = last_part_num_as_str.size();
      const fc::path block_log_file_to_truncate = output_dir / fc::path(std::string("block_log_part.0000").replace(block_log_part_filename_pattern.size() - lpnas_size, lpnas_size, last_part_num_as_str));
      ilog("Truncating ${block_log_file_to_truncate} to block: ${last_block}", (block_log_file_to_truncate)(last_block));
      if (!json_output)
        std::cout << "Requested block_log with head_block_num: " << last_block << " ... truncating " << block_log_file_to_truncate.generic_string() << " to " << last_block << " blocks...\n";
      hive::chain::block_log log(app);
      log.open(block_log_file_to_truncate, thread_pool, false);
      FC_ASSERT(log.head(), "Cannot operate on empty block_log");
      const uint32_t head_block_num = log.read_head()->get_block_num();
      FC_ASSERT(head_block_num > last_block);
      FC_ASSERT(block_log_info::get_first_block_num_for_file_name(block_log_filename) <= last_block);
      log.truncate(last_block);
      ilog("Truncating finished.");
    }
  }
  FC_CAPTURE_AND_RETHROW()
  return ExitCode::Ok;
}

ExitCode merge_block_logs(const fc::path &input_block_log_dir, const fc::path &output_block_log_dir, const int32_t block_number_arg, appbase::application &app, hive::chain::blockchain_worker_thread_pool &thread_pool, const bool json_output)
{
  try
  {
    const auto block_log_reader = hive::chain::block_log_wrapper::create_limited_wrapper(input_block_log_dir, app, thread_pool);
    hive::chain::block_log_wrapper::block_log_open_args args;
    args.data_dir = input_block_log_dir;
    block_log_reader->open_and_init(args, true, nullptr);
    const uint32_t head_block_num = block_log_reader->head_block_num();
    if (head_block_num == 0)
    {
      print_and_log_error("No block_logs detected in input directory", json_output);
      return ExitCode::BlockLogInvalid;
    }
    const uint32_t highest_block_limit = get_effective_range_of_blocks(1, block_number_arg, head_block_num, 1).second;
    if (head_block_num < highest_block_limit)
    {
      print_and_log_error("last head_block_num: " + std::to_string(head_block_num) + " must be bigger than than last_block: " + std::to_string(highest_block_limit), json_output);
      return ExitCode::BlocksRangeError;
    }
    if (!fc::exists(output_block_log_dir))
    {
      dlog("Creating directories: ${output_block_log_dir}", (output_block_log_dir));
      fc::create_directories(output_block_log_dir);
    }
    const auto block_log_writer = hive::chain::block_log_wrapper::create_opened_wrapper(output_block_log_dir.generic_string() + "/" + hive::chain::block_log_file_name_info::_legacy_file_name,
      app, thread_pool, false /* read_only */);

    auto writer_processor = [&block_log_writer](const std::shared_ptr<hive::chain::full_block_type>& full_block) -> bool
    {
      block_log_writer->append(full_block, false /* is_at_live_sync */);
      return true;
    };

    if (!json_output)
      std::cout << "Merging process began.\n";
    block_log_reader->process_blocks(1, highest_block_limit, writer_processor, thread_pool);

    if (json_output)
      std::cout << generate_json_output({
        {"Result", fc::variant("Merging process finished")},
        {"output_dir", fc::variant(output_block_log_dir)},
        {"block_log_path", fc::variant(input_block_log_dir)}
      });
    else
      std::cout << "Merging process finished.\n";
  }
  FC_CAPTURE_AND_RETHROW()
  return ExitCode::Ok;
}

int main(int argc, char **argv)
{
  boost::program_options::options_description minor_options("Minor options");
  minor_options.add_options()("jobs,j", boost::program_options::value<unsigned>()->default_value(4), "The number of worker threads to spawn. (max 16)");
  minor_options.add_options()("help,h", "Print usage instructions");
  minor_options.add_options()("version,v", "Print version info.");
  minor_options.add_options()("log-path,l", boost::program_options::value<boost::filesystem::path>()->value_name("filename")->default_value("./block_log_util.log"), "Path to log file. All logs are saved into this file.");
  minor_options.add_options()("json", "Output will give just a json response with result of operation.");

  boost::program_options::options_description block_log_operations("block_log operations");
  block_log_operations.add_options()("block-log,i", boost::program_options::value<boost::filesystem::path>(), "Path to input block-log or directory with split block log for processing, depending on the operation opening in read only (RO) or read & write (RW) mode. Artifacts are required for all operations, expect 'generate-artifacts', 'find-end', 'get-head-block-number'");
  block_log_operations.add_options()("compare", "Compare input block_log with another block_log. (Both block_logs opened in RO mode) (operate both on sigle block log or directory with split block log)");
  block_log_operations.add_options()("find-end", "Check if input block_log is not corrupted. Finds out place where last full block is successfully stored in block_log and proposes block_log truncation if recommended. (single block_log operation)");
  block_log_operations.add_options()("generate-artifacts", "Open input block_log in read & write mode and generate artifacts file if necessary. (operate both on sigle block log or directory with split block log)");
  block_log_operations.add_options()("get-block", "Get range of blocks, (Block_log opened in RO mode) (operate both on sigle block log or directory with split block log)");
  block_log_operations.add_options()("get-block-artifacts", "Get range of artifacts. Allows to run full artifacts verification, (Block_log opened in RO mode) (single block_log operation)");
  block_log_operations.add_options()("get-block-ids", "Get range of blocks ids. (Block_log opened in RO mode) (operate both on sigle block log or directory with split block log)");
  block_log_operations.add_options()("get-head-block-number", "Get block_log head block number. (Block_log opened in RO mode) (operate both on sigle block log or directory with split block log");
  block_log_operations.add_options()("sha256sum", "Verify sha256 checksums in block-log. (Block_log opened in RO mode) (single block_log operation)");
  block_log_operations.add_options()("split", "Split legacy monolithic block log file into new-style multiple part files.");
  block_log_operations.add_options()("truncate", "Truncate block log to given block number. (single block_log operation)");

  boost::program_options::options_description additional_operations("additional operations");
  additional_operations.add_options()("verify-checksums-from-file", boost::program_options::value<boost::filesystem::path>()->value_name("filename"), "Verify sha256 from text file.");
  additional_operations.add_options()("merge-block-logs", "Merge new-style split block log part files into legacy monolithic single file.");

  boost::program_options::options_description merge_block_logs_options("merge-block-logs options");
  merge_block_logs_options.add_options()("input,i", boost::program_options::value<boost::filesystem::path>()->value_name("directory"), "Directory containing split block log part files to be merged. Can be the same as --output");
  merge_block_logs_options.add_options()("output,o", boost::program_options::value<boost::filesystem::path>()->value_name("directory"), "Directory where merged block log file should be created.. Can be the same as --input");
  merge_block_logs_options.add_options()("block-number,n", boost::program_options::value<int32_t>()->value_name("n"), "Merge block_log files up to given block number (inclusive). Negative numbers mean distance from end (-1 is head block of last split block_log). Defaults to -1.");

  // args for sha256sum subcommand
  boost::program_options::options_description sha256sum_options("sha256sum options");
  sha256sum_options.add_options()("checkpoint", boost::program_options::value<uint32_t>()->value_name("n"), "Print the SHA256 every n blocks");

  // args for compare subcommand
  boost::program_options::options_description cmp_options("compare options");
  cmp_options.add_options()("second-block-log", boost::program_options::value<boost::filesystem::path>(), "The second block log");
  cmp_options.add_options()("from", boost::program_options::value<int32_t>()->value_name("n"), "Start comparison at block n, assume all blocks below n are equivalent without checking. Negative numbers mean distance from end (-1 is head block). Defaults to 1.");
  cmp_options.add_options()("to", boost::program_options::value<int32_t>()->value_name("m"), "End comparison at block m, assume all blocks above m are equivalent without checking. Negative numbers mean distance from end (-1 is head block). Defaults to -1.");

  // args for truncate subcommand
  boost::program_options::options_description truncate_options("truncate options");
  truncate_options.add_options()("block-number", boost::program_options::value<uint32_t>()->value_name("n")->required(), "Truncate to the given block number");
  truncate_options.add_options()("force,f", "Don't prompt for confirmation");

  // args for get-block-ids subcommand
  boost::program_options::options_description get_block_ids_options("get-block-ids options");
  get_block_ids_options.add_options()("block-number,n", boost::program_options::value<int32_t>()->value_name("x"), "Equivalent of '--from x --to x'.");
  get_block_ids_options.add_options()("from", boost::program_options::value<int32_t>()->value_name("n"), "Return range of ids starting from x. Negative numbers mean distance from end (-1 is head block). Defaults to 1.");
  get_block_ids_options.add_options()("to", boost::program_options::value<int32_t>()->value_name("m"), "Return range of ids ending at x (inclusive). Negative numbers mean distance from end (-1 is head block). Defaults to -1.");
  get_block_ids_options.add_options()("print-block-numbers", "print the block number before the corresponding block id");

  // args for get-block subcommand
  boost::program_options::options_description get_block_options("get-block options");
  get_block_options.add_options()("block-number,n", boost::program_options::value<int32_t>()->value_name("x"), "Equivalent of '--from x --to x'.");
  get_block_options.add_options()("from", boost::program_options::value<int32_t>()->value_name("x"), "Return range of blocks starting from x. Negative numbers mean distance from end (-1 is head block). Defaults to 1.");
  get_block_options.add_options()("to", boost::program_options::value<int32_t>()->value_name("x"), "Return range of blocks ending at x (inclusive). Negative numbers mean distance from end (-1 is head block). Defaults to -1.");
  get_block_options.add_options()("header-only", "only print the block header");
  get_block_options.add_options()("pretty", "pretty-print the JSON");
  get_block_options.add_options()("binary", "output the binary form of the block (--output-dir is obligatory for this feature)");
  get_block_options.add_options()("output-dir,o", boost::program_options::value<boost::filesystem::path>()->value_name("directory"), "Directory where results will be stored.");

  // args for get-block-artifacts subcommand
  boost::program_options::options_description get_block_artifacts_options("get-block-artifacts options");
  get_block_artifacts_options.add_options()("block-number,n", boost::program_options::value<int32_t>()->value_name("x"), "Equivalent of '--from x --to x'.");
  get_block_artifacts_options.add_options()("from", boost::program_options::value<int32_t>()->value_name("n"), "Return range of artifacts from given block number (inclusive). Negative numbers mean distance from end (-1 is head block). Defaults to 1.");
  get_block_artifacts_options.add_options()("to", boost::program_options::value<int32_t>()->value_name("m"), "Return range of artifacts to given block number (inclusive). Negative numbers mean distance from end (-1 is head block). Defaults to -1.");
  get_block_artifacts_options.add_options()("header-only", "only print the artifacts header");
  get_block_artifacts_options.add_options()("do-full-artifacts-verification-match-check", "Performs check if all artifacts from file matches block_log");

  // args for split subcommand
  boost::program_options::options_description split_block_log_options("split options");
  split_block_log_options.add_options()("output-dir,o", boost::program_options::value<boost::filesystem::path>()->value_name("directory"), "Directory where block_log files will be stored.");
  {
    std::string info = "Split block_log file from given block number (exclusive). Block number must be multiplicity of " + std::to_string(BLOCKS_IN_SPLIT_BLOCK_LOG_FILE) + ". Defaults to 0.";
    split_block_log_options.add_options()("from", boost::program_options::value<int32_t>()->value_name("n"), info.c_str());
    info = "Split block_log file to given block number (inclusive). Negative numbers mean distance from end (-1 is head block). Defaults to -1.";
    split_block_log_options.add_options()("to", boost::program_options::value<int32_t>()->value_name("m"), info.c_str());
    info = "Number of files which will be created. Every block_log file will contain " + std::to_string(BLOCKS_IN_SPLIT_BLOCK_LOG_FILE) + " blocks. Cannot be used together with both from and to."
           "If --from or nothing specified, tool will create c files from block m, if --to specified, it will create c files to block m. Negative numbers mean distance from end (-1 is file contains head block, but cannot be used with --from or --to).";
    split_block_log_options.add_options()("files-count", boost::program_options::value<int32_t>()->value_name("c"), info.c_str());
  }

  const auto print_usage = [&]() {
    print_version();
    std::cout << "\n";
    std::cout << "Exit codes:\n0 - Success\n1 - log file couldn not be created\n2 - Invalid argument - used or passed wrong arguments or their value is wrong\n" <<
    "3 - Specified blocks range is wrong - block log doesn't have required block(s) or is empty\n4 - artifacts file is not valid\5 - block_log is invalid\n" <<
    "6 - operation required to switch from read only to write mode, but it failed\n7 - some exception during work occured\n8 - block_log operation failed for some reason, for example no right to create file\n\n";
    std::cout << minor_options << "\n";
    std::cout << block_log_operations << "\n";
    std::cout << additional_operations << "\n";
    std::cout << cmp_options << "\n";
    std::cout << get_block_options << "\n";
    std::cout << get_block_artifacts_options << "\n";
    std::cout << get_block_ids_options << "\n";
    std::cout << sha256sum_options << "\n";
    std::cout << split_block_log_options << "\n";
    std::cout << truncate_options << "\n";
    std::cout << merge_block_logs_options << "\n";
  };

  try
  {
    boost::program_options::variables_map options_map;

    auto update_options_map = [&options_map, argc, &const_argv = std::as_const(argv)](const boost::program_options::options_description &options)
    {
      boost::program_options::parsed_options parsed_options = boost::program_options::command_line_parser(argc, const_argv).options(options).allow_unregistered().run();

      boost::program_options::store(parsed_options, options_map);
    };

    auto get_arguments_as_string = [](const boost::program_options::variables_map &options_map) -> std::string
    {
      std::stringstream ss;
      ss << "{";
      for (const auto &el : options_map)
      {
        ss << el.first.c_str() << " : ";
        const auto value = el.second.value();
        if (auto v = boost::any_cast<int>(&value))
          ss << *v;
        else if (auto v = boost::any_cast<std::string>(&value))
          ss << *v;
        else if (auto v = boost::any_cast<boost::filesystem::path>(&value))
          ss << v->string();
        ss << ",";
      }
      std::string arguments_list = ss.str();
      char &last_char = arguments_list.back();
      last_char = '}';
      return arguments_list;
    };

    update_options_map(minor_options);
    update_options_map(block_log_operations);
    update_options_map(additional_operations);

    fc::logging_config logging_config;
    fc::file_appender::config file_config;
    {
      const fc::path log_path = options_map["log-path"].as<boost::filesystem::path>();
      options_map.erase("log-path");
      if (fc::exists(log_path) && fc::is_directory(log_path))
      {
        std::cerr << log_path.string() << " points to directory. You must specify a file for place for logs (--log-path/-l)\n";
        return ExitCode::LogFileError;
      }

      file_config.filename = log_path;
    }
    file_config.flush = true;
    file_config.rotate = false;

    logging_config.appenders.push_back(fc::appender_config("default", "file", fc::variant(file_config)));
    logging_config.loggers = {fc::logger_config("default")};
    logging_config.loggers.front().level = fc::log_level::all;
    logging_config.loggers.front().appenders = {"default"};
    fc::configure_logging(logging_config);

    const bool json_output = options_map.count("json") ? true : false;
    options_map.erase("json");

    const unsigned threads_num = options_map["jobs"].as<unsigned>();
    if (threads_num > 16)
    {
      print_and_log_error("jobs limit is 16, cannot use " + std::to_string(threads_num), json_output);
      return ExitCode::InvalidArgumentError;
    }

    options_map.erase("jobs");

    if (options_map.count("version"))
    {
      print_version();
      return ExitCode::Ok;
    }
    else if (options_map.count("help"))
    {
      print_usage();
      return ExitCode::Ok;
    }
    else if (options_map.empty())
    {
      print_and_log_error("Not valid arguments were passed via command line. Use --help or -h to get help", json_output);
      return ExitCode::InvalidArgumentError;
    }
    // that's additional available operation, which doesn't request block_log file
    if (options_map.count("verify-checksums-from-file"))
    {
      const fc::path path_to_file = options_map["verify-checksums-from-file"].as<boost::filesystem::path>();

      options_map.erase("verify-checksums-from-file");
      if (!options_map.empty())
      {
        print_and_log_error("You cannot perform block_log operation if you requested to perform one of additional operations (ambiguous arguments). Unnecessary arguments: " + get_arguments_as_string(options_map), json_output);
        return ExitCode::InvalidArgumentError;
      }

      shutdown_executor se(threads_num);

      dlog("block_log_util will perform verify-checksums-from-file operation on file: ${path_to_file}", (path_to_file));
      return validate_block_log_checksums_from_file(path_to_file, se.the_app, se.thread_pool, json_output);
    }
    else if (options_map.count("merge-block-logs"))
    {
      update_options_map(merge_block_logs_options);
      options_map.erase("merge-block-logs");

      if (!options_map.count("input") || !options_map.count("output"))
      {
        print_and_log_error("input directory with split block_log and output directory for new block_log must be specified for merge block logs operation", json_output);
        return ExitCode::InvalidArgumentError;
      }

      const fc::path input_block_log_files_dir = options_map["input"].as<boost::filesystem::path>();
      const fc::path output_block_log_file_dir = options_map["output"].as<boost::filesystem::path>();
      options_map.erase("input");
      options_map.erase("output");
      options_map.erase("block-log"); // because -i is used for main block_log operations, using -i for merging will add `input` and `block-log` to options map.

      if (!fc::is_directory(input_block_log_files_dir))
      {
        print_and_log_error("input for merge block logs operation must be a directory", json_output);
        return ExitCode::InvalidArgumentError;
      }
      if (!fc::is_directory(output_block_log_file_dir))
      {
        print_and_log_error("output for merge block logs operation must be a directory", json_output);
        return ExitCode::InvalidArgumentError;
      }
      if (fc::directory_size(input_block_log_files_dir) == 0)
      {
        print_and_log_error("input directory size for merge block logs operation is 0 - looks like there are no files inside", json_output);
        return ExitCode::InvalidArgumentError;
      }

      const int32_t max_block_number = options_map.count("block-number") ? options_map["block-number"].as<int32_t>() : -1;

      if (options_map.count("block-number"))
        options_map.erase("block-number");

      if (!options_map.empty())
      {
        print_and_log_error("You cannot perform block_log operation if you requested to perform one of additional operations (ambiguous arguments). Unnecessary arguments: " + get_arguments_as_string(options_map), json_output);
        return ExitCode::InvalidArgumentError;
      }

      shutdown_executor se(threads_num);

      dlog("block_log_util will perform merge block logs operation. Input directory: ${input_block_log_files_dir}, output directory: ${output_block_log_file_dir}, block-number: ${max_block_number}", (input_block_log_files_dir)(output_block_log_file_dir)(max_block_number));
      return merge_block_logs(input_block_log_files_dir, output_block_log_file_dir, max_block_number, se.the_app, se.thread_pool, json_output);
    }
    // we should handle operation which request block_log file
    else
    {
      if (!options_map.count("block-log"))
      {
        print_and_log_error("--block-log with path to block_log file is requested", json_output);
        return ExitCode::InvalidArgumentError;
      }

      const fc::path block_log_path = options_map["block-log"].as<boost::filesystem::path>();
      options_map.erase("block-log");

      if (!fc::exists(block_log_path))
      {
        print_and_log_error(block_log_path.string() + " does not exist (input block_log)", json_output);
        return ExitCode::InvalidArgumentError;
      }

      auto blocks_range_options_is_wrong = [&options_map, json_output]() -> bool {
        if ((options_map.count("from") || options_map.count("to")) && options_map.count("block-number"))
        {
          print_and_log_error("'--from' and '--to' cannot be specified with --block-number", json_output);
          return true;
        }
        
        if (options_map.count("from") && options_map["from"].as<int32_t>() == 0)
        {
          print_and_log_error("'--from' cannot be 0", json_output);
          return true;
        }
        
        if (options_map.count("to") && options_map["to"].as<int32_t>() == 0)
        {
          print_and_log_error("'--to' cannot be 0", json_output);
          return true;
        }

        if (options_map.count("block-number") && options_map["block-number"].as<int32_t>() == 0)
        {
          print_and_log_error("'--block-number' cannot be 0", json_output);
          return true;
        }
        
        return false;
      };

      auto get_first_and_last_block_from_options = [&options_map]() -> std::pair<int32_t, int32_t>
      {
        int32_t first_block, last_block;
        if (options_map.count("block-number"))
          first_block = last_block = options_map["block-number"].as<int32_t>();
        else
        {
          first_block = options_map.count("from") ? options_map["from"].as<int32_t>() : 1;
          last_block = options_map.count("to") ? options_map["to"].as<int32_t>() : -1;
        }

        return std::make_pair(first_block, last_block);
      };

      if (options_map.empty())
      {
        print_and_log_error("Could not recognize which operation should be performed on block_log", json_output);
        return ExitCode::InvalidArgumentError;
      }
      else if (options_map.size() > 1)
      {
        print_and_log_error("You specified more than one block_log operation. One one at once is allowed. Passed arguments: " + get_arguments_as_string(options_map), json_output);
        return ExitCode::InvalidArgumentError;
      }

      shutdown_executor se(threads_num);

      const bool input_block_log_is_directory = fc::is_directory(block_log_path);

      if (!input_block_log_is_directory)
      {
        if (fc::file_size(block_log_path) == 0)
        {
          print_and_log_error(block_log_path.string() + " is an empty file!", json_output);
          return ExitCode::InvalidArgumentError;
        }
        if (!hive::chain::block_log_file_name_info::is_block_log_file_name(block_log_path))
        {
          print_and_log_error(block_log_path.string() + " file has not valid block_log file name!", json_output);
          return ExitCode::InvalidArgumentError;
        }
      }
      else if (fc::directory_size(block_log_path) == 0)
      {
        print_and_log_error("input directory with split block_log size is 0 - looks like there are no files inside", json_output);
        return ExitCode::InvalidArgumentError;
      }

      auto validate_last_block_in_split_block_log = [&block_log_path, &se, json_output](const uint32_t last_block) -> bool
      {
        uint32_t head_block_num;
        if (fc::is_directory(block_log_path))
        {
          const auto block_log_reader = hive::chain::block_log_wrapper::create_limited_wrapper(block_log_path, se.the_app, se.thread_pool);
          hive::chain::block_log_wrapper::block_log_open_args args;
          args.data_dir = block_log_path;
          block_log_reader->open_and_init(args, true, nullptr);
          head_block_num = block_log_reader->head_block_num();
        }
        else
        {
          hive::chain::block_log block_log(se.the_app);
          block_log.open(block_log_path, se.thread_pool, true, false);
          head_block_num = block_log.head() ? block_log.head()->get_block_num() : 0;
        }
        if (head_block_num == 0)
        {
          print_and_log_error("No block_logs detected in input directory", json_output);
          return false;
        }
        const uint32_t highest_block_limit = get_effective_range_of_blocks(1, last_block, head_block_num, 1).second;
        if (head_block_num < highest_block_limit)
        {
          print_and_log_error("Arguments specified that operation will require block: " + std::to_string(highest_block_limit) + " while head block in split block log is: " + std::to_string(head_block_num), json_output);
          return false;
        }

        return true;
      };

      if (options_map.count("compare"))
      {
        if (!input_block_log_is_directory && artifacts_file_is_not_valid(block_log_path, json_output))
          return ExitCode::ArtifactsFileNotValid;
        update_options_map(cmp_options);
        if (!options_map.count("second-block-log"))
        {
          print_and_log_error("Missing \'--second-block-log\' for compare operation", json_output);
          return ExitCode::InvalidArgumentError;
        }

        const fc::path second_block_log_path = options_map["second-block-log"].as<boost::filesystem::path>();

        const auto [first_block, last_block] = get_first_and_last_block_from_options();
        dlog("block_log_util will perform compare operation between block_log: ${block_log_path} and ${second_block_log_path}, from: ${first_block}, to: ${last_block}", (block_log_path)(second_block_log_path)(first_block)(last_block));
        return compare_block_logs(block_log_path, second_block_log_path, first_block, last_block, se.the_app, se.thread_pool, json_output);
      }
      else if (options_map.count("find-end"))
      {
        if (!input_block_log_is_directory && input_block_log_is_directory)
        {
          print_and_log_error("find-end operation accepts only single block_log", json_output);
          return ExitCode::InvalidArgumentError;
        }
        dlog("block_log_util will perform find-end operation on block_log: ${block_log_path}", (block_log_path));
        return find_end(block_log_path, json_output);
      }
      else if (options_map.count("generate-artifacts"))
      {
        dlog("block_log_util will open block_log: ${block_log_path} and generate artifacts file if necessary", (block_log_path));
        return generate_artifacts(block_log_path, se.the_app, se.thread_pool, json_output);
      }
      else if (options_map.count("get-block"))
      {
        if (!input_block_log_is_directory && artifacts_file_is_not_valid(block_log_path, json_output))
          return ExitCode::ArtifactsFileNotValid;
  
        update_options_map(get_block_options);
        if (blocks_range_options_is_wrong())
          return ExitCode::InvalidArgumentError;
        const auto [first_block, last_block] = get_first_and_last_block_from_options();
        if (!validate_last_block_in_split_block_log(last_block))
          return ExitCode::BlocksRangeError;

        const fc::optional<fc::path> output_dir = options_map.count("output-dir") ? options_map["output-dir"].as<boost::filesystem::path>() : fc::optional<fc::path>();
        const bool header_only = options_map.count("header-only") != 0;
        const bool pretty = options_map.count("pretty") != 0;
        const bool binary = options_map.count("binary") != 0;
        ilog("block_log_util will perform get_block operation on block_log: ${block_log_path}, parameters - first_block: ${first_block}, last_block: ${last_block}, header_only: ${header_only}, pretty: ${pretty}, binary: ${binary}, output_dir: ${output_dir} ",
             (block_log_path)(first_block)(last_block)(header_only)(pretty)(binary)(output_dir));

        return get_block_range(block_log_path, first_block, last_block, output_dir, header_only, pretty, binary, se.the_app, se.thread_pool, json_output);
      }
      else if (options_map.count("get-block-artifacts"))
      {
        if (input_block_log_is_directory)
        {
          print_and_log_error("get block artifacts operation accepts only single block_log", json_output);
          return ExitCode::InvalidArgumentError;
        }
  
        if (artifacts_file_is_not_valid(block_log_path, json_output))
          return ExitCode::ArtifactsFileNotValid;

        update_options_map(get_block_artifacts_options);
        const bool header_only = options_map.count("header-only") != 0;
        const bool full_match_verification = options_map.count("do-full-artifacts-verification-match-check") != 0;

        if (header_only && (options_map.count("from") || options_map.count("to") || options_map.count("block-number")))
        {
          print_and_log_error("'--get-block-artifacts' with header-only option cannot accept '--from', '--to' or '--block-number' parameters.", json_output);
          return ExitCode::InvalidArgumentError;
        }

        if (blocks_range_options_is_wrong())
          return ExitCode::InvalidArgumentError;
        const auto [first_block, last_block] = get_first_and_last_block_from_options();
        dlog("block_log_util will perform get-block-artifacts operation on block_log: ${block_log_path}, parameters - first_block: ${first_block}, last_block: ${last_block}, header_only: ${header_only}, full_match_verification: ${full_match_verification}",
             (block_log_path)(first_block)(last_block)(header_only)(full_match_verification));
        return get_block_artifacts(block_log_path, first_block, last_block, header_only, full_match_verification, se.the_app, se.thread_pool, json_output);
      }
      else if (options_map.count("get-block-ids"))
      {
        if (!input_block_log_is_directory && artifacts_file_is_not_valid(block_log_path, json_output))
          return ExitCode::ArtifactsFileNotValid;

        update_options_map(get_block_ids_options);
        if (blocks_range_options_is_wrong())
          return ExitCode::InvalidArgumentError;
        const auto [first_block, last_block] = get_first_and_last_block_from_options();
        if (!validate_last_block_in_split_block_log(last_block))
          return ExitCode::BlocksRangeError;
        const bool print_block_numbers = options_map.count("print-block-numbers") != 0;

        dlog("block_log_util will perform get-block-ids operation on ${block_log_path} - parameters - first_block: ${first_block}, last_block: ${last_block}, print_block_numbers: ${print_block_numbers}",
             (block_log_path)(first_block)(last_block)(print_block_numbers));
        return get_block_ids(block_log_path, first_block, last_block, print_block_numbers, se.the_app, se.thread_pool, json_output);
      }
      else if (options_map.count("sha256sum"))
      {
        if (input_block_log_is_directory)
        {
          print_and_log_error("split operation accepts only single block_log", json_output);
          return ExitCode::InvalidArgumentError;
        }
        if (artifacts_file_is_not_valid(block_log_path, json_output))
          return ExitCode::ArtifactsFileNotValid;

        update_options_map(sha256sum_options);
        const fc::optional<uint32_t> checkpoint_every_n_blocks = options_map.count("checkpoint") ? options_map["checkpoint"].as<uint32_t>() : fc::optional<uint32_t>();
        dlog("block_log_util will perform sha256sum operation on block_log: ${block_log_path}, checkpoint_every_n_blocks: ${checkpoint_every_n_blocks}", (block_log_path)(checkpoint_every_n_blocks));
        return checksum_block_log(block_log_path, checkpoint_every_n_blocks, se.the_app, se.thread_pool, json_output);
      }
      else if (options_map.count("split"))
      {
        if (input_block_log_is_directory)
        {
          print_and_log_error("split operation accepts only single block_log", json_output);
          return ExitCode::InvalidArgumentError;
        }
        if (artifacts_file_is_not_valid(block_log_path, json_output))
          return ExitCode::ArtifactsFileNotValid;

        update_options_map(split_block_log_options);

        int32_t files_count = 0;
        if (options_map.count("files-count"))
        {
          const bool from_given = options_map.count("from");
          const bool to_given = options_map.count("to");
          if (from_given && to_given)
          {
            print_and_log_error("--files-count cannot be specified together with both from and to in split operation", json_output);
            return ExitCode::InvalidArgumentError;
          }

          files_count = options_map["files-count"].as<int32_t>();
          if (files_count < 0 && (from_given || to_given))
          {
            print_and_log_error("if --files-count is negative, cannot use --from or --to in split operation", json_output);
            return ExitCode::InvalidArgumentError;
          }
        }

        if (!options_map.count("output-dir"))
        {
          print_and_log_error("output_dir is necessary when splitting block_log in order to know where new block_log files should be stored.", json_output);
          return ExitCode::InvalidArgumentError;
        }
        
        if (blocks_range_options_is_wrong())
          return ExitCode::InvalidArgumentError;
        const auto [first_block, last_block] = get_first_and_last_block_from_options();
        if (!validate_last_block_in_split_block_log(last_block))
          return ExitCode::BlocksRangeError;
        if (!(first_block == 1 || first_block % BLOCKS_IN_SPLIT_BLOCK_LOG_FILE == 0))
        {
          print_and_log_error("first block should be 1 or multiplicity of " + std::to_string(BLOCKS_IN_SPLIT_BLOCK_LOG_FILE), json_output);
          return ExitCode::InvalidArgumentError;
        }

        const fc::path output_dir = options_map["output-dir"].as<boost::filesystem::path>();
        dlog("block_log_util will perform split block_log operation on ${block_log_path} - parameters - first_block: ${first_block}, last_block: ${last_block}, output_dir: ${output_dir}, files_count: ${files_count}",
             (block_log_path)(first_block)(last_block)(output_dir)(files_count));
        return split_block_log(block_log_path, first_block, last_block, output_dir, files_count, se.the_app, se.thread_pool, json_output);
      }
      else if (options_map.count("truncate"))
      {
        if (input_block_log_is_directory)
        {
          print_and_log_error("split operation accepts only single block_log", json_output);
          return ExitCode::InvalidArgumentError;
        }
        if (artifacts_file_is_not_valid(block_log_path, json_output))
          return ExitCode::ArtifactsFileNotValid;

        update_options_map(truncate_options);
        if (!options_map.count("block-number"))
        {
          print_and_log_error("\"--block-number\" is mandatory for truncate operation", json_output);
          return ExitCode::InvalidArgumentError;
        }

        const uint32_t block_number = options_map["block-number"].as<uint32_t>();
        const bool force = options_map.count("force") ? true : false;
        dlog("block_log_util will perform truncate operation on block_log: ${block_log_path}, block_number: ${block_number}, force: ${force}", (block_log_path)(block_number)(force));
        return truncate_block_log(block_log_path, block_number, force, se.the_app, se.thread_pool, json_output);
      }
      else if (options_map.count("get-head-block-number"))
      {
        dlog("block_log_util will perform get-head-block-number operation on block_log: ${block_log_path}", (block_log_path));
        return get_head_block_number(block_log_path, se.the_app, se.thread_pool, json_output);
      }
      else
      {
        print_and_log_error("block_log operation was not recognized. cmd arguments: " + get_arguments_as_string(options_map), json_output);
        return ExitCode::InvalidArgumentError;
      }
    }
  }
  catch (const fc::exception& e)
  {
    elog("fc error occured: ${error}", ("error", e.to_detail_string()));
    std::cerr << "\n\nFC Error occured: " << e.to_detail_string() << "\n";
    return ExitCode::ExceptionOccured;
  }
  catch (const boost::program_options::error& e)
  {
    elog("boost error occured: ${error}", ("error", e.what()));
    std::cerr << "\n\n boost program options error occured: " << e.what() << "\n";
    return ExitCode::ExceptionOccured;
  }
  catch (const std::exception& e)
  {
    elog("std error occured: ${error}", ("error", e.what()));
    std::cerr << "\n\nstd error occured: " << e.what() << "\n";
    return ExitCode::ExceptionOccured;
  }
  catch(...)
  {
    elog("An unknown error occured");
    std::cerr << "\n\nAn unknown error occured\n";
    return ExitCode::ExceptionOccured;
  }

  return ExitCode::Ok;
}

FC_REFLECT_DERIVED(extended_signed_block_header, (hive::protocol::signed_block_header),
                   (block_id)(signing_key))

FC_REFLECT_DERIVED(extended_signed_block, (extended_signed_block_header),
                   (transactions))