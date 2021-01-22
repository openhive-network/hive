#pragma once
#include <appbase/plugin.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/core/demangle.hpp>
#include <boost/asio.hpp>
#include <boost/throw_exception.hpp>

#include <iostream>
#include <atomic>

#define APPBASE_VERSION_STRING ("appbase 1.0")

namespace appbase {

  namespace bpo = boost::program_options;
  namespace bfs = boost::filesystem;

  class io_handler
  {
    public:

      using p_io_handler = std::shared_ptr< io_handler >;
      using p_signal_set = std::shared_ptr< boost::asio::signal_set >;
      using final_action_type = std::function< void() >;

    private:

      std::atomic_flag        lock = ATOMIC_FLAG_INIT;

      bool                    closed = false;
      bool                    allow_close_when_signal_is_received = false;
      uint32_t                last_signal_code = 0;

      final_action_type       final_action;

      p_signal_set            signals;

      boost::asio::io_service io_serv;

      void close_signal();

      void handle_signal( uint32_t _last_signal_code );

    public:

      io_handler( bool _allow_close_when_signal_is_received, final_action_type&& _final_action );

      boost::asio::io_service& get_io_service();

      void close();

      void attach_signals();

      void run();

      void set_interrupt_request( uint32_t _last_signal_code );
      bool is_interrupt_request() const;
  };

  class application
  {
    public:
      ~application();

      /**
        * @brief Looks for the --plugin commandline / config option and calls initialize on those plugins
        *
        * @tparam Plugin List of plugins to initalize even if not mentioned by configuration. For plugins started by
        * configuration settings or dependency resolution, this template has no effect.
        * @return true if the application and plugins were initialized, false or exception on error
        */
      template< typename... Plugin >
      bool initialize( int argc, char** argv )
      {
        return initialize_impl( argc, argv, { find_plugin( Plugin::name() )... } );
      }

      void startup();
      void shutdown();

      /**
        *  Wait until quit(), SIGINT or SIGTERM and then shutdown
        */
      void exec();

      static application& instance( bool reset = false );

      template< typename Plugin >
      auto& register_plugin()
      {
        auto existing = find_plugin( Plugin::name() );
        if( existing )
          return *dynamic_cast< Plugin* >( existing );

        auto plug = std::make_shared< Plugin >();
        plugins[Plugin::name()] = plug;
        plug->register_dependencies();
        return *plug;
      }

      template< typename Plugin >
      Plugin* find_plugin()const
      {
        Plugin* plugin = dynamic_cast< Plugin* >( find_plugin( Plugin::name() ) );

        // Do not return plugins that are registered but not at least initialized.
        if( plugin != nullptr && plugin->get_state() == abstract_plugin::registered )
        {
          return nullptr;
        }

        return plugin;
      }

      template< typename Plugin >
      Plugin& get_plugin()const
      {
        auto ptr = find_plugin< Plugin >();
        if( ptr == nullptr )
          BOOST_THROW_EXCEPTION( std::runtime_error( "unable to find plugin: " + Plugin::name() ) );
        return *ptr;
      }

      bfs::path data_dir()const;

      void add_program_options( const bpo::options_description& cli, const bpo::options_description& cfg );
      const bpo::variables_map& get_args() const;

      void set_version_string( const string& version ) { version_info = version; }
      const std::string& get_version_string() const { return version_info; }
      void set_app_name( const string& name ) { app_name = name; }

      template< typename... Plugin >
      void set_default_plugins() { default_plugins = { Plugin::name()... }; }

      boost::asio::io_service& get_io_service() { return main_io_handler.get_io_service(); }

      void generate_interrupt_request()
      {
        if( startup_io_handler )
          startup_io_handler->set_interrupt_request( SIGINT );
      }

      bool is_interrupt_request() const
      {
        return startup_io_handler ? startup_io_handler->is_interrupt_request() : _is_interrupt_request;
      }

      std::set< std::string > get_plugins_names() const;

    protected:
      template< typename Impl >
      friend class plugin;

      bool initialize_impl( int argc, char** argv, vector< abstract_plugin* > autostart_plugins );

      abstract_plugin* find_plugin( const string& name )const;
      abstract_plugin& get_plugin( const string& name )const;

      /** these notifications get called from the plugin when their state changes so that
        * the application can call shutdown in the reverse order.
        */
      ///@{
      void plugin_initialized( abstract_plugin& plug ) { initialized_plugins.push_back( &plug ); }
      void plugin_started( abstract_plugin& plug ) { running_plugins.push_back( &plug ); }
      ///@}

    private:
      application(); ///< private because application is a singlton that should be accessed via instance()
      map< string, std::shared_ptr< abstract_plugin > >  plugins; ///< all registered plugins
      vector< abstract_plugin* >                         initialized_plugins; ///< stored in the order they were started running
      vector< abstract_plugin* >                         running_plugins; ///< stored in the order they were started running
      std::string                                        version_info;
      std::string                                        app_name = "appbase";
      std::vector< std::string >                         default_plugins;

      void set_program_options();
      void write_default_config( const bfs::path& cfg_file );
      std::unique_ptr< class application_impl > my;

      io_handler                 main_io_handler;

      //This handler is designed only for startup purposes
      io_handler::p_io_handler   startup_io_handler;

      bool _is_interrupt_request = false;
  };

  application& app();
  application& reset();

  template< typename Impl >
  class plugin : public abstract_plugin
  {
    public:
      virtual ~plugin() {}

      virtual state get_state() const override { return _state; }
      virtual const std::string& get_name()const override final { return Impl::name(); }

      virtual void register_dependencies()
      {
        this->plugin_for_each_dependency( [&]( abstract_plugin& plug ){} );
      }

      virtual void initialize(const variables_map& options) override final
      {
        if( _state == registered )
        {
          _state = initialized;
          this->plugin_for_each_dependency( [&]( abstract_plugin& plug ){ plug.initialize( options ); } );
          this->plugin_initialize( options );
          // std::cout << "Initializing plugin " << Impl::name() << std::endl;
          app().plugin_initialized( *this );
        }
        if (_state != initialized)
          BOOST_THROW_EXCEPTION( std::runtime_error("Initial state was not registered, so final state cannot be initialized.") );
      }

      virtual void startup() override final
      {
        if( _state == initialized )
        {
          _state = started;
          this->plugin_for_each_dependency( [&]( abstract_plugin& plug ){ plug.startup(); } );
          this->plugin_startup();
          app().plugin_started( *this );
        }
        if (_state != started )
          BOOST_THROW_EXCEPTION( std::runtime_error("Initial state was not initialized, so final state cannot be started.") );
      }

      virtual void shutdown() override final
      {
        if( _state == started )
        {
          _state = stopped;
          //ilog( "shutting down plugin ${name}", ("name",name()) );
          this->plugin_shutdown();
        }
      }

    protected:
      plugin() = default;

    private:
      state _state = abstract_plugin::registered;
  };
}
