
#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/comment_object.hpp>

#include <hive/chain/external_storage/rocksdb_storage_provider.hpp>
#include <hive/chain/external_storage/comment_rocksdb_objects.hpp>

#include <boost/program_options.hpp>
#include <string>

namespace bpo = boost::program_options;
namespace bfs = boost::filesystem;

using ::rocksdb::Slice;
using ::rocksdb::PinnableSlice;
using ::rocksdb::ReadOptions;

using hive::chain::rocksdb_comment_storage_provider;
using hive::chain::CommentsColumns;
using hive::chain::comment_object;
using hive::chain::rocksdb_comment_object;

class comments_handler: public rocksdb_comment_storage_provider
{
  private:

    std::unique_ptr<::rocksdb::Iterator> it;

  public:

  comments_handler( const bfs::path& blockchain_storage_path, const bfs::path& storage_path, appbase::application* app )
                  : rocksdb_comment_storage_provider( blockchain_storage_path, storage_path, app )
  {
  }

  ~comments_handler() override{}

  comment_object::author_and_permlink_hash_type unpackSlice(const Slice& s )
  {
    comment_object::author_and_permlink_hash_type hash;

    assert( hash->data_size() == s.size() );
    memcpy(hash.data(), s.data(), s.size() );

    return hash;
  }

  void start_scanning( const std::optional<Slice>& start )
  {
    ReadOptions rOptions;
    it = std::unique_ptr<::rocksdb::Iterator>( _storage->NewIterator( rOptions, _columnHandles[CommentsColumns::COMMENT]) );

    if( start )
      it->Seek( *start );
    else
      it->SeekToFirst();
  }

  void read_records( uint32_t limit, std::list<comment_object::author_and_permlink_hash_type>& records )
  {
    const uint32_t _max_limit = 1'000'000;

    if( limit > _max_limit )
      limit = _max_limit;

    uint32_t _cnt = 0;

    for( ; it->Valid() && _cnt < limit; it->Next(), ++_cnt )
    {
      records.push_back( unpackSlice( it->key() ) );
    }
  }

};

struct comments_scanner
{
  void find_rocksdb_comment_objects( std::ofstream& file, const bfs::path& _comments_storage_path )
  {
    comments_handler _ch( ""/*blockchain_storage_path*/, _comments_storage_path, nullptr );

    _ch.init( false/*destroy_on_setup*/);

    uint32_t _limit = 1'000'000;
    std::optional<Slice> _start;

    std::list<comment_object::author_and_permlink_hash_type> _records;

    _ch.start_scanning( _start );

    uint32_t _cnt = 0;
    do
    {
      _records.clear();
      _ch.read_records( _limit, _records );

      if( _records.size() )
      {
        for( auto& item : _records )
        {
          file << item.str() << std::endl;
        }
        auto& _last = _records.back();
        _start = Slice( _last.data(), _last.data_size() );

        _cnt +=  _records.size();
        ilog( "Read ${_cnt} records...", (_cnt) );
      }
    } while( _records.size() );

  }

  void get_hashes( std::ifstream& i_file, std::ofstream& o_file, size_t limit )
  {
    limit /= 2;

    comment_object::author_and_permlink_hash_type _tmp;
    size_t _length = _tmp.str().size() + 1;

    std::list<std::string> _records_00;
    std::list<std::string> _records_01;

    i_file.seekg( 0, std::ios_base::end );
    size_t _file_size = i_file.tellg();
    ilog( "file size: ${_file_size}", (_file_size) );

    i_file.seekg( - limit * _length, std::ios_base::end );

    for( size_t i = 0; i < limit; ++i )
    {
      std::string _content;
      std::getline( i_file, _content );
      _records_00.emplace_back( _content );
    }

    i_file.seekg( 0, std::ios_base::beg );

    for( size_t i = 0; i < limit; ++i )
    {
      std::string _content;
      std::getline( i_file, _content );
      _records_01.emplace_back( _content );
    }

    size_t _cnt = 0;

    auto _it_00 = _records_00.begin();
    auto _it_01 = _records_01.begin();

    while( !( _it_00 == _records_00.end() && _it_01 == _records_01.end() ) )
    {
      if( _cnt % 2 == 0 )
      {
        if( _it_00 != _records_00.end() )
        {
          o_file << ( *_it_00 ) << std::endl;
          ++_it_00;
        }
      }
      else
      {
        if( _it_01 != _records_01.end() )
        {
          o_file << ( *_it_01 ) << std::endl;
          ++_it_01;
        }
      }

      ++_cnt;
    }

    ilog( "Read ${_cnt} records...", (_cnt) );
  }

  void get_rocksdb_comment_objects( std::ifstream& i_file, const bfs::path& _comments_storage_path )
  {
    rocksdb_comment_storage_provider _provider( ""/*blockchain_storage_path*/, _comments_storage_path, nullptr );

    _provider.init( false/*destroy_on_setup*/);

    std::string _content;

    size_t _cnt_found = 0;
    size_t _cnt_not_found = 0;

    while( std::getline( i_file, _content ) )
    {
      auto _hash = comment_object::author_and_permlink_hash_type( _content );

      Slice _key( _hash.data(), _hash.data_size() );

      PinnableSlice _buffer;

      if( _provider.read( _key, _buffer ) )
        ++_cnt_found;
      else
        ++_cnt_not_found;

      //It's not needed here, but in real situation these objects have to be created.
      rocksdb_comment_object _obj;
      load( _obj, _buffer.data(), _buffer.size() );
      auto _a_pointer =std::shared_ptr<comment_object>( new comment_object ( _obj.comment_id, _obj.parent_comment, _hash, _obj.depth ) );

    }
    ilog("found: ${_cnt_found} not found: ${_cnt_not_found}", (_cnt_found)(_cnt_not_found));
  }
};

int main( int argc, char** argv )
{
  try
  {
    bpo::options_description _options("Options");

    _options.add_options()("input,i", bpo::value<boost::filesystem::path>(), "Path to a directory with comments RocksDB");
    _options.add_options()("output,o", bpo::value<boost::filesystem::path>(), "Path to a file where all hashes are saved.");
    _options.add_options()("output_test,o", bpo::value<boost::filesystem::path>(), "Path to a file where all hashes are saved in order to test performance.");

    _options.add_options()("read-db", bpo::bool_switch()->default_value(false), "Read all hashes from RockDB");
    _options.add_options()("create-set", bpo::bool_switch()->default_value(false), "Create a smaller set of hashes");
    _options.add_options()("check-time", bpo::bool_switch()->default_value(false), "Calculate time");

    _options.add_options()("size-set", bpo::value<size_t>()->default_value(1000), "Size of hashes set");

    bpo::parsed_options __options = bpo::command_line_parser(argc, argv).options(_options).run();

    bpo::variables_map _options_map;
    bpo::store( __options, _options_map );

    if( !_options_map.count( "input" ) )
    {
      ilog("Lack of input directory with comments RocksDB.");
      return 0;
    }

    if( !_options_map.count( "output" ) )
    {
      ilog("Lack of an output path where hashes are saved.");
      return 0;
    }

    if( !_options_map.count( "output_test" ) )
    {
      ilog("Lack of an output-test path where hashes are saved.");
      return 0;
    }

    auto _read_db = _options_map.count( "read-db" ) ? _options_map.at( "read-db" ).as<bool>() : false;
    auto _create_set = _options_map.count( "create-set" ) ? _options_map.at( "create-set" ).as<bool>() : false;
    auto _check_time = _options_map.count( "check-time" ) ? _options_map.at( "check-time" ).as<bool>() : false;
    auto _size_set = _options_map.count( "size-set" ) ? _options_map.at( "size-set" ).as<size_t>() : false;

    bfs::path _comments_storage_path = _options_map.at("input").as<bfs::path>();
    bfs::path _file_path = _options_map.at("output").as<bfs::path>();
    bfs::path _file_path_test = _options_map.at("output_test").as<bfs::path>();

    {
      if( _read_db )
      {
        ilog("Read all hashes from RockDB...");
        std::ofstream _file;
        _file.open( _file_path, std::ios::out );

        comments_scanner cs;
        cs.find_rocksdb_comment_objects( _file, _comments_storage_path );

        _file.close();
        ilog("****************************************************");
      }
    }

    {
      if( _create_set )
      {
        ilog("Create a smaller set of hashes...");
        std::ifstream _i_file;
        _i_file.open( _file_path, std::ios::in );

        std::ofstream _o_file;
        _o_file.open( _file_path_test, std::ios::out );

        comments_scanner cs;
        cs.get_hashes( _i_file, _o_file, _size_set );

        _i_file.close();
        _o_file.close();
        ilog("****************************************************");
      }
    }

    {
      if( _check_time )
      {
        ilog("Calculate time...");
        std::ifstream _i_file;
        _i_file.open( _file_path_test, std::ios::in );

        auto _start = std::chrono::high_resolution_clock::now();

        comments_scanner cs;
        cs.get_rocksdb_comment_objects( _i_file, _comments_storage_path );

        auto _interval = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::high_resolution_clock::now() - _start ).count();
        ilog( " Time: ${_interval} [ms]", (_interval) );

        _i_file.close();
        ilog("****************************************************");
      }
    }
  }
  catch ( const boost::exception& e )
  {
    elog( boost::diagnostic_information(e) );
  }
  catch ( const fc::exception& e )
  {
    elog( e.to_detail_string() );
  }
  catch ( const std::exception& e )
  {
    elog( e.what() );
  }
  catch ( ... )
  {
    elog( "unknown exception" );
  }

  return -1;
}
