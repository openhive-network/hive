
#pragma once

#include <hive/chain/hive_object_types.hpp>

namespace hive { namespace chain {

class external_storage_finder
{
  public:

    using ptr = std::shared_ptr<external_storage_finder>;

    virtual std::shared_ptr<comment_object> find_comment( const account_id_type& author, const std::string& permlink ) = 0;
};

} } // hive::chain
