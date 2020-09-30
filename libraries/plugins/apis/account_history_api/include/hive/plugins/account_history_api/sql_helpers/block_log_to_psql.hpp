#pragma once

#include "sql_serializer_helper.hpp"

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
					params.command_queue.wait_push(
						command{BLOCKS, "( " + block_number_str + " , '" + block.id().str() + "' , '" + block.timestamp.to_iso_string() + "' )"});
					for (int64_t tx = 0; tx < static_cast<int64_t>(block.transactions.size()); tx++)
					{
						auto &trx = block.transactions[tx];
						params.command_queue.wait_push(
							command{TRANSACTIONS,
									"( " + block_number_str +
										", " + std::to_string(tx) +
										", '" + trx.id().str().c_str() + "' )"});
						for (int64_t i = 0; i < static_cast<int64_t>(trx.operations.size()); i++)
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

			for (const auto &kv : names_to_flush)
				params.command_queue.wait_push(command{
					OPERATION_NAMES,
					"( " + std::to_string(kv.first) + ", '" + kv.second + "')"});

			std::cout << "block_log thread finished" << std::endl;
		}};
	}

	std::thread rip_vops_to_psql(const psql_serialization_params &params, const account_history_rocksdb_plugin &_dataSource)
	{
		return std::thread{[&]() -> void {
			_dataSource.enum_operations_from_block_range(
				params.start_block,
				params.end_block,
				fc::optional<uint64_t>{}, std::numeric_limits<uint32_t>::max(),
				[&](const rocksdb_operation_object &op, uint64_t operation_id) -> bool {
					aob _api_obj{op};
					_api_obj.operation_id = operation_id;
					// std::cout << "trx pos vops: " << _api_obj.trx_in_block << ", " << _api_obj.virtual_op << ", " << _api_obj.block << std::endl;
					const sql_command_t vec = _api_obj.op.visit(PSQL::sql_serializer_visitor{_api_obj.block, _api_obj.trx_in_block, _api_obj.op_in_trx, _api_obj.op.which()});
					for (const auto &v : vec)
						params.command_queue.wait_push(v);
					return true;
				});
			std::cout << "vop thread finished" << std::endl;
		}};
	}

	void async_db_pusher(const std::string &db_con_str, queue<command> &src_queue)
	{
		connection conn{db_con_str.c_str()};
		FC_ASSERT(conn.is_open());
		try
		{
			auto flusher = [&](const TABLE k) {
				auto &it = flushes.find(k)->second;
				if (it.to_dump->empty())
					return;
				std::stringstream ss;
				std::lock_guard<std::mutex> lck{*(it).mtx};

				std::string str;
				str = it.to_dump->front().c_str();
				it.to_dump->pop();
				ss << "INSERT INTO " << it.table_name << " VALUES " << str;
				while (!it.to_dump->empty())
				{
					str = it.to_dump->front().c_str();
					it.to_dump->pop();
					ss << "," << str;
				}
				nontransaction w{conn};
				w.exec(ss.str());
				// it.second.clear();
				// it.second.reserve(buff_size);
			};

			command ret;
			src_queue.wait_pull(ret);
			while (ret.first != TABLE::END)
			{
				{
					std::lock_guard<std::mutex> lck{*(flushes.at(ret.first).mtx)};
					flushes.at(ret.first).to_dump->emplace(ret.second);
				}
				if (flushes.at(ret.first).to_dump->size() == buff_size)
					flusher(ret.first);
				src_queue.wait_pull(ret);
			}
			std::cout << "flush break!" << std::endl;

			for (auto &kv : flushes)
				flusher(kv.first);
		}
		catch (const std::exception &e)
		{
			std::cout << "catched exception: " << e.what() << std::endl;
		}
		catch (...)
		{
			std::cout << "exception in worker thread" << std::endl;
		}
		std::cout << "flush worker thread finished" << std::endl;
	}
}; // namespace PSQL