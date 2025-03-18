
#pragma once

#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/external_storage/comment_rocksdb_objects.hpp>

namespace hive { namespace chain {

class external_storage_provider
{
  public:

    using ptr = std::shared_ptr<external_storage_provider>;

    virtual void store_comment( const hive::protocol::comment_operation& op ) = 0;
    virtual void comment_was_paid( const comment_cashout_object& comment_cashout ) = 0;
    virtual void move_to_external_storage( const volatile_comment_object& volatile_object ) = 0;
    virtual std::shared_ptr<comment_object> find_comment( const account_id_type& author, const std::string& permlink ) = 0;
};

} } // hive::chain
