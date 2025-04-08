#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include <fc/log/logger_config.hpp>
#include <fc/log/console_appender.hpp>
#include <fc/exception/exception.hpp>
#include <fc/filesystem.hpp>

#include <hive/chain/util/decoded_types_data_storage.hpp>

#include <hive/chain/account_object.hpp>
#include <hive/chain/block_summary_object.hpp>
#include <hive/chain/comment_object.hpp>
#include <hive/chain/dhf_objects.hpp>
#include <hive/chain/global_property_object.hpp>
#include <hive/chain/hardfork_property_object.hpp>
#include <hive/chain/hive_objects.hpp>
#include <hive/chain/transaction_object.hpp>
#include <hive/chain/witness_objects.hpp>
#include <hive/chain/rc/rc_objects.hpp>
#include <hive/chain/smt_objects/account_balance_object.hpp>
#include <hive/chain/smt_objects/nai_pool_object.hpp>
#include <hive/chain/smt_objects/smt_token_object.hpp>

#include <hive/plugins/account_by_key/account_by_key_objects.hpp>
#include <hive/plugins/account_history_rocksdb/account_history_rocksdb_objects.hpp>
#include <hive/plugins/block_log_info/block_log_info_objects.hpp>
#include <hive/plugins/market_history/market_history_plugin.hpp>
#include <hive/plugins/reputation/reputation_objects.hpp>
#include <hive/plugins/transaction_status/transaction_status_objects.hpp>
#include <hive/plugins/witness/witness_plugin_objects.hpp>
#include <hive/plugins/state_snapshot/state_snapshot_tools.hpp>

#include <chrono>
#include <iostream>

using hive::plugins::state_snapshot::index_dump_reader;
using hive::plugins::state_snapshot::index_dump_to_file_writer;

const std::string TEMPORARY_SHARED_MEMORY_FILE_SIZE = "24G";

class BaseApp
{
public:
  virtual void work() = 0;

protected:
  chainbase::database db;
  const fc::path output_dir;
  const bool dump_indices;
  const uint8_t worker_threads;

  BaseApp(const boost::filesystem::path &_input_dir, const boost::filesystem::path &_output_dir, const bool _dump_indices, const uint8_t _worker_threads, const bool _create_tmp_db);
  void initialize_indices(const bool log_founded_indicies);
  void log_result(const std::string &content, const std::string &content_type, const std::string &filename) const;
  void perform_dump_indices();

  // verify if index is in DB and log that info.
  // If index is not "added" to DB, we will not get data about it, so we need to add all indices first to check which indices are in shared memory file.
  template <typename T>
  std::optional<std::string> add_index_to_db()
  {
    try
    { //_add_index_impl
      db.add_index<T>();
      const std::string index_name = boost::core::demangle(typeid(T).name());
      // ilog("Index: ${index} is in the shared memory file", ("index", index_name));
      return index_name;
    }
    catch (std::logic_error &e)
    {
      // dlog("Adding ${index} to DB failed", ("index", boost::core::demangle(typeid(T).name())));
    }

    return std::optional<std::string>();
  }
};

BaseApp::BaseApp(const boost::filesystem::path &_input_dir, const boost::filesystem::path &_output_dir, const bool _dump_indices, const uint8_t _worker_threads, const bool _create_tmp_db) : output_dir(_output_dir), dump_indices(_dump_indices), worker_threads(_worker_threads)
{
  if (dump_indices)
    FC_ASSERT(output_dir != fc::path(), "If extracting all data, output directory must be specified");

  if (output_dir != fc::path())
  {
    if (!fc::exists(output_dir))
    {
      dlog("Creating directories for requested data: ${output_dir}", (output_dir));
      fc::create_directories(output_dir);
    }
    else if (!fc::is_directory(output_dir))
      FC_THROW_EXCEPTION(fc::invalid_arg_exception, "${output_dir} should points for directory.", (output_dir));
  }
  if (_create_tmp_db)
  {
    const size_t file_size = fc::parse_size(TEMPORARY_SHARED_MEMORY_FILE_SIZE);
    dlog("Creating shared memory file in directory: ${path_to_dir} File size: ${file_size}", ("path_to_dir", _input_dir.generic_string())(file_size));
    db.open(_input_dir, 0, file_size);
  }
  else
  {
    dlog("Opening shared memory file from directory: ${path_to_dir}", ("path_to_dir", _input_dir.generic_string()));
    db.open(_input_dir);
  }
}

void BaseApp::log_result(const std::string &content, const std::string &content_type, const std::string &filename) const
{
  if (output_dir != fc::path())
  {
    const std::string log_path = output_dir.generic_string() + "/" + filename;
    std::fstream file_stream;
    file_stream.open(log_path, std::ios::out | std::ios::trunc);
    if (file_stream.good())
    {
      file_stream << content;
      dlog("Extracted ${content_type} from shared memory file into ${log_path}", (content_type)(log_path));
    }
    else
      FC_THROW("Couldn't create log file for ${content_type}: ${log_path}", (content_type)(log_path));

    file_stream.flush();
    file_stream.close();
  }
  else
    std::cout << content << "\n";
}

void BaseApp::initialize_indices(const bool log_founded_indicies)
{
  std::stringstream detected_indices_stream;
  size_t index_counter = 0;

  auto save_index_name = [&detected_indices_stream, &index_counter](std::optional<std::string> index_name)
  {
    if (index_name)
    {
      detected_indices_stream << *index_name << "\n\n";
      ++index_counter;
    }
  };

  // List of all indices which can be found in shared memory file.
  save_index_name(add_index_to_db<hive::chain::account_index>());
  save_index_name(add_index_to_db<hive::chain::account_metadata_index>());
  save_index_name(add_index_to_db<hive::chain::owner_authority_history_index>());
  save_index_name(add_index_to_db<hive::chain::account_authority_index>());
  save_index_name(add_index_to_db<hive::chain::vesting_delegation_index>());
  save_index_name(add_index_to_db<hive::chain::vesting_delegation_expiration_index>());
  save_index_name(add_index_to_db<hive::chain::account_recovery_request_index>());
  save_index_name(add_index_to_db<hive::chain::change_recovery_account_request_index>());
  save_index_name(add_index_to_db<hive::chain::block_summary_index>());
  save_index_name(add_index_to_db<hive::chain::comment_vote_index>());
  save_index_name(add_index_to_db<hive::chain::comment_index>());
  save_index_name(add_index_to_db<hive::chain::comment_cashout_index>());
  save_index_name(add_index_to_db<hive::chain::comment_cashout_ex_index>());
  save_index_name(add_index_to_db<hive::chain::proposal_index>());
  save_index_name(add_index_to_db<hive::chain::proposal_vote_index>());
  save_index_name(add_index_to_db<hive::chain::dynamic_global_property_index>());
  save_index_name(add_index_to_db<hive::chain::hardfork_property_index>());
  save_index_name(add_index_to_db<hive::chain::limit_order_index>());
  save_index_name(add_index_to_db<hive::chain::convert_request_index>());
  save_index_name(add_index_to_db<hive::chain::collateralized_convert_request_index>());
  save_index_name(add_index_to_db<hive::chain::liquidity_reward_balance_index>());
  save_index_name(add_index_to_db<hive::chain::feed_history_index>());
  save_index_name(add_index_to_db<hive::chain::withdraw_vesting_route_index>());
  save_index_name(add_index_to_db<hive::chain::escrow_index>());
  save_index_name(add_index_to_db<hive::chain::savings_withdraw_index>());
  save_index_name(add_index_to_db<hive::chain::decline_voting_rights_request_index>());
  save_index_name(add_index_to_db<hive::chain::reward_fund_index>());
  save_index_name(add_index_to_db<hive::chain::recurrent_transfer_index>());
  save_index_name(add_index_to_db<hive::chain::transaction_index>());
  save_index_name(add_index_to_db<hive::chain::witness_index>());
  save_index_name(add_index_to_db<hive::chain::witness_vote_index>());
  save_index_name(add_index_to_db<hive::chain::witness_schedule_index>());
  save_index_name(add_index_to_db<hive::chain::rc_resource_param_index>());
  save_index_name(add_index_to_db<hive::chain::rc_pool_index>());
  save_index_name(add_index_to_db<hive::chain::rc_stats_index>());
  save_index_name(add_index_to_db<hive::chain::rc_direct_delegation_index>());
  save_index_name(add_index_to_db<hive::chain::rc_expired_delegation_index>());
  save_index_name(add_index_to_db<hive::chain::rc_usage_bucket_index>());
#ifdef HIVE_ENABLE_SMT
  save_index_name(add_index_to_db<hive::chain::account_regular_balance_index>());
  save_index_name(add_index_to_db<hive::chain::account_rewards_balance_index>());
  save_index_name(add_index_to_db<hive::chain::nai_pool_index>());
  save_index_name(add_index_to_db<hive::chain::smt_contribution_index>());
  save_index_name(add_index_to_db<hive::chain::smt_token_index>());
  save_index_name(add_index_to_db<hive::chain::smt_ico_index>());
  save_index_name(add_index_to_db<hive::chain::smt_token_emissions_index>());
#endif

  save_index_name(add_index_to_db<hive::plugins::account_by_key::key_lookup_index>());
  save_index_name(add_index_to_db<hive::plugins::account_history_rocksdb::volatile_operation_index>());
  save_index_name(add_index_to_db<hive::plugins::block_log_info::block_log_hash_state_index>());
  save_index_name(add_index_to_db<hive::plugins::block_log_info::block_log_pending_message_index>());
  save_index_name(add_index_to_db<hive::plugins::market_history::bucket_index>());
  save_index_name(add_index_to_db<hive::plugins::market_history::order_history_index>());
  save_index_name(add_index_to_db<hive::plugins::reputation::reputation_index>());
  save_index_name(add_index_to_db<hive::plugins::transaction_status::transaction_status_index>());
  save_index_name(add_index_to_db<hive::plugins::transaction_status::transaction_status_block_index>());
  save_index_name(add_index_to_db<hive::plugins::witness::witness_custom_op_index>());

  if (log_founded_indicies)
    log_result("Detected " + std::to_string(index_counter) + " indices:\n" + detected_indices_stream.str(), std::to_string(index_counter) + " detected indices", "detected_indices.log");
}

void BaseApp::perform_dump_indices()
{
  boost::asio::io_service io_service;
  boost::thread_group threadpool;
  std::unique_ptr<boost::asio::io_service::work> work = std::make_unique<boost::asio::io_service::work>(io_service);

  const bool parallel_dumping = worker_threads > 1;

  for (unsigned int i = 0; parallel_dumping && i < worker_threads; ++i)
    threadpool.create_thread(boost::bind(&boost::asio::io_service::run, &io_service));

  std::vector<std::unique_ptr<index_dump_to_file_writer>> writers;
  const auto indices = db.get_abstract_index_cntr();
  writers.reserve(indices.size());
  FC_ASSERT(!indices.empty());
  dlog("Dumping ${indices_count} indices from database to log files. Threads count: ${worker_threads}", ("indices_count", indices.size())(worker_threads));

  auto run_dump_snapshot_process = [](const chainbase::abstract_index *idx, index_dump_to_file_writer *writer) -> void
  {
    idx->dump_snapshot(*writer);
  };

  for (const chainbase::abstract_index *idx : indices)
  {
    writers.emplace_back(std::make_unique<index_dump_to_file_writer>(db, *idx, output_dir, parallel_dumping));
    index_dump_to_file_writer *writer = writers.back().get();

    if (parallel_dumping)
      io_service.post(boost::bind<void>(run_dump_snapshot_process, idx, writer));
    else
      idx->dump_snapshot(*writer);
  }

  if (parallel_dumping)
  {
    work.reset();
    threadpool.join_all();
  }

  dlog("Dumping indicies finished.");
}

class ShmToolApp : public BaseApp
{
public:
  ShmToolApp(const boost::filesystem::path &_path_to_shared_memory_file_dir, const boost::filesystem::path &_output_dir, const bool _get_all_data,
             const bool _get_decoded_state_objects_data, const bool _get_details, const bool _get_blockchain_config, const bool _list_plugins,
             const bool _list_indices, const bool _dump_indices, const uint8_t _worker_threads);
  ~ShmToolApp();
  void work() override;

private:
  const bool extract_all_data;
  const bool extract_decoded_state_objects_data;
  const bool extract_shm_details;
  const bool extract_blockchain_config;
  const bool list_plugins;
  const bool list_indices;

  void read_decoded_state_objects_data() const;
  void read_shared_memory_file_details();
  void read_blockchain_config() const;
  void read_plugins() const;
};

ShmToolApp::ShmToolApp(const boost::filesystem::path &_path_to_shared_memory_file_dir, const boost::filesystem::path &_output_dir, const bool _get_all_data,
                       const bool _get_decoded_state_objects_data, const bool _get_details, const bool _get_blockchain_config, const bool _list_plugins,
                       const bool _list_indices, const bool _dump_indices, const uint8_t _worker_threads)
    : BaseApp(_path_to_shared_memory_file_dir, _output_dir, _dump_indices, _worker_threads, false),
      extract_all_data(_get_all_data), extract_decoded_state_objects_data(_get_decoded_state_objects_data),
      extract_shm_details(_get_details), extract_blockchain_config(_get_blockchain_config), list_plugins(_list_plugins),
      list_indices(_list_indices)
{
  if (extract_all_data)
    FC_ASSERT(output_dir != fc::path(), "If extracting all data, output directory must be specified");
}

ShmToolApp::~ShmToolApp()
{
  dlog("Shared memory file util finished work. Closing shared memory file");
  db.close();
}

void ShmToolApp::read_decoded_state_objects_data() const
{
  const std::string state_objects_definitions_data = hive::chain::util::decoded_types_data_storage(db.get_decoded_state_objects_data_from_shm()).generate_decoded_types_data_pretty_string();
  log_result(state_objects_definitions_data, "decoded state objects definitions", "decoded_state_objects_data.log");
}

void ShmToolApp::read_shared_memory_file_details()
{
  std::stringstream ss;
  ss << db.get_environment_details() << "\n";
  auto segment_manager = db.get_segment_manager();
  const auto irreversible_object = segment_manager->find<hive::chain::irreversible_block_data_type>("irreversible");
  ss << fc::json::to_pretty_string(*irreversible_object.first) << "\n";
  log_result(ss.str(), "shm details", "shared_memory_file_details.log");
}

void ShmToolApp::read_blockchain_config() const
{
  const std::string blockchain_config_pretty = fc::json::to_pretty_string(fc::json::from_string(db.get_blockchain_config_from_shm(), fc::json::format_validation_mode::full));
  log_result(blockchain_config_pretty, "blockchain config", "blockchain_config.log");
}

void ShmToolApp::read_plugins() const
{
  const std::string plugins_list = db.get_plugins_from_shm();
  log_result(plugins_list, "plugins", "plugins.log");
}

void ShmToolApp::work()
{
  const auto started_at = std::chrono::steady_clock::now();

  if (extract_all_data || list_indices || dump_indices)
    initialize_indices(true);

  if (extract_all_data || extract_decoded_state_objects_data)
    read_decoded_state_objects_data();

  if (extract_all_data || extract_shm_details)
    read_shared_memory_file_details();

  if (extract_all_data || extract_blockchain_config)
    read_blockchain_config();

  if (extract_all_data || list_plugins)
    read_plugins();

  if (extract_all_data || dump_indices)
    perform_dump_indices();

  const auto ended_at = std::chrono::steady_clock::now();
  dlog("Work finished in ${seconds} seconds.", ("seconds", std::chrono::duration_cast<std::chrono::seconds>(ended_at - started_at).count()));
}

class SnapshotToolApp : public BaseApp
{
public:
  SnapshotToolApp(const boost::filesystem::path &_snapshot_dir, const boost::filesystem::path &_path_to_temporary_db_dir, const boost::filesystem::path &_output_dir,
                  const bool _dump_manifest, const bool _dump_indices, const uint8_t _worker_threads);
  ~SnapshotToolApp();
  void work() override;

private:
  const boost::filesystem::path snapshot_dir;
  const bool dump_manifest;
  void perform_dump_manifest(const hive::plugins::state_snapshot::manifest_data &manifest);
  void load_snapshot_to_temporary_db(const hive::plugins::state_snapshot::manifest_data &manifest);
};

SnapshotToolApp::SnapshotToolApp(const boost::filesystem::path &_snapshot_dir, const boost::filesystem::path &_path_to_temporary_db_dir, const boost::filesystem::path &_output_dir, const bool _dump_manifest,
                                 const bool _dump_indices, const uint8_t _worker_threads)
    : BaseApp(_path_to_temporary_db_dir, _output_dir, _dump_indices, _worker_threads, true), snapshot_dir(_snapshot_dir), dump_manifest(_dump_manifest)
{
}

SnapshotToolApp::~SnapshotToolApp()
{
  dlog("Shared memory file util finished work. Closing shared memory file");
  db.close();
}

void SnapshotToolApp::perform_dump_manifest(const hive::plugins::state_snapshot::manifest_data &manifest)
{
  const auto manifest_v = hive::plugins::state_snapshot::snapshot_manifest_to_variant(manifest);
  log_result(fc::json::to_pretty_string(manifest_v), "snapshot manifest", "snapshot_manifest.log");
}

void SnapshotToolApp::load_snapshot_to_temporary_db(const hive::plugins::state_snapshot::manifest_data &manifest)
{
  boost::asio::io_service io_service;
  boost::thread_group threadpool;
  std::unique_ptr<boost::asio::io_service::work> work = std::make_unique<boost::asio::io_service::work>(io_service);

  const bool parallel_loading = worker_threads > 1;

  for (unsigned int i = 0; parallel_loading && i < worker_threads; ++i)
    threadpool.create_thread(boost::bind(&boost::asio::io_service::run, &io_service));

  std::vector<std::unique_ptr<index_dump_reader>> readers;
  auto indices = db.get_abstract_index_cntr();
  readers.reserve(indices.size());
  FC_ASSERT(!indices.empty());
  dlog("Loading ${indices_count} indices to database from snapshot. Threads count: ${worker_threads}", ("indices_count", indices.size())(worker_threads));

  auto run_load_snapshot_process = [](chainbase::abstract_index *idx, index_dump_reader *reader) -> void
  {
    try
    {
    idx->load_snapshot(*reader);
    }
    catch(...) {}
  };

  std::exception_ptr eptr;

  std::vector<std::unique_ptr<std::atomic_bool>> error_flags;
  error_flags.reserve(indices.size());

  for (chainbase::abstract_index *idx : indices)
  {
    error_flags.emplace_back(std::make_unique<std::atomic_bool>(false));
    readers.emplace_back(std::make_unique<index_dump_reader>(std::get<0>(manifest), snapshot_dir, eptr, *error_flags.back()));
    index_dump_reader *reader = readers.back().get();

    if (parallel_loading)
      io_service.post(boost::bind<void>(run_load_snapshot_process, idx, reader));
    else
      idx->load_snapshot(*reader);
  }

  if (parallel_loading)
  {
    work.reset();
    threadpool.join_all();
  }

  dlog("Loading indicies finished.");
}

void SnapshotToolApp::work()
{
  const auto started_at = std::chrono::steady_clock::now();
  initialize_indices(false);
  std::shared_ptr<hive::chain::full_block_type> lib;
  const hive::plugins::state_snapshot::manifest_data manifest = hive::plugins::state_snapshot::load_snapshot_manifest(snapshot_dir, lib, db.get_segment_manager());

  if (dump_manifest)
    perform_dump_manifest(manifest);

  if (dump_indices)
  {
    load_snapshot_to_temporary_db(manifest);
    perform_dump_indices();
  }

  const auto ended_at = std::chrono::steady_clock::now();
  dlog("Work finished in ${seconds} seconds.", ("seconds", std::chrono::duration_cast<std::chrono::seconds>(ended_at - started_at).count()));
}

int main(int argc, char **argv)
{
  boost::program_options::options_description tool_options("Snapshot and shared memory file util options");
  tool_options.add_options()("help,h", "Print usage instructions");
  tool_options.add_options()("debug", "Show debug logs");
  tool_options.add_options()("worker-threads", boost::program_options::value<unsigned>()->value_name("Number")->default_value(1), "Number of threads for dumping process. (Max 16, default 1)");
  tool_options.add_options()("snapshot", "Tool will perform operation on snapshot");
  tool_options.add_options()("shm", "Tool will perform operation on shared memory file");
  tool_options.add_options()("dump-indices", "Dump data from all indices into files.");
  tool_options.add_options()("input,i", boost::program_options::value<boost::filesystem::path>()->value_name("directory")->required(), "Path to directory which contains snapshot or shared memory file");
  tool_options.add_options()("output,o", boost::program_options::value<boost::filesystem::path>()->value_name("directory")->required(), "Path to directory where all requested data from shared memory file or snapshot will be saved.");

  boost::program_options::options_description shared_memory_file_options("shared memory file operations");
  shared_memory_file_options.add_options()("get-all-data", "Extracts all possible data from shared memory file (ignores rest options except dump-threads)");
  shared_memory_file_options.add_options()("get-state-objects-data", "Extracts decoded state objects data from shared memory file");
  shared_memory_file_options.add_options()("get-blockchain-config", "Extracts blockchain configuration from shared memory file");
  shared_memory_file_options.add_options()("get-details", "Extracts data about versions, irreversible block and so on.");
  shared_memory_file_options.add_options()("list-plugins", "Extracts plugins list from shared memory file");
  shared_memory_file_options.add_options()("list-indices", "List all indices detected in shared memory file");

  boost::program_options::options_description snapshot_options("snapshot operations");
  snapshot_options.add_options()("temporary-db", boost::program_options::value<boost::filesystem::path>()->value_name("directory")->required(), ("Path to directory where temporary database will be created because reading data from snapshot requires database. New one will be created, shared memory file size is set to:" + TEMPORARY_SHARED_MEMORY_FILE_SIZE).c_str());
  snapshot_options.add_options()("dump-manifest", "Dump snapshot manifest.");

  try
  {
    auto print_help = [&tool_options, &shared_memory_file_options, &snapshot_options]()
    {
      std::cout << "\n"
                << tool_options << "\n"
                << shared_memory_file_options << "\n"
                << snapshot_options << "\n";
    };

    boost::program_options::variables_map options_map;
    {
      auto update_options_map = [&options_map, argc, &const_argv = std::as_const(argv)](const boost::program_options::options_description &options)
      {
        boost::program_options::parsed_options parsed_options = boost::program_options::command_line_parser(argc, const_argv).options(options).allow_unregistered().run();
        boost::program_options::store(parsed_options, options_map);
      };

      update_options_map(tool_options);
      update_options_map(shared_memory_file_options);
      update_options_map(snapshot_options);
    }

    FC_ASSERT(!options_map.count("shm") || !options_map.count("snapshot"), "--shm and --snapshot cannot be used at the same time");

    // worker-threads has default value so it's already in map
    if (options_map.count("help") || options_map.size() == 1)
    {
      print_help();
      return options_map.count("help") ? 0 : 1;
    }

    fc::logging_config logging_config;
    logging_config.appenders.push_back(fc::appender_config("stderr", "console", fc::variant(fc::console_appender::config())));
    logging_config.loggers = {fc::logger_config("default")};
    logging_config.loggers.front().level = options_map.count("debug") ? fc::log_level::all : fc::log_level::warn;
    logging_config.loggers.front().appenders = {"stderr"};
    fc::configure_logging(logging_config);

    const unsigned worker_threads = options_map["worker-threads"].as<unsigned>();
    if (worker_threads > 16)
      FC_THROW("worker_threads: ${worker_threads} exceeds limit - only 16 is allowed.", (worker_threads));

    if (!options_map.count("input"))
    {
      std::cout << "input is mandatory\n";
      return 0;
    }

    if (options_map.count("shm"))
    {
      ShmToolApp app(options_map["input"].as<boost::filesystem::path>(),                                                            // _path_to_shared_memory_file_dir
                     options_map.count("output") ? options_map["output"].as<boost::filesystem::path>() : boost::filesystem::path(), // _output_dir
                     options_map.count("get-all-data") ? true : false,                                                              // _get_all_data
                     options_map.count("get-state-objects-data") ? true : false,                                                    // _get_decoded_state_objects_data
                     options_map.count("get-details") ? true : false,                                                               // _get_details
                     options_map.count("get-blockchain-config") ? true : false,                                                     // _get_blockchain_config
                     options_map.count("list-plugins") ? true : false,                                                              // _list_plugins
                     options_map.count("list-indices") ? true : false,                                                              // _list_indices
                     options_map.count("dump-indices") ? true : false,                                                              // _dump_indices
                     static_cast<uint8_t>(worker_threads)                                                                           // _worker_threads

      );

      app.work();
    }
    else if (options_map.count("snapshot"))
    {
      if (!options_map.count("temporary-db"))
      {
        std::cout << "when operation on snapshot, --temporary-db is mandatory\n";
        return 0;
      }
      SnapshotToolApp app(
          options_map["input"].as<boost::filesystem::path>(),
          options_map["temporary-db"].as<boost::filesystem::path>(),                                                     // _path_to_shared_memory_file_dir
          options_map.count("output") ? options_map["output"].as<boost::filesystem::path>() : boost::filesystem::path(), // _output_dir
          options_map.count("dump-manifest") ? true : false,                                                             // _dump_indices
          options_map.count("dump-indices") ? true : false,                                                              // _dump_indices
          static_cast<uint8_t>(worker_threads)                                                                           // _worker_threads

      );

      app.work();
    }
    else
      FC_THROW("Must use --snapshot or --shm");
  }
  catch (const fc::exception &e)
  {
    elog("FC error: ${error}", ("error", e.to_detail_string()));
    return 1;
  }
  catch (const boost::program_options::error &e)
  {
    elog("boost::program_options::error: ${error}", ("error", e.what()));
    return 1;
  }
  catch (const std::exception &e)
  {
    elog("std error: ${error}", ("error", e.what()));
    return 1;
  }
  catch (...)
  {
    elog("Unkown error occured");
    return 1;
  }

  return 0;
}