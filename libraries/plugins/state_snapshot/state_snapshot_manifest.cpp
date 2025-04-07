#include <hive/plugins/state_snapshot/state_snapshot_manifest.hpp>
#include <hive/plugins/state_snapshot/state_snapshot_tools.hpp>

#include <chainbase/state_snapshot_support.hpp>
#include <hive/chain/database.hpp>

#include <appbase/application.hpp>

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/slice.h>

#define SNAPSHOT_FORMAT_VERSION "2.3"

using ::rocksdb::Slice;

namespace hive
{
  namespace plugins
  {
    namespace state_snapshot
    {

      manifest_data load_snapshot_manifest(const bfs::path &actualStoragePath, std::shared_ptr<hive::chain::full_block_type> &lib, hive::chain::database &db)
      {
        bfs::path manifestDbPath(actualStoragePath);
        manifestDbPath /= "snapshot-manifest";

        ::rocksdb::Options dbOptions;
        dbOptions.create_if_missing = false;
        dbOptions.max_open_files = 1024;

        ::rocksdb::ColumnFamilyDescriptor cfDescriptor;
        cfDescriptor.name = "INDEX_MANIFEST";

        std::vector<::rocksdb::ColumnFamilyDescriptor> cfDescriptors;
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

        cfDescriptor = ::rocksdb::ColumnFamilyDescriptor();
        cfDescriptor.name = "BLOCKCHAIN_CONFIGURATION";
        cfDescriptors.push_back(cfDescriptor);

        cfDescriptor = ::rocksdb::ColumnFamilyDescriptor();
        cfDescriptor.name = "PLUGINS";
        cfDescriptors.push_back(cfDescriptor);

        std::vector<::rocksdb::ColumnFamilyHandle *> cfHandles;
        std::unique_ptr<::rocksdb::DB> manifestDbPtr;
        ::rocksdb::DB *manifestDb = nullptr;
        auto status = ::rocksdb::DB::OpenForReadOnly(dbOptions, manifestDbPath.string(), cfDescriptors, &cfHandles, &manifestDb);
        manifestDbPtr.reset(manifestDb);
        if (status.ok())
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
          for (indexIterator->SeekToFirst(); indexIterator->Valid(); indexIterator->Next())
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
          for (indexIterator->SeekToFirst(); indexIterator->Valid(); indexIterator->Next())
          {
            auto keySlice = indexIterator->key();
            auto valueSlice = indexIterator->value();

            buffer.insert(buffer.end(), valueSlice.data(), valueSlice.data() + valueSlice.size());

            std::string plugin_name = keySlice.data();
            std::string relative_path = {buffer.begin(), buffer.end()};

            buffer.clear();

            ilog("Loaded external data info for plugin ${p} having storage of external files inside: `${d}' (relative path)", ("p", plugin_name)("d", relative_path));

            bfs::path extDataPath(actualStoragePath);
            extDataPath /= relative_path;

            if (bfs::exists(extDataPath) == false)
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

        uint32_t libn = 0;

        {
          ::rocksdb::ReadOptions rOptions;

          std::unique_ptr<::rocksdb::Iterator> irreversibleStateIterator(manifestDb->NewIterator(rOptions, cfHandles[3]));
          irreversibleStateIterator->SeekToFirst();
          FC_ASSERT(irreversibleStateIterator->Valid(), "No entry for IRREVERSIBLE_STATE. Probably used old snapshot format (must be regenerated).");

          std::vector<char> buffer;
          Slice keySlice = irreversibleStateIterator->key();
          auto valueSlice = irreversibleStateIterator->value();

          std::string keyName = keySlice.ToString();
          ;

          FC_ASSERT(keyName == "LAST_IRREVERSIBLE_BLOCK", "Broken snapshot - no entry for LAST_IRREVERSIBLE_BLOCK");

          buffer.insert(buffer.end(), valueSlice.data(), valueSlice.data() + valueSlice.size());
          hive::chain::irreversible_block_data_type ibd{chainbase::allocator<hive::chain::irreversible_block_data_type>(db.get_segment_manager())};
          chainbase::serialization::unpack_from_buffer(ibd, buffer);
          buffer.clear();
          // Store block num for further use.
          libn = ibd._irreversible_block_num;
          // Store full block data in form of block - which is immune to database wipe.
          lib = ibd._irreversible_block_data.create_full_block();
          // ilog("Loaded last irreversible block num ${num}", ("num", libn));
          // ilog("Loaded last irreversible block data ${data}", ("data", ibd._irreversible_block_data));

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

        std::string blockchain_configuration;

        {
          ::rocksdb::ReadOptions rOptions;

          std::unique_ptr<::rocksdb::Iterator> blockchainConfigurationDataIterator(manifestDb->NewIterator(rOptions, cfHandles[5]));
          blockchainConfigurationDataIterator->SeekToFirst();
          FC_ASSERT(blockchainConfigurationDataIterator->Valid(), "No entry for BLOCKCHAIN_CONFIGURATION. Probably used old snapshot format (must be regenerated).");

          Slice keySlice = blockchainConfigurationDataIterator->key();
          auto valueSlice = blockchainConfigurationDataIterator->value();

          std::string keyName = keySlice.ToString();
          FC_ASSERT(keyName == "BLOCKCHAIN_CONFIGURATION", "Broken snapshot - no entry for BLOCKCHAIN_CONFIGURATION");
          blockchain_configuration = valueSlice.ToString();
        }

        std::string plugins;
        {
          ::rocksdb::ReadOptions rOptions;

          std::unique_ptr<::rocksdb::Iterator> pluginsDataIterator(manifestDb->NewIterator(rOptions, cfHandles[6]));
          pluginsDataIterator->SeekToFirst();
          FC_ASSERT(pluginsDataIterator->Valid(), "No entry for PLUGINS. Probably used old snapshot format (must be regenerated).");

          Slice keySlice = pluginsDataIterator->key();
          auto valueSlice = pluginsDataIterator->value();

          std::string keyName = keySlice.ToString();
          FC_ASSERT(keyName == "PLUGINS", "Broken snapshot - no entry for PLUGINS");
          plugins = valueSlice.ToString();
        }

        for (auto *cfh : cfHandles)
        {
          status = manifestDb->DestroyColumnFamilyHandle(cfh);
          if (status.ok() == false)
          {
            elog("Cannot destroy column family handle...'. Error details: `${e}'.", ("e", status.ToString()));
          }
        }

        manifestDb->Close();
        manifestDbPtr.release();

        return std::make_tuple(retVal, extDataIdx, std::move(state_definitions_data), std::move(blockchain_configuration), std::move(plugins), libn);
      }

      class rocksdb_cleanup_helper
      {
      public:
        static rocksdb_cleanup_helper open(const ::rocksdb::Options &opts, const bfs::path &path);
        void close();

        ::rocksdb::ColumnFamilyHandle *create_column_family(const std::string &name, const ::rocksdb::ColumnFamilyOptions *cfOptions = nullptr);

        rocksdb_cleanup_helper(rocksdb_cleanup_helper &&) = default;
        rocksdb_cleanup_helper &operator=(rocksdb_cleanup_helper &&) = default;

        rocksdb_cleanup_helper(const rocksdb_cleanup_helper &) = delete;
        rocksdb_cleanup_helper &operator=(const rocksdb_cleanup_helper &) = delete;

        ~rocksdb_cleanup_helper()
        {
          cleanup();
        }

        ::rocksdb::DB *operator->() const
        {
          return _db.get();
        }

      private:
        bool cleanup()
        {
          if (_db)
          {
            for (auto *ch : _column_handles)
            {
              auto s = _db->DestroyColumnFamilyHandle(ch);
              if (s.ok() == false)
              {
                elog("Cannot destroy column family handle. Error: `${e}'", ("e", s.ToString()));
                return false;
              }
            }

            auto s = _db->Close();
            if (s.ok() == false)
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

        typedef std::vector<::rocksdb::ColumnFamilyHandle *> column_handles;
        std::unique_ptr<::rocksdb::DB> _db;
        column_handles _column_handles;
      };

      rocksdb_cleanup_helper rocksdb_cleanup_helper::open(const ::rocksdb::Options &opts, const bfs::path &path)
      {
        rocksdb_cleanup_helper retVal;

        ::rocksdb::DB *db = nullptr;
        auto status = ::rocksdb::DB::Open(opts, path.string(), &db);
        retVal._db.reset(db);
        if (status.ok())
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
        if (cleanup() == false)
          throw std::exception();
      }

      ::rocksdb::ColumnFamilyHandle *rocksdb_cleanup_helper::create_column_family(const std::string &name, const ::rocksdb::ColumnFamilyOptions *cfOptions /* = nullptr*/)
      {
        ::rocksdb::ColumnFamilyOptions actualOptions;

        if (cfOptions != nullptr)
          actualOptions = *cfOptions;

        ::rocksdb::ColumnFamilyHandle *cf = nullptr;
        auto status = _db->CreateColumnFamily(actualOptions, name, &cf);
        if (status.ok() == false)
        {
          elog("Cannot create column family. Error details: `${e}'.", ("e", status.ToString()));
          throw std::exception();
        }

        _column_handles.emplace_back(cf);

        return cf;
      }

      void store_snapshot_manifest(const bfs::path &actualStoragePath, const std::vector<std::unique_ptr<index_dump_writer>> &builtWriters, const snapshot_dump_supplement_helper &dumpHelper, const hive::chain::database &db)
      {
        bfs::path manifestDbPath(actualStoragePath);
        manifestDbPath /= "snapshot-manifest";

        if (bfs::exists(manifestDbPath) == false)
          bfs::create_directories(manifestDbPath);

        ::rocksdb::Options dbOptions;
        dbOptions.create_if_missing = true;
        dbOptions.max_open_files = 1024;

        rocksdb_cleanup_helper rocksdb = rocksdb_cleanup_helper::open(dbOptions, manifestDbPath);
        ::rocksdb::ColumnFamilyHandle *manifestCF = rocksdb.create_column_family("INDEX_MANIFEST");
        ::rocksdb::ColumnFamilyHandle *externalDataCF = rocksdb.create_column_family("EXTERNAL_DATA");
        ::rocksdb::ColumnFamilyHandle *snapshotManifestCF = rocksdb.create_column_family("IRREVERSIBLE_STATE");
        ::rocksdb::ColumnFamilyHandle *StateDefinitionsDataCF = rocksdb.create_column_family("STATE_DEFINITIONS_DATA");
        ::rocksdb::ColumnFamilyHandle *BlockchainConfigurationCF = rocksdb.create_column_family("BLOCKCHAIN_CONFIGURATION");
        ::rocksdb::ColumnFamilyHandle *PluginsConfigurationCF = rocksdb.create_column_family("PLUGINS");

        ::rocksdb::WriteOptions writeOptions;

        std::vector<std::pair<index_manifest_info, std::vector<char>>> infoCache;

        for (const auto &w : builtWriters)
        {
          infoCache.emplace_back(index_manifest_info(), std::vector<char>());
          index_manifest_info &info = infoCache.back().first;
          w->store_index_manifest(&info);

          std::vector<char> &storage = infoCache.back().second;

          chainbase::serialization::pack_to_buffer(storage, info);
          Slice key(info.name);
          Slice value(storage.data(), storage.size());

          auto status = rocksdb->Put(writeOptions, manifestCF, key, value);
          if (status.ok() == false)
          {
            elog("Cannot write an index manifest entry to output file: `${p}'. Error details: `${e}'.", ("p", manifestDbPath.string())("e", status.ToString()));
            ilog("Failing key value: ${k}", ("k", info.name));

            throw std::exception();
          }
        }

        const auto &extDataIdx = dumpHelper.get_external_data_index();

        for (const auto &d : extDataIdx)
        {
          const auto &plugin_name = d.first;
          const auto &path = d.second.path;

          auto relativePath = bfs::relative(path, actualStoragePath);
          auto relativePathStr = relativePath.string();

          Slice key(plugin_name);
          Slice value(relativePathStr);

          auto status = rocksdb->Put(writeOptions, externalDataCF, key, value);
          if (status.ok() == false)
          {
            elog("Cannot write an index manifest entry to output file: `${p}'. Error details: `${e}'.", ("p", manifestDbPath.string())("e", status.ToString()));
            ilog("Failing key value: ${k}", ("k", plugin_name));

            throw std::exception();
          }
        }

        {
          Slice key("LAST_IRREVERSIBLE_BLOCK");

          const hive::chain::irreversible_block_data_type *ibd = db.get_last_irreversible_object();
          // ilog("Dumping last irreversible block num ${num}", ("num", ibd->_irreversible_block_num));
          // ilog("Dumping last irreversible block data ${data}", ("data", ibd->_irreversible_block_data));
          std::vector<char> storage;
          chainbase::serialization::pack_to_buffer(storage, *ibd);

          Slice value(storage.data(), storage.size());
          auto status = rocksdb->Put(writeOptions, snapshotManifestCF, key, value);

          if (status.ok() == false)
          {
            elog("Cannot write an index manifest entry to output file: `${p}'. Error details: `${e}'.", ("p", manifestDbPath.string())("e", status.ToString()));
            ilog("Failing key value: \"LAST_IRREVERSIBLE_BLOCK\"");

            throw std::exception();
          }
        }

        {
          Slice key("SNAPSHOT_VERSION");
          Slice value(SNAPSHOT_FORMAT_VERSION);
          auto status = rocksdb->Put(writeOptions, snapshotManifestCF, key, value);

          if (status.ok() == false)
          {
            elog("Cannot write an index manifest entry to output file: `${p}'. Error details: `${e}'.", ("p", manifestDbPath.string())("e", status.ToString()));
            ilog("Failing key value: \"SNAPSHOT_VERSION\"");

            throw std::exception();
          }
        }

        {
          const std::string json = db.get_decoded_state_objects_data_from_shm();
          Slice key("STATE_DEFINITIONS_DATA");
          Slice value(json);
          auto status = rocksdb->Put(writeOptions, StateDefinitionsDataCF, key, value);

          if (status.ok() == false)
          {
            elog("Cannot write an index manifest entry to output file: `${p}'. Error details: `${e}'.", ("p", manifestDbPath.string())("e", status.ToString()));
            ilog("Failing key value: \"STATE_DEFINITIONS_DATA\"");

            throw std::exception();
          }
        }

        {
          const std::string json = db.get_blockchain_config_from_shm();
          Slice key("BLOCKCHAIN_CONFIGURATION");
          Slice value(json);
          auto status = rocksdb->Put(writeOptions, BlockchainConfigurationCF, key, value);

          if (status.ok() == false)
          {
            elog("Cannot write an index manifest entry to output file: `${p}'. Error details: `${e}'.", ("p", manifestDbPath.string())("e", status.ToString()));
            ilog("Failing key value: \"BLOCKCHAIN_CONFIGURATION\"");

            throw std::exception();
          }
        }

        {
          const std::string json = db.get_plugins_from_shm();
          Slice key("PLUGINS");
          Slice value(json);
          auto status = rocksdb->Put(writeOptions, PluginsConfigurationCF, key, value);

          if (status.ok() == false)
          {
            elog("Cannot write an index manifest entry to output file: `${p}'. Error details: `${e}'.", ("p", manifestDbPath.string())("e", status.ToString()));
            ilog("Failing key value: \"PLUGINS\"");

            throw std::exception();
          }
        }

        rocksdb.close();
      }

      void snapshot_dump_supplement_helper::store_external_data_info(const hive::plugins::chain::abstract_plugin &plugin, const fc::path &storage_path)
      {
        plugin_external_data_info info;
        info.path = storage_path;
        auto ii = external_data_index.emplace(plugin.get_name(), info);
        FC_ASSERT(ii.second, "Only one external data path allowed per plugin");
      }

      bool snapshot_load_supplement_helper::load_external_data_info(const hive::plugins::chain::abstract_plugin &plugin, fc::path *storage_path)
      {
        const std::string &name = plugin.get_name();

        auto i = ext_data_idx.find(name);

        if (i == ext_data_idx.end())
        {
          *storage_path = fc::path();
          return false;
        }

        *storage_path = i->second.path;
        return true;
      }

      fc::variant snapshot_manifest_to_variant(const manifest_data &manifest)
      {
        fc::variant_object_builder builder;
        {
          std::vector<std::string> index_manifest_jsons;
          const auto &index_manifest = std::get<0>(manifest);
          index_manifest_jsons.reserve(index_manifest.size());
          for (const auto &el : std::get<0>(manifest))
            index_manifest_jsons.push_back(fc::json::to_pretty_string(el));

          builder("index_manifests", index_manifest_jsons);
        }
        {
          const auto external_data_index = std::get<1>(manifest);
          if (external_data_index.empty())
            builder("external_data", "null");
          else
          {
            std::vector<std::pair<std::string, std::string>> external_data;
            external_data.reserve(external_data_index.size());
            for (const auto &el : external_data_index)
              external_data.push_back(std::make_pair(el.first, el.second.path.generic_string()));

            builder("external_data", external_data);
          }
        }
        builder("state_definitions", fc::json::from_string(std::get<2>(manifest), fc::json::format_validation_mode::full));
        builder("blockchain_configuration", fc::json::from_string(std::get<3>(manifest), fc::json::format_validation_mode::full));
        builder("plugins", std::get<4>(manifest));
        builder("irreversible_state_number", std::get<5>(manifest));
        return builder.get();
      }

    }
  }
} // hive::plugins::state_snapshot