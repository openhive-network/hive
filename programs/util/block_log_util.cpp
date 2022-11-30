#include <fc/log/logger.hpp>
#include <fc/log/logger_config.hpp>
#include <fc/log/console_appender.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/io/json.hpp>
#include <fc/filesystem.hpp>
#include <fc/thread/thread.hpp>
#include <hive/chain/block_log.hpp>
#include <hive/chain/detail/block_attributes.hpp>
#include <hive/chain/block_compression_dictionaries.hpp>
#include <hive/chain/full_block.hpp>
#include <hive/chain/blockchain_worker_thread_pool.hpp>
#include <hive/chain/block_compression_dictionaries.hpp>
#include <hive/utilities/io_primitives.hpp>

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

struct block_log_hashes
{
  std::optional<fc::sha256> final_hash;
  std::map<uint32_t, fc::sha256> checkpoints;
};
typedef std::map<fc::path, block_log_hashes> block_logs_and_hashes_type;

void append_full_block(const std::shared_ptr<hive::chain::full_block_type>& full_block, uint64_t& start_offset, fc::sha256::encoder& encoder)
{
  const hive::chain::uncompressed_block_data& uncompressed = full_block->get_uncompressed_block();
  // the block log gets the block's raw data followed by the 8-byte offset of the start of the block
  encoder.write(uncompressed.raw_bytes.get(), uncompressed.raw_size);
  encoder.write((const char*)&start_offset, sizeof(start_offset));
  start_offset += uncompressed.raw_size + sizeof(start_offset);
}

void checksum_block_log(const fc::path& block_log, std::optional<uint32_t> checkpoint_every_n_blocks) 
{
  try
  {
    hive::chain::block_log log;
    log.open(block_log, true);

    uint64_t uncompressed_block_start_offset = 0; // as we go, keep track of the block's starting offset if it were recorded uncompressed
    fc::sha256::encoder block_log_sha256_encoder;

    const uint32_t head_block_num = log.head() ? log.head()->get_block_num() : 0;
    if (head_block_num) // if the log is non-empty
    {
      const auto process_block = [&](const std::shared_ptr<hive::chain::full_block_type> full_block) {
        if (full_block->get_block_num() % 1000000 == 0)
          dlog("processed block ${current} of ${total}", ("current", full_block->get_block_num())("total", head_block_num));

        append_full_block(full_block, uncompressed_block_start_offset, block_log_sha256_encoder);

        if (checkpoint_every_n_blocks && full_block->get_block_num() % *checkpoint_every_n_blocks == 0 && full_block->get_block_num() != head_block_num)
        {
          fc::sha256::encoder block_log_encoder_up_to_this_block(block_log_sha256_encoder);
          std::cout << block_log_encoder_up_to_this_block.result().str() << "  " << block_log.string() << "@" << full_block->get_block_num() << "\n";
        }

        return true;
      };
      log.for_each_block(1, head_block_num, process_block, hive::chain::block_log::for_each_purpose::decompressing);
    }
    fc::sha256 final_hash = block_log_sha256_encoder.result();
    std::cout << final_hash.str() << "  " << block_log.string();
    if (checkpoint_every_n_blocks)
      std::cout << "@" << head_block_num;
    std::cout << "\n";
  }
  FC_LOG_AND_RETHROW()
}

bool validate_block_log_checksum(const fc::path& block_log, const block_log_hashes& hashes_to_validate)
{
  try
  {
    hive::chain::block_log log;
    log.open(block_log, true);

    uint64_t uncompressed_block_start_offset = 0; // as we go, keep track of the block's starting offset if it were recorded uncompressed
    fc::sha256::encoder block_log_sha256_encoder;

    std::optional<uint32_t> last_good_checkpoint_block_number;
    auto next_checkpoint_iter = hashes_to_validate.checkpoints.begin();
    bool all_hashes_matched = true;

    const uint32_t head_block_num = log.head() ? log.head()->get_block_num() : 0;
    if (head_block_num) // if the log is non-empty
    {
      const auto process_block = [&](const std::shared_ptr<hive::chain::full_block_type> full_block) {
        if (full_block->get_block_num() % 1000000 == 0)
          dlog("processed block ${current} of ${total}", ("current", full_block->get_block_num())("total", head_block_num));

        append_full_block(full_block, uncompressed_block_start_offset, block_log_sha256_encoder);

        if (next_checkpoint_iter != hashes_to_validate.checkpoints.end())
        {
          const uint32_t checkpoint_block = next_checkpoint_iter->first;
          const fc::sha256& checkpoint_hash = next_checkpoint_iter->second;
          if (full_block->get_block_num() == checkpoint_block)
          {
            const fc::sha256 block_log_hash_up_to_this_block = fc::sha256::encoder(block_log_sha256_encoder).result();

            std::cout << block_log.string() << "@" << checkpoint_block << ": ";
            if (block_log_hash_up_to_this_block != checkpoint_hash)
            {
              std::cout << "FAILED (checksum mismatch)\n";
              all_hashes_matched = false;
              for (; next_checkpoint_iter != hashes_to_validate.checkpoints.end(); ++next_checkpoint_iter)
                std::cout << block_log.string() << "@" << next_checkpoint_iter->first << ": FAILED (presumed checksum mismatch because of checksum mismatch on earlier block " << checkpoint_block << ")\n";
              return false; // stop iterating
            }
            else
            {
              std::cout << "OK\n";
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
      log.for_each_block(1, head_block_num, process_block, hive::chain::block_log::for_each_purpose::decompressing);
    }
    fc::sha256 final_hash = block_log_sha256_encoder.result();

    for (; next_checkpoint_iter != hashes_to_validate.checkpoints.end(); ++next_checkpoint_iter)
    {
      std::cout << block_log.string() << "@" << next_checkpoint_iter->first << ": FAILED (block log only contains " << head_block_num << " blocks)\n";
      all_hashes_matched = false;
    }

    if (hashes_to_validate.final_hash)
    {
      std::cout << block_log.string() << ": ";

      if (final_hash != *hashes_to_validate.final_hash)
      {
        std::cout << "FAILED\n";
        all_hashes_matched = false;
      }
      else
        std::cout << "OK\n";
    }

    if (all_hashes_matched)
    {
      if (!hashes_to_validate.final_hash &&
          !hashes_to_validate.checkpoints.empty() &&
          hashes_to_validate.checkpoints.rbegin()->first != head_block_num)
        std::cout << "WARNING: all checksums matched, but your block log contains " << head_block_num - *last_good_checkpoint_block_number << " blocks past the last checkpoint\n";
    }

    return all_hashes_matched;
  }
  FC_CAPTURE_AND_RETHROW()
}

block_logs_and_hashes_type parse_checkpoints(const fc::path& checkpoints_file)
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
          std::optional<uint32_t> block_number;
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

bool validate_block_log_checksums_from_file(const fc::path& checksums_file)
{
  try
  {
    block_logs_and_hashes_type hashes_in_checksums_file = parse_checkpoints(checksums_file);
    unsigned fail_count = 0;
    for (const auto& [block_log, hashes_to_validate] : hashes_in_checksums_file)
      if (!validate_block_log_checksum(block_log, hashes_to_validate))
        ++fail_count;
    if (fail_count)
      std::cout << "WARNING: " << fail_count << " block logs had checksums that did NOT match\n";
    return fail_count == 0;
  }
  FC_CAPTURE_AND_RETHROW()
}

bool compare_block_logs(const fc::path& first_filename, const fc::path& second_filename, 
                        std::optional<uint32_t> start_at_block)
{
  try
  {
    // open both block log files
    hive::chain::block_log first_block_log;
    first_block_log.open(first_filename, true);
    const uint32_t first_head_block_num = first_block_log.head() ? first_block_log.head()->get_block_num() : 0;

    hive::chain::block_log second_block_log;
    second_block_log.open(second_filename, true);
    const uint32_t second_head_block_num = second_block_log.head() ? second_block_log.head()->get_block_num() : 0;

    const uint32_t number_of_blocks_in_common = std::min(first_head_block_num, second_head_block_num);

    if (start_at_block && number_of_blocks_in_common < *start_at_block)
    {
      std::cout << "Error: can't start at block " << *start_at_block << ", ";
      if (first_head_block_num < *start_at_block)
        std::cout << first_filename.string() << " only has " << first_head_block_num << " blocks\n";
      if (second_head_block_num < *start_at_block)
        std::cout << second_filename.string() << " only has " << second_head_block_num << " blocks\n";
      return false;
    }

    // set up to loop through all the blocks.  we'll be doing two for_each_block calls in parallel, 
    // and we don't know whether we'll get the blocks from the first or second block log first.
    // when we get the block, store it in the variable below.  once we have both, compare
    std::shared_ptr<hive::chain::full_block_type> first_full_block;
    std::shared_ptr<hive::chain::full_block_type> second_full_block;

    // if our compare fails, store the fact in these variables
    std::optional<uint32_t> mismatch_on_block_number;
    std::atomic<bool> mismatch_detected = { false };

    const auto compare_blocks = [&]() {
      const hive::chain::uncompressed_block_data& first_uncompressed = first_full_block->get_uncompressed_block();
      const hive::chain::uncompressed_block_data& second_uncompressed = second_full_block->get_uncompressed_block();
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

    const auto generate_block_processor = [&](std::shared_ptr<hive::chain::full_block_type>& destination) {
      return [&](const std::shared_ptr<hive::chain::full_block_type>& full_block) {
        destination = full_block;
        sync_point.wait();
        return !mismatch_detected.load(std::memory_order_relaxed);
      };
    };

    // walk through the blocks, comparing as we go
    std::thread first_enumerator_thread([&]() {
      fc::set_thread_name("compare_1"); // tells the OS the thread's name
      fc::thread::current().set_name("compare_1"); // tells fc the thread's name for logging
      first_block_log.for_each_block(start_at_block.value_or(1), number_of_blocks_in_common, generate_block_processor(first_full_block), hive::chain::block_log::for_each_purpose::decompressing);
    });

    std::thread second_enumerator_thread([&]() {
      fc::set_thread_name("compare_2"); // tells the OS the thread's name
      fc::thread::current().set_name("compare_2"); // tells fc the thread's name for logging
      second_block_log.for_each_block(start_at_block.value_or(1), number_of_blocks_in_common, generate_block_processor(second_full_block), hive::chain::block_log::for_each_purpose::decompressing);
    });

    first_enumerator_thread.join();
    second_enumerator_thread.join();

    // check the results
    if (mismatch_detected.load(std::memory_order_relaxed))
    {
      std::cout << first_filename.string() << " and " << second_filename.string() << " differ at block " << *mismatch_on_block_number << "\n";
      return false;
    }
    if (first_head_block_num > second_head_block_num)
    {
      std::cout << first_filename.string() << " has " << first_head_block_num - second_head_block_num << " more blocks than " << second_filename.string() << ".  Both logs are equivalent through block " << second_head_block_num << ", which is the last block in " << second_filename.string() << "\n";
      return false;
    }
    else if (second_head_block_num > first_head_block_num)
    {
      std::cout << second_filename.string() << " has " << second_head_block_num - first_head_block_num << " more blocks than " << first_filename.string() << ".  Both logs are equivalent through the end of " << first_filename.string() << ", block " << first_head_block_num << "\n";
      return false;
    }
    else
      return true;
  }
  FC_CAPTURE_AND_RETHROW()
}

bool truncate_block_log(const fc::path& block_log_filename, uint32_t new_head_block_num, bool force)
{
  try
  {
    if (!force)
    {
      std::cout << "Really truncate " << block_log_filename.string() << " to " << new_head_block_num << " blocks? [yes/no] ";
      std::string response;
      std::cin >> response;
      std::transform(response.begin(), response.end(), response.begin(), tolower);
      if (response != "yes")
      {
        std::cout << "aborting\n";
        return false;
      }
    }
    hive::chain::block_log log;
    log.open(block_log_filename, false);
    log.truncate(new_head_block_num);
    return true;
  }
  FC_CAPTURE_AND_RETHROW()
}

// helper function.  Reads the 8 bytes at the given location, and determines whether they "look like" the 
// flags/offset byte that's written at the end of each block.  Used for when you have a corrupt block log
// and need to find the last complete block.
// if it looked reasonable, returns the start of the block it would point at
std::optional<uint64_t> is_data_at_file_position_a_plausible_offset_and_flags(int block_log_fd, uint64_t offset_of_pos_and_flags)
{
  uint64_t block_offset_with_flags;
  hive::utilities::perform_read(block_log_fd, (char*)&block_offset_with_flags, sizeof(block_offset_with_flags), 
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

  return offset_is_plausible && flags_are_plausible && dictionary_is_plausible ? offset : std::optional<uint64_t>();
}

bool find_end(const fc::path& block_log_filename)
{
  try
  {
    bool possible_end_found = false;

    int block_log_fd = open(block_log_filename.string().c_str(), O_RDONLY | O_CLOEXEC, 0644);
    if (block_log_fd == -1)
      FC_THROW("Error opening block log file ${block_log_filename}: ${error}", (block_log_filename)("error", strerror(errno)));

    struct stat file_stats;
    if (fstat(block_log_fd, &file_stats) == -1)
      FC_THROW("Error getting size of file: ${error}", ("error", strerror(errno)));
    const uint64_t block_log_size = file_stats.st_size;
    FC_ASSERT(block_log_size >= (ssize_t)sizeof(uint64_t));

    uint64_t offset_of_pos_and_flags = block_log_size - sizeof(uint64_t);
    for (int i = 0; i < HIVE_MAX_BLOCK_SIZE; ++i)
    {
      std::optional<uint64_t> possible_start_of_block = is_data_at_file_position_a_plausible_offset_and_flags(block_log_fd, offset_of_pos_and_flags);
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
        std::cout << "The block log already appears to end in a full block, no truncation is necessary\n";
      else
      {
        std::cout << "Likely end detected, block log size should be reduced to " << new_file_size << ",\n";
        std::cout << "which will reduce the block log size by " << block_log_size - new_file_size << " bytes\n\n";
        std::cout << "Do you want to truncate " << block_log_filename.string() << " now? [yes/no] ";
        std::string response;
        std::cin >> response;
        std::transform(response.begin(), response.end(), response.begin(), tolower);
        if (response == "yes")
        {
          close(block_log_fd);
          block_log_fd = open(block_log_filename.string().c_str(), O_RDWR | O_APPEND | O_CREAT | O_CLOEXEC, 0644);
          if (block_log_fd == -1)
            FC_THROW("Unable to reopen the block log file ${block_log_filename} in write mode for truncating: ${error}", (block_log_filename)("error", strerror(errno)));
          FC_ASSERT(ftruncate(block_log_fd, new_file_size) == 0, 
                    "failed to truncate block log, ${error}", ("error", strerror(errno)));
          std::cout << "done\n";
        }
        else
        {
          std::cout << "Leaving the block log unmolested.\n";
          std::cout << "You may truncate the file manually later using the command: truncate -s " << new_file_size << " " << block_log_filename.string() << "\n";
        }
      }
    }
    else
      std::cout << "Unable to detect the end of the block log\n";

    close(block_log_fd);
    return possible_end_found;
  }
  FC_CAPTURE_AND_RETHROW()
}

void get_head_block_number(const fc::path& block_log_filename)
{
  try
  {
    hive::chain::block_log log;
    log.open(block_log_filename, true);
    const uint32_t head_block_num = log.head() ? log.head()->get_block_num() : 0;
    std::cout << head_block_num << "\n";
  }
  FC_CAPTURE_AND_RETHROW()
}

bool get_block_ids(const fc::path& block_log_filename, uint32_t starting_block_number, std::optional<uint32_t> ending_block_number, bool print_block_numbers)
{
  try
  {
    hive::chain::block_log log;
    log.open(block_log_filename, true);
    const uint32_t head_block_num = log.head() ? log.head()->get_block_num() : 0;
    if (starting_block_number > head_block_num ||
        (ending_block_number && *ending_block_number > head_block_num))
    {
      std::cerr << "Block log only has " << head_block_num << " blocks\n";
      return false;
    }

    for (unsigned i = starting_block_number; i <= ending_block_number.value_or(starting_block_number); ++i)
    {
      if (print_block_numbers)
        std::cout << std::setw(9) << i << " ";
      std::cout << log.read_block_id_by_num(i).str() << "\n";
    }
    return true;
  }
  FC_CAPTURE_AND_RETHROW()
}

bool get_block(const fc::path& block_log_filename, uint32_t block_number, bool header_only, bool pretty_print, bool binary)
{
  try
  {
    hive::chain::block_log log;
    log.open(block_log_filename, true);
    const uint32_t head_block_num = log.head() ? log.head()->get_block_num() : 0;
    if (block_number < head_block_num)
    {
      std::cerr << "Block log only has " << head_block_num << " blocks\n";
      return false;
    }
    std::shared_ptr<hive::chain::full_block_type> full_block = log.read_block_by_num(block_number);
    if (binary)
    {
      if (header_only)
      {
        std::cerr << "Error: unable to write only the header in binary mode\n";
        return false;
      }
      const hive::chain::uncompressed_block_data& uncompressed = full_block->get_uncompressed_block();
      std::cout.write(uncompressed.raw_bytes.get(), uncompressed.raw_size);
    }
    else
    {
      if (header_only)
      {
        if (pretty_print)
          std::cout << fc::json::to_pretty_string(full_block->get_block_header()) << "\n";
        else
          std::cout << fc::json::to_string(full_block->get_block_header()) << "\n";
      }
      else
      {
        if (pretty_print)
          std::cout << fc::json::to_pretty_string(full_block->get_block()) << "\n";
        else
          std::cout << fc::json::to_string(full_block->get_block()) << "\n";
      }
    }
    return true;
  }
  FC_CAPTURE_AND_RETHROW()
}

int main(int argc, char** argv)
{
  boost::program_options::options_description global_options("Global options");
  global_options.add_options()("jobs,j", boost::program_options::value<int>()->default_value(4), "The number of worker threads to spawn");
  global_options.add_options()("verbose,v", "Enable info messages to stderr");
  global_options.add_options()("debug", "Enable debug messages to stderr");
  global_options.add_options()("help,h", "Print usage instructions");
  global_options.add_options()("command", boost::program_options::value<std::string>()->required(), "command to execute");
  global_options.add_options()("subargs", boost::program_options::value<std::vector<std::string>>(), "Arguments for command");

  boost::program_options::positional_options_description global_positional_options;
  global_positional_options.add("command", 1);
  global_positional_options.add("subargs", -1);

  // args for sha256sum subcommand
  boost::program_options::options_description sha256sum_options("sha256sum options");
  sha256sum_options.add_options()("input-block-log", boost::program_options::value<std::vector<std::string>>(), "The input block logs to checksum");
  sha256sum_options.add_options()("check,c", boost::program_options::value<std::string>()->value_name("filename"), "read SHA256 sums from a file and check them");
  sha256sum_options.add_options()("checkpoint", boost::program_options::value<uint32_t>()->value_name("n"), "Print the SHA256 every n blocks");

  boost::program_options::positional_options_description sha256sum_positional_options;
  sha256sum_positional_options.add("input-block-log", 1);

  // args for cmp subcommand
  boost::program_options::options_description cmp_options("cmp options");
  cmp_options.add_options()("first-block-log", boost::program_options::value<std::string>()->value_name("filename1")->required(), "The first block log");
  cmp_options.add_options()("second-block-log", boost::program_options::value<std::string>()->value_name("filename2")->required(), "The second block log");
  cmp_options.add_options()("start-at-block", boost::program_options::value<uint32_t>()->value_name("n"), "Start comparison at block n, assume all blocks n-1 and below are equivalent without checking");

  boost::program_options::positional_options_description cmp_positional_options;
  cmp_positional_options.add("first-block-log", 1);
  cmp_positional_options.add("second-block-log", 1);

  // args for truncate subcommand
  boost::program_options::options_description truncate_options("truncate options");
  truncate_options.add_options()("block-log", boost::program_options::value<std::string>()->value_name("filename")->required(), "The block log to truncate");
  truncate_options.add_options()("block-number", boost::program_options::value<uint32_t>()->value_name("n")->required(), "Truncate to the given block number");
  truncate_options.add_options()("force,f", "Don't prompt for confirmation");

  boost::program_options::positional_options_description truncate_positional_options;
  truncate_positional_options.add("block-log", 1);
  truncate_positional_options.add("block-number", 1);

  // args for find-end subcommand
  boost::program_options::options_description find_end_options("find-end options");
  find_end_options.add_options()("block-log", boost::program_options::value<std::string>()->value_name("filename")->required(), "The block log to find the end of");

  boost::program_options::positional_options_description find_end_positional_options;
  find_end_positional_options.add("block-log", 1);

  // args for get-head-block-number subcommand
  boost::program_options::options_description get_head_block_number_options("get-head-block-number options");
  get_head_block_number_options.add_options()("block-log", boost::program_options::value<std::string>()->value_name("filename")->required(), "The block log to get the head of");
  boost::program_options::positional_options_description get_head_block_number_positional_options;
  get_head_block_number_positional_options.add("block-log", 1);

  // args for get-block-ids subcommand
  boost::program_options::options_description get_block_ids_options("get-block-ids options");
  get_block_ids_options.add_options()("block-log", boost::program_options::value<std::string>()->value_name("filename")->required(), "The block log to read the block ids from");
  get_block_ids_options.add_options()("starting-block-number", boost::program_options::value<uint32_t>()->value_name("n")->required(), "The block number to return the id of");
  get_block_ids_options.add_options()("ending-block-number", boost::program_options::value<uint32_t>()->value_name("m"), "If present, return all block ids in from starting-block-number to ending-block-number, inclusive");
  get_block_ids_options.add_options()("print-block-numbers", "print the block number before the corresponding block id");
  boost::program_options::positional_options_description get_block_ids_positional_options;
  get_block_ids_positional_options.add("block-log", 1);
  get_block_ids_positional_options.add("starting-block-number", 1);
  get_block_ids_positional_options.add("ending-block-number", 1);

  // args for get-block subcommand
  boost::program_options::options_description get_block_options("get-block options");
  get_block_options.add_options()("block-log", boost::program_options::value<std::string>()->value_name("filename")->required(), "The block log to read the block from");
  get_block_options.add_options()("block-number", boost::program_options::value<uint32_t>()->value_name("n")->required(), "The block number to return");
  get_block_options.add_options()("header", "only print the block header");
  get_block_options.add_options()("pretty", "pretty-print the JSON");
  get_block_options.add_options()("binary", "output the binary form of the block (don't output this to a terminal!)");
  boost::program_options::positional_options_description get_block_positional_options;
  get_block_positional_options.add("block-log", 1);
  get_block_positional_options.add("block-number", 1);


  const auto print_usage = [&]() {
    std::cout << global_options << "\n";
    std::cout << "Supported subcommands:\n";
    std::cout << "  cmp\n";
    std::cout << "  find-end\n";
    std::cout << "  get-block\n";
    std::cout << "  get-block-ids\n";
    std::cout << "  get-head-block-number\n";
    std::cout << "  sha256sum\n";
    std::cout << "  truncate\n\n";

    std::cout << cmp_options << "\n";
    std::cout << find_end_options << "\n";
    std::cout << get_block_options << "\n";
    std::cout << get_block_ids_options << "\n";
    std::cout << get_head_block_number_options << "\n";
    std::cout << sha256sum_options << "\n";
    std::cout << truncate_options << "\n";
  };

  try
  {
    boost::program_options::parsed_options parsed_global_options = boost::program_options::command_line_parser(argc, argv).
      options(global_options).
      positional(global_positional_options).
      allow_unregistered().
      run();
    boost::program_options::variables_map options_map;
    boost::program_options::store(parsed_global_options, options_map);

    if (options_map.count("help") || !options_map.count("command"))
    {
      print_usage();
      return options_map.count("help") ? 0 : 1;
    }

    hive::chain::blockchain_worker_thread_pool::set_thread_pool_size(options_map["jobs"].as<int>());
    BOOST_SCOPE_EXIT(void) { hive::chain::blockchain_worker_thread_pool::get_instance().shutdown(); } BOOST_SCOPE_EXIT_END

    if (options_map.count("verbose") || options_map.count("debug"))
    {
      fc::logging_config logging_config;
      logging_config.appenders.push_back(fc::appender_config("stderr", "console", fc::variant(fc::console_appender::config())));
      logging_config.loggers = { fc::logger_config("default") };
      logging_config.loggers.front().level = options_map.count("debug") ? fc::log_level::debug : fc::log_level::info;
      logging_config.loggers.front().appenders = {"stderr"};
      fc::configure_logging(logging_config);
    }
    else
      fc::configure_logging(fc::logging_config());

    const std::string command = options_map["command"].as<std::string>();
    if (command == "sha256sum")
    {
      std::vector<std::string> subcommand_options = boost::program_options::collect_unrecognized(parsed_global_options.options, boost::program_options::include_positional);
      subcommand_options.erase(subcommand_options.begin());
      
      boost::program_options::parsed_options parsed_sha256_options = boost::program_options::command_line_parser(subcommand_options).
        options(sha256sum_options).
        positional(sha256sum_positional_options).
        run();
      boost::program_options::store(parsed_sha256_options, options_map);

      if (options_map.count("input-block-log") && options_map.count("check"))
      {
        std::cerr << "You cannot --check a sha5sum.txt file and specify an input-block-log at the same time\n";
        return 1;
      }
      if (options_map.count("input-block-log"))
      {
        const std::vector<std::string> input_block_logs = options_map["input-block-log"].as<std::vector<std::string>>();
        std::optional<uint32_t> checkpoint_every_n_blocks;
        if (options_map.count("checkpoint"))
          checkpoint_every_n_blocks = options_map["checkpoint"].as<uint32_t>();
        for (const std::string& input_block_log : input_block_logs)
          checksum_block_log(input_block_log, checkpoint_every_n_blocks);
        return 0;
      }
      else if (options_map.count("check"))
      {
        if (options_map.count("checkpoint"))
        {
          std::cerr << "the --checkpoint option doesn't make sense with the --check option\n";
          return 1;
        }
        return validate_block_log_checksums_from_file(options_map["check"].as<std::string>()) ? 0 : 1;
      }
    }
    else if (command == "cmp")
    {
      std::vector<std::string> subcommand_options = boost::program_options::collect_unrecognized(parsed_global_options.options, boost::program_options::include_positional);
      subcommand_options.erase(subcommand_options.begin());
      
      boost::program_options::parsed_options parsed_cmp_options = boost::program_options::command_line_parser(subcommand_options).
        options(cmp_options).
        positional(cmp_positional_options).
        run();
      boost::program_options::store(parsed_cmp_options, options_map);

      std::optional<uint32_t> start_at_block;
      if (options_map.count("start-at-block"))
        start_at_block = options_map["start-at-block"].as<uint32_t>();
      return compare_block_logs(options_map["first-block-log"].as<std::string>(), options_map["second-block-log"].as<std::string>(), start_at_block) ? 0 : 1;
    }
    else if (command == "truncate")
    {
      std::vector<std::string> subcommand_options = boost::program_options::collect_unrecognized(parsed_global_options.options, boost::program_options::include_positional);
      subcommand_options.erase(subcommand_options.begin());
      
      boost::program_options::parsed_options parsed_truncate_options = boost::program_options::command_line_parser(subcommand_options).
        options(truncate_options).
        positional(truncate_positional_options).
        run();
      boost::program_options::store(parsed_truncate_options, options_map);

      return truncate_block_log(options_map["block-log"].as<std::string>(), options_map["block-number"].as<uint32_t>(), options_map.count("force")) ? 0 : 1;
    }
    else if (command == "find-end")
    {
      std::vector<std::string> subcommand_options = boost::program_options::collect_unrecognized(parsed_global_options.options, boost::program_options::include_positional);
      subcommand_options.erase(subcommand_options.begin());
      
      boost::program_options::parsed_options parsed_find_end_options = boost::program_options::command_line_parser(subcommand_options).
        options(find_end_options).
        positional(find_end_positional_options).
        run();
      boost::program_options::store(parsed_find_end_options, options_map);

      return find_end(options_map["block-log"].as<std::string>()) ? 0 : 1;
    }
    else if (command == "get-head-block-number")
    {
      std::vector<std::string> subcommand_options = boost::program_options::collect_unrecognized(parsed_global_options.options, boost::program_options::include_positional);
      subcommand_options.erase(subcommand_options.begin());
      
      boost::program_options::parsed_options parsed_get_head_block_number_options = boost::program_options::command_line_parser(subcommand_options).
        options(get_head_block_number_options).
        positional(get_head_block_number_positional_options).
        run();
      boost::program_options::store(parsed_get_head_block_number_options, options_map);

      get_head_block_number(options_map["block-log"].as<std::string>());
      return 0;
    }
    else if (command == "get-block-ids")
    {
      std::vector<std::string> subcommand_options = boost::program_options::collect_unrecognized(parsed_global_options.options, boost::program_options::include_positional);
      subcommand_options.erase(subcommand_options.begin());
      
      boost::program_options::parsed_options parsed_get_block_ids_options = boost::program_options::command_line_parser(subcommand_options).
        options(get_block_ids_options).
        positional(get_block_ids_positional_options).
        run();
      boost::program_options::store(parsed_get_block_ids_options, options_map);

      const uint32_t starting_block_number = options_map["starting-block-number"].as<uint32_t>();
      std::optional<uint32_t> ending_block_number;
      if (options_map.count("ending-block-number"))
        ending_block_number = options_map["ending-block-number"].as<uint32_t>();

      bool print_block_numbers = options_map.count("print-block-numbers") != 0;

      return get_block_ids(options_map["block-log"].as<std::string>(), starting_block_number, ending_block_number, print_block_numbers) ? 0 : 1;
    }
    else if (command == "get-block")
    {
      std::vector<std::string> subcommand_options = boost::program_options::collect_unrecognized(parsed_global_options.options, boost::program_options::include_positional);
      subcommand_options.erase(subcommand_options.begin());
      
      boost::program_options::parsed_options parsed_get_block_options = boost::program_options::command_line_parser(subcommand_options).
        options(get_block_options).
        positional(get_block_positional_options).
        run();
      boost::program_options::store(parsed_get_block_options, options_map);

      const uint32_t block_number = options_map["block-number"].as<uint32_t>();
      const bool header_only = options_map.count("header") != 0;
      const bool pretty = options_map.count("pretty") != 0;
      const bool binary = options_map.count("binary") != 0;

      return get_block(options_map["block-log"].as<std::string>(), block_number, header_only, pretty, binary) ? 0 : 1;
    }
    else
    {
      std::cerr << "Unknown command: \"" << command << "\"\n";
      return 1;
    }
  }
  catch (const fc::exception& e)
  {
    std::cerr << "Error: " << e.to_detail_string() << "\n";
    return 1;
  }
  catch (const boost::program_options::error& e)
  {
    std::cerr << e.what() << "\n";
    print_usage();
    return 1;
  }
  catch (const std::exception& e)
  {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
