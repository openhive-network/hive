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

			using app::Inserters::Member;
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
					PSQL::queue<processing_object> sql_command_queue;

					PSQL::counter_container_t counters;
					operation_types_t names_to_flush;
					asset_container_t assets_to_flush;
					const std::map<TABLE, p_str_que_str> flushes{
							{table_flush_values_cell_t{TABLE::BLOCKS, p_str_que_str{"hive_blocks"}},
							 table_flush_values_cell_t{TABLE::TRANSACTIONS, p_str_que_str{"hive_transactions"}},

							 table_flush_values_cell_t{TABLE::ACCOUNTS, p_str_que_str{"hive_accounts"}},

							 table_flush_values_cell_t{TABLE::OPERATIONS, p_str_que_str{"hive_operations"}},
							 table_flush_values_cell_t{TABLE::VIRTUAL_OPERATIONS, p_str_que_str{"hive_virtual_operations"}},

							 table_flush_values_cell_t{TABLE::OPERATION_NAMES, p_str_que_str{"hive_operation_types"}},
							 table_flush_values_cell_t{TABLE::DETAILS_ASSET, p_str_que_str{"hive_operations_payment_details"}},
							 table_flush_values_cell_t{TABLE::DETAILS_VIRTUAL_ASSET, p_str_que_str{"hive_virtual_operations_payment_details"}},

							 table_flush_values_cell_t{TABLE::ASSET_DICT, p_str_que_str{"hive_asset_dictionary"}},
							 table_flush_values_cell_t{TABLE::PERMLINK_DICT, p_str_que_str{"hive_permlink_data"}},
							 table_flush_values_cell_t{TABLE::MEMBER_DICT, p_str_que_str{"hive_asset_members_dictionary"}}}};

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

				inline fc::string asset_num_to_string(uint32_t asset_num)
				{
					switch (asset_num)
					{
#ifdef IS_TEST_NET
					case HIVE_ASSET_NUM_HIVE:
						return "TESTS";
					case HIVE_ASSET_NUM_HBD:
						return "TBD";
#else
					case HIVE_ASSET_NUM_HIVE:
						return "HIVE";
					case HIVE_ASSET_NUM_HBD:
						return "HBD";
#endif
					case HIVE_ASSET_NUM_VESTS:
						return "VESTS";
					default:
						return "UNKN";
					}
				}

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
				{
					my->path_to_schema = options["psql-path-to-schema"].as<fc::string>();
					std::cout << "PATH: '" << *my->path_to_schema << "'" << std::endl;
				}
				my->set_index = options.count("psql-reindex-on-exit") > 0;
				initialize_varriables();

				// ids
				my->assets_to_flush[asset_symbol_type::from_asset_num(HIVE_ASSET_NUM_HIVE)] = 0;
				my->assets_to_flush[asset_symbol_type::from_asset_num(HIVE_ASSET_NUM_HBD)] = 1;
				my->assets_to_flush[asset_symbol_type::from_asset_num(HIVE_ASSET_NUM_VESTS)] = 2;

				// signals
				auto &db = appbase::app().get_plugin<hive::plugins::chain::chain_plugin>().db();
				my->_on_post_apply_operation_con = db.add_post_apply_operation_handler([&](const operation_notification &note) { on_post_apply_operation(note); }, *this);
				my->_on_post_apply_trx_con = db.add_post_apply_transaction_handler([&](const transaction_notification &note) { on_post_apply_trx(note); }, *this);
				my->_on_post_apply_block_con = db.add_post_apply_block_handler([&](const block_notification &note) { on_post_apply_block(note); }, *this);
				my->_on_finished_reindex = db.add_post_reindex_handler([&](const reindex_notification &note) {
					ilog("Flushing left data...");
					my->sql_command_queue.wait_push(processing_object{fc::string(), nullptr});
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

						// flush operation asset members
						for (uint8_t i = 0; i != static_cast<uint8_t>(Member::END); i++)
							my->flushes.at(TABLE::MEMBER_DICT).to_dump->push(generate([&](strstrm &ss) { ss << "( " << static_cast<uint32_t>(i) << ", '" << static_cast<Member>(i) << "' )"; }));

						// flush all data
						auto flusher = [&]() {
							FC_ASSERT(conn.is_open());

							// If there is no blocks to flush end
							if (my->flushes.find(TABLE::BLOCKS)->second.to_dump->empty())
								return;

							// flush op names
							for (const auto &kv : my->names_to_flush)
								my->flushes.at(TABLE::OPERATION_NAMES).to_dump->push("( " + std::to_string(kv.first) + " , '" + kv.second.first + "', " + (kv.second.second ? "TRUE" : "FALSE") + " )");

							// flush asset symobols
							for (const auto &kv : my->assets_to_flush)
								my->flushes.at(TABLE::ASSET_DICT).to_dump->push(generate([&](strstrm &ss) { ss << "( " << kv.second << ", '" << kv.first.to_nai_string() << "', " << static_cast<uint32_t>(kv.first.decimals()) << ", '" << detail::asset_num_to_string(kv.first.asset_num) << "')"; }));

							pqxx::nontransaction trx{conn};
							trx.exec("BEGIN;");

							// Update table by table
							for (const auto &kv : my->flushes)
							{
								std::stringstream ss;
								std::string str;

								auto &it = kv.second;
								if (it.to_dump->empty())
									continue;
								it.to_dump->pull(str);
								ss << "INSERT INTO " << it.table_name << " VALUES " << str;
								while (!it.to_dump->empty())
								{
									it.to_dump->pull(str);
									ss << "," << str;
								}

								// Theese tables contains constant data, that is unique on hived site
								if (kv.first == TABLE::OPERATION_NAMES || kv.first == TABLE::ASSET_DICT || kv.first == TABLE::MEMBER_DICT)
									ss << " ON CONFLICT DO NOTHING";

								ss << ";";
								trx.exec(ss.str());
							}
							trx.exec("COMMIT;");
						};

						// worker
						processing_object input;
						my->sql_command_queue.wait_pull(input);

						// work loop
						while (input.valid())
						{
							const sql_command_t vec = PSQL::sql_serializer_processor{input, my->names_to_flush, my->counters, my->assets_to_flush, [&](const char *str_to_esc) { return conn.esc(str_to_esc); }}();

							// flush
							for (const auto &v : vec)
							{
								// if (v.first == TABLE::BLOCKS)
								my->flushes.at(v.first).to_dump->push(v.second);
								if (v.first == TABLE::BLOCKS && my->flushes.at(v.first).to_dump->size() == 1'000)
									flusher();
							}

							// gather next element
							my->sql_command_queue.wait_pull(input);
						}

						// flush what hasn't been already flushed
						flusher();
					});
				},
																															*this);
			}

			void sql_serializer_plugin::plugin_startup() {}

			void sql_serializer_plugin::plugin_shutdown() {}

			void sql_serializer_plugin::on_post_apply_operation(const operation_notification &note)
			{
				if (note.block < 2)
					return;

				processing_object obj{
						"",
						std::make_shared<operation>(note.op),
						note.block,
						note.trx_in_block,
						note.op_in_trx,
						note.virtual_op};
				my->sql_command_queue.wait_push(obj);
			}

			void sql_serializer_plugin::on_post_apply_block(const block_notification &note)
			{
				for (int64_t i = 0; i < static_cast<int64_t>(note.block.transactions.size()); i++)
					handle_transaction(note.block.transactions[i].id().str(), note.block_num, i);

				processing_object obj{
						note.block_id.str(),
						nullptr,
						note.block_num};
				my->sql_command_queue.wait_push(obj);
			}

			void sql_serializer_plugin::on_post_apply_trx(const transaction_notification &) {}

			void sql_serializer_plugin::handle_transaction(const fc::string &hash, const int64_t block_num, const int64_t trx_in_block)
			{
				processing_object obj{
						hash,
						nullptr,
						block_num,
						trx_in_block};
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
				catch (const pqxx::pqxx_exception&)
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
	}		// namespace plugins
} // namespace hive