#pragma once

#include <core/beekeeper_wallet_manager.hpp>
#include <core/utilities.hpp>

#include <hive/plugins/json_rpc/utility.hpp>

#include <fc/optional.hpp>
#include <fc/variant.hpp>
#include <fc/vector.hpp>

namespace appbase
{
  class application;
}

namespace beekeeper {

namespace detail
{
  class beekeeper_api_impl;
}

class beekeeper_wallet_api
{
  public:
    beekeeper_wallet_api( std::shared_ptr<beekeeper::beekeeper_wallet_manager> wallet_mgr, appbase::application& app, uint64_t unlock_interval );
    ~beekeeper_wallet_api();

    DECLARE_API(
      (create)
      (open)
      (close)
      (set_timeout)
      (lock_all)
      (lock)
      (unlock)
      (import_key)
      (remove_key)
      (list_wallets)
      (list_created_wallets)
      (get_public_keys)
      (sign_digest)
      (get_info)
      (create_session)
      (close_session)
      (has_matching_private_key)
      (encrypt_data)
      (decrypt_data)
      (get_version)
    )

  private:
    std::unique_ptr< detail::beekeeper_api_impl > my;
};

} // beekeeper
