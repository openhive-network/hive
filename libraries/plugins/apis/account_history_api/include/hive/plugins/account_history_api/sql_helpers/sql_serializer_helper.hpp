#pragma once

// STL 
#include <string>
#include <functional>
#include <sstream>

// Internal
#include <hive/chain/database.hpp>
#include <hive/chain/util/impacted.hpp>

// Boost
#include <boost/thread/sync_queue.hpp>
#include <boost/type_index.hpp>

namespace PSQL
{
  template<typename T>
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

  using table_flush_values_cell_t = std::pair<const TABLE, std::pair<std::string, std::vector<std::string>>>;
  const size_t buff_size = 1000;

  std::map<TABLE, std::pair<std::string, std::vector<std::string> > > flushes
  {
    table_flush_values_cell_t{ BLOCKS, std::pair< std::string, std::vector<std::string> >{ "hive_blocks", std::vector<std::string>{} } },
    table_flush_values_cell_t{ TRANSACTIONS, {"hive_transactions", {}} },
    table_flush_values_cell_t{ ACCOUNTS, {"hive_accounts", {}} },
    table_flush_values_cell_t{ OPERATIONS, {"hive_operations", {}} },
    table_flush_values_cell_t{ OPERATION_NAMES, {"hive_operation_names", {}} },
    table_flush_values_cell_t{ DETAILS_ASSET, {"hive_operation_details_asset", {}} },
    table_flush_values_cell_t{ DETAILS_PERMLINK, {"hive_operation_details_permlink", {}} },
    table_flush_values_cell_t{ DETAILS_ID, {"hive_operation_details_id", {}} },
    table_flush_values_cell_t{ ASSET_DICT, {"hive_asset_dictionary", {}} },
    table_flush_values_cell_t{ PERMLINK_DICT, {"hive_permlink_dictionary", {}} }
  };

  struct command
  {
    TABLE first = END;
    std::string second = "";
  };

  struct psql_serialization_params
  {
    uint32_t start_block;
    uint32_t end_block;
    queue<command>& command_queue;
    hive::chain::database& _db;
  };

  using sql_command_t = std::vector<command>;
  using ssman = std::stringstream;
  using namespace hive::protocol;

  struct sql_serializer_visitor
  {
    typedef sql_command_t result_type;

    size_t block_num = 0;
    size_t trx_pos = 0;
    size_t op_position = 0;
    int64_t tag = 0;

    template<typename operation_t>
    sql_command_t operator()( const operation_t& op ) const 
    {
      add_type_id<operation_t>();
      return sql_command_t{ 
        command{ 
          TABLE::OPERATIONS,
          generate([&](ssman& ss){ ss << "( " << block_num <<", " << trx_pos << ", " << op_position << ", " << tag << ")" ;})
        } 
      }; 
    }

  private:

    template<typename T>
    void add_type_id() const
    {
      names_to_flush[tag] = boost::typeindex::type_id<T>().pretty_name();
    }

    std::string generate( std::function<void(ssman&)> fun ) const
    {
      std::stringstream ss;
      fun(ss);
      return ss.str();
    }
  };

};
