
#pragma once

#include <hive/chain/external_storage/external_storage_snapshot.hpp>
#include <hive/chain/external_storage/external_storage_provider.hpp>

#include <hive/chain/account_object.hpp>

namespace hive { namespace chain {

struct executor_interface
{
  virtual void create_object( const account_metadata_object& obj ) = 0;
  virtual void create_object( const account_authority_object& obj ) = 0;
  virtual void create_object( const account_object& obj ) = 0;

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
      exec.create_object( obj );
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
      exec.create_object( obj );
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
      exec.create_object( obj );
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
    virtual external_storage_reader_writer::ptr get_external_storage_reader_writer() const = 0;

  public:

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

    virtual const account_metadata_object* get_account_metadata( const account_name_type& account_name, bool account_metadata_is_required ) const = 0;
    virtual const account_authority_object* get_account_authority( const account_name_type& account_name, bool account_authority_is_required ) const = 0;
    virtual const account_object* get_account( const account_name_type& account_name, bool account_is_required ) const = 0;
    virtual const account_object* get_account( const account_id_type& account_id, bool account_is_required ) const = 0;

    virtual void open() = 0;
    virtual void close() = 0;
    virtual void wipe() = 0;

  #ifdef IS_TEST_NET
    virtual void set_limit( size_t new_limit ) = 0;
  #endif
};

} } // hive::chain
