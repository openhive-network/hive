#include <hive/utilities/postgres_connection_holder.hpp>

namespace hive { namespace utilities {

    using work = pqxx::work;

    void mylog(const char* msg)
    {
      std::cout << "[ " << std::this_thread::get_id() << " ] " << msg << std::endl;
    }

    fc::string generate(std::function<void(fc::string &)> fun)
    {
      fc::string ss;
      fun(ss);
      return std::move(ss);
    }
}//namespace utilities
}//namespace hive
