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

      void set_pending_payout( discussion& d );
      void set_url( discussion& d );
      discussion lookup_discussion( chain::comment_id_type, uint32_t truncate_body = 0 );

      static bool filter_default( const database_api::api_comment_object& c ) { return false; }
      static bool exit_default( const database_api::api_comment_object& c )   { return false; }
      static bool tag_exit_default( const tags::tag_object& c )               { return false; }

      template<typename Index, typename StartItr>
      discussion_query_result get_discussions( const discussion_query& q,
                                               const string& tag,
                                               chain::comment_id_type parent,
                                               const Index& idx, StartItr itr,
                                               uint32_t truncate_body = 0,
                                               const std::function< bool( const database_api::api_comment_object& ) >& filter = &tags_api_impl::filter_default,
                                               const std::function< bool( const database_api::api_comment_object& ) >& exit   = &tags_api_impl::exit_default,
                                               const std::function< bool( const tags::tag_object& ) >& tag_exit               = &tags_api_impl::tag_exit_default,
                                               bool ignore_parent = false
                                               );

      chain::comment_id_type get_parent( const discussion_query& q );

      chain::database& _db;
      std::shared_ptr< hive::plugins::follow::follow_api > _follow_api;
};

DEFINE_API_IMPL( tags_api_impl, get_trending_tags )
{
   FC_ASSERT( args.limit <= 1000, "Cannot retrieve more than 1000 tags at a time." );
   get_trending_tags_return result;
   result.tags.reserve( args.limit );

   const auto& nidx = _db.get_index< tags::tag_stats_index, tags::by_tag >();

   const auto& ridx = _db.get_index< tags::tag_stats_index, tags::by_trending >();
   auto itr = ridx.begin();
   if( args.start_tag != "" && nidx.size() )
   {
      auto nitr = nidx.lower_bound( args.start_tag );
      if( nitr == nidx.end() )
         itr = ridx.end();
      else
         itr = ridx.iterator_to( *nitr );
   }

   while( itr != ridx.end() && result.tags.size() < args.limit )
   {
      result.tags.push_back( api_tag_object( *itr ) );
      ++itr;
   }
   return result;
}

DEFINE_API_IMPL( tags_api_impl, get_tags_used_by_author )
{
   const auto* acnt = _db.find_account( args.author );
   FC_ASSERT( acnt != nullptr, "Author not found" );

   const auto& tidx = _db.get_index< tags::author_tag_stats_index, tags::by_author_posts_tag >();
   auto itr = tidx.lower_bound( boost::make_tuple( acnt->get_id(), 0 ) );

   get_tags_used_by_author_return result;

   while( itr != tidx.end() && itr->author == acnt->get_id() && result.tags.size() < 1000 )
   {
      result.tags.push_back( tag_count_object( { itr->tag, itr->total_posts } ) );
      ++itr;
   }

   return result;
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
   args.validate();
   auto tag = fc::to_lower( args.tag );
   auto parent = get_parent( args );

   const auto& tidx = _db.get_index< tags::tag_index, tags::by_parent_active >();
   auto tidx_itr = tidx.lower_bound( boost::make_tuple( tag, parent, fc::time_point_sec::maximum() )  );

   return get_discussions( args, tag, parent, tidx, tidx_itr, args.truncate_body );
}

DEFINE_API_IMPL( tags_api_impl, get_discussions_by_cashout )
{
   args.validate();
   vector<discussion> result;

   auto tag = fc::to_lower( args.tag );
   auto parent = get_parent( args );

   const auto& tidx = _db.get_index< tags::tag_index, tags::by_cashout >();
   auto tidx_itr = tidx.lower_bound( boost::make_tuple( tag, fc::time_point::now() - fc::minutes( 60 ) ) );

   return get_discussions( args, tag, parent, tidx, tidx_itr, args.truncate_body, []( const database_api::api_comment_object& c ){ return c.net_rshares < 0; });
}

DEFINE_API_IMPL( tags_api_impl, get_discussions_by_votes )
{
   args.validate();
   auto tag = fc::to_lower( args.tag );
   auto parent = get_parent( args );

   const auto& tidx = _db.get_index< tags::tag_index, tags::by_parent_net_votes >();
   auto tidx_itr = tidx.lower_bound( boost::make_tuple( tag, parent, std::numeric_limits< int32_t >::max() )  );

   return get_discussions( args, tag, parent, tidx, tidx_itr, args.truncate_body );
}

DEFINE_API_IMPL( tags_api_impl, get_discussions_by_children )
{
   args.validate();
   auto tag = fc::to_lower( args.tag );
   auto parent = get_parent( args );

   const auto& tidx = _db.get_index< tags::tag_index, tags::by_parent_children >();
   auto tidx_itr = tidx.lower_bound( boost::make_tuple( tag, parent, std::numeric_limits< int32_t >::max() )  );

   return get_discussions( args, tag, parent, tidx, tidx_itr, args.truncate_body );
}

DEFINE_API_IMPL( tags_api_impl, get_discussions_by_hot )
{
    FC_ASSERT( false, "Supported by hivemind" );
}

DEFINE_API_IMPL( tags_api_impl, get_discussions_by_feed )
{
    args.validate();
    FC_ASSERT( _db.has_index< follow::feed_index >(), "Node is not running the follow plugin" );
    auto start_author = args.start_author ? *( args.start_author ) : "";
    auto start_permlink = args.start_permlink ? *( args.start_permlink ) : "";

    const auto& account = _db.get_account( args.tag );

    const auto& c_idx = _db.get_index< follow::feed_index, follow::by_comment >();
    const auto& f_idx = _db.get_index< follow::feed_index, follow::by_feed >();
    auto feed_itr = f_idx.lower_bound( account.name );

    if( start_author.size() || start_permlink.size() )
    {
        auto start_c = c_idx.find( boost::make_tuple( _db.get_comment( start_author, start_permlink ).get_id(), account.name ) );
        FC_ASSERT( start_c != c_idx.end(), "Comment is not in account's feed" );
        feed_itr = f_idx.iterator_to( *start_c );
    }

    get_discussions_by_feed_return result;
    result.discussions.reserve( args.limit );

    while( result.discussions.size() < args.limit && feed_itr != f_idx.end() )
    {
        if( feed_itr->account != account.name )
            break;
        try
        {
            result.discussions.push_back( lookup_discussion( feed_itr->comment ) );
            if( feed_itr->first_reblogged_by != account_name_type() )
            {
                result.discussions.back().reblogged_by = vector<account_name_type>( feed_itr->reblogged_by.begin(), feed_itr->reblogged_by.end() );
                result.discussions.back().first_reblogged_by = feed_itr->first_reblogged_by;
                result.discussions.back().first_reblogged_on = feed_itr->first_reblogged_on;
            }
        }
        catch ( const fc::exception& e )
        {
            edump((e.to_detail_string()));
        }

        ++feed_itr;
    }
    return result;
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
   get_active_votes_return result;
   const auto& comment = _db.get_comment( args.author, args.permlink );
   const auto& idx = _db.get_index< chain::comment_vote_index, chain::by_comment_voter >();
   chain::comment_id_type cid( comment.get_id() );
   auto itr = idx.lower_bound( cid );
   while( itr != idx.end() && itr->comment == cid )
   {
      const auto& vo = _db.get( itr->voter );
      vote_state vstate;
      vstate.voter = vo.name;
      vstate.weight = itr->weight;
      vstate.rshares = itr->rshares;
      vstate.percent = itr->vote_percent;
      vstate.time = itr->last_update;

      if( _follow_api )
      {
         auto reps = _follow_api->get_account_reputations( follow::get_account_reputations_args( { vo.name, 1 } ) ).reputations;
         if( reps.size() )
            vstate.reputation = reps[0].reputation;
      }

      result.votes.push_back( vstate );
      ++itr;
   }
   return result;
}

void tags_api_impl::set_pending_payout( discussion& d )
{
   const auto& cidx = _db.get_index< tags::tag_index, tags::by_comment>();
   auto itr = cidx.lower_bound( d.id );
   if( itr != cidx.end() && itr->comment == d.id )  {
      d.promoted = asset( itr->promoted_balance, HBD_SYMBOL );
   }

   const auto& props = _db.get_dynamic_global_properties();
   const auto& hist  = _db.get_feed_history();

   asset pot;
   if( _db.has_hardfork( HIVE_HARDFORK_0_17__774 ) )
      pot = _db.get_reward_fund().reward_balance;
   else
      pot = props.get_total_reward_fund_hive();

   if( !hist.current_median_history.is_null() ) pot = pot * hist.current_median_history;

   u256 total_r2 = 0;
   if( _db.has_hardfork( HIVE_HARDFORK_0_17__774 ) )
      total_r2 = chain::util::to256( _db.get_reward_fund().recent_claims );
   else
      total_r2 = chain::util::to256( props.total_reward_shares2 );

   if( total_r2 > 0 )
   {
      uint128_t vshares;
      if( _db.has_hardfork( HIVE_HARDFORK_0_17__774 ) )
      {
         const auto& rf = _db.get_reward_fund();
         vshares = d.net_rshares.value > 0 ? chain::util::evaluate_reward_curve( d.net_rshares.value, rf.author_reward_curve, rf.content_constant ) : 0;
      }
      else
         vshares = d.net_rshares.value > 0 ? chain::util::evaluate_reward_curve( d.net_rshares.value ) : 0;

      u256 r2 = chain::util::to256( vshares ); //to256(abs_net_rshares);
      r2 *= pot.amount.value;
      r2 /= total_r2;

      d.pending_payout_value = asset( static_cast<uint64_t>(r2), pot.symbol );

      if( _follow_api )
      {
         d.author_reputation = _follow_api->get_account_reputations( follow::get_account_reputations_args( { d.author, 1} ) ).reputations[0].reputation;
      }
   }

   if( d.parent_author != HIVE_ROOT_POST_PARENT )
      d.cashout_time = _db.calculate_discussion_payout_time( _db.get< chain::comment_object >( d.id ) );

   if( d.body.size() > 1024*128 )
      d.body = "body pruned due to size";
   if( d.parent_author.size() > 0 && d.body.size() > 1024*16 )
      d.body = "comment pruned due to size";

   set_url( d );
}

void tags_api_impl::set_url( discussion& d )
{
   const database_api::api_comment_object root( _db.get_comment( d.root_author, d.root_permlink ), _db );
   d.url = "/" + root.category + "/@" + root.author + "/" + root.permlink;
   d.root_title = root.title;
   if( root.id != d.id )
      d.url += "#@" + d.author + "/" + d.permlink;
}

discussion tags_api_impl::lookup_discussion( chain::comment_id_type id, uint32_t truncate_body )
{
   discussion d( _db.get( id ), _db );
   set_url( d );
   set_pending_payout( d );
   d.active_votes = get_active_votes( get_active_votes_args( { d.author, d.permlink } ) ).votes;
   d.body_length = d.body.size();
   if( truncate_body )
   {
      d.body = d.body.substr( 0, truncate_body );

      if( !fc::is_utf8( d.body ) )
         d.body = fc::prune_invalid_utf8( d.body );
   }
   return d;
}

template<typename Index, typename StartItr>
discussion_query_result tags_api_impl::get_discussions( const discussion_query& query,
                                                        const string& tag,
                                                        chain::comment_id_type parent,
                                                        const Index& tidx, StartItr tidx_itr,
                                                        uint32_t truncate_body,
                                                        const std::function< bool( const database_api::api_comment_object& ) >& filter,
                                                        const std::function< bool( const database_api::api_comment_object& ) >& exit,
                                                        const std::function< bool( const tags::tag_object& ) >& tag_exit,
                                                        bool ignore_parent
                                                        )
{
   discussion_query_result result;

   const auto& cidx = _db.get_index< tags::tag_index, tags::by_comment >();
   chain::comment_id_type start;

   if( query.start_author && query.start_permlink )
   {
      start = _db.get_comment( *query.start_author, *query.start_permlink ).get_id();
      auto itr = cidx.find( start );
      while( itr != cidx.end() && itr->comment == start )
      {
         if( itr->tag == tag )
         {
            tidx_itr = tidx.iterator_to( *itr );
            break;
         }
         ++itr;
      }
   }

   uint32_t count = query.limit;
   uint64_t itr_count = 0;
   uint64_t filter_count = 0;
   uint64_t exc_count = 0;
   uint64_t max_itr_count = 10 * query.limit;
   while( count > 0 && tidx_itr != tidx.end() )
   {
      ++itr_count;
      if( itr_count > max_itr_count )
      {
         wlog( "Maximum iteration count exceeded serving query: ${q}", ("q", query) );
         wlog( "count=${count}   itr_count=${itr_count}   filter_count=${filter_count}   exc_count=${exc_count}",
               ("count", count)("itr_count", itr_count)("filter_count", filter_count)("exc_count", exc_count) );
         break;
      }
      if( tidx_itr->tag != tag || ( !ignore_parent && tidx_itr->parent != parent ) )
         break;
      try
      {
         result.discussions.push_back( lookup_discussion( tidx_itr->comment, truncate_body ) );
         result.discussions.back().promoted = asset(tidx_itr->promoted_balance, HBD_SYMBOL );

         if( filter( result.discussions.back() ) )
         {
            result.discussions.pop_back();
            ++filter_count;
         }
         else if( exit( result.discussions.back() ) || tag_exit( *tidx_itr )  )
         {
            result.discussions.pop_back();
            break;
         }
         else
            --count;
      }
      catch ( const fc::exception& e )
      {
         ++exc_count;
         edump((e.to_detail_string()));
      }
      ++tidx_itr;
   }
   return result;
}

chain::comment_id_type tags_api_impl::get_parent( const discussion_query& query )
{
   chain::comment_id_type parent;
   if( query.parent_author && query.parent_permlink ) {
      parent = _db.get_comment( *query.parent_author, *query.parent_permlink ).get_id();
   }
   return parent;
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

void tags_api::set_pending_payout( discussion& d )
{
   my->set_pending_payout( d );
}

void tags_api::api_startup()
{
   auto follow_api_plugin = appbase::app().find_plugin< hive::plugins::follow::follow_api_plugin >();

   if( follow_api_plugin != nullptr )
      my->_follow_api = follow_api_plugin->api;
}

} } } // hive::plugins::tags
