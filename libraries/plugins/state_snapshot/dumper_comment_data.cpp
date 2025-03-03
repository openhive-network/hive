#include <hive/plugins/state_snapshot/dumper_comment_data.hpp>

namespace hive {

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

  dumper_comment_data::dumper_comment_data()
  {
    _storagePath = "~/src/dump";
    prepare( "test");
  }

  dumper_comment_data::~dumper_comment_data()
  {
    if(_writer)
      _writer->Finish();
  }

  void dumper_comment_data::prepare( const std::string& snapshotName )
  {
    bfs::path actualStoragePath = _storagePath / snapshotName;
    actualStoragePath = actualStoragePath.normalize();

    if(bfs::exists(actualStoragePath) == false)
      bfs::create_directories(actualStoragePath);
    else
    {
      FC_ASSERT(bfs::is_empty(actualStoragePath), "Directory ${p} is not empty. Creating snapshot rejected.", ("p", actualStoragePath.string()));
    }

    _outputFile = actualStoragePath / "xyz.sst";

    if(!_writer)
      prepareWriter();
  }

  ::rocksdb::EnvOptions dumper_comment_data::get_environment_config() const
    {
    return ::rocksdb::EnvOptions();
    }

  ::rocksdb::Options dumper_comment_data::get_storage_config() const
    {
    ::rocksdb::Options opts;
    opts.comparator = &_string_comparator;

    opts.max_open_files = 1024;

    return opts;
    }

  void dumper_comment_data::prepareWriter()
    {
    auto options = get_storage_config();
    auto envOptions = get_environment_config();

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

    void dumper_comment_data::write_impl( const std::string& obj )
    {
      Slice key( obj.data(), obj.size() );
      Slice value( "" );
      auto status = _writer->Put(key, value);
      if(status.ok() == false)
        {
        elog("Cannot write to output file: `${p}'. Error details: `${e}'.", ("p", _outputFile.string())("e", status.ToString()));
        ilog("Failing key value: ${k}", ("k", obj));

        throw std::exception();
        }
    }

    void dumper_comment_data::write( const void* obj )
    {
      const hive::chain::comment_object* _comment = reinterpret_cast<const hive::chain::comment_object*>( obj );
      FC_ASSERT( _comment );
      //ilog( _comment->get_author_and_permlink_hash().str() );
      
      write_impl( _comment->get_author_and_permlink_hash().str() );
    }

}
