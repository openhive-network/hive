#pragma once

#include <chainbase/forward_declarations.hpp>
#include <hive/chain/hive_object_types.hpp>
#include <hive/protocol/asset_symbol.hpp>

#ifdef HIVE_ENABLE_SMT

namespace hive { namespace chain {

  class nai_pool_object : public object< nai_pool_object_type, nai_pool_object >
  {
    CHAINBASE_OBJECT( nai_pool_object );

  public:
    CHAINBASE_DEFAULT_CONSTRUCTOR( nai_pool_object )

    uint8_t num_available_nais = 0;
    fc::array< asset_symbol_type, SMT_MAX_NAI_POOL_COUNT > nais;

    std::vector< asset_symbol_type > pool() const
    {
      return std::vector< asset_symbol_type >{ nais.begin(), nais.begin() + num_available_nais };
    }

    bool contains( const asset_symbol_type& a ) const
    {
      const auto end = nais.begin() + num_available_nais;
      return std::find( nais.begin(), end, asset_symbol_type::from_asset_num( a.get_stripped_precision_smt_num() ) ) != end;
    }

    CHAINBASE_UNPACK_CONSTRUCTOR(nai_pool_object);
  };

  typedef multi_index_container <
    nai_pool_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< nai_pool_object, nai_pool_object::id_type, &nai_pool_object::get_id > >
    >,
    allocator< nai_pool_object >
  > nai_pool_index;

} } // namespace hive::chain

FC_REFLECT( hive::chain::nai_pool_object, (id)(num_available_nais)(nais) )

CHAINBASE_SET_INDEX_TYPE( hive::chain::nai_pool_object, hive::chain::nai_pool_index )

#endif
