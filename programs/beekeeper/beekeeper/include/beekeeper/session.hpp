#pragma once

#include <core/session_base.hpp>

namespace beekeeper {

class beekeeper_instance_base;

class session: public session_base
{
  public:

    session( const std::string& token, std::shared_ptr<time_manager_base> time, const boost::filesystem::path& wallet_directory );
};

} //beekeeper
