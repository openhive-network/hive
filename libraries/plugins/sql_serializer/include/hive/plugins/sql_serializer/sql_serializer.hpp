#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <hive/plugins/chain/chain_plugin.hpp>
#include <hive/plugins/sql_serializer/sql_serializer_objects.hpp>

#define HIVE_SQL_SERIALIZER_NAME "sql_serializer"

namespace hive { namespace plugins { namespace sql_serializer {

namespace detail { class sql_serializer_impl; }

using namespace appbase;

class sql_serializer_plugin : public plugin<sql_serializer_plugin>
{
  public:
      sql_serializer();
      virtual ~sql_serializer();

      APPBASE_PLUGIN_REQUIRES((hive::plugins::chain::chain_plugin))

      static const std::string& name() 
      { 
        static std::string name = HIVE_SQL_SERIALIZER_NAME; 
        return name; 
      }

      boost::signals2::connection      _on_post_apply_operation_con;

      virtual void push( const  )

      virtual void set_program_options(options_description& cli, options_description& cfg ) override;
      virtual void plugin_initialize(const variables_map& options) override;
      virtual void plugin_startup() override;
      virtual void plugin_shutdown() override;

  private:
      std::unique_ptr<detail::sql_serializer_plugin_impl> my;
};

} } } //hive::plugins::sql_serializer

