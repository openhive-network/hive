#pragma once

#include <string>

namespace beekeeper {

class beekeeper_app_base
{
  protected:

    virtual void set_program_options() = 0;
    virtual bool save_keys( const std::string& token, const std::string& wallet_name, const std::string& wallet_password ) = 0;
    virtual std::pair<bool, std::string> initialize_program_options() = 0;
    virtual std::pair<bool, bool> initialize( int argc, char** argv ) = 0;
    virtual bool start() = 0;

    int run( int argc, char** argv );

  public:

    beekeeper_app_base(){}

    virtual ~beekeeper_app_base(){}

    virtual int init( int argc, char** argv ) = 0;

};

}
