#pragma once

#include <core/utilities.hpp>

#include <string>

namespace beekeeper {

class time_manager_base
{
  public:

    time_manager_base(){}
    virtual ~time_manager_base(){};

    virtual void add( const std::string& token, types::lock_method_type&& lock_method, types::notification_method_type&& notification_method ){};
    virtual void change( const std::string& token, const types::timepoint_t& time ){};

    virtual void run(){};

    virtual void close( const std::string& token ){};
};

} //beekeeper

