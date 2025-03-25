
#pragma once

#include <hive/chain/external_storage/external_storage_finder.hpp>

namespace hive { namespace chain {

class external_storage_processor: public external_storage_finder
{
  public:

    using ptr = std::shared_ptr<external_storage_processor>;

    virtual void store_comment( const account_name_type& author, const std::string& permlink ) = 0;
    virtual void delete_comment( const account_name_type& author, const std::string& permlink ) = 0;
    virtual void allow_move_to_external_storage( const account_id_type& account_id, const shared_string& permlink ) = 0;
    virtual void move_to_external_storage( uint32_t block_num ) = 0;
};

} } // hive::chain
