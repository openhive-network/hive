/*

destruktor - przenieść kończenie
konstruktor- przenieść tworzenie
kontener komunikacja - paczka po 1000


*/

#include <hive/plugins/sql_serializer/sql_serializer_plugin.hpp>

#include <hive/chain/util/impacted.hpp>
#include <hive/protocol/config.hpp>
#include <hive/utilities/plugin_utilities.hpp>

#include <fc/io/json.hpp>
#include <fc/smart_ref_impl.hpp>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>

// C++ connector library for PostgreSQL (http://pqxx.org/development/libpqxx/)
#include <pqxx/pqxx>

#include <thread>

namespace hive
{
	namespace plugins
	{
		namespace sql_serializer
		{
			using namespace hive::protocol;
			using namespace hive::plugins::sql_serializer::PSQL;

			using chain::database;
			using chain::operation_object;

			namespace detail
			{
				using transaction_t = std::unique_ptr<pqxx::work>;

				struct postgress_connection_holder
				{

					explicit postgress_connection_holder(const fc::string &url)
							: _connection{std::make_unique<pqxx::connection>(url)} {}

					transaction_t start_transaction() const
					{
						pqxx::work* trx = new pqxx::work{ *_connection };
						trx->exec("SET CONSTRAINTS ALL DEFERRED;");
						return transaction_t{trx};
					}

					bool exec_transaction(transaction_t& trx, const fc::string& sql)
					{
						if(sql == fc::string()) return true;
						// else std::cout << "executing: " << sql << std::endl;
						return sql_safe_execution([&trx, &sql]() { trx->exec(sql); });
					}

					bool commit_transaction(transaction_t& trx)
					{
						// std::cout << "commiting" << std::endl;
						return sql_safe_execution([&]() { trx->commit(); destroy_transaction(trx); });
					}

					void abort_transaction(transaction_t& trx)
					{
						// std::cout << "aborting" << std::endl;
						trx->abort();
						destroy_transaction(trx);
					}

					bool exec_no_transaction(const fc::string& sql, pqxx::result * result = nullptr)
					{
						if(sql == fc::string()) return true;
						return sql_safe_execution([&]() {
							pqxx::nontransaction work{ *_connection };
							if(result) *result = work.exec(sql);
							else work.exec(sql);
						});
					}

				private:
					std::unique_ptr<pqxx::connection> _connection;

					void destroy_transaction(transaction_t& trx) const { trx.~unique_ptr(); }

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

				class sql_serializer_plugin_impl
				{

				public:
					sql_serializer_plugin_impl(const std::string &url) : _db(appbase::app().get_plugin<hive::plugins::chain::chain_plugin>().db()),
																															connection{url}
					{
						worker = std::make_shared<std::thread>([&]() {

							transaction_t trx;

							// worker
							processing_object_t input;
							sql_command_queue.wait_pull(input);

							// work loop
							while (input.visit(PSQL::processing_objects::is_processable_visitor{}))
							{
								// flow controll
								if(input.visit(PSQL::processing_objects::commit_command_visitor{}))
								{
									connection.commit_transaction(trx);
									sql_command_queue.wait_pull(input);
									continue;
								}else if( trx.get() == nullptr )
								{
									trx = connection.start_transaction();
								}

								// dump data
								if(!connection.exec_transaction( trx, input.visit(PSQL::sql_dump_visitor{
										_db,
										names_to_flush,
										[&](const char *str_to_esc) { return trx->esc(str_to_esc); }
									})
								))
								{
									connection.abort_transaction(trx);
									FC_ASSERT( false );
								}

								// gather next element
								sql_command_queue.wait_pull(input);
							}
						});
					}

					virtual ~sql_serializer_plugin_impl()
					{
						ilog("Flushing left data...");
						sql_command_queue.wait_push(PSQL::processing_objects::commit_data_t{});
						sql_command_queue.wait_push(PSQL::processing_objects::end_processing_t{});
						worker->join();
						if (set_index)
						{
							ilog("Creating indexes on user request");
							create_indexes();
						}
						ilog("Done. Connection closed");
					}

					database &_db;
					boost::signals2::connection _on_post_apply_operation_con;
					boost::signals2::connection _on_post_apply_trx_con;
					boost::signals2::connection _on_post_apply_block_con;
					boost::signals2::connection _on_starting_reindex;
					boost::signals2::connection _on_finished_reindex;

					postgress_connection_holder connection;
					fc::optional<fc::string> path_to_schema;
					bool set_index = false;

					std::shared_ptr<std::thread> worker;
					PSQL::queue<processing_object_t> sql_command_queue;
					uint32_t blocks_per_commit = 1;

					operation_types_container_t names_to_flush;
					std::list<PSQL::processing_objects::process_operation_t> cached_virtual_ops;

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
				};
			} // namespace detail

			sql_serializer_plugin::sql_serializer_plugin() {}
			sql_serializer_plugin::~sql_serializer_plugin()
			{
				if (my->_on_post_apply_block_con.connected())
					chain::util::disconnect_signal(my->_on_post_apply_block_con);
				if (my->_on_post_apply_trx_con.connected())
					chain::util::disconnect_signal(my->_on_post_apply_trx_con);
				if (my->_on_post_apply_operation_con.connected())
					chain::util::disconnect_signal(my->_on_post_apply_operation_con);
				if (my->_on_starting_reindex.connected())
					chain::util::disconnect_signal(my->_on_starting_reindex);
				if (my->_on_finished_reindex.connected())
					chain::util::disconnect_signal(my->_on_finished_reindex);
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
				my->_on_post_apply_trx_con = db.add_post_apply_transaction_handler([&](const transaction_notification &note) { on_post_apply_trx(note); }, *this);
				my->_on_post_apply_block_con = db.add_post_apply_block_handler([&](const block_notification &note) { on_post_apply_block(note); }, *this);
				my->_on_finished_reindex = db.add_post_reindex_handler([&](const reindex_notification &) { my->blocks_per_commit = 1; },
																															 *this);

				my->_on_starting_reindex = db.add_pre_reindex_handler([&](const reindex_notification & note) { 
					if(note.force_replay && my->path_to_schema.valid())
						my->recreate_db();
					my->blocks_per_commit = 1000; 
					},*this);
			}

			void sql_serializer_plugin::plugin_startup() {}

			void sql_serializer_plugin::plugin_shutdown() {}

			void sql_serializer_plugin::on_post_apply_operation(const operation_notification &note)
			{
				if (note.block < 2)
					return;

				const bool is_virtual = note.op.visit(PSQL::is_virtual_visitor{});
				PSQL::processing_objects::process_operation_t obj{
						note.block,
						note.trx_in_block,
						note.op_in_trx,
						note.op
				};

				// if virtual push on front, so while iterating it will be in 'proper' order
				if (is_virtual && note.trx_in_block >= 0)
					my->cached_virtual_ops.emplace_back(obj);
				else
				{
					my->sql_command_queue.wait_push(obj);
					for (const auto &vop : my->cached_virtual_ops)
						my->sql_command_queue.wait_push(vop);
					my->cached_virtual_ops.clear();
				}
			}

			void sql_serializer_plugin::on_post_apply_block(const block_notification &note)
			{
				for (int64_t i = 0; i < static_cast<int64_t>(note.block.transactions.size()); i++)
					handle_transaction(note.block.transactions[i].id().str(), note.block_num, i);

				processing_object_t obj = PSQL::processing_objects::process_block_t(
						note.block_id.str(),
						note.block_num);
				my->sql_command_queue.wait_push(obj);

				if(note.block_num % my->blocks_per_commit == 0) 
					my->sql_command_queue.wait_push(PSQL::processing_objects::commit_data_t{});
			}

			void sql_serializer_plugin::on_post_apply_trx(const transaction_notification &) {}

			void sql_serializer_plugin::handle_transaction(const fc::string &hash, const int64_t block_num, const int64_t trx_in_block)
			{
				processing_object_t obj = PSQL::processing_objects::process_transaction_t(
						hash,
						block_num,
						trx_in_block);
				my->sql_command_queue.wait_push(obj);
			}

			void sql_serializer_plugin::initialize_varriables()
			{
				// load current operation type
				pqxx::result result;
				my->connection.exec_no_transaction("SELECT id, name, is_virtual FROM hive_operation_types", &result);
				for(const auto& row : result)
					my->names_to_flush[ row.at(0).as<int64_t>() ] = std::make_pair( row.at(1).as<fc::string>(), row.at(2).as<bool>() );
			}

		} // namespace sql_serializer
	}		// namespace plugins
} // namespace hive