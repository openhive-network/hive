#pragma once

#include <boost/asio/deadline_timer.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/filesystem/path.hpp>

#include <string>

namespace beekeeper
{
  class singleton_beekeeper
  {
    private:

      bool instance_started = false;

      const std::string file_ext = ".wallet";

      boost::filesystem::path pid_file;
      boost::filesystem::path connection_file;
      boost::filesystem::path lock_path_file;

      boost::filesystem::path wallet_directory;

      std::unique_ptr<boost::interprocess::file_lock> wallet_dir_lock;

      void start_lock_watch( std::shared_ptr<boost::asio::deadline_timer> t );
      void initialize_lock();
   
      void save_pid();
      void save_connection_data();

    public:

      singleton_beekeeper( const boost::filesystem::path& _wallet_directory );
      ~singleton_beekeeper();

      void start();
      void close();

      boost::filesystem::path create_wallet_filename( const std::string& wallet_name ) const;

      bool started() const;
      void save_data();
  };
}
