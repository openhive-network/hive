
#pragma once

#include <hive/chain/external_storage/comment.hpp>

#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/comment_object.hpp>

namespace hive { namespace chain {

class comments_handler
{
  public:

    using ptr = std::shared_ptr<comments_handler>;

    virtual void allow_move_to_external_storage( const comment_id_type& comment_id, const account_id_type& account_id, const std::string& permlink ) = 0;

    virtual comment get_comment( const account_id_type& author, const std::string& permlink, bool comment_is_required ) const = 0;
    virtual comment get_comment( const account_name_type& author, const std::string& permlink, bool comment_is_required ) const = 0;

};

} } // hive::chain
