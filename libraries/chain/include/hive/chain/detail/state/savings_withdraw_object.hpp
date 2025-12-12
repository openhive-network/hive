#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <hive/protocol/asset.hpp>
#include <hive/chain/hive_object_types.hpp>

namespace hive { namespace chain {

  using hive::protocol::asset;

  class savings_withdraw_object : public object< savings_withdraw_object_type, savings_withdraw_object, std::true_type >
  {
    CHAINBASE_OBJECT( savings_withdraw_object );
    public:
      template< typename Allocator >
      savings_withdraw_object( allocator< Allocator > a, uint64_t _id,
        const account_name_type& _from, const account_name_type& _to, const asset& _amount,
        const string& _memo, const time_point_sec& _time_of_completion, uint32_t _request_id )
        : id( _id ), from( _from ), to( _to ), memo( a ), request_id( _request_id ),
        amount( _amount ), complete( _time_of_completion )
      {
        from_string( memo, _memo );
      }

      //amount of savings to withdraw (HIVE or HBD)
      const asset& get_withdraw_amount() const { return amount; }

      account_name_type from; //< TODO: can be replaced with account_id_type
      account_name_type to; //< TODO: can be replaced with account_id_type
      shared_string     memo;
      uint32_t          request_id = 0;
      asset             amount; //can be expressed in HIVE or HBD
      time_point_sec    complete;

      size_t get_dynamic_alloc() const
      {
        size_t size = 0;
        size += memo.capacity() * sizeof( decltype( memo )::value_type );
        return size;
      }

    CHAINBASE_UNPACK_CONSTRUCTOR(savings_withdraw_object, (memo));
  };

} } // hive::chain

FC_REFLECT( hive::chain::savings_withdraw_object,
          (id)(from)(to)(memo)(request_id)(amount)(complete) )
