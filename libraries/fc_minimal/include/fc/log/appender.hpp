#pragma once
#include <fc/shared_ptr.hpp>
#include <fc/string.hpp>
#include <fc/reflect/variant.hpp>

namespace fc {
   class appender;
   class log_message;
   class variant;

   class appender_factory : public fc::retainable {
      public:
       typedef fc::shared_ptr<appender_factory> ptr;

       virtual ~appender_factory(){};
       virtual fc::shared_ptr<appender> create( const variant& args ) = 0;
   };

   namespace detail {
      template<typename T>
      class appender_factory_impl : public appender_factory {
        public:
           virtual fc::shared_ptr<appender> create( const variant& args ) {
              return fc::shared_ptr<appender>(new T(args));
           }
      };
   }

   class appender : public fc::retainable {
      public:
         enum class time_format {
           milliseconds_since_hour,    /// 344686ms (ca 5 minutes 44 seconds after full hour)
           milliseconds_since_epoch,   /// 1685696704999ms
           iso_8601_seconds,           /// 2023-06-02T09:05:41
           iso_8601_milliseconds,      /// 2023-06-02T09:05:10.580
           iso_8601_microseconds       /// 2023-06-02T09:05:06.894046
         };

         typedef fc::shared_ptr<appender> ptr;

         template<typename T>
         static bool register_appender(const fc::string& type) {
            return register_appender( type, new detail::appender_factory_impl<T>() );
         }

         static appender::ptr create( const fc::string& name, const fc::string& type, const variant& args  );
         static appender::ptr get( const fc::string& name );
         static bool          register_appender( const fc::string& type, const appender_factory::ptr& f );
         static std::string format_time_as_string(time_point time, time_format format);

         virtual void log( const log_message& m ) = 0;
   };
}
FC_REFLECT_ENUM( fc::appender::time_format, 
                 (milliseconds_since_hour)
                 (milliseconds_since_epoch)
                 (iso_8601_seconds)
                 (iso_8601_milliseconds)
                 (iso_8601_microseconds) )
