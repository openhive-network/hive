#pragma once

// STL
#include <sstream>
#include <string>
#include <atomic>
#include <functional>
#include <locale>
#include <codecvt>

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

namespace hive
{
	namespace plugins
	{
		namespace sql_serializer
		{
			namespace escapings
			{
				inline fc::string escape_sql(const uint64_t &num) { return std::to_string(num); }

				inline fc::string escape_sql(const std::string &text)
				{
					const static std::map<wchar_t, fc::string> special_chars{{std::pair<wchar_t, fc::string>{L'\x00', " "}, {L'\r', "\\015"}, {L'\n', "\\012"}, {L'\v', "\\013"}, {L'\f', "\\014"}, {L'\\', "\\134"}, {L'\'', "\\047"}, {L'%', "\\045"}, {L'_', "\\137"}, {L':', "\\072"}}};

					if(text.empty()) return "E''";

					std::wstring_convert<std::codecvt_utf8<wchar_t>> conv{};
					std::wstring utf32 = conv.from_bytes(text);
					std::stringstream ss{};
					ss << "E'";

					for (auto it = utf32.begin(); it != utf32.end(); it++)
					{
						const wchar_t c{*it};
						const auto result = special_chars.find(c);
						if (result != special_chars.end()) ss << result->second;
						else
						{
							const char _c = conv.to_bytes(c)[0];
							if( _c > 0 && std::isprint(_c) ) ss << _c;
							else
							{
								const bool is_utf_32{ int(c) > 0xffff };
								ss << ( is_utf_32 ? "\\U" : "\\u" ) << std::setfill('0') << std::setw(4 + (4 * is_utf_32)) << std::hex << int(c);
							}
						}
					}
					ss << '\'';
					return ss.str();
				}

				struct escape_visitor
				{
					using result_type = fc::string;

					template<typename operation_t>
					result_type operator()(const operation_t& op) const
					{
						return to_string(op);
					}

				private:

					template<typename operation_t>
					result_type to_string(const operation_t& op) const 
					{
						return escape_sql(
							fc::json::to_string(
								fc::mutable_variant_object( op )
							)
						);
					}

				};
			}

			namespace PSQL
			{

				using hive::protocol::account_name_type;
				using account_names_container_t = boost::container::flat_set<account_name_type>;
				// account_name_type: | account_id | counter |
				using accountname_id_counter_map_t = std::map<hive::protocol::account_name_type, uint64_t>;
				using operation_types_container_t = std::map<int64_t, std::pair<fc::string, bool>>;
				using hive::protocol::operation;

				struct typename_gatherer
				{
					operation_types_container_t &names;

					template <typename T, typename SV>
					void operator()(boost::type<boost::tuple<T, SV>> t) const
					{
						names[names.size()] = std::pair<fc::string, bool>(boost::typeindex::type_id<T>().pretty_name(), T{}.is_virtual());
					}
				};

				namespace processing_objects
				{
					struct process_base_t
					{
						using hash_t = fc::ripemd160;

						hash_t hash;
						int64_t block_number;

						process_base_t() = default;
						process_base_t(const hash_t &_hash, const int64_t _block_number) : hash{_hash}, block_number{_block_number} {}
					};

					struct process_block_t : public process_base_t
					{
						fc::time_point_sec created_at;

						process_block_t() = default;
						process_block_t(const hash_t &_hash, const int64_t _block_number, const fc::time_point_sec _tp) : process_base_t{_hash, _block_number}, created_at{_tp} {}
					};

					struct process_transaction_t : public process_base_t
					{
						using process_base_t::hash_t;

						int64_t trx_in_block;

						process_transaction_t() = default;
						process_transaction_t(const hash_t &_hash, const int64_t _block_number, const int64_t _trx_in_block) : process_base_t{_hash, _block_number}, trx_in_block{_trx_in_block} {}
					};

					struct process_operation_t
					{
						uint64_t order_id;
						int64_t block_number;
						int64_t trx_in_block;
						int64_t op_in_trx;
						fc::optional<operation> op;
						fc::string deserialized_op;
						std::vector< uint64_t > participants;


						process_operation_t() = default;
						process_operation_t(const uint64_t order_id, const int64_t _block_number, const int64_t _trx_in_block, const int64_t _op_in_trx, const operation &_op, const fc::string _deserialized_op, std::vector<uint64_t> _participants) : order_id{order_id}, block_number{_block_number}, trx_in_block{_trx_in_block}, op_in_trx{_op_in_trx}, op{_op}, deserialized_op{_deserialized_op}, participants{_participants} {}
					};

					using process_virtual_operation_t = process_operation_t;

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
					using result_type = size_t;

					fc::string blocks{};
					fc::string transactions{};
					fc::string operations{};
					fc::string virtual_operations{};

					cache_contatiner_t permlink_cache;

					void log_query( const fc::string& sql )
					{
						static const char* log = getenv("LOG_FINAL_QUERY");
						if(log != nullptr) std::cout << "[ SQL QUERY LOG ]" << sql << std::endl;
					}
					
					fc::string get_operations_sql()
					{
						if(operations.size() == 0) return fc::string{};
						
						operations.insert(0, R"(
 INSERT INTO hive_operations (block_num, trx_in_block, op_pos, op_type_id, body, participants, permlink_ids)
 SELECT T.bn, T.trx, T.opn, T.opt, T.body, T.part, array_agg(hpd.id) FROM ( 
 SELECT w.order_id, w.bn, w.trx, w.opn, w.opt, w.body, w.part, unnest(w.perm) as _permlink FROM ( VALUES 
)");
						
						operations.append(R"(
 ) AS w (order_id, bn, trx, opn, opt, body, part, perm)) as T
 INNER JOIN hive_permlink_data as hpd ON hpd.permlink=T._permlink
 GROUP BY T.order_id, T.bn, T.trx, T.opn, T.opt, T.body, T.part ORDER BY T.order_id DESC;
)");

						log_query( operations );
						return std::move(operations);
					}

					fc::string get_virtual_operations_sql()
					{
						if(virtual_operations.size() == 0) return fc::string{};
						virtual_operations.insert(0, R"(
 INSERT INTO hive_virtual_operations (block_num, trx_in_block, op_pos, op_type_id, body, participants)
 SELECT T.bn, T.trx, T.opn, T.opt, T.body, T.part FROM ( VALUES  
)");

						virtual_operations.append(" ) AS T (order_id, bn, trx, opn, opt, body, part)");

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
						any_operations++;

						fc::string result;
						get_operation_value_prefix(result, pop);

						result.append(",");
						result.append(get_formatted_permlinks(*pop.op));
						result.append("/* formatted permlinks: */ )");

						operations.append(result);

						return operations.size();
					}

					result_type process_virtual_operation(const processing_objects::process_virtual_operation_t &pop)
					{
						virtual_operations.append(any_virtual_operations == 0 ? "" : ",");
						any_virtual_operations++;

						fc::string result;
						get_operation_value_prefix(result, pop);
						result.append(")");

						virtual_operations.append(result);

						return virtual_operations.size();
					}

					result_type process_block(const processing_objects::process_block_t &bop)
					{
						if (any_blocks++) blocks.append(",");

						blocks.append("\n( ");
						blocks.append(std::to_string(bop.block_number) + " , '\\x");
						blocks.append(bop.hash.str() + "', '" );
						blocks.append(fc::string(bop.created_at) + "' )");

						return blocks.size();
					}

					result_type process_transaction(const processing_objects::process_transaction_t &top)
					{
						if (any_transactions++) transactions.append(",");

						transactions.append("\n( ");
						transactions.append(std::to_string(top.block_number) + " , ");
						transactions.append(std::to_string(top.trx_in_block) + " , '\\x");
						transactions.append(top.hash.str() + "' )");
						

						return transactions.size();
					}

					void get_dumped_cache(fc::string &out_permlinks)
					{
						out_permlinks = fc::string();

						if (permlink_cache.size())
						{
							out_permlinks.append("INSERT INTO hive_permlink_data(permlink) VALUES ");

							for (auto it = permlink_cache.begin(); it != permlink_cache.end(); it++)
							{
								if (it != permlink_cache.begin())
									out_permlinks.append(",");
								out_permlinks.append("( ");
								out_permlinks.append(std::move(*it));
								out_permlinks.append(" )");
							}

							out_permlinks.append(" ON CONFLICT DO NOTHING");
							log_query(out_permlinks);
						}
					}

				private:
					template<typename iterable_t, typename function_t>
					fc::string format_array( const iterable_t& coll, function_t for_each, const char * empty = "ARRAY[ '' ]" )
					{
						if( coll.size() == 0 ) return empty;
						fc::string ret{ "ARRAY[ " };
						for(auto cit = coll.begin(); cit != coll.end(); cit++)
						{
							if(cit != coll.begin()) ret.append(",");
							fc::string _tmp{ escapings::escape_sql(*cit) };
							for_each(_tmp);
							ret.append(std::move(_tmp));
						}
						ret.append("]");
						return std::move(ret);
					}

					fc::string get_formated_participants(const processing_objects::process_operation_t & pop)
					{
						
						return std::move( format_array( pop.participants, [](const auto&){}, "NULL::BIGINT[]" ) );
					}

					fc::string get_formatted_permlinks(const operation &op)
					{
						std::vector<fc::string> result;
						hive::app::operation_get_permlinks(op, result);
						return std::move( format_array( result, [&](const fc::string& perm) { permlink_cache.insert( perm ); } ));
					}

					fc::string get_body(const char *op) const
					{
						if (std::strlen(op) == 0)
							return "NULL";
						return fc::string(op);
					}

					fc::string get_operation_type_id(const operation &op) const
					{
						return std::move(std::to_string(op.which()));
					}

					void get_operation_value_prefix(fc::string &ss, const processing_objects::process_operation_t &pop)
					{
						const char *separator = ", ";
						ss.append("\n( ");
						ss.append(std::to_string(pop.order_id) + " /* order_id */");
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
						ss.append( get_formated_participants(pop) + " /* participants */" );
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