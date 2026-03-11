#include <hive/chain/detail/state/escrow_object_multiindex.hpp>
#include <hive/chain/detail/state/savings_withdraw_object_multiindex.hpp>
#include <hive/chain/detail/state/decline_voting_rights_request_object_multiindex.hpp>
#include <hive/chain/detail/state/limit_order_object_multiindex.hpp>

#include <hive/chain/account_object_multiindex.hpp>
#include <hive/chain/database_virtual_operations.hpp>
#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/index.hpp>
#include <chainbase/chainbase.inl>

#include <hive/chain/util/type_registrar_definition.hpp>

#include <hive/protocol/hive_operations.hpp>
#include <hive/protocol/hive_virtual_operations.hpp>

namespace hive { namespace chain {

using hive::protocol::escrow_rejected_operation;
using hive::protocol::fill_transfer_from_savings_operation;
using hive::protocol::fill_order_operation;
using hive::protocol::limit_order_cancelled_operation;

void initialize_core_indexes_08( database& db )
{
  HIVE_ADD_CORE_INDEX(db, escrow_index);
  HIVE_ADD_CORE_INDEX(db, savings_withdraw_index);
  HIVE_ADD_CORE_INDEX(db, decline_voting_rights_request_index);
  HIVE_ADD_CORE_INDEX(db, limit_order_index);

}

const escrow_object& database::get_escrow( const account_name_type& name, uint32_t escrow_id )const
{ try {
  const auto* _escrow = find_escrow( name, escrow_id );
  FC_ASSERT( _escrow != nullptr, "Escrow balance with 'name' ${name} 'escrow_id' ${escrow_id} doesn't exist.", (name)(escrow_id) );
  return *_escrow;
} FC_CAPTURE_AND_RETHROW( (name)(escrow_id) ) }

const escrow_object* database::find_escrow( const account_name_type& name, uint32_t escrow_id )const
{
  return find< escrow_object, by_from_id >( boost::make_tuple( name, escrow_id ) );
}

const savings_withdraw_object* database::find_savings_withdraw( const account_name_type& owner, uint32_t request_id )const
{
  return find< savings_withdraw_object, by_from_rid >( boost::make_tuple( owner, request_id ) );
}

const limit_order_object* database::find_limit_order( const account_name_type& name, uint32_t orderid )const
{
  if( !has_hardfork( HIVE_HARDFORK_0_6__127 ) )
    orderid = orderid & 0x0000FFFF;

  return find< limit_order_object, by_account >( boost::make_tuple( name, orderid ) );
}

void database::expire_escrow_ratification()
{
  const auto& escrow_idx = get_index< escrow_index, by_ratification_deadline >();
  auto escrow_itr = escrow_idx.lower_bound( false );
  auto now = head_block_time();

  while( escrow_itr != escrow_idx.end() && !escrow_itr->is_approved() && escrow_itr->get_ratification_deadline() <= now )
  {
    const auto& old_escrow = *escrow_itr;
    ++escrow_itr;

    adjust_balance( old_escrow.get_from(), old_escrow.get_hive_balance() );
    adjust_balance( old_escrow.get_from(), old_escrow.get_hbd_balance() );
    adjust_balance( old_escrow.get_from(), old_escrow.get_fee() );

    push_virtual_operation( *this, escrow_rejected_operation( old_escrow.get_from(), old_escrow.get_to(), old_escrow.get_agent(), old_escrow.get_escrow_id(),
      old_escrow.get_hbd_balance(), old_escrow.get_hive_balance(), old_escrow.get_fee() ) );

    modify( get_account( old_escrow.get_from() ), []( account_object& a )
    {
      a.pending_escrow_transfers--;
    } );

    remove( old_escrow );
  }
}

void database::remove_pending_escrows( const account_object& account, const account_name_type& account_name )
{
  // Remove pending escrows (return balance to account - compare with expire_escrow_ratification())
  const auto& escrow_idx = get_index< escrow_index, by_from_id >();
  auto escrow_itr = escrow_idx.lower_bound( account_name );
  while( escrow_itr != escrow_idx.end() && escrow_itr->get_from() == account_name )
  {
    auto& escrow = *escrow_itr;
    ++escrow_itr;

    adjust_balance( account, escrow.get_hive_balance() );
    adjust_balance( account, escrow.get_hbd_balance() );
    adjust_balance( account, escrow.get_fee() );

    modify( account, []( account_object& a )
    {
      a.pending_escrow_transfers--;
    } );

    remove( escrow );
  }
}

void database::get_escrow_totals( HIVE_asset& total_hive, HBD_asset& total_hbd, uint64_t& escrow_count ) const
{
  const auto& escrow_idx = get_index< escrow_index >().indices().get< by_id >();

  for( auto itr = escrow_idx.begin(); itr != escrow_idx.end(); ++itr )
  {
    total_hive += itr->get_hive_balance();
    total_hbd += itr->get_hbd_balance();

    if( itr->get_fee().symbol == HIVE_SYMBOL )
      total_hive += HIVE_asset( itr->get_fee() );
    else
    {
      FC_ASSERT( itr->get_fee().symbol == HBD_SYMBOL, "found escrow pending fee that is not HBD or HIVE" );
      total_hbd += HBD_asset( itr->get_fee() );
    }
    ++escrow_count;
  }
}

const savings_withdraw_object& database::get_savings_withdraw( const account_name_type& owner, uint32_t request_id )const
{ try {
  const auto* _savings_withdraw = find_savings_withdraw( owner, request_id );
  FC_ASSERT( _savings_withdraw != nullptr, "Savings withdraw for `owner` ${owner} and 'request_id' ${request_id} doesn't exist.", (owner)(request_id) );
  return *_savings_withdraw;
} FC_CAPTURE_AND_RETHROW( (owner)(request_id) ) }

void database::process_savings_withdraws()
{
  const auto& idx = get_index< savings_withdraw_index >().indices().get< by_complete_from_rid >();
  auto itr = idx.begin();

  int count = 0;
  if( _benchmark_dumper.is_enabled() )
    _benchmark_dumper.begin();
  while( itr != idx.end() )
  {
    if( itr->complete > head_block_time() )
      break;
    adjust_balance( get_account( itr->to ), itr->amount );

    modify( get_account( itr->from ), [&]( account_object& a )
    {
      a.savings_withdraw_requests--;
    });

    push_virtual_operation( *this, fill_transfer_from_savings_operation( itr->from, itr->to, itr->amount, itr->request_id, to_string( itr->memo) ) );

    remove( *itr );
    itr = idx.begin();
    ++count;
  }
  if( _benchmark_dumper.is_enabled() && count )
    _benchmark_dumper.end( "processing", "hive::protocol::transfer_from_savings_operation", count );
}

void database::remove_pending_savings_withdraws( const account_object& account, const account_name_type& account_name )
{
  // Remove ongoing saving withdrawals (return/pass balance to account)
  const auto& withdraw_from_idx = get_index< savings_withdraw_index, by_from_rid >();
  auto withdraw_from_itr = withdraw_from_idx.lower_bound( account_name );
  while( withdraw_from_itr != withdraw_from_idx.end() && withdraw_from_itr->from == account_name )
  {
    auto& withdrawal = *withdraw_from_itr;
    ++withdraw_from_itr;

    adjust_balance( account, withdrawal.amount );
    modify( account, []( account_object& a )
    {
      a.savings_withdraw_requests--;
    } );

    remove( withdrawal );
  }

  const auto& withdraw_to_idx = get_index< savings_withdraw_index, by_to_complete >();
  auto withdraw_to_itr = withdraw_to_idx.lower_bound( account_name );
  while( withdraw_to_itr != withdraw_to_idx.end() && withdraw_to_itr->to == account_name )
  {
    auto& withdrawal = *withdraw_to_itr;
    ++withdraw_to_itr;

    adjust_balance( account, withdrawal.amount );
    modify( get_account( withdrawal.from ), []( account_object& a )
    {
      a.savings_withdraw_requests--;
    } );

    remove( withdrawal );
  }
}

void database::get_savings_withdraw_totals( HIVE_asset& total_hive, HBD_asset& total_hbd, uint64_t& withdrawal_count ) const
{
  const auto& savings_withdraw_idx = get_index< savings_withdraw_index >().indices().get< by_id >();

  for( auto itr = savings_withdraw_idx.begin(); itr != savings_withdraw_idx.end(); ++itr )
  {
    if( itr->amount.symbol == HIVE_SYMBOL )
      total_hive += HIVE_asset( itr->amount );
    else
    {
      FC_ASSERT( itr->amount.symbol == HBD_SYMBOL, "found savings withdraw that is not HBD or HIVE" );
      total_hbd += HBD_asset( itr->amount );
    }
    ++withdrawal_count;
  }
}

const limit_order_object& database::get_limit_order( const account_name_type& name, uint32_t orderid )const
{ try {
  if( !has_hardfork( HIVE_HARDFORK_0_6__127 ) )
    orderid = orderid & 0x0000FFFF;

  const auto* _limit_order = find_limit_order( name, orderid );
  FC_ASSERT( _limit_order != nullptr, "Limit order with 'name' ${name} 'order_id' ${orderid} doesn't exist.", (name)(orderid) );
  return *_limit_order;
} FC_CAPTURE_AND_RETHROW( (name)(orderid) ) }

bool database::apply_order( const limit_order_object& new_order_object )
{
  auto order_id = new_order_object.get_id();

  const auto& limit_price_idx = get_index<limit_order_index>().indices().get<by_price>();

  auto max_price = ~new_order_object.sell_price;
  auto limit_itr = limit_price_idx.lower_bound(max_price.max());
  auto limit_end = limit_price_idx.upper_bound(max_price);

  bool finished = false;
  while( !finished && limit_itr != limit_end )
  {
    auto old_limit_itr = limit_itr;
    ++limit_itr;
    // match returns 2 when only the old order was fully filled. In this case, we keep matching; otherwise, we stop.
    finished = ( match(new_order_object, *old_limit_itr, old_limit_itr->sell_price) & 0x1 );
  }

  return find< limit_order_object >( order_id ) == nullptr;
}

int database::match( const limit_order_object& new_order, const limit_order_object& old_order, const price& match_price )
{
  HIVE_ASSERT( new_order.sell_price.quote.symbol == old_order.sell_price.base.symbol,
    order_match_exception, "error matching orders: ${new_order} ${old_order} ${match_price}",
    ("new_order", new_order)("old_order", old_order)("match_price", match_price) );
  HIVE_ASSERT( new_order.sell_price.base.symbol  == old_order.sell_price.quote.symbol,
    order_match_exception, "error matching orders: ${new_order} ${old_order} ${match_price}",
    ("new_order", new_order)("old_order", old_order)("match_price", match_price) );
  HIVE_ASSERT( new_order.for_sale.amount > 0 && old_order.for_sale.amount > 0,
    order_match_exception, "error matching orders: ${new_order} ${old_order} ${match_price}",
    ("new_order", new_order)("old_order", old_order)("match_price", match_price) );
  HIVE_ASSERT( match_price.quote.symbol == new_order.sell_price.base.symbol,
    order_match_exception, "error matching orders: ${new_order} ${old_order} ${match_price}",
    ("new_order", new_order)("old_order", old_order)("match_price", match_price) );
  HIVE_ASSERT( match_price.base.symbol == old_order.sell_price.base.symbol,
    order_match_exception, "error matching orders: ${new_order} ${old_order} ${match_price}",
    ("new_order", new_order)("old_order", old_order)("match_price", match_price) );

  auto new_order_for_sale = new_order.amount_for_sale();
  auto old_order_for_sale = old_order.amount_for_sale();

  asset new_order_pays, new_order_receives, old_order_pays, old_order_receives;

  if( new_order_for_sale <= old_order_for_sale * match_price )
  {
    old_order_receives = new_order_for_sale;
    new_order_receives  = new_order_for_sale * match_price;
  }
  else
  {
    //This line once read: assert( old_order_for_sale < new_order_for_sale * match_price );
    //This assert is not always true -- see trade_amount_equals_zero in operation_tests.cpp
    //Although new_order_for_sale is greater than old_order_for_sale * match_price, old_order_for_sale == new_order_for_sale * match_price
    //Removing the assert seems to be safe -- apparently no asset is created or destroyed.
    new_order_receives = old_order_for_sale;
    old_order_receives = old_order_for_sale * match_price;
  }

  old_order_pays = new_order_receives;
  new_order_pays = old_order_receives;

  HIVE_ASSERT( new_order_pays == new_order.amount_for_sale() ||
               old_order_pays == old_order.amount_for_sale(),
    order_match_exception, "error matching orders: ${new_order} ${old_order} ${match_price}",
    ("new_order", new_order)("old_order", old_order)("match_price", match_price) );

  auto age = head_block_time() - old_order.created;
  if( !has_hardfork( HIVE_HARDFORK_0_12__178 ) &&
      ( (age >= HIVE_MIN_LIQUIDITY_REWARD_PERIOD_SEC && !has_hardfork( HIVE_HARDFORK_0_10__149)) ||
      (age >= HIVE_MIN_LIQUIDITY_REWARD_PERIOD_SEC_HF10 && has_hardfork( HIVE_HARDFORK_0_10__149) ) ) )
  {
    if( old_order_receives.symbol == HIVE_SYMBOL )
    {
      adjust_liquidity_reward( get_account( old_order.seller ), old_order_receives, false );
      adjust_liquidity_reward( get_account( new_order.seller ), -old_order_receives, false );
    }
    else
    {
      adjust_liquidity_reward( get_account( old_order.seller ), new_order_receives, true );
      adjust_liquidity_reward( get_account( new_order.seller ), -new_order_receives, true );
    }
  }

  push_virtual_operation( *this, fill_order_operation( new_order.seller, new_order.orderid, new_order_pays, old_order.seller, old_order.orderid, old_order_pays ) );

  int result = 0;
  result |= fill_order( new_order, new_order_pays, new_order_receives );
  result |= fill_order( old_order, old_order_pays, old_order_receives ) << 1;

  HIVE_ASSERT( result != 0,
    order_match_exception, "error matching orders: ${new_order} ${old_order} ${match_price}",
    ("new_order", new_order)("old_order", old_order)("match_price", match_price) );
  return result;
}

bool database::fill_order( const limit_order_object& order, const asset& pays, const asset& receives )
{
  try
  {
    HIVE_ASSERT( order.amount_for_sale().symbol == pays.symbol,
      order_fill_exception, "error filling orders: ${order} ${pays} ${receives}",
      ("order", order)("pays", pays)("receives", receives) );
    HIVE_ASSERT( pays.symbol != receives.symbol,
      order_fill_exception, "error filling orders: ${order} ${pays} ${receives}",
      ("order", order)("pays", pays)("receives", receives) );

    adjust_balance( order.seller, receives );

    if( pays == order.amount_for_sale() )
    {
      remove( order );
      return true;
    }
    else
    {
      HIVE_ASSERT( pays < order.amount_for_sale(),
        order_fill_exception, "error filling orders: ${order} ${pays} ${receives}",
        ("order", order)("pays", pays)("receives", receives) );

      modify( order, [&]( limit_order_object& b )
      {
        b.for_sale -= pays;
      } );
      /**
        *  There are times when the AMOUNT_FOR_SALE * SALE_PRICE == 0 which means that we
        *  have hit the limit where the seller is asking for nothing in return.  When this
        *  happens we must refund any balance back to the seller, it is too small to be
        *  sold at the sale price.
        */
      if( order.amount_to_receive().amount == 0 )
      {
        cancel_order(order);
        return true;
      }
      return false;
    }
  }
  FC_CAPTURE_AND_RETHROW( (order)(pays)(receives) )
}

void database::cancel_order( const limit_order_object& order, bool suppress_vop )
{
  auto amount_back = order.amount_for_sale();

  adjust_balance( order.seller, amount_back );
  if( !suppress_vop )
    push_virtual_operation( *this, limit_order_cancelled_operation(order.seller, order.orderid, amount_back));

  remove(order);
}

void database::clear_expired_orders()
{
  auto now = head_block_time();
  const auto& orders_by_exp = get_index<limit_order_index>().indices().get<by_expiration>();
  auto itr = orders_by_exp.begin();
  while( itr != orders_by_exp.end() && itr->expiration < now )
  {
    cancel_order( *itr );
    itr = orders_by_exp.begin();
  }
}

void database::get_limit_order_totals( HIVE_asset& total_hive, HBD_asset& total_hbd, uint64_t& order_count ) const
{
  const auto& limit_order_idx = get_index< limit_order_index >().indices();

  for( auto itr = limit_order_idx.begin(); itr != limit_order_idx.end(); ++itr )
  {
    if( itr->for_sale.symbol == HIVE_SYMBOL )
    {
      total_hive += HIVE_asset( itr->for_sale );
    }
    else if ( itr->for_sale.symbol == HBD_SYMBOL )
    {
      total_hbd += HBD_asset( itr->for_sale );
    }
    ++order_count;
  }
}

void database::remove_pending_limit_orders( const account_object& account, const account_name_type& account_name )
{
  const auto& order_idx = get_index< limit_order_index, by_account >();
  auto order_itr = order_idx.lower_bound( account_name );
  while( order_itr != order_idx.end() && order_itr->seller == account_name )
  {
    auto& order = *order_itr;
    ++order_itr;

    cancel_order( order, true );
  }
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::escrow_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::savings_withdraw_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::decline_voting_rights_request_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::limit_order_index)

// Explicit template instantiations for chainbase::database methods
template const chainbase::generic_index<hive::chain::escrow_index>& chainbase::database::get_index<hive::chain::escrow_index>() const;
template chainbase::generic_index<hive::chain::escrow_index>& chainbase::database::get_mutable_index<hive::chain::escrow_index>();

template const chainbase::generic_index<hive::chain::savings_withdraw_index>& chainbase::database::get_index<hive::chain::savings_withdraw_index>() const;
template chainbase::generic_index<hive::chain::savings_withdraw_index>& chainbase::database::get_mutable_index<hive::chain::savings_withdraw_index>();

template const chainbase::generic_index<hive::chain::decline_voting_rights_request_index>& chainbase::database::get_index<hive::chain::decline_voting_rights_request_index>() const;
template chainbase::generic_index<hive::chain::decline_voting_rights_request_index>& chainbase::database::get_mutable_index<hive::chain::decline_voting_rights_request_index>();

template const chainbase::generic_index<hive::chain::limit_order_index>& chainbase::database::get_index<hive::chain::limit_order_index>() const;
template chainbase::generic_index<hive::chain::limit_order_index>& chainbase::database::get_mutable_index<hive::chain::limit_order_index>();
