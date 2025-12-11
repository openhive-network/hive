#pragma once
#include <hive/chain/hive_fwd.hpp>

#include <hive/protocol/authority.hpp>
#include <hive/protocol/asset.hpp>
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


  class savings_withdraw_object : public object< savings_withdraw_object_type, savings_withdraw_object, std::true_type >
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

      size_t get_dynamic_alloc() const
      {
        size_t size = 0;
        size += memo.capacity() * sizeof( decltype( memo )::value_type );
        return size;
      }

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
      time_point_sec    last_update = fc::time_point_sec::min(); /// used to decay negative liquidity balances. block num
      uint128_t         weight = 0;
      int64_t           hive_volume = 0;
      int64_t           hbd_volume = 0;

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
  class feed_history_object : public object< feed_history_object_type, feed_history_object, std::true_type >
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

      size_t get_dynamic_alloc() const
      {
        size_t size = 0;
        size += price_history.size() * sizeof( decltype( price_history )::value_type );
        return size;
      }

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

  class recurrent_transfer_object : public object< recurrent_transfer_object_type, recurrent_transfer_object, std::true_type >
  {
    CHAINBASE_OBJECT( recurrent_transfer_object );
    public:
      template< typename Allocator >
      recurrent_transfer_object( allocator< Allocator > a, uint64_t _id,
        const time_point_sec& _trigger_date, const account_object& _from,
        const account_object& _to, const asset& _amount, const string& _memo,
        const uint16_t _recurrence, const uint16_t _remaining_executions, const uint8_t _pair_id )
      : id( _id ), trigger_date( _trigger_date ), from_id( _from.get_id() ), to_id( _to.get_id() ),
        amount( _amount ), memo( a ), recurrence( _recurrence ), remaining_executions( _remaining_executions ), pair_id( _pair_id )
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

      time_point_sec get_final_trigger_date() const
      {
        return trigger_date + fc::hours( recurrence * ( remaining_executions - 1 ) );
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
      /// How many executions are remaining
      uint16_t          remaining_executions = 0;
      /// How many payment have failed in a row, at HIVE_MAX_CONSECUTIVE_RECURRENT_TRANSFER_FAILURES the object is deleted
      uint8_t           consecutive_failures = 0;
      uint8_t           pair_id = 0;

      size_t get_dynamic_alloc() const
      {
        size_t size = 0;
        size += memo.capacity() * sizeof( decltype( memo )::value_type );
        return size;
      }

    CHAINBASE_UNPACK_CONSTRUCTOR(recurrent_transfer_object, (memo));
  };

} } // hive::chain

FC_REFLECT( hive::chain::limit_order_object,
          (id)(created)(expiration)(seller)(orderid)(for_sale)(sell_price) )

FC_REFLECT( hive::chain::feed_history_object,
          (id)(current_median_history)(market_median_history)(current_min_history)(current_max_history)(price_history) )

FC_REFLECT( hive::chain::convert_request_object,
          (id)(owner)(amount)(requestid)(conversion_date) )

FC_REFLECT( hive::chain::collateralized_convert_request_object,
          (id)(owner)(collateral_amount)(converted_amount)(requestid)(conversion_date) )

FC_REFLECT( hive::chain::liquidity_reward_balance_object,
          (id)(owner)(last_update)(weight)(hive_volume)(hbd_volume) )

FC_REFLECT( hive::chain::withdraw_vesting_route_object,
          (id)(from_account)(to_account)(percent)(auto_vest) )

FC_REFLECT( hive::chain::savings_withdraw_object,
          (id)(from)(to)(memo)(request_id)(amount)(complete) )

FC_REFLECT( hive::chain::escrow_object,
          (id)(escrow_id)(from)(to)(agent)
          (ratification_deadline)(escrow_expiration)
          (hbd_balance)(hive_balance)(pending_fee)
          (to_approved)(agent_approved)(disputed) )

FC_REFLECT( hive::chain::decline_voting_rights_request_object,
          (id)(account)(effective_date) )

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

FC_REFLECT(hive::chain::recurrent_transfer_object, (id)(trigger_date)(from_id)(to_id)(amount)(memo)(recurrence)(remaining_executions)(consecutive_failures)(pair_id) )
