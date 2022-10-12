#include <hive/utilities/notifications.hpp>

#include <fc/io/json.hpp>
#include <fc/log/logger.hpp>
#include <fc/network/http/connection.hpp>
#include <fc/network/resolve.hpp>

namespace hive { namespace utilities { namespace notifications {

namespace flags{
  const char* notifiations_endpoint()
  {
    return "notifications-endpoint";
  }

  const char* notifiations_filter()
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
  return args.count( flags::notifiations_endpoint() ) > 0;
}

void add_program_options(boost::program_options::options_description& options)
{
  options.add_options()
  (
    flags::notifiations_endpoint(),
    boost::program_options::value< std::vector<fc::string> >()->multitoken(),
    "list of addresses, that will receive notification about in-chain events"
  )
  (
    flags::notifiations_filter(),
    boost::program_options::value< fc::string >()->default_value("!timer"),
    "notification is accepted if name matches given regular expression, if not specified all notifications are accepted"
  );
}

void setup_notifications(const boost::program_options::variables_map &args)
{
  if( !check_is_notifications_enabled(args) ) return;
  const auto& address_pool = args[ flags::notifiations_endpoint() ].as<std::vector<fc::string>>();

  std::vector<fc::ip::endpoint> epool;
  epool.reserve(address_pool.size());

  for(const fc::string& x : address_pool)
    epool.emplace_back( fc::resolve_string_to_ip_endpoints(x)[0] );

  hive::utilities::notifications::get_notification_handler_instance().setup(
    epool, args[ flags::notifiations_filter() ].as<fc::string>()
  );
}

namespace detail
{
bool error_handler(const std::function<void ()> &foo)
{
  bool is_exception_occured = false;
  auto handle_exception = [&is_exception_occured](const fc::string &ex)
  {
    is_exception_occured = true;
    wlog("caught exception! details:\n${details}", ("details", ex));
  };

  try { foo(); }
  catch(const fc::assert_exception& ex){ handle_exception(ex.to_detail_string()); }
  catch(const fc::exception& ex){ handle_exception(ex.to_string()); }
  catch(const std::exception& ex){ handle_exception(ex.what()); }
  catch(...){ handle_exception("unknown exception"); }
  return !is_exception_occured;
}

notification_handler &instance()
{
  static notification_handler instance{};
  return instance;
}

void notification_handler::setup(const std::vector<fc::ip::endpoint> &address_pool, const fc::string& regex)
{
  if (address_pool.empty()) network.reset();
  else
  {
    const size_t ap_size = address_pool.size();
    ilog("setting up notification handler for ${count} address${fix}", ("count", ap_size)( "fix", (ap_size > 1 ? "es" : "") ));

    network = std::make_unique<network_broadcaster>(address_pool);

    if (!regex.empty() && !(regex.size() == 1 && regex[0] == notification_handler::exclude_matching_sign))
    {
      this->exclude_matching = (regex[0] == notification_handler::exclude_matching_sign);
      this->name_filter = std::regex(regex.substr(static_cast<size_t>(this->exclude_matching)));
    }

    futures.reserve(max_amount_of_futures);
  }
}

bool notification_handler::is_broadcasting_active() const
{
  return network.get() != nullptr;
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

void scope_guarded_timer::send_notif()
{
  hive::notify("timer",
//{
    "name",     this->timer_name,
    "unit",     "ms",
    "duration", (fc::time_point::now() - this->start).count() / 1000 // ms
//}
  );
}

void scope_guarded_timer::reset()
{
  send_notif();
  this->start = fc::time_point::now();
}

scope_guarded_timer::~scope_guarded_timer()
{
  send_notif();
}

} // detail

detail::notification_handler &get_notification_handler_instance() { return detail::instance(); }

} // notifications
} // utilities

void notify_hived_status(const fc::string& current_status) noexcept
{
  notify("hived_status", "current_status", current_status);
}

void notify_hived_error(const fc::string& error_message) noexcept
{
  notify("error", "message", error_message);
}

utilities::notifications::detail::scope_guarded_timer notify_hived_timer(const fc::string &timer_name) noexcept
{
  return utilities::notifications::detail::scope_guarded_timer{ timer_name };
}

} // hive
