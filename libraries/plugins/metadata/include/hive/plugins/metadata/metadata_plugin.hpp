#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <hive/plugins/chain/chain_plugin.hpp>
#include <hive/plugins/metadata/metadata_objects.hpp>

#include <hive/chain/hive_object_types.hpp>

#ifndef HIVE_METADATA_SPACE_ID
#define HIVE_METADATA_SPACE_ID 22
#endif

#ifndef HIVE_METADATA_PLUGIN_NAME
#define HIVE_METADATA_PLUGIN_NAME "metadata"
#endif


namespace hive { namespace plugins { namespace metadata {

using namespace hive::chain;
using namespace appbase;


namespace detail { class metadata_plugin_impl; }

class metadata_plugin : public plugin< metadata_plugin >
{
  public:
    metadata_plugin();
    virtual ~metadata_plugin();

    APPBASE_PLUGIN_REQUIRES( (hive::plugins::chain::chain_plugin) )

    static const std::string& name() { static std::string name = HIVE_METADATA_PLUGIN_NAME; return name; }

    virtual void set_program_options(
      options_description& cli,
      options_description& cfg ) override;

  protected:
    virtual void plugin_initialize( const variables_map& options ) override;
    virtual void plugin_startup() override;
    virtual void plugin_shutdown() override;

  private:
    friend class detail::metadata_plugin_impl;
    std::unique_ptr< detail::metadata_plugin_impl > my;
};

}}}
