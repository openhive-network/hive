#pragma once

#include<hive/chain/database.hpp>

namespace hive { namespace chain {

class external_storage_connector
{
  public:

    using ptr = std::shared_ptr<external_storage_connector>;

  protected:

    database& db;

  public:

    external_storage_connector( database& db ):db( db ){}
    virtual ~external_storage_connector(){}
};

}}
