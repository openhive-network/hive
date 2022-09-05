#include "fc/string.hpp"
#include <boost/program_options/variables_map.hpp>
#include <hive/utilities/notifications.hpp>

#include <fc/io/json.hpp>
#include <fc/log/logger.hpp>
#include <fc/network/http/connection.hpp>
#include <fc/network/resolve.hpp>
#include <vector>

namespace hive { namespace utilities { namespace notifications {

namespace flags{
  const char* notifications_endpoint()
  {
    return "notifications-endpoint";
  }

  const char* notifications_filter()
  {
    /*
      notification filter can work in one of two modes:

        - send all that matches given regex, ex [config.ini]:

          notifications-filter = (jsonrpc/.*)                    # sends only jsonrpc timings
          notifications-filter = (hived_status|benchmark)        # sends only notification with current hived status and multiindex stats

        - send all that doesn't match given regex, ex [config.ini]:

          notifications-filter = !(benchmark)                    # sends all, except benchmark notifications
          notifications-filter = !(hived_status|jsonrpc/.*)      # sends all, except these with current hived status and jsonrpc timings
    */
    return "notifications-filter";
  }
}

bool check_is_notifications_enabled(const boost::program_options::variables_map &args)
{
  return args.count( flags::notifications_endpoint() ) > 0;
}

bool check_is_notification_filter_provided(const boost::program_options::variables_map &args)
{
  return args.count( flags::notifications_filter() ) > 0;
}

void add_program_options(boost::program_options::options_description& options)
{
  options.add_options()
  (
    flags::notifications_endpoint(),
    boost::program_options::value< std::vector<fc::string> >()->multitoken(),
    "list of addresses, that will receive notification about in-chain events"
  )
  (
    flags::notifications_filter(),
    boost::program_options::value< fc::string >()->default_value(""),
    "notification is accepted if name matches given regular expression, if not specified all notifications are accepted"
  );
}

std::tuple<std::vector<fc::string>, fc::string> setup_notifications(const boost::program_options::variables_map &args)
{
  std::vector<fc::string> endpoints{};
  fc::string filter = "";
  if( hive::utilities::notifications::check_is_flag_set(args))
  {
    endpoints = args[ flags::notifications_endpoint() ].as<std::vector<fc::string>>();
    if(hive::utilities::notifications::check_is_notification_filter_provided(args))
      filter = args[ flags::notifications_filter() ].as<fc::string>();
  }
  return {endpoints, filter};
}

namespace detail
{
bool error_handler(const std::function<void ()> &foo)
{
  bool is_exception_occurred = false;
  auto handle_exception = [&is_exception_occurred](const fc::string &ex)
  {
    is_exception_occurred = true;
    wlog("caught exception! details:\n${details}", ("details", ex));
  };

  try { foo(); }
  catch(const fc::assert_exception& ex){ handle_exception(ex.to_detail_string()); }
  catch(const fc::exception& ex){ handle_exception(ex.to_string()); }
  catch(const std::exception& ex){ handle_exception(ex.what()); }
  catch(...){ handle_exception("unknown exception"); }
  return !is_exception_occurred;
}

void notification_handler::setup(const std::vector<std::string> &address_pool, const fc::string& regex)
{
  const std::vector<fc::ip::endpoint> _address_pool = create_endpoints(address_pool);

  if (_address_pool.empty()) network.reset();
  else
  {
    const size_t ap_size = _address_pool.size();
    ilog("setting up notification handler for ${count} address${fix}", ("count", ap_size)( "fix", (ap_size > 1 ? "es" : "") ));

    network = std::make_unique<network_broadcaster>(_address_pool, on_send);

    if (!regex.empty() && !(regex.size() == 1 && regex[0] == notification_handler::exclude_matching_sign))
    {
      this->exclude_matching = (regex[0] == notification_handler::exclude_matching_sign);
      this->name_filter = std::regex(regex.substr(static_cast<size_t>(this->exclude_matching)));
    }
  }
}

void notification_handler::register_endpoints( const std::vector<std::string> &pool )
{
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
    epool.emplace_back( fc::resolve_string_to_ip_endpoints(x)[0] );

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

    if (is_exception_occured && ++address.second == MAX_RETRIES)
      wlog("address ${addr} exceeded max amount of failures, no future requests will be sent there",
            ("addr", address.first));
    else address.second = 0;
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
