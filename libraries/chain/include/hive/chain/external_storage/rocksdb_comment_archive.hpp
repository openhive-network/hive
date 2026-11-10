#pragma once

#include <hive/chain/external_storage/comments_handler.hpp>
#include <hive/chain/external_storage/comment_archive_prefetch.hpp>
#include <hive/chain/external_storage/comment_archive_prefetcher.hpp>
#include <hive/chain/external_storage/rocksdb_comment_storage_provider.hpp>
#include <hive/chain/external_storage/comment_rocksdb_objects.hpp>
#include <hive/chain/external_storage/external_storage_snapshot.hpp>

#include <shared_mutex>

namespace hive { namespace chain {

namespace bfs = boost::filesystem;

class rocksdb_comment_archive : public comments_handler, public comment_archive_prefetcher
{
  private:

    size_t volatile_objects_limit = 10'000;

    database& db;

    rocksdb_comment_storage_provider::ptr   provider;
    external_storage_snapshot::ptr          snapshot;

    const bool                              prefetch_enabled;
    /// Held shared by prefetching workers and exclusively while the storage is opened, closed or replaced.
    std::shared_mutex                       storage_mutex;
    mutable prefetched_comment_cache        prefetched_comments;
    mutable author_id_cache                 author_ids;

    void move_to_external_storage_impl( uint32_t block_num, const volatile_comment_object& volatile_object );
    std::shared_ptr<comment_object> get_comment_impl( const comment_object::author_and_permlink_hash_type& hash ) const;
    std::shared_ptr<comment_object> get_prefetched_comment( const comment_object::author_and_permlink_hash_type& hash ) const;
    void remember_author( const account_id_type& author ) const;
    void clear_prefetched();

  public:

    rocksdb_comment_archive( database& db, const bfs::path& blockchain_storage_path, const bfs::path& storage_path, appbase::application& app,
      size_t block_cache_size = rocksdb_comment_storage_provider::DEFAULT_BLOCK_CACHE_SIZE, bool enable_prefetch = false,
      const rocksdb_trace_config& trace_config = rocksdb_trace_config() );
    virtual ~rocksdb_comment_archive();

    void on_cashout( uint32_t _block_num, const comment_object& _comment, const comment_cashout_object& _comment_cashout ) override;
    void on_irreversible_block( uint32_t block_num ) override;

    comment get_comment( const account_id_type& author, const std::string& permlink, bool comment_is_required ) const override;

    void save_snapshot( const prepare_snapshot_supplement_notification& note ) override;
    void load_snapshot( const load_snapshot_supplement_notification& note ) override;

    void open() override;
    void close() override;
    void flush( bool force_storage_flush ) override;
    void wipe() override;

    void on_end_of_syncing() override;
    void on_segment_remapped() override{ /*nothing to do here*/};

    /// Reads archived comments the transactions look up into memory, where get_comment finds them
    /// after the state misses. Does nothing unless the prefetch was enabled at construction.
    void prefetch_comments( const std::vector<full_transaction_ptr>& transactions ) override;
};

} }
