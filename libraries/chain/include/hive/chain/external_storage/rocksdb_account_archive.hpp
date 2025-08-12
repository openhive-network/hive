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
    rocksdb_account_column_family_iterator_provider::ptr get_rocksdb_account_column_family_iterator_provider() override;

  private:

#ifdef IS_TEST_NET
    const size_t volatile_objects_limit = 0;
#else
    const size_t volatile_objects_limit = 1'000;
#endif

    chainbase::database& db;

    rocksdb_account_storage_provider::ptr   provider;
    external_storage_snapshot::ptr          snapshot;

    bool destroy_database_on_startup = false;
    bool destroy_database_on_shutdown = false;

    uint32_t get_block_num() const;

    template<typename Key_Type, typename Volatile_Object_Type, typename Volatile_Index_Type, typename Volatile_Sub_Index_Type, typename Object_Type, typename SHM_Object_Type, typename SHM_Object_Index, typename SHM_Object_Sub_Index>
    Object_Type get_object( const Key_Type& key, const std::vector<ColumnTypes>& column_types, bool is_required ) const;

    template<typename SHM_Object_Type>
    void modify( const SHM_Object_Type& obj, std::function<void(SHM_Object_Type&)> modifier );

    template<typename Volatile_Index_Type, typename Volatile_Object_Type, typename SHM_Object_Type, typename RocksDB_Object_Type, typename RocksDB_Object_Type2, typename RocksDB_Object_Type3>
    bool on_irreversible_block_impl( uint32_t block_num, const std::vector<ColumnTypes>& column_types );

    template<typename Volatile_Index_Type, typename Volatile_Object_Type, typename SHM_Object_Type>
    void create_or_update_volatile_impl( const SHM_Object_Type& obj );

  public:

    rocksdb_account_archive( database& db, const bfs::path& blockchain_storage_path, const bfs::path& storage_path,
      appbase::application& app, bool destroy_on_startup, bool destroy_on_shutdown );
    virtual ~rocksdb_account_archive();

    void on_irreversible_block( uint32_t block_num ) override;

    void create_or_update_volatile( const account_metadata_object& obj ) override;
    account_metadata get_account_metadata( const account_name_type& account_name ) const override;
    void modify_object( const account_metadata_object& obj, std::function<void(account_metadata_object&)>&& modifier ) override;

    void create_or_update_volatile( const account_authority_object& obj ) override;
    account_authority get_account_authority( const account_name_type& account_name ) const override;
    void modify_object( const account_authority_object& obj, std::function<void(account_authority_object&)>&& modifier ) override;

    void create_or_update_volatile( const account_object& obj ) override;
    account get_account( const account_name_type& account_name, bool account_is_required ) const override;
    account get_account( const account_id_type& account_id, bool account_is_required ) const override;
    void modify_object( const account_object& obj, std::function<void(account_object&)>&& modifier ) override;

    void save_snapshot( const prepare_snapshot_supplement_notification& note ) override;
    void load_snapshot( const load_snapshot_supplement_notification& note ) override;

    void open() override;
    void close() override;
    void wipe() override;
};

} }
