#pragma once
#include <hive/protocol/block.hpp>

#include <hive/plugins/p2p/p2p_plugin.hpp>
#include <hive/plugins/chain/chain_plugin.hpp>
#include <hive/plugins/json_rpc/utility.hpp>

#include <graphene/net/core_messages.hpp>

#include <fc/optional.hpp>
#include <fc/variant.hpp>
#include <fc/vector.hpp>
#include <fc/network/ip.hpp>

#include <boost/thread/mutex.hpp>

namespace hive { namespace plugins { namespace witness_api {

using std::vector;
using fc::variant;
using fc::optional;
using hive::plugins::json_rpc::void_type;
using hive::plugins::p2p::api_peer_status;

/* enable_fast_confirm */

typedef void_type          enable_fast_confirm_args;
typedef void_type          enable_fast_confirm_return;

/* disable_fast_confirm */

typedef void_type          disable_fast_confirm_args;
typedef void_type          disable_fast_confirm_return;

/* is_fast_confirm_enabled */

typedef void_type          is_fast_confirm_enabled_args;
typedef bool               is_fast_confirm_enabled_return;


namespace detail{ class witness_api_impl; }

class witness_api
{
   public:
      witness_api();
      ~witness_api();

      DECLARE_API(
         (enable_fast_confirm)
         (disable_fast_confirm)
         (is_fast_confirm_enabled)
      )

   private:
      std::unique_ptr< detail::witness_api_impl > my;
};

} } } // hive::plugins::witness_api

