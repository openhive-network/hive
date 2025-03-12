
#pragma once

#include <hive/chain/hive_object_types.hpp>

namespace hive { namespace chain {

class external_storage_provider
{
  public:

    using ptr = std::shared_ptr<external_storage_provider>;

    virtual void store_comment( const comment_id_type& comment_id, uint32_t block_number ) = 0;
    virtual void comment_was_paid( const comment_cashout_object& comment_cashout ) = 0;
};

} } // hive::chain
