#pragma once

#include <hive/chain/external_storage/external_storage_provider.hpp>
#include <hive/chain/external_storage/external_storage_mgr.hpp>

namespace hive { namespace chain {

class rocksdb_storage_provider: public external_storage_provider
{
  private:

    external_storage_mgr::ptr mgr;

  public:

    rocksdb_storage_provider( const external_storage_mgr::ptr& mgr ): mgr( mgr )
    {
    }

    void store_comment( const comment_id_type& comment_id, uint32_t block_number ) override;
    void comment_was_paid( const comment_cashout_object& comment_cashout ) override;
    void move_to_external_storage( const volatile_comment_object& volatile_object ) override;
};

}}
