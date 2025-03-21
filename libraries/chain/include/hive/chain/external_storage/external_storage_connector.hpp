#pragma once

#include <hive/chain/external_storage/external_storage_processor.hpp>

namespace hive { namespace chain {

class external_storage_connector
{
  public:

    using ptr = std::shared_ptr<external_storage_connector>;

  protected:

    external_storage_processor::ptr processor;

  public:

    external_storage_connector( external_storage_processor::ptr processor ):processor( processor ){}
    virtual ~external_storage_connector(){}
};

}}
