#pragma once

#include <hive/chain/external_storage/accounts_handler.hpp>
#include <hive/chain/external_storage/rocksdb_account_storage_provider.hpp>
#include <hive/chain/external_storage/external_storage_snapshot.hpp>

#include <hive/chain/database.hpp>

namespace hive { namespace chain {

namespace bfs = boost::filesystem;

class rocksdb_account_archive : public accounts_handler
{
  protected:

    const chainbase::database& get_db() const override;
    external_storage_reader_writer::ptr get_external_storage_reader_writer() const override;

  private:

#ifdef IS_TEST_NET
    size_t objects_limit = 0;
#else
    size_t objects_limit = 100'000;
#endif

    size_t compaction_frequency_counter  = 0;
    size_t compaction_frequency          = 1'200;//about once per hour (assuming 3s blocks)

    uint32_t retention_blocks = 0; //default 0 blocks

    database& db;

    rocksdb_account_storage_provider::ptr   provider;
    external_storage_snapshot::ptr          snapshot;

    template<typename Key_Type, typename SHM_Object_Type, typename SHM_Object_Sub_Index>
    const SHM_Object_Type* get_object( const Key_Type& key, const std::vector<ColumnTypes>& column_types, bool is_required ) const;

    template<typename SHM_Index_Type, typename SHM_Object_Type,
            typename RocksDB_Object_Type, typename RocksDB_Object_Type2>
    bool on_irreversible_block_impl( uint32_t block_num, const std::vector<ColumnTypes>& column_types );

  public:

    rocksdb_account_archive( database& db, const bfs::path& blockchain_storage_path, const bfs::path& storage_path, uint32_t retention_blocks,
      appbase::application& app );
    virtual ~rocksdb_account_archive();

    void on_irreversible_block( uint32_t block_num ) override;

    void create_object( const account_metadata_object& obj ) override;
    const account_metadata_object* get_account_metadata( const account_name_type& account_name, bool account_metadata_is_required ) const override;
    void modify_object( const account_metadata_object& obj, std::function<void(account_metadata_object&)>&& modifier ) override;

    void create_object( const account_authority_object& obj ) override;
    const account_authority_object* get_account_authority( const account_name_type& account_name, bool account_authority_is_required ) const override;
    void modify_object( const account_authority_object& obj, std::function<void(account_authority_object&)>&& modifier ) override;

    void create_object( const account_object& obj ) override;
    const account_object* get_account( const account_name_type& account_name, bool account_is_required ) const override;
    const account_object* get_account( const account_id_type& account_id, bool account_is_required ) const override;
    void modify_object( const account_object& obj, std::function<void(account_object&)>&& modifier ) override;

    void save_snapshot( const prepare_snapshot_supplement_notification& note ) override;
    void load_snapshot( const load_snapshot_supplement_notification& note ) override;

    void open() override;
    void close() override;
    void wipe() override;

    void remove_objects_limit() override;
};

} }
