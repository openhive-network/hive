#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <hive/plugins/chain/chain_plugin.hpp>

#include <appbase/application.hpp>

#define HIVE_SQL_SOUCE_NAME "account_history_sql"

namespace hive { namespace plugins { namespace account_history_sql {

namespace detail { class account_history_sql_plugin_impl; }

using boost::program_options::options_description;
using boost::program_options::variables_map;

struct account_history_sql_object
{
  hive::protocol::transaction_id_type trx_id;
  uint32_t                            block = 0;
  uint32_t                            trx_in_block = 0;
  uint32_t                            op_in_trx = 0;
  uint32_t                            virtual_op = 0;
  uint64_t                            operation_id = 0;
  fc::time_point_sec                  timestamp;
  hive::protocol::operation           op;
};

class account_history_sql_plugin : public appbase::plugin<account_history_sql_plugin>
{
  public:

    using sql_result_type = std::function< void( const account_history_sql::account_history_sql_object& ) >;
    using sql_sequence_result_type = std::function< void( unsigned int sequence, const account_history_sql::account_history_sql_object& ) >;

  public:
      account_history_sql_plugin();
      virtual ~account_history_sql_plugin();

      APPBASE_PLUGIN_REQUIRES((hive::plugins::chain::chain_plugin))

      static const std::string& name() 
      { 
        static std::string name = HIVE_SQL_SOUCE_NAME;
        return name; 
      }

      virtual void set_program_options(options_description& cli, options_description& cfg ) override;
      virtual void plugin_initialize(const variables_map& options) override;
      virtual void plugin_startup() override;
      virtual void plugin_shutdown() override;

      void get_ops_in_block( sql_result_type sql_result,
                            uint32_t block_num, bool only_virtual, const fc::optional<bool>& include_reversible ) const;

      void get_transaction( hive::protocol::annotated_signed_transaction& sql_result,
                            const hive::protocol::transaction_id_type& id, const fc::optional<bool>& include_reversible ) const;

      void get_account_history( sql_sequence_result_type sql_sequence_result,
                                const hive::protocol::account_name_type& account, uint64_t start, uint32_t limit,
                                const fc::optional<bool>& include_reversible,
                                const fc::optional<uint64_t>& operation_filter_low,
                                const fc::optional<uint64_t>& operation_filter_high ) const;

      void enum_virtual_ops( sql_result_type sql_result,
                            uint32_t block_range_begin, uint32_t block_range_end,
                            const fc::optional<bool>& include_reversible,
                            const fc::optional< uint64_t >& operation_begin, const fc::optional< uint32_t >& limit,
                            const fc::optional< uint32_t >& filter ) const;

  private:

      std::unique_ptr<detail::account_history_sql_plugin_impl> my;

      void handle_transaction(const hive::protocol::transaction_id_type &hash, const int64_t block_num, const int64_t trx_in_block);
};

} } } //hive::plugins::account_history_sql
