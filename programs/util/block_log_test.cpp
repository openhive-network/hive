#include <fc/log/logger.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/filesystem.hpp>
#include <hive/chain/block_log.hpp>
#include <hive/chain/block_log_artifacts.hpp>

#include <boost/program_options.hpp>

#include <iostream>
#include <iomanip>
#include <fstream>

using namespace hive::chain;

void generate_artifacts(const fc::path& block_log_path, uint32_t blocks_to_process)
{
  block_log bl;

  bl.open(block_log_path, true, false);

  if(blocks_to_process == 0)
  {
    hive::protocol::signed_block b = bl.read_head();

    blocks_to_process = b.block_num();
  }

  auto bla = block_log_artifacts::open(block_log_path, false, bl, blocks_to_process);

  bla.reset();

  ilog("open and generation finished...");
}

void generate_from_scratch(const fc::path& block_log_path, uint32_t blocks_to_process)
{
  fc::path artifact_file_path = block_log_path.generic_string() + ".artifacts";

  fc::remove_all(artifact_file_path);

  generate_artifacts(block_log_path, blocks_to_process);
}

int main(int argc, char** argv)
{
  try
  {
    boost::program_options::options_description options("Allowed options");
    options.add_options()("input-block-log,i", boost::program_options::value<std::string>()->required(), "The path pointing the input block log file");
    options.add_options()("block-count,n", boost::program_options::value<uint32_t>(), "Stop after this many blocks");

    options.add_options()("help,h", "Print usage instructions");

    boost::program_options::positional_options_description positional_options;
    positional_options.add("input-block-log", 1);

    boost::program_options::variables_map options_map;
    boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(options).positional(positional_options).run(), options_map);


    if (!options_map.count("input-block-log"))
    {
      std::cerr << "Error: missing parameter for input-block-log \n";
      return 1;
    }

    fc::path input_block_log_path;
    if (options_map.count("input-block-log"))
      input_block_log_path = options_map["input-block-log"].as<std::string>();

    uint32_t blocks_to_process = 0;

    if(options_map.count("block-count"))
      blocks_to_process = options_map["block-count"].as<uint32_t>();

    uint32_t block_count = 100000;
    generate_from_scratch(input_block_log_path, block_count);
    
    ilog("Trying to resume generation from ${f} to ${t} blocks...", ("f", block_count)("t", 2*block_count));
    generate_artifacts(input_block_log_path, 2*block_count);

    ilog("Trying to perform full block log generation ...");
    generate_from_scratch(input_block_log_path, 0);
  }
  catch(const fc::exception& e)
  {
    edump((e.to_detail_string()));
  }

  catch ( const std::exception& e )
  {
    edump( ( std::string( e.what() ) ) );
  }

  return 0;
}
