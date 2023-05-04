#include <hive/chain/hive_fwd.hpp>

#include <hive/plugins/state_snapshot/state_snapshot_plugin.hpp>

#include <hive/utilities/benchmark_dumper.hpp>

#include <chainbase/state_snapshot_support.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/util/decoded_types_data_storage.hpp>

#include <hive/plugins/chain/chain_plugin.hpp>
#include <hive/plugins/chain/state_snapshot_provider.hpp>

#include <hive/utilities/benchmark_dumper.hpp>
#include <hive/utilities/plugin_utilities.hpp>

#include <appbase/application.hpp>

#include <fc/io/json.hpp>
#include <fc/io/raw.hpp>
#include <fc/reflect/reflect.hpp>

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/slice.h>
#include <rocksdb/sst_file_reader.h>
#include <rocksdb/utilities/write_batch_with_index.h>

#include <boost/bind/bind.hpp>
#include <boost/type.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/thread/thread.hpp>

#include <limits>
#include <string>
#include <typeindex>
#include <typeinfo>

namespace bpo = boost::program_options;

#define SNAPSHOT_FORMAT_VERSION "2.1"

namespace {

namespace bfs = boost::filesystem;

struct index_manifest_file_info
  {
  /// Path relative against snapshot main-directory.
  std::string relative_path;
  uint64_t    file_size;
  };

struct index_manifest_info
  {
  std::string name;
  /// Number of source index items dumped into snapshot.
  size_t      dumpedItems = 0;
  size_t      firstId = 0;
  size_t      lastId = 0;
  /** \warning indexNextId must be then explicitly loaded to generic_index::next_id to conform proposal ID rules. next_id can be different (higher)
      than last_object_id + 1 due to removing proposals, which does not correct next_id
  */
  size_t      indexNextId = 0;
  std::vector<index_manifest_file_info> storage_files;
  };

struct index_manifest_info_less
  {
  bool operator()(const index_manifest_info& i1, const index_manifest_info& i2) const
    {
    return i1.name < i2.name;
    }
  };

typedef std::set <index_manifest_info, index_manifest_info_less> snapshot_manifest;

class rocksdb_cleanup_helper
  {
  public:
    static rocksdb_cleanup_helper open(const ::rocksdb::Options& opts, const bfs::path& path);
    void close();

    ::rocksdb::ColumnFamilyHandle* create_column_family(const std::string& name, const ::rocksdb::ColumnFamilyOptions* cfOptions = nullptr);

    rocksdb_cleanup_helper(rocksdb_cleanup_helper&&) = default;
    rocksdb_cleanup_helper& operator=(rocksdb_cleanup_helper&&) = default;

    rocksdb_cleanup_helper(const rocksdb_cleanup_helper&) = delete;
    rocksdb_cleanup_helper& operator=(const rocksdb_cleanup_helper&) = delete;

    ~rocksdb_cleanup_helper()
      {
      cleanup();
      }

    ::rocksdb::DB* operator ->() const
      {
      return _db.get();
      }

  private:
    bool cleanup()
      {
      if(_db)
        {
        for(auto* ch : _column_handles)
          {
          auto s = _db->DestroyColumnFamilyHandle(ch);
          if(s.ok() == false)
            {
            elog("Cannot destroy column family handle. Error: `${e}'", ("e", s.ToString()));
            return false;
            }
          }

        auto s = _db->Close();
        if(s.ok() == false)
          {
          elog("Cannot Close database. Error: `${e}'", ("e", s.ToString()));
          return false;
          }

        _db.reset();
        }

      return true;
      }

  private:
    rocksdb_cleanup_helper() = default;

    typedef std::vector<::rocksdb::ColumnFamilyHandle*> column_handles;
    std::unique_ptr<::rocksdb::DB> _db;
    column_handles                 _column_handles;
  };

rocksdb_cleanup_helper rocksdb_cleanup_helper::open(const ::rocksdb::Options& opts, const bfs::path& path)
  {
  rocksdb_cleanup_helper retVal;

  ::rocksdb::DB* db = nullptr;
  auto status = ::rocksdb::DB::Open(opts, path.string(), &db);
  retVal._db.reset(db);
  if(status.ok())
    {
    ilog("Successfully opened db at path: `${p}'", ("p", path.string()));
    }
  else
    {
    elog("Cannot open db at path: `${p}'. Error details: `${e}'.", ("p", path.string())("e", status.ToString()));
    throw std::exception();
    }

  return retVal;
  }

void rocksdb_cleanup_helper::close()
  {
  if(cleanup() == false)
    throw std::exception();
  }

::rocksdb::ColumnFamilyHandle* rocksdb_cleanup_helper::create_column_family(const std::string& name, const ::rocksdb::ColumnFamilyOptions* cfOptions/* = nullptr*/)
  {
  ::rocksdb::ColumnFamilyOptions actualOptions;

  if(cfOptions != nullptr)
    actualOptions = *cfOptions;

  ::rocksdb::ColumnFamilyHandle* cf = nullptr;
  auto status = _db->CreateColumnFamily(actualOptions, name, &cf);
  if(status.ok() == false)
    {
    elog("Cannot create column family. Error details: `${e}'.", ("e", status.ToString()));
    throw std::exception();
    }

  _column_handles.emplace_back(cf);

  return cf;
  }

struct plugin_external_data_info
{
  fc::path path;
};

typedef std::map<std::string, plugin_external_data_info> plugin_external_data_index;

class snapshot_dump_supplement_helper final : public hive::plugins::chain::snapshot_dump_helper
{
public:
  const plugin_external_data_index& get_external_data_index() const
  {
    return external_data_index;
  }

  virtual void store_external_data_info(const hive::chain::abstract_plugin& plugin, const fc::path& storage_path) override
  {
    plugin_external_data_info info;
    info.path = storage_path;
    auto ii = external_data_index.emplace(plugin.get_name(), info);
    FC_ASSERT(ii.second, "Only one external data path allowed per plugin");
  }

private:
  plugin_external_data_index external_data_index;
};

class snapshot_load_supplement_helper final : public hive::plugins::chain::snapshot_load_helper
{
public:
  explicit snapshot_load_supplement_helper(const plugin_external_data_index& idx) : ext_data_idx(idx) {}

  virtual bool load_external_data_info(const hive::chain::abstract_plugin& plugin, fc::path* storage_path) override
  {
    const std::string& name = plugin.get_name();

    auto i = ext_data_idx.find(name);

    if(i == ext_data_idx.end())
    {
      *storage_path = fc::path();
      return false;
    }

    *storage_path = i->second.path;
    return true;
  }

private:
  const plugin_external_data_index& ext_data_idx;
};

} /// namespace anonymous

FC_REFLECT(index_manifest_info, (name)(dumpedItems)(firstId)(lastId)(indexNextId)(storage_files))
FC_REFLECT(index_manifest_file_info, (relative_path)(file_size))

namespace hive { namespace plugins { namespace state_snapshot {

using ::rocksdb::DB;
using ::rocksdb::DBOptions;
using ::rocksdb::Options;
using ::rocksdb::PinnableSlice;
using ::rocksdb::ReadOptions;
using ::rocksdb::Slice;

using hive::utilities::benchmark_dumper;

namespace {

#define ITEMS_PER_WORKER ((size_t)2000000)

class dumping_worker;
class loading_worker;

/** Helper base class to cover all common functionality across defined comparators.
  *
  */
class AComparator : public ::rocksdb::Comparator
  {
  public:
    virtual const char* Name() const override final
      {
      static const std::string name = boost::core::demangle(typeid(this).name());
      return name.c_str();
      }

    virtual void FindShortestSeparator(std::string* start, const Slice& limit) const override final
      {
        /// Nothing to do.
      }

    virtual void FindShortSuccessor(std::string* key) const override final
      {
        /// Nothing to do.
      }

  protected:
    AComparator() = default;
  };

template <typename T>
class PrimitiveTypeComparatorImpl final : public AComparator
  {
  public:
    virtual int Compare(const Slice& a, const Slice& b) const override
      {
      if(a.size() != sizeof(T) || b.size() != sizeof(T))
        return a.compare(b);

      const T& id1 = retrieveKey(a);
      const T& id2 = retrieveKey(b);

      if(id1 < id2)
        return -1;

      if(id1 > id2)
        return 1;

      return 0;
      }

    virtual bool Equal(const Slice& a, const Slice& b) const override
      {
      if(a.size() != sizeof(T) || b.size() != sizeof(T))
        return a == b;

      const auto& id1 = retrieveKey(a);
      const auto& id2 = retrieveKey(b);

      return id1 == id2;
      }

  private:
    const T& retrieveKey(const Slice& slice) const
      {
      assert(sizeof(T) == slice.size());
      const char* rawData = slice.data();
      return *reinterpret_cast<const T*>(rawData);
      }
  };

typedef PrimitiveTypeComparatorImpl<size_t> size_t_comparator;

size_t_comparator _size_t_comparator;

template <class BaseClass>
class snapshot_processor_data : public BaseClass
  {
  public:
    using typename BaseClass::snapshot_converter_t;

    const bfs::path& get_root_path() const
      {
      return _rootPath;
      }

    ::rocksdb::EnvOptions get_environment_config() const
      {
      return ::rocksdb::EnvOptions();
      }

    ::rocksdb::Options get_storage_config() const
      {
      ::rocksdb::Options opts;
      opts.comparator = &_size_t_comparator;

      opts.max_open_files = 1024;

      return opts;
      }

    snapshot_converter_t get_converter() const
      {
      return _converter;
      }

    void set_processing_success(bool s)
      {
      _processingSuccess = s;
      }

    bfs::path prepare_index_storage_path(const std::string& indexDescription) const
      {
      std::string baseFilePath = indexDescription;
      boost::replace_all(baseFilePath, " ", "_");
      boost::replace_all(baseFilePath, ":", "_");
      boost::replace_all(baseFilePath, "<", "_");
      boost::replace_all(baseFilePath, ">", "_");

      bfs::path storagePath(_rootPath);
      storagePath /= baseFilePath;

      return storagePath;
      }

    const std::string& getIndexDescription() const
      {
      return _indexDescription;
      }

    /// Returns true if given index shall be processed.
    bool process_index(const std::string& indexDescription) const
      {
      //if(indexDescription == "comment_object")
        return true;

      //return false;
      }

    bool perform_debug_logging() const
    {
      //if(indexDescription == "comment_object")
      //  return true;

      return false;
    }


  protected:
    snapshot_processor_data(const bfs::path& rootPath) :
      _rootPath(rootPath) {}

  protected:
    snapshot_converter_t _converter;
    bfs::path _rootPath;
    std::string _indexDescription;

    bool _processingSuccess = false;
  };

class index_dump_writer final : public snapshot_processor_data<chainbase::snapshot_writer>
  {
  public:
    index_dump_writer(const chain::database& mainDb, const chainbase::abstract_index& index, const bfs::path& outputRootPath,
      bool allow_concurrency) :
      snapshot_processor_data<chainbase::snapshot_writer>(outputRootPath), _mainDb(mainDb), _index(index), _firstId(0), _lastId(0),
      _nextId(0), _allow_concurrency(allow_concurrency) {}

    index_dump_writer(const index_dump_writer&) = delete;
    index_dump_writer& operator=(const index_dump_writer&) = delete;

    virtual ~index_dump_writer() = default;

    virtual workers prepare(const std::string& indexDescription, size_t firstId, size_t lastId, size_t indexSize, size_t indexNextId,
      snapshot_converter_t converter) override;
    virtual void start(const workers& workers) override;

    const chain::database& getMainDb() const
    {
      return _mainDb;
    }

    void store_index_manifest(index_manifest_info* manifest) const;

  private:
    const chain::database& _mainDb;
    const chainbase::abstract_index& _index;
    std::vector <std::unique_ptr<dumping_worker>> _builtWorkers;
    size_t _firstId;
    size_t _lastId;
    size_t _nextId;
    bool   _allow_concurrency;
  };

class index_dump_reader final : public snapshot_processor_data<chainbase::snapshot_reader>
  {
  public:
    index_dump_reader(const snapshot_manifest& snapshotManifest, const bfs::path& rootPath) :
      snapshot_processor_data<chainbase::snapshot_reader>(rootPath),
      _snapshotManifest(snapshotManifest), currentWorker(nullptr) {}

    index_dump_reader(const index_dump_reader&) = delete;
    index_dump_reader& operator=(const index_dump_reader&) = delete;

    virtual ~index_dump_reader() = default;

    virtual workers prepare(const std::string& indexDescription, snapshot_converter_t converter, size_t* snapshot_index_next_id) override;
    virtual void start(const workers& workers) override;

    size_t getCurrentlyProcessedId() const;

  private:
    const snapshot_manifest& _snapshotManifest;
    std::vector <std::unique_ptr<loading_worker>> _builtWorkers;
    const loading_worker* currentWorker;
  };

class dumping_worker final : public chainbase::snapshot_writer::worker
  {
  public:
    dumping_worker(const bfs::path& outputFile, index_dump_writer& writer, size_t startId, size_t endId) :
      chainbase::snapshot_writer::worker(writer, startId, endId), _controller(writer), _outputFile(outputFile),
      _writtenEntries(0), _write_finished(false)
      {
      }

    virtual ~dumping_worker()
      {
      if(_writer)
        _writer->Finish();
      }

    void perform_dump();

    void store_index_manifest(index_manifest_info* manifest) const;

    bool is_write_finished(size_t* writtenEntries = nullptr) const
      {
      if(writtenEntries != nullptr)
        *writtenEntries = _writtenEntries;

      return _write_finished;
      }

    const bfs::path& get_output_file() const
      {
      return _outputFile;
      }

  private:
    virtual void flush_converted_data(const serialized_object_cache& cache) override;
    virtual std::string prettifyObject(const fc::variant& object, const std::vector<char>& buffer) const override
    {
      std::string s;

      if(_controller.perform_debug_logging())
      {
        s = fc::json::to_pretty_string(object);
        s += '\n';
        s += "Buffer size: " + std::to_string(buffer.size());
      }

      return s;
    }

    void prepareWriter();

  private:
    index_dump_writer& _controller;
    bfs::path _outputFile;
    std::unique_ptr<::rocksdb::SstFileWriter> _writer;
    ::rocksdb::ExternalSstFileInfo _sstFileInfo;
    size_t _writtenEntries;
    bool _write_finished;
  };

void dumping_worker::perform_dump()
  {
  ilog("Performing a dump into file ${p}", ("p", _outputFile.string()));

  try
    {
    auto converter = _controller.get_converter();
    converter(this);

    if(_writer)
      {
      _writer->Finish(&_sstFileInfo);
      _writer.release();
      }

    _write_finished = true;

    ilog("Finished dump into file ${p}", ("p", _outputFile.string()));
    }
  FC_CAPTURE_AND_LOG(())
  }

void dumping_worker::store_index_manifest(index_manifest_info* manifest) const
  {
  FC_ASSERT(is_write_finished(), "Write not finished yet for worker processing range: <${b}, ${e}> for index: `${i}'",
    ("b", _startId)("e", _endId)("i", _controller.getIndexDescription()));
  FC_ASSERT(!_writer);

  FC_ASSERT(_writtenEntries == 0 || _sstFileInfo.file_size != 0);

  auto relativePath = bfs::relative(_outputFile, _controller.get_root_path());

  manifest->storage_files.emplace_back(index_manifest_file_info{ relativePath.string(), _sstFileInfo.file_size });
  }

void dumping_worker::flush_converted_data(const serialized_object_cache& cache)
  {
  if(!_writer)
    prepareWriter();

  ilog("Flushing converted data <${f}:${b}> for file ${o}", ("f", cache.front().first)("b", cache.back().first)("o", _outputFile.string()));

  for(const auto& kv : cache)
    {
    Slice key(reinterpret_cast<const char*>(&kv.first), sizeof(kv.first));
    Slice value(kv.second.data(), kv.second.size());
    auto status = _writer->Put(key, value);
    if(status.ok() == false)
      {
      elog("Cannot write to output file: `${p}'. Error details: `${e}'.", ("p", _outputFile.string())("e", status.ToString()));
      ilog("Failing key value: ${k}", ("k", kv.first));

      throw std::exception();
      }
    }

  _writtenEntries += cache.size();
  }

void dumping_worker::prepareWriter()
  {
  auto options = _controller.get_storage_config();
  auto envOptions = _controller.get_environment_config();

  _writer = std::make_unique<::rocksdb::SstFileWriter>(envOptions, options);
  ilog("Trying to open SST Writer output file: `${p}'", ("p", _outputFile.string()));

  auto status = _writer->Open(_outputFile.string());
  if(status.ok())
    {
    ilog("Successfully opened SST Writer output file: `${p}'", ("p", _outputFile.string()));
    }
  else
    {
    elog("Cannot open SST Writer output file: `${p}'. Error details: `${e}'.", ("p", _outputFile.string())("e", status.ToString()));
    _writer.release();
    throw std::exception();
    }
  }

chainbase::snapshot_writer::workers 
index_dump_writer::prepare(const std::string& indexDescription, size_t firstId, size_t lastId, size_t indexSize,
  size_t indexNextId, snapshot_converter_t converter)
  {
  ilog("Preparing snapshot writer to store index holding `${d}' items. Index size: ${s}. Index next_id: ${indexNextId}.Index id range: <${f}, ${l}>.",
    ("d", indexDescription)("s", indexSize)(indexNextId)("f", firstId)("l", lastId));

  _converter = converter;
  _indexDescription = indexDescription;
  _firstId = firstId;
  _lastId = lastId;
  _nextId = indexNextId;

  if(indexSize == 0 || process_index(indexDescription) == false)
    return workers();

  chainbase::snapshot_writer::workers retVal;

  size_t workerCount = indexSize / ITEMS_PER_WORKER + 1;
  size_t left = firstId;
  size_t right = 0;

  if(indexSize <= ITEMS_PER_WORKER)
    right = lastId;
  else
    right = std::min(firstId + ITEMS_PER_WORKER, lastId);

  ilog("Preparing ${n} workers to store index holding `${d}' items.", ("d", indexDescription)("n", workerCount));

  bfs::path outputPath = prepare_index_storage_path(indexDescription);
  bfs::create_directories(outputPath);

  for(size_t i = 0; i < workerCount; ++i)
    {
    std::string fileName = std::to_string(left) + '_' + std::to_string(right) + ".sst";
    bfs::path actualOutputPath(outputPath);
    actualOutputPath /= fileName;

    _builtWorkers.emplace_back(std::make_unique<dumping_worker>(actualOutputPath, *this, left, right));

    retVal.emplace_back(_builtWorkers.back().get());

    left = right + 1;
    if(i == workerCount - 2) /// Last iteration shall cover items up to container end.
      right = lastId + 1;
    else
      right += ITEMS_PER_WORKER;
    }
  
  return retVal;
  }

void index_dump_writer::start(const workers& workers)
  {
  FC_ASSERT(_builtWorkers.size() == workers.size());

  const size_t num_threads = _allow_concurrency ? std::min(workers.size(), static_cast<size_t>(16)) : 1;

  if(num_threads > 1)
    {
    boost::asio::io_service ioService;
    boost::thread_group threadpool;
    std::unique_ptr<boost::asio::io_service::work> work = std::make_unique<boost::asio::io_service::work>(ioService);

    for(unsigned int i = 0; i < num_threads; ++i)
      threadpool.create_thread(boost::bind(&boost::asio::io_service::run, &ioService));

    for(size_t i = 0; i < _builtWorkers.size(); ++i)
      {
      dumping_worker* w = _builtWorkers[i].get();
      FC_ASSERT(w == workers[i]);

      ioService.post(boost::bind(&dumping_worker::perform_dump, w));
      }

    ilog("Waiting for dumping-workers jobs completion");

    /// Run the horses...
    work.reset();

    threadpool.join_all();
    }
  else
    {
    for(size_t i = 0; i < _builtWorkers.size(); ++i)
      {
      dumping_worker* w = _builtWorkers[i].get();
      FC_ASSERT(w == workers[i]);

      w->perform_dump();
      }
    }
  }

void index_dump_writer::store_index_manifest(index_manifest_info* manifest) const
  {
  if(process_index(_indexDescription) == false)
    return;

  FC_ASSERT(_processingSuccess);

  manifest->name = _indexDescription;
  manifest->dumpedItems = _index.size();
  manifest->firstId = _firstId;
  manifest->lastId = _lastId;
  manifest->indexNextId = _nextId;

  size_t totalWrittenEntries = 0;

  for(size_t i = 0; i < _builtWorkers.size(); ++i)
    {
    const dumping_worker* w = _builtWorkers[i].get();
    w->store_index_manifest(manifest);
    size_t writtenEntries = 0;
    w->is_write_finished(&writtenEntries);

    totalWrittenEntries += writtenEntries;
    }

  FC_ASSERT(_index.size() == totalWrittenEntries, "Mismatch between written entries: ${e} and size ${s} of index: `${i}",
    ("e", totalWrittenEntries)("s", _index.size())("i", _indexDescription));

  ilog("Saved manifest for index: '${d}' containing ${s} items and ${n} saved as next_id", ("d", _indexDescription)("s", manifest->dumpedItems)("n", manifest->indexNextId));
  }

class loading_worker final : public chainbase::snapshot_reader::worker
  {
  public:
    loading_worker(const index_manifest_info& manifestInfo, const bfs::path& inputPath, index_dump_reader& reader) :
      chainbase::snapshot_reader::worker(reader, 0, 0),
      _manifestInfo(manifestInfo), _controller(reader), _inputPath(inputPath), _load_finished(false)
      {
      _startId = manifestInfo.firstId;
      _endId = manifestInfo.lastId;
      }

    virtual ~loading_worker() = default;

    virtual void load_converted_data(worker_common_base::serialized_object_cache* cache) override;
    virtual std::string prettifyObject(const fc::variant& object, const std::vector<char>& buffer) const override
    {
      std::string s;
      if(_controller.perform_debug_logging())
      {
        s = fc::json::to_pretty_string(object);
        s += '\n';
        s += "Data buffer size: " + std::to_string(buffer.size());
      }

      return s;
    }

    void perform_load();

  private:
    const index_manifest_info& _manifestInfo;
    index_dump_reader& _controller;
    bfs::path _inputPath;
    std::unique_ptr<::rocksdb::SstFileReader> _reader;
    std::unique_ptr<::rocksdb::Iterator> _entryIt;
    bool _load_finished;
  };

void loading_worker::load_converted_data(worker_common_base::serialized_object_cache* cache)
  {
  FC_ASSERT(_entryIt);

  const size_t maxSize = get_serialized_object_cache_max_size();
  cache->reserve(maxSize);
  for(size_t n = 0; _entryIt->Valid() && n < maxSize; _entryIt->Next(), ++n)
    {
    auto key = _entryIt->key();
    auto value = _entryIt->value();

    const size_t* keyId = reinterpret_cast<const size_t*>(key.data());
    FC_ASSERT(sizeof(size_t) == key.size());

    cache->emplace_back(*keyId, std::vector<char>());
    cache->back().second.insert(cache->back().second.end(), value.data(), value.data() + value.size());
    }

  if(cache->empty())
    {
    ilog("No objects loaded from storage for index: `${i}'", ("i", _manifestInfo.name));
    }
  else
    {
    size_t b = cache->front().first;
    size_t e = cache->back().first;

    ilog("Loaded objects from range <${b}, ${e}> for index: `${i}'", ("b", b)("e", e)("i", _manifestInfo.name));
    }
  }

void loading_worker::perform_load()
  {
  if(_manifestInfo.dumpedItems == 0)
    {
    ilog("Snapshot data contains empty-set stored for index: `${i}.", ("i", _manifestInfo.name));
    return;
    }

  for(const auto& fileInfo : _manifestInfo.storage_files)
    {
    _reader = std::make_unique< ::rocksdb::SstFileReader>(_controller.get_storage_config());
    
    bfs::path sstFilePath(_inputPath);
    sstFilePath /= fileInfo.relative_path;
    
    auto status = _reader->Open(sstFilePath.string());
    if(status.ok())
      {
      ilog("Successfully opened index SST file at path: `${p}'", ("p", sstFilePath.string()));
      }
    else
      {
      elog("Cannot open snapshot index SST file at path: `${p}'. Error details: `${e}'.", ("p", sstFilePath.string())("e", status.ToString()));
      throw std::exception();
      }

    ::rocksdb::ReadOptions rOptions;
    _entryIt.reset(_reader->NewIterator(rOptions));
    _entryIt->SeekToFirst();

    auto converter = _controller.get_converter();
    converter(this);

    _entryIt.reset();
    _reader.reset();

    ilog("Finished processing of SST file at path: `${p}'", ("p", sstFilePath.string()));
    }
  }

chainbase::snapshot_reader::workers 
index_dump_reader::prepare(const std::string& indexDescription, snapshot_converter_t converter, size_t* snapshot_index_next_id)
  {
  _converter = converter;
  _indexDescription = indexDescription;

  index_manifest_info key;
  key.name = indexDescription;
  auto snapshotIt = _snapshotManifest.find(key);

  if(snapshotIt == _snapshotManifest.end())
    {
    elog("chainbase index `${i}' has no data saved in the snapshot. chainbase index cleared, but no data loaded.", ("i", indexDescription));
    return workers();
    }

  const index_manifest_info& manifestInfo = *snapshotIt;

  *snapshot_index_next_id = manifestInfo.indexNextId;

  _builtWorkers.emplace_back(std::make_unique<loading_worker>(manifestInfo, _rootPath, *this));

  workers retVal;
  retVal.emplace_back(_builtWorkers.front().get());

  return retVal;
  }

void index_dump_reader::start(const workers& workers)
  {
  FC_ASSERT(_builtWorkers.size() == workers.size());

  for(size_t i = 0; i < _builtWorkers.size(); ++i)
    {
    loading_worker* w = _builtWorkers[i].get();
    FC_ASSERT(w == workers[i]);
    currentWorker = w;
    w->perform_load();
    }
  }

size_t index_dump_reader::getCurrentlyProcessedId() const
  {
  return currentWorker->getProcessedId();
  }

} /// namespace anonymous

class state_snapshot_plugin::impl final : protected chain::state_snapshot_provider
  {
  using database = hive::chain::database;

  public:
    impl(state_snapshot_plugin& self, const bpo::variables_map& options) :
      _self(self),
      _mainDb(appbase::app().get_plugin<hive::plugins::chain::chain_plugin>().db())
      {
      collectOptions(options);

      appbase::app().get_plugin<hive::plugins::chain::chain_plugin>().register_snapshot_provider(*this);

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
      void safe_spawn_snapshot_dump(const chainbase::abstract_index* idx, index_dump_writer* writer);
      void safe_spawn_snapshot_load(chainbase::abstract_index* idx, index_dump_reader* reader);
      void store_snapshot_manifest(const bfs::path& actualStoragePath, const std::vector<std::unique_ptr<index_dump_writer>>& builtWriters,
        const snapshot_dump_supplement_helper& dumpHelper) const;

      std::tuple<snapshot_manifest, plugin_external_data_index, std::string, uint32_t> load_snapshot_manifest(const bfs::path& actualStoragePath);
      void load_snapshot_external_data(const plugin_external_data_index& idx);

      void load_snapshot_impl(const std::string& snapshotName, const hive::chain::open_args& openArgs);

    private:
      state_snapshot_plugin&  _self;
      database&               _mainDb;
      bfs::path               _storagePath;
      std::unique_ptr<DB>     _storage;
      std::string             _snapshot_name;
      uint32_t                _num_threads = 32;
      bool                    _do_immediate_load = false;
      bool                    _do_immediate_dump = false;
      bool                    _allow_concurrency = true;
  };

void state_snapshot_plugin::impl::collectOptions(const bpo::variables_map& options)
  {
  bfs::path snapshotPath;

  if(options.count("snapshot-root-dir"))
    snapshotPath = options.at("snapshot-root-dir").as<bfs::path>();

  if(snapshotPath.is_absolute() == false)
    {
    auto basePath = appbase::app().data_dir();
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

  FC_ASSERT(!_do_immediate_load || !_do_immediate_dump, "You can only dump or load snapshot at once.");

  fc::mutable_variant_object state_opts;

  appbase::app().get_plugin< hive::plugins::chain::chain_plugin >().report_state_options(_self.name(), state_opts);
  }

std::string state_snapshot_plugin::impl::generate_name() const
  {
  return "snapshot_" + std::to_string(fc::time_point::now().sec_since_epoch());
  }

void state_snapshot_plugin::impl::safe_spawn_snapshot_dump(const chainbase::abstract_index* idx, index_dump_writer* writer)
  {
  try
    {
    writer->set_processing_success(false);
    idx->dump_snapshot(*writer);
    writer->set_processing_success(true);
    }
  FC_CAPTURE_AND_LOG(())
  }

void state_snapshot_plugin::impl::store_snapshot_manifest(const bfs::path& actualStoragePath,
  const std::vector<std::unique_ptr<index_dump_writer>>& builtWriters, const snapshot_dump_supplement_helper& dumpHelper) const
  {
  bfs::path manifestDbPath(actualStoragePath);
  manifestDbPath /= "snapshot-manifest";

  if(bfs::exists(manifestDbPath) == false)
    bfs::create_directories(manifestDbPath);

  ::rocksdb::Options dbOptions;
  dbOptions.create_if_missing = true;
  dbOptions.max_open_files = 1024;

  rocksdb_cleanup_helper db = rocksdb_cleanup_helper::open(dbOptions, manifestDbPath);
  ::rocksdb::ColumnFamilyHandle* manifestCF = db.create_column_family("INDEX_MANIFEST");
  ::rocksdb::ColumnFamilyHandle* externalDataCF = db.create_column_family("EXTERNAL_DATA");
  ::rocksdb::ColumnFamilyHandle* snapshotManifestCF = db.create_column_family("IRREVERSIBLE_STATE");
  ::rocksdb::ColumnFamilyHandle* StateDefinitionsDataCF = db.create_column_family("STATE_DEFINITIONS_DATA");

  ::rocksdb::WriteOptions writeOptions;

  std::vector<std::pair<index_manifest_info, std::vector<char>>> infoCache;

  for(const auto& w : builtWriters)
    {
    infoCache.emplace_back(index_manifest_info(), std::vector<char>());
    index_manifest_info& info = infoCache.back().first;
    w->store_index_manifest(&info);

    std::vector<char>& storage = infoCache.back().second;

    chainbase::serialization::pack_to_buffer(storage, info);
    Slice key(info.name);
    Slice value(storage.data(), storage.size());

    auto status = db->Put(writeOptions, manifestCF, key, value);
    if(status.ok() == false)
      {
      elog("Cannot write an index manifest entry to output file: `${p}'. Error details: `${e}'.", ("p", manifestDbPath.string())("e", status.ToString()));
      ilog("Failing key value: ${k}", ("k", info.name));

      throw std::exception();
      }
    }

  const auto& extDataIdx = dumpHelper.get_external_data_index();

  for(const auto& d : extDataIdx)
  {
    const auto& plugin_name = d.first;
    const auto& path = d.second.path;

    auto relativePath = bfs::relative(path, actualStoragePath);
    auto relativePathStr = relativePath.string();

    Slice key(plugin_name);
    Slice value(relativePathStr);

    auto status = db->Put(writeOptions, externalDataCF, key, value);
    if(status.ok() == false)
    {
      elog("Cannot write an index manifest entry to output file: `${p}'. Error details: `${e}'.", ("p", manifestDbPath.string())("e", status.ToString()));
      ilog("Failing key value: ${k}", ("k", plugin_name));

      throw std::exception();
    }
  }

  {
    Slice key("LAST_IRREVERSIBLE_BLOCK");
    uint32_t lib = _mainDb.get_last_irreversible_block_num();
    Slice value(reinterpret_cast<const char*>(&lib), sizeof(uint32_t));
    auto status = db->Put(writeOptions, snapshotManifestCF, key, value);

    if(status.ok() == false)
    {
      elog("Cannot write an index manifest entry to output file: `${p}'. Error details: `${e}'.", ("p", manifestDbPath.string())("e", status.ToString()));
      ilog("Failing key value: \"LAST_IRREVERSIBLE_BLOCK\"");

      throw std::exception();
    }
  }

  {
    Slice key("SNAPSHOT_VERSION");
    Slice value(SNAPSHOT_FORMAT_VERSION);
    auto status = db->Put(writeOptions, snapshotManifestCF, key, value);

    if (status.ok() == false)
    {
      elog("Cannot write an index manifest entry to output file: `${p}'. Error details: `${e}'.", ("p", manifestDbPath.string())("e", status.ToString()));
      ilog("Failing key value: \"SNAPSHOT_VERSION\"");

      throw std::exception();
    }
  }

    {
    const std::string json = _mainDb.get_decoded_state_objects_data();
    Slice key("STATE_DEFINITIONS_DATA");
    Slice value(json);
    auto status = db->Put(writeOptions, StateDefinitionsDataCF, key, value);

    if(status.ok() == false)
    {
      elog("Cannot write an index manifest entry to output file: `${p}'. Error details: `${e}'.", ("p", manifestDbPath.string())("e", status.ToString()));
      ilog("Failing key value: \"STATE_DEFINITIONS_DATA\"");

      throw std::exception();
    }
  }

  db.close();
  }

std::tuple<snapshot_manifest, plugin_external_data_index, std::string, uint32_t> state_snapshot_plugin::impl::load_snapshot_manifest(const bfs::path& actualStoragePath)
{
  bfs::path manifestDbPath(actualStoragePath);
  manifestDbPath /= "snapshot-manifest";

  ::rocksdb::Options dbOptions;
  dbOptions.create_if_missing = false;
  dbOptions.max_open_files = 1024;
  
  ::rocksdb::ColumnFamilyDescriptor cfDescriptor;
  cfDescriptor.name = "INDEX_MANIFEST";

  std::vector <::rocksdb::ColumnFamilyDescriptor> cfDescriptors;
  cfDescriptors.emplace_back(::rocksdb::kDefaultColumnFamilyName, ::rocksdb::ColumnFamilyOptions());
  cfDescriptors.push_back(cfDescriptor);

  cfDescriptor = ::rocksdb::ColumnFamilyDescriptor();
  cfDescriptor.name = "EXTERNAL_DATA";
  cfDescriptors.push_back(cfDescriptor);

  cfDescriptor = ::rocksdb::ColumnFamilyDescriptor();
  cfDescriptor.name = "IRREVERSIBLE_STATE";
  cfDescriptors.push_back(cfDescriptor);

  cfDescriptor = ::rocksdb::ColumnFamilyDescriptor();
  cfDescriptor.name = "STATE_DEFINITIONS_DATA";
  cfDescriptors.push_back(cfDescriptor);

  std::vector<::rocksdb::ColumnFamilyHandle*> cfHandles;
  std::unique_ptr<::rocksdb::DB> manifestDbPtr;
  ::rocksdb::DB* manifestDb = nullptr;
  auto status = ::rocksdb::DB::OpenForReadOnly(dbOptions, manifestDbPath.string(), cfDescriptors, &cfHandles , &manifestDb);
  manifestDbPtr.reset(manifestDb);
  if(status.ok())
    {
    ilog("Successfully opened snapshot manifest-db at path: `${p}'", ("p", manifestDbPath.string()));
    }
  else
    {
    elog("Cannot open snapshot manifest-db at path: `${p}'. Error details: `${e}'.", ("p", manifestDbPath.string())("e", status.ToString()));
    throw std::exception();
    }

  ilog("Attempting to read snapshot manifest from opened database...");

  snapshot_manifest retVal;
  
  {
    ::rocksdb::ReadOptions rOptions;

    std::unique_ptr<::rocksdb::Iterator> indexIterator(manifestDb->NewIterator(rOptions, cfHandles[1]));

    std::vector<char> buffer;
    for(indexIterator->SeekToFirst(); indexIterator->Valid(); indexIterator->Next())
    {
      auto keySlice = indexIterator->key();
      auto valueSlice = indexIterator->value();

      buffer.insert(buffer.end(), valueSlice.data(), valueSlice.data() + valueSlice.size());

      index_manifest_info info;

      chainbase::serialization::unpack_from_buffer(info, buffer);

      FC_ASSERT(keySlice.ToString() == info.name);

      ilog("Loaded manifest info for index ${i}, containing ${s} items, next_id: ${n}, having storage files: ${sf}", ("i", info.name)("s", info.dumpedItems)("n", info.indexNextId)("sf", info.storage_files));

      retVal.emplace(std::move(info));

      buffer.clear();
    }
  }

  plugin_external_data_index extDataIdx;

  {
    ::rocksdb::ReadOptions rOptions;

    std::unique_ptr<::rocksdb::Iterator> indexIterator(manifestDb->NewIterator(rOptions, cfHandles[2]));

    std::vector<char> buffer;
    for(indexIterator->SeekToFirst(); indexIterator->Valid(); indexIterator->Next())
    {
      auto keySlice = indexIterator->key();
      auto valueSlice = indexIterator->value();

      buffer.insert(buffer.end(), valueSlice.data(), valueSlice.data() + valueSlice.size());

      std::string plugin_name = keySlice.data();
      std::string relative_path = { buffer.begin(), buffer.end() };

      buffer.clear();

      ilog("Loaded external data info for plugin ${p} having storage of external files inside: `${d}' (relative path)", ("p", plugin_name)("d", relative_path));

      bfs::path extDataPath(actualStoragePath);
      extDataPath /= relative_path;

      if(bfs::exists(extDataPath) == false)
      {
        elog("Specified path to the external data does not exists: `${d}'.", ("d", extDataPath.string()));
        throw std::exception();
      }

      plugin_external_data_info info;
      info.path = extDataPath;
      auto ii = extDataIdx.emplace(plugin_name, info);
      FC_ASSERT(ii.second, "Multiple entries for plugin: ${p}", ("p", plugin_name));
    }
  }

  uint32_t lib = 0;

  {
    ::rocksdb::ReadOptions rOptions;

    std::unique_ptr<::rocksdb::Iterator> irreversibleStateIterator(manifestDb->NewIterator(rOptions, cfHandles[3]));
    irreversibleStateIterator->SeekToFirst();
    FC_ASSERT(irreversibleStateIterator->Valid(), "No entry for IRREVERSIBLE_STATE. Probably used old snapshot format (must be regenerated).");

    std::vector<char> buffer;
    Slice keySlice = irreversibleStateIterator->key();
    auto valueSlice = irreversibleStateIterator->value();

    std::string keyName = keySlice.ToString();;

    FC_ASSERT(keyName == "LAST_IRREVERSIBLE_BLOCK", "Broken snapshot - no entry for LAST_IRREVERSIBLE_BLOCK");

    buffer.insert(buffer.end(), valueSlice.data(), valueSlice.data() + valueSlice.size());
    chainbase::serialization::unpack_from_buffer(lib, buffer);
    buffer.clear();
    //ilog("lib: ${s}", ("s", lib));

    irreversibleStateIterator->Next();
    FC_ASSERT(irreversibleStateIterator->Valid(), "Expected multiple entries specifying irreversible block.");

    keySlice = irreversibleStateIterator->key();
    valueSlice = irreversibleStateIterator->value();
    keyName = keySlice.ToString();

    FC_ASSERT(keyName == "SNAPSHOT_VERSION", "Broken snapshot - no entry for SNAPSHOT_VERSION, ${k} found.", ("k", keyName));

    std::string versionValue = valueSlice.ToString();
    FC_ASSERT(versionValue == SNAPSHOT_FORMAT_VERSION, "Snapshot version mismatch - ${f} found, ${e} expected.", ("f", versionValue)("e", SNAPSHOT_FORMAT_VERSION));

    irreversibleStateIterator->Next();

    FC_ASSERT(irreversibleStateIterator->Valid() == false, "Unexpected entries specifying irreversible block ?");
  }

  std::string state_definitions_data;

  {
    ::rocksdb::ReadOptions rOptions;

    std::unique_ptr<::rocksdb::Iterator> stateDefinitionsDataIterator(manifestDb->NewIterator(rOptions, cfHandles[4]));
    stateDefinitionsDataIterator->SeekToFirst();
    FC_ASSERT(stateDefinitionsDataIterator->Valid(), "No entry for STATE_DEFINITIONS_DATA. Probably used old snapshot format (must be regenerated).");

    Slice keySlice = stateDefinitionsDataIterator->key();
    auto valueSlice = stateDefinitionsDataIterator->value();

    std::string keyName = keySlice.ToString();
    FC_ASSERT(keyName == "STATE_DEFINITIONS_DATA", "Broken snapshot - no entry for STATE_DEFINITIONS_DATA");
    state_definitions_data = valueSlice.ToString();
  }

  for(auto* cfh : cfHandles)
  {
    status = manifestDb->DestroyColumnFamilyHandle(cfh);
    if(status.ok() == false)
    {
      elog("Cannot destroy column family handle...'. Error details: `${e}'.", ("e", status.ToString()));
    }
  }

  manifestDb->Close();
  manifestDbPtr.release();

  return std::make_tuple(retVal, extDataIdx, std::move(state_definitions_data), lib);
}

void state_snapshot_plugin::impl::load_snapshot_external_data(const plugin_external_data_index& idx)
  {
  snapshot_load_supplement_helper load_helper(idx);

  hive::chain::load_snapshot_supplement_notification notification(load_helper);

  _mainDb.notify_load_snapshot_data_supplement(notification);
  }

void state_snapshot_plugin::impl::safe_spawn_snapshot_load(chainbase::abstract_index* idx, index_dump_reader* reader)
  {
  try
  {
    reader->set_processing_success(false);
    idx->load_snapshot(*reader);
    reader->set_processing_success(true);
  }
  catch( boost::interprocess::bad_alloc& ex )
  {
    wlog("Problem with a snapshot allocation. A value of `shared-file-size` option has to be greater or equals to a size of snapshot data...");
    wlog( "${details}", ("details",ex.what()) );
    wlog("index description: ${idx_desc} id: ${id}", ("idx_desc", reader->getIndexDescription())("id", reader->getCurrentlyProcessedId()));
    throw;
  }
  catch(...)
  {
    wlog("index description: ${idx_desc} id: ${id}", ("idx_desc", reader->getIndexDescription())("id", reader->getCurrentlyProcessedId()));
    throw fc::unhandled_exception( FC_LOG_MESSAGE( warn, "Unknown error occured when a snapshot's loading" ), std::current_exception() );
  }
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

  ilog("Attempting to dump contents of ${n} indices...", ("n", indices.size()));

  boost::asio::io_service ioService;
  boost::thread_group threadpool;
  std::unique_ptr<boost::asio::io_service::work> work = std::make_unique<boost::asio::io_service::work>(ioService);

  for(unsigned int i = 0; _allow_concurrency && i < _num_threads; ++i)
    threadpool.create_thread(boost::bind(&boost::asio::io_service::run, &ioService));

  std::vector<std::unique_ptr<index_dump_writer>> builtWriters;

  for(const chainbase::abstract_index* idx : indices)
    {
    builtWriters.emplace_back(std::make_unique<index_dump_writer>(_mainDb, *idx, actualStoragePath, _allow_concurrency));
    index_dump_writer* writer = builtWriters.back().get();

    if(_allow_concurrency)
      ioService.post(boost::bind(&impl::safe_spawn_snapshot_dump, this, idx, writer));
    else
      safe_spawn_snapshot_dump(idx, writer);
    }

  ilog("Waiting for dumping jobs completion");

  /// Run the horses...
  work.reset();

  threadpool.join_all();

  fc::path external_data_storage_base_path(actualStoragePath);
  external_data_storage_base_path /= "ext_data";

  if(bfs::exists(external_data_storage_base_path) == false)
    bfs::create_directories(external_data_storage_base_path);

  snapshot_dump_supplement_helper dump_helper;
  
  hive::chain::prepare_snapshot_supplement_notification notification(external_data_storage_base_path, dump_helper);

  _mainDb.notify_prepare_snapshot_data_supplement(notification);

  store_snapshot_manifest(actualStoragePath, builtWriters, dump_helper);

  auto blockNo = _mainDb.head_block_num();

  const auto& measure = dumper.measure(blockNo, [](benchmark_dumper::index_memory_details_cntr_t&, bool) {});
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

  auto snapshotManifest = load_snapshot_manifest(actualStoragePath);
  const std::string& loaded_decoded_type_data = std::get<2>(snapshotManifest);

  {
    chain::util::decoded_types_data_storage dtds(_mainDb.get_decoded_state_objects_data());
    auto result = dtds.check_if_decoded_types_data_json_matches_with_current_decoded_data(loaded_decoded_type_data);

    if (!result.first)
    {
      std::fstream loaded_decoded_types_details, current_decoded_types_details;
      constexpr char current_data_filename[] = "current_decoded_types_details.log";
      constexpr char loaded_data_filename[] = "loaded_from_snapshot_decoded_types_details.log";

      loaded_decoded_types_details.open(loaded_data_filename, std::ios::out | std::ios::trunc);
      if (loaded_decoded_types_details.good())
        loaded_decoded_types_details << dtds.generate_decoded_types_data_pretty_string(loaded_decoded_type_data);
      loaded_decoded_types_details.flush();
      loaded_decoded_types_details.close();

      current_decoded_types_details.open(current_data_filename, std::ios::out | std::ios::trunc);
      if (current_decoded_types_details.good())
        current_decoded_types_details << dtds.generate_decoded_types_data_pretty_string();
      current_decoded_types_details.flush();
      current_decoded_types_details.close();

      FC_THROW_EXCEPTION(chain::snapshot_state_definitions_mismatch_exception,
        "Details:\n ${details}"
        "\nFull data about decoded state objects are in files: ${current_data_filename}, ${loaded_data_filename}",
        ("details", result.second)(current_data_filename)(loaded_data_filename));
    }
  }

  _mainDb.resetState(openArgs);
  _mainDb.set_decoded_state_objects_data(loaded_decoded_type_data);

  const auto& indices = _mainDb.get_abstract_index_cntr();

  ilog("Attempting to load contents of ${n} indices...", ("n", indices.size()));

  boost::asio::io_service ioService;
  boost::thread_group threadpool;
  std::unique_ptr<boost::asio::io_service::work> work = std::make_unique<boost::asio::io_service::work>(ioService);

  for(unsigned int i = 0; _allow_concurrency && i < _num_threads; ++i)
    threadpool.create_thread(boost::bind(&boost::asio::io_service::run, &ioService));

  std::vector<std::unique_ptr< index_dump_reader>> builtReaders;

  for(chainbase::abstract_index* idx : indices)
    {
    builtReaders.emplace_back(std::make_unique<index_dump_reader>(std::get<0>(snapshotManifest), actualStoragePath));
    index_dump_reader* reader = builtReaders.back().get();

    if(_allow_concurrency)
      ioService.post(boost::bind(&impl::safe_spawn_snapshot_load, this, idx, reader));
    else
      safe_spawn_snapshot_load(idx, reader);
    }

  ilog("Waiting for loading jobs completion");

  /// Run the horses...
  work.reset();

  threadpool.join_all();

  plugin_external_data_index& extDataIdx = std::get<1>(snapshotManifest);
  if(extDataIdx.empty())
  {
    ilog("Skipping external data load due to lack of data saved to the snapshot");
  }
  else
  {
    load_snapshot_external_data(extDataIdx);
  }

  auto last_irr_block = std::get<3>(snapshotManifest);
  // set irreversible block number after database::resetState
  _mainDb.set_last_irreversible_block_num(last_irr_block);

  auto blockNo = _mainDb.head_block_num();

  ilog("Setting chainbase revision to ${b} block... Loaded irreversible block is: ${lib}.", ("b", blockNo)("lib", last_irr_block));

  _mainDb.set_revision(blockNo);
  _mainDb.load_state_initial_data(openArgs);

  const auto& measure = dumper.measure(blockNo, [](benchmark_dumper::index_memory_details_cntr_t&, bool) {});
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
      hive::notify_hived_status("loading snapshot");
      load_snapshot(_snapshot_name, openArgs);
      hive::notify_hived_status("finished loading snapshot");
    }

    if(_do_immediate_dump)
    {
      hive::notify_hived_status("dumping snapshot");
      prepare_snapshot(_snapshot_name);
      hive::notify_hived_status("finished dumping snapshot");
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
    ;
  }

void state_snapshot_plugin::plugin_initialize(const boost::program_options::variables_map& options)
  {
  ilog("Initializing state_snapshot_plugin...");
  _my = std::make_unique<impl>(*this, options);
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

