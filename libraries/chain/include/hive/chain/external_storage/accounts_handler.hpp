
#pragma once

#include <hive/chain/external_storage/external_storage_snapshot.hpp>
#include <hive/chain/external_storage/account_item.hpp>

#include <hive/chain/hive_object_types.hpp>

#include <hive/utilities/benchmark_dumper.hpp>

namespace hive { namespace chain {

class accounts_handler : public external_storage_snapshot
{
  public:

    using ptr = std::shared_ptr<accounts_handler>;

    virtual void on_irreversible_block( uint32_t block_num ) = 0;

    virtual void create_volatile_account_metadata( const account_metadata_object& obj ) = 0;
    virtual account_metadata get_account_metadata( const std::string& account_name ) const = 0;

    virtual void create_volatile_account_authority( const account_authority_object& obj, bool init_genesis = false ) = 0;
    virtual account_authority get_account_authority( const std::string& account_name ) const = 0;

    virtual void open() = 0;
    virtual void close() = 0;
    virtual void wipe() = 0;

    static hive::utilities::benchmark_dumper::account_archive_details_t stats; // note: times should be measured in nanoseconds
};

} } // hive::chain
