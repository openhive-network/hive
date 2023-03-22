#pragma once
#include <appbase/application.hpp>
#include <hive/plugins/wallet/wallet_manager.hpp>

#define HIVE_WALLET_PLUGIN_NAME "wallet"

namespace hive { namespace plugins { namespace wallet {

class wallet_plugin : public appbase::plugin<wallet_plugin> {
public:
   APPBASE_PLUGIN_REQUIRES()

   wallet_plugin();
   wallet_plugin(const wallet_plugin&) = delete;
   wallet_plugin(wallet_plugin&&) = delete;
   wallet_plugin& operator=(const wallet_plugin&) = delete;
   wallet_plugin& operator=(wallet_plugin&&) = delete;
   virtual ~wallet_plugin() override = default;

  static const std::string& name() { static std::string name = HIVE_WALLET_PLUGIN_NAME; return name; }

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