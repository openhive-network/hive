#include <hive/plugins/follow/follow_operations.hpp>

#include <hive/protocol/operation_util_impl.hpp>

namespace hive { namespace plugins{ namespace follow {

void follow_operation::validate()const
{
   FC_ASSERT( follower != following, "You cannot follow yourself" );
}

void reblog_operation::validate()const
{
   FC_ASSERT( account != author, "You cannot reblog your own content" );
}

} } } //hive::plugins::follow

HIVE_DEFINE_OPERATION_TYPE( hive::plugins::follow::follow_plugin_operation )
