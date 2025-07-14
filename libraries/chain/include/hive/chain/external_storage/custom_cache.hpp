#pragma once

#include <hive/chain/account_object.hpp>

#ifndef HIVE_ACCOUNT_ROCKSDB_SPACE_ID
#define HIVE_ACCOUNT_ROCKSDB_SPACE_ID 24
#endif

namespace hive { namespace chain {

  template<typename Object_Type>
  class custom_cache
  {
    public:

      using items_with_name_type = std::map<account_name_type, std::weak_ptr<Object_Type>>;
      using items_with_id_type = std::map<account_id_type, std::weak_ptr<Object_Type>>;

    private:

      static items_with_name_type items_with_name;
      static items_with_id_type   items_with_id;

      template<typename Key_Type, typename Items>
      static void add_internal( std::shared_ptr<Object_Type> obj, const Key_Type& key, Items& current_items )
      {
        auto _found = current_items.find( key );
        if( _found == current_items.end() )
        {
          current_items.emplace( key, obj );
        }
      }

      template<typename Key_Type, typename Items>
      static std::shared_ptr<Object_Type> get_internal( const Key_Type& key, Items& current_items )
      {
        auto _found = current_items.find( key );
        if( _found != current_items.end() )
          return _found->second.lock();
        else
          return std::shared_ptr<Object_Type>();
      }

      template<typename Key_Type, typename Items>
      static void erase_internal( const Key_Type& key, Items& current_items )
      {
        auto _found = current_items.find( key );

        if( _found != current_items.end() )
        {
          current_items.erase( _found );
        }
      }

    public:

      static void add( std::shared_ptr<Object_Type> obj )
      {
        if( !obj )
          return;

        add_internal( obj, obj->get_name(), items_with_name );
      }

      static void add_complex( std::shared_ptr<Object_Type> obj )
      {
        if( !obj )
          return;

        add_internal( obj, obj->get_name(), items_with_name );
        add_internal( obj, obj->get_id(), items_with_id );
      }

      static std::shared_ptr<Object_Type> get( const account_id_type& id )
      {
        return get_internal( id, items_with_id );
      }

      static std::shared_ptr<Object_Type> get( const account_name_type& name )
      {
        return get_internal( name, items_with_name );
      }

      static void erase( const account_name_type& name )
      {
        erase_internal( name, items_with_name );
      }

      static void erase_complex( const account_name_type& name, const account_id_type& id )
      {
        erase_internal( name, items_with_name );
        erase_internal( id, items_with_id );
      }

  };

  template<typename Object_Type>
  typename custom_cache<Object_Type>::items_with_name_type custom_cache<Object_Type>::items_with_name;

  template<typename Object_Type>
  typename custom_cache<Object_Type>::items_with_id_type custom_cache<Object_Type>::items_with_id;

  template<typename Object_Type>
  class custom_deleter
  {
    public:

      void operator()( Object_Type* ptr )
      {
        custom_cache<Object_Type>::erase( ptr->get_name() );

        delete ptr;
      }
  };

  template<>
  class custom_deleter<account_object>
  {
    public:

      void operator()( account_object* ptr )
      {
        custom_cache<account_object>::erase_complex( ptr->get_name(), ptr->get_id() );

        delete ptr;
      }
  };

} } // hive::chain
