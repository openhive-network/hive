#include <hive/protocol/misc_utilities.hpp>

namespace hive { namespace protocol {

thread_local transaction_serialization_type serialization_mode_controller::transaction_serialization = serialization_mode_controller::default_transaction_serialization;

serialization_mode_controller::mode_guard::mode_guard( transaction_serialization_type val ) : old_transaction_serialization( serialization_mode_controller::transaction_serialization )
{
  serialization_mode_controller::transaction_serialization = val;
}

serialization_mode_controller::mode_guard::~mode_guard()
{
  serialization_mode_controller::transaction_serialization = old_transaction_serialization;
}

std::string trim_legacy_typename_namespace( const std::string& name )
{
  auto start = name.find_last_of( ':' ) + 1;
  auto end   = name.find_last_of( '_' );

  //An exception for `update_proposal_end_date` operation. Unfortunately this operation hasn't a postix `operation`.
  if( end != std::string::npos )
  {
    const std::string _postfix = "operation";
    if( name.substr( end + 1 ) != _postfix )
      end = name.size();
  }

  return name.substr( start, end-start );
}

} }

