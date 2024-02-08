#include<atomic>
#include<chrono>
#include<mutex>

namespace beekeeper {

class extended_api
{
  public:

    using status = enum { disabled, enabled_without_error, enabled_after_interval };

  private:

    uint64_t interval = 500;

    std::atomic<bool> error_status;
    std::atomic<uint64_t> start;

    std::mutex mtx;

    bool enabled_impl( const std::atomic<uint64_t>& end )
    {
      return error_status.load() && end.load() > start.load() && end.load() - start.load() > interval;
    }

    uint64_t get_milliseconds()
    {
      return ( std::chrono::time_point_cast<std::chrono::milliseconds>( std::chrono::system_clock::now() ).time_since_epoch() ).count();
    }

  public:

    extended_api();
    ~extended_api(){}

    status unlock_allowed();

    void was_error();
};

}