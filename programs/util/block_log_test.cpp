#include <appbase/application.hpp>

#include <fc/log/logger.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/filesystem.hpp>
#include <hive/chain/block_log.hpp>
#include <hive/chain/full_block.hpp>
#include <hive/chain/block_log_artifacts.hpp>
#include <hive/chain/blockchain_worker_thread_pool.hpp>

#include <boost/program_options.hpp>

#include <iostream>
#include <iomanip>
#include <fstream>

using namespace hive::chain;

void generate_artifacts(const fc::path& block_log_path, appbase::application& app, hive::chain::blockchain_worker_thread_pool& thread_pool)
{
  block_log bl( app );

  bl.open(block_log_path, thread_pool, true, false);
  auto bla = block_log_artifacts::open(block_log_path, bl, false, false, false, app, thread_pool);
  bla.reset();
  ilog("open and generation finished...");
}

void generate_from_scratch(const fc::path& block_log_path, appbase::application& app, hive::chain::blockchain_worker_thread_pool& thread_pool)
{
  fc::path artifact_file_path = block_log_path.generic_string() + block_log_file_name_info::_artifacts_extension;

  fc::remove_all(artifact_file_path);

  generate_artifacts(block_log_path, app, thread_pool);
}

int main(int argc, char** argv)
{
  try
  {
    appbase::application theApp;
    hive::chain::blockchain_worker_thread_pool thread_pool = hive::chain::blockchain_worker_thread_pool( theApp );

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
    generate_from_scratch(input_block_log_path, theApp, thread_pool);
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
