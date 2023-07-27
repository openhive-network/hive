#include<atomic>
#include<chrono>
#include<mutex>

namespace beekeeper {

class extended_api
{
  private:

    uint64_t interval = 500;
    std::atomic<uint64_t> start;

    std::mutex mtx;

    uint64_t get_milliseconds()
    {
      return ( std::chrono::time_point_cast<std::chrono::milliseconds>( std::chrono::system_clock::now() ).time_since_epoch() ).count();
    }

  public:

    extended_api();
    ~extended_api(){}

    bool enabled();
};

}