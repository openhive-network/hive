#pragma once

#include <hive/protocol/asset.hpp>

#define OBSOLETE_SYMBOL_LEGACY_SER_1   (uint64_t(1) | (OBSOLETE_SYMBOL_U64 << 8))
#define OBSOLETE_SYMBOL_LEGACY_SER_2   (uint64_t(2) | (OBSOLETE_SYMBOL_U64 << 8))
#define OBSOLETE_SYMBOL_LEGACY_SER_3   (uint64_t(5) | (OBSOLETE_SYMBOL_U64 << 8))
#define OBSOLETE_SYMBOL_LEGACY_SER_4   (uint64_t(3) | (uint64_t('0') << 8) | (uint64_t('.') << 16) | (uint64_t('0') << 24) | (uint64_t('0') << 32) | (uint64_t('1') << 40))
#define OBSOLETE_SYMBOL_LEGACY_SER_5   (uint64_t(3) | (uint64_t('6') << 8) | (uint64_t('.') << 16) | (uint64_t('0') << 24) | (uint64_t('0') << 32) | (uint64_t('0') << 40))

namespace hive { namespace protocol {

class legacy_hive_asset_symbol_type
{
  public:
    legacy_hive_asset_symbol_type() {}

    bool is_canon()const
    {   return ( ser == OBSOLETE_SYMBOL_SER );    }

    uint64_t ser = OBSOLETE_SYMBOL_SER;
};

struct legacy_hive_asset
{
  public:
    legacy_hive_asset() {}

    template< bool force_canon >
    asset to_asset()const
    {
      if( force_canon )
      {
        FC_ASSERT( symbol.is_canon(), "Must use canonical HIVE symbol serialization" );
      }
      return asset( amount, HIVE_SYMBOL );
    }

    static legacy_hive_asset from_amount( share_type amount )
    {
      legacy_hive_asset leg;
      leg.amount = amount;
      return leg;
    }

    static legacy_hive_asset from_asset( const asset& a )
    {
      FC_ASSERT( a.symbol == HIVE_SYMBOL );
      return from_amount( a.amount );
    }

    share_type                       amount;
    legacy_hive_asset_symbol_type   symbol;
};

} }

namespace fc { namespace raw {

template< typename Stream >
inline void pack( Stream& s, const hive::protocol::legacy_hive_asset_symbol_type& sym )
{
  switch( sym.ser )
  {
    case OBSOLETE_SYMBOL_LEGACY_SER_1:
    case OBSOLETE_SYMBOL_LEGACY_SER_2:
    case OBSOLETE_SYMBOL_LEGACY_SER_3:
    case OBSOLETE_SYMBOL_LEGACY_SER_4:
    case OBSOLETE_SYMBOL_LEGACY_SER_5:
      wlog( "pack legacy serialization ${s}", ("s", sym.ser) );
      pack( s, sym.ser );
      break;
    case OBSOLETE_SYMBOL_SER:
      if( hive::protocol::serialization_mode_controller::get_current_pack() == hive::protocol::pack_type::legacy )
        pack( s, sym.ser );
      else
        pack( s, HIVE_ASSET_NUM_HIVE );
      break;
    default:
      FC_ASSERT( false, "Cannot serialize legacy symbol ${s}", ("s", sym.ser) );
  }
}

template< typename Stream >
inline void unpack( Stream& s, hive::protocol::legacy_hive_asset_symbol_type& sym, uint32_t )
{
  //  994240:        "account_creation_fee": "0.1 HIVE"
  // 1021529:        "account_creation_fee": "10.0 HIVE"
  // 3143833:        "account_creation_fee": "3.00000 HIVE"
  // 3208405:        "account_creation_fee": "2.00000 HIVE"
  // 3695672:        "account_creation_fee": "3.00 HIVE"

// block_num=3705111 Merkle check failed mtlk block_num=3705111 
// block_num=3705120 failed mtlk block_num=3705120
// block_num=3713940 Merkle check failed mtlk block_num=3713940
// block_num=3714132 Merkle check failed mtlk block_num=3714132
// block_num=3714567 Merkle check failed mtlk block_num=3714567
// block_num=3714588 Merkle check failed mtlk block_num=3714588
// block_num=4138790 Merkle check failed mtlk block_num=4138790


  // 4338089:        "account_creation_fee": "0.001 0.001"
  // 4626205:        "account_creation_fee": "6.000 6.000"
  // 4632595:        "account_creation_fee": "6.000 6.000"


// Merkle check failed mtlk block_num=3143833 id=002ff899e865c901ff76737b976964ce09589931 block.transaction_merkle_root=3f3a0d6afadaab31bafd5e0c24166a6d1f5e9a02 merkle_root=2d1cc72b9d875bb9cff86f290c0cb0bdce738cdd block={"previous":"002ff8988b0ce1b83db32995529e09557bf69c52","timestamp":"2016-07-12T21:42:18","witness":"arhag","transaction_merkle_root":"3f3a0d6afadaab31bafd5e0c24166a6d1f5e9a02","extensions":[],"witness_signature":"1f64814e2befcf468ab8b8b1c16f35a1b30822d0552df0bcc67a0fbd771680f39e56df8f14e177b0d15b3ffd4d32837faf8dc4efdaf956301d33b5cd3feefe28f6","transactions":[{"ref_block_num":63640,"ref_block_prefix":3101756555,"expiration":"2016-07-12T21:42:45","operations":[{"type":"witness_update_operation","value":{"owner":"datasecuritynode","url":"http://steemd.com/@datasecuritynode/datasecuritynode-witness-post","block_signing_key":"STM5zo37Am3hyCgGWjMVNbacRohRRrQvjodfZWnDYL3rPaGCVjKFU","props":{"account_creation_fee":{"amount":"300000","precision":3,"nai":"@@000000021"},"maximum_block_size":131072,"hbd_interest_rate":1000},"fee":{"amount":"0","precision":3,"nai":"@@000000021"}}}],"extensions":[],"signatures":["1f4b43086972eb86197c12e94a501f8b4ed1c11d6c81a58c94dfc01705714656a36e89afe0e5d0d5dc360d27757b03b04c9a7e4dbdf4ef8ba69ad0dc579404e858"]},{"ref_block_num":63572,"ref_block_prefix":696456221,"expiration":"2016-07-12T21:42:30","operations":[{"type":"comment_operation","value":{"parent_author":"mrwang","parent_permlink":"for-account-holder-bitrex-or-anybody-that-can-help","author":"liondani","permlink":"re-mrwang-for-account-holder-bitrex-or-anybody-that-can-help-20160712t214216554z","title":"","body":"We can make him whole just up-voting his post here... let's do it!","json_metadata":"{\"tags\":[\"money\"]}"}}],"extensions":[],"signatures":["200a59c4b98c568490ae8c12b57e303f27ef22b66e299b2481592befcf02b11e7c1a28981006bc4f80c6806b138a9d69a6ef2d37ae772401eeeca612bfd1a0e3a2"]}]}
// Missing Active Authority datasecuritynode auth={"weight_threshold":1,"account_auths":[],"key_auths":[["STM5zo37Am3hyCgGWjMVNbacRohRRrQvjodfZWnDYL3rPaGCVjKFU",1]]} owner={"weight_threshold":1,"account_auths":[],"key_auths":[["STM5zo37Am3hyCgGWjMVNbacRohRRrQvjodfZWnDYL3rPaGCVjKFU",1]]} block_num=3143833
// Once more Missing Active Authority datasecuritynode auth={"weight_threshold":1,"account_auths":[],"key_auths":[["STM5zo37Am3hyCgGWjMVNbacRohRRrQvjodfZWnDYL3rPaGCVjKFU",1]]} owner={"weight_threshold":1,"account_auths":[],"key_auths":[["STM5zo37Am3hyCgGWjMVNbacRohRRrQvjodfZWnDYL3rPaGCVjKFU",1]]} block_num=3143833
// :
// 3143833
// 3208405
// 3695672
// 3705111
// 3705120
// 3713940
// 3714132
// 3714567
// 3714588
// 4138790
// 4338089
// 4626205
// 4632595


  uint64_t ser = 0;
  s.read( (char*) &ser, 4 );

  if( ser == HIVE_ASSET_NUM_HIVE )
  {
    sym.ser = OBSOLETE_SYMBOL_SER;
    return;
  }
  s.read( ((char*) &ser)+4, 4 );

  switch( ser )
  {
    case OBSOLETE_SYMBOL_LEGACY_SER_1:
    case OBSOLETE_SYMBOL_LEGACY_SER_2:
    case OBSOLETE_SYMBOL_LEGACY_SER_3:
    case OBSOLETE_SYMBOL_LEGACY_SER_4:
    case OBSOLETE_SYMBOL_LEGACY_SER_5:
      wlog( "unpack legacy serialization ${s}", ("s", ser) );
    case OBSOLETE_SYMBOL_SER:
      sym.ser = ser;
      break;
    default:
      FC_ASSERT( false, "Cannot deserialize legacy symbol ${s}", ("s", ser) );
  }
}

} // fc::raw

inline void to_variant( const hive::protocol::legacy_hive_asset& leg, fc::variant& v )
{
  to_variant( leg.to_asset<false>(), v );
}

inline void from_variant( const fc::variant& v, hive::protocol::legacy_hive_asset& leg )
{
  hive::protocol::asset a;
  from_variant( v, a );
  leg = hive::protocol::legacy_hive_asset::from_asset( a );
}

template<>
struct get_typename< hive::protocol::legacy_hive_asset_symbol_type >
{
  static const char* name()
  {
    return "hive::protocol::legacy_hive_asset_symbol_type";
  }
};

} // fc

FC_REFLECT( hive::protocol::legacy_hive_asset,
  (amount)
  (symbol)
  )
