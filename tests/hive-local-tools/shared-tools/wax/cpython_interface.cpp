#include "cpython_interface.hpp"

#include <hive/protocol/types.hpp>
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

int cpp_calculate_digest( char* content, unsigned char* digest, const char* chain_id )
{
  if( !content )
    return false;

  int _result = 0;

  try
  {
    if (digest) {
      const auto _transaction =
          fc::json::from_string(content).as<hive::protocol::transaction>();
      const auto _digest = _transaction.sig_digest(
          hive::protocol::chain_id_type(chain_id),
          hive::protocol::pack_type::hf26);
      memcpy(digest, _digest.data(), _digest.data_size());
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

int cpp_serialize_transaction( char* content, unsigned char* serialized_transaction, unsigned int* dest_size )
{
  if( !content || !serialized_transaction || !dest_size )
    return false;

  int _result = 0;

  try
  {
    hive::protocol::serialization_mode_controller::mode_guard guard( hive::protocol::transaction_serialization_type::hf26 );
    hive::protocol::serialization_mode_controller::set_pack( hive::protocol::transaction_serialization_type::hf26 );

    fc::variant _v = fc::json::from_string( content );
    hive::protocol::transaction _transaction = _v.as<hive::protocol::transaction>();

    auto _packed = fc::raw::pack_to_vector( _transaction );

    memcpy( serialized_transaction, _packed.data(), _packed.size() );

    *dest_size = _packed.size();

    _result = 1;
  } FC_CAPTURE_AND_LOG(( content ))

  return _result;
}
