#pragma once
#include <appbase/application.hpp>
#include <hive/plugins/clive/wallet_manager.hpp>

#define HIVE_CLIVE_PLUGIN_NAME "clive"

namespace hive { namespace plugins { namespace clive {

class clive_plugin : public appbase::plugin<clive_plugin> {
public:
   APPBASE_PLUGIN_REQUIRES()

   clive_plugin();
   clive_plugin(const clive_plugin&) = delete;
   clive_plugin(clive_plugin&&) = delete;
   clive_plugin& operator=(const clive_plugin&) = delete;
   clive_plugin& operator=(clive_plugin&&) = delete;
   virtual ~clive_plugin() override = default;

  static const std::string& name() { static std::string name = HIVE_CLIVE_PLUGIN_NAME; return name; }

   virtual void set_program_options(boost::program_options::options_description& cli, boost::program_options::options_description& cfg) override;
   void plugin_initialize(const boost::program_options::variables_map& options);
   void plugin_startup() {}
   void plugin_shutdown() {}

   // api interface provider
   wallet_manager& get_wallet_manager();

private:
   std::unique_ptr<wallet_manager> wallet_manager_ptr;
};

}}}