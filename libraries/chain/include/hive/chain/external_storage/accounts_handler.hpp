
#pragma once

#include <hive/chain/external_storage/external_storage_snapshot.hpp>
#include <hive/chain/external_storage/account_iterator.hpp>

#include <hive/utilities/benchmark_dumper.hpp>

namespace hive { namespace chain {

struct executor_interface
{
  virtual void create_or_update_volatile( const account_metadata_object& obj ) = 0;
  virtual void create_or_update_volatile( const account_authority_object& obj ) = 0;
  virtual void create_or_update_volatile( const account_object& obj ) = 0;

  virtual void modify_object( const account_metadata_object& obj, std::function<void(account_metadata_object&)>&& modifier ) = 0;
  virtual void modify_object( const account_authority_object& obj, std::function<void(account_authority_object&)>&& modifier ) = 0;
  virtual void modify_object( const account_object& obj, std::function<void(account_object&)>&& modifier ) = 0;
};

namespace
{
  template<typename ObjectType>
  struct executor
  {
    void create( const ObjectType& obj, executor_interface& exec ) {}

    template<typename Modifier>
    bool modify( const ObjectType& obj, Modifier&& m, executor_interface& exec ) { return false; }
  };

  template<>
  struct executor<account_metadata_object>
  {
    void create( const account_metadata_object& obj, executor_interface& exec )
    {
      exec.create_or_update_volatile( obj );
    }

    template<typename Modifier>
    bool modify( const account_metadata_object& obj, Modifier&& m, executor_interface& exec )
    {
      exec.modify_object( obj, std::function<void(account_metadata_object&)>( m ) );
      return true;
    }
  };

  template<>
  struct executor<account_authority_object>
  {
    void create( const account_authority_object& obj, executor_interface& exec )
    {
      exec.create_or_update_volatile( obj );
    }

    template<typename Modifier>
    bool modify( const account_authority_object& obj, Modifier&& m, executor_interface& exec )
    {
      exec.modify_object( obj, std::function<void(account_authority_object&)>( m ) );
      return true;
    }
  };

  template<>
  struct executor<account_object>
  {
    void create( const account_object& obj, executor_interface& exec )
    {
      exec.create_or_update_volatile( obj );
    }

    template<typename Modifier>
    bool modify( const account_object& obj, Modifier&& m, executor_interface& exec )
    {
      exec.modify_object( obj, std::function<void(account_object&)>( m ) );
      return true;
    }
  };
}

class accounts_handler : public executor_interface, public external_storage_snapshot
{
  protected:

    virtual const chainbase::database& get_db() const = 0;
    virtual rocksdb_account_column_family_iterator_provider::ptr get_rocksdb_account_column_family_iterator_provider() = 0;
    virtual external_storage_reader_writer::ptr get_external_storage_reader_writer() = 0;

  public:

    template<typename ByIndex>
    std::shared_ptr<account_iterator<ByIndex>> get_iterator()
    {
      return std::make_shared<account_iterator<ByIndex>>( get_db(), get_rocksdb_account_column_family_iterator_provider(), get_external_storage_reader_writer() );
    }

    using ptr = std::shared_ptr<accounts_handler>;

    virtual void on_irreversible_block( uint32_t block_num ) = 0;

    template<typename ObjectType>
    void create( const ObjectType& obj )
    {
      executor<ObjectType>().create( obj, *this );
    }

    template<typename ObjectType, typename Modifier>
    bool modify( const ObjectType& obj, Modifier&& m )
    {
      return executor<ObjectType>().template modify<Modifier>( obj, m, *this );
    }

    virtual account_metadata get_account_metadata( const account_name_type& account_name ) const = 0;
    virtual account_authority get_account_authority( const account_name_type& account_name ) const = 0;
    virtual account get_account( const account_name_type& account_name, bool account_is_required ) const = 0;
    virtual account get_account( const account_id_type& account_id, bool account_is_required ) const = 0;

    virtual void open() = 0;
    virtual void close() = 0;
    virtual void wipe() = 0;

    static hive::utilities::benchmark_dumper::account_archive_details_t stats; // note: times should be measured in nanoseconds
};

} } // hive::chain
