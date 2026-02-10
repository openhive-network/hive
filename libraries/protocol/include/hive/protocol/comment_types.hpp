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

} } // hive::protocol

FC_REFLECT( hive::protocol::beneficiary_route_type, (account)(weight) )
