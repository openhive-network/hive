#pragma once

#include <core/beekeeper_wallet_base.hpp>

#include <fc/reflect/reflect.hpp>

#include <functional>
#include <string>
#include <chrono>

namespace beekeeper {

struct wallet_details
{
  std::string name;
  bool unlocked = false;
};

struct info
{
  std::string now;
  std::string timeout_time;
};

namespace types
{
  using basic_method_type         = std::function<void()>;
  using notification_method_type  = basic_method_type;
  using lock_method_type          = basic_method_type;
  using timepoint_t               = std::chrono::time_point<std::chrono::system_clock>;
}

namespace utility
{
  inline public_key_type get_public_key( const std::string& source )
  {
    return public_key_type::from_base58( source, false/*is_sha256*/ );
  }

  template<typename result_collection_type, typename source_type>
  result_collection_type get_public_keys( const source_type& source )
  {
    result_collection_type _result;

    std::transform( source.begin(), source.end(), std::inserter( _result, _result.end() ),
    []( const public_key_type& public_key ){ return public_key_type::to_base58( public_key, false/*is_sha256*/ ); } );

    return _result;
  }
}

}
namespace fc
{
  void from_variant( const fc::variant& var, beekeeper::wallet_details& vo );
  void to_variant( const beekeeper::wallet_details& var, fc::variant& vo );

  void from_variant( const fc::variant& var, beekeeper::info& vo );
  void to_variant( const beekeeper::info& var, fc::variant& vo );
}

FC_REFLECT( beekeeper::wallet_details, (name)(unlocked) )
FC_REFLECT( beekeeper::info, (now)(timeout_time) )