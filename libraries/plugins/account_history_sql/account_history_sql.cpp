#include <hive/plugins/account_history_sql/account_history_sql_plugin.hpp>

#include <hive/utilities/postgres_connection_holder.hpp>

//#include <hive/chain/util/impacted.hpp>
//#include <hive/protocol/config.hpp>

#include <fc/log/logger.hpp>
// C++ connector library for PostgreSQL (http://pqxx.org/development/libpqxx/)
#include <pqxx/pqxx>

namespace hive
{
 namespace plugins
 {
  namespace account_history_sql
  {
    using chain::database;
    using hive::utilities::postgres_connection_holder;

    namespace detail
    {
      class account_history_sql_plugin_impl
      {
        public:
          account_history_sql_plugin_impl(const std::string &url) : _db(appbase::app().get_plugin<hive::plugins::chain::chain_plugin>().db()), connection{url}
          {}

          virtual ~account_history_sql_plugin_impl()
          {
            ilog("finishing account_history_sql_plugin_impl destructor...");
          }

          void get_ops_in_block( account_history_sql::account_history_sql_plugin::sql_result_type sql_result,
                                uint32_t block_num, bool only_virtual, const fc::optional<bool>& include_reversible );

          void get_transaction( account_history_sql::account_history_sql_plugin::sql_result_type sql_result,
                                const hive::protocol::transaction_id_type& id, const fc::optional<bool>& include_reversible );

          void get_account_history( account_history_sql::account_history_sql_plugin::sql_sequence_result_type sql_sequence_result,
                                    const hive::protocol::account_name_type& account, uint64_t start, uint32_t limit,
                                    const fc::optional<bool>& include_reversible,
                                    const fc::optional<uint64_t>& operation_filter_low,
                                    const fc::optional<uint64_t>& operation_filter_high );

          void enum_virtual_ops( account_history_sql::account_history_sql_plugin::sql_result_type sql_result,
                                uint32_t block_range_begin, uint32_t block_range_end,
                                const fc::optional<bool>& include_reversible, const fc::optional<bool>& group_by_block,
                                const fc::optional< uint64_t >& operation_begin, const fc::optional< uint32_t >& limit,
                                const fc::optional< uint32_t >& filter );

        private:

          database &_db;

          postgres_connection_holder connection;

      };//class account_history_sql_plugin_impl

      void account_history_sql_plugin_impl::get_ops_in_block( account_history_sql::account_history_sql_plugin::sql_result_type sql_result,
                                                        uint32_t block_num, bool only_virtual, const fc::optional<bool>& include_reversible )
      {

      }

      void account_history_sql_plugin_impl::get_transaction( account_history_sql::account_history_sql_plugin::sql_result_type sql_result,
                                                        const hive::protocol::transaction_id_type& id, const fc::optional<bool>& include_reversible )
      {
        
      }

      void account_history_sql_plugin_impl::get_account_history( account_history_sql::account_history_sql_plugin::sql_sequence_result_type sql_sequence_result,
                                                            const hive::protocol::account_name_type& account, uint64_t start, uint32_t limit,
                                                            const fc::optional<bool>& include_reversible,
                                                            const fc::optional<uint64_t>& operation_filter_low,
                                                            const fc::optional<uint64_t>& operation_filter_high )
      {
        
      }

      void account_history_sql_plugin_impl::enum_virtual_ops( account_history_sql::account_history_sql_plugin::sql_result_type sql_result,
                                                        uint32_t block_range_begin, uint32_t block_range_end,
                                                        const fc::optional<bool>& include_reversible, const fc::optional<bool>& group_by_block,
                                                        const fc::optional< uint64_t >& operation_begin, const fc::optional< uint32_t >& limit,
                                                        const fc::optional< uint32_t >& filter )
      {
        
      }

    }//namespace detail
  

    account_history_sql_plugin::account_history_sql_plugin() {}
    account_history_sql_plugin::~account_history_sql_plugin() {}

    void account_history_sql_plugin::set_program_options(options_description &cli, options_description &cfg)
    {
      //ahsql-url = dbname=ah_db_name user=postgres password=pass hostaddr=127.0.0.1 port=5432
      cfg.add_options()("ahsql-url", boost::program_options::value<string>(), "postgres connection string for AH database");
    }

    void account_history_sql_plugin::plugin_initialize(const boost::program_options::variables_map &options)
    {
      ilog("Initializing SQL account history plugin");
      FC_ASSERT(options.count("ahsql-url"), "`ahsql-url` is required");
      my = std::make_unique<detail::account_history_sql_plugin_impl>(options["ahsql-url"].as<fc::string>());
    }

    void account_history_sql_plugin::plugin_startup()
    {
      ilog("sql::plugin_startup()");
    }

    void account_history_sql_plugin::plugin_shutdown()
    {
      ilog("sql::plugin_shutdown()");
    }

    void account_history_sql_plugin::get_ops_in_block( sql_result_type sql_result,
                                                      uint32_t block_num, bool only_virtual, const fc::optional<bool>& include_reversible ) const
    {
      my->get_ops_in_block( sql_result, block_num, only_virtual, include_reversible );
    }

    void account_history_sql_plugin::get_transaction( sql_result_type sql_result,
                                                      const hive::protocol::transaction_id_type& id, const fc::optional<bool>& include_reversible ) const
    {
      my->get_transaction( sql_result, id, include_reversible );
    }

    void account_history_sql_plugin::get_account_history( sql_sequence_result_type sql_sequence_result,
                                                          const hive::protocol::account_name_type& account, uint64_t start, uint32_t limit,
                                                          const fc::optional<bool>& include_reversible,
                                                          const fc::optional<uint64_t>& operation_filter_low,
                                                          const fc::optional<uint64_t>& operation_filter_high ) const
    {
      my->get_account_history( sql_sequence_result, account, start, limit, include_reversible, operation_filter_low, operation_filter_high );
    }

    void account_history_sql_plugin::enum_virtual_ops( sql_result_type sql_result,
                                                      uint32_t block_range_begin, uint32_t block_range_end,
                                                      const fc::optional<bool>& include_reversible, const fc::optional<bool>& group_by_block,
                                                      const fc::optional< uint64_t >& operation_begin, const fc::optional< uint32_t >& limit,
                                                      const fc::optional< uint32_t >& filter ) const
    {
      my->enum_virtual_ops( sql_result, block_range_begin, block_range_end, include_reversible, group_by_block,
                            operation_begin, limit, filter );
    }

  } // namespace account_history_sql
 }  // namespace plugins
} // namespace hive
