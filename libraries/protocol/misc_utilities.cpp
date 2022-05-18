#include <hive/protocol/misc_utilities.hpp>

namespace hive { namespace protocol {

thread_local transaction_serialization_type serialization_mode_controller::transaction_serialization = serialization_mode_controller::default_transaction_serialization;
pack_type serialization_mode_controller::pack                                                        = serialization_mode_controller::default_pack;
thread_local fc::optional<pack_type> serialization_mode_controller::current_pack                     = fc::optional<pack_type>();

serialization_mode_controller::mode_guard::mode_guard( transaction_serialization_type val ) : old_transaction_serialization( serialization_mode_controller::transaction_serialization )
{
  serialization_mode_controller::transaction_serialization = val;
}

serialization_mode_controller::mode_guard::~mode_guard()
{
  serialization_mode_controller::transaction_serialization = old_transaction_serialization;
}

serialization_mode_controller::pack_guard::pack_guard( pack_type new_pack ) : old_pack( serialization_mode_controller::get_current_pack() )
{
  serialization_mode_controller::current_pack = new_pack;
}

serialization_mode_controller::pack_guard::pack_guard() : old_pack( serialization_mode_controller::get_current_pack() )
{
  serialization_mode_controller::current_pack = serialization_mode_controller::get_another_pack();
}

serialization_mode_controller::pack_guard::~pack_guard()
{
  serialization_mode_controller::current_pack = old_pack;
}

bool serialization_mode_controller::legacy_enabled()
{
  return transaction_serialization == hive::protocol::transaction_serialization_type::legacy;
}

void serialization_mode_controller::set_pack( pack_type new_pack )
{
  pack          = new_pack;
  current_pack  = new_pack;
}

pack_type serialization_mode_controller::get_current_pack()
{
  if( !current_pack.valid() )
    current_pack = { pack };
  return *current_pack;
}

pack_type serialization_mode_controller::get_another_pack()
{
  switch( get_current_pack() )
  {
    case pack_type::legacy: return pack_type::hf26;
    case pack_type::hf26: return pack_type::legacy;
    default:
      FC_ASSERT( false, "an incorrect value of pack mode" );
  }
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

