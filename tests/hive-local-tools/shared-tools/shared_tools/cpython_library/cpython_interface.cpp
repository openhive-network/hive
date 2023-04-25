#include "cpython_interface.hpp"

#include <hive/protocol/asset.hpp>

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
