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

  typedef static_variant<
        protocol::comment_payout_beneficiaries
  #ifdef HIVE_ENABLE_SMT
        ,protocol::allowed_vote_assets
  #endif
      > legacy_comment_options_extensions;

  typedef vector< legacy_comment_options_extensions > legacy_comment_options_extensions_type;

  typedef static_variant<
        protocol::pow2,
        protocol::equihash_pow
      > legacy_pow2_work;

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

namespace fc {

struct from_old_static_variant
{
  variant& var;
  from_old_static_variant( variant& dv ):var(dv){}

  typedef void result_type;
  template<typename T> void operator()( const T& v )const
  {
    to_variant( v, var );
  }
};

struct to_old_static_variant
{
  const variant& var;
  to_old_static_variant( const variant& dv ):var(dv){}

  typedef void result_type;
  template<typename T> void operator()( T& v )const
  {
    from_variant( var, v );
  }
};

template< typename T >
void old_sv_to_variant( const T& sv, fc::variant& v )
{
  variant tmp;
  variants vars(2);
  vars[0] = sv.which();
  sv.visit( from_old_static_variant(vars[1]) );
  v = std::move(vars);
}

template< typename T >
void old_sv_from_variant( const fc::variant& v, T& sv )
{
  auto ar = v.get_array();
  if( ar.size() < 2 ) return;
  sv.set_which( static_cast< int64_t >( ar[0].as_uint64() ) );
  sv.visit( to_old_static_variant(ar[1]) );
}

template< typename T >
void new_sv_from_variant( const fc::variant& v, T& sv )
{
  FC_ASSERT( v.is_object(), "Input data have to treated as object." );
  auto v_object = v.get_object();

  FC_ASSERT( v_object.contains( "type" ), "Type field doesn't exist." );
  FC_ASSERT( v_object.contains( "value" ), "Value field doesn't exist." );

  auto ar = v.get_object();
  sv.set_which( static_cast< int64_t >( ar["type"].as_uint64() ) );
  sv.visit( to_old_static_variant(ar["value"]) );
}

// allows detection of pre-rebranding calls and special error reaction
// note: that and related code will be removed some time after HF24
//       since all nodes and libraries should be updated by then
struct obsolete_call_detector
{
  static bool enable_obsolete_call_detection;

  static void report_error()
  {
    FC_ASSERT( false, "Obsolete form of transaction detected, update your wallet." );
  }
};

}

#define FC_REFLECT_ALIAS_HANDLER( r, name, elem )                                  \
  || ( ( strcmp( name, std::make_pair elem .first ) == 0 ) &&                      \
       ( vo.find( std::make_pair elem .second ) != vo.end() ) )

#define FC_REFLECT_ALIASED_NAMES( TYPE, MEMBERS, ALIASES )                         \
FC_REFLECT( TYPE, MEMBERS )                                                        \
namespace fc {                                                                     \
                                                                                   \
template<>                                                                         \
class from_variant_visitor<TYPE> : obsolete_call_detector                          \
{                                                                                  \
public:                                                                            \
from_variant_visitor( const variant_object& _vo, TYPE& v ) :vo( _vo ), val( v ) {} \
                                                                                   \
template<typename Member, class Class, Member( Class::* member )>                  \
void operator()( const char* name )const                                           \
{                                                                                  \
  auto itr = vo.find( name );                                                      \
  if( itr != vo.end() )                                                            \
    from_variant( itr->value(), val.*member );                                     \
  else if( enable_obsolete_call_detection && ( false                               \
    BOOST_PP_SEQ_FOR_EACH( FC_REFLECT_ALIAS_HANDLER, name, ALIASES )               \
  ) )                                                                              \
    report_error();                                                                \
}                                                                                  \
                                                                                   \
const variant_object& vo;                                                          \
TYPE& val;                                                                         \
};                                                                                 \
                                                                                   \
}

FC_REFLECT( hive::plugins::condenser_api::api_chain_properties,
        (account_creation_fee)(maximum_block_size)(hbd_interest_rate)(account_subsidy_budget)(account_subsidy_decay)
        )

FC_REFLECT( hive::plugins::condenser_api::legacy_price, (base)(quote) )
