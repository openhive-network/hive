#pragma once
#include <hive/chain/detail/state/decline_voting_rights_request_object.hpp>

namespace hive { namespace chain {

  struct by_account;
  struct by_effective_date;
  typedef multi_index_container<
    decline_voting_rights_request_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< decline_voting_rights_request_object, decline_voting_rights_request_object::id_type, &decline_voting_rights_request_object::get_id > >,
      ordered_unique< tag< by_account >,
        member< decline_voting_rights_request_object, account_name_type, &decline_voting_rights_request_object::account >
      >,
      ordered_unique< tag< by_effective_date >,
        composite_key< decline_voting_rights_request_object,
          member< decline_voting_rights_request_object, time_point_sec, &decline_voting_rights_request_object::effective_date >,
          member< decline_voting_rights_request_object, account_name_type, &decline_voting_rights_request_object::account >
        >,
        composite_key_compare< std::less< time_point_sec >, std::less< account_name_type > >
      >
    >,
    multi_index_allocator< decline_voting_rights_request_object >
  > decline_voting_rights_request_index;

} } // hive::chain

CHAINBASE_SET_INDEX_TYPE( hive::chain::decline_voting_rights_request_object, hive::chain::decline_voting_rights_request_index )
