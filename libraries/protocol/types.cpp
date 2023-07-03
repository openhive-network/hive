#include <hive/protocol/config.hpp>
#include <hive/protocol/types.hpp>

#include <fc/crypto/base58.hpp>
#include <fc/crypto/ripemd160.hpp>
#include <fc/exception/exception.hpp>
#include <fc/io/raw.hpp>

namespace hive { namespace protocol {

  public_key_type::public_key_type():key_data(){};

  public_key_type::public_key_type( const fc::ecc::public_key_data& data )
    :key_data( data ) {};

  public_key_type::public_key_type( const fc::ecc::public_key& pubkey )
    :key_data( pubkey ) {};

  public_key_type::public_key_type( const std::string& base58str )
  {
    /*
      In this context it's allowed that `key_data` == `empty_pub`.
      Check assertions 'my->_key != empty_pub' in `fc::public_key`
    */
    std::string _empty_base58str = static_cast<std::string>( public_key_type() );
    if( _empty_base58str != base58str )
      key_data = fc::ecc::public_key::from_base58_with_prefix( base58str, HIVE_ADDRESS_PREFIX );
  };

  public_key_type::operator fc::ecc::public_key_data() const
  {
    return key_data;
  };

  public_key_type::operator fc::ecc::public_key() const
  {
    return fc::ecc::public_key( key_data );
  };

  public_key_type::operator std::string() const
  {
    return fc::ecc::public_key::to_base58_with_prefix( key_data, HIVE_ADDRESS_PREFIX );
  }

  bool operator == ( const public_key_type& p1, const fc::ecc::public_key& p2)
  {
    return p1.key_data == p2.serialize();
  }

  bool operator == ( const public_key_type& p1, const public_key_type& p2)
  {
    return p1.key_data == p2.key_data;
  }

  bool operator != ( const public_key_type& p1, const public_key_type& p2)
  {
    return p1.key_data != p2.key_data;
  }

  // extended_public_key_type

  extended_public_key_type::extended_public_key_type():key_data(){};

  extended_public_key_type::extended_public_key_type( const fc::ecc::extended_key_data& data )
    :key_data( data ){};

  extended_public_key_type::extended_public_key_type( const fc::ecc::extended_public_key& extpubkey )
  {
    key_data = extpubkey.serialize_extended();
  };

  extended_public_key_type::extended_public_key_type( const std::string& base58str )
  {
    std::string prefix( HIVE_ADDRESS_PREFIX );

    const size_t prefix_len = prefix.size();
    FC_ASSERT( base58str.size() > prefix_len );
    FC_ASSERT( base58str.substr( 0, prefix_len ) ==  prefix , "", ("base58str", base58str) );
    auto bin = fc::from_base58( base58str.substr( prefix_len ) );
    binary_key bin_key;
    fc::raw::unpack_from_vector(bin, bin_key);
    FC_ASSERT( fc::ripemd160::hash( bin_key.data.data, bin_key.data.size() )._hash[0] == bin_key.check );
    key_data = bin_key.data;
  }

  extended_public_key_type::operator fc::ecc::extended_public_key() const
  {
    return fc::ecc::extended_public_key::deserialize( key_data );
  }

  extended_public_key_type::operator std::string() const
  {
    binary_key k;
    k.data = key_data;
    k.check = fc::ripemd160::hash( k.data.data, k.data.size() )._hash[0];
    auto data = fc::raw::pack_to_vector( k );
    return HIVE_ADDRESS_PREFIX + fc::to_base58( data.data(), data.size() );
  }

  bool operator == ( const extended_public_key_type& p1, const fc::ecc::extended_public_key& p2)
  {
    return p1.key_data == p2.serialize_extended();
  }

  bool operator == ( const extended_public_key_type& p1, const extended_public_key_type& p2)
  {
    return p1.key_data == p2.key_data;
  }

  bool operator != ( const extended_public_key_type& p1, const extended_public_key_type& p2)
  {
    return p1.key_data != p2.key_data;
  }

  // extended_private_key_type

  extended_private_key_type::extended_private_key_type():key_data(){};

  extended_private_key_type::extended_private_key_type( const fc::ecc::extended_key_data& data )
    :key_data( data ){};

  extended_private_key_type::extended_private_key_type( const fc::ecc::extended_private_key& extprivkey )
  {
    key_data = extprivkey.serialize_extended();
  };

  extended_private_key_type::extended_private_key_type( const std::string& base58str )
  {
    std::string prefix( HIVE_ADDRESS_PREFIX );

    const size_t prefix_len = prefix.size();
    FC_ASSERT( base58str.size() > prefix_len );
    FC_ASSERT( base58str.substr( 0, prefix_len ) ==  prefix , "", ("base58str", base58str) );
    auto bin = fc::from_base58( base58str.substr( prefix_len ) );
    binary_key bin_key;
    fc::raw::unpack_from_vector(bin, bin_key);
    FC_ASSERT( fc::ripemd160::hash( bin_key.data.data, bin_key.data.size() )._hash[0] == bin_key.check );
    key_data = bin_key.data;
  }

  extended_private_key_type::operator fc::ecc::extended_private_key() const
  {
    return fc::ecc::extended_private_key::deserialize( key_data );
  }

  extended_private_key_type::operator std::string() const
  {
    binary_key k;
    k.data = key_data;
    k.check = fc::ripemd160::hash( k.data.data, k.data.size() )._hash[0];
    auto data = fc::raw::pack_to_vector( k );
    return HIVE_ADDRESS_PREFIX + fc::to_base58( data.data(), data.size() );
  }

  bool operator == ( const extended_private_key_type& p1, const fc::ecc::extended_public_key& p2)
  {
    return p1.key_data == p2.serialize_extended();
  }

  bool operator == ( const extended_private_key_type& p1, const extended_private_key_type& p2)
  {
    return p1.key_data == p2.key_data;
  }

  bool operator != ( const extended_private_key_type& p1, const extended_private_key_type& p2)
  {
    return p1.key_data != p2.key_data;
  }

  chain_id_type generate_chain_id( const std::string& chain_id_name )
  {
    if( chain_id_name.size() )
      return fc::sha256::hash( chain_id_name.c_str() );
    else
      return fc::sha256();
  }

} } // hive::protocol

namespace fc
{
  using namespace std;
  void to_variant( const hive::protocol::public_key_type& var,  fc::variant& vo )
  {
    vo = std::string( var );
  }

  void from_variant( const fc::variant& var,  hive::protocol::public_key_type& vo )
  {
    vo = hive::protocol::public_key_type( var.as_string() );
  }

  void to_variant( const hive::protocol::extended_public_key_type& var, fc::variant& vo )
  {
    vo = std::string( var );
  }

  void from_variant( const fc::variant& var, hive::protocol::extended_public_key_type& vo )
  {
    vo = hive::protocol::extended_public_key_type( var.as_string() );
  }

  void to_variant( const hive::protocol::extended_private_key_type& var, fc::variant& vo )
  {
    vo = std::string( var );
  }

  void from_variant( const fc::variant& var, hive::protocol::extended_private_key_type& vo )
  {
    vo = hive::protocol::extended_private_key_type( var.as_string() );
  }
} // fc
