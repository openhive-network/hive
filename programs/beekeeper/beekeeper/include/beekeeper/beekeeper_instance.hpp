#pragma once

#include <appbase/application.hpp>

#include <core/beekeeper_instance_base.hpp>
#include <core/utilities.hpp>

#include <boost/asio/deadline_timer.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/filesystem/path.hpp>


#include <string>

namespace beekeeper
{
  using collector_t = hive::utilities::notifications::collector_t;

  class beekeeper_instance: public beekeeper_instance_base
  {
    private:

      bool instance_started = false;

      appbase::application& app;

      boost::filesystem::path lock_path_file;

      std::unique_ptr<boost::interprocess::file_lock> wallet_dir_lock;

      std::shared_ptr<status> app_status;

      void start_lock_watch( std::shared_ptr<boost::asio::deadline_timer> t );
      void initialize_lock();

    public:

      beekeeper_instance( appbase::application& app, const boost::filesystem::path& wallet_directory, const std::shared_ptr<status>& app_status );
      ~beekeeper_instance() override;

      bool start() override;
  };
}
