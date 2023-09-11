#pragma once

#include <core/utilities.hpp>

#include <string>

namespace beekeeper {

class beekeeper_app_base
{
  protected:

    virtual void set_program_options() = 0;
    virtual bool save_keys( const std::string& notification, const std::string& wallet_name, const std::string& wallet_password ) = 0;
    virtual init_data initialize( int argc, char** argv ) = 0;
    virtual void start() = 0;

    init_data run( int argc, char** argv );

  public:

    beekeeper_app_base(){}

    virtual ~beekeeper_app_base(){}

    virtual init_data init( int argc, char** argv ) = 0;

};

}
