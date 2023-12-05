#pragma once
#include <hive/chain/hive_object_types.hpp>

#include <hive/chain/full_block.hpp>

namespace hive { namespace chain {

using chainbase::t_vector;
using hive::chain::detail::block_attributes_t;
using hive::protocol::block_id_type;

/**
  *  @brief allows storing (limited) number of full blocks data in state
  *  @ingroup object
  *
  *  Used by e.g. file-less block reader/writer
  */
class full_block_object : public object< full_block_object_type, full_block_object >
{
  CHAINBASE_OBJECT( full_block_object );
  public:
    using t_block_bytes = t_vector< char >;

    template< typename Allocator >
    full_block_object( allocator< Allocator > a, uint64_t _id )
    : id( _id ), block_bytes( a )
    {}

    block_attributes_t  compression_attributes;
    size_t              byte_size = 0;
    t_block_bytes       block_bytes;
    block_id_type       block_id;
  
  CHAINBASE_UNPACK_CONSTRUCTOR(full_block_object, (block_bytes));
};

struct by_hash;
typedef multi_index_container<
  full_block_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< full_block_object, full_block_object::id_type, &full_block_object::get_id > >/*,
    ordered_unique< tag< by_hash >,
      const_mem_fun< full_block_object, block_id_type, &full_block_object::get_hash > >*/
  >,
  allocator< full_block_object >
> full_block_index;

} } // hive::chain

FC_REFLECT( hive::chain::full_block_object, (id)(compression_attributes)(byte_size)(block_bytes)(block_id) )
CHAINBASE_SET_INDEX_TYPE( hive::chain::full_block_object, hive::chain::full_block_index )

namespace helpers
{
  template <>
  class index_statistic_provider<hive::chain::full_block_index>
  {
  public:
    typedef hive::chain::full_block_index IndexType;
    typedef typename hive::chain::full_block_object::t_block_bytes t_block_bytes;

    index_statistic_info gather_statistics(const IndexType& index, bool onlyStaticInfo) const
    {
      index_statistic_info info;
      gather_index_static_data(index, &info);

      if(onlyStaticInfo == false)
      {
        for(const auto& o : index)
        {
          info._item_additional_allocation += o.block_bytes.capacity()*sizeof(t_block_bytes::value_type);
        }
      }

      return info;
    }
  };

} /// namespace helpers