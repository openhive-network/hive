#pragma once

#include <hive/chain/external_storage/comments_handler.hpp>

#include <chainbase/allocator_helpers.hpp>

#include <boost/multi_index/hashed_index.hpp>
#include <boost/functional/hash.hpp>
#include <boost/interprocess/allocators/adaptive_pool.hpp>

#include <map>

namespace hive { namespace chain {

class database;
using boost::multi_index::hashed_unique;

class memory_comment_archive final : public comments_handler
{
  public:
    /*
    struct compare_comments
    {
      using is_transparent = void;
      bool operator() ( const comment_object& c1, const comment_object& c2 ) const
      {
        return c1.get_author_and_permlink_hash() < c2.get_author_and_permlink_hash();
      }
      bool operator() ( const comment_object& c1, const comment_object::author_and_permlink_hash_type& k2 ) const
      {
        return c1.get_author_and_permlink_hash() < k2;
      }
      bool operator() ( const comment_object::author_and_permlink_hash_type& k1, const comment_object& c2 ) const
      {
        return k1 < c2.get_author_and_permlink_hash();
      }
    };
    typedef chainbase::t_set<
      comment_object,
      compare_comments,
      chainbase::multi_index_allocator<comment_object>
    > archived_comment_index;
    */
    typedef multi_index_container<
      comment_object,
      indexed_by<
        hashed_unique< tag< by_permlink >,
          //ABW: note - hashed index is the fastest, but it has a very nasty flaw - it takes a lot of time (minutes) and space to
          //rehash once in a while, so it cannot be used in live sync, especially not on witness nodes
        const_mem_fun< comment_object, const comment_object::author_and_permlink_hash_type&, &comment_object::get_author_and_permlink_hash > >
      >,
      boost::interprocess::adaptive_pool< comment_object, boost::interprocess::managed_mapped_file::segment_manager, 128*1024 >
        //ABW: note - pool_allocator_t can't be used with hashed index as it does not support allocation of multiple objects at once
    > archived_comment_index;

    struct archive_object_type
    {
      template< typename Allocator >
      archive_object_type( chainbase::allocator< Allocator > a )
        : comments( archived_comment_index::allocator_type( a.get_segment_manager() ) ) {}

      uint32_t last_irreversible_block_num = 0;
      archived_comment_index comments;
    };

  private:
    database& db;
    // data stored in SHM segment
    archive_object_type* archive = nullptr;
    /*
    This is memory only structure. When we start, it is empty. It only contains data on comments from blocks
    that are still reversible. Because of that, even if it was stored in storage under undo session, it would
    became empty on restart (due to undo_all).
    Note: it is not enough to just keep id from last processed cashout, even though they are processed in
    order of id, because once in many years the id wraps.
    */
    std::map<uint32_t, std::vector< comment_id_type >> comments_in_blocks;

  public:
    memory_comment_archive( database& db ) : db( db ) {}
    virtual ~memory_comment_archive() {}

    virtual void on_cashout( uint32_t _block_num, const comment_object& _comment, const comment_cashout_object& _comment_cashout ) override;
    virtual void on_irreversible_block( uint32_t block_num ) override;

    virtual comment get_comment( const account_id_type& author, const std::string& permlink, bool comment_is_required ) const override;

    virtual void save_snaphot( const prepare_snapshot_supplement_notification& note ) override;
    virtual void load_snapshot( const load_snapshot_supplement_notification& note ) override;

    virtual void open() override;
    virtual void close() override;
    virtual void wipe() override;
};

} }

FC_REFLECT( hive::chain::memory_comment_archive::archive_object_type,
          (last_irreversible_block_num)(comments) )
