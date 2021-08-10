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
        fc::optional< std::reference_wrapper< const comment_object > > _parent_comment );

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
    fc::optional< std::reference_wrapper< const comment_object > > _parent_comment
  )
    : id ( _id )
  {
    author_and_permlink_hash = compute_author_and_permlink_hash( _author.get_id(), _permlink );

    if ( _parent_comment.valid() )
    {
      parent_comment = ( *_parent_comment ).get().get_id();
      depth = ( *_parent_comment ).get().get_depth() + 1;
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
      template< typename Allocator >
      comment_cashout_object( allocator< Allocator > a, uint64_t _id,
        const comment_object& _comment, const account_object& _author, const std::string& _permlink,
        const time_point_sec& _creation_time, const time_point_sec& _cashout_time, uint16_t _reward_weight = 0 )
      : id( _comment.get_id() ), //note that it is possible because relation is 1->{0,1} so we can share id
        author_id( _author.get_id() ), permlink( a ), active( _creation_time ),
        created( _creation_time ), cashout_time( _cashout_time ),
        reward_weight( _reward_weight ), beneficiaries( a )
#ifdef HIVE_ENABLE_SMT
        , allowed_vote_assets( a )
#endif
      {
        from_string( permlink, _permlink );
        FC_ASSERT( _creation_time <= _cashout_time );
      }

      //returns id of associated comment
      comment_id_type get_comment_id() const { return comment_object::id_type( id ); }

      //returns creation time
      const time_point_sec& get_creation_time() const { return created; }

      account_id_type   author_id;
      shared_string     permlink;

      time_point_sec    active; ///< the last time this post was "touched" by voting or reply
      int32_t           net_votes = 0;

      /// index on pending_payout for "things happning now... needs moderation"
      /// TRENDING = UNCLAIMED + PENDING
      share_type        net_rshares; // reward is proportional to rshares^2, this is the sum of all votes (positive and negative)
      share_type        abs_rshares; /// this is used to track the total abs(weight) of votes for the purpose of calculating cashout_time
      share_type        vote_rshares; /// Total positive rshares from all votes. Used to calculate delta weights. Needed to handle vote changing and removal.

      uint32_t          children = 0; ///< used to track the total number of children, grandchildren, etc...

    private:
      time_point_sec    created;
    public:
      time_point_sec    cashout_time; /// 24 hours from the weighted average of vote time
      uint16_t          reward_weight = 0;
      uint16_t          percent_hbd = HIVE_100_PERCENT; /// the percent of HBD to key, unkept amounts will be received as VESTS

      uint64_t          total_vote_weight = 0; /// the total weight of voting rewards, used to calculate pro-rata share of curation payouts

      HBD_asset         max_accepted_payout = asset( 1000000000, HBD_SYMBOL );       /// HBD value of the maximum payout this post will receive
      bool              allow_votes   = true;      /// allows a post to receive votes;
      bool              allow_curation_rewards = true;

      using t_beneficiaries = t_vector< beneficiary_route_type >;
      t_beneficiaries   beneficiaries;
#ifdef HIVE_ENABLE_SMT
      using t_votable_assets = t_vector< t_pair< asset_symbol_type, votable_asset_info > >;
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
        const comment_object& _comment, fc::optional< std::reference_wrapper< const comment_cashout_ex_object > > _parent_comment )
      : id( _comment.get_id() ), //note that it is possible because relation is 1->{0,1} so we can share id
        last_payout( time_point_sec::min() ), max_cashout_time( time_point_sec::maximum() )
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

      //accumulates abs_rshares value from new vote in discussion
      void add_abs_rshares( int64_t abs_rshares )
      {
        children_abs_rshares += abs_rshares;
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
        children_abs_rshares = 0;
        max_cashout_time = time_point_sec::maximum();
      }

    private:
      comment_id_type root_comment;

      time_point_sec  last_payout;
      time_point_sec  max_cashout_time;
      share_type      children_abs_rshares; /// used to calculate cashout time of a discussion

      HBD_asset       total_payout_value;
      HBD_asset       curator_payout_value;
      HBD_asset       beneficiary_payout_value;

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
      CHAINBASE_DEFAULT_CONSTRUCTOR( comment_vote_object )

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
          member< comment_cashout_object, time_point_sec, &comment_cashout_object::cashout_time>,
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

FC_REFLECT( hive::chain::comment_cashout_object,
          (id)(author_id)(permlink)
          (active)
          (net_rshares)(abs_rshares)(vote_rshares)
          (children)(created)(cashout_time)
          (total_vote_weight)(reward_weight)
          (net_votes)
          (max_accepted_payout)(percent_hbd)(allow_votes)(allow_curation_rewards)
          (beneficiaries)
#ifdef HIVE_ENABLE_SMT
          (allowed_vote_assets)
#endif
        )

CHAINBASE_SET_INDEX_TYPE( hive::chain::comment_cashout_object, hive::chain::comment_cashout_index )

FC_REFLECT( hive::chain::comment_cashout_ex_object,
           (id)(root_comment)
           (last_payout)(max_cashout_time)
           (children_abs_rshares)
           (total_payout_value)(curator_payout_value)(beneficiary_payout_value)
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
          info._item_additional_allocation += o.permlink.capacity()*sizeof(shared_string::value_type);
          info._item_additional_allocation += o.beneficiaries.capacity()*sizeof(t_beneficiaries::value_type);
#ifdef HIVE_ENABLE_SMT
          info._item_additional_allocation += o.allowed_vote_assets.capacity()*sizeof(t_votable_assets::value_type);
#endif
        }
      }

      return info;
    }
  };

} /// namespace helpers