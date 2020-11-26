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

			namespace detail
			{
				using transaction_t = std::unique_ptr<work>;

				struct postgress_connection_holder
				{
					explicit postgress_connection_holder(const fc::string &url)
							: _connection{std::make_unique<pqxx::connection>(url)} {}

					transaction_t start_transaction()
					{
						work *trx = new work{*_connection};
						trx->exec("SET CONSTRAINTS ALL DEFERRED;");
						return transaction_t{trx};
					}

					bool exec_transaction(transaction_t &trx, const fc::string &sql)
					{
						if (sql == fc::string())
							return true;
						// else std::cout << "executing: " << sql << std::endl;
						return sql_safe_execution([&trx, &sql]() { trx->exec(sql); });
					}

					bool commit_transaction(transaction_t &trx)
					{
						// std::cout << "commiting" << std::endl;
						return sql_safe_execution([&]() { trx->commit(); destroy_transaction(trx); });
					}

					void abort_transaction(transaction_t &trx)
					{
						// std::cout << "aborting" << std::endl;
						trx->abort();
						destroy_transaction(trx);
					}

					bool exec_no_transaction(const fc::string &sql, pqxx::result *result = nullptr)
					{
						if (sql == fc::string())
							return true;
						return sql_safe_execution([&]() {
							pqxx::nontransaction _work{*_connection};
							if (result)
								*result = _work.exec(sql);
							else
								_work.exec(sql);
						});
					}

					auto get_escaping_charachter_methode()
					{
						return [this](const char *val) -> fc::string { return this->_connection->esc(val); }; //  { return _connection.esc(val); }
					}

				private:
					std::unique_ptr<pqxx::connection> _connection;
					// std::mutex mtx;

					void destroy_transaction(transaction_t &trx) const { trx.~unique_ptr(); }

					bool sql_safe_execution(const std::function<void()> &f)
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
						return false;
					}
				};

				struct cached_data_t
				{
					std::vector<PSQL::processing_objects::process_block_t> blocks;
					std::vector<PSQL::processing_objects::process_transaction_t> transactions;
					std::vector<PSQL::processing_objects::process_operation_t> operations;
					std::vector<PSQL::processing_objects::process_operation_t> virtual_operations;

					cached_data_t(const size_t reservation_size)
					{
						blocks.reserve(reservation_size);
						transactions.reserve(reservation_size);
						operations.reserve(reservation_size);
						virtual_operations.reserve(reservation_size);
					}
				};

				class sql_serializer_plugin_impl
				{

				public:
					sql_serializer_plugin_impl(const std::string &url) : _db(appbase::app().get_plugin<hive::plugins::chain::chain_plugin>().db()),
																															 connection{url}
					{
					}

					virtual ~sql_serializer_plugin_impl() {}

					database &_db;
					boost::signals2::connection _on_post_apply_operation_con;
					boost::signals2::connection _on_post_apply_block_con;
					boost::signals2::connection _on_starting_reindex;
					boost::signals2::connection _on_finished_reindex;

					postgress_connection_holder connection;
					fc::optional<fc::string> path_to_schema;
					bool set_index = false;

					using thread_with_status = std::pair<std::shared_ptr<std::thread>, bool>;
					thread_with_status worker;
					uint32_t blocks_per_commit = 1;
					uint32_t last_commit_block = 0;

					operation_types_container_t names_to_flush;
					using cached_containter_t = std::shared_ptr<cached_data_t>;
					cached_containter_t currently_caching_data;
					PSQL::queue<cached_containter_t> cached_objects;

					void create_indexes()
					{
						static const std::vector<fc::string> indexes{{R"(CREATE INDEX IF NOT EXISTS hive_operations_operation_types_index ON "hive_operations" ("op_type_id");)",
																													R"(CREATE INDEX IF NOT EXISTS hive_operations_participants_index ON "hive_operations" USING GIN ("participants");)",
																													R"(CREATE INDEX IF NOT EXISTS hive_operations_permlink_ids_index ON "hive_operations" USING GIN ("permlink_ids");)",
																													R"(CREATE INDEX IF NOT EXISTS hive_virtual_operations_operation_types_index ON "hive_virtual_operations" ("op_type_id");)",
																													R"(CREATE INDEX IF NOT EXISTS hive_virtual_operations_participants_index ON "hive_virtual_operations" USING GIN ("participants");)"}};

						for (const auto &idx : indexes)
							connection.exec_no_transaction(idx);
					}

					void recreate_db()
					{
						using bpath = boost::filesystem::path;

						bpath path = bpath((*path_to_schema).c_str());
						const size_t size = boost::filesystem::file_size(path);
						std::unique_ptr<char[]> _data{new char[size]};
						std::ifstream file{path.string()};
						file.read(_data.get(), size);

						connection.exec_no_transaction(_data.get());
					}

					void create_caches(transaction_t &trx, PSQL::sql_dumper &dumper)
					{
						fc::string perms, accs;
						dumper.get_dumped_cache(perms, accs);

						if (perms != fc::string())
						{
							connection.exec_transaction(trx, "SELECT prepare_permlink_cache()");
							connection.exec_transaction(trx, perms);
							connection.exec_transaction(trx, "SELECT fill_permlink_cache()");
						}

						if (accs != fc::string())
						{
							connection.exec_transaction(trx, "SELECT prepare_accounts_cache()");
							connection.exec_transaction(trx, accs);
							connection.exec_transaction(trx, "SELECT fill_accounts_cache()");
						}
					}

					void process_data(PSQL::sql_dumper &dumper, const cached_data_t &data)
					{
						for (const auto &block : data.blocks)
							dumper.process_block(block);
						for (const auto &trx : data.transactions)
							dumper.process_transaction(trx);
						for (const auto &op : data.operations)
							dumper.process_operation(op);
						for (const auto &vop : data.virtual_operations)
							dumper.process_virtual_operation(vop);
					}

					bool send_data(const PSQL::sql_dumper &dumper, transaction_t &transaction)
					{
						return (!dumper.any_blocks || connection.exec_transaction(transaction, dumper.blocks.str())) &&
									 (!dumper.any_transactions || connection.exec_transaction(transaction, dumper.transactions.str())) &&
									 (!dumper.any_operations || connection.exec_transaction(transaction, dumper.operations.str())) &&
									 (!dumper.any_virtual_operations || connection.exec_transaction(transaction, dumper.virtual_operations.str()));
					}

					void join_ready_threads(const bool force = false)
					{
						if (worker.first.get() && (force || worker.second))
							worker.first.reset();
					}

					void switch_constraints(const bool active)
					{
						const static std::array<const char *, 3> tables_to_disable{{"hive_transactions", "hive_operations", "hive_virtual_operations"}};
						for (const auto &table : tables_to_disable)
							connection.exec_no_transaction(generate([&](strstrm &ss) {
								ss << "ALTER TABLE " << table << (active ? " ENABLE" : " DISABLE") << " TRIGGER ALL";
							}));
					}

					void start_worker_thread()
					{
						if (worker.first.get()) return;
						worker = thread_with_status(nullptr, false);
						worker.first.reset(
								new std::thread([this](cached_containter_t input, bool *is_ready) {
									auto tm = fc::time_point().now();
									auto measure_time = [&](const char *caption) {
										// std::cout << caption << (fc::time_point().now() - tm).count() << std::endl;
										tm = fc::time_point().now();
									};

									if (input.get() == nullptr)
										return;

									// processing
									PSQL::sql_dumper dumper{_db, names_to_flush, this->connection.get_escaping_charachter_methode()}; //[&](const char *str_to_esc) { return connection->esc(str_to_esc); }};
									dumper.init();
									this->process_data(dumper, *input);
									measure_time("processing: ");
									input.reset();

									// acquiring lock
									transaction_t trx = this->connection.start_transaction();
									measure_time("starting transaction: ");
									this->create_caches(trx, dumper);
									measure_time("creating caches: ");
									if (!this->send_data(dumper, trx))
									{
										this->connection.abort_transaction(trx);
										*is_ready = true;
										FC_ASSERT(false);
									}
									else
									{
										*is_ready = true;
										this->connection.commit_transaction(trx);
									}
									measure_time("commiting: ");
								}, currently_caching_data, &worker.second),
								[](std::thread *ptr) { if(ptr){ptr->join(); delete ptr;} });
					}

					void push_currently_cached_data(const size_t reserve)
					{
						start_worker_thread();
						if(reserve)
							currently_caching_data = std::make_shared<cached_data_t>(reserve);
					}

					// return fresh
					bool is_new_op_name(const operation &op, const bool is_virtual)
					{
						const auto it = names_to_flush.find(op.which());
						if (it != names_to_flush.end())
							return false;
						else
						{
							names_to_flush[op.which()] = std::pair<fc::string, bool>(op.visit(name_gathering_visitor{}).c_str(), is_virtual);
							return true;
						}
					}
				};
			} // namespace detail

			sql_serializer_plugin::sql_serializer_plugin() {}
			sql_serializer_plugin::~sql_serializer_plugin() 
			{
				// flush rest of non-committed data
				my->push_currently_cached_data(0);
			}

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
				initialize_varriables();

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
				// my->start_worker_thread();
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
				auto &cdtf = my->currently_caching_data; // alias
				if (is_virtual)
				{
					cdtf->virtual_operations.emplace_back(
							note.block,
							note.trx_in_block,
							note.op_in_trx,
							note.op,
							my->is_new_op_name(note.op, is_virtual));
				}
				else
				{
					cdtf->operations.emplace_back(
							note.block,
							note.trx_in_block,
							note.op_in_trx,
							note.op,
							my->is_new_op_name(note.op, is_virtual));
				}
			}

			void sql_serializer_plugin::on_post_apply_block(const block_notification &note)
			{
				for (int64_t i = 0; i < static_cast<int64_t>(note.block.transactions.size()); i++)
					handle_transaction(note.block.transactions[i].id().str(), note.block_num, i);

				my->currently_caching_data->blocks.emplace_back(
						note.block_id.str(),
						note.block_num);

				if (note.block_num - my->last_commit_block >= my->blocks_per_commit && my->worker.first.get())
				{
					my->push_currently_cached_data(note.block_num - my->last_commit_block);
					my->last_commit_block = note.block_num;
				}
				else
					my->join_ready_threads();
			}

			void sql_serializer_plugin::handle_transaction(const fc::string &hash, const int64_t block_num, const int64_t trx_in_block)
			{
				my->currently_caching_data->transactions.emplace_back(
						hash,
						block_num,
						trx_in_block);
			}

			void sql_serializer_plugin::initialize_varriables()
			{
				// load current operation type
				my->currently_caching_data = std::make_unique<detail::cached_data_t>(16'000);
				pqxx::result result;
				my->connection.exec_no_transaction("SELECT id, name, is_virtual FROM hive_operation_types", &result);
				for (const auto &row : result)
					my->names_to_flush[row.at(0).as<int64_t>()] = std::make_pair(row.at(1).as<fc::string>(), row.at(2).as<bool>());
			}

			void sql_serializer_plugin::on_pre_reindex(const reindex_notification &note)
			{
				my->last_commit_block = note.last_block_number;
				if (note.force_replay && my->path_to_schema.valid())
					my->recreate_db();
				my->blocks_per_commit = 10'000;
				my->switch_constraints(false);
			}

			void sql_serializer_plugin::on_post_reindex(const reindex_notification &)
			{
				// flush rest of data
				my->push_currently_cached_data(0);
				my->blocks_per_commit = 1;
				my->switch_constraints(true);
				if (my->set_index)
				{
					ilog("Creating indexes on user request");
					my->create_indexes();
				}
			}

		} // namespace sql_serializer
	}		// namespace plugins
} // namespace hive