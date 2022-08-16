#pragma once
#include <boost/program_options.hpp>
#include <boost/preprocessor/seq/for_each.hpp>

#include <functional>
#include <string>
#include <vector>
#include <map>

#define APPBASE_PLUGIN_REQUIRES_VISIT( r, visitor, elem ) \
  visitor( appbase::app().register_plugin<elem>() );

#define APPBASE_PLUGIN_REQUIRES( PLUGINS )                               \
  virtual void plugin_for_each_dependency( plugin_processor&& l ) override {  \
    BOOST_PP_SEQ_FOR_EACH( APPBASE_PLUGIN_REQUIRES_VISIT, l, PLUGINS ) \
  }

namespace appbase {

  using boost::program_options::options_description;
  using boost::program_options::variables_map;
  using std::string;
  using std::vector;
  using std::map;

  class application;
  application& app();

  class abstract_plugin {
    public:
      enum state {
        registered, ///< the plugin is constructed but doesn't do anything
        initialized, ///< the plugin has initlaized any state required but is idle
        started, ///< the plugin is actively running
        stopped ///< the plugin is no longer running
      };

      enum pre_shutdown_order {
        basic_order = 0,    ///< most plugins don't need to be prepared before another plugins, therefore it doesn't matter when they will be closed
        p2p_order   = 1,    ///< p2p plugin has to reject/break all connections at the start
        webserver_order = 2 ///< shutdown webserver and make sure all API calls are finished before stopping other plugins
                            // (needs to be done before p2p is shut down in cases like when network_broadcast_api is enabled, because
                            //  those API calls call into the p2p plugin to do their broadcasting)
      };

      virtual ~abstract_plugin(){}

      virtual pre_shutdown_order get_pre_shutdown_order()const = 0;
      virtual state get_state()const = 0;
      virtual const std::string& get_name()const  = 0;
      virtual void set_program_options( options_description& cli, options_description& cfg ) = 0;
      virtual void initialize(const variables_map& options) = 0;
      virtual void startup() = 0;
      virtual void finalize_startup() = 0;
      virtual void pre_shutdown() = 0;
      virtual void shutdown() = 0;
    protected:
      typedef std::function<void(abstract_plugin&)> plugin_processor;

      /** Abstract method to be reimplemented in final plugin implementation.
          It is a part of initialization/startup process triggerred by main application.
          Allows to process all plugins, this one depends on.
      */
      virtual void plugin_for_each_dependency(plugin_processor&& processor) = 0;

      /** Abstract method to be reimplemented in final plugin implementation.
          It is a part of initialization process triggerred by main application.
      */
      virtual void plugin_initialize( const variables_map& options ) = 0;

      /** Abstract method to be reimplemented in final plugin implementation.
          It is a part of startup process triggerred by main application.
      */
      virtual void plugin_startup() = 0;

      /** Abstract method to be reimplemented in final plugin implementation.
          It is a part of final stage of startup process triggerred by main application.
      */
      virtual void plugin_finalize_startup() = 0;

      /** Abstract method to be reimplemented in final plugin implementation.
          It is a part of shutdown process triggerred by main application.
      */
      virtual void plugin_pre_shutdown() = 0;

      /** Abstract method to be reimplemented in final plugin implementation.
          It is a part of shutdown process triggerred by main application.
      */
      virtual void plugin_shutdown() = 0;

      virtual void set_pre_shutdown_order( pre_shutdown_order val ) = 0;
  };

  template<typename Impl>
  class plugin;
}
