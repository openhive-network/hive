#pragma once
#include <chainbase/hive_fwd.hpp>
#include <hive/plugins/chain/chain_plugin.hpp>

#define HIVE_SQL_SERIALIZER_NAME "sql_serializer"

namespace hive { namespace plugins { namespace sql_serializer {

namespace detail { class sql_serializer_plugin_impl; }

constexpr size_t prereservation_size = 16'000u;

class sql_serializer_plugin final : public appbase::plugin<sql_serializer_plugin>
{
  public:
      sql_serializer_plugin();
      virtual ~sql_serializer_plugin();

      APPBASE_PLUGIN_REQUIRES((hive::plugins::chain::chain_plugin))

      static const std::string& name() 
      { 
        static std::string name = HIVE_SQL_SERIALIZER_NAME; 
        return name; 
      }

      virtual void set_program_options(appbase::options_description& cli, appbase::options_description& cfg ) override;
      virtual void plugin_initialize(const appbase::variables_map& options) override;
      virtual void plugin_startup() override;
      virtual void plugin_shutdown() override;

  private:
     std::unique_ptr<detail::sql_serializer_plugin_impl> my;
};

} } } //hive::plugins::sql_serializer

