#pragma once
#include <chainbase/hive_fwd.hpp>

#include <hive/plugins/chain/chain_plugin.hpp>

#include <appbase/application.hpp>

#include <functional>
#include <memory>

namespace hive {

namespace plugins { namespace state_snapshot {

namespace bfs = boost::filesystem;

class state_snapshot_plugin final : public appbase::plugin< state_snapshot_plugin >
{
  public:
    APPBASE_PLUGIN_REQUIRES((hive::plugins::chain::chain_plugin))

    static const std::string& name() { static std::string name = "state_snapshot"; return name; }

    state_snapshot_plugin();
    virtual ~state_snapshot_plugin();

    virtual void set_program_options(
      boost::program_options::options_description& command_line_options,
      boost::program_options::options_description& config_file_options) override;

    virtual void plugin_initialize(const boost::program_options::variables_map& options) override;
    virtual void plugin_startup() override;
    virtual void plugin_shutdown() override;

  private:
    class impl;
    std::unique_ptr<impl> _my;

};
}
}
} /// hive::plugins::state_snapshot

