AppBase
--------------

The AppBase library provides a basic framework for building applications from
a set of plugins. AppBase manages the plugin life-cycle and ensures that all
plugins are configured, initialized, started, and shutdown in the proper order.

## Key Features

- Dynamically Specify Plugins to Load
- Automaticly Load Dependent Plugins in Order
- Plugins can specify commandline arguments and configuration file options
- Program gracefully exits from SIGINT and SIGTERM
- Minimal Dependencies (Boost 1.58, c++14)

## Defining a Plugin

A simple example of a 2-plugin application can be found in the /examples directory. Each plugin has
a simple life cycle:

1. Initialize - parse configuration file options
2. Startup - start executing, using configuration file options
3. Shutdown - stop everything and free all resources

All plugins complete the Initialize step before any plugin enters the Startup step. Any dependent plugin specified
by `APPBASE_PLUGIN_REQUIRES` will be Initialized or Started prior to the plugin being Initialized or Started. 

Shutdown is called in the reverse order of Startup. 

```
class net_plugin : public appbase::plugin<net_plugin>
{
   public:
     net_plugin(): appbase::plugin<net_plugin>(){};
     ~net_plugin(){};

     APPBASE_PLUGIN_REQUIRES( (chain_plugin) );

     virtual void set_program_options( options_description& cli, options_description& cfg ) override
     {
        cfg.add_options()
              ("listen-endpoint", bpo::value<string>()->default_value( "127.0.0.1:9876" ), "The local IP address and port to listen for incoming connections.")
              ("remote-endpoint", bpo::value< vector<string> >()->composing(), "The IP address and port of a remote peer to sync with.")
              ("public-endpoint", bpo::value<string>()->default_value( "0.0.0.0:9876" ), "The public IP address and port that should be advertized to peers.")
              ;
     }

     void plugin_initialize( const variables_map& options ) { std::cout << "initialize net plugin\n"; }
     void plugin_startup()  { std::cout << "starting net plugin \n"; }
     void plugin_shutdown() { std::cout << "shutdown net plugin \n"; }

};

int main( int argc, char** argv ) {
   try {
      appbase::application theApp;
      theApp.register_plugin<net_plugin>(); // implicit registration of chain_plugin dependency

      auto initResult = theApp.initialize( argc, argv );
      if( !initResult.should_start_loop() )
        return initResult.get_result_code();
      
      theApp.startup();
      theApp.exec();
   } catch ( const boost::exception& e ) {
      std::cerr << boost::diagnostic_information(e) << "\n";
   } catch ( const std::exception& e ) {
      std::cerr << e.what() << "\n";
   } catch ( ... ) {
      std::cerr << "unknown exception\n";
   }
   std::cout << "exited cleanly\n";
   return 0;
}
```

This example can be used like follows:

```
./examples/appbase_example --plugin net_plugin
initialize chain plugin
initialize net plugin
starting chain plugin
starting net plugin
^C
shutdown net plugin
shutdown chain plugin
exited cleanly
```

### Boost ASIO 

The application owns a `boost::asio::io_service` which starts running when appbase::exec() is called. If 
a plugin needs to perform IO or other asynchronous operations then it should dispatch it via 
`get_app().get_io_service().post( lambda )`.

Because the app calls `io_service::run()` from within `application::exec()` all asynchronous operations
posted to the io_service should be run in the same thread.  

## Graceful Exit 

To trigger a graceful exit send SIGTERM or SIGINT to the process.

## Dependencies 

1. c++14 or newer  (clang or g++)
2. Boost 1.60 or newer compiled with C++14 support

To compile boost with c++14 use:

   ./b2 ...  cxxflags="-std=c++0x -stdlib=libc++" linkflags="-stdlib=libc++" ...


