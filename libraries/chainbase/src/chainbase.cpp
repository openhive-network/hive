#include <chainbase/chainbase.hpp>
#include <boost/array.hpp>
#include <boost/any.hpp>
#include <iostream>
#include <fc/log/logger.hpp>

namespace chainbase {

size_t snapshot_base_serializer::worker_common_base::get_serialized_object_cache_max_size() const
{
  return 512 * 1024;
}

  class environment_check {

    public:

#ifdef ENABLE_STD_ALLOCATOR
      environment_check()
#else
      template< typename Allocator >
      environment_check( allocator< Allocator > a )
                  : version_info( a ), plugins( a )
#endif
      {
        memset( &compiler_version, 0, sizeof( compiler_version ) );
        memcpy( &compiler_version, __VERSION__, std::min<size_t>( strlen(__VERSION__), 256 ) );

#ifndef NDEBUG
        debug = true;
#endif
#ifdef __APPLE__
        apple = true;
#endif
#ifdef WIN32
        windows = true;
#endif
      }

      friend bool operator == ( const environment_check& a, const environment_check& b ) {
        return std::make_tuple( a.compiler_version, a.debug, a.apple, a.windows )
          ==  std::make_tuple( b.compiler_version, b.debug, b.apple, b.windows );
      }

      environment_check& operator = ( const environment_check& other )
      {
#ifndef ENABLE_STD_ALLOCATOR
        plugins = other.plugins;
#endif
        version_info = other.version_info;
        compiler_version = other.compiler_version;
        debug = other.debug;
        apple = other.apple;
        windows = other.windows;

        return *this;
      }

      std::string dump() const
      {
        std::string retVal("{\"compiler\":\"");
        retVal += compiler_version.data();
        retVal += "\", \"debug\":" + std::to_string(debug);
        retVal += ", \"apple\":" + std::to_string(apple);
        retVal += ", \"windows\":" + std::to_string(windows);

#ifndef ENABLE_STD_ALLOCATOR
        retVal += ", " + std::string( version_info.c_str() );
        retVal += ", " + dump( plugins );
#endif
        retVal += "}";

        return retVal;
      }
#ifndef ENABLE_STD_ALLOCATOR
      template< typename Set >
      std::string dump( const Set& source ) const
      {
        bool first = true;

        std::string res = "\"plugins\" : [";

        for( auto& item : source )
        {
          res += first ? "\"" : ", \"";
          res += std::string( item.c_str() ) + "\"";

          first = false;
        }
        res += "]";

        return res;
      }

      void test_set_plugins( const helpers::environment_extension_resources& environment_extension )
      {
        if( created_storage )
        {
          version_info = environment_extension.version_info.c_str();

          for( auto& item : environment_extension.plugins )
            plugins.insert( shared_string( item.c_str(), version_info.get_allocator() ) );
        }
        else
        {
          bool result = strcmp( version_info.c_str(), environment_extension.version_info.c_str() ) == 0;
          if( !result )
          {
            std::string message = "Persistent storage was created according to the version: " + std::string( version_info.c_str() );
            message += " but current node has the version: " + environment_extension.version_info;

            environment_extension.logger( message );
          }

          result = std::equal( plugins.begin(), plugins.end(), environment_extension.plugins.begin(), environment_extension.plugins.end(), []( const shared_string& s1, const std::string& s2 )
          {
            return strcmp( s1.c_str(), s2.c_str() ) == 0;
          });

          if( !result )
          {
            std::string dump_plugins = dump( plugins );
            std::string dump_current_plugins = dump( environment_extension.plugins );

            std::string message = "Persistent storage was created using plugins: " + dump_plugins;
            message += " but current node has following plugins: " + dump_current_plugins;

            environment_extension.logger( message );
          }
        }
        created_storage = false;
      }

      shared_string                 version_info;
      t_flat_set< shared_string >   plugins;
#endif
      boost::array<char,256>  compiler_version;
      bool                    debug = false;
      bool                    apple = false;
      bool                    windows = false;

      bool                    created_storage = true;
  };

  namespace{
  std::string shared_memory_bin_filename(const std::string& context)
  {
    if (context.empty())
    {
        return "shared_memory.bin";
    }
    else
    {
      return context + "_" + "shared_memory.bin";
    }
  }
  }

  void database::open(const bfs::path& dir, uint32_t flags, size_t shared_file_size, const boost::any& database_cfg, const helpers::environment_extension_resources* environment_extension, const bool wipe_shared_file, const std::string& context)
  {
    assert( dir.is_absolute() );
    bfs::create_directories( dir );
    if( _data_dir != dir ) close();
    if( wipe_shared_file ) wipe( dir, context );

    _data_dir = dir;
    _database_cfg = database_cfg;
#ifndef ENABLE_STD_ALLOCATOR
    auto abs_path = bfs::absolute( dir / shared_memory_bin_filename(context) );
    
    if( bfs::exists( abs_path ) )
    {
        //mtlk TODO is it needed ?
        bfs::permissions(abs_path, bfs::perms::all_all | bfs::perms::add_perms);

      _file_size = bfs::file_size( abs_path );
      if( shared_file_size > _file_size )
      {
        if( !bip::managed_mapped_file::grow( abs_path.generic_string().c_str(), shared_file_size - _file_size ) )
          BOOST_THROW_EXCEPTION( std::runtime_error( "could not grow database file to requested size." ) );

        _file_size = shared_file_size;
      }

      _segment.reset( new bip::managed_mapped_file( bip::open_only,
                                      abs_path.generic_string().c_str()
                                      ) );

      auto env = _segment->find< environment_check >( "environment" );

      if( flags & skip_env_check )
      {
        if( !env.first )
        {
          _segment->construct< environment_check >( "environment" )( allocator< environment_check >( _segment->get_segment_manager() ) );
        }
        else
        {
          *env.first = environment_check( allocator< environment_check >( _segment->get_segment_manager() ) );
          if (environment_extension)
          {
            env.first->version_info = environment_extension->version_info.c_str();
            for( auto& item : environment_extension->plugins )
              env.first->plugins.insert( shared_string( item.c_str(), _segment->get_segment_manager() ) );
          }
        }
      }
      else
      {
        environment_check eCheck( allocator< environment_check >( _segment->get_segment_manager() ) );
        if( !env.first || !( *env.first == eCheck) ) {
          if(!env.first)
            BOOST_THROW_EXCEPTION( std::runtime_error( "Unable to find environment data saved in persistent storage. Probably database created by a different compiler, build, or operating system" ) );

          std::string dp = env.first->dump();
          std::string dr = eCheck.dump();
          BOOST_THROW_EXCEPTION(std::runtime_error("Different persistent & runtime environments. Persistent: `" + dp + "'. Runtime: `"+ dr + "'.Probably database created by a different compiler, build, or operating system"));
        }

        std::cout << "Compiler and build environment read from persistent storage: `" << env.first->dump() << '\'' << std::endl;
      }
    } else {
      _file_size = shared_file_size;
      _segment.reset( new bip::managed_mapped_file( bip::create_only,
                                      abs_path.generic_string().c_str(), shared_file_size
                                      ) );
      _segment->find_or_construct< environment_check >( "environment" )( allocator< environment_check >( _segment->get_segment_manager() ) );

              //mtlk TODO is it needed ?
        bfs::permissions(abs_path, bfs::perms::all_all | bfs::perms::add_perms);

    }

    auto env = _segment->find< environment_check >( "environment" );
    if( environment_extension )
      env.first->test_set_plugins( *environment_extension );

    _flock = bip::file_lock( abs_path.generic_string().c_str() );
    if( !_flock.try_lock() )
      BOOST_THROW_EXCEPTION( std::runtime_error( "could not gain write access to the shared memory file" ) );
#endif

    _is_open = true;
  }

  void database::flush() {
    if( _segment )
      _segment->flush();
    if( _meta )
      _meta->flush();
  }

  void database::close()
  {
    if( _is_open )
    {
      _segment.reset();
      _meta.reset();
      _data_dir = bfs::path();

      wipe_indexes();

      _is_open = false;
    }
  }

  void database::wipe_indexes()
  {
    _index_list.clear();
    _index_map.clear();

    _at_least_one_index_was_created_earlier = false;
    _at_least_one_index_is_created_now      = false;
  }


  void database::wipe( const bfs::path& dir , const std::string& context )
  {
    assert( !_is_open );
    _segment.reset();
    _meta.reset();
    bfs::remove_all( dir / shared_memory_bin_filename(context));
    bfs::remove_all( dir / "shared_memory.meta" );
    _data_dir = bfs::path();

    wipe_indexes();

  }

  void database::resize( size_t new_shared_file_size )
  {
    if( _undo_session_count )
      BOOST_THROW_EXCEPTION( std::runtime_error( "Cannot resize shared memory file while undo session is active" ) );

    _segment.reset();
    _meta.reset();

    open( _data_dir, 0, new_shared_file_size );

    wipe_indexes();

    for( auto& index_type : _index_types )
    {
      index_type->add_index( *this );
    }
  }

  void database::set_require_locking( bool enable_require_locking )
  {
#ifdef CHAINBASE_CHECK_LOCKING
    _enable_require_locking = enable_require_locking;
#endif
  }

#ifdef CHAINBASE_CHECK_LOCKING
  void database::require_lock_fail( const char* method, const char* lock_type, const char* tname )const
  {
    std::string err_msg = "database::" + std::string( method ) + " require_" + std::string( lock_type ) + "_lock() failed on type " + std::string( tname );
    std::cerr << err_msg << std::endl;
    BOOST_THROW_EXCEPTION( std::runtime_error( err_msg ) );
  }
#endif

  void database::undo()
  {
    for( auto& item : _index_list )
    {
      item->undo();
    }
  }

  void database::squash()
  {
    for( auto& item : _index_list )
    {
      item->squash();
    }
  }

  void database::commit( int64_t revision )
  {
    for( auto& item : _index_list )
    {
      item->commit( revision );
    }
  }

  void database::undo_all()
  {
    for( auto& item : _index_list )
    {
      item->undo_all();
    }
  }

  database::session database::start_undo_session()
  {
    vector< std::unique_ptr<abstract_session> > _sub_sessions;
    _sub_sessions.reserve( _index_list.size() );
    for( auto& item : _index_list ) {
      _sub_sessions.push_back( item->start_undo_session() );
    }
    return session( std::move( _sub_sessions ), _undo_session_count );
  }

}  // namespace chainbase


