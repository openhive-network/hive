
#pragma once

#include <hive/chain/external_storage/comments_handler.hpp>
#include <hive/chain/external_storage/external_storage_snapshot.hpp>
#include <hive/chain/external_storage/external_storage_provider.hpp>

namespace hive { namespace chain {

class external_storage_processor: public comments_handler, public external_storage_snapshot
{
  public:

    using ptr = std::shared_ptr<external_storage_processor>;

    virtual void move_to_external_storage( uint32_t block_num ) = 0;
    virtual void shutdown( bool remove_db = false ) = 0;
};

} } // hive::chain
