#pragma once
#include <hive/chain/hive_fwd.hpp>

#include <hive/protocol/authority.hpp>
#include <hive/protocol/hive_operations.hpp>

#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/witness_objects.hpp>

#include <hive/chain/util/tiny_asset.hpp>

#define HIVE_ROOT_POST_PARENT_ID hive::chain::account_id_type::null_id()

namespace hive { namespace chain {

   using protocol::beneficiary_route_type;
   using chainbase::t_vector;
   using chainbase::t_pair;
#ifdef HIVE_ENABLE_SMT
   using protocol::votable_asset_info;
#endif

   struct strcmp_less
   {
      bool operator()( const shared_string& a, const shared_string& b )const
      {
         return less( a.c_str(), b.c_str() );
      }

#ifndef ENABLE_MIRA
      bool operator()( const shared_string& a, const string& b )const
      {
         return less( a.c_str(), b.c_str() );
      }

      bool operator()( const string& a, const shared_string& b )const
      {
         return less( a.c_str(), b.c_str() );
      }
#endif

      private:
         inline bool less( const char* a, const char* b )const
         {
            return std::strcmp( a, b ) < 0;
         }
   };

   class comment_object : public object< comment_object_type, comment_object >
   {
      CHAINBASE_OBJECT( comment_object );
      public:
         CHAINBASE_DEFAULT_CONSTRUCTOR( comment_object, (category)(parent_permlink)(permlink) )

         shared_string     category;
         account_id_type   parent_author_id;
         shared_string     parent_permlink;
         account_id_type   author_id;
         shared_string     permlink;

         time_point_sec    last_update;
         time_point_sec    created;
         uint16_t          depth = 0;

         comment_id_type   root_comment;

         bool              allow_replies = true;      /// allows a post to disable replies.
   };

   /*
      Helper class related to `comment_object` - members needed for payout calculation.

      Objects of this class can be removed, it depends on `cashout_time`
      when `cashout_time == fc::time_point_sec::maximum()`
   */
   class comment_cashout_object : public object< comment_cashout_object_type, comment_cashout_object >
   {
      CHAINBASE_OBJECT( comment_cashout_object );
      public:
         template< typename Allocator >
         comment_cashout_object( allocator< Allocator > a, uint64_t _id,
            const comment_object& _comment, const time_point_sec& _cashout_time, uint16_t _reward_weight = 0 )
            : id( _comment.get_id() ), //note that it is possible because relation is 1->{0,1} so we can share id
            active( _comment.created ), last_payout( time_point_sec::min() ), cashout_time( _cashout_time ),
            max_cashout_time( time_point_sec::maximum() ), reward_weight( _reward_weight ), beneficiaries( a )
#ifdef HIVE_ENABLE_SMT
            , allowed_vote_assets( a )
#endif
         {}

         //returns id of associated comment
         comment_id_type get_comment_id() const { return comment_object::id_type( id ); }

         time_point_sec    active; ///< the last time this post was "touched" by voting or reply
         time_point_sec    last_payout;

         uint32_t          children = 0; ///< used to track the total number of children, grandchildren, etc...

         /// index on pending_payout for "things happning now... needs moderation"
         /// TRENDING = UNCLAIMED + PENDING
         share_type        net_rshares; // reward is proportional to rshares^2, this is the sum of all votes (positive and negative)
         share_type        abs_rshares; /// this is used to track the total abs(weight) of votes for the purpose of calculating cashout_time
         share_type        vote_rshares; /// Total positive rshares from all votes. Used to calculate delta weights. Needed to handle vote changing and removal.

         share_type        children_abs_rshares; /// this is used to calculate cashout time of a discussion.
         time_point_sec    cashout_time; /// 24 hours from the weighted average of vote time
         time_point_sec    max_cashout_time;
         uint64_t          total_vote_weight = 0; /// the total weight of voting rewards, used to calculate pro-rata share of curation payouts

         uint16_t          reward_weight = 0;

         /** tracks the total payout this comment has received over time, measured in HBD */
         HBD_asset         total_payout_value = asset(0, HBD_SYMBOL);
         HBD_asset         curator_payout_value = asset(0, HBD_SYMBOL);
         HBD_asset         beneficiary_payout_value = asset( 0, HBD_SYMBOL );

         share_type        author_rewards = 0;

         int32_t           net_votes = 0;

         HBD_asset         max_accepted_payout = asset( 1000000000, HBD_SYMBOL );       /// HBD value of the maximum payout this post will receive
         uint16_t          percent_hbd = HIVE_100_PERCENT; /// the percent of HBD to key, unkept amounts will be received as VESTS
         bool              allow_votes   = true;      /// allows a post to receive votes;
         bool              allow_curation_rewards = true;

         using t_beneficiaries = t_vector< beneficiary_route_type >;
         t_beneficiaries   beneficiaries;
#ifdef HIVE_ENABLE_SMT
         using t_votable_assets = t_vector< t_pair< asset_symbol_type, votable_asset_info > >;
         t_votable_assets  allowed_vote_assets;
#endif
   };

   class comment_content_object : public object< comment_content_object_type, comment_content_object >
   {
      CHAINBASE_OBJECT( comment_content_object );
      public:
         CHAINBASE_DEFAULT_CONSTRUCTOR( comment_content_object, (title)(body)(json_metadata) )

         comment_id_type   comment;

         shared_string     title;
         shared_string     body;
         shared_string     json_metadata;
   };

   /**
    * This index maintains the set of voter/comment pairs that have been used, voters cannot
    * vote on the same comment more than once per payout period.
    */
   class comment_vote_object : public object< comment_vote_object_type, comment_vote_object>
   {
      CHAINBASE_OBJECT( comment_vote_object );
      public:
         CHAINBASE_DEFAULT_CONSTRUCTOR( comment_vote_object )

         account_id_type   voter;
         comment_id_type   comment;
         uint64_t          weight = 0; ///< defines the score this vote receives, used by vote payout calc. 0 if a negative vote or changed votes.
         int64_t           rshares = 0; ///< The number of rshares this vote is responsible for
         int16_t           vote_percent = 0; ///< The percent weight of the vote
         time_point_sec    last_update; ///< The time of the last update of the vote
         int8_t            num_changes = 0;
   };

   struct by_comment_voter;
   struct by_voter_comment;
   typedef multi_index_container<
      comment_vote_object,
      indexed_by<
         ordered_unique< tag< by_id >,
            const_mem_fun< comment_vote_object, comment_vote_object::id_type, &comment_vote_object::get_id > >,
         ordered_unique< tag< by_comment_voter >,
            composite_key< comment_vote_object,
               member< comment_vote_object, comment_id_type, &comment_vote_object::comment>,
               member< comment_vote_object, account_id_type, &comment_vote_object::voter>
            >
         >,
         ordered_unique< tag< by_voter_comment >,
            composite_key< comment_vote_object,
               member< comment_vote_object, account_id_type, &comment_vote_object::voter>,
               member< comment_vote_object, comment_id_type, &comment_vote_object::comment>
            >
         >
      >,
      allocator< comment_vote_object >
   > comment_vote_index;


   struct by_permlink; /// author, perm
   struct by_root;
   struct by_parent;
   struct by_last_update; /// parent_auth, last_update
   struct by_author_last_update;

   /**
    * @ingroup object_index
    */
   typedef multi_index_container<
      comment_object,
      indexed_by<
         /// CONSENSUS INDICES - used by evaluators
         ordered_unique< tag< by_id >,
            const_mem_fun< comment_object, comment_object::id_type, &comment_object::get_id > >,
         ordered_unique< tag< by_permlink >, /// used by consensus to find posts referenced in ops
            composite_key< comment_object,
               member< comment_object, account_id_type, &comment_object::author_id >,
               member< comment_object, shared_string, &comment_object::permlink >
            >,
            composite_key_compare< std::less< account_id_type >, strcmp_less >
         >,
         ordered_unique< tag< by_root >,
            composite_key< comment_object,
               member< comment_object, comment_id_type, &comment_object::root_comment >,
               const_mem_fun< comment_object, comment_object::id_type, &comment_object::get_id >
            >
         >,
         ordered_unique< tag< by_parent >, /// used by consensus to find posts referenced in ops
            composite_key< comment_object,
               member< comment_object, account_id_type, &comment_object::parent_author_id >,
               member< comment_object, shared_string, &comment_object::parent_permlink >,
               const_mem_fun< comment_object, comment_object::id_type, &comment_object::get_id >
            >,
            composite_key_compare< std::less< account_id_type >, strcmp_less, std::less< comment_id_type > >
         >
         /// NON_CONSENSUS INDICIES - used by APIs
#ifndef IS_LOW_MEM
         ,
         ordered_unique< tag< by_last_update >,
            composite_key< comment_object,
               member< comment_object, account_id_type, &comment_object::parent_author_id >,
               member< comment_object, time_point_sec, &comment_object::last_update >,
               const_mem_fun< comment_object, comment_object::id_type, &comment_object::get_id >
            >,
            composite_key_compare< std::less< account_id_type >, std::greater< time_point_sec >, std::less< comment_id_type > >
         >,
         ordered_unique< tag< by_author_last_update >,
            composite_key< comment_object,
               member< comment_object, account_id_type, &comment_object::author_id >,
               member< comment_object, time_point_sec, &comment_object::last_update >,
               const_mem_fun< comment_object, comment_object::id_type, &comment_object::get_id >
            >,
            composite_key_compare< std::less< account_id_type >, std::greater< time_point_sec >, std::less< comment_id_type > >
         >
#endif
      >,
      allocator< comment_object >
   > comment_index;

   struct by_cashout_time; /// cashout_time

   typedef multi_index_container<
      comment_cashout_object,
      indexed_by<
         ordered_unique< tag< by_id >,
            const_mem_fun< comment_cashout_object, comment_cashout_object::id_type, &comment_cashout_object::get_id > >,
         ordered_unique< tag< by_cashout_time >,
            composite_key< comment_cashout_object,
               member< comment_cashout_object, time_point_sec, &comment_cashout_object::cashout_time>,
               const_mem_fun< comment_cashout_object, comment_cashout_object::id_type, &comment_cashout_object::get_id >
            >
         >
      >,
      allocator< comment_cashout_object >
   > comment_cashout_index;

   struct by_comment;

   typedef multi_index_container<
      comment_content_object,
      indexed_by<
         ordered_unique< tag< by_id >,
            const_mem_fun< comment_content_object, comment_content_object::id_type, &comment_content_object::get_id > >,
         ordered_unique< tag< by_comment >,
            member< comment_content_object, comment_id_type, &comment_content_object::comment > >
      >,
      allocator< comment_content_object >
   > comment_content_index;

} } // hive::chain

#ifdef ENABLE_MIRA
namespace mira {

template<> struct is_static_length< hive::chain::comment_vote_object > : public boost::true_type {};

} // mira
#endif

FC_REFLECT( hive::chain::comment_object,
             (id)(category)
             (parent_author_id)(parent_permlink)(author_id)(permlink)
             (last_update)(created)(depth)(root_comment)(allow_replies)
          )

CHAINBASE_SET_INDEX_TYPE( hive::chain::comment_object, hive::chain::comment_index )

FC_REFLECT( hive::chain::comment_cashout_object,
             (id)
             (active)(last_payout)
             (children)
             (net_rshares)(abs_rshares)(vote_rshares)
             (children_abs_rshares)(cashout_time)(max_cashout_time)
             (total_vote_weight)(reward_weight)(total_payout_value)(curator_payout_value)(beneficiary_payout_value)
             (author_rewards)(net_votes)
             (max_accepted_payout)(percent_hbd)(allow_votes)(allow_curation_rewards)
             (beneficiaries)
#ifdef HIVE_ENABLE_SMT
             (allowed_vote_assets)
#endif
          )

CHAINBASE_SET_INDEX_TYPE( hive::chain::comment_cashout_object, hive::chain::comment_cashout_index )

FC_REFLECT( hive::chain::comment_content_object,
            (id)(comment)(title)(body)(json_metadata) )
CHAINBASE_SET_INDEX_TYPE( hive::chain::comment_content_object, hive::chain::comment_content_index )

FC_REFLECT( hive::chain::comment_vote_object,
             (id)(voter)(comment)(weight)(rshares)(vote_percent)(last_update)(num_changes)
          )
CHAINBASE_SET_INDEX_TYPE( hive::chain::comment_vote_object, hive::chain::comment_vote_index )

namespace helpers
{
   using hive::chain::shared_string;

   template <>
   class index_statistic_provider<hive::chain::comment_index>
   {
   public:
      typedef hive::chain::comment_index IndexType;
      index_statistic_info gather_statistics(const IndexType& index, bool onlyStaticInfo) const
      {
         index_statistic_info info;
         gather_index_static_data(index, &info);

         if(onlyStaticInfo == false)
         {
            for(const auto& o : index)
            {
               info._item_additional_allocation += o.category.capacity()*sizeof(shared_string::value_type);
               info._item_additional_allocation += o.parent_permlink.capacity()*sizeof(shared_string::value_type);
               info._item_additional_allocation += o.permlink.capacity()*sizeof(shared_string::value_type);
            }
         }

         return info;
      }
   };

   template <>
   class index_statistic_provider<hive::chain::comment_cashout_index>
   {
   public:
      typedef hive::chain::comment_cashout_index IndexType;
      typedef typename hive::chain::comment_cashout_object::t_beneficiaries t_beneficiaries;
#ifdef HIVE_ENABLE_SMT
      typedef typename hive::chain::comment_cashout_object::t_votable_assets t_votable_assets;
#endif
      index_statistic_info gather_statistics(const IndexType& index, bool onlyStaticInfo) const
      {
         index_statistic_info info;
         gather_index_static_data(index, &info);

         if(onlyStaticInfo == false)
         {
            for(const auto& o : index)
            {
               info._item_additional_allocation += o.beneficiaries.capacity()*sizeof(t_beneficiaries::value_type);
#ifdef HIVE_ENABLE_SMT
               info._item_additional_allocation += o.allowed_vote_assets.capacity()*sizeof(t_votable_assets::value_type);
#endif
            }
         }

         return info;
      }
   };

   template <>
   class index_statistic_provider<hive::chain::comment_content_index>
   {
   public:
      typedef hive::chain::comment_content_index IndexType;

      index_statistic_info gather_statistics(const IndexType& index, bool onlyStaticInfo) const
      {
         index_statistic_info info;
         gather_index_static_data(index, &info);

         if(onlyStaticInfo == false)
         {
            for(const auto& o : index)
            {
               info._item_additional_allocation += o.title.capacity()*sizeof(shared_string::value_type);
               info._item_additional_allocation += o.body.capacity()*sizeof(shared_string::value_type);
               info._item_additional_allocation += o.json_metadata.capacity()*sizeof(shared_string::value_type);
            }
         }

         return info;
      }
   };

} /// namespace helpers