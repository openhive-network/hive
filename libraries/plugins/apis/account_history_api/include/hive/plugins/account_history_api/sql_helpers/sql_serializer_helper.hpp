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

  std::map<uint64_t, std::string> names_to_flush;

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

  using que_str = std::queue<std::string, std::list<std::string> >;
  using mtx_t = std::mutex;
  struct p_str_que_str
  {
    std::string table_name;
    std::shared_ptr<que_str> to_dump{new que_str{}};
    std::shared_ptr<mtx_t> mtx{ new mtx_t{} };

    explicit p_str_que_str(const std::string& s) : table_name{s} {}
  };
  using table_flush_values_cell_t = std::pair<const TABLE, p_str_que_str>;
  const size_t buff_size = 1000;

  const std::map<TABLE, p_str_que_str> flushes{
    {
      table_flush_values_cell_t{ BLOCKS, 				p_str_que_str{ "hive_blocks" } },
      table_flush_values_cell_t{ TRANSACTIONS, 		p_str_que_str{"hive_transactions"} },
      table_flush_values_cell_t{ ACCOUNTS, 			p_str_que_str{"hive_accounts"} },
      table_flush_values_cell_t{ OPERATIONS, 			p_str_que_str{"hive_operations"} },
      table_flush_values_cell_t{ OPERATION_NAMES, 	p_str_que_str{"hive_operation_names"} },
      table_flush_values_cell_t{ DETAILS_ASSET, 		p_str_que_str{"hive_operation_details_asset"} },
      table_flush_values_cell_t{ DETAILS_PERMLINK,	p_str_que_str{"hive_operation_details_permlink"} },
      table_flush_values_cell_t{ DETAILS_ID, 			p_str_que_str{"hive_operation_details_id"} },
      table_flush_values_cell_t{ ASSET_DICT, 			p_str_que_str{"hive_asset_dictionary"} },
      table_flush_values_cell_t{ PERMLINK_DICT, 		p_str_que_str{"hive_permlink_dictionary"} }
    }
  };

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

    command log() const
    {
      // std::cout << "{{ " << first << " ; " << second << " }}" << std::endl;
      return *this;
    }
  };

  struct psql_serialization_params
  {
    uint32_t start_block;
    uint32_t end_block;
    queue<command> &command_queue;
    hive::chain::database &_db;
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

    template <typename operation_t>
    sql_command_t operator()(const operation_t &op) const
    {
      add_type_id<operation_t>();
      return sql_command_t{
          command{
              TABLE::OPERATIONS,
              generate([&](ssman &ss) { ss << "( " << block_num << ", " << trx_pos << ", " << op_position << ", " << tag << ", False, '{}'" << ")"; })}};
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
      // const std::string s{ ss.str() };
      //   std::cout << "generated: " << s << std::endl;
      return ss.str();
    }
  };

}; // namespace PSQL
