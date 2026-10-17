#pragma once

#include <hive/chain/hive_object_types.hpp>

namespace hive { namespace chain {

/** Permanent grant ID high-water mark for one grantor. */
class scoped_authority_grantor_metadata_object : public object<
  scoped_authority_grantor_metadata_object_type,
  scoped_authority_grantor_metadata_object,
  std::true_type >
{
  CHAINBASE_OBJECT( scoped_authority_grantor_metadata_object );
public:
  template< typename Allocator >
  scoped_authority_grantor_metadata_object( allocator< Allocator >, uint64_t _id,
    const account_name_type& _grantor, uint32_t _last_grant_id )
    : id( _id ), grantor( _grantor ), last_grant_id( _last_grant_id )
  {}

  account_name_type grantor;
  uint32_t last_grant_id = 0;

  size_t get_dynamic_alloc() const { return 0; }

  CHAINBASE_UNPACK_CONSTRUCTOR( scoped_authority_grantor_metadata_object );
};

} } // hive::chain

FC_REFLECT( hive::chain::scoped_authority_grantor_metadata_object, (id)(grantor)(last_grant_id) )
