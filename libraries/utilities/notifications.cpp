#include <hive/utilities/notifications.hpp>

#include <fc/io/json.hpp>
#include <fc/log/logger.hpp>
#include <fc/network/http/connection.hpp>
#include <fc/network/resolve.hpp>

namespace hive { namespace utilities { namespace notifications {

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

void setup_notifications(const boost::program_options::variables_map &args)
{
  if( !check_is_flag_set(args) ) return;
  const auto& address_pool = args[ get_flag() ].as<std::vector<fc::string>>();

  std::vector<fc::ip::endpoint> epool;
  epool.reserve(address_pool.size());

  for(const fc::string& x : address_pool)
    epool.emplace_back( fc::resolve_string_to_ip_endpoints(x)[0] );

  hive::utilities::notifications::get_notification_handler_instance().setup(epool);
}

namespace detail
{
bool error_handler(std::function<void ()> foo)
{
  bool is_exception_occured = false;
  auto handle_exception = [&is_exception_occured](const fc::string &ex)
  {
    is_exception_occured = true;
    wlog("cought exception! details:\n${details}", ("details", ex));
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

void notification_handler::setup(const std::vector<fc::ip::endpoint> &address_pool)
{
  if (address_pool.empty()) network.reset();
  else
  {
    const size_t ap_size = address_pool.size();
    ilog("setting up notification handler for ${count} address${fix}", ("count", ap_size)( "fix", (ap_size > 1 ? "es" : "") ));

    network = std::make_unique<network_broadcaster>(address_pool, on_send);
  }
}

void notification_handler::broadcast(const fc::string &notifi_name)
{
  if (!is_broadcasting_active())
    return;

  on_send(notification_t{notifi_name});
}

void notification_handler::broadcast_impl(notification_handler::variant_map_t &object, const fc::string &key, const fc::variant &var, notification_handler::___null_t)
{
  add_variant(object, key, var);
}

void notification_handler::add_variant(notification_handler::variant_map_t &object, const fc::string &key, const fc::variant &var)
{
  auto it = object.emplace(key, var);
  FC_ASSERT( it.second, "duplicated key in map" );
}

bool notification_handler::is_broadcasting_active() const
{
  return network.get() != nullptr;
}

void notification_handler::network_broadcaster::broadcast(const notification_t &ev) noexcept
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
        con->request("PUT", "", fc::json::to_string(ev));
      }
    });

    if (is_exception_occured && ++address.second == MAX_RETRIES)
      wlog("address ${addr} exceeded max amount of failures, no future requests will be sent there",
            ("addr", address.first));
    else address.second = 0;
  }
}
} // detail

detail::notification_handler &get_notification_handler_instance() { return detail::instance(); }

} // notifications
} // utilities

void notify_hived_status(const fc::string& current_status) noexcept
{
  notify("hived_status", "current_status", current_status);
}

} // hive
