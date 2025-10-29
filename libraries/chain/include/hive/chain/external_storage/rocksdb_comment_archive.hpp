#pragma once

#include <hive/chain/external_storage/comments_handler.hpp>
#include <hive/chain/external_storage/rocksdb_comment_storage_provider.hpp>
#include <hive/chain/external_storage/comment_rocksdb_objects.hpp>
#include <hive/chain/external_storage/external_storage_snapshot.hpp>

namespace hive { namespace chain {

namespace bfs = boost::filesystem;

class rocksdb_comment_archive : public comments_handler
{
  private:

#ifdef IS_TEST_NET
    const size_t volatile_objects_limit = 0;
#else
    const size_t volatile_objects_limit = 10'000;
#endif

    database& db;

    rocksdb_comment_storage_provider::ptr   provider;
    external_storage_snapshot::ptr          snapshot;

    void move_to_external_storage_impl( uint32_t block_num, const volatile_comment_object& volatile_object );
    std::shared_ptr<comment_object> get_comment_impl( const comment_object::author_and_permlink_hash_type& hash ) const;

  public:

    rocksdb_comment_archive( database& db, const bfs::path& blockchain_storage_path, const bfs::path& storage_path, appbase::application& app );
    virtual ~rocksdb_comment_archive();

    void on_cashout( uint32_t _block_num, const comment_object& _comment, const comment_cashout_object& _comment_cashout ) override;
    void on_irreversible_block( uint32_t block_num ) override;

    comment get_comment( const account_id_type& author, const std::string& permlink, bool comment_is_required ) const override;

    void save_snapshot( const prepare_snapshot_supplement_notification& note ) override;
    void load_snapshot( const load_snapshot_supplement_notification& note ) override;

    void open() override;
    void close() override;
    void flush() override;
    void wipe() override;
};

} }
