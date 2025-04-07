#pragma once

#include <boost/filesystem.hpp>

#include <chainbase/state_snapshot_support.hpp>

#include <hive/plugins/chain/state_snapshot_provider.hpp>

#include <fc/filesystem.hpp>
#include <fc/reflect/reflect.hpp>

#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <tuple>

namespace hive
{
  namespace chain
  {
    class full_block_type;
    class database;
  }
}

namespace bfs = boost::filesystem;

namespace hive
{
  namespace plugins
  {
    namespace state_snapshot
    {

      struct plugin_external_data_info
      {
        fc::path path;
      };

      struct index_manifest_file_info
      {
        /// Path relative against snapshot main-directory.
        std::string relative_path;
        uint64_t file_size;
      };

      struct index_manifest_info
      {
        std::string name;
        /// Number of source index items dumped into snapshot.
        size_t dumpedItems = 0;
        size_t firstId = 0;
        size_t lastId = 0;
        /** \warning indexNextId must be then explicitly loaded to generic_index::next_id to conform proposal ID rules. next_id can be different (higher)
            than last_object_id + 1 due to removing proposals, which does not correct next_id
        */
        size_t indexNextId = 0;
        std::vector<index_manifest_file_info> storage_files;
      };

      struct index_manifest_info_less
      {
        bool operator()(const index_manifest_info &i1, const index_manifest_info &i2) const
        {
          return i1.name < i2.name;
        }
      };

      using plugin_external_data_index = std::map<std::string, plugin_external_data_info>;
      using snapshot_manifest = std::set<index_manifest_info, index_manifest_info_less>;
      using manifest_data = std::tuple<snapshot_manifest, plugin_external_data_index, std::string, std::string, std::string, uint32_t>;

      class snapshot_dump_supplement_helper final : public hive::plugins::chain::snapshot_dump_helper
      {
      public:
        const plugin_external_data_index &get_external_data_index() const
        {
          return external_data_index;
        }

        virtual void store_external_data_info(const hive::plugins::chain::abstract_plugin &plugin, const fc::path &storage_path) override;

      private:
        plugin_external_data_index external_data_index;
      };

      class snapshot_load_supplement_helper final : public hive::plugins::chain::snapshot_load_helper
      {
      public:
        explicit snapshot_load_supplement_helper(const plugin_external_data_index &idx) : ext_data_idx(idx) {}

        virtual bool load_external_data_info(const hive::plugins::chain::abstract_plugin &plugin, fc::path *storage_path) override;

      private:
        const plugin_external_data_index &ext_data_idx;
      };

      manifest_data load_snapshot_manifest(const bfs::path &actualStoragePath, std::shared_ptr<hive::chain::full_block_type> &lib, hive::chain::database &db);

      class index_dump_writer;
      void store_snapshot_manifest(const bfs::path &actualStoragePath, const std::vector<std::unique_ptr<index_dump_writer>> &builtWriters, const snapshot_dump_supplement_helper &dumpHelper, const hive::chain::database &db);
      fc::variant snapshot_manifest_to_variant(const manifest_data& manifest);

    }
  }
} // hive::plugins::state_snapshot

FC_REFLECT(hive::plugins::state_snapshot::index_manifest_info, (name)(dumpedItems)(firstId)(lastId)(indexNextId)(storage_files))
FC_REFLECT(hive::plugins::state_snapshot::index_manifest_file_info, (relative_path)(file_size))