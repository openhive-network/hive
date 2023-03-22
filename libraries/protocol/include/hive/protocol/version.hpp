#pragma once

#include <fc/string.hpp>
#include <fc/time.hpp>


#pragma GCC push_options
#pragma GCC optimize("O0")

namespace hive { namespace protocol {

/*
  * This class represents the basic versioning scheme of the Hive blockchain.
  * All versions are a triple consisting of a major version, hardfork version, and release version.
  * It allows easy comparison between versions. A version is a read only object.
  */
struct version
{
  version() {}
  version( uint8_t m, uint8_t h, uint16_t r );
  virtual ~version() {}

  bool operator == ( const version& o )const { return _operator< operator_type::equal >( o.v_num ); }
  bool operator != ( const version& o )const { return _operator< operator_type::not_equal >( o.v_num ); }
  bool operator <  ( const version& o )const { return _operator< operator_type::less >( o.v_num ); }
  bool operator <= ( const version& o )const { return _operator< operator_type::less_equal >( o.v_num ); }
  bool operator >  ( const version& o )const { return _operator< operator_type::greater >( o.v_num ); }
  bool operator >= ( const version& o )const { return _operator< operator_type::greater_equal >( o.v_num ); }

  operator fc::string()const;

  uint32_t major_v() const { return ( v_num & 0XFF000000 ) >> 24; }
  uint32_t minor_v() const { return ( v_num & 0x00FF0000 ) >> 16; }
  uint32_t rev_v()   const { return   v_num & 0x0000FFFF; }

  uint32_t v_num = 0;

  protected:

    enum class operator_type
    {
      equal,
      not_equal,
      less,
      less_equal,
      greater,
      greater_equal
    };

    template< operator_type op_type >
    bool _operator( uint32_t val, std::function< uint32_t( uint32_t ) >&& val_call = []( uint32_t val ){ return val; } ) const
    {
      /*
        There is allowed after HF24, that major version can be different from another one.
        The old major version always had '0' value, new major version has always `1` value.
        Some witnesses can still use nodes before HF24, so loosening restrictions regarding a version is necessary.
      */
      auto _val = ( val & 0xFF000000 ) >> 24;

      if( major_v() == 0 && _val == 1 )//0.HARDFORK_NUMBER == 1.HARDFORK_NUMBER
        val &= 0x00FFFFFF;
      else if( major_v() == 1 && _val == 0 )//1.HARDFORK_NUMBER == 0.HARDFORK_NUMBER
        val &= 0x01FFFFFF;

      if( op_type == operator_type::equal )
        return v_num == val_call( val );
      else if( op_type == operator_type::not_equal )
        return v_num != val_call( val );
      else if( op_type == operator_type::less )
        return v_num < val_call( val );
      else if( op_type == operator_type::less_equal )
        return v_num <= val_call( val );
      else if( op_type == operator_type::greater )
        return v_num > val_call( val );
      else if( op_type == operator_type::greater_equal )
        return v_num >= val_call( val );
    }

};

struct hardfork_version : version
{
  hardfork_version():version() {}
  hardfork_version( uint8_t m, uint8_t h ):version( m, h, 0 ) {}
  hardfork_version( version v ) { v_num = v.v_num & 0xFFFF0000; }
  ~hardfork_version() {}

  void operator =  ( const version& o ) { v_num = o.v_num & 0xFFFF0000; }
  void operator =  ( const hardfork_version& o ) { v_num = o.v_num & 0xFFFF0000; }

  bool operator == ( const hardfork_version& o )const { return _operator< operator_type::equal >( o.v_num ); }
  bool operator != ( const hardfork_version& o )const { return _operator< operator_type::not_equal >( o.v_num ); }
  bool operator <  ( const hardfork_version& o )const { return _operator< operator_type::less >( o.v_num ); }
  bool operator <= ( const hardfork_version& o )const { return _operator< operator_type::less_equal >( o.v_num ); }
  bool operator >  ( const hardfork_version& o )const { return _operator< operator_type::greater >( o.v_num ); }
  bool operator >= ( const hardfork_version& o )const { return _operator< operator_type::greater_equal >( o.v_num ); }

  bool operator == ( const version& o )const { return _operator< operator_type::equal >(          o.v_num, []( uint32_t val ){ return val & 0xFFFF0000; } ); }
  bool operator != ( const version& o )const { return _operator< operator_type::not_equal >(      o.v_num, []( uint32_t val ){ return val & 0xFFFF0000; } ); }
  bool operator <  ( const version& o )const { return _operator< operator_type::less >(           o.v_num, []( uint32_t val ){ return val & 0xFFFF0000; } ); }
  bool operator <= ( const version& o )const { return _operator< operator_type::less_equal >(     o.v_num, []( uint32_t val ){ return val & 0xFFFF0000; } ); }
  bool operator >  ( const version& o )const { return _operator< operator_type::greater >(        o.v_num, []( uint32_t val ){ return val & 0xFFFF0000; } ); }
  bool operator >= ( const version& o )const { return _operator< operator_type::greater_equal >(  o.v_num, []( uint32_t val ){ return val & 0xFFFF0000; } ); }
};

struct hardfork_version_vote
{
  hardfork_version_vote() {}
  hardfork_version_vote( hardfork_version v, fc::time_point_sec t ):hf_version( v ),hf_time( t ) {}

  hardfork_version   hf_version;
  fc::time_point_sec hf_time;
};

} } // hive::protocol

namespace fc
{
  class variant;
  void to_variant( const hive::protocol::version& v, variant& var );
  void from_variant( const variant& var, hive::protocol::version& v );

  void to_variant( const hive::protocol::hardfork_version& hv, variant& var );
  void from_variant( const variant& var, hive::protocol::hardfork_version& hv );
} // fc

#include <fc/reflect/reflect.hpp>
FC_REFLECT( hive::protocol::version, (v_num) )
FC_REFLECT_DERIVED( hive::protocol::hardfork_version, (hive::protocol::version), )

FC_REFLECT( hive::protocol::hardfork_version_vote, (hf_version)(hf_time) )


#pragma GCC pop_options
