
#pragma once

#include <hive/chain/external_storage/comment.hpp>

#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/comment_object.hpp>

namespace hive { namespace chain {

class external_storage_finder
{
  public:

    using ptr = std::shared_ptr<external_storage_finder>;

    virtual comment get_comment( const account_id_type& author, const std::string& permlink, bool comment_is_required ) const = 0;
    virtual comment get_comment( const account_name_type& author, const std::string& permlink, bool comment_is_required ) const = 0;

};

} } // hive::chain
