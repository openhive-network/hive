#include <hive/plugins/tags_api/tags_api_plugin.hpp>
#include <hive/plugins/tags_api/tags_api.hpp>
#include <hive/plugins/tags/tags_plugin.hpp>
#include <hive/plugins/follow_api/follow_api_plugin.hpp>
#include <hive/plugins/follow_api/follow_api.hpp>

#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/util/reward.hpp>
#include <hive/chain/util/uint256.hpp>

namespace hive { namespace plugins { namespace tags {

namespace detail {

class tags_api_impl
{
   public:
      tags_api_impl() : _db( appbase::app().get_plugin< hive::plugins::chain::chain_plugin >().db() ) {}

      DECLARE_API_IMPL(
         (get_trending_tags)
         (get_tags_used_by_author)
         (get_discussion)
         (get_content_replies)
         (get_post_discussions_by_payout)
         (get_comment_discussions_by_payout)
         (get_discussions_by_trending)
         (get_discussions_by_created)
         (get_discussions_by_active)
         (get_discussions_by_cashout)
         (get_discussions_by_votes)
         (get_discussions_by_children)
         (get_discussions_by_hot)
         (get_discussions_by_feed)
         (get_discussions_by_blog)
         (get_discussions_by_comments)
         (get_discussions_by_promoted)
         (get_replies_by_last_update)
         (get_discussions_by_author_before_date)
         (get_active_votes)
      )

      chain::database& _db;
      std::shared_ptr< hive::plugins::follow::follow_api > _follow_api;
};

DEFINE_API_IMPL( tags_api_impl, get_trending_tags )
{
    FC_ASSERT( false, "Supported by hivemind" );
}

DEFINE_API_IMPL( tags_api_impl, get_tags_used_by_author )
{
    FC_ASSERT( false, "Supported by hivemind" );
}

DEFINE_API_IMPL( tags_api_impl, get_discussion )
{
    FC_ASSERT( false, "Supported by hivemind" );
}

DEFINE_API_IMPL( tags_api_impl, get_content_replies )
{
    FC_ASSERT( false, "Supported by hivemind" );
}

DEFINE_API_IMPL( tags_api_impl, get_post_discussions_by_payout )
{
    FC_ASSERT( false, "Supported by hivemind" );
}

DEFINE_API_IMPL( tags_api_impl, get_comment_discussions_by_payout )
{
    FC_ASSERT( false, "Supported by hivemind" );
}

DEFINE_API_IMPL( tags_api_impl, get_discussions_by_trending )
{
    FC_ASSERT( false, "Supported by hivemind" );
}

DEFINE_API_IMPL( tags_api_impl, get_discussions_by_created )
{
    FC_ASSERT( false, "Supported by hivemind" );
}

DEFINE_API_IMPL( tags_api_impl, get_discussions_by_active )
{
    FC_ASSERT( false, "Supported by hivemind" );
}

DEFINE_API_IMPL( tags_api_impl, get_discussions_by_cashout )
{
    FC_ASSERT( false, "Supported by hivemind" );
}

DEFINE_API_IMPL( tags_api_impl, get_discussions_by_votes )
{
    FC_ASSERT( false, "Supported by hivemind" );
}

DEFINE_API_IMPL( tags_api_impl, get_discussions_by_children )
{
    FC_ASSERT( false, "Supported by hivemind" );
}

DEFINE_API_IMPL( tags_api_impl, get_discussions_by_hot )
{
    FC_ASSERT( false, "Supported by hivemind" );
}

DEFINE_API_IMPL( tags_api_impl, get_discussions_by_feed )
{
    FC_ASSERT( false, "Supported by hivemind" );
}

DEFINE_API_IMPL( tags_api_impl, get_discussions_by_blog )
{
    FC_ASSERT( false, "Supported by hivemind" );
}

DEFINE_API_IMPL( tags_api_impl, get_discussions_by_comments )
{
    FC_ASSERT( false, "Supported by hivemind" );
}

DEFINE_API_IMPL( tags_api_impl, get_discussions_by_promoted )
{
    FC_ASSERT( false, "Supported by hivemind" );
}

DEFINE_API_IMPL( tags_api_impl, get_replies_by_last_update )
{
   FC_ASSERT( false, "Supported by hivemind" );
}

DEFINE_API_IMPL( tags_api_impl, get_discussions_by_author_before_date )
{
    FC_ASSERT( false, "Supported by hivemind" );
}

DEFINE_API_IMPL( tags_api_impl, get_active_votes )
{
    FC_ASSERT( false, "Supported by hivemind" );
}

} // detail

tags_api::tags_api(): my( new detail::tags_api_impl() )
{
   JSON_RPC_REGISTER_API( HIVE_TAGS_API_PLUGIN_NAME );
}

tags_api::~tags_api() {}

DEFINE_READ_APIS( tags_api,
   (get_trending_tags)
   (get_tags_used_by_author)
   (get_discussion)
   (get_content_replies)
   (get_post_discussions_by_payout)
   (get_comment_discussions_by_payout)
   (get_discussions_by_trending)
   (get_discussions_by_created)
   (get_discussions_by_active)
   (get_discussions_by_cashout)
   (get_discussions_by_votes)
   (get_discussions_by_children)
   (get_discussions_by_hot)
   (get_discussions_by_feed)
   (get_discussions_by_blog)
   (get_discussions_by_comments)
   (get_discussions_by_promoted)
   (get_replies_by_last_update)
   (get_discussions_by_author_before_date)
   (get_active_votes)
)

} } } // hive::plugins::tags
