#pragma once

#include <core/beekeeper_instance_base.hpp>

#include <boost/asio/deadline_timer.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/filesystem/path.hpp>

#include <hive/utilities/notifications.hpp>

#include <string>

namespace beekeeper
{
  using collector_t = hive::utilities::notifications::collector_t;

  class beekeeper_instance: public beekeeper_instance_base
  {
    private:

      bool instance_started = false;

      boost::filesystem::path pid_file;
      boost::filesystem::path connection_file;
      boost::filesystem::path lock_path_file;

      std::unique_ptr<boost::interprocess::file_lock> wallet_dir_lock;
      std::string error_notifications_endpoint;

      void start_lock_watch( std::shared_ptr<boost::asio::deadline_timer> t );
      void initialize_lock();

      template<typename content_type>
      void write_to_file( const boost::filesystem::path& file_name, const content_type& content );

      fc::variant read_file( const boost::filesystem::path& file_name );

      void save_pid();

      void send_fail_notification();

    public:

      beekeeper_instance( const boost::filesystem::path& _wallet_directory, const std::string& _notifications_endpoint );
      ~beekeeper_instance() override;

      bool start() override;

      void save_connection_details( const collector_t& values );
  };
}
