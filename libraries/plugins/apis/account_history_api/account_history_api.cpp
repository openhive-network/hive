#include <hive/plugins/account_history_api/account_history_api_plugin.hpp>
#include <hive/plugins/account_history_api/account_history_api.hpp>

#include <hive/plugins/account_history_rocksdb/account_history_rocksdb_plugin.hpp>
#include <hive/plugins/account_history_sql/account_history_sql_plugin.hpp>

namespace hive { namespace plugins { namespace account_history {

using filter_type_opt = types::filter_type_opt;

namespace detail {

class abstract_account_history_api_impl
{
  public:
    abstract_account_history_api_impl() : _db( appbase::app().get_plugin< hive::plugins::chain::chain_plugin >().db() ) {}
    virtual ~abstract_account_history_api_impl() {}

    virtual get_ops_in_block_return get_ops_in_block( const get_ops_in_block_args& ) = 0;
    virtual get_transaction_return get_transaction( const get_transaction_args& ) = 0;
    virtual get_account_history_return get_account_history( const get_account_history_args& ) = 0;
    virtual enum_virtual_ops_return enum_virtual_ops( const enum_virtual_ops_args& ) = 0;

    virtual get_ops_in_block_return get_ops_in_block_filter( const get_ops_in_block_args&, filter_type_opt filter ) = 0;
    virtual get_account_history_return get_account_history_filter( const get_account_history_args&, filter_type_opt filter ) = 0;

    chain::database& _db;
};

class account_history_api_chainbase_impl : public abstract_account_history_api_impl
{
  private:

    get_ops_in_block_return _get_ops_in_block( const get_ops_in_block_args&, filter_type_opt filter = filter_type_opt() );
    get_account_history_return _get_account_history( const get_account_history_args&, filter_type_opt filter = filter_type_opt() );

  public:
    account_history_api_chainbase_impl() : abstract_account_history_api_impl() {}
    ~account_history_api_chainbase_impl() {}

    get_ops_in_block_return get_ops_in_block( const get_ops_in_block_args& ) override;
    get_transaction_return get_transaction( const get_transaction_args& ) override;
    get_account_history_return get_account_history( const get_account_history_args& ) override;
    enum_virtual_ops_return enum_virtual_ops( const enum_virtual_ops_args& ) override;

    get_ops_in_block_return get_ops_in_block_filter( const get_ops_in_block_args& args, filter_type_opt filter ) override;
    get_account_history_return get_account_history_filter( const get_account_history_args& args, filter_type_opt filter ) override;
};

get_ops_in_block_return account_history_api_chainbase_impl::_get_ops_in_block( const get_ops_in_block_args& args, filter_type_opt filter )
{
  FC_ASSERT(args.include_reversible.valid() == false, "Supported only in AH-Rocksdb plugin");

  return _db.with_read_lock( [&]()
  {
    const auto& idx = _db.get_index< chain::operation_index, chain::by_location >();
    auto itr = idx.lower_bound( args.block_num );

    get_ops_in_block_return result;

    while( itr != idx.end() && itr->block == args.block_num )
    {
      auto _op = api_operation_object::get_op( itr->serialized_op );
      api_operation_object temp = api_operation_object( *itr, std::move( api_operation_object::get_variant( _op ) ) );
      if( !args.only_virtual || temp.virtual_op )
      {
        if( filter.valid() )
        {
          if( (*filter)( _op ) )
            result.ops.emplace( std::move( temp ) );
        }
        else
        {
          result.ops.emplace( std::move( temp ) );
        }
      }
      ++itr;
    }

    return result;
  });
}

get_ops_in_block_return account_history_api_chainbase_impl::get_ops_in_block_filter( const get_ops_in_block_args& args, filter_type_opt filter )
{
  return _get_ops_in_block( args, filter );
}

DEFINE_API_IMPL( account_history_api_chainbase_impl, get_ops_in_block )
{
  return _get_ops_in_block( args );
}

DEFINE_API_IMPL( account_history_api_chainbase_impl, get_transaction )
{
#ifdef SKIP_BY_TX_ID
  FC_ASSERT( false, "This node's operator has disabled operation indexing by transaction_id" );
#else

  FC_ASSERT(args.include_reversible.valid() == false, "Supported only in AH-Rocksdb plugin");

  return _db.with_read_lock( [&]()
  {
    get_transaction_return result;

    const auto& idx = _db.get_index< chain::operation_index, chain::by_transaction_id >();
    auto itr = idx.lower_bound( args.id );
    if( itr != idx.end() && itr->trx_id == args.id )
    {
      auto blk = _db.fetch_block_by_number( itr->block );
      FC_ASSERT( blk.valid() );
      FC_ASSERT( blk->transactions.size() > itr->trx_in_block );
      result = blk->transactions[itr->trx_in_block];
      result.block_num       = itr->block;
      result.transaction_num = itr->trx_in_block;
    }
    else
    {
      FC_ASSERT( false, "Unknown Transaction ${t}", ("t",args.id) );
    }

    return result;
  });
#endif
}

get_account_history_return account_history_api_chainbase_impl::_get_account_history( const get_account_history_args& args, filter_type_opt filter )
{
  FC_ASSERT( args.limit <= 1000, "limit of ${l} is greater than maxmimum allowed", ("l",args.limit) );
  FC_ASSERT( args.start >= args.limit-1, "start must be greater than or equal to limit-1 (start is 0-based index)" );

  FC_ASSERT(args.include_reversible.valid() == false, "Supported only in AH-Rocksdb plugin");

  return _db.with_read_lock( [&]()
  {
    const auto& idx = _db.get_index< chain::account_history_index, chain::by_account >();
    auto itr = idx.lower_bound( boost::make_tuple( args.account, args.start ) );
    uint32_t n = 0;

    get_account_history_return result;
    while( true )
    {
      if( itr == idx.end() )
        break;
      if( itr->account != args.account )
        break;
      if( n >= args.limit )
        break;

      const chain::operation_object& op_obj = _db.get( itr->op );
      auto _op = api_operation_object::get_op( op_obj.serialized_op );
      api_operation_object api_obj( op_obj, std::move( api_operation_object::get_variant( _op ) ) );

      if( filter.valid() )
      {
        if( (*filter)( _op ) )
          result.history[ itr->sequence ] = std::move( api_obj );
      }
      else
      {
        result.history[ itr->sequence ] = std::move( api_obj );
      }
      ++itr;
      ++n;
    }

    return result;
  });
}

get_account_history_return account_history_api_chainbase_impl::get_account_history_filter( const get_account_history_args& args, filter_type_opt filter )
{
  return _get_account_history( args, filter );
}

DEFINE_API_IMPL( account_history_api_chainbase_impl, get_account_history )
{
  return _get_account_history( args );
}

DEFINE_API_IMPL( account_history_api_chainbase_impl, enum_virtual_ops )
{
  FC_ASSERT( false, "This API is not supported for account history backed by Chainbase" );
}

class account_history_api_sql_impl : public abstract_account_history_api_impl
{
  private:

    get_ops_in_block_return _get_ops_in_block( const get_ops_in_block_args&, filter_type_opt filter = filter_type_opt() );
    get_account_history_return _get_account_history( const get_account_history_args&, filter_type_opt filter = filter_type_opt() );

  public:
    account_history_api_sql_impl() :
    abstract_account_history_api_impl(), _dataSource( appbase::app().get_plugin< hive::plugins::account_history_sql::account_history_sql_plugin >() ) {}
    ~account_history_api_sql_impl() {}

    get_ops_in_block_return get_ops_in_block( const get_ops_in_block_args& ) override;
    get_transaction_return get_transaction( const get_transaction_args& ) override;
    get_account_history_return get_account_history( const get_account_history_args& ) override;
    enum_virtual_ops_return enum_virtual_ops( const enum_virtual_ops_args& ) override;

    get_ops_in_block_return get_ops_in_block_filter( const get_ops_in_block_args& args, filter_type_opt filter ) override;
    get_account_history_return get_account_history_filter( const get_account_history_args&, filter_type_opt filter ) override;

    const account_history_sql::account_history_sql_plugin& _dataSource;
};

get_ops_in_block_return account_history_api_sql_impl::_get_ops_in_block( const get_ops_in_block_args& args, filter_type_opt filter )
{
  get_ops_in_block_return result;

  _dataSource.get_ops_in_block( [ &result, &filter ] ( account_history_sql::account_history_sql_object& op )
                                {
                                  api_operation_object temp( op, std::move( op.op ) );
                                  auto _op = op.get_op();
                                  if( filter.valid() )
                                  {
                                    if( (*filter)( _op ) )
                                      result.ops.emplace( std::move( temp ) );
                                  }
                                  else
                                  {
                                    result.ops.emplace( std::move( temp ) );
                                  }
                                },
                                args.block_num, args.only_virtual, args.include_reversible );

  return result;
}

get_ops_in_block_return account_history_api_sql_impl::get_ops_in_block_filter( const get_ops_in_block_args& args, filter_type_opt filter )
{
  return _get_ops_in_block( args, filter );
}

DEFINE_API_IMPL( account_history_api_sql_impl, get_ops_in_block )
{
  return _get_ops_in_block( args );
}

DEFINE_API_IMPL( account_history_api_sql_impl, get_transaction )
{
#ifdef SKIP_BY_TX_ID
  FC_ASSERT(false, "This node's operator has disabled operation indexing by transaction_id");
#else

  get_transaction_return result;

  _dataSource.get_transaction( result, args.id, args.include_reversible );

  return result;
#endif
}

get_account_history_return account_history_api_sql_impl::_get_account_history( const get_account_history_args& args, filter_type_opt filter )
{
  FC_ASSERT( args.limit <= 1000, "limit of ${l} is greater than maxmimum allowed", ("l",args.limit) );
  FC_ASSERT( args.start >= args.limit-1, "start must be greater than or equal to limit-1 (start is 0-based index)" );

  get_account_history_return result;

  _dataSource.get_account_history( [ &result, &filter ] ( unsigned int sequence, account_history_sql::account_history_sql_object& op )
                                  {
                                    api_operation_object temp( op, std::move( op.op ) );
                                    auto _op = op.get_op();
                                    if( filter.valid() )
                                    {
                                      if( (*filter)( _op ) )
                                        result.history.emplace( sequence, std::move( temp ) );
                                    }
                                    else
                                    {
                                      result.history.emplace( sequence, std::move( temp ) );
                                    }
                                  },
                                  args.account, args.start, args.limit,
                                  args.include_reversible,
                                  args.operation_filter_low,
                                  args.operation_filter_high );

  return result;
}

get_account_history_return account_history_api_sql_impl::get_account_history_filter( const get_account_history_args& args, filter_type_opt filter )
{
  return _get_account_history( args, filter );
}

DEFINE_API_IMPL( account_history_api_sql_impl, get_account_history )
{
  return _get_account_history( args );
}

DEFINE_API_IMPL( account_history_api_sql_impl, enum_virtual_ops)
{
  enum_virtual_ops_return result;

  bool groupOps = args.group_by_block.valid() && *args.group_by_block;

  _dataSource.enum_virtual_ops( [ &result, &groupOps ] ( account_history_sql::account_history_sql_object& op )
                                {
                                  api_operation_object temp( op, std::move( op.op ) );
                                  temp.operation_id = op.operation_id;

                                  if( groupOps )
                                  {
                                    auto ii = result.ops_by_block.emplace( ops_array_wrapper( op.block ) );
                                    ops_array_wrapper& w = const_cast<ops_array_wrapper&>( *ii.first );

                                    if( ii.second )
                                    {
                                      w.timestamp = op.timestamp;
                                      w.irreversible = true;//NSY
                                    }
                                    w.ops.emplace_back( std::move( temp ) );
                                  }
                                  else
                                  {
                                    result.ops.emplace_back( std::move( temp ) );
                                  }
                                  
                                },
                                args.block_range_begin, args.block_range_end,
                                args.include_reversible,
                                args.operation_begin, args.limit,
                                args.filter,
                                result.next_block_range_begin,
                                result.next_operation_begin );

  return result;
}

class account_history_api_rocksdb_impl : public abstract_account_history_api_impl
{
  private:

    get_ops_in_block_return _get_ops_in_block( const get_ops_in_block_args&, filter_type_opt filter = filter_type_opt() );
    get_account_history_return _get_account_history( const get_account_history_args&, filter_type_opt filter = filter_type_opt() );

  public:
    account_history_api_rocksdb_impl() :
      abstract_account_history_api_impl(), _dataSource( appbase::app().get_plugin< hive::plugins::account_history_rocksdb::account_history_rocksdb_plugin >() ) {}
    ~account_history_api_rocksdb_impl() {}

    get_ops_in_block_return get_ops_in_block( const get_ops_in_block_args& ) override;
    get_transaction_return get_transaction( const get_transaction_args& ) override;
    get_account_history_return get_account_history( const get_account_history_args& ) override;
    enum_virtual_ops_return enum_virtual_ops( const enum_virtual_ops_args& ) override;

    get_ops_in_block_return get_ops_in_block_filter( const get_ops_in_block_args& args, filter_type_opt filter ) override;
    get_account_history_return get_account_history_filter( const get_account_history_args&, filter_type_opt filter ) override;

    const account_history_rocksdb::account_history_rocksdb_plugin& _dataSource;
};

get_ops_in_block_return account_history_api_rocksdb_impl::_get_ops_in_block( const get_ops_in_block_args& args, filter_type_opt filter )
{
  get_ops_in_block_return result;

  bool include_reversible = args.include_reversible.valid() ? *args.include_reversible : false;
  _dataSource.find_operations_by_block(args.block_num, include_reversible,
    [&result, &args, &filter](const account_history_rocksdb::rocksdb_operation_object& op)
    {
      auto _op = api_operation_object::get_op( op.serialized_op );
      api_operation_object temp( op, std::move( api_operation_object::get_variant( _op ) ) );
      if( !args.only_virtual || temp.virtual_op )
      {
        if( filter.valid() )
        {
          if( (*filter)( _op ) )
            result.ops.emplace( std::move( temp ) );
        }
        else
        {
          result.ops.emplace( std::move( temp ) );
        }
      }
    }
  );
  return result;
}

get_ops_in_block_return account_history_api_rocksdb_impl::get_ops_in_block_filter( const get_ops_in_block_args& args, filter_type_opt filter )
{
  return _get_ops_in_block( args, filter );
}

DEFINE_API_IMPL( account_history_api_rocksdb_impl, get_ops_in_block )
{
  return _get_ops_in_block( args );
}

get_account_history_return account_history_api_rocksdb_impl::_get_account_history( const get_account_history_args& args, filter_type_opt filter )
{
  FC_ASSERT( args.limit <= 1000, "limit of ${l} is greater than maxmimum allowed", ("l",args.limit) );
  FC_ASSERT( args.start >= args.limit-1, "start must be greater than or equal to limit-1 (start is 0-based index)" );

  get_account_history_return result;

  bool include_reversible = args.include_reversible.valid() ? *args.include_reversible : false;
  
  unsigned int total_processed_items = 0;

  if(args.operation_filter_low || args.operation_filter_high)
  {
    uint64_t filter_low = args.operation_filter_low ? *args.operation_filter_low : 0;
    uint64_t filter_high = args.operation_filter_high ? *args.operation_filter_high : 0;

    try
    {

    _dataSource.find_account_history_data(args.account, args.start, args.limit, include_reversible,
      [&result, filter_low, filter_high, &total_processed_items, &filter](unsigned int sequence, const account_history_rocksdb::rocksdb_operation_object& op) -> bool
      {
        FC_ASSERT(total_processed_items < 2000, "Could not find filtered operation in ${total_processed_items} operations, to continue searching, set start=${sequence}.",("total_processed_items",total_processed_items)("sequence",sequence));

        // we want to accept any operations where the corresponding bit is set in {filter_high, filter_low}
        auto _op = api_operation_object::get_op( op.serialized_op );
        api_operation_object api_op( op, std::move( api_operation_object::get_variant( _op ) ) );
        unsigned bit_number = _op.which();
        bool accepted = bit_number < 64 ? filter_low & (UINT64_C(1) << bit_number)
                                        : filter_high & (UINT64_C(1) << (bit_number - 64));

        ++total_processed_items;

        if(accepted)
        {
          if( filter.valid() )
          {
            if( (*filter)( _op ) )
            {
              result.history.emplace(sequence, std::move(api_op));
            }
          }
          else
          {
            result.history.emplace(sequence, std::move(api_op));
          }
          return true;
        }
        else
        {
          return false;
        }
      });
    }
    catch(const fc::exception& e)
    { //if we have some results but not all requested, return what we have
      //but if no results, throw an exception that gives a starting item for further searching via pagination
      if (result.history.empty())
	throw;
    }
  }
  else
  {
    _dataSource.find_account_history_data(args.account, args.start, args.limit, include_reversible,
      [&result, &filter](unsigned int sequence, const account_history_rocksdb::rocksdb_operation_object& op) -> bool
      {
        auto _op = api_operation_object::get_op( op.serialized_op );
        api_operation_object api_op( op, std::move( api_operation_object::get_variant( _op ) ) );
        /// Here internal counter (inside find_account_history_data) does the limiting job.
        if( filter.valid() )
        {
          if( (*filter)( _op ) )
          {
            result.history[sequence] = std::move( api_op );
          }
        }
        else
        {
          result.history[sequence] = std::move( api_op );
        }
        return true;
      });
  }

  return result;
}

get_account_history_return account_history_api_rocksdb_impl::get_account_history_filter( const get_account_history_args& args, filter_type_opt filter )
{
  return _get_account_history( args, filter );
}

DEFINE_API_IMPL( account_history_api_rocksdb_impl, get_account_history )
{
  return _get_account_history( args );
}

DEFINE_API_IMPL( account_history_api_rocksdb_impl, get_transaction )
{
#ifdef SKIP_BY_TX_ID
  FC_ASSERT(false, "This node's operator has disabled operation indexing by transaction_id");
#else
  uint32_t blockNo = 0;
  uint32_t txInBlock = 0;

  bool include_reversible = args.include_reversible.valid() ? *args.include_reversible : false;

  if(_dataSource.find_transaction_info(args.id, include_reversible, &blockNo, &txInBlock))
    {
    get_transaction_return result;
    _db.with_read_lock([this, blockNo, txInBlock, &result]()
    {
    auto blk = _db.fetch_block_by_number(blockNo);
    FC_ASSERT(blk.valid());
    FC_ASSERT(blk->transactions.size() > txInBlock);
    result = blk->transactions[txInBlock];
    result.block_num = blockNo;
    result.transaction_num = txInBlock;
    });

    return result;
    }
  else
    {
    FC_ASSERT(false, "Unknown Transaction ${t}", ("t", args.id));
    }
#endif

}

#define CHECK_OPERATION( r, data, CLASS_NAME ) \
  void operator()( const hive::protocol::CLASS_NAME& op ) { _accepted = (_filter & enum_vops_filter::CLASS_NAME) == enum_vops_filter::CLASS_NAME; }

#define CHECK_OPERATIONS( CLASS_NAMES ) \
  BOOST_PP_SEQ_FOR_EACH( CHECK_OPERATION, _, CLASS_NAMES )

struct filtering_visitor
{
  typedef void result_type;

  bool check(uint32_t filter, const hive::protocol::operation& op)
  {
    _filter = filter;
    _accepted = false;
    op.visit(*this);

    return _accepted;
  }

  template< typename T >
  void operator()( const T& ) { _accepted = false; }

  CHECK_OPERATIONS( (fill_convert_request_operation)(author_reward_operation)(curation_reward_operation)
  (comment_reward_operation)(liquidity_reward_operation)(interest_operation)
  (fill_vesting_withdraw_operation)(fill_order_operation)(shutdown_witness_operation)
  (fill_transfer_from_savings_operation)(hardfork_operation)(comment_payout_update_operation)
  (return_vesting_delegation_operation)(comment_benefactor_reward_operation)(producer_reward_operation)
  (clear_null_account_balance_operation)(proposal_pay_operation)(sps_fund_operation)
  (hardfork_hive_operation)(hardfork_hive_restore_operation)(delayed_voting_operation)
  (consolidate_treasury_balance_operation)(effective_comment_vote_operation)(ineffective_delete_comment_operation)
  (sps_convert_operation) )

private:
  uint32_t _filter = 0;
  bool     _accepted = false;
};

DEFINE_API_IMPL( account_history_api_rocksdb_impl, enum_virtual_ops)
{
  enum_virtual_ops_return result;

  bool groupOps = args.group_by_block.valid() && *args.group_by_block;

  bool include_reversible = args.include_reversible.valid() ? *args.include_reversible : false;

  std::pair< uint32_t, uint64_t > next_values = _dataSource.enum_operations_from_block_range(args.block_range_begin,
    args.block_range_end, include_reversible, args.operation_begin, args.limit,
    [groupOps, &result, &args ](const account_history_rocksdb::rocksdb_operation_object& op, uint64_t operation_id, bool irreversible)
    {

      if( args.filter.valid() )
      {
        auto _op = api_operation_object::get_op( op.serialized_op );
        api_operation_object _api_obj( op, std::move( api_operation_object::get_variant( _op ) ) );

        _api_obj.operation_id = operation_id;

        filtering_visitor accepting_visitor;

        if(accepting_visitor.check(*args.filter, _op))
        {
          if(groupOps)
          {
            auto ii = result.ops_by_block.emplace(op.block);
            ops_array_wrapper& w = const_cast<ops_array_wrapper&>(*ii.first);

            if(ii.second)
            {
              w.timestamp = op.timestamp;
              w.irreversible = irreversible;
            }
            
            w.ops.emplace_back(std::move(_api_obj));
          }
          else
          {
            result.ops.emplace_back(std::move(_api_obj));
          }
          return true;
        }
        else
          return false;
      }
      else
      {
        if(groupOps)
        {
          auto ii = result.ops_by_block.emplace(op.block);
          ops_array_wrapper& w = const_cast<ops_array_wrapper&>(*ii.first);

          if(ii.second)
          {
            w.timestamp = op.timestamp;
            w.irreversible = irreversible;
          }

          api_operation_object _api_obj(op);
          _api_obj.operation_id = operation_id;
          w.ops.emplace_back(std::move(_api_obj));
        }
        else
        {
          api_operation_object _api_obj(op);
          _api_obj.operation_id = operation_id;

          result.ops.emplace_back(std::move(_api_obj));
        }
        return true;
      }
    }
  );

  result.next_block_range_begin = next_values.first;
  result.next_operation_begin = next_values.second;

  return result;
}

} // detail

account_history_api::account_history_api()
{
  auto ah_cb = appbase::app().find_plugin< hive::plugins::account_history::account_history_plugin >();
  auto ah_rocks = appbase::app().find_plugin< hive::plugins::account_history_rocksdb::account_history_rocksdb_plugin >();
  auto ah_sql = appbase::app().find_plugin< hive::plugins::account_history_sql::account_history_sql_plugin >();

  if( ah_rocks != nullptr && ah_sql != nullptr )
  {
    FC_ASSERT( false, "Only one plugin can be enabled - account_history_rocksdb or account_history_sql" );
  }

  if( ah_rocks != nullptr )
  {
    if( ah_cb != nullptr )
      wlog( "account_history and account_history_rocksdb plugins are both enabled. account_history_api will query from account_history_rocksdb" );

    my = std::make_unique< detail::account_history_api_rocksdb_impl >();
  }
  else if( ah_sql != nullptr )
  {
    if( ah_cb != nullptr )
      wlog( "account_history and account_history_sql plugins are both enabled. account_history_api will query from account_history_sql" );

    my = std::make_unique< detail::account_history_api_sql_impl >();
  }
  else if( ah_cb != nullptr )
  {
    my = std::make_unique< detail::account_history_api_chainbase_impl >();
  }
  else
  {
    FC_ASSERT( false, "Account History API only works if account_history or account_history_rocksdb/account_history_sql plugins are enabled" );
  }

  JSON_RPC_REGISTER_API( HIVE_ACCOUNT_HISTORY_API_PLUGIN_NAME );
}

account_history_api::~account_history_api() {}

get_ops_in_block_return account_history_api::get_ops_in_block_filter( const get_ops_in_block_args& args, types::filter_type_opt filter )
{
  FC_ASSERT( my, "Account History API is not initialized" );
  return my->get_ops_in_block_filter( args, filter );
}

get_account_history_return account_history_api::get_account_history_filter( const get_account_history_args& args, types::filter_type_opt filter )
{
  FC_ASSERT( my, "Account History API is not initialized" );
  return my->get_account_history_filter( args, filter );
}

DEFINE_LOCKLESS_APIS( account_history_api ,
  (get_ops_in_block)
  (get_transaction)
  (get_account_history)
  (enum_virtual_ops)
)

} } } // hive::plugins::account_history
