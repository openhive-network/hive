#pragma once

#include <hive/chain/external_storage/external_storage_provider.hpp>
#include <hive/chain/external_storage/external_storage_mgr.hpp>

namespace hive { namespace chain {

class rocksdb_storage_provider: public external_storage_provider
{
  private:

    external_storage_mgr::ptr mgr;

  public:

    rocksdb_storage_provider( const external_storage_mgr::ptr& mgr );

    void store_comment( const hive::protocol::comment_operation& op ) override;
    void comment_was_paid( const comment_id_type& comment_id, const account_id_type& account_id, const shared_string& permlink ) override;
    void move_to_external_storage( const volatile_comment_object& volatile_object ) override;
    std::shared_ptr<comment_object> find_comment( const account_id_type& author, const std::string& permlink ) override;
};

}}
