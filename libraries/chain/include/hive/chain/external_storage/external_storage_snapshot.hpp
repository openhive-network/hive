#pragma once

#include <memory>

namespace hive { namespace chain {

struct prepare_snapshot_supplement_notification;
struct load_snapshot_supplement_notification;

class external_storage_snapshot
{
  public:

    using ptr = std::shared_ptr<external_storage_snapshot>;

    virtual void save_snaphot( const prepare_snapshot_supplement_notification& note ) = 0;
    virtual void load_snapshot( const load_snapshot_supplement_notification& note ) = 0;
};

}}
