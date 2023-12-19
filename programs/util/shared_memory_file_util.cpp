#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include <fc/exception/exception.hpp>
#include <fc/filesystem.hpp>

#include <chainbase/chainbase.hpp>

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
#include <hive/plugins/follow/follow_objects.hpp>
#include <hive/plugins/market_history/market_history_plugin.hpp>
#include <hive/plugins/reputation/reputation_objects.hpp>
#include <hive/plugins/transaction_status/transaction_status_objects.hpp>
#include <hive/plugins/witness/witness_plugin_objects.hpp>

#include <chrono>
#include <fstream>
#include <iostream>

namespace shared_memory_file_util
{
  /* Tools for dumping a snapshot from shared memory file */

  class dumping_worker;

  class index_dump_writer : public chainbase::snapshot_writer
  {
  public:
    index_dump_writer(const index_dump_writer &) = delete;
    index_dump_writer &operator=(const index_dump_writer &) = delete;
    virtual ~index_dump_writer() = default;

    index_dump_writer(const chainbase::database &_db, const chainbase::abstract_index &_index, const fc::path &_output_dir, bool _allow_concurrency)
        : database(_db), index(_index), output_dir(_output_dir), allow_concurrency(_allow_concurrency) { conversion_type = chainbase::snapshot_writer::conversion_t::convert_to_json; }

    virtual workers prepare(const std::string &indexDescription, size_t firstId, size_t lastId, size_t indexSize, size_t indexNextId, snapshot_converter_t converter) override;
    virtual void start(const workers &workers) override;

    snapshot_converter_t get_snapshot_converter() const { return snapshot_converter; }

  private:
    std::vector<std::unique_ptr<dumping_worker>> snapshot_writer_workers;
    const chainbase::database &database;
    const chainbase::abstract_index &index;
    const fc::path output_dir;
    snapshot_converter_t snapshot_converter;
    std::string index_description;
    size_t first_id;
    size_t last_id;
    size_t next_id;
    bool allow_concurrency;
  };

  class dumping_worker : public chainbase::snapshot_writer::worker
  {
  public:
    dumping_worker(const fc::path &_output_file, index_dump_writer &_writer, const size_t _start_id, const size_t _end_id);
    void perform_dump();
    bool is_write_finished() const { return write_finished; }

  private:
    virtual void flush_converted_data(const serialized_object_cache &cache) override;
    virtual std::string prettifyObject(const fc::variant &object, const std::vector<char> &buffer) const override { return ""; }

    index_dump_writer &controller;
    fc::path output_file_path;
    std::fstream output_file;
    bool write_finished = false;
  };

  dumping_worker::dumping_worker(const fc::path &_output_file, index_dump_writer &_writer, const size_t _start_id, const size_t _end_id)
      : chainbase::snapshot_writer::worker(_writer, _start_id, _end_id), controller(_writer), output_file_path(_output_file)
  {
    output_file.open(output_file_path.generic_string(), std::ios::out | std::ios::trunc);
    FC_ASSERT(output_file.is_open(), "An error occured during creating files for dta from shared memory file.");
  }

  void dumping_worker::flush_converted_data(const serialized_object_cache &cache)
  {
    FC_ASSERT(output_file.is_open());

    for (const auto &kv : cache)
    {
      const auto &v = kv.second;
      std::string s(v.data(), v.size());
      output_file << s << "\n";
    }
  }

  void dumping_worker::perform_dump()
  {
    dlog("Performing a dump into file ${p}", ("p", output_file_path));
    auto converter = controller.get_snapshot_converter();
    converter(this);

    if (output_file.is_open())
      output_file.close();

    write_finished = true;
    dlog("Finished dump into file ${p}", ("p", output_file_path));
    output_file.close();
  }

  chainbase::snapshot_writer::workers index_dump_writer::prepare(const std::string &indexDescription, size_t firstId, size_t lastId, size_t indexSize, size_t indexNextId, snapshot_converter_t converter)
  {
    dlog("Preparing snapshot writer to store index holding `${d}' items. Index size: ${s}. Index next_id: ${indexNextId}. Index id range: <${f}, ${l}>.",
         ("d", indexDescription)("s", indexSize)(indexNextId)("f", firstId)("l", lastId));

    snapshot_converter = converter;
    index_description = indexDescription;
    first_id = firstId;
    last_id = lastId;
    next_id = indexNextId;

    if (indexSize == 0)
    {
      dlog("${index_description} has size 0, no workers created.", (index_description));
      return chainbase::snapshot_writer::workers();
    }

    constexpr size_t ITEMS_PER_WORKER = 2000000;
    chainbase::snapshot_writer::workers workers_for_index;
    size_t worker_count = indexSize / ITEMS_PER_WORKER + 1;
    size_t left = first_id;
    size_t right = 0;

    if (indexSize <= ITEMS_PER_WORKER)
      right = last_id;
    else
      right = std::min(first_id + ITEMS_PER_WORKER, last_id);

    std::string file_name = index_description;
    boost::replace_all(file_name, " ", "_");
    boost::replace_all(file_name, ":", "_");
    boost::replace_all(file_name, "<", "_");
    boost::replace_all(file_name, ">", "_");

    fc::path file_path(output_dir);
    file_path /= file_name;
    dlog("Preparing ${n} workers to store index holding `${d}' items)", ("d", index_description)("n", worker_count));
    fc::create_directories(file_path);

    for (size_t i = 0; i < worker_count; ++i)
    {
      std::string file_name = std::to_string(left) + '_' + std::to_string(right) + ".log";
      fc::path actual_output_file(file_path);
      actual_output_file /= file_name;
      dlog("Creating file ${actual_output_file}", (actual_output_file));
      snapshot_writer_workers.emplace_back(std::make_unique<dumping_worker>(actual_output_file, *this, left, right));
      workers_for_index.emplace_back(snapshot_writer_workers.back().get());
      left = right + 1;
      if (i == worker_count - 2)
        right = last_id + 1;
      else
        right += ITEMS_PER_WORKER;
    }

    return workers_for_index;
  }

  void index_dump_writer::start(const chainbase::snapshot_writer::workers &workers)
  {
    FC_ASSERT(snapshot_writer_workers.size() == workers.size(),
              "snapshot_writer_workers: ${snapshot_writer_workers} - workers: ${workers}",
              ("snapshot_writer_workers", snapshot_writer_workers.size())("workers", workers.size()));

    const size_t num_threads = allow_concurrency ? std::min(workers.size(), static_cast<size_t>(16)) : 1;
    if (num_threads > 1)
    {
      boost::asio::io_service io_service;
      boost::thread_group threadpool;
      std::unique_ptr<boost::asio::io_service::work> work = std::make_unique<boost::asio::io_service::work>(io_service);

      for (unsigned int i = 0; i < num_threads; ++i)
        threadpool.create_thread(boost::bind(&boost::asio::io_service::run, &io_service));

      for (size_t i = 0; i < snapshot_writer_workers.size(); ++i)
      {
        dumping_worker *w = snapshot_writer_workers[i].get();
        FC_ASSERT(w == workers[i]);

        io_service.post(boost::bind(&dumping_worker::perform_dump, w));
      }

      ilog("Waiting for dumping-workers jobs completion");

      /// Run the horses...
      work.reset();

      threadpool.join_all();
    }
    else
    {
      for (size_t i = 0; i < snapshot_writer_workers.size(); ++i)
      {
        dumping_worker *w = snapshot_writer_workers[i].get();
        FC_ASSERT(w == workers[i]);

        w->perform_dump();
      }
    }
  }

  /* Main App */

  class App
  {
  public:
    App(const boost::filesystem::path &_path_to_shared_memory_file_dir, const boost::filesystem::path &_output_dir, const bool _get_all_data,
        const bool _get_decoded_state_objects_data, const bool _get_details, const bool _list_indices, const bool _dump_indices, const uint8_t _dump_threads);
    ~App();
    void work();

  private:
    chainbase::database db;
    const fc::path output_dir;
    const bool extract_all_data;
    const bool extract_decoded_state_objects_data;
    const bool extract_shm_details;
    const bool list_indices;
    const bool dump_indices;
    const uint8_t dump_threads;

    void dump_decoded_state_objects_data() const;
    void dump_shared_memory_file_details();
    void initialize_indices();
    void perform_dump_indices();

    // verify if index is in DB and log that info.
    // If index is not "added" to DB, we will not get data about it, so we need to add all indices first to check which indices are in shared memory file.
    template <typename T>
    std::optional<std::string> add_index_to_db()
    {
      try
      {
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

  App::App(const boost::filesystem::path &_path_to_shared_memory_file_dir, const boost::filesystem::path &_output_dir, const bool _get_all_data,
           const bool _get_decoded_state_objects_data, const bool _get_details, const bool _list_indices, const bool _dump_indices, const uint8_t _dump_threads)
      : output_dir(_output_dir), extract_all_data(_get_all_data), extract_decoded_state_objects_data(_get_decoded_state_objects_data),
        extract_shm_details(_get_details), list_indices(_list_indices), dump_indices(_dump_indices), dump_threads(_dump_threads)
  {
    if (!fc::exists(output_dir))
    {
      ilog("Creating directories for requested data: ${output_dir}", (output_dir));
      fc::create_directories(output_dir);
    }
    else if (!fc::is_directory(output_dir))
      FC_THROW_EXCEPTION(fc::invalid_arg_exception, "${output_dir} should points for directory.", (output_dir));

    ilog("Opening shared memory file from directory: ${path_to_dir}", ("path_to_dir", _path_to_shared_memory_file_dir.generic_string()));

    db.open(_path_to_shared_memory_file_dir);
  }

  App::~App()
  {
    ilog("Shared memory file util finished work. Closing shared memory file");
    db.close();
  }

  void App::initialize_indices()
  {
    const std::string log_path = output_dir.generic_string() + "/detected_indices.log";
    std::fstream detected_indices_stream;
    detected_indices_stream.open(log_path, std::ios::out | std::ios::trunc);
    if (!detected_indices_stream.good())
      FC_THROW("Couldn't create log file for detected indices: ${log_path}", (log_path));

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
    save_index_name(add_index_to_db<hive::plugins::follow::follow_index>());
    save_index_name(add_index_to_db<hive::plugins::follow::blog_author_stats_index>());
    save_index_name(add_index_to_db<hive::plugins::follow::feed_index>());
    save_index_name(add_index_to_db<hive::plugins::follow::blog_index>());
    save_index_name(add_index_to_db<hive::plugins::follow::reputation_index>());
    save_index_name(add_index_to_db<hive::plugins::follow::follow_count_index>());
    save_index_name(add_index_to_db<hive::plugins::market_history::bucket_index>());
    save_index_name(add_index_to_db<hive::plugins::market_history::order_history_index>());
    save_index_name(add_index_to_db<hive::plugins::reputation::reputation_index>());
    save_index_name(add_index_to_db<hive::plugins::transaction_status::transaction_status_index>());
    save_index_name(add_index_to_db<hive::plugins::witness::witness_custom_op_index>());

    detected_indices_stream.flush();
    detected_indices_stream.close();
    ilog("${index_counter} detected indices types has been saved into ${log_path}", (index_counter)(log_path));
  }

  void App::dump_decoded_state_objects_data() const
  {
    const std::string log_path = output_dir.generic_string() + "/decoded_state_objects_data.log";
    std::fstream decoded_state_objects_data_stream;
    decoded_state_objects_data_stream.open(log_path, std::ios::out | std::ios::trunc);
    if (decoded_state_objects_data_stream.good())
    {
      decoded_state_objects_data_stream << hive::chain::util::decoded_types_data_storage(db.get_decoded_state_objects_data_from_shm()).generate_decoded_types_data_pretty_string();
      ilog("Extracted decoded state objects data from shared memory file into ${log_path}", (log_path));
    }
    else
      FC_THROW("Couldn't create log file for decoded_state_objects_data: ${log_path}", (log_path));

    decoded_state_objects_data_stream.flush();
    decoded_state_objects_data_stream.close();
  }

  void App::dump_shared_memory_file_details()
  {
    const std::string log_path = output_dir.generic_string() + "/shared_memory_file_details.log";
    std::fstream shm_details_stream;
    shm_details_stream.open(log_path, std::ios::out | std::ios::trunc);
    if (!shm_details_stream.good())
      FC_THROW("Couldn't create log file for shared memory details: ${log_path}", (log_path));

    shm_details_stream << db.get_environment_details() << "\n\n";

    auto segment_manager = db.get_segment_manager();
    const auto irreversible_object = segment_manager->find<hive::chain::database::irreversible_object_type>("irreversible");

    shm_details_stream << fc::json::to_pretty_string(*irreversible_object.first) << "\n";
    shm_details_stream.flush();
    shm_details_stream.close();
    ilog("Extracted shared memory details into ${log_path}", (log_path));
  }

  void App::perform_dump_indices()
  {
    boost::asio::io_service io_service;
    boost::thread_group threadpool;
    std::unique_ptr<boost::asio::io_service::work> work = std::make_unique<boost::asio::io_service::work>(io_service);

    const bool parallel_dumping = dump_threads > 1;

    for (unsigned int i = 0; parallel_dumping && i < dump_threads; ++i)
      threadpool.create_thread(boost::bind(&boost::asio::io_service::run, &io_service));

    std::vector<std::unique_ptr<index_dump_writer>> writers;
    const auto indices = db.get_abstract_index_cntr();
    FC_ASSERT(!indices.empty());
    ilog("Dumping ${indices_count} indices from database to log files. Threads count: ${dump_threads}", ("indices_count", indices.size())(dump_threads));

    auto run_dump_snapshot_process = [](const chainbase::abstract_index *idx, index_dump_writer *writer) -> void
    {
      idx->dump_snapshot(*writer);
    };

    for (const chainbase::abstract_index *idx : indices)
    {
      writers.emplace_back(std::make_unique<index_dump_writer>(db, *idx, output_dir, parallel_dumping));
      index_dump_writer *writer = writers.back().get();

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

    ilog("Dumping indicies finished.");
  }

  void App::work()
  {
    const auto started_at = std::chrono::steady_clock::now();

    if (extract_all_data || list_indices || dump_indices)
      initialize_indices();

    if (extract_all_data || extract_decoded_state_objects_data)
      dump_decoded_state_objects_data();

    if (extract_all_data || extract_shm_details)
      dump_shared_memory_file_details();

    if (extract_all_data || dump_indices)
      perform_dump_indices();

    const auto ended_at = std::chrono::steady_clock::now();
    ilog("Work finished in ${seconds} seconds.", ("seconds", std::chrono::duration_cast<std::chrono::seconds>(ended_at - started_at).count()));
  }

} // shared_memory_file_util

int main(int argc, char **argv)
{
  boost::program_options::options_description shared_memory_file_util_options("Shared memory file util options");
  shared_memory_file_util_options.add_options()("input,i", boost::program_options::value<boost::filesystem::path>()->value_name("directory")->required(), "Path to directory with contains shared memory file: 'shared_memory.bin'");
  shared_memory_file_util_options.add_options()("output,o", boost::program_options::value<boost::filesystem::path>()->value_name("directory")->required(), "Path to directory where all requested data from shared memory file will be saved.");
  shared_memory_file_util_options.add_options()("help,h", "Print usage instructions");
  shared_memory_file_util_options.add_options()("get-all-data", "Extracts all possible data from shared memory file (ignores rest options except dump-threads)");
  shared_memory_file_util_options.add_options()("get-state-objects-data", "Extracts decoded state objects data from shared memory file");
  shared_memory_file_util_options.add_options()("get-details", "Extracts data about versions, irreversible block and so on.");
  shared_memory_file_util_options.add_options()("list-indices", "List all indices detected in shared memory file");
  shared_memory_file_util_options.add_options()("dump-indices", "Dump data from all indices into files.");
  shared_memory_file_util_options.add_options()("dump-threads", boost::program_options::value<unsigned>()->value_name("Number")->default_value(1), "Number of threads for dumping process. (Max 16)");

  try
  {
    boost::program_options::parsed_options parsed_shared_memory_file_util_options = boost::program_options::command_line_parser(argc, argv).options(shared_memory_file_util_options).allow_unregistered().run();

    boost::program_options::variables_map options_map;
    boost::program_options::store(parsed_shared_memory_file_util_options, options_map);
    std::vector<std::string> unrecognized_options = boost::program_options::collect_unrecognized(parsed_shared_memory_file_util_options.options, boost::program_options::include_positional);

    auto print_help = [&shared_memory_file_util_options]()
    {
      std::cout << "\n"
                << shared_memory_file_util_options << "\n";
    };

    if (!unrecognized_options.empty())
    {
      std::cout << "Unrecognized_options: \n";
      for (const auto &opt : unrecognized_options)
        std::cout << "  " << opt << "\n";

      print_help();
      return 0;
    }

    if (options_map.count("help") || options_map.empty())
    {
      print_help();
      return 0;
    }

    if (!options_map.count("input"))
    {
      std::cout << "Missing path to directory with shared memory file\n";
      return 0;
    }

    if (!options_map.count("output"))
    {
      std::cout << "Missing path to directory where all requested data from shared memory will be saved\n";
      return 0;
    }
    const unsigned dump_threads = options_map["dump-threads"].as<unsigned>();
    if (dump_threads > 16)
      FC_THROW("dump_threads: ${dump_threads} exceeds limit - only 16 is allowed.", (dump_threads));

    shared_memory_file_util::App app(options_map["input"].as<boost::filesystem::path>(),         // _path_to_shared_memory_file_dir
                                     options_map["output"].as<boost::filesystem::path>(),        // _output_dir
                                     options_map.count("get-all-data") ? true : false,           // _get_all_data
                                     options_map.count("get-state-objects-data") ? true : false, // _get_decoded_state_objects_data
                                     options_map.count("get-details") ? true : false,            // _get_details
                                     options_map.count("list-indices") ? true : false,           // _list_indices
                                     options_map.count("dump-indices") ? true : false,           // _dump_indices
                                     static_cast<uint8_t>(dump_threads)                          // _dump_threads
    );

    app.work();
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