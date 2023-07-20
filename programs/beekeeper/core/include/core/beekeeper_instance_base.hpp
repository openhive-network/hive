#pragma once

#include <boost/filesystem/path.hpp>

namespace beekeeper
{
  class beekeeper_instance_base
  {
    private:

      const std::string file_ext = ".wallet";

    protected:

      boost::filesystem::path wallet_directory;

    public:

      beekeeper_instance_base( const boost::filesystem::path& _wallet_directory ): wallet_directory( _wallet_directory ){}
      virtual ~beekeeper_instance_base(){}

      virtual bool start(){ return true; }

      virtual boost::filesystem::path create_wallet_filename( const std::string& wallet_name ) const
      {
        return wallet_directory / ( wallet_name + file_ext );
      }
  };
}
