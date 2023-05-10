#include "cpython_interface.hpp"

#include <hive/protocol/asset.hpp>
#include <hive/protocol/operations.hpp>
#include <hive/protocol/transaction.hpp>

#include <fc/io/json.hpp>

#include <iostream>

void log( const std::string& message )
{
  std::cout<< message << std::endl;
}

struct validate_visitor
{
  typedef void result_type;

  template< typename T >
  void operator()( const T& x )
  {
    x.validate();
  }
};

int cpp_validate_operation( char* content )
{
  if( !content )
    return false;

  int _result = 0;

  try
  {
    fc::variant _v = fc::json::from_string( content );
    hive::protocol::operation _operation = _v.as<hive::protocol::operation>();

    validate_visitor _visitor;
    _operation.visit( _visitor );

    _result = 1;
  } FC_CAPTURE_AND_LOG(( content ))

  return _result;
}

int cpp_calculate_digest( char* content, unsigned char* digest )
{
  if( !content )
    return false;

  int _result = 0;

  try
  {
    if( digest )
    {
      fc::variant _v = fc::json::from_string( content );
      hive::protocol::transaction _transaction = _v.as<hive::protocol::transaction>();

      hive::protocol::serialization_mode_controller::pack_guard guard( hive::protocol::pack_type::hf26 );
      hive::protocol::digest_type::encoder enc;
      fc::raw::pack( enc, _transaction );
      hive::protocol::digest_type _digest = enc.result();

      memcpy( digest, _digest.data(), _digest.data_size() );
    }

    _result = 1;
  } FC_CAPTURE_AND_LOG(( content ))

  return _result;
}

int cpp_validate_transaction( char* content )
{
  if( !content )
    return false;

  int _result = 0;

  try
  {
    fc::variant _v = fc::json::from_string( content );
    hive::protocol::transaction _transaction = _v.as<hive::protocol::transaction>();

    _transaction.validate();

    _result = 1;
  } FC_CAPTURE_AND_LOG(( content ))

  return _result;
}

int cpp_serialize_transaction( char* content, unsigned char* serialized_transaction )
{
  if( !content )
    return false;

  int _result = 0;

  try
  {
    fc::variant _v = fc::json::from_string( content );
    hive::protocol::transaction _transaction = _v.as<hive::protocol::transaction>();

    std::string _str = fc::to_hex( fc::raw::pack_to_vector( _transaction ) );

    memcpy( serialized_transaction, _str.data(), _str.size() );

    _result = 1;
  } FC_CAPTURE_AND_LOG(( content ))

  return _result;
}
