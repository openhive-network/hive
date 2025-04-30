#pragma once

#include<shared_mutex>

namespace beekeeper {

class mutex_handler
{
  private:

    std::shared_mutex mtx;

  public:

    inline std::shared_mutex& get_mutex(){ return mtx; }
};

} //beekeeper
