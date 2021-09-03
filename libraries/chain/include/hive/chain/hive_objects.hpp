#pragma once
#include <chainbase/forward_declarations.hpp>

#include <hive/protocol/authority.hpp>
#include <hive/protocol/hive_operations.hpp>
#include <hive/protocol/misc_utilities.hpp>

#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/account_object.hpp>

#include <boost/multiprecision/cpp_int.hpp>


namespace hive { namespace chain {

  using hive::protocol::HBD_asset;
  using hive::protocol::HIVE_asset;
  using hive::protocol::VEST_asset;
  using hive::protocol::asset;
  using hive::protocol::price;
  using hive::protocol::asset_symbol_type;
  using chainbase::t_deque;

  typedef protocol::fixed_string< 16 > reward_fund_name_type;

  /**
    *  This object is used to track pending requests to convert HBD to HIVE
    */
  class convert_request_object : public object< convert_request_object_type, convert_request_object >
  {
    CHAINBASE_OBJECT( convert_request_object );
    public:
      template< typename Allocator >
      convert_request_object( allocator< Allocator > a, uint64_t _id,
        const account_object& _owner, const HBD_asset& _amount, const time_point_sec& _conversion_time, uint32_t _requestid )
        : id( _id ), owner( _owner.get_id() ), amount( _amount ), requestid( _requestid ), conversion_date( _conversion_time )
      {}

      //amount of HBD to be converted to HIVE
      const HBD_asset& get_convert_amount() const { return amount; }

      //request owner
      account_id_type get_owner() const { return owner; }
      //request id - unique id of request among active requests of the same owner
      uint32_t get_request_id() const { return requestid; }
      //time of actual conversion
      time_point_sec get_conversion_date() const { return conversion_date; }

    private:
      account_id_type owner;
      HBD_asset       amount;
      uint32_t        requestid = 0; //< id set by owner, the owner,requestid pair must be unique
      time_point_sec  conversion_date; //< actual conversion takes place at that time

    CHAINBASE_UNPACK_CONSTRUCTOR(convert_request_object);
  };

  /**
    *  This object is used to track pending requests to convert HIVE to HBD with collateral
    */
  class collateralized_convert_request_object : public object< collateralized_convert_request_object_type, collateralized_convert_request_object >
  {
    CHAINBASE_OBJECT( collateralized_convert_request_object );
    public:
      template< typename Allocator >
      collateralized_convert_request_object( allocator< Allocator > a, uint64_t _id,
        const account_object& _owner, const HIVE_asset& _collateral_amount, const HBD_asset& _converted_amount,
        const time_point_sec& _conversion_time, uint32_t _requestid )
        : id( _id ), owner( _owner.get_id() ), collateral_amount( _collateral_amount ), converted_amount( _converted_amount ),
        requestid( _requestid ), conversion_date( _conversion_time )
      {}

      //amount of HIVE collateral
      const HIVE_asset& get_collateral_amount() const { return collateral_amount; }
      //amount of HBD that was already given to owner
      const HBD_asset& get_converted_amount() const { return converted_amount; }

      //request owner
      account_id_type get_owner() const { return owner; }
      //request id - unique id of request among active requests of the same owner
      uint32_t get_request_id() const { return requestid; }
      //time of actual conversion
      time_point_sec get_conversion_date() const { return conversion_date; }

    private:
      account_id_type owner;
      HIVE_asset      collateral_amount;
      HBD_asset       converted_amount;
      uint32_t        requestid = 0; //< id set by owner, the (owner,requestid) pair must be unique
      time_point_sec  conversion_date; //< actual conversion takes place at that time, excess collateral is returned to owner

      CHAINBASE_UNPACK_CONSTRUCTOR( collateralized_convert_request_object );
  };

  class escrow_object : public object< escrow_object_type, escrow_object >
  {
    CHAINBASE_OBJECT( escrow_object );
    public:
      template< typename Allocator >
      escrow_object( allocator< Allocator > a, uint64_t _id,
        const account_name_type& _from, const account_name_type& _to, const account_name_type& _agent,
        const asset& _hive_amount, const asset& _hbd_amount, const asset& _fee,
        const time_point_sec& _ratification_deadline, const time_point_sec& _escrow_expiration, uint32_t _escrow_transfer_id )
        : id( _id ), escrow_id( _escrow_transfer_id ), from( _from ), to( _to ), agent( _agent ),
        ratification_deadline( _ratification_deadline ), escrow_expiration( _escrow_expiration ),
        hbd_balance( _hbd_amount ), hive_balance( _hive_amount ), pending_fee( _fee )
      {}

      //HIVE portion of transfer balance
      const asset& get_hive_balance() const { return hive_balance; }
      //HBD portion of transfer balance
      const asset& get_hbd_balance() const { return hbd_balance; }
      //fee offered to escrow (can be either in HIVE or HBD)
      const asset& get_fee() const { return pending_fee; }

      bool is_approved() const { return to_approved && agent_approved; }

      uint32_t          escrow_id = 20;
      account_name_type from; //< TODO: can be replaced with account_id_type
      account_name_type to; //< TODO: can be replaced with account_id_type
      account_name_type agent; //< TODO: can be replaced with account_id_type
      time_point_sec    ratification_deadline;
      time_point_sec    escrow_expiration;
      asset             hbd_balance; //< TODO: can be replaced with HBD_asset
      asset             hive_balance; //< TODO: can be replaced with HIVE_asset
      asset             pending_fee; //fee can use HIVE of HBD
      bool              to_approved = false; //< TODO: can be replaced with bit field along with all flags
      bool              agent_approved = false;
      bool              disputed = false;

    CHAINBASE_UNPACK_CONSTRUCTOR(escrow_object);
  };


  class savings_withdraw_object : public object< savings_withdraw_object_type, savings_withdraw_object >
  {
    CHAINBASE_OBJECT( savings_withdraw_object );
    public:
      template< typename Allocator >
      savings_withdraw_object( allocator< Allocator > a, uint64_t _id,
        const account_name_type& _from, const account_name_type& _to, const asset& _amount,
        const string& _memo, const time_point_sec& _time_of_completion, uint32_t _request_id )
        : id( _id ), from( _from ), to( _to ), memo( a ), request_id( _request_id ),
        amount( _amount ), complete( _time_of_completion )
      {
        from_string( memo, _memo );
      }

      //amount of savings to withdraw (HIVE or HBD)
      const asset& get_withdraw_amount() const { return amount; }

      account_name_type from; //< TODO: can be replaced with account_id_type
      account_name_type to; //< TODO: can be replaced with account_id_type
      shared_string     memo;
      uint32_t          request_id = 0;
      asset             amount; //can be expressed in HIVE or HBD
      time_point_sec    complete;

    CHAINBASE_UNPACK_CONSTRUCTOR(savings_withdraw_object, (memo));
  };


  /**
    *  If last_update is greater than 1 week, then volume gets reset to 0
    *
    *  When a user is a maker, their volume increases
    *  When a user is a taker, their volume decreases
    *
    *  Every 1000 blocks, the account that has the highest volume_weight() is paid the maximum of
    *  1000 HIVE or 1000 * virtual_supply / (100*blocks_per_year) aka 10 * virtual_supply / blocks_per_year
    *
    *  After being paid volume gets reset to 0
    */
  class liquidity_reward_balance_object : public object< liquidity_reward_balance_object_type, liquidity_reward_balance_object >
  {
    CHAINBASE_OBJECT( liquidity_reward_balance_object );
    public:
      CHAINBASE_DEFAULT_CONSTRUCTOR( liquidity_reward_balance_object )

      int64_t get_hive_volume() const { return hive_volume; }
      int64_t get_hbd_volume() const { return hbd_volume; }

      account_id_type   owner;
      int64_t           hive_volume = 0;
      int64_t           hbd_volume = 0;
      uint128_t         weight = 0;

      time_point_sec    last_update = fc::time_point_sec::min(); /// used to decay negative liquidity balances. block num

      /// this is the sort index
      uint128_t volume_weight()const
      {
        return hive_volume * hbd_volume * is_positive();
      }

      uint128_t min_volume_weight()const
      {
        return std::min(hive_volume,hbd_volume) * is_positive();
      }

      void update_weight( bool hf9 )
      {
          weight = hf9 ? min_volume_weight() : volume_weight();
      }

      inline int is_positive()const
      {
        return ( hive_volume > 0 && hbd_volume > 0 ) ? 1 : 0;
      }
    CHAINBASE_UNPACK_CONSTRUCTOR(liquidity_reward_balance_object);
  };


  /**
    *  This object gets updated once per hour, on the hour. Tracks price of HIVE (technically in HBD, but
    *  the meaning is in dollars).
    */
  class feed_history_object : public object< feed_history_object_type, feed_history_object >
  {
    CHAINBASE_OBJECT( feed_history_object );
    public:
      CHAINBASE_DEFAULT_CONSTRUCTOR( feed_history_object, (price_history) )

      price current_median_history; ///< the current median of the price history, used as the base for most convert operations
      price market_median_history; ///< same as current_median_history except when the latter is artificially changed upward
      price current_min_history; ///< used as immediate price for collateralized conversion (after fee correction)
      price current_max_history;

      using t_price_history = t_deque< price >;

      t_deque< price > price_history; ///< tracks this last week of median_feed one per hour

    CHAINBASE_UNPACK_CONSTRUCTOR(feed_history_object, (price_history));
  };


  /**
    *  @brief an offer to sell a amount of a asset at a specified exchange rate by a certain time
    *  @ingroup object
    *  @ingroup protocol
    *  @ingroup market
    *
    *  This limit_order_objects are indexed by @ref expiration and is automatically deleted on the first block after expiration.
    */
  class limit_order_object : public object< limit_order_object_type, limit_order_object >
  {
    CHAINBASE_OBJECT( limit_order_object );
    public:
      template< typename Allocator >
      limit_order_object( allocator< Allocator > a, uint64_t _id,
        const account_name_type& _seller, const asset& _amount_to_sell, const price& _sell_price,
        const time_point_sec& _creation_time, const time_point_sec& _expiration_time, uint32_t _orderid )
        : id( _id ), created( _creation_time ), expiration( _expiration_time ), seller( _seller ),
        orderid( _orderid ), for_sale( _amount_to_sell.amount ), sell_price( _sell_price )
      {
        FC_ASSERT( _amount_to_sell.symbol == _sell_price.base.symbol );
      }

      pair< asset_symbol_type, asset_symbol_type > get_market() const
      {
        return sell_price.base.symbol < sell_price.quote.symbol ?
          std::make_pair( sell_price.base.symbol, sell_price.quote.symbol ) :
          std::make_pair( sell_price.quote.symbol, sell_price.base.symbol );
      }

      asset amount_for_sale() const { return asset( for_sale, sell_price.base.symbol ); }
      asset amount_to_receive() const { return amount_for_sale() * sell_price; }

      time_point_sec    created;
      time_point_sec    expiration;
      account_name_type seller; //< TODO: can be replaced with account_id_type
      uint32_t          orderid = 0;
      share_type        for_sale; ///< asset id is sell_price.base.symbol
      price             sell_price;

      CHAINBASE_UNPACK_CONSTRUCTOR(limit_order_object);
  };


  /**
    * @breif a route to send withdrawn vesting shares.
    */
  class withdraw_vesting_route_object : public object< withdraw_vesting_route_object_type, withdraw_vesting_route_object >
  {
    CHAINBASE_OBJECT( withdraw_vesting_route_object, true );
    public:
      CHAINBASE_DEFAULT_CONSTRUCTOR( withdraw_vesting_route_object )

      account_name_type from_account;
      account_name_type to_account;
      uint16_t          percent = 0;
      bool              auto_vest = false;

    CHAINBASE_UNPACK_CONSTRUCTOR(withdraw_vesting_route_object);
  };


  class decline_voting_rights_request_object : public object< decline_voting_rights_request_object_type, decline_voting_rights_request_object >
  {
    CHAINBASE_OBJECT( decline_voting_rights_request_object );
    public:
      CHAINBASE_DEFAULT_CONSTRUCTOR( decline_voting_rights_request_object )

      account_name_type account;
      time_point_sec    effective_date;

    CHAINBASE_UNPACK_CONSTRUCTOR(decline_voting_rights_request_object);
  };

  class reward_fund_object : public object< reward_fund_object_type, reward_fund_object >
  {
    CHAINBASE_OBJECT( reward_fund_object );
    public:
      template< typename Allocator >
      reward_fund_object( allocator< Allocator > a, uint64_t _id,
        const string& _name, const asset& _balance, const time_point_sec& _creation_time, const uint128_t& _claims = 0 )
        : id( _id ), name( _name ), reward_balance( _balance ), recent_claims( _claims ), last_update( _creation_time )
      {}

      //amount of HIVE in reward fund
      const asset& get_reward_balance() const { return reward_balance; }

      reward_fund_name_type   name;
      asset                   reward_balance = asset( 0, HIVE_SYMBOL );
      uint128_t               recent_claims = 0;
      time_point_sec          last_update;
      uint128_t               content_constant = HIVE_CONTENT_CONSTANT_HF0;
      uint16_t                percent_curation_rewards = HIVE_1_PERCENT * 25;
      uint16_t                percent_content_rewards = HIVE_100_PERCENT;
      protocol::curve_id      author_reward_curve = protocol::curve_id::quadratic;
      protocol::curve_id      curation_reward_curve = protocol::curve_id::bounded_curation;

    CHAINBASE_UNPACK_CONSTRUCTOR(reward_fund_object);
  };

  class recurrent_transfer_object : public object< recurrent_transfer_object_type, recurrent_transfer_object >
  {
    CHAINBASE_OBJECT( recurrent_transfer_object );
    public:
      template< typename Allocator >
      recurrent_transfer_object( allocator< Allocator > a, uint64_t _id,
        const time_point_sec& _trigger_date, const account_object& _from,
        const account_object& _to, const asset& _amount, const string& _memo,
        const uint16_t _recurrence, const uint16_t _remaining_executions )
      : id( _id ), trigger_date( _trigger_date ), from_id( _from.get_id() ), to_id( _to.get_id() ),
        amount( _amount ), memo( a ), recurrence( _recurrence ), remaining_executions( _remaining_executions )
      {
        from_string( memo, _memo );
      }

      void update_next_trigger_date()
      {
        trigger_date += fc::hours(recurrence);
      }

      time_point_sec get_trigger_date() const
      {
        return trigger_date;
      }

      // if the recurrence changed, we must update the trigger_date
      void set_recurrence_trigger_date( const time_point_sec& _head_block_time, uint16_t _recurrence )
      {
        if ( _recurrence != recurrence )
          trigger_date = _head_block_time + fc::hours( _recurrence );

        recurrence = _recurrence;
      }

    private:
      time_point_sec    trigger_date;
    public:
      account_id_type   from_id;
      account_id_type   to_id;
      asset             amount;
      /// The memo is plain-text, any encryption on the memo is up to a higher level protocol.
      shared_string     memo;
      /// How often will the payment be triggered, unit: hours
      uint16_t          recurrence = 0;
      /// How many payment have failed in a row, at HIVE_MAX_CONSECUTIVE_RECURRENT_TRANSFER_FAILURES the object is deleted
      uint8_t           consecutive_failures = 0;
      /// How many executions are remaining
      uint16_t          remaining_executions = 0;

    CHAINBASE_UNPACK_CONSTRUCTOR(recurrent_transfer_object, (memo));
  };

  struct by_price;
  struct by_expiration;
  struct by_account;
  typedef multi_index_container<
    limit_order_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< limit_order_object, limit_order_object::id_type, &limit_order_object::get_id > >,
      ordered_unique< tag< by_expiration >,
        composite_key< limit_order_object,
          member< limit_order_object, time_point_sec, &limit_order_object::expiration >,
          const_mem_fun< limit_order_object, limit_order_object::id_type, &limit_order_object::get_id >
        >
      >,
      ordered_unique< tag< by_price >,
        composite_key< limit_order_object,
          member< limit_order_object, price, &limit_order_object::sell_price >,
          const_mem_fun< limit_order_object, limit_order_object::id_type, &limit_order_object::get_id >
        >,
        composite_key_compare< std::greater< price >, std::less< limit_order_id_type > >
      >,
      ordered_unique< tag< by_account >,
        composite_key< limit_order_object,
          member< limit_order_object, account_name_type, &limit_order_object::seller >,
          member< limit_order_object, uint32_t, &limit_order_object::orderid >
        >
      >
    >,
    allocator< limit_order_object >
  > limit_order_index;

  struct by_owner;
  struct by_conversion_date;
  typedef multi_index_container<
    convert_request_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< convert_request_object, convert_request_object::id_type, &convert_request_object::get_id > >,
      ordered_unique< tag< by_conversion_date >,
        composite_key< convert_request_object,
          const_mem_fun< convert_request_object, time_point_sec, &convert_request_object::get_conversion_date >,
          const_mem_fun< convert_request_object, convert_request_object::id_type, &convert_request_object::get_id >
        >
      >,
      ordered_unique< tag< by_owner >,
        composite_key< convert_request_object,
          const_mem_fun< convert_request_object, account_id_type, &convert_request_object::get_owner >,
          const_mem_fun< convert_request_object, uint32_t, &convert_request_object::get_request_id >
        >
      >
    >,
    allocator< convert_request_object >
  > convert_request_index;

  struct by_owner;
  struct by_conversion_date;
  typedef multi_index_container<
    collateralized_convert_request_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< collateralized_convert_request_object, collateralized_convert_request_object::id_type, &collateralized_convert_request_object::get_id > >,
      ordered_unique< tag< by_conversion_date >,
        composite_key< collateralized_convert_request_object,
          const_mem_fun< collateralized_convert_request_object, time_point_sec, &collateralized_convert_request_object::get_conversion_date >,
          const_mem_fun< collateralized_convert_request_object, collateralized_convert_request_object::id_type, &collateralized_convert_request_object::get_id >
        >
      >,
      ordered_unique< tag< by_owner >,
        composite_key< collateralized_convert_request_object,
          const_mem_fun< collateralized_convert_request_object, account_id_type, &collateralized_convert_request_object::get_owner >,
          const_mem_fun< collateralized_convert_request_object, uint32_t, &collateralized_convert_request_object::get_request_id >
        >
      >
    >,
    allocator< collateralized_convert_request_object >
  > collateralized_convert_request_index;

  struct by_owner;
  struct by_volume_weight;

  typedef multi_index_container<
    liquidity_reward_balance_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< liquidity_reward_balance_object, liquidity_reward_balance_object::id_type, &liquidity_reward_balance_object::get_id > >,
      ordered_unique< tag< by_owner >,
        member< liquidity_reward_balance_object, account_id_type, &liquidity_reward_balance_object::owner > >,
      ordered_unique< tag< by_volume_weight >,
        composite_key< liquidity_reward_balance_object,
            member< liquidity_reward_balance_object, fc::uint128, &liquidity_reward_balance_object::weight >,
            member< liquidity_reward_balance_object, account_id_type, &liquidity_reward_balance_object::owner >
        >,
        composite_key_compare< std::greater< fc::uint128 >, std::less< account_id_type > >
      >
    >,
    allocator< liquidity_reward_balance_object >
  > liquidity_reward_balance_index;

  typedef multi_index_container<
    feed_history_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< feed_history_object, feed_history_object::id_type, &feed_history_object::get_id > >
    >,
    allocator< feed_history_object >
  > feed_history_index;

  struct by_withdraw_route;
  struct by_destination;
  typedef multi_index_container<
    withdraw_vesting_route_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< withdraw_vesting_route_object, withdraw_vesting_route_object::id_type, &withdraw_vesting_route_object::get_id > >,
      ordered_unique< tag< by_withdraw_route >,
        composite_key< withdraw_vesting_route_object,
          member< withdraw_vesting_route_object, account_name_type, &withdraw_vesting_route_object::from_account >,
          member< withdraw_vesting_route_object, account_name_type, &withdraw_vesting_route_object::to_account >
        >,
        composite_key_compare< std::less< account_name_type >, std::less< account_name_type > >
      >,
      ordered_unique< tag< by_destination >,
        composite_key< withdraw_vesting_route_object,
          member< withdraw_vesting_route_object, account_name_type, &withdraw_vesting_route_object::to_account >,
          const_mem_fun< withdraw_vesting_route_object, withdraw_vesting_route_object::id_type, &withdraw_vesting_route_object::get_id >
        >
      >
    >,
    allocator< withdraw_vesting_route_object >
  > withdraw_vesting_route_index;

  struct by_from_id;
  struct by_ratification_deadline;
  typedef multi_index_container<
    escrow_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< escrow_object, escrow_object::id_type, &escrow_object::get_id > >,
      ordered_unique< tag< by_from_id >,
        composite_key< escrow_object,
          member< escrow_object, account_name_type, &escrow_object::from >,
          member< escrow_object, uint32_t, &escrow_object::escrow_id >
        >
      >,
      ordered_unique< tag< by_ratification_deadline >,
        composite_key< escrow_object,
          const_mem_fun< escrow_object, bool, &escrow_object::is_approved >,
          member< escrow_object, time_point_sec, &escrow_object::ratification_deadline >,
          const_mem_fun< escrow_object, escrow_object::id_type, &escrow_object::get_id >
        >,
        composite_key_compare< std::less< bool >, std::less< time_point_sec >, std::less< escrow_id_type > >
      >
    >,
    allocator< escrow_object >
  > escrow_index;

  struct by_from_rid;
  struct by_to_complete;
  struct by_complete_from_rid;
  typedef multi_index_container<
    savings_withdraw_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< savings_withdraw_object, savings_withdraw_object::id_type, &savings_withdraw_object::get_id > >,
      ordered_unique< tag< by_from_rid >,
        composite_key< savings_withdraw_object,
          member< savings_withdraw_object, account_name_type, &savings_withdraw_object::from >,
          member< savings_withdraw_object, uint32_t, &savings_withdraw_object::request_id >
        >
      >,
      ordered_unique< tag< by_complete_from_rid >,
        composite_key< savings_withdraw_object,
          member< savings_withdraw_object, time_point_sec, &savings_withdraw_object::complete >,
          member< savings_withdraw_object, account_name_type, &savings_withdraw_object::from >,
          member< savings_withdraw_object, uint32_t, &savings_withdraw_object::request_id >
        >
      >,
      ordered_unique< tag< by_to_complete >,
        composite_key< savings_withdraw_object,
          member< savings_withdraw_object, account_name_type, &savings_withdraw_object::to >,
          member< savings_withdraw_object, time_point_sec, &savings_withdraw_object::complete >,
          const_mem_fun< savings_withdraw_object, savings_withdraw_object::id_type, &savings_withdraw_object::get_id >
        >
      >
    >,
    allocator< savings_withdraw_object >
  > savings_withdraw_index;

  struct by_account;
  struct by_effective_date;
  typedef multi_index_container<
    decline_voting_rights_request_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< decline_voting_rights_request_object, decline_voting_rights_request_object::id_type, &decline_voting_rights_request_object::get_id > >,
      ordered_unique< tag< by_account >,
        member< decline_voting_rights_request_object, account_name_type, &decline_voting_rights_request_object::account >
      >,
      ordered_unique< tag< by_effective_date >,
        composite_key< decline_voting_rights_request_object,
          member< decline_voting_rights_request_object, time_point_sec, &decline_voting_rights_request_object::effective_date >,
          member< decline_voting_rights_request_object, account_name_type, &decline_voting_rights_request_object::account >
        >,
        composite_key_compare< std::less< time_point_sec >, std::less< account_name_type > >
      >
    >,
    allocator< decline_voting_rights_request_object >
  > decline_voting_rights_request_index;

  typedef multi_index_container<
    reward_fund_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< reward_fund_object, reward_fund_object::id_type, &reward_fund_object::get_id > >,
      ordered_unique< tag< by_name >,
        member< reward_fund_object, reward_fund_name_type, &reward_fund_object::name > >
    >,
    allocator< reward_fund_object >
  > reward_fund_index;

  struct by_from_to_id;
  struct by_from_id;
  struct by_trigger_date;
  typedef multi_index_container<
    recurrent_transfer_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< recurrent_transfer_object, recurrent_transfer_object::id_type, &recurrent_transfer_object::get_id > >,
      ordered_unique< tag< by_trigger_date >,
        composite_key< recurrent_transfer_object,
          const_mem_fun< recurrent_transfer_object, time_point_sec, &recurrent_transfer_object::get_trigger_date >,
          const_mem_fun< recurrent_transfer_object, recurrent_transfer_object::id_type, &recurrent_transfer_object::get_id >
        >
      >,
      ordered_unique< tag< by_from_id >,
        composite_key< recurrent_transfer_object,
          member< recurrent_transfer_object, account_id_type, &recurrent_transfer_object::from_id >,
          const_mem_fun< recurrent_transfer_object, recurrent_transfer_object::id_type, &recurrent_transfer_object::get_id >
        >
      >,
      ordered_unique< tag< by_from_to_id >,
        composite_key< recurrent_transfer_object,
          member< recurrent_transfer_object, account_id_type, &recurrent_transfer_object::from_id >,
          member< recurrent_transfer_object, account_id_type, &recurrent_transfer_object::to_id >
        >,
        composite_key_compare< std::less< account_id_type >, std::less< account_id_type > >
      >
    >,
    allocator< recurrent_transfer_object >
  > recurrent_transfer_index;

} } // hive::chain

#include <hive/chain/comment_object.hpp>
#include <hive/chain/account_object.hpp>

FC_REFLECT( hive::chain::limit_order_object,
          (id)(created)(expiration)(seller)(orderid)(for_sale)(sell_price) )
CHAINBASE_SET_INDEX_TYPE( hive::chain::limit_order_object, hive::chain::limit_order_index )

FC_REFLECT( hive::chain::feed_history_object,
          (id)(current_median_history)(market_median_history)(current_min_history)(current_max_history)(price_history) )
CHAINBASE_SET_INDEX_TYPE( hive::chain::feed_history_object, hive::chain::feed_history_index )

FC_REFLECT( hive::chain::convert_request_object,
          (id)(owner)(amount)(requestid)(conversion_date) )
CHAINBASE_SET_INDEX_TYPE( hive::chain::convert_request_object, hive::chain::convert_request_index )

FC_REFLECT( hive::chain::collateralized_convert_request_object,
          (id)(owner)(collateral_amount)(converted_amount)(requestid)(conversion_date) )
CHAINBASE_SET_INDEX_TYPE( hive::chain::collateralized_convert_request_object, hive::chain::collateralized_convert_request_index )

FC_REFLECT( hive::chain::liquidity_reward_balance_object,
          (id)(owner)(hive_volume)(hbd_volume)(weight)(last_update) )
CHAINBASE_SET_INDEX_TYPE( hive::chain::liquidity_reward_balance_object, hive::chain::liquidity_reward_balance_index )

FC_REFLECT( hive::chain::withdraw_vesting_route_object,
          (id)(from_account)(to_account)(percent)(auto_vest) )
CHAINBASE_SET_INDEX_TYPE( hive::chain::withdraw_vesting_route_object, hive::chain::withdraw_vesting_route_index )

FC_REFLECT( hive::chain::savings_withdraw_object,
          (id)(from)(to)(memo)(request_id)(amount)(complete) )
CHAINBASE_SET_INDEX_TYPE( hive::chain::savings_withdraw_object, hive::chain::savings_withdraw_index )

FC_REFLECT( hive::chain::escrow_object,
          (id)(escrow_id)(from)(to)(agent)
          (ratification_deadline)(escrow_expiration)
          (hbd_balance)(hive_balance)(pending_fee)
          (to_approved)(agent_approved)(disputed) )
CHAINBASE_SET_INDEX_TYPE( hive::chain::escrow_object, hive::chain::escrow_index )

FC_REFLECT( hive::chain::decline_voting_rights_request_object,
          (id)(account)(effective_date) )
CHAINBASE_SET_INDEX_TYPE( hive::chain::decline_voting_rights_request_object, hive::chain::decline_voting_rights_request_index )

FC_REFLECT( hive::chain::reward_fund_object,
        (id)
        (name)
        (reward_balance)
        (recent_claims)
        (last_update)
        (content_constant)
        (percent_curation_rewards)
        (percent_content_rewards)
        (author_reward_curve)
        (curation_reward_curve)
      )
CHAINBASE_SET_INDEX_TYPE( hive::chain::reward_fund_object, hive::chain::reward_fund_index )

FC_REFLECT(hive::chain::recurrent_transfer_object, (id)(trigger_date)(from_id)(to_id)(amount)(memo)(recurrence)(consecutive_failures)(remaining_executions) )
CHAINBASE_SET_INDEX_TYPE( hive::chain::recurrent_transfer_object, hive::chain::recurrent_transfer_index )
