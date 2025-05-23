
#pragma once

#include <hive/chain/external_storage/comment.hpp>

#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/comment_object.hpp>

namespace hive { namespace chain {

class comments_handler
{
  public:

    using ptr = std::shared_ptr<comments_handler>;

    virtual void on_cashout( const comment_object& _comment, const comment_cashout_object& _comment_cashout ) = 0;
    virtual void on_irreversible_block( uint32_t block_num ) = 0;

    virtual comment get_comment( const account_id_type& author, const std::string& permlink, bool comment_is_required ) const = 0;

};

} } // hive::chain
