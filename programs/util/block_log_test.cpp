#include <fc/log/logger.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/filesystem.hpp>
#include <hive/chain/block_log.hpp>
#include <hive/chain/full_block.hpp>
#include <hive/chain/block_log_artifacts.hpp>

#include <boost/program_options.hpp>

#include <iostream>
#include <iomanip>
#include <fstream>

using namespace hive::chain;

void generate_artifacts(const fc::path& block_log_path)
{
  block_log bl;

  bl.open(block_log_path, true, false);
  auto bla = block_log_artifacts::open(block_log_path, bl, false, false);
  bla.reset();
  ilog("open and generation finished...");
}

void generate_from_scratch(const fc::path& block_log_path)
{
  fc::path artifact_file_path = block_log_path.generic_string() + ".artifacts";

  fc::remove_all(artifact_file_path);

  generate_artifacts(block_log_path);
}

int main(int argc, char** argv)
{
  try
  {
    boost::program_options::options_description options("Allowed options");
    options.add_options()("input-block-log,i", boost::program_options::value<std::string>()->required(), "The path pointing the input block log file");
    options.add_options()("help,h", "Print usage instructions");

    boost::program_options::positional_options_description positional_options;
    positional_options.add("input-block-log", 1);

    boost::program_options::variables_map options_map;
    boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(options).positional(positional_options).run(), options_map);


    if (!options_map.count("input-block-log"))
    {
      elog("Error: missing parameter for input-block-log ");
      return 1;
    }

    fc::path input_block_log_path = options_map["input-block-log"].as<std::string>();

    ilog("Trying to perform full block log generation ...");
    generate_from_scratch(input_block_log_path);
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
