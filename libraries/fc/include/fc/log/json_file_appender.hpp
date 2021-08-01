#pragma once

#include <fc/filesystem.hpp>
#include <fc/log/appender.hpp>
#include <fc/log/logger.hpp>
#include <fc/time.hpp>

namespace fc {

class json_file_appender : public appender {
    public:
         struct config {
            config( const fc::path& p = "log.json" );

            fc::path                           filename;
            bool                               flush = true;
            bool                               truncate = true;
            bool                               rotate = false;
            microseconds                       rotation_interval;
            microseconds                       rotation_limit;
            bool                               rotation_compression = false;
            bool                               truncate_method_names = true;
         };
         json_file_appender( const variant& args );
         ~json_file_appender();
         virtual void log( const log_message& m )override;

      private:
         class impl;
         fc::shared_ptr<impl> my;
   };
} // namespace fc

#include <fc/reflect/reflect.hpp>
FC_REFLECT( fc::json_file_appender::config,
            (filename)(flush)(truncate)(rotate)(rotation_interval)(rotation_limit)(rotation_compression)(truncate_method_names) )
