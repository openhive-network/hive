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
				class sql_serializer_plugin_impl
				{

				public:
					sql_serializer_plugin_impl(const std::string &url) : _db(appbase::app().get_plugin<hive::plugins::chain::chain_plugin>().db()),
																															 connection_url{url}
					{
					}

					virtual ~sql_serializer_plugin_impl() {}

					database &_db;
					boost::signals2::connection _on_post_apply_operation_con;
					boost::signals2::connection _on_post_apply_trx_con;
					boost::signals2::connection _on_post_apply_block_con;
					boost::signals2::connection _on_starting_reindex;
					boost::signals2::connection _on_finished_reindex;

					fc::string connection_url;
					fc::optional<fc::string> path_to_schema;
					bool set_index = false;

					std::shared_ptr<std::thread> worker;
					PSQL::queue<processing_object_t> sql_command_queue;

					PSQL::counter_container_t counters;
					operation_types_container_t names_to_flush;

					const std::map<TABLE, table_flush_buffer> flushes{
						{	
							table_flush_values_cell_t{TABLE::BLOCKS, table_flush_buffer{"hive_blocks"}},
							table_flush_values_cell_t{TABLE::TRANSACTIONS, table_flush_buffer{"hive_transactions"}},

							table_flush_values_cell_t{TABLE::ACCOUNTS, table_flush_buffer{"hive_accounts"}},

							table_flush_values_cell_t{TABLE::OPERATIONS, table_flush_buffer{"hive_operations"}},
							table_flush_values_cell_t{TABLE::VIRTUAL_OPERATIONS, table_flush_buffer{"hive_virtual_operations"}},

							table_flush_values_cell_t{TABLE::OPERATION_NAMES, table_flush_buffer{"hive_operation_types"}},
							table_flush_values_cell_t{TABLE::PERMLINK_DICT, table_flush_buffer{"hive_permlink_data"}}
						}
					};

					const fc::string &get_table_name(const TABLE tbl) const
					{
						FC_ASSERT(tbl != TABLE::END);
						return flushes.at(tbl).table_name;
					}

					void create_indexes() const
					{
						static const std::vector<fc::string> indexes{{R"(CREATE INDEX IF NOT EXISTS hive_operations_operation_types_index ON "hive_operations" ("op_type_id");)",
																													R"(CREATE INDEX IF NOT EXISTS hive_operations_participants_index ON "hive_operations" USING GIN ("participants");)",
																													R"(CREATE INDEX IF NOT EXISTS hive_operations_permlink_ids_index ON "hive_operations" USING GIN ("permlink_ids");)",
																													R"(CREATE INDEX IF NOT EXISTS hive_virtual_operations_operation_types_index ON "hive_virtual_operations" ("op_type_id");)",
																													R"(CREATE INDEX IF NOT EXISTS hive_virtual_operations_participants_index ON "hive_virtual_operations" USING GIN ("participants");)"}};

						pqxx::connection conn{connection_url};
						FC_ASSERT(conn.is_open());
						for (const auto &idx : indexes)
							pqxx::nontransaction{conn}.exec(idx);
					}

					void recreate_db() const
					{
						using bpath = boost::filesystem::path;

						bpath path = bpath((*path_to_schema).c_str());
						const size_t size = boost::filesystem::file_size(path);
						std::unique_ptr<char[]> _data{new char[size]};
						std::ifstream file{path.string()};
						file.read(_data.get(), size);

						pqxx::connection conn{connection_url};
						FC_ASSERT(conn.is_open());
						pqxx::nontransaction{conn}.exec(const_cast<const char *>(_data.get()));
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
				my->_on_finished_reindex = db.add_post_reindex_handler([&](const reindex_notification &note) {
					ilog("Flushing left data...");
					my->sql_command_queue.wait_push(PSQL::processing_objects::end_processing_t{});
					my->worker->join();
					if (my->set_index)
					{
						ilog("Creating indexes on user request");
						my->create_indexes();
					}
					ilog("Done. Connection closed");
				},
																															 *this);

				my->_on_starting_reindex = db.add_pre_reindex_handler([&](const reindex_notification &note) {
					my->worker = std::make_shared<std::thread>([&]() {
						pqxx::connection conn{my->connection_url.c_str()};

						// flush all data
						auto flusher = [&]() {
							FC_ASSERT(conn.is_open());

							// If there is no blocks to flush end
							if (my->flushes.find(TABLE::BLOCKS)->second.to_dump->empty())
								return;

							// flush op names
							for (const auto &kv : my->names_to_flush)
								my->flushes.at(TABLE::OPERATION_NAMES).to_dump->push("( " + std::to_string(kv.first) + " , '" + kv.second.first + "', " + (kv.second.second ? "TRUE" : "FALSE") + " )");

							pqxx::nontransaction trx{conn};
							trx.exec("BEGIN;");

							// Update table by table
							for (const auto &kv : my->flushes)
							{
								std::stringstream ss;
								std::string str;
								auto &buffer = kv.second;

								if (buffer.to_dump->empty())
									continue;

								buffer.to_dump->pull(str);
								ss << "INSERT INTO " << buffer.table_name << " VALUES " << str;
								while (!buffer.to_dump->empty())
								{
									buffer.to_dump->pull(str);
									ss << "," << str;
								}

								// Theese tables contains constant data, that is unique on hived site
								if (kv.first == TABLE::OPERATION_NAMES)
									ss << " ON CONFLICT DO NOTHING";

								ss << ";";
								trx.exec(ss.str());
							}

							trx.exec("COMMIT;");
						};

						// worker
						processing_object_t input;
						my->sql_command_queue.wait_pull(input);

						// work loop
						while (input.visit( PSQL::processing_objects::is_processable_visitor{} ))
						{
							sql_command_t vec;
							input.visit(PSQL::sql_dump_visitor{
								my->_db,
								vec,
								my->names_to_flush,
								[&](const char *str_to_esc) {
									return conn.esc(str_to_esc); 
							}});

								// flush
								for (const auto &v : vec)
								{
									my->flushes.at(v.destination_table).to_dump->push(v.insertion_tuple);
									if (v.destination_table == TABLE::BLOCKS && my->flushes.at(v.destination_table).to_dump->size() == 1'000)
										flusher();
								}

								// gather next element
								my->sql_command_queue.wait_pull(input);
						}

						// flush what hasn't been already flushed
						flusher();
						});
					},
					*this
				);
			}

			void sql_serializer_plugin::plugin_startup() {}

			void sql_serializer_plugin::plugin_shutdown() {}

			void sql_serializer_plugin::on_post_apply_operation(const operation_notification &note)
			{
					static std::list<PSQL::processing_objects::process_operation_t> cached_virtual_ops;

					if (note.block < 2)
						return;

					const bool is_virtual = note.op.visit(PSQL::is_virtual_visitor{});
					auto &cnter = my->counters.find(( is_virtual ? TABLE::VIRTUAL_OPERATIONS : TABLE::OPERATIONS))->second;
					PSQL::processing_objects::process_operation_t obj(
						++cnter,
						note.block,
						note.trx_in_block,
						note.op_in_trx,
						note.op
					);

					// if virtual push on front, so while iterating it will be in 'proper' order
					if(is_virtual && note.trx_in_block >= 0) cached_virtual_ops.emplace_back(obj);
					else
					{
						my->sql_command_queue.wait_push(obj);
						for(const auto& vop : cached_virtual_ops)
							my->sql_command_queue.wait_push(vop);
						cached_virtual_ops.clear();
					}
			}

			void sql_serializer_plugin::on_post_apply_block(const block_notification &note)
			{
					for (int64_t i = 0; i < static_cast<int64_t>(note.block.transactions.size()); i++)
						handle_transaction(note.block.transactions[i].id().str(), note.block_num, i);

					processing_object_t obj = PSQL::processing_objects::process_block_t(
						note.block_id.str(),
						note.block_num
					);
					my->sql_command_queue.wait_push(obj);
			}

			void sql_serializer_plugin::on_post_apply_trx(const transaction_notification &) {}

			void sql_serializer_plugin::handle_transaction(const fc::string &hash, const int64_t block_num, const int64_t trx_in_block)
			{
					processing_object_t obj = PSQL::processing_objects::process_transaction_t(
						hash,
						block_num,
						trx_in_block
					);
					my->sql_command_queue.wait_push(obj);
			}

			void sql_serializer_plugin::initialize_varriables()
			{
					static const std::vector<TABLE> counters_to_initialize{{TABLE::OPERATIONS, TABLE::VIRTUAL_OPERATIONS}};

					pqxx::connection conn{my->connection_url.c_str()};
					auto get_single_number = [&](const std::string sql) -> uint32_t {
						pqxx::nontransaction w{conn};
						return w.exec(sql).at(0).at(0).as<uint32_t>();
					};
					try
					{
						if (get_single_number("SELECT COUNT(*) FROM " + my->flushes.at(TABLE::BLOCKS).table_name) == 0)
						{
							for (const TABLE key : counters_to_initialize)
								my->counters[key] = 0;
						}
						else
						{
							for (const TABLE key : counters_to_initialize)
								my->counters[key] = get_single_number("SELECT COALESCE(MAX( id ), 0) FROM " + my->flushes.at(key).table_name);
						}
					}
					catch (const pqxx::pqxx_exception &)
					{
						ilog("cought SQL exception");
						if (my->path_to_schema.valid())
						{
							ilog("recreating database on user request");
							my->recreate_db();
							for (const TABLE key : counters_to_initialize)
								my->counters[key] = 0;
						}
						else
						{
							throw;
						}
					}
			}

			} // namespace sql_serializer
		}		// namespace sql_serializer
	}			// namespace plugins