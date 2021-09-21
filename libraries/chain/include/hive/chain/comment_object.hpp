#pragma once
#include <hive/chain/hive_fwd.hpp>

#include <hive/protocol/authority.hpp>
#include <hive/protocol/hive_operations.hpp>

#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/witness_objects.hpp>
#include <hive/chain/account_object.hpp>

#include <fc/crypto/ripemd160.hpp>

#define HIVE_ROOT_POST_PARENT_ID hive::chain::account_id_type::null_id()

namespace hive { namespace chain {

  using protocol::beneficiary_route_type;
  using chainbase::t_vector;
  using chainbase::t_pair;
#ifdef HIVE_ENABLE_SMT
  using protocol::votable_asset_info;
#endif

  class comment_object : public object< comment_object_type, comment_object >
  {
    CHAINBASE_OBJECT( comment_object );
    public:
      using author_and_permlink_hash_type = fc::ripemd160;

      template< typename Allocator >
      comment_object( allocator< Allocator > a, uint64_t _id,
        const account_object& _author, const std::string& _permlink,
       const comment_object* _parent_comment );

      //returns comment identification hash
      const author_and_permlink_hash_type& get_author_and_permlink_hash() const { return author_and_permlink_hash; }

      static author_and_permlink_hash_type compute_author_and_permlink_hash(
        account_id_type author_account_id, const std::string& permlink );

      //returns id of parent comment (null_id() when top comment)
      comment_id_type get_parent_id() const { return parent_comment; }
      //tells if comment is top comment
      bool is_root() const { return parent_comment == comment_id_type::null_id(); }

      //returns comment depth (distance to root)
      uint16_t get_depth() const { return depth; }

    private:
      comment_id_type parent_comment;

      author_and_permlink_hash_type author_and_permlink_hash; //ABW: splitting it to author_id + 128 bit author+permlink hash would allow for
        //moving it to DB and the DB would only need to keep it cached for active authors; now most likely whole index would land in mem anyway

      uint16_t        depth = 0; //looks like a candidate for removal (see https://github.com/steemit/steem/issues/767 )
        //currently soft limited to HIVE_SOFT_MAX_COMMENT_DEPTH in witness plugin; consensus doesn't need limit, but HiveMind might

      CHAINBASE_UNPACK_CONSTRUCTOR(comment_object);
  };

  template< typename Allocator >
  inline comment_object::comment_object( allocator< Allocator > a, uint64_t _id,
    const account_object& _author, const std::string& _permlink,
    const comment_object* _parent_comment
  )
    : id ( _id )
  {
    author_and_permlink_hash = compute_author_and_permlink_hash( _author.get_id(), _permlink );

    if ( _parent_comment != nullptr )
    {
      parent_comment = _parent_comment->get_id();
      depth = _parent_comment->get_depth() + 1;
    }
    else
    {
      parent_comment = comment_id_type::null_id();
    }
  }

  inline comment_object::author_and_permlink_hash_type comment_object::compute_author_and_permlink_hash(
    account_id_type author_account_id, const std::string& permlink )
  {
    return fc::ripemd160::hash( permlink + "@" + std::to_string( author_account_id ) );
  }
    
  /*
    Helper class related to `comment_object` - members needed for payout calculation.

    After HF19 objects of this class are removed at the end of comment cashout process.
  */
  class comment_cashout_object : public object< comment_cashout_object_type, comment_cashout_object >
  {
    CHAINBASE_OBJECT( comment_cashout_object );
    public:
      //helper structure to hold comment beneficiaries
      struct stored_beneficiary_route_type
      {
        stored_beneficiary_route_type() {}
        stored_beneficiary_route_type( const account_object& a, const uint16_t& w )
          : account_id( a.get_id() ), weight( w )
        {}

        account_id_type account_id;
        uint16_t        weight;
      };
      using t_beneficiaries = t_vector< stored_beneficiary_route_type >;
#ifdef HIVE_ENABLE_SMT
      using t_votable_assets = t_vector< t_pair< asset_symbol_type, votable_asset_info > >;
#endif

      template< typename Allocator >
      comment_cashout_object( allocator< Allocator > a, uint64_t _id,
        const comment_object& _comment, const account_object& _author, const std::string& _permlink,
        const time_point_sec& _creation_time, const time_point_sec& _cashout_time )
      : id( _comment.get_id() ), //note that it is possible because relation is 1->{0,1} so we can share id
        author_id( _author.get_id() ), permlink( a ), active( _creation_time ),
        created( _creation_time ), cashout_time( _cashout_time ), beneficiaries( a )
#ifdef HIVE_ENABLE_SMT
        , allowed_vote_assets( a )
#endif
      {
        from_string( permlink, _permlink );
        FC_ASSERT( _creation_time <= _cashout_time );
      }

      void configure_options( uint16_t _percent_hbd, const HBD_asset& _max_accepted_payout, bool _allow_votes, bool _allow_curation )
      {
        percent_hbd = _percent_hbd;
        max_accepted_payout = _max_accepted_payout;
        allow_votes = _allow_votes;
        allow_curation_rewards = _allow_curation;
      }
      void add_beneficiary( const account_object& beneficiary, uint16_t weight )
      {
        beneficiaries.emplace_back( beneficiary, weight );
      }

      //returns id of associated comment
      comment_id_type get_comment_id() const { return comment_object::id_type( id ); }
      //returns id of author of associated comment
      account_id_type get_author_id() const { return author_id; }
      //returns permlink of associated comment
      const shared_string& get_permlink() const { return permlink; }

      //returns creation time
      time_point_sec get_creation_time() const { return created; }
      //returns time of next cashout (since HF17 there is only one cashout)
      time_point_sec get_cashout_time() const { return cashout_time; }
      //returns time of last reply (or its deletion, creation time if no replies)
      time_point_sec get_last_activity_time() const { return active; }

      //returns max payout configured for related comment
      const HBD_asset& get_max_accepted_payout() const { return max_accepted_payout; }
      //returns configured percent of reward (in basis points) that can be paid in HBD to be actually paid in HBD
      uint16_t get_percent_hbd() const { return percent_hbd; }
      //returns list of weighted accounts to receive part of comment author reward
      const t_beneficiaries& get_beneficiaries() const { return beneficiaries; }

      //tells if voting is allowed
      bool allows_votes() const { return allow_votes; }
      //tells if curation rewards are allowed
      bool allows_curation_rewards() const { return allow_curation_rewards; }
      //tells if associated comment was voted on
      bool has_votes() const { return was_voted_on; }
      //tells if associated comment has replies
      bool has_replies() const { return children > 0; }
      //returns number of direct replies
      uint32_t get_number_of_replies() const { return children; }
      //returns net vote count (upvotes - downvotes)
      int32_t get_net_votes() const { return net_votes; }

      //returns sum of all rshares from votes
      int64_t get_net_rshares() const { return net_rshares.value; }
      //returns sum of all rshares from upvotes
      int64_t get_vote_rshares() const { return vote_rshares.value; }
      //returns accumulated weight of all votes for related post
      uint64_t get_total_vote_weight() const { return total_vote_weight; }

      //called on vote (new or edit)
      void on_vote( int16_t vote_percent, int16_t old_vote_percent = 0, bool real_vote = true )
      {
        FC_TODO( "After HF26 try to make it unconditional" );
        if( real_vote )
          was_voted_on = true;
        if( old_vote_percent > 0 )
          --net_votes;
        else if( old_vote_percent < 0 )
          ++net_votes;
        if( vote_percent > 0 )
          ++net_votes;
        else if( vote_percent < 0 )
          --net_votes;
      }
      //acumulates rshares of a vote
      void accumulate_vote_rshares( int64_t _net_rshares, int64_t _positive_rshares )
      {
        net_rshares += _net_rshares;
        vote_rshares += _positive_rshares;
      }
      //accumulates weight of the vote used later to split curation rewards
      void accumulate_vote_weight( int64_t vote_weight )
      {
        total_vote_weight += vote_weight;
      }
      //called on reply (including deleting of existing one)
      void on_reply( const time_point_sec& _reply_time, bool _deleting = false )
      {
        active = _reply_time;
        if( _deleting )
          --children;
        else
          ++children;
      }
      //called when comment is edited
      void on_edit( const time_point_sec& _edit_time )
      {
        active = _edit_time;
      }
      //called when related comment is paid
      void on_payout()
      {
        net_rshares = 0;
        vote_rshares = 0;
        total_vote_weight = 0;
      }

      //sets time of next cashout (default value disables cashout)
      void set_cashout_time( const time_point_sec& next_cashout = time_point_sec::maximum() )
      {
        cashout_time = next_cashout;
      }

    private:
      account_id_type   author_id;
      shared_string     permlink;

      time_point_sec    active; // only used by APIs (rules for that value are very different in Hivemind)
      int32_t           net_votes = 0; // only used by APIs

      share_type        net_rshares; // sum of all rshares from votes. Influences comment weight and therefore reward payout
      share_type        vote_rshares; // sum of all rshares from new upvotes. Influences vote weights and therefore allotment of curation rewards

      uint32_t          children = 0; // tracks number of direct replies

      time_point_sec    created;
      time_point_sec    cashout_time; // since HF17 it is 7 days after creation time, earlier rules were more complex

      uint16_t          percent_hbd = HIVE_100_PERCENT; /// the percent of HBD to key, unkept amounts will be received as VESTS
      uint64_t          total_vote_weight = 0; /// the total weight of voting rewards, used to calculate pro-rata share of curation payouts

      HBD_asset         max_accepted_payout = asset( 1000000000, HBD_SYMBOL ); /// HBD value of the maximum payout this post will receive

      bool              allow_votes = true;
      bool              allow_curation_rewards = true;
      bool              was_voted_on = false;

      t_beneficiaries   beneficiaries;

#ifdef HIVE_ENABLE_SMT
    public:
      t_votable_assets  allowed_vote_assets;

      CHAINBASE_UNPACK_CONSTRUCTOR(comment_cashout_object, (permlink)(beneficiaries)(allowed_vote_assets));
#else
      CHAINBASE_UNPACK_CONSTRUCTOR(comment_cashout_object, (permlink)(beneficiaries));
#endif
  };

  /*
    Similar to `comment_cashout_object` - extra members needed for payout calculation pre HF19.

    Objects of this class are no longer produced after HF19. It also means related indexes are
    no longer taking space. For the most part it is only used up to HF17, with exception of last_payout
  */
  class comment_cashout_ex_object : public object< comment_cashout_ex_object_type, comment_cashout_ex_object >
  {
    CHAINBASE_OBJECT( comment_cashout_ex_object );
    public:
      template< typename Allocator >
      comment_cashout_ex_object( allocator< Allocator > a, uint64_t _id,
        const comment_object& _comment, fc::optional< std::reference_wrapper< const comment_cashout_ex_object > > _parent_comment,
        uint16_t _reward_weight )
      : id( _comment.get_id() ), //note that it is possible because relation is 1->{0,1} so we can share id
        last_payout( time_point_sec::min() ), max_cashout_time( time_point_sec::maximum() ),
        reward_weight( _reward_weight )
      {
        if( _parent_comment.valid() )
          root_comment = ( *_parent_comment ).get().get_root_id();
        else
          root_comment = get_comment_id();
      }

      //returns id of associated comment
      comment_id_type get_comment_id() const { return comment_object::id_type( id ); }
      //returns id of root comment
      comment_id_type get_root_id() const { return root_comment; }
      //tells if this cashout object is related to root comment
      bool is_root() const { return root_comment == get_comment_id(); }

      //returns time of previous payout (or min if comment was not yet cashed out)
      time_point_sec get_last_payout() const { return last_payout; }
      //tells if associated comment was paid
      bool was_paid() const { return last_payout > time_point_sec::min(); }

      //returns accumulated abs_rshares value for related comment
      int64_t get_abs_rshares() const { return abs_rshares.value; }
      //returns accumulated abs_rshares value for discussion
      int64_t get_children_abs_rshares() const { return children_abs_rshares.value; }

      //returns maximum time to next cashout (up to HF17 actual cashout time depended on whole discussion)
      time_point_sec get_max_cashout_time() const { return max_cashout_time; }

      //returns accumulated value of payouts across potentially multiple cashout windows
      const HBD_asset& get_total_payout() const { return total_payout_value; }
      //returns accumulated value of payouts to curators
      const HBD_asset& get_curator_payout() const { return curator_payout_value; }
      //returns accumulated value of payouts to beneficiaries
      const HBD_asset& get_beneficiary_payout() const { return beneficiary_payout_value; }

      //returns percentage of max post reward allowed for related post (in basis points)
      uint16_t get_reward_weight() const { return reward_weight; }

      //accumulates abs_rshares value from vote on related comment
      void accumulate_abs_rshares( int64_t _abs_rshares )
      {
        abs_rshares += _abs_rshares;
      }
      //accumulates abs_rshares value from vote in discussion
      void accumulate_children_abs_rshares( int64_t _abs_rshares )
      {
        children_abs_rshares += _abs_rshares;
      }
      //limits maximum cashout time
      void set_max_cashout_time( time_point_sec new_max )
      {
        max_cashout_time = new_max;
      }
      //accumulates payout values (on successful payout)
      void add_payout_values( const HBD_asset& payout, const HBD_asset& curator_payout, const HBD_asset& beneficiary_payout )
      {
        total_payout_value += payout;
        curator_payout_value += curator_payout;
        beneficiary_payout_value += beneficiary_payout;
      }
      //sets payout time for associated comment and resets some fields on payout
      void on_payout( time_point_sec payout_time )
      {
        last_payout = payout_time;
        abs_rshares = 0;
        children_abs_rshares = 0;
        max_cashout_time = time_point_sec::maximum();
      }

    private:
      comment_id_type root_comment;

      time_point_sec  last_payout;
      time_point_sec  max_cashout_time;
      share_type      abs_rshares; // only used for very early comments
      share_type      children_abs_rshares; /// used to calculate cashout time of a discussion

      HBD_asset       total_payout_value;
      HBD_asset       curator_payout_value;
      HBD_asset       beneficiary_payout_value;

      uint16_t        reward_weight; // before HF17 posts received less reward when they were more frequent than assumed average

      CHAINBASE_UNPACK_CONSTRUCTOR(comment_cashout_ex_object);
  };

  /**
    * This index maintains the set of voter/comment pairs that have been used, voters cannot
    * vote on the same comment more than once per payout period.
    */
  class comment_vote_object : public object< comment_vote_object_type, comment_vote_object>
  {
    CHAINBASE_OBJECT( comment_vote_object );
    public:
      template< typename Allocator >
      comment_vote_object( allocator< Allocator > a, uint64_t _id,
        const account_object& _voter, const comment_object& _comment,
        const time_point_sec& _creation_time, int16_t _vote_percent, uint64_t _weight, int64_t _rshares )
      : id( _id ), voter( _voter.get_id() ), comment( _comment.get_id() ), weight( _weight ),
        rshares( _rshares ), vote_percent( _vote_percent ), last_update( _creation_time )
      {}

      void set( const time_point_sec& _edit_time, int16_t _vote_percent, uint64_t _weight, int64_t _rshares )
      {
        last_update = _edit_time;
        vote_percent = _vote_percent;
        weight = _weight;
        rshares = _rshares;
        ++num_changes;
      }

      account_id_type get_voter() const { return voter; }
      comment_id_type get_comment() const { return comment; }
      uint64_t        get_weight() const { return weight; }
      int64_t         get_rshares() const { return rshares; }
      int16_t         get_vote_percent() const { return vote_percent; }
      time_point_sec  get_last_update() const { return last_update; }
      int8_t          get_number_of_changes() const { return num_changes; }

    private:
      account_id_type   voter;
      comment_id_type   comment;
      uint64_t          weight = 0; ///< defines the score this vote receives, used by vote payout calc. 0 if a negative vote or changed votes.
      int64_t           rshares = 0; ///< The number of rshares this vote is responsible for
      int16_t           vote_percent = 0; ///< The percent weight of the vote
      time_point_sec    last_update; ///< The time of the last update of the vote
      int8_t            num_changes = 0;

    CHAINBASE_UNPACK_CONSTRUCTOR(comment_vote_object);
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
          const_mem_fun< comment_vote_object, comment_id_type, &comment_vote_object::get_comment>,
          const_mem_fun< comment_vote_object, account_id_type, &comment_vote_object::get_voter>
        >
      >,
      ordered_unique< tag< by_voter_comment >,
        composite_key< comment_vote_object,
          const_mem_fun< comment_vote_object, account_id_type, &comment_vote_object::get_voter>,
          const_mem_fun< comment_vote_object, comment_id_type, &comment_vote_object::get_comment>
        >
      >
    >,
    allocator< comment_vote_object >
  > comment_vote_index;


  struct by_permlink; /// author, perm
  
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
        const_mem_fun< comment_object, const comment_object::author_and_permlink_hash_type&, &comment_object::get_author_and_permlink_hash > >
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
          const_mem_fun< comment_cashout_object, time_point_sec, &comment_cashout_object::get_cashout_time>,
          const_mem_fun< comment_cashout_object, comment_cashout_object::id_type, &comment_cashout_object::get_id >
        >
      >
    >,
    allocator< comment_cashout_object >
  > comment_cashout_index;

  struct by_root;

  /// This is empty after HF19
  typedef multi_index_container<
    comment_cashout_ex_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< comment_cashout_ex_object, comment_cashout_ex_object::id_type, &comment_cashout_ex_object::get_id > >,
      ordered_unique< tag< by_root >,
        composite_key< comment_cashout_ex_object,
          const_mem_fun< comment_cashout_ex_object, comment_id_type, &comment_cashout_ex_object::get_root_id >,
          const_mem_fun< comment_cashout_ex_object, comment_cashout_ex_object::id_type, &comment_cashout_ex_object::get_id >
        >
      >
    >,
    allocator< comment_cashout_ex_object >
  > comment_cashout_ex_index;

} } // hive::chain

FC_REFLECT( hive::chain::comment_object,
          (id)(parent_comment)
          (author_and_permlink_hash)(depth)
        )

CHAINBASE_SET_INDEX_TYPE( hive::chain::comment_object, hive::chain::comment_index )

FC_REFLECT( hive::chain::comment_cashout_object::stored_beneficiary_route_type,
            (account_id)(weight)
          )

FC_REFLECT( hive::chain::comment_cashout_object,
          (id)(author_id)(permlink)
          (active)(net_votes)
          (net_rshares)(vote_rshares)
          (children)(created)(cashout_time)
          (percent_hbd)(total_vote_weight)
          (max_accepted_payout)(allow_votes)(allow_curation_rewards)(was_voted_on)
          (beneficiaries)
#ifdef HIVE_ENABLE_SMT
          (allowed_vote_assets)
#endif
        )

CHAINBASE_SET_INDEX_TYPE( hive::chain::comment_cashout_object, hive::chain::comment_cashout_index )

FC_REFLECT( hive::chain::comment_cashout_ex_object,
           (id)(root_comment)
           (last_payout)(max_cashout_time)
           (abs_rshares)(children_abs_rshares)
           (total_payout_value)(curator_payout_value)(beneficiary_payout_value)
           (reward_weight)
          )

CHAINBASE_SET_INDEX_TYPE( hive::chain::comment_cashout_ex_object, hive::chain::comment_cashout_ex_index )

FC_REFLECT( hive::chain::comment_vote_object,
          (id)(voter)(comment)(weight)(rshares)(vote_percent)(last_update)(num_changes)
        )
CHAINBASE_SET_INDEX_TYPE( hive::chain::comment_vote_object, hive::chain::comment_vote_index )

namespace helpers
{
  using hive::chain::shared_string;

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
          info._item_additional_allocation += o.get_permlink().capacity()*sizeof(shared_string::value_type);
          info._item_additional_allocation += o.get_beneficiaries().capacity()*sizeof(t_beneficiaries::value_type);
#ifdef HIVE_ENABLE_SMT
          info._item_additional_allocation += o.allowed_vote_assets.capacity()*sizeof(t_votable_assets::value_type);
#endif
        }
      }

      return info;
    }
  };

} /// namespace helpers
