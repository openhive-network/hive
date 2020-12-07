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
			// [which] = ( op_name, is_virtual )
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

				using hive::protocol::operation;

				namespace processing_objects
				{
					struct process_block_t
					{
						using hash_t = fc::ripemd160;

						hash_t hash;
						int64_t block_number;

						process_block_t() = default;
						process_block_t(const hash_t& _hash, const int64_t _block_number) : hash{_hash}, block_number{_block_number} {}
					};

					struct process_transaction_t : public process_block_t
					{
						using process_block_t::hash_t;

						int64_t trx_in_block;

						process_transaction_t() = default;
						process_transaction_t(const hash_t &_hash, const int64_t _block_number, const int64_t _trx_in_block) : process_block_t{_hash, _block_number}, trx_in_block{_trx_in_block} {}
					};

					struct process_operation_t
					{
						int64_t block_number;
						int64_t trx_in_block;
						int64_t op_in_trx;
						fc::optional<operation> op;
						fc::string deserialized_op;

						process_operation_t() = default;
						process_operation_t(const int64_t _block_number, const int64_t _trx_in_block, const int64_t _op_in_trx, const operation &_op, const fc::string _deserialized_op) : block_number{_block_number}, trx_in_block{_trx_in_block}, op_in_trx{_op_in_trx}, op{_op}, deserialized_op{_deserialized_op} {}
					};
				}; // namespace processing_objects

				using processing_object_t = fc::static_variant<
						processing_objects::process_operation_t,
						processing_objects::process_transaction_t,
						processing_objects::process_block_t>;

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

				constexpr const char* SQL_bool(const bool val) { return (val ? "TRUE" : "FALSE"); }

				inline fc::string get_all_type_definitions()
				{
					namespace te = type_extractor;
					typedef te::sv_processor<hive::protocol::operation> processor;

					operation_types_container_t result;
					typename_gatherer p{result};
					boost::mpl::for_each< processor::transformed_type_list, boost::type<boost::mpl::_> >(p);

					if(result.empty()) return fc::string{};
					else{ 
						return generate([&](fc::string& ss){
						ss.append("INSERT INTO hive_operation_types VALUES ");
						for(auto it = result.begin(); it != result.end(); it++)
						{
							if(it != result.begin()) ss.append(",");
							ss.append("( ");
							ss.append( std::to_string(it->first) );
							ss.append( " , '" );
							ss.append( it->second.first );
							ss.append( "', " );
							ss.append( SQL_bool(it->second.second) );
							ss.append( " )" );
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

					const hive::chain::database &db;
					const escape_function_t escape;

					fc::string blocks{};
					fc::string transactions{};

					cache_contatiner_t permlink_cache;
					cache_contatiner_t account_cache;

					sql_dumper(const hive::chain::database &_db, const escape_function_t _escape)
							: db{_db}, escape{_escape}
					{
						reset_blocks_stream();
						reset_transaction_stream();
						reset_operations_stream();
						reset_virtual_operation_stream();
					}

					fc::string reset_operations_stream()
					{
						// do not remove single space at the beginiing in all 4 following sqls !!!
						operations.append(R"(
) as T( order_id, bn, trx, opn, opt, body, perm, part ) INNER JOIN hive_accounts AS ha ON T.part=ha.name
 INNER JOIN hive_permlink_data AS hpd ON T.perm=hpd.permlink GROUP BY T.order_id, T.bn, T.trx, T.opn, T.opt, T.body ORDER BY T.order_id )");

						fc::string result{ std::move(operations) };

						operations = fc::string{R"(
INSERT INTO hive_operations( block_num, trx_in_block, op_pos, op_type_id, body, participants, permlink_ids)
 SELECT T.bn, T.trx, T.opn, T.opt, T.body, array_remove(array_agg(ha.id), 0), array_remove(array_agg(hpd.id), 0) FROM ( VALUES )"};

						any_operations = 0;

						return std::move(result);
					}

					fc::string reset_virtual_operation_stream()
					{
						virtual_operations.append(R"(
) as T( order_id, bn, trx, opn, opt, body, part ) INNER JOIN hive_accounts AS ha ON T.part=ha.name
 GROUP BY T.order_id, T.bn, T.trx, T.opn, T.opt, T.body ORDER BY T.order_id )");

						fc::string result{ std::move(virtual_operations) };

						virtual_operations = fc::string{R"(
INSERT INTO hive_virtual_operations(block_num, trx_in_block, op_pos, op_type_id, body ,participants)
 SELECT T.bn, T.trx, T.opn, T.opt, T.body, array_remove(array_agg(ha.id), 0) FROM ( VALUES )"};

						any_virtual_operations = 0;

						return std::move(result);
					}

					void reset_blocks_stream()
					{
						blocks = fc::string{ "INSERT INTO hive_blocks VALUES " };
						any_blocks = false;
					}

					void reset_transaction_stream()
					{
						transactions = fc::string{ "INSERT INTO hive_transactions VALUES " };
						any_transactions = false;
					}

					result_type process_operation(const processing_objects::process_operation_t &pop)
					{
						operations.append(any_operations == 0 ? "" : ",");
						any_operations++;

						fc::string pre_generate;
						get_operation_value_prefix(pre_generate, pop);

						operations.append( pre_generate );
						operations.append( " get_null_permlink(), '' )" );

						get_formatted_permlinks(pre_generate, *pop.op, operations);
						pre_generate.append("get_null_permlink(), ");
						format_participants(pre_generate, *pop.op, operations);

						return operations.size();
					}

					result_type process_virtual_operation(const processing_objects::process_operation_t &pop)
					{
						virtual_operations.append(any_virtual_operations == 0 ? "" : ",");
						any_virtual_operations++;

						fc::string pre_generate;
						get_operation_value_prefix(pre_generate, pop);

						virtual_operations.append(pre_generate);
						virtual_operations.append(" '' )");

						format_participants(pre_generate, *pop.op, virtual_operations);

						return virtual_operations.size();
					}

					result_type process_block(const processing_objects::process_block_t &bop)
					{
						if(any_blocks) blocks.append(",");
						any_blocks = true;

						blocks.append("( "); 
						blocks.append(std::to_string(bop.block_number)); 
						blocks.append(" , '");
						blocks.append(bop.hash.str());
						blocks.append("' )");


						return blocks.size();
					}

					result_type process_transaction(const processing_objects::process_transaction_t &top)
					{
						if(any_transactions) transactions.append(",");
						any_transactions = true;

						transactions.append("( "); 
						transactions.append(std::to_string(top.block_number));
						transactions.append(" , ");
						transactions.append(std::to_string(top.trx_in_block));
						transactions.append(" , '");
						transactions.append(top.hash.str());
						transactions.append("' )");


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
								if(it != permlink_cache.begin()) out_permlinks.append(",");
								out_permlinks.append("( E'");
								out_permlinks.append(escape(it->c_str())); 
								out_permlinks.append("')");
							}

							out_permlinks.append(" ON CONFLICT DO NOTHING");
						}

						if (account_cache.size())
						{
							out_accounts.append("INSERT INTO hive_accounts(name) VALUES ");

							for (auto it = account_cache.begin(); it != account_cache.end(); it++)
							{
								if(it != account_cache.begin()) out_accounts.append(",");
								out_accounts.append("( E'");
								out_accounts.append(escape(it->c_str()));
								out_accounts.append("')");
							}

							out_accounts.append(" ON CONFLICT DO NOTHING");
						}
					}

				private:
					void format_participants(const fc::string &prefix, const operation &op, fc::string &output)
					{
						using hive::protocol::account_name_type;
						boost::container::flat_set<account_name_type> impacted;
						hive::app::operation_get_impacted_accounts(op, impacted);
						impacted.erase(account_name_type());
						if (!impacted.empty())
							for (auto it = impacted.begin(); it != impacted.end(); it++)
							{
								account_cache.emplace(fc::string(*it));
								output.append(",");
								output.append(prefix);
								output.append("E'");
								output.append(escape(fc::string(*it).c_str()));
								output.append("' )");
							}
					}

					void get_formatted_permlinks(const fc::string &prefix, const operation &op, fc::string &output)
					{
						std::vector<fc::string> result;
						hive::app::operation_get_permlinks(op, result);
						if (!result.empty())
							for (auto it = result.begin(); it != result.end(); it++)
							{
								permlink_cache.emplace(*it);
								output.append(",");
								output.append(prefix);
								output.append("E'");
								output.append(escape(it->c_str()));
								output.append("', '' )");
							}
					}

					fc::string get_body(const char* op) const
					{
						if ( std::strlen(op) == 0 ) return "NULL";
						return generate([&](fc::string &ss) {
							ss.append("E'");
							ss.append(escape( op ));
							ss.append("'");
						});
					}

					fc::string get_operation_type_id(const operation &op) const
					{
						return std::move(std::to_string(op.which()));
					}

					void get_operation_value_prefix(fc::string& ss, const processing_objects::process_operation_t& pop)
					{
						const char* separator = ", ";
						ss.append( "( " );
						ss.append( std::to_string(any_operations) ); 
						ss.append( separator );
						ss.append( std::to_string( pop.block_number ) ); 
						ss.append( separator );
						ss.append( std::to_string( pop.trx_in_block ) );
						ss.append( separator );
						ss.append( std::to_string( pop.op_in_trx ) );
						ss.append( separator );
						ss.append( get_operation_type_id(*pop.op) );
						ss.append( separator );
						ss.append( get_body(pop.deserialized_op.c_str()) );
						ss.append( separator );
					}

					bool any_blocks = false;
					bool any_transactions = false;
					uint32_t any_operations = 0;
					uint32_t any_virtual_operations = 0;

					fc::string operations{};
					fc::string virtual_operations{};
				};

			} // namespace PSQL
		}		// namespace sql_serializer
	}			// namespace plugins
} // namespace hive