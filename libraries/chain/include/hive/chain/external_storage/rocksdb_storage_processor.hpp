#pragma once

#include <hive/chain/external_storage/external_storage_processor.hpp>
#include <hive/chain/external_storage/external_storage_provider.hpp>

namespace hive { namespace chain {

class rocksdb_storage_processor: public external_storage_processor
{
  private:

    database& db;

    external_storage_provider::ptr provider;

    void move_to_external_storage_impl( uint32_t block_num, const volatile_comment_object& volatile_object );

  public:

    rocksdb_storage_processor( database& db, const external_storage_provider::ptr& provider );

    void store_comment( const account_name_type& author, const std::string& permlink ) override;
    void delete_comment( const account_name_type& author, const std::string& permlink ) override;
    void comment_was_paid( const account_id_type& account_id, const shared_string& permlink ) override;
    void move_to_external_storage( uint32_t block_num ) override;
    std::shared_ptr<comment_object> find_comment( const account_id_type& author, const std::string& permlink ) override;
};

}}
