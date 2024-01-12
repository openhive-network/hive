#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <hive/plugins/chain/chain_plugin.hpp>
#include <hive/plugins/p2p/p2p_plugin.hpp>

#include <appbase/application.hpp>

#include <hive/protocol/testnet_blockchain_configuration.hpp>

#define HIVE_PACEMAKER_PLUGIN_NAME "pacemaker"

namespace hive { namespace plugins { namespace pacemaker {

namespace detail { class pacemaker_plugin_impl; }

enum class block_emission_condition
{
  normal = 0,
  lag = 1,
  too_early = 2,
  too_late = 3,
  no_more_blocks = 4,
  exception_emitting_block = 5
};

class pacemaker_plugin : public appbase::plugin< pacemaker_plugin >
{
public:
  APPBASE_PLUGIN_REQUIRES(
    (hive::plugins::chain::chain_plugin)
    (hive::plugins::p2p::p2p_plugin)
  )

  pacemaker_plugin();
  virtual ~pacemaker_plugin();

  static const std::string& name() { static std::string name = HIVE_PACEMAKER_PLUGIN_NAME; return name; }

  virtual void set_program_options(
    boost::program_options::options_description &command_line_options,
    boost::program_options::options_description &config_file_options
    ) override;

  virtual void plugin_initialize(const boost::program_options::variables_map& options) override;
  virtual void plugin_startup() override;
  virtual void plugin_shutdown() override;

private:
  std::unique_ptr< detail::pacemaker_plugin_impl > my;
};

} } } // hive::plugins::pacemaker
