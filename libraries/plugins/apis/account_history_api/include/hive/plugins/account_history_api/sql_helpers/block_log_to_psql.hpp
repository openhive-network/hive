#pragma once

#include "sql_serializer_helper.hpp"

// PSQL
#include <pqxx/pqxx>

namespace PSQL
{
	using namespace pqxx;

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
							fc::flat_set<account_name_type> ret;
							hive::app::operation_get_impacted_accounts(trx.operations[i], ret);
							const sql_command_t vec = trx.operations[i].visit(
								PSQL::sql_serializer_visitor{
									block.block_num(), /* block number */
									tx, /* trx number in block */
									i, /* op number in transaction */
									trx.operations[i].which(), /* operation type */
									false,  /* is_virtual */
									std::vector<account_name_type>( ret.begin(), ret.end() ) /* impacted */
								}
							);
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

	std::thread post_process(queue<post_process_command> &qu, queue<command> &command_queue)
	{
		return std::thread{[&]() {
			// std::mutex mtx;
			auto fun = [&]() {
				post_process_command ret;
				qu.wait_pull(ret);
				while (ret.op.valid())
				{
					aob _api_obj{*ret.op};
					_api_obj.operation_id = ret.op_id;
					const sql_command_t vec = _api_obj.op.visit(
						PSQL::sql_serializer_visitor{
							_api_obj.block, 
							_api_obj.trx_in_block, 
							_api_obj.op_in_trx, 
							_api_obj.op.which(), /* operation type */
							true, /* is_virtual */
							ret.op->impacted /* impacted */
						}
					);
					for (const auto &v : vec)
						command_queue.wait_push(v);
					qu.wait_pull(ret);
				}
				std::cout << "vop support thread finished" << std::endl;
			};

			std::array< std::shared_ptr< std::thread >, 1 > threads;
			for (auto &th : threads)
				th = std::make_shared<std::thread>(fun);
			for (auto &th : threads)
			{
				th->join();
				th.reset();
			}
		}};
	}

	std::thread rip_vops_to_psql(const psql_serialization_params& params, const account_history_rocksdb_plugin &_dataSource, queue<post_process_command> &qqq)
	{
		return std::thread{[params, &qqq, &_dataSource]() -> void {
			std::cout << "started thread: " << params.start_block << " ; " << params.end_block << std::endl;
			_dataSource.enum_operations_from_block_range(
				params.start_block,
				params.end_block,
				fc::optional<uint64_t>{}, std::numeric_limits<uint32_t>::max(),
				[&](const rocksdb_operation_object &op, uint64_t operation_id) -> bool {
					qqq.wait_push(post_process_command{op, operation_id});
					return true;
				},
				true);
			std::cout << "vop thread finished in range: " << params.start_block << " ; " << params.end_block << std::endl;
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

				std::string str;
				it.to_dump->pull(str);
				ss << "INSERT INTO " << it.table_name << " VALUES " << str;
				while (!it.to_dump->empty())
				{
					it.to_dump->pull(str);
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
					flushes.at(ret.first).to_dump->push(ret.second);
					if (flushes.at(ret.first).to_dump->size() == buff_size)
						flusher(ret.first);
				}
				src_queue.wait_pull(ret);
			}
			std::cout << "flush break!" << std::endl;

			for (auto &kv : flushes)
				if (!kv.second.to_dump->empty())
				{
					std::lock_guard<std::mutex> lck{*(kv.second.mtx)};
					flusher(kv.first);
				}
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