#pragma once

#include <hive/chain/external_storage/account_metadata_rocksdb_objects.hpp>
#include <hive/chain/external_storage/account_authority_rocksdb_objects.hpp>

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
    const size_t volatile_objects_limit = 5;
#endif

    database& db;

    rocksdb_account_storage_provider::ptr   provider;
    external_storage_snapshot::ptr          snapshot;

    bool destroy_database_on_startup = false;
    bool destroy_database_on_shutdown = false;

    template<typename Volatile_Object_Type, typename RocksDB_Object_Type>
    void move_to_external_storage_impl( uint32_t block_num, const Volatile_Object_Type& volatile_object, ColumnTypes column_type );

    template<typename SHM_Object_Type, typename SHM_Object_Index>
    auto get_allocator() const;

    template<typename SHM_Object_Type, typename SHM_Object_Index>
    std::shared_ptr<SHM_Object_Type> create( const PinnableSlice& buffer ) const;

    template<typename SHM_Object_Type, typename SHM_Object_Index>
    std::shared_ptr<SHM_Object_Type> get_object_impl( const std::string& account_name, ColumnTypes column_type ) const;

    template<typename Object_Type, typename SHM_Object_Type, typename SHM_Object_Index>
    Object_Type get_object( const std::string& account_name, ColumnTypes column_type ) const;

    template<typename Volatile_Index_Type, typename Volatile_Object_Type, typename SHM_Object_Type, typename RocksDB_Object_Type>
    void on_irreversible_block_impl( uint32_t block_num, ColumnTypes column_type );

  public:

    rocksdb_account_archive( database& db, const bfs::path& blockchain_storage_path, const bfs::path& storage_path,
      appbase::application& app, bool destroy_on_startup, bool destroy_on_shutdown );
    virtual ~rocksdb_account_archive();

    void on_irreversible_block( uint32_t block_num ) override;

    void create_volatile_account_metadata( uint32_t block_num, const account_metadata_object& obj ) override;
    account_metadata get_account_metadata( const std::string& account_name ) const override;

    void create_volatile_account_authority( uint32_t block_num, const account_authority_object& obj ) override;
    account_authority get_account_authority( const std::string& account_name ) const override;

    void save_snaphot( const prepare_snapshot_supplement_notification& note ) override;
    void load_snapshot( const load_snapshot_supplement_notification& note ) override;

    void open() override;
    void close() override;
    void wipe() override;
};

} }
