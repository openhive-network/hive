#pragma once

#include <appbase/application.hpp>

#include <string>

namespace beekeeper {

class beekeeper_app_base
{
  public:

    using p_beekeeper_app_base = std::unique_ptr<beekeeper_app_base>;

  protected:

    appbase::application& app;

    virtual void set_program_options() = 0;
    virtual void save_keys( const std::string& token, const std::string& wallet_name, const std::string& wallet_password ) = 0;
    virtual std::pair<bool, std::string> initialize_program_options() = 0;
    virtual std::pair<appbase::initialization_result::result, bool> initialize( int argc, char** argv ) = 0;
    virtual appbase::initialization_result::result start() = 0;

  public:

    beekeeper_app_base(): app( appbase::app() ){}

    virtual ~beekeeper_app_base(){}

    int run( int argc, char** argv );
    static int execute( p_beekeeper_app_base&& app, int argc, char** argv );

};

}