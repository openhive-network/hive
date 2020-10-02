#pragma once

// STL
#include <thread>
#include <queue>
#include <future>
#include <sstream>
#include <string>
#include <functional>
#include <shared_mutex>

// Internal
#include <hive/chain/database.hpp>
#include <hive/chain/util/impacted.hpp>
#include <hive/plugins/account_history_rocksdb/account_history_rocksdb_plugin.hpp>

// Boost
#include <boost/thread/sync_queue.hpp>
#include <boost/type_index.hpp>

namespace PSQL
{
  template <typename T>
  using queue = boost::concurrent::sync_queue<T>;
  using hive::plugins::account_history_rocksdb::account_history_rocksdb_plugin;
  using hive::plugins::account_history_rocksdb::rocksdb_operation_object;
  using aob = hive::plugins::account_history::api_operation_object;
  using hive::protocol::signed_block;

  using namespace PSQL;

  std::map<uint64_t, std::string> names_to_flush;
  std::set< hive::protocol::asset_symbol_type > assets_to_flush; 

  enum TABLE
  {
    // basics
    BLOCKS = 0,
    TRANSACTIONS = 1,
    ACCOUNTS = 2,

    // operation stuff
    OPERATIONS = 3,
    OPERATION_NAMES = 4,

    // operation details
    DETAILS_ASSET = 5,
    DETAILS_PERMLINK = 6,
    DETAILS_ID = 7,

    // let's safe some place
    ASSET_DICT = 8,
    PERMLINK_DICT = 9,

    END
  };

  using que_str = queue<std::string>;
  using mtx_t = std::mutex;
  struct p_str_que_str
  {
    std::string table_name;
    std::shared_ptr<que_str> to_dump{new que_str{}};
    std::shared_ptr<mtx_t> mtx{new mtx_t{}};

    explicit p_str_que_str(const std::string &s) : table_name{s} {}
  };
  using table_flush_values_cell_t = std::pair<const TABLE, p_str_que_str>;
  const size_t buff_size = 1000;

  const std::map<TABLE, p_str_que_str> flushes{
      {table_flush_values_cell_t{BLOCKS, p_str_que_str{"hive_blocks"}},
       table_flush_values_cell_t{TRANSACTIONS, p_str_que_str{"hive_transactions"}},
       table_flush_values_cell_t{ACCOUNTS, p_str_que_str{"hive_accounts"}},
       table_flush_values_cell_t{OPERATIONS, p_str_que_str{"hive_operations"}},
       table_flush_values_cell_t{OPERATION_NAMES, p_str_que_str{"hive_operation_names"}},
       table_flush_values_cell_t{DETAILS_ASSET, p_str_que_str{"hive_operation_details_asset"}},
       table_flush_values_cell_t{DETAILS_PERMLINK, p_str_que_str{"hive_operation_details_permlink"}},
       table_flush_values_cell_t{DETAILS_ID, p_str_que_str{"hive_operation_details_id"}},
       table_flush_values_cell_t{ASSET_DICT, p_str_que_str{"hive_asset_dictionary"}},
       table_flush_values_cell_t{PERMLINK_DICT, p_str_que_str{"hive_permlink_dictionary"}}}};

  void init_flushes()
  {
    FC_ASSERT(flushes.size() > 0);
    if (flushes.size() == 0)
      return;
    // flushes.insert(table_flush_values_cell_t{ BLOCKS, p_str_que_str{"hive_blocks"} } );
    // flushes[TRANSACTIONS] = p_str_que_str{"hive_transactions"};
    // flushes[ACCOUNTS] = p_str_que_str{"hive_accounts"};
    // flushes[OPERATIONS] = p_str_que_str{"hive_operations"};
    // flushes[OPERATION_NAMES] = p_str_que_str{"hive_operation_names"};
    // flushes[DETAILS_ASSET] = p_str_que_str{"hive_operation_details_asset"};
    // flushes[DETAILS_PERMLINK] = p_str_que_str{"hive_operation_details_permlink"};
    // flushes[DETAILS_ID] = p_str_que_str{"hive_operation_details_id"};
    // flushes[ASSET_DICT] = p_str_que_str{"hive_asset_dictionary"};
    // flushes[PERMLINK_DICT] = p_str_que_str{"hive_permlink_dictionary"};
  }

  struct command
  {
    TABLE first = END;
    std::string second = "";

    command() = default;
    command( const TABLE tbl, const std::string& str ) : first{tbl}, second{str} {}

    command log() const
    {
      // std::cout << "{{ " << first << " ; " << second << " }}" << std::endl;
      return *this;
    }
  };

  struct post_process_command
  {
    using optional_op_t = fc::optional<rocksdb_operation_object>;
    post_process_command() : op{} {}
    post_process_command(const rocksdb_operation_object &obj, const uint64_t _op_id) : op{obj}, op_id{_op_id} {}
    optional_op_t op;
    uint64_t op_id = ~(0ull);
  };

  struct psql_serialization_params
  {
    uint32_t start_block;
    uint32_t end_block;
    queue<command> &command_queue;
    hive::chain::database &_db;

    bool move_range(const uint32_t bc, const uint32_t upper_bound)
    {
      if (upper_bound == end_block)
        return false;
      start_block = end_block;
      end_block = std::min(start_block + bc, upper_bound);
      std::cout << "NEW RANGE: " << start_block << " " << end_block << std::endl;
      return true;
    }
  };

  using sql_command_t = std::vector<command>;
  using ssman = std::stringstream;
  using namespace hive::protocol;

  struct sql_serializer_visitor
  {
    typedef sql_command_t result_type;

    int64_t block_num = 0;
    int64_t trx_pos = 0;
    int64_t op_position = 0;
    int64_t tag = 0;
    bool is_virtual = false;
    const std::vector<account_name_type> &impacted;

    template <typename operation_t>
    sql_command_t operator()(const operation_t &op) const
    {
      add_type_id<operation_t>();
      auto ret = get_basic_info(op);
      process(op, ret);
      return ret;
    }

    template <typename operation_t>
    sql_command_t get_basic_info(const operation_t &op) const
    {
      return sql_command_t{
          command{
              TABLE::OPERATIONS,
              generate([&](ssman &ss) { ss << "( " << block_num << ", " << trx_pos << ", " << op_position << ", " << tag <<  ", " << ( is_virtual ? "True" : "False" ) << ", " << 
              generate([&](ssman& ss2) { 
                if(impacted.size() == 0) {ss2 << "'{}'"; return; }
                ss2 << "'{\"" << std::string{ impacted.front() } << '"';
                for(size_t i = 1; i < impacted.size(); i++)
                  ss2 << ", \"" << std::string{ impacted[i] } << '"';
                ss2 << "}'";
              })
              << ")"; })}};
    }

    void get_asset_details(const asset& a, sql_command_t& ret) const
    {
      assets_to_flush.insert(a.symbol);
      ret.emplace_back( 
        TABLE::DETAILS_ASSET,
        generate([&](ssman & s){ s << "( " << block_num << ", " << trx_pos << ", " << op_position << ", " << a.amount.value << ", " << a.symbol.asset_num << " )"; })
      );
    }

    template<typename num_t>
    void get_id_details(const num_t num, sql_command_t& ret) const
    {
      static_assert( std::is_arithmetic<num_t>() );
      ret.emplace_back(
        TABLE::DETAILS_ID,
        generate([&](ssman & s){ s << "( " << block_num << ", " << trx_pos << ", " << op_position << ", " << num << " )"; })
      );
    }

    void get_permlink_details(const fc::string& str, sql_command_t& ret) const
    {
      ret.emplace_back(
        TABLE::DETAILS_PERMLINK,
        generate([&](ssman & s){ s << "( " << block_num << ", " << trx_pos << ", " << op_position << ", '" << str << "' )"; })
      );
    }


  private:

    template <typename T>
    void add_type_id() const
    {
      names_to_flush[tag] = boost::typeindex::type_id<T>().pretty_name();
    }

    std::string generate(std::function<void(ssman &)> fun) const
    {
      std::stringstream ss;
      fun(ss);
      return ss.str();
    }
    
/*
 "ops_behavior": {
            "transfer_to_vesting_operation": ["amount"],
            "transfer_operation": ["amount"],
            "account_witness_vote_operation": ["approve"],
            "comment_operation": ["permlink"],
            "vote_operation": ["permlink"],
            "withdraw_vesting_operation": ["vesting_shares"],
            "custom_operation": "id",
            "limit_order_create_operation": ["orderid"],
            "limit_order_cancel_operation": ["orderid"],
            "custom_json_operation": ["id"],
            "delete_comment_operation": ["permlink"],
            "set_withdraw_vesting_route_operation": ["auto_vest"],
            "convert_operation": ["requestid"],
        }
*/
    template<typename operation_t> void process(const operation_t& op, sql_command_t& ret) const {}
  };
}; // namespace PSQL

template<> void PSQL::sql_serializer_visitor::process(const transfer_to_vesting_operation& op, sql_command_t& ret) const
{
  get_asset_details( op.amount, ret );
}

template<> void PSQL::sql_serializer_visitor::process(const transfer_operation& op, sql_command_t& ret) const
{
  get_asset_details( op.amount, ret );
}

template<> void PSQL::sql_serializer_visitor::process(const account_witness_vote_operation& op, sql_command_t& ret) const
{
  get_id_details( op.approve, ret );
}

template<> void PSQL::sql_serializer_visitor::process(const comment_operation& op, sql_command_t& ret) const
{
  get_permlink_details( op.permlink, ret );
}

template<> void PSQL::sql_serializer_visitor::process(const vote_operation& op, sql_command_t& ret) const
{
  get_permlink_details( op.permlink, ret );
}

template<> void PSQL::sql_serializer_visitor::process(const withdraw_vesting_operation& op, sql_command_t& ret) const
{
  get_asset_details( op.vesting_shares, ret );
}

template<> void PSQL::sql_serializer_visitor::process(const custom_operation& op, sql_command_t& ret) const
{
  get_id_details( op.id, ret );
}

template<> void PSQL::sql_serializer_visitor::process(const limit_order_create_operation& op, sql_command_t& ret) const
{
  get_id_details( op.orderid, ret );
}

template<> void PSQL::sql_serializer_visitor::process(const limit_order_cancel_operation& op, sql_command_t& ret) const
{
  get_id_details( op.orderid, ret );
}

template<> void PSQL::sql_serializer_visitor::process(const delete_comment_operation& op, sql_command_t& ret) const
{
  get_permlink_details( op.permlink, ret );
}

template<> void PSQL::sql_serializer_visitor::process(const set_withdraw_vesting_route_operation& op, sql_command_t& ret) const
{
  get_id_details( op.percent, ret );
}

template<> void PSQL::sql_serializer_visitor::process(const convert_operation& op, sql_command_t& ret) const
{
  get_id_details( op.requestid, ret );
}

