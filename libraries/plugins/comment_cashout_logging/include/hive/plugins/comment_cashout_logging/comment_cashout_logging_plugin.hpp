#pragma once
#include <chainbase/forward_declarations.hpp>
#include <hive/plugins/chain/chain_plugin.hpp>

#define HIVE_COMMENT_CASHOUT_PLUGIN_NAME "comment_cashout_logging"

namespace hive { namespace plugins { namespace comment_cashout_logging {

namespace detail { class comment_cashout_logging_plugin_impl; }

using namespace appbase;

#ifndef HIVE_COMMENT_CASHOUT_SPACE_ID
#define HIVE_COMMENT_CASHOUT_SPACE_ID 666
#endif

class comment_cashout_logging_plugin : public plugin<comment_cashout_logging_plugin>
{
   public:
      comment_cashout_logging_plugin();
      virtual ~comment_cashout_logging_plugin();

      APPBASE_PLUGIN_REQUIRES((hive::plugins::chain::chain_plugin))

      static const std::string& name() 
      { 
        static std::string name = HIVE_COMMENT_CASHOUT_PLUGIN_NAME; 
        return name; 
      }

      virtual void set_program_options(options_description& cli, options_description& cfg ) override;
      virtual void plugin_initialize(const variables_map& options) override;
      virtual void plugin_startup() override;
      virtual void plugin_shutdown() override;

   private:
      std::unique_ptr<detail::comment_cashout_logging_plugin_impl> my;
};

} } } //hive::plugins::comment_cashout_logging

