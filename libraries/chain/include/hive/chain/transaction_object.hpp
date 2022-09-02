#pragma once
#include <hive/chain/hive_fwd.hpp>

#include <hive/protocol/transaction.hpp>

#include <hive/chain/buffer_type.hpp>
#include <hive/chain/hive_object_types.hpp>

//#include <boost/multi_index/hashed_index.hpp>

namespace hive { namespace chain {

  using hive::protocol::signed_transaction;
  using chainbase::t_vector;

  /**
    * The purpose of this object is to enable the detection of duplicate transactions. When a transaction is included
    * in a block a transaction_object is added. At the end of block processing all transaction_objects that have
    * expired can be removed from the index.
    */
  class transaction_object : public object< transaction_object_type, transaction_object >
  {
    CHAINBASE_OBJECT( transaction_object );
    public:
      CHAINBASE_DEFAULT_CONSTRUCTOR( transaction_object )

      transaction_id_type  trx_id;
      time_point_sec       expiration;
  };

  struct by_expiration;
  struct by_trx_id;
  typedef multi_index_container<
    transaction_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< transaction_object, transaction_object::id_type, &transaction_object::get_id > >,
      ordered_unique< tag< by_trx_id >,
        member< transaction_object, transaction_id_type, &transaction_object::trx_id > >,
      ordered_unique< tag< by_expiration >,
        composite_key< transaction_object,
          member<transaction_object, time_point_sec, &transaction_object::expiration >,
          const_mem_fun<transaction_object, transaction_object::id_type, &transaction_object::get_id >
        >
      >
    >,
    allocator< transaction_object >
  > transaction_index;

} } // hive::chain

FC_REFLECT( hive::chain::transaction_object, (id)(trx_id)(expiration) )
CHAINBASE_SET_INDEX_TYPE( hive::chain::transaction_object, hive::chain::transaction_index )

namespace helpers
{
  template <>
  class index_statistic_provider<hive::chain::transaction_index>
  {
  public:
    typedef hive::chain::transaction_index IndexType;

    index_statistic_info gather_statistics(const IndexType& index, bool onlyStaticInfo) const
    {
      index_statistic_info info;
      gather_index_static_data(index, &info);
      return info;
    }
  };

} /// namespace helpers
