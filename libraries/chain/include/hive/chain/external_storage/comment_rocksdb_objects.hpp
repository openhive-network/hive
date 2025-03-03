#pragma once

#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/comment_object.hpp>

#include <hive/chain/util/type_registrar_definition.hpp>

#ifndef HIVE_COMMENT_ROCKSDB_SPACE_ID
#define HIVE_COMMENT_ROCKSDB_SPACE_ID 20
#endif

namespace hive { namespace chain {

enum comment_rocksdb_object_types
{
  volatile_comment_object_type = ( HIVE_COMMENT_ROCKSDB_SPACE_ID << 8 )
};

class volatile_comment_object : public object< volatile_comment_object_type, volatile_comment_object >
{
  CHAINBASE_OBJECT( volatile_comment_object );

  public:

    CHAINBASE_DEFAULT_CONSTRUCTOR( volatile_comment_object )

    const comment_object::author_and_permlink_hash_type& get_author_and_permlink_hash() const { return author_and_permlink_hash; }
    void set_author_and_permlink_hash( const comment_object::author_and_permlink_hash_type& val ) { author_and_permlink_hash = val; }

    comment_id_type                               comment_id;
    comment_id_type                               parent_comment;
    uint16_t                                      depth = 0;

    uint32_t                                      block_number = 0;
    bool                                          was_paid = false;

  private:

    comment_object::author_and_permlink_hash_type author_and_permlink_hash;
};

typedef oid_ref< volatile_comment_object > volatile_comment_id_type;

struct by_block;
struct by_permlink;

typedef multi_index_container<
    volatile_comment_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< volatile_comment_object, volatile_comment_object::id_type, &volatile_comment_object::get_id > >,
      ordered_unique< tag< by_block >,
        composite_key< volatile_comment_object,
          member< volatile_comment_object, uint32_t, &volatile_comment_object::block_number>,
          const_mem_fun< volatile_comment_object, volatile_comment_object::id_type, &volatile_comment_object::get_id >
        >
      >,
      ordered_unique< tag< by_permlink >,
        const_mem_fun< volatile_comment_object, const comment_object::author_and_permlink_hash_type&, &volatile_comment_object::get_author_and_permlink_hash > >
    >,
    allocator< volatile_comment_object >
  > volatile_comment_index;

class rocksdb_comment_object
{
  public:

    rocksdb_comment_object(){}

    rocksdb_comment_object( const volatile_comment_object& obj )
    {
      id                        = obj.get_id();

      comment_id                = obj.comment_id;
      parent_comment            = obj.parent_comment;
      depth                     = obj.depth;
    }

    uint64_t    id = 0;

    uint32_t    comment_id;
    uint32_t    parent_comment;
    uint16_t    depth = 0;
};

} } // hive::chain


FC_REFLECT( hive::chain::volatile_comment_object, (id)(comment_id)(parent_comment)(author_and_permlink_hash)(depth)(block_number)(was_paid) )
CHAINBASE_SET_INDEX_TYPE( hive::chain::volatile_comment_object, hive::chain::volatile_comment_index )

FC_REFLECT( hive::chain::rocksdb_comment_object, (id)(comment_id)(parent_comment)(depth) )