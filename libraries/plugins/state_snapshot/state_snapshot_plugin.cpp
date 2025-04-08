#include <hive/chain/hive_fwd.hpp>

#include <hive/plugins/state_snapshot/state_snapshot_plugin.hpp>
#include <hive/plugins/state_snapshot/state_snapshot_tools.hpp>

#include <hive/utilities/benchmark_dumper.hpp>

#include <hive/chain/util/decoded_types_data_storage.hpp>
#include <hive/chain/util/state_checker_tools.hpp>

#include <hive/plugins/chain/chain_plugin.hpp>

#include <hive/protocol/get_config.hpp>

#include <hive/utilities/benchmark_dumper.hpp>

#include <appbase/application.hpp>

#include <fc/io/json.hpp>

#include <boost/bind/bind.hpp>
#include <boost/thread/thread.hpp>

#include <string>
#include <thread>
namespace bpo = boost::program_options;


namespace hive { namespace plugins { namespace state_snapshot {

using hive::utilities::benchmark_dumper;


class state_snapshot_plugin::impl final : protected chain::state_snapshot_provider
  {
  using database = hive::chain::database;

  public:
    impl(state_snapshot_plugin& self, const bpo::variables_map& options, appbase::application& app) :
      _self(self),
      _mainDb(app.get_plugin<hive::plugins::chain::chain_plugin>().db())
      {
      collectOptions(options);

      if (!_num_threads)
      {
        _num_threads = std::thread::hardware_concurrency();
        if (_num_threads == 0)
        {
          _num_threads = 8;
          wlog("Could not detect available threads number. Setting threads number for snapshot processing to ${_num_threads}", (_num_threads));
        }
      }

      app.get_plugin<hive::plugins::chain::chain_plugin>().register_snapshot_provider(*this);

      ilog("Registering add_prepare_snapshot_handler...");

      _mainDb.add_prepare_snapshot_handler([&](const database& db, const database::abstract_index_cntr_t& indexContainer) -> void
        {
        std::string name = generate_name();
        prepare_snapshot(name);
        }, _self, 0);
      }

    void prepare_snapshot(const std::string& snapshotName);
    void load_snapshot(const std::string& snapshotName, const hive::chain::open_args& openArgs);

  protected:
    /// chain::state_snapshot_provider implementation:
    virtual void process_explicit_snapshot_requests(const hive::chain::open_args& openArgs) override;

    private:
      void collectOptions(const bpo::variables_map& options);
      std::string generate_name() const;
      void load_snapshot_external_data(const plugin_external_data_index& idx);

      void load_snapshot_impl(const std::string& snapshotName, const hive::chain::open_args& openArgs);

    private:
      state_snapshot_plugin&  _self;
      database&               _mainDb;
      bfs::path               _storagePath;
      std::unique_ptr<DB>     _storage;
      std::string             _snapshot_name;
      uint32_t                _num_threads = 0;
      bool                    _do_immediate_load = false;
      bool                    _do_immediate_dump = false;
      std::exception_ptr      _exception;
      std::atomic_bool        _is_error{false};
  };

void state_snapshot_plugin::impl::collectOptions(const bpo::variables_map& options)
  {
  bfs::path snapshotPath;

  if(options.count("snapshot-root-dir"))
    snapshotPath = options.at("snapshot-root-dir").as<bfs::path>();

  if(snapshotPath.is_absolute() == false)
    {
    auto basePath = _self.get_app().data_dir();
    auto actualPath = basePath / snapshotPath;
    snapshotPath = actualPath;
    }

  _storagePath = snapshotPath;

  _do_immediate_load = options.count("load-snapshot");
  if(_do_immediate_load)
    _snapshot_name = options.at("load-snapshot").as<std::string>();

  _do_immediate_dump = options.count("dump-snapshot");
  if(_do_immediate_dump)
    _snapshot_name = options.at("dump-snapshot").as<std::string>();

  if (options.count("process-snapshot-threads-num"))
  {
    _num_threads = options.at("process-snapshot-threads-num").as<unsigned>();
    FC_ASSERT(_num_threads, "You have to assing at least one thread for snapshot processing");
  }
  FC_ASSERT(!_do_immediate_load || !_do_immediate_dump, "You can only dump or load snapshot at once.");

  fc::mutable_variant_object state_opts;

  _self.get_app().get_plugin< hive::plugins::chain::chain_plugin >().report_state_options(_self.name(), state_opts);
  }

std::string state_snapshot_plugin::impl::generate_name() const
  {
  return "snapshot_" + std::to_string(fc::time_point::now().sec_since_epoch());
  }

void state_snapshot_plugin::impl::load_snapshot_external_data(const plugin_external_data_index& idx)
  {
  snapshot_load_supplement_helper load_helper(idx);

  hive::chain::load_snapshot_supplement_notification notification(load_helper);

  _mainDb.notify_load_snapshot_data_supplement(notification);
  }

void state_snapshot_plugin::impl::prepare_snapshot(const std::string& snapshotName)
  {
  try
  {
  benchmark_dumper dumper;
  dumper.initialize([](benchmark_dumper::database_object_sizeof_cntr_t&) {}, "state_snapshot_dump.json");

  bfs::path actualStoragePath = _storagePath / snapshotName;
  actualStoragePath = actualStoragePath.normalize();

  ilog("Request to generate snapshot in the location: `${p}'", ("p", actualStoragePath.string()));

  if(bfs::exists(actualStoragePath) == false)
    bfs::create_directories(actualStoragePath);
  else
  {
    FC_ASSERT(bfs::is_empty(actualStoragePath), "Directory ${p} is not empty. Creating snapshot rejected.", ("p", actualStoragePath.string()));
  }
  
  const auto& indices = _mainDb.get_abstract_index_cntr();
  ilog("Attempting to dump contents of ${n} indices using ${_num_threads} thread(s).", ("n", indices.size())(_num_threads));
  std::vector<std::unique_ptr<index_dump_writer>> builtWriters;

  if (_num_threads > 1)
  {
    boost::asio::io_service ioService;
    boost::thread_group threadpool;
    std::unique_ptr<boost::asio::io_service::work> work = std::make_unique<boost::asio::io_service::work>(ioService);

    for(unsigned int i = 0; i < _num_threads; ++i)
      threadpool.create_thread(boost::bind(&boost::asio::io_service::run, &ioService));

    for(const chainbase::abstract_index* idx : indices)
    {
      builtWriters.emplace_back(std::make_unique<index_dump_writer>(_mainDb, *idx, actualStoragePath, true /* allow_concurrency */, _exception, _is_error));
      index_dump_writer* writer = builtWriters.back().get();
      ioService.post(boost::bind(&index_dump_writer::safe_spawn_snapshot_dump, writer, idx));
    }
    ilog("Waiting for dumping jobs completion");
    work.reset();
    threadpool.join_all();
    if( _exception )
    {
      wlog("Snapshot writing is aborted because of some errors");
      std::rethrow_exception( _exception );
    }
  }
  else
  {
    for(const chainbase::abstract_index* idx : indices)
    {
      builtWriters.emplace_back(std::make_unique<index_dump_writer>(_mainDb, *idx, actualStoragePath, false /* allow_concurrency */, _exception, _is_error));
      index_dump_writer* writer = builtWriters.back().get();
      writer->safe_spawn_snapshot_dump(idx);
    }
  }

  fc::path external_data_storage_base_path(actualStoragePath);
  external_data_storage_base_path /= "ext_data";

  if(bfs::exists(external_data_storage_base_path) == false)
    bfs::create_directories(external_data_storage_base_path);

  snapshot_dump_supplement_helper dump_helper;
  
  hive::chain::prepare_snapshot_supplement_notification notification(external_data_storage_base_path, dump_helper);

  _mainDb.notify_prepare_snapshot_data_supplement(notification);

  store_snapshot_manifest(actualStoragePath, builtWriters, dump_helper, _mainDb);

  auto blockNo = _mainDb.head_block_num();

  const auto& measure = dumper.measure(blockNo, [](benchmark_dumper::index_memory_details_cntr_t&) {});
  ilog("State snapshot generation. Elapsed time: ${rt} ms (real), ${ct} ms (cpu). Memory usage: ${cm} (current), ${pm} (peak) kilobytes.",
    ("rt", measure.real_ms)
    ("ct", measure.cpu_ms)
    ("cm", measure.current_mem)
    ("pm", measure.peak_mem));

  ilog("Snapshot generation finished");
  return;
  }
  FC_CAPTURE_AND_LOG(());

  elog("Snapshot generation FAILED.");
  }

void state_snapshot_plugin::impl::load_snapshot_impl(const std::string& snapshotName, const hive::chain::open_args& openArgs)
  {
  bfs::path actualStoragePath = _storagePath / snapshotName;
  actualStoragePath = actualStoragePath.normalize();

  if(bfs::exists(actualStoragePath) == false)
    {
    elog("Snapshot `${n}' does not exist in the snapshot directory: `${d}' or is inaccessible.", ("n", snapshotName)("d", _storagePath.string()));
    return;
    }

  ilog("Trying to access snapshot in the location: `${p}'", ("p", actualStoragePath.string()));

  benchmark_dumper dumper;
  dumper.initialize([](benchmark_dumper::database_object_sizeof_cntr_t&) {}, "state_snapshot_load.json");

  std::shared_ptr<hive::chain::full_block_type> lib;
  auto snapshotManifest = load_snapshot_manifest(actualStoragePath, lib, _mainDb.get_segment_manager());

  {
    const std::string& plugins_in_snapshot = std::get<4>(snapshotManifest);
    fc::variants plugins_in_snapshot_vector = fc::json::from_string(plugins_in_snapshot, fc::json::format_validation_mode::full).get_array();
    std::set<std::string> current_plugins = _self.get_app().get_plugins_names();
    bool snapshot_has_more_plugins = false;

    for (const auto& plugin_from_snapshot_as_variant : plugins_in_snapshot_vector)
    {
      const std::string plugin_from_snapshot = plugin_from_snapshot_as_variant.as_string();
      if (current_plugins.count(plugin_from_snapshot))
        current_plugins.erase(plugin_from_snapshot);
      else
        snapshot_has_more_plugins = true;
    }

    if (snapshot_has_more_plugins)
      wlog("Snaphot has more plugins than current hived configuration. Snapshot plugins: ${plugins_in_snapshot}, hived plugins: ${hived_plugins}", (plugins_in_snapshot)("hived_plugins", _self.get_app().get_plugins_names()));
    else if (!current_plugins.empty())
      wlog("Snapshot misses plugins: ${current_plugins}. Snapshot plugins: ${plugins_in_snapshot}, hived plugins: ${hived_plugins}", (current_plugins)(plugins_in_snapshot)("hived_plugins", _self.get_app().get_plugins_names()));
  }

  const std::string& loaded_decoded_type_data = std::get<2>(snapshotManifest);
  const std::string& full_loaded_blockchain_configuration_json = std::get<3>(snapshotManifest);

  wlog("Snapshot state definitions matches current app version - wiping DB.");
  _mainDb.close();
  _mainDb.wipe(openArgs.shared_mem_dir);
  _mainDb.pre_open(openArgs);
  dlog("Updating DB decoded state objects data and blockchain config with data from snapshot.");
  _mainDb.set_decoded_state_objects_data(loaded_decoded_type_data);
  _mainDb.set_blockchain_config(full_loaded_blockchain_configuration_json);
  _mainDb.open(openArgs);

  const auto& indices = _mainDb.get_abstract_index_cntr();
  ilog("Attempting to load contents of ${n} indices using ${_num_threads} thread(s).", ("n", indices.size())(_num_threads));

  if (_num_threads > 1)
  {
    boost::asio::io_service ioService;
    boost::thread_group threadpool;
    std::unique_ptr<boost::asio::io_service::work> work = std::make_unique<boost::asio::io_service::work>(ioService);

    for(unsigned int i = 0; i < _num_threads; ++i)
      threadpool.create_thread(boost::bind(&boost::asio::io_service::run, &ioService));

    std::vector<std::unique_ptr< index_dump_reader>> builtReaders;

    for(chainbase::abstract_index* idx : indices)
    {
      builtReaders.emplace_back(std::make_unique<index_dump_reader>(std::get<0>(snapshotManifest), actualStoragePath, _exception, _is_error));
      index_dump_reader* reader = builtReaders.back().get();
      ioService.post(boost::bind(&index_dump_reader::safe_spawn_snapshot_load, reader, idx));
    }

    ilog("Waiting for loading jobs completion");
    work.reset();
    threadpool.join_all();
    if( _exception )
    {
      wlog("Snapshot loading is aborted because of some errors");
      std::rethrow_exception( _exception );
    }
  }
  else
  {
    for(chainbase::abstract_index* idx : indices)
    {
      std::unique_ptr< index_dump_reader> reader = std::make_unique<index_dump_reader>(std::get<0>(snapshotManifest), actualStoragePath, _exception, _is_error);
      reader->safe_spawn_snapshot_load(idx);
    }
  }

  plugin_external_data_index& extDataIdx = std::get<1>(snapshotManifest);
  if(extDataIdx.empty())
  {
    ilog("Skipping external data load due to lack of data saved to the snapshot");
  }
  else
  {
    load_snapshot_external_data(extDataIdx);
  }

  auto last_irr_block = std::get<5>(snapshotManifest);
  
  _mainDb.set_last_irreversible_block_num(last_irr_block);
  if(lib)
    _mainDb.set_last_irreversible_block_data( lib );

  auto blockNo = _mainDb.head_block_num();

  ilog("Setting chainbase revision to ${b} block... Loaded irreversible block is: ${lib}.", ("b", blockNo)("lib", last_irr_block));

  _mainDb.set_revision(blockNo);
  _mainDb.load_state_initial_data(openArgs);

  const auto& measure = dumper.measure(blockNo, [](benchmark_dumper::index_memory_details_cntr_t&) {});
  ilog("State snapshot load. Elapsed time: ${rt} ms (real), ${ct} ms (cpu). Memory usage: ${cm} (current), ${pm} (peak) kilobytes.",
    ("rt", measure.real_ms)
    ("ct", measure.cpu_ms)
    ("cm", measure.current_mem)
    ("pm", measure.peak_mem));

  ilog("Snapshot loading finished, starting validate_invariants to check consistency...");
  _mainDb.validate_invariants();
  ilog("Validate_invariants finished...");

  _mainDb.set_snapshot_loaded();
  }

void state_snapshot_plugin::impl::load_snapshot(const std::string& snapshotName, const hive::chain::open_args& openArgs)
  {
    try
    {
      load_snapshot_impl( snapshotName, openArgs );
    }
    FC_CAPTURE_LOG_AND_RETHROW( (snapshotName) );
  }

void state_snapshot_plugin::impl::process_explicit_snapshot_requests(const hive::chain::open_args& openArgs)
  {
    if(_do_immediate_load)
    {
      _self.get_app().notify_status("loading snapshot");
      load_snapshot(_snapshot_name, openArgs);
      _self.get_app().notify_status("finished loading snapshot");
    }

    if(_do_immediate_dump)
    {
      _self.get_app().notify_status("dumping snapshot");
      prepare_snapshot(_snapshot_name);
      _self.get_app().notify_status("finished dumping snapshot");
    }
  }

state_snapshot_plugin::state_snapshot_plugin()
  {
  }

state_snapshot_plugin::~state_snapshot_plugin()
  {
  }

void state_snapshot_plugin::set_program_options(
  boost::program_options::options_description& command_line_options,
  boost::program_options::options_description& cfg)
  {
  cfg.add_options()
    ("snapshot-root-dir", bpo::value<bfs::path>()->default_value("snapshot"),
      "The location (root-dir) of the snapshot storage, to save/read portable state dumps")
    ;
  command_line_options.add_options()
    ("load-snapshot", bpo::value<std::string>(),
      "Allows to force immediate snapshot import at plugin startup. All data in state storage are overwritten")
    ("dump-snapshot", bpo::value<std::string>(),
      "Allows to force immediate snapshot dump at plugin startup. All data in the snaphsot storage are overwritten")
    ("process-snapshot-threads-num", bpo::value<unsigned>(),
      "Number of threads intended for snapshot processing. By default set to detected available threads count.")
    ;
  }

void state_snapshot_plugin::plugin_initialize(const boost::program_options::variables_map& options)
  {
  ilog("Initializing state_snapshot_plugin...");
  _my = std::make_unique<impl>(*this, options, get_app());
  }

void state_snapshot_plugin::plugin_startup()
  {
  ilog("Starting up state_snapshot_plugin...");
  }

void state_snapshot_plugin::plugin_shutdown()
  {
  ilog("Shutting down state_snapshot_plugin...");
  /// TO DO ADD CHECK DEFERRING SHUTDOWN WHEN SNAPSHOT IS GENERATED/LOADED.
  }

}}} ///hive::plugins::state_snapshot

