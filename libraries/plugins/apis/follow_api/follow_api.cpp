#include <hive/plugins/follow_api/follow_api_plugin.hpp>
#include <hive/plugins/follow_api/follow_api.hpp>

#include <hive/plugins/follow/follow_objects.hpp>

namespace hive { namespace plugins { namespace follow {

namespace detail {

inline void set_what( vector< follow::follow_type >& what, uint16_t bitmask )
{
   if( bitmask & 1 << follow::blog )
      what.push_back( follow::blog );
   if( bitmask & 1 << follow::ignore )
      what.push_back( follow::ignore );
}

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
   FC_ASSERT( args.limit <= 500, "Cannot retrieve more than 500 feed entries at a time." );

   auto entry_id = args.start_entry_id == 0 ? ~0 : args.start_entry_id;

   get_feed_entries_return result;
   result.feed.reserve( args.limit );

   const auto& feed_idx = _db.get_index< follow::feed_index >().indices().get< follow::by_feed >();
   auto itr = feed_idx.lower_bound( boost::make_tuple( args.account, entry_id ) );

   while( itr != feed_idx.end() && itr->account == args.account && result.feed.size() < args.limit )
   {
      const auto& comment = _db.get( itr->comment );
      feed_entry entry;
      entry.author = _db.get_account(comment.author_id).name;
      entry.permlink = chain::to_string( comment.permlink );
      entry.entry_id = itr->account_feed_id;

      if( itr->first_reblogged_by != account_name_type() )
      {
         entry.reblog_by.reserve( itr->reblogged_by.size() );

         for( const auto& a : itr->reblogged_by )
            entry.reblog_by.push_back(a);

         entry.reblog_on = itr->first_reblogged_on;
      }

      result.feed.push_back( entry );
      ++itr;
   }

   return result;
}

DEFINE_API_IMPL( follow_api_impl, get_feed )
{
   FC_ASSERT( args.limit <= 500, "Cannot retrieve more than 500 feed entries at a time." );

   auto entry_id = args.start_entry_id == 0 ? ~0 : args.start_entry_id;

   get_feed_return result;
   result.feed.reserve( args.limit );

   const auto& feed_idx = _db.get_index< follow::feed_index >().indices().get< follow::by_feed >();
   auto itr = feed_idx.lower_bound( boost::make_tuple( args.account, entry_id ) );

   while( itr != feed_idx.end() && itr->account == args.account && result.feed.size() < args.limit )
   {
      const auto& comment = _db.get( itr->comment );
      comment_feed_entry entry;
      entry.comment = database_api::api_comment_object( comment, _db );
      entry.entry_id = itr->account_feed_id;

      if( itr->first_reblogged_by != account_name_type() )
      {
         entry.reblog_by.reserve( itr->reblogged_by.size() );

         for( const auto& a : itr->reblogged_by )
            entry.reblog_by.push_back( a );

         entry.reblog_on = itr->first_reblogged_on;
      }

      result.feed.push_back( entry );
      ++itr;
   }

   return result;
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
   get_blog_authors_return result;

   const auto& stats_idx = _db.get_index< follow::blog_author_stats_index, follow::by_blogger_guest_count >();
   auto itr = stats_idx.lower_bound( args.blog_account );

   while( itr != stats_idx.end() && itr->blogger == args.blog_account && result.blog_authors.size() < 2000 )
   {
      result.blog_authors.push_back( reblog_count{ itr->guest, itr->count } );
      ++itr;
   }

   return result;
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
