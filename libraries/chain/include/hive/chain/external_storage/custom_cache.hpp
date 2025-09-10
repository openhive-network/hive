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

      using items_type = std::map<account_name_type, std::weak_ptr<Object_Type>>;

    private:

      static items_type items;

    public:

      static void add( std::shared_ptr<Object_Type> obj )
      {
        if( !obj )
          return;

        auto _found = items.find( obj->get_name() );
        if( _found == items.end() )
        {
          items.emplace( obj->get_name(), obj );
        }
      }

      static std::shared_ptr<Object_Type> get( const account_id_type& name )
      {
        return std::shared_ptr<Object_Type>();
      }

      static std::shared_ptr<Object_Type> get( const account_name_type& name )
      {
        auto _found = items.find( name );
        if( _found != items.end() )
          return _found->second.lock();
        else
          return std::shared_ptr<Object_Type>();
      }

      static void erase( const account_name_type& name )
      {
        auto _found = items.find( name );

        if( _found != items.end() )
          items.erase( _found );
      }
  };

  template<typename Object_Type>
  typename custom_cache<Object_Type>::items_type custom_cache<Object_Type>::items;

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


} } // hive::chain
