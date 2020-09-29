#pragma once

#include <hive/plugins/account_history_rocksdb/account_history_rocksdb_plugin.hpp>
#include <hive/plugins/account_history_api/account_history_api.hpp>
#include "sql_serializer_helper.hpp"

// STL
#include <thread>
#include <future>
#include <sstream>
#include <string>

// PSQL
#include <pqxx/pqxx>

namespace PSQL
{
  using hive::plugins::account_history_rocksdb::account_history_rocksdb_plugin;
  using hive::plugins::account_history_rocksdb::rocksdb_operation_object;
  using aob = hive::plugins::account_history::api_operation_object;
  using hive::protocol::signed_block;
  using namespace pqxx;
  using namespace PSQL;

  std::thread rip_block_log_to_psql(const psql_serialization_params &params)
  {
    return std::thread{[&]() -> void {
      params._db.get_block_log().iterate_over_block_log(fc::optional<fc::path>(), [&](const hive::protocol::signed_block &block) -> bool {
        if (block.block_num() < params.start_block)
          return true;
        if (block.block_num() < params.end_block)
        {
          const std::string block_number_str{std::to_string(block.block_num())};
          params.command_queue.push(
              command{BLOCKS, "( " + block_number_str + " , '" + block.id().str() + "' , '" + block.timestamp.to_iso_string() + "' )"});
          for (size_t tx = 0; tx < block.transactions.size(); tx++)
          {
            auto& trx = block.transactions[tx];
            params.command_queue.push(
                command{TRANSACTIONS,
                        "( " + block_number_str +
                            ", " + std::to_string(tx) +
                            ", '" + trx.id().str().c_str() + "' )"});
            for (size_t i = 0; i < trx.operations.size(); i++)
            {
              const sql_command_t vec = trx.operations[i].visit(PSQL::sql_serializer_visitor{block.block_num(), tx, i, trx.operations[i].which()});
              for (const auto &v : vec)
                params.command_queue.push(v);
            }
          }
          return true;
        }
        else
          return false;
      });
    }};
  }

  std::thread rip_vops_to_psql(const psql_serialization_params &params, const account_history_rocksdb_plugin &_dataSource)
  {
    return std::thread{[&]() ->void {
    _dataSource.enum_operations_from_block_range(
        params.start_block,
        params.end_block,
        fc::optional<uint64_t>{}, std::numeric_limits<uint32_t>::max(),
        [&](const rocksdb_operation_object &op, uint64_t operation_id) -> bool {
          aob _api_obj{op};
          _api_obj.operation_id = operation_id;

          const sql_command_t vec = _api_obj.op.visit(PSQL::sql_serializer_visitor{_api_obj.block, _api_obj.trx_in_block, _api_obj.op_in_trx, _api_obj.op.which()});
          for (const auto &v : vec)
            params.command_queue.push(v);
          return true;
        });
    }};
  }

  std::future<void> async_db_pusher(const std::string& db_con_str, queue<command>& src_queue)
  {
    return std::async(
      std::launch::async, [&]() -> void {
        connection conn{db_con_str.c_str()};
        FC_ASSERT(conn.is_open());
        try
        {
          auto flusher = [&](const TABLE k) {
            auto &it = flushes.find(k)->second;
            if (it.second.empty())
              return;
            std::stringstream ss;
            ss << "INSERT INTO " << it.first << " VALUES " << it.second[0];
            for (size_t i = 1; i < it.second.size(); i++)
              ss << "," << it.second[i];
            nontransaction w{conn};
            w.exec(ss.str());
            it.second.clear();
            it.second.reserve(buff_size);
          };

          command ret;
          src_queue.pull(ret);
          while (ret.first != TABLE::END)
          {
            flushes[ret.first].second.emplace_back(ret.second);
            if (flushes[ret.first].second.size() == buff_size)
              flusher(ret.first);
            src_queue.pull(ret);
          }

          for (auto &kv : flushes)
            flusher(kv.first);
        }
        catch (...)
        {
          std::cout << "exception in worker thread" << std::endl;
        }
      }
    );
  }
};