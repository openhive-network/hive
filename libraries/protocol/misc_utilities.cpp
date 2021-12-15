#include <hive/protocol/misc_utilities.hpp>

namespace hive { namespace protocol {

thread_local bool dynamic_serializer::legacy_enabled = false;

const std::string legacy_switcher::serialization_detector = "Invalid cast from";

legacy_switcher::legacy_switcher() : old_legacy_enabled( dynamic_serializer::legacy_enabled )
{
  dynamic_serializer::legacy_enabled = !dynamic_serializer::legacy_enabled;
}

legacy_switcher::legacy_switcher( bool val ) : old_legacy_enabled( dynamic_serializer::legacy_enabled )
{
  dynamic_serializer::legacy_enabled = val;
}

legacy_switcher::~legacy_switcher()
{
  dynamic_serializer::legacy_enabled = old_legacy_enabled;
}

bool legacy_switcher::try_another_serialization( const fc::bad_cast_exception& e )
{
  auto _found = e.to_string().find( serialization_detector );
  return _found != std::string::npos;
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

