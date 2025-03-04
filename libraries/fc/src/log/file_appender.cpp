#include <fc/exception/exception.hpp>
#include <fc/io/fstream.hpp>
#include <fc/log/file_appender.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/thread/scoped_lock.hpp>
#include <fc/thread/thread.hpp>
#include <fc/variant.hpp>
#include <fc/log/appender.hpp>
#include <boost/thread/mutex.hpp>
#include <iomanip>
#include <queue>
#include <sstream>
#include <iostream>

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>

namespace fc {

   class file_appender::impl : public fc::retainable
   {
      public:
         config                      cfg;
         boost::filesystem::ofstream out;
         boost::mutex                slock;

         fc::optional<time_point>   last_log_message_timestamp;
      private:
         future<void>               _rotation_task;
         time_point_sec             _current_file_start_time;

         time_point_sec get_file_start_time( const time_point_sec& timestamp, const microseconds& interval )
         {
             int64_t interval_seconds = interval.to_seconds();
             int64_t file_number = timestamp.sec_since_epoch() / interval_seconds;
             return time_point_sec( (uint32_t)(file_number * interval_seconds) );
         }

      public:
         impl( const config& c) : cfg( c )
         {
             if( cfg.rotate )
             {
                 FC_ASSERT( cfg.rotation_interval >= seconds( 1 ) );
                 FC_ASSERT( cfg.rotation_limit >= cfg.rotation_interval );




                 _rotation_task = async( [this]() { rotate_files( true ); }, "rotate_files(1)" );
             }
         }

         ~impl()
         {
            try
            {
              _rotation_task.cancel_and_wait("file_appender is destructing");
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

   file_appender::config::config( const fc::path& p ) :
      format( "${timestamp} ${context} ${file}:${line} ${method} ${level}]  ${message}" ),
      filename( p ),
      flush( true ),
      truncate( true ),
      rotate( false )
   {}

   file_appender::file_appender( const variant& args ) :
      my( new impl( args.as<config>() ) )
   {
      try
      {
         const fc::path parent_path = my->cfg.filename.parent_path();
         if (parent_path != fc::path() && !fc::exists(parent_path))
            fc::create_directories(parent_path);

         if(!my->cfg.rotate) 
         {
            if (my->cfg.truncate)
               my->out.open( my->cfg.filename, std::ios_base::out | std::ios_base::trunc);
            else
               my->out.open( my->cfg.filename, std::ios_base::out | std::ios_base::app);
         }

      }
      catch(const fc::exception &e)
      {
         std::cerr << "error opening log file: " << e.to_detail_string() << "\n";
      }
      catch(const std::exception &e)
      {
         std::cerr << "error opening log file: " << e.what() << "\n";
      }
      catch( ... )
      {
         std::cerr << "error opening log file: " << my->cfg.filename.preferred_string() << "\n";
      }
   }

   file_appender::~file_appender(){}

   // MS THREAD METHOD  MESSAGE \t\t\t File:Line
   void file_appender::log( const log_message& m )
   {
      std::stringstream line;
      line << appender::format_time_as_string(m.get_context().get_timestamp(), my->cfg.time_format);

      if (my->cfg.delta_times)
      {
        fc::optional<fc::time_point> last_log_message_timestamp;
        {
          fc::scoped_lock<boost::mutex> lock( my->slock );
          last_log_message_timestamp = my->last_log_message_timestamp;
          my->last_log_message_timestamp = m.get_context().get_timestamp();
        }

        if (last_log_message_timestamp)
        {
          microseconds time_since_last_message = m.get_context().get_timestamp() - *last_log_message_timestamp;
          line << " Δ" << std::setw(7) << time_since_last_message.count() << "µs ";
        }
        else
          line << "            ";
      }
      line << "  " << m.get_context().get_file() << ":" << m.get_context().get_line_number() << "  ";

      string method_name = m.get_context().get_method();
      // strip all leading scopes...
      if( method_name.size() )
      {
         uint32_t p = 0;
         for( uint32_t i = 0;i < method_name.size(); ++i )
         {
             if( method_name[i] == ':' ) p = i;
         }

         if( method_name[p] == ':' )
           ++p;
         line << std::setw( 20 ) << std::left << m.get_context().get_method().substr(p,20).c_str() <<" ";
      }

      line << "] ";
      fc::string message = fc::format_string( m.get_format(), m.get_data() );
      line << message.c_str();
      line << "\n";

      //fc::variant lmsg(m);

      // fc::string fmt_str = fc::format_string( my->cfg.format, mutable_variant_object(m.get_context())( "message", message)  );

      {
        fc::scoped_lock<boost::mutex> lock( my->slock );
        my->out << line.str();
        if( my->cfg.flush )
          my->out.flush();
      }
   }

   static bool reg_file_appender = []( __attribute__((unused)) bool* )->bool
   {
      return appender::register_appender<file_appender>( "file" );
   }( &reg_file_appender );

} // fc
