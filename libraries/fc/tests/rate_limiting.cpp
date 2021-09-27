#include <fc/network/http/http.hpp>
#include <fc/network/rate_limiting.hpp>
#include <fc/network/ip.hpp>
#include <fc/time.hpp>
#include <fc/thread/thread.hpp>

#include <iostream>
#include <memory>
fc::rate_limiting_group rate_limiter(1000000, 1000000);

void download_url(const std::string& ip_address, const std::string& url)
{
  auto client = std::make_shared< http_client >();
  auto con = client->connect( url );
  std::cout << "Starting download...\n";
  fc::time_point start_time(fc::time_point::now());
  con->on_http_handler( [&]( const std::string& msg ){
    fc::time_point end_time(fc::time_point::now());
    std::cout << "Retreived " << msg.size() << " bytes in " << ((end_time - start_time).count() / fc::milliseconds(1).count()) << "ms\n";
    std::cout << "Average speed " << ((1000 * (uint64_t)msg.size()) / ((end_time - start_time).count() / fc::milliseconds(1).count())) << " bytes per second";
  } );
  con->send_message("");
}

int main( int argc, char** argv )
{
  rate_limiter.set_actual_rate_time_constant(fc::seconds(1));

  std::vector<fc::future<void> > download_futures;
  download_futures.push_back(fc::async([](){ download_url("198.82.184.145", "http://mirror.cs.vt.edu/pub/cygwin/glibc/releases/glibc-2.9.tar.gz"); }));
  download_futures.push_back(fc::async([](){ download_url("198.82.184.145", "http://mirror.cs.vt.edu/pub/cygwin/glibc/releases/glibc-2.7.tar.gz"); }));

  while (1)
  {
    bool all_done = true;
    for (unsigned i = 0; i < download_futures.size(); ++i)
      if (!download_futures[i].ready())
        all_done = false;
    if (all_done)
      break;
    std::cout << "Current measurement of actual transfer rate: upload " << rate_limiter.get_actual_upload_rate() << ", download " << rate_limiter.get_actual_download_rate() << "\n";
    fc::usleep(fc::seconds(1));
  }

  for (unsigned i = 0; i < download_futures.size(); ++i)
    download_futures[i].wait();
  return 0;
}
