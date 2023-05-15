#pragma once
#include <hive/chain/hive_fwd.hpp>

#include <hive/protocol/operations.hpp>
#include <hive/protocol/dhf_operations.hpp>

#include <hive/chain/witness_objects.hpp>

#include <hive/protocol/asset.hpp>

#include <fc/exception/exception.hpp>

namespace hive { namespace plugins { namespace condenser_api {

  template< typename T >
  struct convert_to_legacy_static_variant
  {
    convert_to_legacy_static_variant( T& l_sv ) :
      legacy_sv( l_sv ) {}

    T& legacy_sv;

    typedef void result_type;

    template< typename V >
    void operator()( const V& v ) const
    {
      legacy_sv = v;
    }
  };

  using namespace hive::protocol;

  struct legacy_price
  {
    legacy_price() {}
    legacy_price( const protocol::price& p ) :
      base( legacy_asset::from_asset( p.base ) ),
      quote( legacy_asset::from_asset( p.quote ) )
    {}

    operator price()const { return price( base, quote ); }

    legacy_asset base;
    legacy_asset quote;
  };

  struct api_chain_properties
  {
    api_chain_properties() {}
    api_chain_properties( const hive::chain::chain_properties& c ) :
      account_creation_fee( legacy_asset::from_asset( c.account_creation_fee ) ),
      maximum_block_size( c.maximum_block_size ),
      hbd_interest_rate( c.hbd_interest_rate ),
      account_subsidy_budget( c.account_subsidy_budget ),
      account_subsidy_decay( c.account_subsidy_decay )
    {}

    operator legacy_chain_properties() const
    {
      legacy_chain_properties props;
      props.account_creation_fee = legacy_hive_asset::from_asset( asset( account_creation_fee ) );
      props.maximum_block_size = maximum_block_size;
      props.hbd_interest_rate = hbd_interest_rate;
      return props;
    }

    legacy_asset   account_creation_fee;
    uint32_t       maximum_block_size = HIVE_MIN_BLOCK_SIZE_LIMIT * 2;
    uint16_t       hbd_interest_rate = HIVE_DEFAULT_HBD_INTEREST_RATE;
    int32_t        account_subsidy_budget = HIVE_DEFAULT_ACCOUNT_SUBSIDY_BUDGET;
    uint32_t       account_subsidy_decay = HIVE_DEFAULT_ACCOUNT_SUBSIDY_DECAY;
  };

} } } // hive::plugins::condenser_api

FC_REFLECT( hive::plugins::condenser_api::api_chain_properties,
        (account_creation_fee)(maximum_block_size)(hbd_interest_rate)(account_subsidy_budget)(account_subsidy_decay)
        )

FC_REFLECT( hive::plugins::condenser_api::legacy_price, (base)(quote) )
