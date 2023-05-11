#include "cpython_interface.hpp"

#include <hive/protocol/types.hpp>
#include <hive/protocol/operations.hpp>
#include <hive/protocol/transaction.hpp>

#include <fc/io/json.hpp>

#include <iostream>

namespace cpp
{
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

  unsigned int protocol::cpp_validate_operation( const std::string& operation )
  {
    unsigned int _result = 0;

    try
    {
      fc::variant _v = fc::json::from_string( operation );
      hive::protocol::operation _operation = _v.as<hive::protocol::operation>();

      validate_visitor _visitor;
      _operation.visit( _visitor );

      _result = 1;
    } FC_CAPTURE_AND_LOG(( operation ))

    return _result;
  };

  unsigned int protocol::cpp_validate_transaction( const std::string& transaction )
  {
    int _result = 0;

    try
    {
      fc::variant _v = fc::json::from_string( transaction );
      hive::protocol::transaction _transaction = _v.as<hive::protocol::transaction>();

      _transaction.validate();

      _result = 1;
    } FC_CAPTURE_AND_LOG(( transaction ))

    return _result;
  }

  result protocol::cpp_calculate_digest( const std::string& transaction, const std::string& chain_id )
  {
    result _result;

    try
    {
      const auto _transaction = fc::json::from_string( transaction ).as<hive::protocol::transaction>();

      const auto _digest = _transaction.sig_digest( hive::protocol::chain_id_type( chain_id ), hive::protocol::pack_type::hf26 );

      _result.content = _digest.str();

      _result.value = 1;
    } FC_CAPTURE_AND_LOG(( transaction ))

    return _result;
  }

  result protocol::cpp_serialize_transaction( const std::string& transaction )
  {
    result _result;

    try
    {
      hive::protocol::serialization_mode_controller::mode_guard guard( hive::protocol::transaction_serialization_type::hf26 );
      hive::protocol::serialization_mode_controller::set_pack( hive::protocol::transaction_serialization_type::hf26 );

      fc::variant _v = fc::json::from_string( transaction );
      hive::protocol::transaction _transaction = _v.as<hive::protocol::transaction>();

      auto _packed = fc::to_hex( fc::raw::pack_to_vector( _transaction ) );

      _result.content = std::string( _packed.data(), _packed.size() );

      _result.value = 1;
    } FC_CAPTURE_AND_LOG(( transaction ))

    return _result;
  }
}
