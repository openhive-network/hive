#pragma once

#include <hive/chain/hive_fwd.hpp>
#include <hive/chain/hive_object_types.hpp>

namespace hive { namespace chain {

  /**
   * time_object is now a minimal shell. All data fields have been merged into assets_object
   * for performance (eliminates redundant get<>/modify() calls).
   * This shell is kept to preserve the time_object_type enum value and object creation sequence.
   */
  class time_object : public object< time_object_type, time_object >
  {
    CHAINBASE_OBJECT( time_object );
    public:
      template< typename Allocator >
      time_object( allocator< Allocator > a, uint64_t _id,
        account_id_type _account_id, const account_name_type& _name = account_name_type() )
        : id( _id ), account_id( _account_id )
      {}

      // Link to parent account_object
      account_id_type get_account_id() const { return account_id; }

    private:
      account_id_type   account_id;               // Links to parent account_object

    CHAINBASE_UNPACK_CONSTRUCTOR(time_object);
  };

  typedef multi_index_container<
    time_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< time_object, time_object::id_type, &time_object::get_id > >
    >,
    multi_index_allocator< time_object >
  > time_index;

} } // hive::chain

FC_REFLECT( hive::chain::time_object,
          (id)(account_id)
        )

CHAINBASE_SET_INDEX_TYPE( hive::chain::time_object, hive::chain::time_index )
