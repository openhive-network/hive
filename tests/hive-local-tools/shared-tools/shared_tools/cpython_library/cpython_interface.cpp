#include "cpython_interface.hpp"

#include <hive/protocol/asset.hpp>
#include <hive/protocol/operations.hpp>

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

int add_three_assets( int param_1, int param_2, int param_3 )
{
  hive::protocol::asset _asset_a( param_1, HIVE_SYMBOL );
  hive::protocol::asset _asset_b( param_2, HIVE_SYMBOL );
  hive::protocol::asset _asset_c( param_3, HIVE_SYMBOL );

  auto _result = _asset_a + _asset_b + _asset_c;

  return _result.amount.value;
}

int add_two_numbers( int param_1, int param_2 )
{
  return param_1 + param_2;
}

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