#pragma once

#include <hive/chain/database.hpp>

namespace hive { namespace chain {

class external_storage_snapshot
{
  public:

    using ptr = std::shared_ptr<external_storage_snapshot>;

    virtual void supplement_snapshot( const hive::chain::prepare_snapshot_supplement_notification& note ) = 0;
    virtual void load_additional_data_from_snapshot( const hive::chain::load_snapshot_supplement_notification& note ) = 0;

};

}}
