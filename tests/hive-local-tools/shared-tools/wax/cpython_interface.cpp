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

  result protocol::method_wrapper( callback&& method )
  {
    result _result;

    try
    {
      method( _result );
      _result.value = ok;
    }
    catch( fc::exception& e )
    {
      _result.exception_message = e.to_detail_string();
    }
    catch( const std::exception& e )
    {
      _result.exception_message = e.what();
    }
    catch(...)
    {
      _result.exception_message = "Unknown exception.";
    }

    return _result;
  }

  result protocol::cpp_validate_operation( const std::string& operation )
  {
    auto impl = [&]( result& )
    {
      fc::variant _v = fc::json::from_string( operation );
      hive::protocol::operation _operation = _v.as<hive::protocol::operation>();

      validate_visitor _visitor;
      _operation.visit( _visitor );
    };

    return method_wrapper( impl );
  };

  result protocol::cpp_validate_transaction( const std::string& transaction )
  {
    auto impl = [&]( result& )
    {
      fc::variant _v = fc::json::from_string( transaction );
      hive::protocol::transaction _transaction = _v.as<hive::protocol::transaction>();

      _transaction.validate();
    };

    return method_wrapper( impl );
  }

  result protocol::cpp_calculate_digest( const std::string& transaction, const std::string& chain_id )
  {
    auto impl = [&]( result& _result )
    {
      const auto _transaction = fc::json::from_string( transaction ).as<hive::protocol::transaction>();

      const auto _digest = _transaction.sig_digest( hive::protocol::chain_id_type( chain_id ), hive::protocol::pack_type::hf26 );

      _result.content = _digest.str();
    };

    return method_wrapper( impl );
  }

  result protocol::cpp_serialize_transaction( const std::string& transaction )
  {
    auto impl = [&]( result& _result )
    {
      hive::protocol::serialization_mode_controller::mode_guard guard( hive::protocol::transaction_serialization_type::hf26 );
      hive::protocol::serialization_mode_controller::set_pack( hive::protocol::transaction_serialization_type::hf26 );

      fc::variant _v = fc::json::from_string( transaction );
      hive::protocol::transaction _transaction = _v.as<hive::protocol::transaction>();

      auto _packed = fc::to_hex( fc::raw::pack_to_vector( _transaction ) );

      _result.content = std::string( _packed.data(), _packed.size() );
    };

    return method_wrapper( impl );
  }
}
