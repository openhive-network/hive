#pragma once
#include <hive/chain/hive_object_types.hpp>

namespace hive { namespace chain {

  using hive::protocol::block_id_type;

  /**
    *  @brief tracks minimal information about past blocks to implement TaPOS
    *  @ingroup object
    *
    *  When attempting to calculate the validity of a transaction we need to
    *  lookup a past block and check its block hash and the time it occurred
    *  so we can calculate whether the current transaction is valid and at
    *  what time it should expire.
    */
  class block_summary_object : public object< block_summary_object_type, block_summary_object >
  {
    CHAINBASE_OBJECT( block_summary_object );
    public:
      CHAINBASE_DEFAULT_CONSTRUCTOR( block_summary_object )

      block_id_type  block_id;
  };

} } // hive::chain

FC_REFLECT( hive::chain::block_summary_object, (id)(block_id) )
