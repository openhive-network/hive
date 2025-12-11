#pragma once
#include <hive/chain/detail/state/comment_object.hpp>

namespace hive { namespace chain {

  struct by_comment_voter {};
  struct by_voter_comment;
  typedef multi_index_container<
    comment_vote_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< comment_vote_object, comment_vote_object::id_type, &comment_vote_object::get_id > >,
      ordered_unique< tag< by_comment_voter >,
        composite_key< comment_vote_object,
          const_mem_fun< comment_vote_object, comment_id_type, &comment_vote_object::get_comment>,
          const_mem_fun< comment_vote_object, account_id_type, &comment_vote_object::get_voter>
        >
      >,
      ordered_unique< tag< by_voter_comment >,
        composite_key< comment_vote_object,
          const_mem_fun< comment_vote_object, account_id_type, &comment_vote_object::get_voter>,
          const_mem_fun< comment_vote_object, comment_id_type, &comment_vote_object::get_comment>
        >
      >
    >,
    multi_index_allocator< comment_vote_object >
  > comment_vote_index;


  struct by_permlink {}; /// author, perm

  /**
    * @ingroup object_index
    */
  typedef multi_index_container<
    comment_object,
    indexed_by<
      /// CONSENSUS INDICES - used by evaluators
      ordered_unique< tag< by_id >,
        const_mem_fun< comment_object, comment_object::id_type, &comment_object::get_id > >,
      ordered_unique< tag< by_permlink >, /// used by consensus to find posts referenced in ops
        const_mem_fun< comment_object, const comment_object::author_and_permlink_hash_type&, &comment_object::get_author_and_permlink_hash > >
    >,
    multi_index_allocator< comment_object >
  > comment_index;

  struct by_cashout_time; /// cashout_time

  typedef multi_index_container<
    comment_cashout_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< comment_cashout_object, comment_cashout_object::id_type, &comment_cashout_object::get_id > >,
      ordered_unique< tag< by_cashout_time >,
        composite_key< comment_cashout_object,
          const_mem_fun< comment_cashout_object, time_point_sec, &comment_cashout_object::get_cashout_time>,
          const_mem_fun< comment_cashout_object, comment_cashout_object::id_type, &comment_cashout_object::get_id >
        >
      >
    >,
    multi_index_allocator< comment_cashout_object >
  > comment_cashout_index;

  struct by_root;

  /// This is empty after HF19
  typedef multi_index_container<
    comment_cashout_ex_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< comment_cashout_ex_object, comment_cashout_ex_object::id_type, &comment_cashout_ex_object::get_id > >,
      ordered_unique< tag< by_root >,
        composite_key< comment_cashout_ex_object,
          const_mem_fun< comment_cashout_ex_object, comment_id_type, &comment_cashout_ex_object::get_root_id >,
          const_mem_fun< comment_cashout_ex_object, comment_cashout_ex_object::id_type, &comment_cashout_ex_object::get_id >
        >
      >
    >,
    multi_index_allocator< comment_cashout_ex_object >
  > comment_cashout_ex_index;

} } // hive::chain

CHAINBASE_SET_INDEX_TYPE( hive::chain::comment_object, hive::chain::comment_index )
CHAINBASE_SET_INDEX_TYPE( hive::chain::comment_cashout_object, hive::chain::comment_cashout_index )
CHAINBASE_SET_INDEX_TYPE( hive::chain::comment_cashout_ex_object, hive::chain::comment_cashout_ex_index )
CHAINBASE_SET_INDEX_TYPE( hive::chain::comment_vote_object, hive::chain::comment_vote_index )
