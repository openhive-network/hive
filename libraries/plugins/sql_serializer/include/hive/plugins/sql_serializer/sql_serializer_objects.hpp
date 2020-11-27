#pragma once

// STL
#include <sstream>
#include <string>
#include <atomic>
#include <functional>

// Boost
#include <boost/thread/sync_queue.hpp>
#include <boost/type_index.hpp>

// Internal
#include <hive/chain/database.hpp>
#include <hive/chain/util/impacted.hpp>
#include <hive/chain/util/extractors.hpp>
#include <hive/chain/account_object.hpp>
// #include <hive/plugins/account_history_rocksdb/account_history_rocksdb_plugin.hpp>

namespace hive
{
	namespace plugins
	{
		namespace sql_serializer
		{
			// [which] = ( op_name, is_virtual )
			using operation_types_container_t = std::map<int64_t, std::pair<fc::string, bool>>;
			namespace PSQL
			{
				template <typename T>
				using queue = boost::concurrent::sync_queue<T>;
				using escape_function_t = std::function<fc::string(const char *)>;

				using hive::protocol::operation;

				namespace processing_objects
				{
					struct process_block_t
					{
						fc::string hash;
						int64_t block_number;

						process_block_t() = default;
						process_block_t(const fc::string &_hash, const int64_t _block_number) : hash{_hash}, block_number{_block_number} {}
					};

					struct process_transaction_t : public process_block_t
					{
						int64_t trx_in_block;

						process_transaction_t() = default;
						process_transaction_t(const fc::string &_hash, const int64_t _block_number, const int64_t _trx_in_block) : process_block_t{_hash, _block_number}, trx_in_block{_trx_in_block} {}
					};

					struct process_operation_t
					{
						int64_t block_number;
						int64_t trx_in_block;
						int64_t op_in_trx;
						fc::optional<operation> op;
						// if set to true, this is first time this operation appears in chain
						bool fresh;

						process_operation_t() = default;
						process_operation_t(const int64_t _block_number, const int64_t _trx_in_block, const int64_t _op_in_trx, const operation &_op, const bool _fresh) : block_number{_block_number}, trx_in_block{_trx_in_block}, op_in_trx{_op_in_trx}, op{_op}, fresh{_fresh} {}
					};
				}; // namespace processing_objects

				using processing_object_t = fc::static_variant<
						processing_objects::process_operation_t,
						processing_objects::process_transaction_t,
						processing_objects::process_block_t>;

				using strstrm = std::stringstream;
				inline std::string generate(std::function<void(strstrm &)> fun)
				{
					strstrm ss;
					fun(ss);
					return ss.str();
				}

				struct is_virtual_visitor
				{
					typedef bool result_type;

					template <typename op_t>
					bool operator()(const op_t &op) const
					{
						return op.is_virtual();
					}
				};

				struct name_gathering_visitor
				{
					using result_type = fc::string;

					template <typename op_t>
					result_type operator()(const op_t &) const
					{
						return boost::typeindex::type_id<op_t>().pretty_name();
					}
				};

				struct body_gathering_visitor
				{
					using result_type = fc::string;

					template <typename operation_t>
					result_type operator()(const operation_t & op) const { return get_body(op); }

					template <typename op_t>
					result_type get_body(const op_t &op) const { return fc::json::to_string(op); }
				};

				struct sql_dumper
				{
					typedef void result_type;

					const hive::chain::database &db;
					const operation_types_container_t &op_types;
					const escape_function_t escape;

					strstrm blocks{};
					strstrm transactions{};
					strstrm operations{};
					strstrm virtual_operations{};

					std::set<fc::string> permlink_cache;
					std::set<fc::string> account_cache;

					sql_dumper( const hive::chain::database &_db, const operation_types_container_t &_op_types, const escape_function_t _escape )
						:db{_db}, op_types{_op_types}, escape{_escape}
					{
						reset_blocks_stream();
						reset_transaction_stream();
						reset_operations_stream();
						reset_virtual_operation_stream();
					}

					void reset_operations_stream()
					{
						operations.clear();
						operations << "INSERT INTO hive_operations(block_num, trx_in_block, op_pos, op_type_id, participants, permlink_ids, body) VALUES ";
						any_operations = false;
					}

					void reset_virtual_operation_stream()
					{
						virtual_operations.clear();
						virtual_operations << "INSERT INTO hive_virtual_operations(block_num, trx_in_block, op_pos, op_type_id, participants, body) VALUES ";
						any_virtual_operations = false;
					}

					void reset_blocks_stream()
					{
						blocks.clear();
						blocks << "INSERT INTO hive_blocks VALUES ";
						any_blocks = false;
					}

					void reset_transaction_stream()
					{
						transactions.clear();
						transactions << "INSERT INTO hive_transactions VALUES ";
						any_transactions = false;
					}

					result_type process_operation(const processing_objects::process_operation_t &pop)
					{
						operations << (!any_operations ? "" : ",") << "( " << pop.block_number << " , "
											 << pop.trx_in_block << ", "
											 << pop.op_in_trx << " , "
											 << get_operation_type_id(*pop.op, false, pop.fresh) << " , "
											 << format_participants(*pop.op) << ", "
											 << get_formatted_permlinks(*pop.op) << " , "
											 << get_body(*pop.op)
											 << ")";
						any_operations = true;
					}

					result_type process_virtual_operation(const processing_objects::process_operation_t &pop)
					{
						virtual_operations << (!any_virtual_operations ? "" : ",") << "( " << pop.block_number << " , "
															 << pop.trx_in_block << ", "
															 << pop.op_in_trx << " , "
															 << get_operation_type_id(*pop.op, true, pop.fresh) << " , "
															 << format_participants(*pop.op) << " , "
															 << get_body(*pop.op)
															 << ")";
						any_virtual_operations = true;
					}

					result_type process_block(const processing_objects::process_block_t &bop)
					{
						blocks << (!any_blocks ? "" : ",") << "( " << bop.block_number << " , '" << bop.hash << "' )";
						any_blocks = true;
					}

					result_type process_transaction(const processing_objects::process_transaction_t &top)
					{
						transactions << (!any_transactions ? "" : ",") << " ( " << top.block_number << " , " << top.trx_in_block << " , '" << top.hash << "' )";
						any_transactions = true;
					}

					void get_dumped_cache(fc::string &out_permlinks, fc::string &out_accounts)
					{
						strstrm permlinks, accounts;

						if (permlink_cache.size())
						{
							permlinks << "INSERT INTO tmp_permlinks VALUES ";
							for (auto it = permlink_cache.begin(); it != permlink_cache.end(); it++)
								permlinks << (it == permlink_cache.begin() ? "" : ",") << "( E'" << escape(it->c_str()) << "')";
							out_permlinks = permlinks.str();
						}
						else
							out_permlinks = fc::string();

						if (account_cache.size())
						{
							accounts << "INSERT INTO tmp_accounts VALUES ";
							for (auto it = account_cache.begin(); it != account_cache.end(); it++)
								accounts << (it == account_cache.begin() ? "" : ",") << "( E'" << escape(it->c_str()) << "')";
							out_accounts = accounts.str();
						}
						else
							out_accounts = fc::string();
					}

				private:
					fc::string format_participants(const operation &op)
					{
						using hive::protocol::account_name_type;
						boost::container::flat_set<account_name_type> impacted;
						hive::app::operation_get_impacted_accounts(op, impacted);
						impacted.erase(account_name_type());
						return generate([&](strstrm &ss) {
							ss << "'{}'::INT[]";
							for (const auto &it : impacted)
							{
								account_cache.insert(fc::string(it));
								ss << " || get_inserted_account_id( '" << escape(fc::string(it).c_str()) << "' )";
							}
						});
					}

					fc::string get_formatted_permlinks(const operation &op)
					{
						std::vector<fc::string> result;
						hive::app::operation_get_permlinks(op, result);
						return generate([&](strstrm &ss) {
							ss << "'{}'::INT[]";
							for (const auto &it : result)
							{
								permlink_cache.insert(fc::string(it));
								ss << " || get_inserted_permlink_id( E'" << escape(fc::string(it).c_str()) << "' )";
							}
						});
					}

					fc::string get_body(const operation &op) const
					{
						const fc::string body = op.visit(body_gathering_visitor{});
						if (body == fc::string{})
							return "NULL";
						return generate([&](strstrm &ss) {
							ss << "E'" << escape(body.c_str()) << "'";
						});
					}

					fc::string get_operation_type_id(const operation &op, const bool is_virtual, const bool fresh) const
					{
						if (fresh)
							return generate([&](strstrm &ss) { ss << "insert_operation_type_id( " << op.which() << ", '" << op.visit(name_gathering_visitor{}) << "', " << (is_virtual ? "TRUE" : "FALSE") << ") "; });
						else
							return std::to_string(op.which());
					}

					bool any_blocks = false;
					bool any_transactions = false;
					bool any_operations = false;
					bool any_virtual_operations = false;
				};

			} // namespace PSQL
		}		// namespace sql_serializer
	}			// namespace plugins
} // namespace hive