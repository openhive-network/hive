#pragma once

#include <hive/chain/external_storage/account_metadata_rocksdb_objects.hpp>
#include <hive/chain/external_storage/account_authority_rocksdb_objects.hpp>
#include <hive/chain/external_storage/account_rocksdb_objects.hpp>

#include <hive/chain/external_storage/accounts_handler.hpp>
#include <hive/chain/external_storage/rocksdb_account_storage_provider.hpp>
#include <hive/chain/external_storage/external_storage_snapshot.hpp>

namespace hive { namespace chain {

namespace bfs = boost::filesystem;

class rocksdb_account_archive : public accounts_handler
{
  private:

#ifdef IS_TEST_NET
    const size_t volatile_objects_limit = 0;
#else
    const size_t volatile_objects_limit = 1'000;
#endif

    database& db;

    rocksdb_account_storage_provider::ptr   provider;
    external_storage_snapshot::ptr          snapshot;

    bool destroy_database_on_startup = false;
    bool destroy_database_on_shutdown = false;

    uint32_t get_block_num() const;

    template<typename Volatile_Object_Type, typename RocksDB_Object_Type>
    void move_to_external_storage_impl( uint32_t block_num, const Volatile_Object_Type& volatile_object, ColumnTypes column_type );

    template<typename SHM_Object_Type, typename SHM_Object_Index>
    auto get_allocator() const;

    template<typename SHM_Object_Type, typename SHM_Object_Index>
    std::shared_ptr<SHM_Object_Type> create_from_buffer( const PinnableSlice& buffer ) const;

    template<typename Volatile_Object_Type, typename SHM_Object_Type, typename SHM_Object_Index>
    std::shared_ptr<SHM_Object_Type> create_from_volatile_object( const Volatile_Object_Type& obj ) const;

    template<typename SHM_Object_Type, typename SHM_Object_Index>
    std::shared_ptr<SHM_Object_Type> get_object_impl( const account_name_type& account_name, ColumnTypes column_type ) const;

    template<typename Volatile_Object_Type, typename Volatile_Index_Type, typename Object_Type, typename SHM_Object_Type, typename SHM_Object_Index>
    Object_Type get_object( const account_name_type& account_name, ColumnTypes column_type, bool is_required ) const;

    template<typename Object_Type, typename SHM_Object_Type>
    void modify( Object_Type& obj, std::function<void(SHM_Object_Type&)> modifier );

    template<typename Volatile_Index_Type, typename Volatile_Object_Type, typename SHM_Object_Type, typename RocksDB_Object_Type>
    bool on_irreversible_block_impl( uint32_t block_num, ColumnTypes column_type );

    template<typename Volatile_Index_Type, typename Volatile_Object_Type, typename SHM_Object_Type>
    void create_or_update_volatile_impl( const SHM_Object_Type& obj );

  public:

    rocksdb_account_archive( database& db, const bfs::path& blockchain_storage_path, const bfs::path& storage_path,
      appbase::application& app, bool destroy_on_startup, bool destroy_on_shutdown );
    virtual ~rocksdb_account_archive();

    void on_irreversible_block( uint32_t block_num ) override;

    void create_or_update_volatile( const account_metadata_object& obj ) override;
    account_metadata get_account_metadata( const account_name_type& account_name ) const override;
    void modify_object( const account_name_type& account_name, std::function<void(account_metadata_object&)>&& modifier ) override;

    void create_or_update_volatile( const account_authority_object& obj ) override;
    account_authority get_account_authority( const account_name_type& account_name ) const override;
    void modify_object( const account_name_type& account_name, std::function<void(account_authority_object&)>&& modifier ) override;

    void create_or_update_volatile( const account_object& obj ) override;
    account get_account( const account_name_type& account_name, bool account_is_required ) const override;
    void modify_object( const account_name_type& account_name, std::function<void(account_object&)>&& modifier ) override;

    void save_snapshot( const prepare_snapshot_supplement_notification& note ) override;
    void load_snapshot( const load_snapshot_supplement_notification& note ) override;

    void open() override;
    void close() override;
    void wipe() override;
};

} }
