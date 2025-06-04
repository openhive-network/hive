#include <chainbase/chainbase.hpp>
#include <boost/array.hpp>
#include <boost/any.hpp>
#include <iostream>
#include <fc/log/logger.hpp>
#include <fc/io/json.hpp>

#include <filesystem>

#ifdef __linux__
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#endif

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
                  : version_info( a ), decoded_state_objects_data_json(a), blockchain_config_json(a), plugins( a )
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
      // decoded_state_objects_data_json and blockchain_config_json is generated later, so we don't check it here.
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
        decoded_state_objects_data_json = other.decoded_state_objects_data_json;
        blockchain_config_json = other.blockchain_config_json;
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

      void test_version( const helpers::environment_extension_resources& environment_extension )
      {
        if( created_storage )
        {
          ilog("Saving version into database: ${version_info}", ("version_info", environment_extension.version_info));
          version_info = environment_extension.version_info.c_str();
        }
        else
        {
          const std::string loaded_version_str(version_info.c_str());
          const std::string current_version_str(environment_extension.version_info.c_str());

          if( loaded_version_str !=  current_version_str)
          {
            const fc::variant loaded_version_v = fc::json::from_string(loaded_version_str, fc::json::full);
            const fc::variant current_version_v = fc::json::from_string(current_version_str, fc::json::full);
            assert(loaded_version_v.is_object());
            assert(current_version_v.is_object());
            const fc::variant_object loaded_version = loaded_version_v.get_object()["version"].get_object();
            const fc::variant_object current_version = current_version_v.get_object()["version"].get_object();

            if (loaded_version["node_type"].as_string() != current_version["node_type"].as_string())
              BOOST_THROW_EXCEPTION( std::runtime_error( "Loaded node type: " + loaded_version["node_type"].as_string() + " is different then current node type: " + current_version["node_type"].as_string()));

            else if (loaded_version["blockchain_version"].as_string() != current_version["blockchain_version"].as_string() ||
                     loaded_version["hive_revision"].as_string() != current_version["hive_revision"].as_string() ||
                     loaded_version["fc_revision"].as_string() != current_version["fc_revision"].as_string())
            {
              std::string message = "Persistent storage was created according to the version: " + loaded_version_str;
              message += " but current node has the version: " + current_version_str;
              environment_extension.logger( message );
            }

            else
            {
              wlog("Other differences (specific to plugin configuration) found between loaded version data: ${lv} and current version data: ${cv}.", ("lv", loaded_version_str)("cv", current_version_str));
            }
          }
        }
      }

      bool test_set_plugins(const helpers::environment_extension_resources& environment_extension )
      {
        if( created_storage )
        {
          std::string list_of_plugins;
          for( auto& item : environment_extension.plugins )
          {
            plugins.insert( shared_string( item.c_str(), version_info.get_allocator()));
            list_of_plugins += item + " ";
          }
          ilog("Saving list of plugins into database: ${list_of_plugins}", (list_of_plugins));
        }
        else
        {
          std::set<std::string> current_plugins_editable = environment_extension.plugins;
          bool less_current_plugins_than_plugins_in_db = false;

          for (const auto& plugin : plugins)
          {
            const std::string plugin_name(plugin.c_str());
            if (current_plugins_editable.count(plugin_name))
              current_plugins_editable.erase(plugin_name);
            else
              less_current_plugins_than_plugins_in_db = true;
          }

          if (!current_plugins_editable.empty())
          {
            const std::string loaded_plugins = dump(plugins);
            const std::string all_current_plugins = dump(environment_extension.plugins);
            const std::string missing_plugins = dump(current_plugins_editable);

            const std::string error_message = "Database misses plugins: " + missing_plugins + " which are requested"
                                              ".\n Enabled plugins: " + all_current_plugins +
                                              "\n Plugins in database: " + loaded_plugins;

            environment_extension.logger(error_message);
            return true;
          }

          else if (less_current_plugins_than_plugins_in_db)
          {
            const std::string loaded_plugins = dump(plugins);
            const std::string all_current_plugins = dump(environment_extension.plugins);
            const std::string warning_message = "Not all plugins from database were requested to load."
                                                "\n Enabled plugins: " + all_current_plugins +
                                                "\n Plugins in database: " + loaded_plugins;

            environment_extension.logger(warning_message);
            return false;
          }
        }
        return true;
      }

      shared_string                 version_info;
      shared_string                 decoded_state_objects_data_json;
      shared_string                 blockchain_config_json;
      t_flat_set< shared_string >   plugins;
#endif
      boost::array<char,256>  compiler_version;
      bool                    debug = false;
      bool                    apple = false;
      bool                    windows = false;

      bool                    created_storage = true;
  };

  void database::open( const bfs::path& dir, uint32_t flags, size_t shared_file_size, const boost::any& database_cfg, const helpers::environment_extension_resources* environment_extension, const bool wipe_shared_file )
  {
    assert( dir.is_absolute() );
    bfs::create_directories( dir );
    if( _data_dir != dir ) close();
    if( wipe_shared_file ) wipe( dir );

    _data_dir = dir;
    _database_cfg = database_cfg;
#ifndef ENABLE_STD_ALLOCATOR
    auto abs_path = bfs::absolute( dir / "shared_memory.bin" );

    auto _size_checker = [&dir]( size_t size )
    {
      std::filesystem::space_info _space_info = std::filesystem::space( dir.generic_string().c_str() );
      ilog( "Free space available: ${available}. Shared file size: ${size}",("available", _space_info.available)(size) );
      if( size > _space_info.available )
        BOOST_THROW_EXCEPTION( std::runtime_error( "Unable to create shared memory file. Free space available is less than declared size of shared memory file." ) );
    };

    if( bfs::exists( abs_path ) )
    {
      _file_size = bfs::file_size( abs_path );
      if( shared_file_size > _file_size )
      {
        _size_checker( shared_file_size - _file_size );
        if( !bip::managed_mapped_file::grow( abs_path.generic_string().c_str(), shared_file_size - _file_size ) )
          BOOST_THROW_EXCEPTION( std::runtime_error( "could not grow database file to requested size." ) );

        _file_size = shared_file_size;
      }

      _segment.reset( new bip::managed_mapped_file( bip::open_only,
                                      abs_path.generic_string().c_str()
                                      ) );

#ifdef __linux__
      // Advise the kernel to use huge pages for this memory mapping if available
      // MADV_HUGEPAGE tells the kernel to back the memory mapping with huge pages
      // which can significantly improve performance for large memory segments by
      // reducing TLB (Translation Lookaside Buffer) misses
      void* addr = _segment->get_address();
      size_t size = _segment->get_size();
      if (addr != nullptr && size > 0) {
        if (madvise(addr, size, MADV_HUGEPAGE) != 0) {
          wlog("madvise for huge pages failed: ${error}", ("error", strerror(errno)));
        } else {
          ilog("Successfully advised kernel to use huge pages for shared memory");
        }
      }
#endif

      auto env = _segment->find< environment_check >( "environment" );
      environment_check eCheck( allocator< environment_check >( _segment->get_segment_manager() ) );
      if( !env.first || !( *env.first == eCheck) ) {
        if(!env.first)
          BOOST_THROW_EXCEPTION( std::runtime_error( "Unable to find environment data saved in persistent storage. Probably database created by a different compiler, build, or operating system" ) );

        std::string dp = env.first->dump();
        std::string dr = eCheck.dump();
        BOOST_THROW_EXCEPTION(std::runtime_error("Different persistent & runtime environments. Persistent: `" + dp + "'. Runtime: `"+ dr + "'.Probably database created by a different compiler, build, or operating system"));
      }

      if (env.first->created_storage)
        env.first->created_storage = false;

      ilog( "Compiler and build environment read from persistent storage: `${storage}'", ( "storage", env.first->dump() ) );
    } else {
      _size_checker( shared_file_size );
      _file_size = shared_file_size;
      _segment.reset( new bip::managed_mapped_file( bip::create_only,
                                      abs_path.generic_string().c_str(), shared_file_size
                                      ) );

#ifdef __linux__
      // Advise the kernel to use huge pages for this memory mapping if available
      // MADV_HUGEPAGE tells the kernel to back the memory mapping with huge pages
      // which can significantly improve performance for large memory segments by
      // reducing TLB (Translation Lookaside Buffer) misses
      void* addr = _segment->get_address();
      size_t size = _segment->get_size();
      if (addr != nullptr && size > 0) {
        if (madvise(addr, size, MADV_HUGEPAGE) != 0) {
          wlog("madvise for huge pages failed: ${error}", ("error", strerror(errno)));
        } else {
          ilog("Successfully advised kernel to use huge pages for shared memory");
        }
      }
#endif

      _segment->find_or_construct< environment_check >( "environment" )( allocator< environment_check >( _segment->get_segment_manager() ) );
      ilog( "Creating storage at ${abs_path}', size: ${shared_file_size}", ( "abs_path",abs_path.generic_string() )(shared_file_size) );
    }

    auto env = _segment->find< environment_check >( "environment" );
    if( environment_extension )
    {
      env.first->test_version(*environment_extension);
      env.first->test_set_plugins(*environment_extension);
    }


    _flock = bip::file_lock( abs_path.generic_string().c_str() );
    if( !_flock.try_lock() )
      BOOST_THROW_EXCEPTION( std::runtime_error( "could not gain write access to the shared memory file" ) );
#endif

    _is_open = true;
  }

  void database::flush() {
    ilog( "Flushing database" );
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

  void database::wipe( const bfs::path& dir )
  {
    assert( !_is_open );
    _segment.reset();
    _meta.reset();
    const bfs::path shared_memory_bin_path(dir / "shared_memory.bin");
    const bfs::path shared_memory_meta_path(dir / "shared_memory.meta");
    bfs::remove_all( shared_memory_bin_path );
    bfs::remove_all( shared_memory_meta_path );
    std::string log("Requested to clear shared memory data. Removed files:\n- " + shared_memory_bin_path.string() + "\n- " + shared_memory_meta_path.string() + "\n");
    ilog( log );
    _data_dir = bfs::path();
    wipe_indexes();
  }

  void database::resize( size_t new_shared_file_size )
  {
    ilog( "Resizing shared memory file to ${size}", ( "size", new_shared_file_size ) );
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
    elog(err_msg);
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

  void database::set_decoded_state_objects_data(const std::string& json)
  {
    assert(_is_open);
    environment_check* const env = _segment->find< environment_check >( "environment" ).first;
    assert(env);

    if (!env->created_storage)
      BOOST_THROW_EXCEPTION( std::runtime_error( "Cannot set decoded state objects data if storage is not newly created." ) );

    env->decoded_state_objects_data_json = json.c_str();
  }

  std::string database::get_decoded_state_objects_data_from_shm() const
  {
    assert(_is_open);
    const environment_check* const env = _segment->find< environment_check >( "environment" ).first;
    assert(env);
    return std::string(env->decoded_state_objects_data_json.c_str());
  }

  void database::set_blockchain_config(const std::string& json)
  {
    /* Blockchain config can change for example via hardfork*/
    assert(_is_open);
    environment_check* const env = _segment->find< environment_check >( "environment" ).first;
    assert(env);
    ilog("Updating blockchain configuration stored in DB to: \n${json}", (json));
    env->blockchain_config_json = json.c_str();
  }

  std::string database::get_blockchain_config_from_shm() const
  {
    assert(_is_open);
    const environment_check* const env = _segment->find< environment_check >( "environment" ).first;
    assert(env);
    return std::string(env->blockchain_config_json.c_str());
  }

  std::string database:: get_plugins_from_shm() const
  {
    assert(_is_open);
    const environment_check* const env = _segment->find< environment_check >( "environment" ).first;
    assert(env);
    std::vector<std::string> db_plugins;
    db_plugins.reserve(env->plugins.size());

    for (const auto& p : env->plugins)
      db_plugins.push_back(std::string(p));

    fc::variant db_plugins_v;
    fc::to_variant(db_plugins, db_plugins_v);
    return std::string(fc::json::to_string(db_plugins_v));
  }

  std::string database::get_environment_details() const
  {
    assert(_is_open);
    const environment_check* const env = _segment->find< environment_check >( "environment" ).first;
    return env->dump();
  }
}  // namespace chainbase


