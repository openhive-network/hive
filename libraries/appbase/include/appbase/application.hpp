#pragma once
#include <appbase/plugin.hpp>
#include <appbase/signals_handler.hpp>

#include <fc/io/json.hpp>

#include <boost/filesystem/path.hpp>
#include <boost/core/demangle.hpp>
#include <boost/asio.hpp>
#include <hive/utilities/notifications.hpp>
#include <boost/throw_exception.hpp>

#include <iostream>
#include <atomic>

#define APPBASE_VERSION_STRING ("appbase 1.0")

namespace fc {
  struct logging_config;
}
namespace appbase {

  namespace bpo = boost::program_options;
  namespace bfs = boost::filesystem;

  class application;

  class initialization_result 
  {
    public:
      enum result {
        ok,
        unrecognised_option,
        error_with_option,
        validation_error,
        unknown_command_line_error
      };

      initialization_result(result result, bool start_loop)
      : init_result(result)
      , start_loop_flag(start_loop)
      {
      }

      int get_result_code() const
      {
        return init_result;
      }

      bool should_start_loop() const
      {
        return start_loop_flag;
      }

    private:
      result init_result;
      bool start_loop_flag;
  };

  class application;

  application& app();
  /// Allows to reset (destroy) application object.
  void reset();

  class application final
  {
    public:

      application();
      ~application();

      application(const application&) = delete;
      application& operator=(const application&) = delete;
      application(application&&) = delete;
      application& operator=(application&&) = delete;

      void init_signals_handler();

      /**
        * @brief Looks for the --plugin commandline / config option and calls initialize on those plugins
        *
        * @tparam Plugin List of plugins to initalize even if not mentioned by configuration. For plugins started by
        * configuration settings or dependency resolution, this template has no effect.
        * @return true if the application and plugins were initialized, false or exception on error
        */
      template< typename... Plugin >
      initialization_result initialize( int argc, char** argv, 
        const bpo::variables_map& arg_overrides = bpo::variables_map() )
      {
        return initialize_impl( argc, argv, { find_plugin( Plugin::name() )... }, arg_overrides );
      }

      fc::optional< fc::logging_config > load_logging_config();

      void startup();

      void pre_shutdown( std::string& actual_plugin_name );
      void shutdown( std::string& actual_plugin_name );

      void finish();

      /**
        *  Wait until quit(), SIGINT or SIGTERM and then shutdown
        */
      void wait4interrupt_request();
      void wait( bool log = false );
      bool is_thread_closed();

      template< typename Plugin >
      auto& register_plugin()
      {
        auto existing = find_plugin( Plugin::name() );
        if( existing )
          return *dynamic_cast< Plugin* >( existing );

        auto plug = std::make_shared< Plugin >();
        plug->attach_application( this );
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
      void add_logging_program_options();
      const bpo::variables_map& get_args() const;

      void set_plugin_options( bpo::options_description* app_options,
                               bpo::options_description* cfg_options ) const;

      void set_version_string( const string& version ) { version_info = version; }
      const std::string& get_version_string() const { return version_info; }
      void set_app_name( const string& name ) { app_name = name; }

      template< typename... Plugin >
      void set_default_plugins() { default_plugins = { Plugin::name()... }; }

      boost::asio::io_service& get_io_service() { return handler_wrapper.get_io_service(); }

      void generate_interrupt_request();

      bool is_interrupt_request() const { return _is_interrupt_request.load(std::memory_order_relaxed); }

      std::set< std::string > get_plugins_names() const;

      void kill();
      bool quit( bool log = false );

      using finish_request_type = std::function<void()>;

    protected:
      template< typename Impl >
      friend class plugin;

      initialization_result initialize_impl( int argc, char** argv, 
        vector< abstract_plugin* > autostart_plugins, const bpo::variables_map& arg_overrides );

      abstract_plugin* find_plugin( const string& name )const;
      abstract_plugin& get_plugin( const string& name )const;

      /** these notifications get called from the plugin when their state changes so that
        * the application can call shutdown in the reverse order.
        */
      ///@{
      void plugin_initialized( abstract_plugin& plug ) { initialized_plugins.push_back( &plug ); }
      void plugin_started( abstract_plugin& plug )
      {
        running_plugins.push_back( &plug );
        pre_shutdown_plugins.insert( &plug );
      }
      ///@}

    private:

      map< string, std::shared_ptr< abstract_plugin > >  plugins; ///< all registered plugins
      vector< abstract_plugin* >                         initialized_plugins; ///< stored in the order they were started running

      using pre_shutdown_cmp = std::function< bool ( abstract_plugin*, abstract_plugin* ) >;
      using pre_shutdown_multiset = std::multiset< abstract_plugin*, pre_shutdown_cmp >;
      pre_shutdown_multiset                              pre_shutdown_plugins; ///< stored in the order what is necessary in order to close every plugin in safe way
      vector< abstract_plugin* >                         running_plugins; ///< stored in the order they were started running
      std::string                                        version_info;
      std::string                                        app_name = "appbase";
      std::vector< std::string >                         default_plugins;

      void set_program_options();
      void write_default_config( const bfs::path& cfg_file );
      void generate_completions();
      std::unique_ptr< class application_impl > my;

      signals_handler_wrapper                 handler_wrapper;

      std::atomic_bool _is_interrupt_request{false};

      bool is_finished = false;

      /*
        When the application is closed very fast (milliseconds after launch),
        then SIGFAULT can be generated, because a startup or an initialization can be done in the same time.
        The best solution is to synchronize these methods.
      */
      std::mutex app_mtx;

    private:

      using notify_status_handler_t = std::function<void(const hive::utilities::collector_t&)>;
      using notify_status_t = boost::signals2::signal<void(const hive::utilities::collector_t&)>;
      notify_status_t notify_status_signal;

    public:

      boost::signals2::connection add_notify_status_handler( const notify_status_handler_t& func );

    public:

      finish_request_type finish_request;

      void save_status( const fc::string& status, const fc::string& status_description = "hived_status" ) const noexcept;

      template <typename... KeyValuesTypes>
      inline void save_information(
          const fc::string &name,
          KeyValuesTypes &&...key_value_pairs) const noexcept
      {
        hive::utilities::collector_t _items( name, std::forward<KeyValuesTypes>( key_value_pairs )... );
        notify_status_signal( _items );
      }
  };

  template< typename Impl >
  class plugin : public abstract_plugin
  {
    public:
      virtual ~plugin() {}

      virtual pre_shutdown_order get_pre_shutdown_order() const override { return _pre_shutdown_order; }
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
          get_app().plugin_initialized( *this );
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
          get_app().plugin_started( *this );
        }
        if (_state != started )
          BOOST_THROW_EXCEPTION( std::runtime_error("Initial state was not initialized, so final state cannot be started.") );
      }

      virtual void finalize_startup() override final
      {
        this->plugin_finalize_startup();
      }

      virtual void plugin_finalize_startup() override
      {
        /*
          By default most plugins don't need any post-actions during startup.
          Problem is with JSON plugin, that has API plugins attached to itself.
          Linking plugins into JSON plugin has to be done after startup actions.
        */
      }

      virtual void plugin_pre_shutdown() override
      {
        /*
          By default most plugins don't need any pre-actions during shutdown.
          A problem appears when P2P plugin receives and sends data into dependent plugins.
          In this case is necessary to close P2P plugin as soon as possible.
        */
      }

      virtual void pre_shutdown() override final
      {
        if( _state == started )
        {
          this->plugin_pre_shutdown();
        }
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

    void attach_application( application* app )
    {
      theApp = app;
    }

    application& get_app()
    {
      FC_ASSERT( theApp );
      return *theApp;
    }

    protected:

      virtual void set_pre_shutdown_order( pre_shutdown_order val ) { _pre_shutdown_order = val; }

    private:

      application* theApp = nullptr;

      pre_shutdown_order _pre_shutdown_order = abstract_plugin::basic_order;
      state _state = abstract_plugin::registered;
  };
}
