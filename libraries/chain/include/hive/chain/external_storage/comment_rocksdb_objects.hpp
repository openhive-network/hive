#pragma once

#include <cstring>
#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/comment_object.hpp>

#include <hive/chain/util/type_registrar_definition.hpp>

#ifndef HIVE_COMMENT_ROCKSDB_SPACE_ID
#define HIVE_COMMENT_ROCKSDB_SPACE_ID 21
#endif

namespace hive { namespace chain {

enum comment_rocksdb_object_types
{
  volatile_comment_object_type = ( HIVE_COMMENT_ROCKSDB_SPACE_ID << 8 )
};

struct extended_hash_creator
{
  static std::string get_extended_hash( const account_id_type& author_id, const comment_object::author_and_permlink_hash_type& hash )
  {
    std::string _result = std::to_string( author_id.get_value() );
    auto _first_element_size = _result.size();

    _result.resize( _first_element_size + hash.data_size() );
    std::memcpy( _result.data() + _first_element_size, hash.data(), hash.data_size() );

    return _result;
  }
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

    account_id_type                               author_id;
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
    multi_index_allocator< volatile_comment_object >
  > volatile_comment_index;

class rocksdb_comment_object
{
  public:

    rocksdb_comment_object(){}

    rocksdb_comment_object( const volatile_comment_object& obj )
    {
      comment_id                = obj.comment_id;
      parent_comment            = obj.parent_comment;
      depth                     = obj.depth;
    }

    comment_id_type comment_id;
    comment_id_type parent_comment;
    uint16_t        depth = 0;
};

} } // hive::chain


FC_REFLECT( hive::chain::volatile_comment_object, (id)(comment_id)(parent_comment)(depth)(block_number)(author_id)(author_and_permlink_hash) )
CHAINBASE_SET_INDEX_TYPE( hive::chain::volatile_comment_object, hive::chain::volatile_comment_index )

FC_REFLECT( hive::chain::rocksdb_comment_object, (comment_id)(parent_comment)(depth) )
