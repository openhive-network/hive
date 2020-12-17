#include <hive/plugins/sql_serializer/sql_serializer_plugin.hpp>

#include <hive/chain/util/impacted.hpp>
#include <hive/protocol/config.hpp>
#include <hive/utilities/plugin_utilities.hpp>

#include <fc/io/json.hpp>
#include <fc/smart_ref_impl.hpp>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <thread>

// C++ connector library for PostgreSQL (http://pqxx.org/development/libpqxx/)
#include <pqxx/pqxx>

namespace hive
{
	namespace plugins
	{
		namespace sql_serializer
		{
			using namespace hive::protocol;
			using work = pqxx::work;
			using namespace hive::plugins::sql_serializer::PSQL;

			using chain::database;
			using chain::operation_object;

			constexpr size_t default_reservation_size{ 16'000u };
			constexpr size_t max_tuples_count{ 1'000 };
			constexpr size_t max_data_length{ 16*1024*1024 }; 

			inline void mylog(const char* msg)
			{
				// std::cout << "[ " << std::this_thread::get_id() << " ] " << msg << std::endl;
			}

			namespace detail
			{
				struct transaction_repr_t
				{
					std::unique_ptr<pqxx::connection> _connection;
					std::unique_ptr<work> _transaction;

					transaction_repr_t() = default;
					transaction_repr_t(pqxx::connection* _conn, work* _trx) : _connection{_conn}, _transaction{_trx} {}

					auto get_escaping_charachter_methode()
					{
						return [this](const char *val) -> fc::string { return std::move( this->_connection->esc(val) ); };
					}
				};
				using transaction_t = std::unique_ptr<transaction_repr_t>;

				struct postgress_connection_holder
				{
					explicit postgress_connection_holder(const fc::string &url)
							: connection_string{url} {}

					transaction_t start_transaction()
					{
						mylog("started transaction");

						pqxx::connection* _conn = new pqxx::connection{ this->connection_string };
						work *_trx = new work{*_conn};
						_trx->exec("SET CONSTRAINTS ALL DEFERRED;");

						return transaction_t{ new transaction_repr_t{ _conn, _trx } };
					}

					bool exec_transaction(transaction_t &trx, const fc::string &sql)
					{
						if (sql == fc::string())
							return true;
						return sql_safe_execution([&trx, &sql]() { trx->_transaction->exec(sql); }, sql.c_str());
					}

					bool commit_transaction(transaction_t &trx)
					{
						mylog("commiting");
						return sql_safe_execution([&]() { trx->_transaction->commit(); }, "commit");
					}

					void abort_transaction(transaction_t &trx)
					{
						mylog("aborting");
						trx->_transaction->abort();
					}

					bool exec_no_transaction(const fc::string &sql, pqxx::result *result = nullptr)
					{
						if (sql == fc::string())
							return true;

						return sql_safe_execution([&]() {
							pqxx::connection conn{ this->connection_string };
							pqxx::work trx{conn};
							if (result)
								*result = trx.exec(sql);
							else
								trx.exec(sql);
							trx.commit();
						}, sql.c_str());
					}

					template<typename T>
					bool get_single_value(const fc::string& query, T& _return)
					{
						pqxx::result res;
						if(!exec_no_transaction( query, &res ) && res.empty() ) return false;
						_return = res.at(0).at(0).as<T>();
						return true;
					}

					template<typename T>
					T get_single_value(const fc::string& query)
					{
						T _return;
						FC_ASSERT( get_single_value( query, _return ) );
						return _return;
					}

				private:
					fc::string connection_string;

					// void destroy_transaction(transaction_t &trx) const { trx.~unique_ptr(); }

					bool sql_safe_execution(const std::function<void()> &f, const char* msg = nullptr)
					{
						try
						{
							f();
							return true;
						}
						catch (const pqxx::pqxx_exception &sql_e)
						{
							elog("Exception from postgress: ${e}", ("e", sql_e.base().what()));
						}
						catch (const std::exception &e)
						{
							elog("Exception: ${e}", ("e", e.what()));
						}
						catch (...)
						{
							elog("Unknown exception occured");
						}
						if(msg) std::cerr << "Error message: " << msg << std::endl;
						return false;
					}
				};

				struct cached_data_t
				{
					std::vector<PSQL::processing_objects::process_block_t> blocks;
					std::vector<PSQL::processing_objects::process_transaction_t> transactions;
					std::vector<PSQL::processing_objects::process_operation_t> operations;
					std::vector<PSQL::processing_objects::process_operation_t> virtual_operations;

					size_t total_size{ 0ul };

					cached_data_t(const size_t reservation_size)
					{
						blocks.reserve(reservation_size);
						transactions.reserve(reservation_size);
						operations.reserve(reservation_size);
						virtual_operations.reserve(reservation_size);
					}

					~cached_data_t()
					{
						log_size("destructor");
					}

					void log_size(const fc::string& msg = "size")
					{
						mylog(generate([&](fc::string& ss){ 
							ss = msg + ": ";
							ss += "blocks: " + std::to_string(blocks.size());
							ss += " trx: " + std::to_string(transactions.size());
							ss += " operations: " + std::to_string(operations.size());
							ss += " vops: " + std::to_string(virtual_operations.size());
							ss += " total size: " + std::to_string(total_size);
							ss += "\n"; 
						}).c_str());
					}
				};
				using cached_containter_t = std::unique_ptr<cached_data_t>;

				class sql_serializer_plugin_impl
				{
				public:
					sql_serializer_plugin_impl(const std::string &url) : _db(appbase::app().get_plugin<hive::plugins::chain::chain_plugin>().db()), connection{url}
					{}

					virtual ~sql_serializer_plugin_impl()
					{
						mylog("finishing from plugin impl destructor");
						cleanup_sequence();
					}

					database &_db;
					boost::signals2::connection _on_post_apply_operation_con;
					boost::signals2::connection _on_post_apply_block_con;
					boost::signals2::connection _on_starting_reindex;
					boost::signals2::connection _on_finished_reindex;

					postgress_connection_holder connection;
					fc::optional<fc::string> path_to_schema;
					bool set_index = false;
					bool are_constraints_active = true;

					using thread_with_status = std::pair<std::shared_ptr<std::thread>, bool>;
					std::list<thread_with_status> threads;
					uint32_t blocks_per_commit = 1;

					cached_containter_t currently_caching_data;

					void create_indexes()
					{
						static const std::vector<fc::string> indexes{{
							R"(CREATE INDEX IF NOT EXISTS hive_operations_operation_types_index ON "hive_operations" ("op_type_id");)",
							R"(CREATE INDEX IF NOT EXISTS hive_operations_participants_index ON "hive_operations" USING GIN ("participants");)",
							R"(CREATE INDEX IF NOT EXISTS hive_operations_permlink_ids_index ON "hive_operations" USING GIN ("permlink_ids");)",
							R"(CREATE INDEX IF NOT EXISTS hive_virtual_operations_operation_types_index ON "hive_virtual_operations" ("op_type_id");)",
							R"(CREATE INDEX IF NOT EXISTS hive_virtual_operations_participants_index ON "hive_virtual_operations" USING GIN ("participants");)"
						}};

						for (const auto &idx : indexes)
							connection.exec_no_transaction(idx);
					}

					void recreate_db()
					{
						using bpath = boost::filesystem::path;

						bpath path = bpath((*path_to_schema).c_str());
						const size_t size = boost::filesystem::file_size(path);

						fc::string schema_str, line;
						schema_str.reserve(size);
						std::ifstream file{path.string()};
						while(std::getline(file, line)) schema_str += line;
						file.close();

						auto trx = connection.start_transaction();
						FC_ASSERT(connection.exec_transaction(trx, schema_str));
						FC_ASSERT(connection.exec_transaction(trx, PSQL::get_all_type_definitions()));
						FC_ASSERT(connection.commit_transaction(trx));
					}

					void upload_caches(transaction_t &trx, PSQL::sql_dumper &dumper)
					{
						fc::string perms, accs;
						dumper.get_dumped_cache(perms, accs);

						if (perms != fc::string()) connection.exec_transaction(trx, perms);
						if (accs != fc::string()) connection.exec_transaction(trx, accs);
					}

					bool process_and_send_data(PSQL::sql_dumper &dumper, const cached_data_t &data, transaction_t& _transaction)
					{
						auto tm = fc::time_point::now();
						auto measure_time = [&](const char* _msg){
							mylog(generate([&](fc::string& ss){
								ss += fc::string{"Elapsed time for "} + _msg + ": " + std::to_string(( fc::time_point::now() - tm ).count()) + "\n";
							}).c_str());
							tm = fc::time_point::now();
						};

						for (size_t i = 0; i < data.blocks.size(); i++)
						{
							const size_t sz = dumper.process_block(data.blocks[i]);
							if( 
								sz >= max_data_length ||
								i == max_tuples_count ||
								i == data.blocks.size() - 1
							)
							{
								if(!send_data( dumper.blocks, _transaction )) return false;
								dumper.reset_blocks_stream();
							}
						}
						measure_time("processing and sending blocks");

						for (size_t i = 0; i < data.transactions.size(); i++)
						{
							const size_t sz = dumper.process_transaction(data.transactions[i]);
							if(
								sz >= max_data_length ||
								i == max_tuples_count ||
								i == data.transactions.size() - 1
							)
							{
								if(!send_data( dumper.transactions, _transaction )) return false;
								dumper.reset_transaction_stream();
							}
						}
						measure_time("processing and sending transactions");

						for (size_t i = 0; i < data.operations.size(); i++)
						{
							const size_t sz = dumper.process_operation(data.operations[i]);
							if( 
								sz >= max_data_length ||
								i == max_tuples_count ||
								i == data.operations.size() - 1
							)
							{
								if(!send_data( dumper.reset_operations_stream(), _transaction )) return false;
							}
						}
						measure_time("processing and sending operations");

						for (size_t i = 0; i < data.virtual_operations.size(); i++)
						{
							const size_t sz = dumper.process_virtual_operation(data.virtual_operations[i]);
							if( 
								sz >= max_data_length ||
								i == max_tuples_count || 
								i == data.virtual_operations.size() - 1
							)
							{
								if(!send_data( dumper.reset_virtual_operation_stream(), _transaction )) return false;
							}
						}
						measure_time("processing and sending virtual operations");

						upload_caches(_transaction, dumper);
						measure_time("processing and sending account_names and permlinks");


						return true;
					}

					bool send_data(const fc::string &dump, transaction_t &transaction)
					{
						return connection.exec_transaction(transaction, dump);
					}

					void join_ready_threads(const bool force = false)
					{
						int cnt = 0;
						threads.remove_if([force, &cnt](const thread_with_status& th){ if(force || th.second){ cnt++; return true; } else return false; });
						mylog(generate([&](fc::string& ss){ ss += "currently running " + std::to_string(threads.size()) + " threads. Joined: " + std::to_string(cnt); }).c_str());
					}

					void switch_constraints(const bool active)
					{
						if(are_constraints_active == active) return;
						else are_constraints_active = active;

						using up_and_down_constraint = std::pair<const char*, const char*>;
						static const std::vector<up_and_down_constraint> constrainsts{{	up_and_down_constraint
							{"ALTER TABLE hive_transactions ADD CONSTRAINT hive_transactions_fk_1 FOREIGN KEY (block_num) REFERENCES hive_blocks (num)","ALTER TABLE hive_operations DROP CONSTRAINT IF EXISTS hive_operations_fk_1"},
							{"ALTER TABLE hive_operations ADD CONSTRAINT hive_operations_fk_1 FOREIGN KEY (op_type_id) REFERENCES hive_operation_types (id)","ALTER TABLE hive_operations DROP CONSTRAINT IF EXISTS hive_operations_fk_1"},
							{"ALTER TABLE hive_operations ADD CONSTRAINT hive_operations_fk_2 FOREIGN KEY (block_num, trx_in_block) REFERENCES hive_transactions (block_num, trx_in_block)","ALTER TABLE hive_operations DROP CONSTRAINT IF EXISTS hive_operations_fk_2"},
							{"ALTER TABLE hive_virtual_operations ADD CONSTRAINT hive_virtual_operations_fk_1 FOREIGN KEY (op_type_id) REFERENCES hive_operation_types (id)","ALTER TABLE hive_virtual_operations DROP CONSTRAINT IF EXISTS hive_virtual_operations_fk_1"},
							{"ALTER TABLE hive_virtual_operations ADD CONSTRAINT hive_virtual_operations_fk_2 FOREIGN KEY (block_num) REFERENCES hive_blocks (num)","ALTER TABLE hive_virtual_operations DROP CONSTRAINT IF EXISTS hive_virtual_operations_fk_2"}
						}};

						auto trx = connection.start_transaction();
						for (const auto &idx : constrainsts)
						{
							FC_ASSERT( connection.exec_transaction(trx, idx.second ), "failed when a dropping constraint" );
							if(active && !connection.exec_transaction(trx, idx.first ))
							{
								connection.abort_transaction(trx);
								FC_ASSERT( false );
							}
						}
						connection.commit_transaction(trx);
					}

					void process_cached_data()
					{
						if(currently_caching_data.get()) return;

						mylog("created new thread");
						const auto cache_processor_function = [this](cached_containter_t input, bool* is_ready) {
							auto finish = [&](const bool is_ok) { if(is_ready) *is_ready = true; FC_ASSERT( is_ok ); };
							if (input.get() == nullptr)
							{
								finish(true);
								return;
							}

							transaction_t trx = this->connection.start_transaction();
							PSQL::sql_dumper dumper{_db, trx->get_escaping_charachter_methode()};

							// sending
							if (!this->process_and_send_data(dumper, *input, trx))
							{
								mylog("aborting transaction");
								this->connection.abort_transaction(trx);
								finish(false);
							}
							else
							{
								mylog("commiting transaction");
								finish(this->connection.commit_transaction(trx));
							}
						};

						mylog("creating new thread");
						threads.emplace_back(nullptr, false);
						currently_caching_data->log_size();
						threads.back().first.reset(
								new std::thread(cache_processor_function, std::move(currently_caching_data), &threads.back().second),
								[](std::thread *ptr) { if(ptr){ptr->join(); delete ptr;} });

					}

					void push_currently_cached_data(const size_t reserve)
					{
						process_cached_data();
						if (reserve)
							currently_caching_data = std::make_unique<cached_data_t>(reserve);
					}

					void cleanup_sequence()
					{
						ilog("Flushing rest of data, wait a moment...");
						push_currently_cached_data(0);
						join_ready_threads(true);
						if(are_constraints_active) return;
						if (set_index)
						{
							ilog("Creating indexes on user request");
							create_indexes();
						}
						ilog("Enabling constraints...");
						switch_constraints(true);
						ilog("Done, cleanup complete");
					}
				};
			} // namespace detail

			sql_serializer_plugin::sql_serializer_plugin() {}
			sql_serializer_plugin::~sql_serializer_plugin() {}

			void sql_serializer_plugin::set_program_options(options_description &cli, options_description &cfg)
			{
				cfg.add_options()("psql-url", boost::program_options::value<string>(), "postgres connection string")("psql-path-to-schema", "if set and replay starts from 0 block, executes script")("psql-reindex-on-exit", "creates indexes and PK on exit");
			}

			void sql_serializer_plugin::plugin_initialize(const boost::program_options::variables_map &options)
			{
				ilog("Initializing sql serializer plugin");
				FC_ASSERT(options.count("psql-url"), "`psql-url` is required argument");
				my = std::make_unique<detail::sql_serializer_plugin_impl>(options["psql-url"].as<fc::string>());

				// settings
				if (options.count("psql-path-to-schema"))
					my->path_to_schema = options["psql-path-to-schema"].as<fc::string>();
				my->set_index = options.count("psql-reindex-on-exit") > 0;
				my->currently_caching_data = std::make_unique<detail::cached_data_t>( default_reservation_size );

				// signals
				auto &db = appbase::app().get_plugin<hive::plugins::chain::chain_plugin>().db();
				my->_on_post_apply_operation_con = db.add_post_apply_operation_handler([&](const operation_notification &note) { on_post_apply_operation(note); }, *this);
				my->_on_post_apply_block_con = db.add_post_apply_block_handler([&](const block_notification &note) { on_post_apply_block(note); }, *this);
				my->_on_finished_reindex = db.add_post_reindex_handler([&](const reindex_notification &note) { on_post_reindex(note); }, *this);
				my->_on_starting_reindex = db.add_pre_reindex_handler([&](const reindex_notification &note) { on_pre_reindex(note); }, *this);
			}

			void sql_serializer_plugin::plugin_startup()
			{
				ilog("sql::plugin_startup()");
			}

			void sql_serializer_plugin::plugin_shutdown()
			{
				ilog("Flushing left data...");
				my->join_ready_threads(true);

				if (my->_on_post_apply_block_con.connected())
					chain::util::disconnect_signal(my->_on_post_apply_block_con);
				if (my->_on_post_apply_operation_con.connected())
					chain::util::disconnect_signal(my->_on_post_apply_operation_con);
				if (my->_on_starting_reindex.connected())
					chain::util::disconnect_signal(my->_on_starting_reindex);
				if (my->_on_finished_reindex.connected())
					chain::util::disconnect_signal(my->_on_finished_reindex);

				ilog("Done. Connection closed");
			}

			void sql_serializer_plugin::on_post_apply_operation(const operation_notification &note)
			{
				const bool is_virtual = note.op.visit(PSQL::is_virtual_visitor{});
				fc::string deserialized_op = fc::json::to_string(note.op);
				detail::cached_containter_t &cdtf = my->currently_caching_data; // alias
				cdtf->total_size += deserialized_op.size() + sizeof(note);
				if (is_virtual)
				{
					cdtf->virtual_operations.emplace_back(
							note.block,
							note.trx_in_block,
							note.op_in_trx,
							note.op,
							std::move(deserialized_op)
						);
				}
				else
				{
					cdtf->operations.emplace_back(
							note.block,
							note.trx_in_block,
							note.op_in_trx,
							note.op,
							std::move(deserialized_op)
						);
				}
			}

			void sql_serializer_plugin::on_post_apply_block(const block_notification &note)
			{
				for (int64_t i = 0; i < static_cast<int64_t>(note.block.transactions.size()); i++)
					handle_transaction(note.block.transactions[i].id(), note.block_num, i);

				my->currently_caching_data->total_size += note.block_id.data_size() + sizeof(note.block_num);
				my->currently_caching_data->blocks.emplace_back(
						note.block_id,
						note.block_num);

				if( my->currently_caching_data->total_size >= max_data_length )
				{
					mylog("pushing because of size");
					my->push_currently_cached_data(prereservation_size);
				}
				else if( note.block_num % my->blocks_per_commit == 0 )
				{
					mylog("pushing because of tuple size");
					my->push_currently_cached_data(prereservation_size);
				}

				if(my->threads.size() > 100 ) my->join_ready_threads();
			}

			void sql_serializer_plugin::handle_transaction(const hive::protocol::transaction_id_type &hash, const int64_t block_num, const int64_t trx_in_block)
			{
				if(block_num >= 14'500'000) mylog("adding trx");
				my->currently_caching_data->total_size += sizeof(hash) + sizeof(block_num) + sizeof(trx_in_block);
				my->currently_caching_data->transactions.emplace_back(
						hash,
						block_num,
						trx_in_block);
			}

			void sql_serializer_plugin::on_pre_reindex(const reindex_notification &note)
			{
				if (note.force_replay && my->path_to_schema.valid())
				{
					my->recreate_db();
					my->are_constraints_active = false;
				}else my->switch_constraints(false);

				my->blocks_per_commit = 10'000;
			}

			void sql_serializer_plugin::on_post_reindex(const reindex_notification &)
			{
				std::cout << "finishing from post reindex" << std::endl;
				// flush rest of data
				my->cleanup_sequence();
				my->blocks_per_commit = 1;
			}

		} // namespace sql_serializer
	}		// namespace plugins
} // namespace hive
