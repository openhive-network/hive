
#pragma once

#include <hive/chain/external_storage/external_storage_snapshot.hpp>
#include <hive/chain/external_storage/account_item.hpp>

#include <hive/chain/hive_object_types.hpp>

#include <hive/utilities/benchmark_dumper.hpp>

namespace hive { namespace chain {

struct executor_interface
{
  virtual void create_volatile_account_metadata( const account_metadata_object& obj ) = 0;
  virtual void create_volatile_account_authority( const account_authority_object& obj ) = 0;

  virtual void modify_account_metadata( const account_name_type& account_name, std::function<void(account_metadata_object&)> modifier ) = 0;
  virtual void modify_account_authority( const account_name_type& account_name, std::function<void(account_authority_object&)> modifier ) = 0;
};

namespace
{
  template<typename ObjectType>
  struct executor
  {
    void create( const ObjectType& obj, executor_interface& exec ) {}

    template<typename Modifier>
    void modify( const account_name_type& account_name, Modifier&& m, executor_interface& exec ) {}
  };

  template<>
  struct executor<account_metadata_object>
  {
    void create( const account_metadata_object& obj, executor_interface& exec )
    {
      exec.create_volatile_account_metadata( obj );
    }

    template<typename Modifier>
    void modify( const account_name_type& account_name, Modifier&& m, executor_interface& exec )
    {
      exec.modify_account_metadata( account_name, std::function<void(account_metadata_object&)>( m ) );
    }
  };

  template<>
  struct executor<account_authority_object>
  {
    void create( const account_authority_object& obj, executor_interface& exec )
    {
      exec.create_volatile_account_authority( obj );
    }

    template<typename Modifier>
    void modify( const account_name_type& account_name, Modifier&& m, executor_interface& exec )
    {
      exec.modify_account_authority( account_name, std::function<void(account_authority_object&)>( m ) );
    }
  };
}

class accounts_handler : public executor_interface, public external_storage_snapshot
{
  public:

    using ptr = std::shared_ptr<accounts_handler>;

    virtual void on_irreversible_block( uint32_t block_num ) = 0;

    template<typename ObjectType>
    void create( const ObjectType& obj )
    {
      executor<ObjectType>().create( obj, *this );
    }

    template<typename ObjectType, typename Modifier>
    void modify( const account_name_type& account_name, Modifier&& m )
    {
      executor<ObjectType>().template modify<Modifier>( account_name, m, *this );
    }

    virtual account_metadata get_account_metadata( const account_name_type& account_name ) const = 0;
    virtual account_authority get_account_authority( const account_name_type& account_name ) const = 0;

    virtual void open() = 0;
    virtual void close() = 0;
    virtual void wipe() = 0;

    static hive::utilities::benchmark_dumper::account_archive_details_t stats; // note: times should be measured in nanoseconds
};

} } // hive::chain
