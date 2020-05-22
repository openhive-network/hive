
#pragma once

#include <hive/app/plugin.hpp>
#include <hive/plugins/block_info/block_info.hpp>

#include <string>
#include <vector>

namespace hive { namespace protocol {
struct signed_block;
} }

namespace hive { namespace plugin { namespace block_info {

using hive::app::application;

class block_info_plugin : public hive::app::plugin
{
   public:
      block_info_plugin( application* app );
      virtual ~block_info_plugin();

      virtual std::string plugin_name()const override;
      virtual void plugin_initialize( const boost::program_options::variables_map& options ) override;
      virtual void plugin_startup() override;
      virtual void plugin_shutdown() override;

      void on_applied_block( const chain::signed_block& b );

      std::vector< block_info > _block_info;

      boost::signals2::scoped_connection _applied_block_conn;
};

} } }
