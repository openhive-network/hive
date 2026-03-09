#pragma once

#include <hive/chain/external_storage/accounts_handler.hpp>
#include <hive/chain/external_storage/rocksdb_account_storage_provider.hpp>
#include <hive/chain/external_storage/external_storage_snapshot.hpp>

#include <hive/chain/database.hpp>

namespace hive { namespace chain {

namespace bfs = boost::filesystem;

class rocksdb_account_archive : public accounts_handler
{
  private:

    // The limit is removed (set to 0) when sync completes via remove_objects_limit().
    // During replay, set_replay_objects_limit() raises this to replay_objects_limit
    // to reduce unnecessary archival churn while still bounding SHM usage.
    #ifdef IS_TEST_NET
        size_t objects_limit = 0;
    #else
        size_t objects_limit = 100'000;
        static constexpr size_t replay_objects_limit = 500'000;
    #endif

    size_t compaction_frequency_counter  = 0;
    size_t compaction_frequency          = 1'200;//about once per hour (assuming 3s blocks)

    uint32_t retention_blocks = 0; //default 0 blocks

    database& db;

    rocksdb_account_storage_provider::ptr   provider;
    external_storage_snapshot::ptr          snapshot;

    template<typename Key_Type, typename SHM_Object_Type, typename SHM_Object_Sub_Index, typename Return_Type>
    Return_Type get_object( const Key_Type& key, const std::vector<ColumnTypes>& column_types, bool is_required ) const;

    template<typename SHM_Index_Type, typename SHM_Object_Type,
            typename RocksDB_Object_Type, typename RocksDB_Object_Type2>
    bool on_irreversible_block_impl( uint32_t block_num, const std::vector<ColumnTypes>& column_types );

    // Specialized implementation for account_metadata_object (needs account name lookup)
    bool on_irreversible_block_impl_metadata( uint32_t block_num, const std::vector<ColumnTypes>& column_types );

    // Specialized implementation for account_object (skips accounts with pending governance votes)
    bool on_irreversible_block_impl_account( uint32_t block_num, const std::vector<ColumnTypes>& column_types );

    /// Maximum number of objects archived per irreversible block per index scan.
    /// Prevents I/O spikes when many accounts become archival-eligible simultaneously.
    static constexpr uint32_t max_archives_per_block = 10'000;

    /// The earliest block number at which any account could possibly need archival.
    /// When block_num < this value, the entire archival scan is skipped.
    uint32_t _next_archival_check_block = 0;

    /// True while replay is active; used to log the replay→live transition.
    bool _was_replaying = false;

    /// Computes the next block at which archival should be checked, based on the
    /// minimum last_access_block across all by_block indices.
    uint32_t compute_next_archival_check() const;

  public:

    rocksdb_account_archive( database& db, const bfs::path& blockchain_storage_path, const bfs::path& storage_path, uint32_t retention_blocks,
      appbase::application& app );
    virtual ~rocksdb_account_archive();

    void on_irreversible_block( uint32_t block_num ) override;

    void create_object( const account_metadata_object& obj ) override;
    const account_metadata_object* get_account_metadata( const account_name_type& account_name, bool account_metadata_is_required ) const override;
    void on_object_modified( const account_metadata_object& obj ) override;

    void create_object( const account_authority_object& obj ) override;
    const account_authority_object* get_account_authority( const account_name_type& account_name, bool account_authority_is_required ) const override;
    void on_object_modified( const account_authority_object& obj ) override;

    void create_object( const account_object& obj ) override;
    const account_object* get_account( const account_name_type& account_name, bool account_is_required ) const override;
    const account_object* get_account( const account_id_type& account_id, bool account_is_required ) const override;
    void on_object_modified( const account_object& obj ) override;

    const account_details_object* get_account_details( const account_id_type& account_id, bool is_required ) const override;

    account_metadata get_volatile_account_metadata( const account_name_type& account_name, bool account_metadata_is_required ) const override;
    account_authority get_volatile_account_authority( const account_name_type& account_name, bool account_authority_is_required ) const override;
    account get_volatile_account( const account_name_type& account_name, bool account_is_required ) const override;
    account get_volatile_account( const account_id_type& account_id, bool account_is_required ) const override;

    void save_snapshot( const prepare_snapshot_supplement_notification& note ) override;
    void load_snapshot( const load_snapshot_supplement_notification& note ) override;

    void open() override;
    void close() override;
    void wipe() override;

    void remove_objects_limit() override;
    void set_replay_objects_limit() override;
};

} }
