#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <hive/chain/hive_object_types.hpp>

namespace hive { namespace chain {

class decline_voting_rights_request_object : public object< decline_voting_rights_request_object_type, decline_voting_rights_request_object >
{
  CHAINBASE_OBJECT( decline_voting_rights_request_object );
public:
  CHAINBASE_DEFAULT_CONSTRUCTOR( decline_voting_rights_request_object )

  account_name_type account;
  time_point_sec    effective_date;

  CHAINBASE_UNPACK_CONSTRUCTOR( decline_voting_rights_request_object );
};

} } // hive::chain

FC_REFLECT( hive::chain::decline_voting_rights_request_object,
          (id)(account)(effective_date) )
