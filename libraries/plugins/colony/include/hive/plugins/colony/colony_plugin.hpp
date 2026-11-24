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

    struct broadcast_stats
    {
      uint64_t broadcast = 0; // transactions handed to p2p
      uint64_t dropped = 0; // transactions not broadcast because broadcast list was full
      uint32_t waiting = 0; // current size of broadcast list
      uint32_t max_waiting = 0; // highest size of broadcast list so far
      uint32_t limit = 0; // colony-broadcast-queue-limit
    };
    broadcast_stats get_broadcast_stats() const;

    // for tests: each transaction waits given time before it goes to p2p, as if p2p thread was that slow
    void delay_broadcast( fc::microseconds delay );

  private:
    std::unique_ptr< detail::colony_plugin_impl > my;
};

} } } //hive::plugins::colony
