#include <hive/chain/database.hpp>

#include <hive/chain/generic_custom_operation_interpreter.hpp>

namespace hive { namespace chain {

std::string legacy_custom_name_from_type( const std::string& type_name )
{
  auto start = type_name.find_last_of( ':' ) + 1;
  auto end   = type_name.find_last_of( '_' );
  return type_name.substr( start, end-start );
}

} } // hive::chain
