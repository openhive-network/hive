#pragma once

#include <hive/protocol/types.hpp>
#include <fc/static_variant.hpp>

namespace hive { namespace protocol {

  struct beneficiary_route_type
  {
    beneficiary_route_type() {}
    beneficiary_route_type( const account_name_type& a, const uint16_t& w ) : account( a ), weight( w ){}

    account_name_type account;
    uint16_t          weight;

    // For use by std::sort such that the route is sorted first by name (ascending)
    bool operator < ( const beneficiary_route_type& o )const { return account < o.account; }
  };

#ifdef HIVE_ENABLE_SMT
  struct votable_asset_info_v1
  {
    votable_asset_info_v1() = default;
    votable_asset_info_v1(const share_type& max_payout, bool allow_rewards) :
      max_accepted_payout(max_payout), allow_curation_rewards(allow_rewards) {}

    share_type        max_accepted_payout    = 0;
    bool              allow_curation_rewards = false;
  };

  typedef static_variant< votable_asset_info_v1 > votable_asset_info;
#endif /// HIVE_ENABLE_SMT

} } // hive::protocol

FC_REFLECT( hive::protocol::beneficiary_route_type, (account)(weight) )

#ifdef HIVE_ENABLE_SMT
FC_REFLECT( hive::protocol::votable_asset_info_v1, (max_accepted_payout)(allow_curation_rewards) )
#endif
