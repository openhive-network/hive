#pragma once

#include <hive/chain/external_storage/account_metadata_rocksdb_objects.hpp>
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

    void move_to_external_storage_impl( uint32_t block_num, const volatile_account_metadata_object& volatile_object );
    std::shared_ptr<account_metadata_object> get_account_metadata_impl( const std::string& account_name ) const;

  public:

    rocksdb_account_archive( database& db, const bfs::path& blockchain_storage_path, const bfs::path& storage_path,
      appbase::application& app, bool destroy_on_startup, bool destroy_on_shutdown );
    virtual ~rocksdb_account_archive();

    void on_irreversible_block( uint32_t block_num ) override;

    void create_volatile_account_metadata( uint32_t block_num, const account_metadata_object& obj ) override;
    account_metadata get_account_metadata( const std::string& account_name ) const override;

    void save_snaphot( const prepare_snapshot_supplement_notification& note ) override;
    void load_snapshot( const load_snapshot_supplement_notification& note ) override;

    void open() override;
    void close() override;
    void wipe() override;
};

} }
