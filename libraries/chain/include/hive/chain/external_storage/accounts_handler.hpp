
#pragma once

#include <hive/chain/external_storage/external_storage_snapshot.hpp>
#include <hive/chain/external_storage/account_item.hpp>

#include <hive/chain/account_object.hpp>

#include <type_traits>

namespace hive { namespace chain {

/// Compile-time trait: true for object types managed by accounts_handler (RocksDB archival).
/// Only these types are routed through the accounts_handler in database::create/modify;
/// all other types go directly to chainbase, avoiding virtual dispatch overhead.
template<typename T>
struct is_accounts_handler_object : std::false_type {};

template<> struct is_accounts_handler_object<account_metadata_object> : std::true_type {};
template<> struct is_accounts_handler_object<account_authority_object> : std::true_type {};
template<> struct is_accounts_handler_object<account_object> : std::true_type {};

template<typename T>
inline constexpr bool is_accounts_handler_object_v = is_accounts_handler_object<T>::value;

struct executor_interface
{
  virtual void create_object( const account_metadata_object& obj ) = 0;
  virtual void create_object( const account_authority_object& obj ) = 0;
  virtual void create_object( const account_object& obj ) = 0;

  /// Called after chainbase::modify completes for account handler objects.
  /// Allows the handler to perform post-modify work (e.g. tiny_account sync, stats).
  virtual void on_object_modified( const account_metadata_object& obj ) = 0;
  virtual void on_object_modified( const account_authority_object& obj ) = 0;
  virtual void on_object_modified( const account_object& obj ) = 0;
};

class accounts_handler : public executor_interface, public external_storage_snapshot
{
  public:

    using ptr = std::shared_ptr<accounts_handler>;

    virtual void on_irreversible_block( uint32_t block_num ) = 0;

    void create( const account_metadata_object& obj ) { create_object( obj ); }
    void create( const account_authority_object& obj ) { create_object( obj ); }
    void create( const account_object& obj ) { create_object( obj ); }
    template<typename ObjectType>
    void create( const ObjectType& ) {} // no-op for non-handler types

    virtual const account_metadata_object* get_account_metadata( const account_name_type& account_name, bool account_metadata_is_required ) const = 0;
    virtual const account_authority_object* get_account_authority( const account_name_type& account_name, bool account_authority_is_required ) const = 0;
    virtual const account_object* get_account( const account_name_type& account_name, bool account_is_required ) const = 0;
    virtual const account_object* get_account( const account_id_type& account_id, bool account_is_required ) const = 0;

    virtual const assets_object* get_asset_account( const account_id_type& account_id, bool is_required ) const = 0;
    virtual const time_object* get_time_account( const account_id_type& account_id, bool is_required ) const = 0;
    virtual const recovery_object* get_recovery_account( const account_id_type& account_id, bool is_required ) const = 0;
    virtual const manabars_rc_object* get_manabars_rc_account( const account_id_type& account_id, bool is_required ) const = 0;
    virtual const delayed_votes_object* get_delayed_votes_account( const account_id_type& account_id, bool is_required ) const = 0;

    virtual account_metadata get_volatile_account_metadata( const account_name_type& account_name, bool account_metadata_is_required ) const = 0;
    virtual account_authority get_volatile_account_authority( const account_name_type& account_name, bool account_authority_is_required ) const = 0;
    virtual account get_volatile_account( const account_name_type& account_name, bool account_is_required ) const = 0;
    virtual account get_volatile_account( const account_id_type& account_id, bool account_is_required ) const = 0;

    virtual void open() = 0;
    virtual void close() = 0;
    virtual void wipe() = 0;

    virtual void remove_objects_limit() = 0;
};

} } // hive::chain
