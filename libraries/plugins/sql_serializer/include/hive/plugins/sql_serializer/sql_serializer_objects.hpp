#pragma once

// STL
#include <sstream>
#include <string>
#include <atomic>
#include <functional>

// Boost
#include <boost/mpl/for_each.hpp>
#include <boost/mpl/copy_if.hpp>
#include <boost/mpl/not.hpp>
#include <boost/type.hpp>
#include <boost/type_index.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/thread/sync_queue.hpp>
#include <boost/spirit/include/karma.hpp>

// Internal
#include <hive/chain/database.hpp>
#include <hive/chain/util/impacted.hpp>
#include <hive/chain/util/extractors.hpp>
#include <hive/chain/account_object.hpp>
#include "type_extractor_processor.hpp"
// #include <hive/plugins/account_history_rocksdb/account_history_rocksdb_plugin.hpp>

namespace hive
{
	namespace plugins
	{
		namespace sql_serializer
		{
			namespace PSQL
			{

				using operation_types_container_t = std::map<int64_t, std::pair<fc::string, bool>>;

				struct typename_gatherer
				{
					operation_types_container_t &names;

					template <typename T, typename SV>
					void operator()(boost::type<boost::tuple<T, SV>> t) const
					{
						names[names.size()] = std::pair<fc::string, bool>(boost::typeindex::type_id<T>().pretty_name(), T{}.is_virtual());
					}
				};

				template <typename T>
				using queue = boost::concurrent::sync_queue<T>;
				using escape_function_t = std::function<fc::string(const char *)>;
				using escape_raw_function_t = std::function<fc::string(const char *, const size_t)>;

				using hive::protocol::operation;

				namespace processing_objects
				{
					struct process_base_t
					{
						using hash_t = fc::ripemd160;

						hash_t hash;
						int block_number;

						process_base_t() = default;
						process_base_t(const hash_t &_hash, const int _block_number) : hash{_hash}, block_number{_block_number} {}
					};

					struct process_block_t : public process_base_t
					{
						fc::time_point_sec created_at;

						process_block_t() = default;
						process_block_t(const hash_t &_hash, const int _block_number, const fc::time_point_sec _tp) : process_base_t{_hash, _block_number}, created_at{_tp} {}
					};

					struct process_transaction_t : public process_base_t
					{
						using process_base_t::hash_t;

						int16_t trx_in_block;

						process_transaction_t() = default;
						process_transaction_t(const hash_t& _hash, const int _block_number, const int16_t _trx_in_block) : process_base_t{_hash, _block_number}, trx_in_block{_trx_in_block} {}
					};

					struct process_operation_t
					{
						int64_t operation_id;
            int32_t block_number;
						int16_t trx_in_block;
						int16_t op_in_trx;
						fc::optional<operation> op;
						fc::string deserialized_op;

						process_operation_t() = default;
						process_operation_t(int64_t _operation_id, int32_t _block_number, const int16_t _trx_in_block, const int16_t _op_in_trx,
              const operation &_op, const fc::string _deserialized_op) : operation_id{_operation_id }, block_number{_block_number}, trx_in_block{_trx_in_block},
              op_in_trx{_op_in_trx}, op{_op}, deserialized_op{_deserialized_op} {}
					};

          /// Holds account information to be put into database
          struct account_data_t
          {
            account_data_t(int _id, std::string _n) : id{_id}, name {_n} {}

            int32_t id;
            std::string name;
          };

          /// Holds permlink information to be put into database
          struct permlink_data_t
          {
            permlink_data_t(int _id, std::string _p) : id{_id}, permlink{_p} {}

            int32_t id;
            std::string permlink;
          };

          /// Holds association between account and its operations.
          struct account_operation_data_t
          {
            int64_t operation_id;
            int32_t account_id;
            int32_t operation_seq_no;

            account_operation_data_t(int64_t _operation_id, int32_t _account_id, int32_t _operation_seq_no) : operation_id{ _operation_id },
              account_id{ _account_id }, operation_seq_no{ _operation_seq_no } {}
          };

				}; // namespace processing_objects

				inline fc::string generate(std::function<void(fc::string &)> fun)
				{
					fc::string ss;
					fun(ss);
					return std::move(ss);
				}

				struct is_virtual_visitor
				{
					using result_type = bool;

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

				constexpr const char *SQL_bool(const bool val) { return (val ? "TRUE" : "FALSE"); }

				inline fc::string get_all_type_definitions()
				{
					namespace te = type_extractor;
					typedef te::sv_processor<hive::protocol::operation> processor;

					operation_types_container_t result;
					typename_gatherer p{result};
					boost::mpl::for_each<processor::transformed_type_list, boost::type<boost::mpl::_>>(p);

					if (result.empty())
						return fc::string{};
					else
					{
						return generate([&](fc::string &ss) {
							ss.append("INSERT INTO hive_operation_types VALUES ");
							for (auto it = result.begin(); it != result.end(); it++)
							{
								if (it != result.begin())
									ss.append(",");
								ss.append("( ");
								ss.append(std::to_string(it->first));
								ss.append(" , '");
								ss.append(it->second.first);
								ss.append("', ");
								ss.append(SQL_bool(it->second.second));
								ss.append(" )");
							}
							ss.append(" ON CONFLICT DO NOTHING");
						});
					}
				}
				using cache_contatiner_t = std::set<fc::string>;

				struct sql_dumper
				{
					// returns current width of stream
					typedef size_t result_type;

					const escape_function_t escape;
					const escape_raw_function_t escape_raw;

					fc::string blocks{};
					fc::string transactions{};
					fc::string operations{};
					fc::string virtual_operations{};

					cache_contatiner_t permlink_cache;
					cache_contatiner_t account_cache;

					sql_dumper(const escape_function_t _escape, const escape_raw_function_t _escape_raw, const fc::string &_null_permlink, const fc::string &_null_account)
							: escape{_escape}, escape_raw{ _escape_raw } {}

					void log_query( const fc::string& sql )
					{
						static const char* log = getenv("LOG_FINAL_QUERY");
						if(log != nullptr) std::cout << "[ SQL QUERY LOG ]" << sql << std::endl;
					}
					
					fc::string get_operations_sql()
					{
						if(operations.size() == 0) return fc::string{};
						
						// do not remove single space at the beginiing in all 4 following sqls !!!
						operations.insert(0, "INSERT INTO hive_operations (block_num, trx_in_block, op_pos, op_type_id, body, participants, permlink_ids) SELECT w.bn, w.trx, w.opn, w.opt, w.body, w.part, w.perm FROM ( VALUES");
						operations.append(") AS w (order_id, bn, trx, opn, opt, body, part, perm)");

						log_query( operations );
						return std::move(operations);
					}

					fc::string get_virtual_operations_sql()
					{
						if(virtual_operations.size() == 0) return fc::string{};

						virtual_operations.insert(0, "INSERT INTO hive_virtual_operations (block_num, trx_in_block, op_pos, op_type_id, body, participants) SELECT w.bn, w.trx, w.opn, w.opt, w.body, w.part as part_name FROM ( VALUES ");
						virtual_operations.append(" ) AS w (order_id, bn, trx, opn, opt, body, part)");

						log_query( virtual_operations );
						return std::move(virtual_operations);
					}

					fc::string get_blocks_sql()
					{
						if(blocks.size() == 0) return fc::string{};
						blocks.insert(0, "INSERT INTO hive_blocks VALUES ");
						log_query( blocks );
						return std::move(blocks);
					}

					fc::string get_transaction_sql()
					{
						if(transactions.size() == 0) return fc::string{};
						transactions.insert(0, "INSERT INTO hive_transactions VALUES ");
						log_query( transactions );
						return std::move(transactions);
					}

					result_type process_operation(const processing_objects::process_operation_t &pop)
					{
						operations.append(any_operations == 0 ? "" : ",");

						fc::string result;
						get_operation_value_prefix(result, pop, any_operations);

						result.append(",");
						result.append(get_formatted_permlinks(*pop.op));
						result.append("/* formatted permlinks: */ )");

						operations.append(result);

						return operations.size();
					}

					result_type process_block(const processing_objects::process_block_t &bop)
					{
						if (any_blocks++) blocks.append(",");

						blocks.append("\n( ");
						blocks.append(std::to_string(bop.block_number) + " , '");
						blocks.append( escape_raw(bop.hash.data(), bop.hash.data_size() ) + "', '" );
						blocks.append(fc::string(bop.created_at) + "' )");

						return blocks.size();
					}

					result_type process_transaction(const processing_objects::process_transaction_t &top)
					{
						if (any_transactions++) transactions.append(",");

						transactions.append("\n( ");
						transactions.append(std::to_string(top.block_number) + " , ");
						transactions.append(std::to_string(top.trx_in_block) + " , '");
						transactions.append(escape_raw(top.hash.data(), top.hash.data_size() ) + "' )");

						return transactions.size();
					}

					void get_dumped_cache(fc::string &out_permlinks, fc::string &out_accounts)
					{
						out_permlinks = fc::string();
						out_accounts = fc::string();

						if (permlink_cache.size())
						{
							out_permlinks.append("INSERT INTO hive_permlink_data(permlink) VALUES ");

							for (auto it = permlink_cache.begin(); it != permlink_cache.end(); it++)
							{
								if (it != permlink_cache.begin())
									out_permlinks.append(",");
								out_permlinks.append("( E'");
								out_permlinks.append(escape(it->c_str()));
								out_permlinks.append("')");
							}

							out_permlinks.append(" ON CONFLICT DO NOTHING");
							log_query(out_permlinks);
						}

						if (account_cache.size())
						{
							out_accounts.append("INSERT INTO hive_accounts(name) VALUES ");

							for (auto it = account_cache.begin(); it != account_cache.end(); it++)
							{
								if (it != account_cache.begin())
									out_accounts.append(",");
								out_accounts.append("( E'");
								out_accounts.append(escape(it->c_str()));
								out_accounts.append("')");
							}

							out_accounts.append(" ON CONFLICT DO NOTHING");
							log_query(out_accounts);
						}
					}

				private:

					template<typename iterable_t, typename function_t>
					fc::string format_text_array( const iterable_t& coll,const fc::string& source_fn, function_t for_each ) const
					{
						if( coll.empty() )
              return "NULL::int[]";
            fc::string query(source_fn);
            query += "(E'";
            auto nameI = coll.begin();
            query += *nameI;
            query += '\'';
            for_each(*nameI);

            for(++nameI; nameI != coll.end(); nameI++)
            {
              query += ",E'";
              query += *nameI;
              query += '\'';
              for_each(*nameI);
            }

            query += ')';

						return std::move(query);
					}

					fc::string get_formated_participants(const operation &op)
					{
						using hive::protocol::account_name_type;
						boost::container::flat_set<account_name_type> impacted;
						hive::app::operation_get_impacted_accounts(op, impacted);
						impacted.erase(account_name_type());

            FC_ASSERT(impacted.size() <= 3, "get_account_ids SQL function is specialized only for at most 3 account names");

						return std::move( format_text_array( impacted, "get_account_ids", [&](const account_name_type& acc) { account_cache.insert( fc::string(acc) ); } ) );
					}

					fc::string get_formatted_permlinks(const operation &op)
					{
						std::vector<fc::string> result;
						hive::app::operation_get_permlinks(op, result);
            FC_ASSERT(result.size() <= 2, "get_permlink_ids SQL function is specialized only for at most 2 permlink identifiers");
						return std::move( format_text_array( result, "get_permlink_ids", [&](const fc::string& perm) { permlink_cache.insert( perm ); } ));
					}

					fc::string get_body(const char *op) const
					{
						if (std::strlen(op) == 0)
							return "NULL";
						return generate([&](fc::string &ss) {
							ss.append("E'");
							ss.append(escape(op));
							ss.append("'");
						});
					}

					fc::string get_operation_type_id(const operation &op) const
					{
						return std::move(std::to_string(op.which()));
					}

					void get_operation_value_prefix(fc::string &ss, const processing_objects::process_operation_t &pop, uint32_t &order_id)
					{
						const char *separator = ", ";
						ss.append("\n( ");
						ss.append(std::to_string(order_id++) + " /* order_id */");
						ss.append(separator);
						ss.append(std::to_string(pop.block_number) + " /* block number */");
						ss.append(separator);
						ss.append(std::to_string(pop.trx_in_block) + " /* trx in blck */");
						ss.append(separator);
						ss.append(std::to_string(pop.op_in_trx) + " /* op in trx */");
						ss.append(separator);
						ss.append(get_operation_type_id(*pop.op) + " /* op type id */");
						ss.append(separator);
						ss.append(get_body(pop.deserialized_op.c_str()) + " /* deserialized op */");
						ss.append(separator);
						ss.append( get_formated_participants(*pop.op) + " /* participants */" );
					}

					uint32_t any_blocks = 0;
					uint32_t any_transactions = 0;
					uint32_t any_operations = 0;
					uint32_t any_virtual_operations = 0;
				}; // namespace PSQL

			} // namespace PSQL
		}		// namespace sql_serializer
	}			// namespace plugins
} // namespace hive