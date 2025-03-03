
#pragma once

#include <hive/chain/external_storage/comments_handler.hpp>
#include <hive/chain/external_storage/external_storage_snapshot.hpp>
#include <hive/chain/external_storage/external_storage_provider.hpp>

namespace hive { namespace chain {

class external_storage_processor: public comments_handler, public external_storage_snapshot
{
  public:

    using ptr = std::shared_ptr<external_storage_processor>;

    virtual void allow_move_to_external_storage( const comment_id_type& comment_id, const account_id_type& account_id, const std::string& permlink ) = 0;
    virtual void move_to_external_storage( uint32_t block_num ) = 0;
};

} } // hive::chain
