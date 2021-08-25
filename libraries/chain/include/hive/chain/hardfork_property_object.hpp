#pragma once
#include <chainbase/hive_fwd.hpp>

#include <hive/protocol/version.hpp>

#include <hive/chain/hive_object_types.hpp>

#include <hive/chain/util/hf23_helper.hpp>

namespace hive { namespace chain {

  using chainbase::t_vector;
  using chainbase::t_flat_map;

  class hardfork_property_object : public object< hardfork_property_object_type, hardfork_property_object >
  {
    CHAINBASE_OBJECT( hardfork_property_object );
    public:
      CHAINBASE_DEFAULT_CONSTRUCTOR( hardfork_property_object, (processed_hardforks)(h23_balances) )

      using t_processed_hardforks = t_vector< fc::time_point_sec >;
      using t_hf23_items          = t_flat_map< account_name_type, hf23_item >;

      t_processed_hardforks       processed_hardforks;
      uint32_t                    last_hardfork = 0;
      protocol::hardfork_version  current_hardfork_version;
      protocol::hardfork_version  next_hardfork;
      fc::time_point_sec          next_hardfork_time;

      //Here are saved balances for given accounts, that were cleared during HF23
      t_hf23_items                h23_balances;

    CHAINBASE_UNPACK_CONSTRUCTOR(hardfork_property_object, (processed_hardforks)(h23_balances));
  };

  typedef multi_index_container<
    hardfork_property_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< hardfork_property_object, hardfork_property_object::id_type, &hardfork_property_object::get_id > >
    >,
    allocator< hardfork_property_object >
  > hardfork_property_index;

} } // hive::chain

FC_REFLECT_TYPENAME( hive::chain::hardfork_property_object::t_hf23_items)

FC_REFLECT( hive::chain::hardfork_property_object,
  (id)(processed_hardforks)(last_hardfork)(current_hardfork_version)
  (next_hardfork)(next_hardfork_time)(h23_balances) )
CHAINBASE_SET_INDEX_TYPE( hive::chain::hardfork_property_object, hive::chain::hardfork_property_index )
