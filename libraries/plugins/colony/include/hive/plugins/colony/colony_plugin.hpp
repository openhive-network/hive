#pragma once
#include <hive/chain/hive_fwd.hpp>

#include <hive/plugins/chain/chain_plugin.hpp>

#define HIVE_COLONY_PLUGIN_NAME "colony"


namespace hive { namespace plugins { namespace colony {

namespace detail { class colony_plugin_impl; }

using namespace appbase;

class colony_plugin : public appbase::plugin< colony_plugin >
{
  public:
    colony_plugin();
    virtual ~colony_plugin();

    APPBASE_PLUGIN_REQUIRES( (hive::plugins::chain::chain_plugin) )

    static const std::string& name() { static std::string name = HIVE_COLONY_PLUGIN_NAME; return name; }

    virtual void set_program_options(
      options_description& cli,
      options_description& cfg ) override;
    virtual void plugin_initialize( const variables_map& options ) override;
    virtual void plugin_startup() override;
    virtual void plugin_pre_shutdown() override;
    virtual void plugin_shutdown() override;

    /// counters of the list of accepted transactions waiting for the colony broadcast thread
    struct broadcast_stats
    {
      uint64_t broadcast = 0; ///< handed to p2p
      uint64_t dropped = 0; ///< not broadcast because the list was full
      uint64_t abandoned = 0; ///< still waiting when broadcasting stopped
      size_t queued = 0; ///< waiting now
      size_t max_queued = 0; ///< longest the list was
    };
    broadcast_stats get_broadcast_stats() const;
    /// makes the broadcast thread wait @p pause after each p2p broadcast, as if p2p thread was that slow (for tests)
    void set_broadcast_pause( fc::microseconds pause );

  private:
    std::unique_ptr< detail::colony_plugin_impl > my;
};

} } } //hive::plugins::colony
