#pragma once

#include <hive/chain/external_storage/external_storage_processor.hpp>
#include <hive/chain/external_storage/external_storage_provider.hpp>
#include <hive/chain/external_storage/comment_rocksdb_objects.hpp>

namespace hive { namespace chain {

class rocksdb_storage_processor: public external_storage_processor
{
  private:

    const size_t volatile_objects_limit = 800'000;

    database& db;

    external_storage_provider::ptr provider;

    void move_to_external_storage_impl( uint32_t block_num, const volatile_comment_object& volatile_object );
    std::shared_ptr<comment_object> get_comment_impl( const account_id_type& author, const std::string& permlink ) const;

  public:

    rocksdb_storage_processor( database& db, const external_storage_provider::ptr& provider );

    void store_comment( const account_name_type& author, const std::string& permlink ) override;
    void delete_comment( const account_name_type& author, const std::string& permlink ) override;
    void allow_move_to_external_storage( const account_id_type& account_id, const shared_string& permlink ) override;
    void move_to_external_storage( uint32_t block_num ) override;

    comment get_comment( const account_id_type& author, const std::string& permlink, bool comment_is_required ) const override;
    comment get_comment( const account_name_type& author, const std::string& permlink, bool comment_is_required ) const override;

};

}}
