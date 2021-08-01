#include <fc/exception/exception.hpp>
#include <fc/io/fstream.hpp>
#include <fc/log/json_file_appender.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/io/json.hpp>
#include <fc/thread/scoped_lock.hpp>
#include <fc/thread/thread.hpp>
#include <fc/variant.hpp>
#ifdef FC_USE_FULL_ZLIB
# include <fc/compress/zlib.hpp>
#endif
#include <boost/thread/mutex.hpp>
#include <boost/algorithm/string/join.hpp>
#include <iomanip>
#include <queue>
#include <sstream>
#include <iostream>

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>

namespace fc {

   static const string compression_extension( ".gz" );

   class json_file_appender::impl : public fc::retainable
   {
      public:
         config                      cfg;
         boost::filesystem::ofstream out;
         boost::mutex                slock;

      private:
         future<void>               _rotation_task;
         time_point_sec             _current_file_start_time;
         std::unique_ptr<thread>    _compression_thread;

         time_point_sec get_file_start_time( const time_point_sec& timestamp, const microseconds& interval )
         {
             int64_t interval_seconds = interval.to_seconds();
             int64_t file_number = timestamp.sec_since_epoch() / interval_seconds;
             return time_point_sec( (uint32_t)(file_number * interval_seconds) );
         }

         void compress_file( const fc::path& filename )
         {
#ifdef FC_USE_FULL_ZLIB
             FC_ASSERT( cfg.rotate && cfg.rotation_compression );
             FC_ASSERT( _compression_thread );
             if( !_compression_thread->is_current() )
             {
                 _compression_thread->async( [this, filename]() { compress_file( filename ); }, "compress_file" ).wait();
                 return;
             }

             try
             {
                 gzip_compress_file( filename, filename.parent_path() / (filename.filename().string() + compression_extension) );
                 remove_all( filename );
             }
             catch( ... )
             {
             }
#endif
         }

      public:
         impl( const config& c) : cfg( c )
         {
            try
            {
               fc::create_directories(cfg.filename.parent_path());

                if( cfg.rotate )
                {
                    FC_ASSERT( cfg.rotation_interval >= seconds( 1 ) );
                    FC_ASSERT( cfg.rotation_limit >= cfg.rotation_interval );

#ifdef FC_USE_FULL_ZLIB
                 if( cfg.rotation_compression )
                     _compression_thread.reset( new thread( "compression") );
#endif

                  rotate_files( true );
                }
            }
            catch( ... )
            {
               std::cerr << "error opening log file: " << cfg.filename.preferred_string() << "\n";
            }
         }

         ~impl()
         {
            try
            {
              _rotation_task.cancel_and_wait("json_file_appender is destructing");
            }
            catch( ... )
            {
            }
         }

         void rotate_files( bool initializing = false )
         {
             FC_ASSERT( cfg.rotate );
             fc::time_point now = time_point::now();
             fc::time_point_sec start_time = get_file_start_time( now, cfg.rotation_interval );
             string timestamp_string = start_time.to_non_delimited_iso_string();
             fc::path link_filename = cfg.filename;
             fc::path log_filename = link_filename.parent_path() / (link_filename.filename().string() + "." + timestamp_string);

             {
               fc::scoped_lock<boost::mutex> lock( slock );

               if( !initializing )
               {
                   if( start_time <= _current_file_start_time )
                   {
                       _rotation_task = schedule( [this]() { rotate_files(); },
                                                  _current_file_start_time + cfg.rotation_interval.to_seconds(),
                                                  "rotate_files(2)" );
                       return;
                   }

                   out.flush();
                   out.close();
               }
               remove_all(link_filename);  // on windows, you can't delete the link while the underlying file is opened for writing
               if (cfg.truncate)
                  out.open( log_filename, std::ios_base::out | std::ios_base::trunc );
               else
                  out.open( log_filename, std::ios_base::out | std::ios_base::app );

               create_hard_link(log_filename, link_filename);
             }

             /* Delete old log files */
             fc::time_point limit_time = now - cfg.rotation_limit;
             string link_filename_string = link_filename.filename().string();
             directory_iterator itr(link_filename.parent_path());
             for( ; itr != directory_iterator(); itr++ )
             {
                 try
                 {
                     string current_filename = itr->filename().string();
                     if (current_filename.compare(0, link_filename_string.size(), link_filename_string) != 0 ||
                         current_filename.size() <= link_filename_string.size() + 1)
                       continue;
                     string current_timestamp_str = current_filename.substr(link_filename_string.size() + 1,
                                                                            timestamp_string.size());
                     fc::time_point_sec current_timestamp = fc::time_point_sec::from_iso_string( current_timestamp_str );
                     if( current_timestamp < start_time )
                     {
                         if( current_timestamp < limit_time || file_size( link_filename.parent_path() / itr->filename() ) <= 0 )
                         {
                             remove_all( *itr );
                             continue;
                         }
                         if( !cfg.rotation_compression )
                           continue;
                         if( current_filename.find( compression_extension ) != string::npos )
                           continue;
                         compress_file( *itr );
                     }
                 }
                 catch (const fc::canceled_exception&)
                 {
                     throw;
                 }
                 catch( ... )
                 {
                 }
             }

             _current_file_start_time = start_time;
             _rotation_task = schedule( [this]() { rotate_files(); },
                                        _current_file_start_time + cfg.rotation_interval.to_seconds(),
                                        "rotate_files(3)" );
         }
   };

   json_file_appender::config::config(const fc::path& p) :
     filename(p),
     flush(true),
     truncate(true),
     rotate(false),
     rotation_compression(false),
     truncate_method_names(true)
   {}

   json_file_appender::json_file_appender( const variant& args ) :
     my( new impl( args.as<config>() ) )
   {
      try
      {
         if(!my->cfg.rotate) 
         {
            if (my->cfg.truncate)
               my->out.open( my->cfg.filename, std::ios_base::out | std::ios_base::trunc);
            else
               my->out.open( my->cfg.filename, std::ios_base::out | std::ios_base::app);
         }
      }
      catch( ... )
      {
         std::cerr << "error opening log file: " << my->cfg.filename.preferred_string() << "\n";
      }
   }

   json_file_appender::~json_file_appender(){}

   // MS THREAD METHOD  MESSAGE \t\t\t File:Line
   void json_file_appender::log( const log_message& m )
   {
      log_context context = m.get_context();

      string method_name = m.get_context().get_method();
      if (my->cfg.truncate_method_names)
      {
         // strip all leading scopes...
         // if the method name is "foo::bar::baz", this will leave you with "baz"
         // you lose some information, but in some cases you can get obnoxiously long
         // template class names in the method name otherwise
         uint32_t p = 0;
         for( uint32_t i = 0; i < method_name.size(); ++i )
            if( method_name[i] == ':' ) 
               p = i;

         if( method_name[p] == ':' )
            method_name = method_name.substr(p + 1);
      }
      
      time_point timestamp = m.get_context().get_timestamp();
      std::ostringstream timestamp_string;
      timestamp_string << string(timestamp);
      uint64_t microseconds = timestamp.time_since_epoch().count() % 1000000;
      timestamp_string << "." << std::setw(6) << std::setfill('0') << microseconds << "Z";

      string message = format_string( m.get_format(), m.get_data() );

      fc::mutable_variant_object log_message_json;
      log_message_json("file", context.get_file());
      log_message_json("line", context.get_line_number());
      log_message_json("method", std::move(method_name));

      string thread_name = context.get_thread_name();
      if (thread_name.size())
         log_message_json("thread", std::move(thread_name));

      string task_name = context.get_task_name();
      if (task_name.size())
         log_message_json("task", std::move(task_name));

      string host_name = context.get_host_name();
      if (host_name.size())
         log_message_json("host", std::move(host_name));

      log_message_json("timestamp", timestamp_string.str());
      log_message_json("log_level", context.get_log_level());

      string context_name = context.get_context();
      if (context_name.size())
         log_message_json("context", std::move(context_name));

      // hive's fc doesn't have scope tags (yet)
      //std::vector<string> scope_tags = context.get_scope_tags();
      //if (!scope_tags.empty())
      //   log_message_json("scope_tags", std::move(scope_tags));

      log_message_json("message", std::move(message));

      {
        fc::scoped_lock<boost::mutex> lock( my->slock );
        my->out << json::to_string((mutable_variant_object)log_message_json) << "\n";
        if( my->cfg.flush )
          my->out.flush();
      }

      return;

   }

} // fc
