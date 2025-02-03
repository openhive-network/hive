#include <hive/utilities/notifications.hpp>

#include <fc/io/json.hpp>
#include <fc/log/logger.hpp>
#include <fc/network/http/connection.hpp>
#include <fc/network/resolve.hpp>
#include <fc/network/url.hpp>

namespace hive { namespace utilities { namespace notifications {

bool version_checker::version = false;

namespace{
  const char* get_flag()
  {
    return "notifications-endpoint";
  }
}

bool check_is_flag_set(const boost::program_options::variables_map &args)
{
  return args.count( get_flag() ) > 0;
}

void add_program_options(boost::program_options::options_description& options)
{
  options.add_options()(
    get_flag(),
    boost::program_options::value< std::vector<fc::string> >()->multitoken(),
    "list of addresses, that will receive notification about in-chain events"
  );
}

std::vector<fc::string> setup_notifications(const boost::program_options::variables_map &args)
{
  if( !hive::utilities::notifications::check_is_flag_set(args) ) return {};
  return args[ hive::utilities::notifications::get_flag() ].as<std::vector<fc::string>>();
}

namespace detail
{
bool error_handler(const std::function<void ()> &foo)
{
  bool is_exception_occured = false;
  auto handle_exception = [&is_exception_occured](const fc::string &ex)
  {
    is_exception_occured = true;
    wlog("notification error: `${details}`", ("details", ex));
  };

  try { foo(); }
  catch(const fc::assert_exception& ex){ handle_exception(ex.to_detail_string()); }
  catch(const fc::exception& ex){ handle_exception(ex.to_string()); }
  catch(const std::exception& ex){ handle_exception(ex.what()); }
  catch(...){ handle_exception("unknown exception"); }
  return !is_exception_occured;
}

void notification_handler::setup(const std::vector<std::string> &address_pool)
{
  const std::vector<fc::ip::endpoint> _address_pool = create_endpoints(address_pool);

  if (_address_pool.empty()) network.reset();
  else
  {
    const size_t ap_size = _address_pool.size();
    ilog("setting up notification handler for ${count} address${fix}", ("count", ap_size)( "fix", (ap_size > 1 ? "es" : "") ));

    network = std::make_unique<network_broadcaster>(_address_pool, on_send);
  }
}

void notification_handler::register_endpoints( const std::vector<std::string> &pool )
{
  if( pool.empty() )
    return;

  if( !network )
    network = std::make_unique<network_broadcaster>(create_endpoints(pool), on_send);
  else
    network->register_endpoints(create_endpoints(pool));
}

bool notification_handler::is_broadcasting_active() const
{
  return network.get() != nullptr;
}

std::vector<fc::ip::endpoint> notification_handler::create_endpoints( const std::vector<std::string>& address_pool )
{
  std::vector<fc::ip::endpoint> epool;
  epool.reserve(address_pool.size());

  for(const fc::string& x : address_pool)
  {
    auto _url = fc::url{ x };
    if( _url.proto() == "http" || _url.proto() == "https" )
    {
      if( _url.host() && _url.port() )
        epool.emplace_back( fc::resolve( *_url.host(), *_url.port() )[0] );
    }
    else
      epool.emplace_back( fc::resolve_string_to_ip_endpoints(x)[0] );
  }

  return epool;
}

void notification_handler::network_broadcaster::broadcast(const notification_t &notification) noexcept
{
  if (!allowed()) return;

  bool is_exception_occured = false;

  for (auto &address : address_pool)
  {
    if (!allowed()) return;
    is_exception_occured = !error_handler([&]{
      if (address.second < MAX_RETRIES)
      {
        fc::http::connection_ptr con{new fc::http::connection{}};
        con->connect_to(address.first);
        con->request("PUT", "", fc::json::to_string(notification), fc::http::headers(), false);
      }
    });

    if( is_exception_occured )
    {
      wlog("Establishing a connection with an address ${addr} failed", ("addr", address.first));

      if( ++address.second == MAX_RETRIES )
        wlog("Address ${addr} exceeded max amount of failures, no future requests will be sent there", ("addr", address.first));
      else
        address.second = 0;
    }
  }
}

void notification_handler::network_broadcaster::register_endpoints( const std::vector<fc::ip::endpoint> &pool )
{
  for (const fc::ip::endpoint &addr : pool)
    address_pool[addr] = 0;
}

} // detail

bool error_handler( const std::function<void()>& foo )
{
  return detail::error_handler( foo );
}

} // notifications
} // utilities

} // hive
