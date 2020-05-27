#include <hive/plugins/follow_api/follow_api_plugin.hpp>
#include <hive/plugins/follow_api/follow_api.hpp>

#include <hive/plugins/follow/follow_objects.hpp>

namespace hive { namespace plugins { namespace follow {

namespace detail {

class follow_api_impl
{
   public:
      follow_api_impl() : _db( appbase::app().get_plugin< hive::plugins::chain::chain_plugin >().db() ) {}

      DECLARE_API_IMPL(
         (get_followers)
         (get_following)
         (get_follow_count)
         (get_feed_entries)
         (get_feed)
         (get_blog_entries)
         (get_blog)
         (get_account_reputations)
         (get_reblogged_by)
         (get_blog_authors)
      )

      chain::database& _db;
};

DEFINE_API_IMPL( follow_api_impl, get_followers )
{
    FC_ASSERT( false, "Supported by hivemind" );
}

DEFINE_API_IMPL( follow_api_impl, get_following )
{
    FC_ASSERT( false, "Supported by hivemind" );
}

DEFINE_API_IMPL( follow_api_impl, get_follow_count )
{
    FC_ASSERT( false, "Supported by hivemind" );
}

DEFINE_API_IMPL( follow_api_impl, get_feed_entries )
{
    FC_ASSERT( false, "Supported by hivemind" );
}

DEFINE_API_IMPL( follow_api_impl, get_feed )
{
    FC_ASSERT( false, "Supported by hivemind" );
}

DEFINE_API_IMPL( follow_api_impl, get_blog_entries )
{
    FC_ASSERT( false, "Supported by hivemind" );
}

DEFINE_API_IMPL( follow_api_impl, get_blog )
{
    FC_ASSERT( false, "Supported by hivemind" );
}

DEFINE_API_IMPL( follow_api_impl, get_account_reputations )
{
    FC_ASSERT( false, "Supported by hivemind" );
}

DEFINE_API_IMPL( follow_api_impl, get_reblogged_by )
{
    FC_ASSERT( false, "Supported by hivemind" );
}

DEFINE_API_IMPL( follow_api_impl, get_blog_authors )
{
    FC_ASSERT( false, "Supported by hivemind" );
}

} // detail

follow_api::follow_api(): my( new detail::follow_api_impl() )
{
   JSON_RPC_REGISTER_API( HIVE_FOLLOW_API_PLUGIN_NAME );
}

follow_api::~follow_api() {}

DEFINE_READ_APIS( follow_api,
   (get_followers)
   (get_following)
   (get_follow_count)
   (get_feed_entries)
   (get_feed)
   (get_blog_entries)
   (get_blog)
   (get_account_reputations)
   (get_reblogged_by)
   (get_blog_authors)
)

} } } // hive::plugins::follow
