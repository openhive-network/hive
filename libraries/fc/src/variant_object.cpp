#include <fc/variant_object.hpp>
#include <fc/exception/exception.hpp>
#include <assert.h>


namespace fc
{
   // ---------------------------------------------------------------
   // entry

   variant_object::entry::entry() {}
   variant_object::entry::entry( const string& k, const variant& v ) : _key(k),_value(v) {}
   variant_object::entry::entry( entry&& e ) : _key(fc::move(e._key)),_value(fc::move(e._value)) {}
   variant_object::entry::entry( const entry& e ) : _key(e._key),_value(e._value) {}
   variant_object::entry& variant_object::entry::operator=( const variant_object::entry& e )
   {
      if( this != &e ) 
      {
         _key = e._key;
         _value = e._value;
      }
      return *this;
   }
   variant_object::entry& variant_object::entry::operator=( variant_object::entry&& e )
   {
      fc_swap( _key, e._key );
      fc_swap( _value, e._value );
      return *this;
   }
   
   const string&        variant_object::entry::key()const
   {
      return _key;
   }

   const variant& variant_object::entry::value()const
   {
      return _value;
   }
   variant& variant_object::entry::value()
   {
      return _value;
   }

   void  variant_object::entry::set( const variant& v )
   {
      _value = v;
   }

   // ---------------------------------------------------------------
   // variant_object

   variant_object::iterator variant_object::begin()
   {
      return _key_value->begin();
   }

   variant_object::iterator variant_object::end()
   {
      return _key_value->end();
   }

   variant_object::const_iterator variant_object::begin() const
   {
      return _key_value->begin();
   }

   variant_object::const_iterator variant_object::end() const
   {
      return _key_value->end();
   }

   variant_object::iterator variant_object::find( const string& key )
   {
      return find( key.c_str() );
   }

   variant_object::iterator variant_object::find( const char* key )
   {
      for( auto itr = begin(); itr != end(); ++itr )
      {
         if( itr->key() == key )
         {
            return itr;
         }
      }
      return end();
   }

   variant_object::const_iterator variant_object::find( const string& key )const
   {
      return find( key.c_str() );
   }

   variant_object::const_iterator variant_object::find( const char* key )const
   {
      for( auto itr = begin(); itr != end(); ++itr )
      {
         if( itr->key() == key )
         {
            return itr;
         }
      }
      return end();
   }

   bool variant_object::contains( const string& key ) const
   {
   return find( key.c_str() ) != end();
   }

   bool variant_object::contains( const char* key ) const
   {
   return find( key ) != end();
   }

   const variant& variant_object::operator[]( const string& key )const
   {
      return (*this)[key.c_str()];
   }

   const variant& variant_object::operator[]( const char* key )const
   {
      auto itr = find( key );
      if( itr != end() ) return itr->value();
      FC_THROW_EXCEPTION( key_not_found_exception, "Key ${key}", ("key",key) );
   }
   variant& variant_object::operator[]( const string& key )
   {
      return (*this)[key.c_str()];
   }

   variant& variant_object::operator[]( const char* key )
   {
      auto itr = find( key );
      if( itr != end() ) return itr->value();
      return _key_value->emplace_back(key, variant()).value();
   }

   size_t variant_object::size() const
   {
      return _key_value->size();
   }

   void variant_object::reserve( size_t s )
   {
      _key_value->reserve(s);
   }

   void  variant_object::erase( const string& key )
   {
      for( auto itr = begin(); itr != end(); ++itr )
      {
         if( itr->key() == key )
         {
            _key_value->erase(itr);
            return;
         }
      }
   }

   variant_object::variant_object() 
      :_key_value(std::make_shared<std::vector<entry>>() )
   {
   }

   variant_object::variant_object( const string& key, const variant& val )
      : _key_value(std::make_shared<std::vector<entry>>())
   {
       //_key_value->push_back(entry(fc::move(key), fc::move(val)));
       _key_value->emplace_back(key, val);
   }

   variant_object::variant_object( const variant_object& obj )
   :_key_value( obj._key_value )
   {
      assert( _key_value != nullptr );
   }

   variant_object::variant_object( variant_object&& obj)
   : _key_value( fc::move(obj._key_value) )
   {
      obj._key_value = std::make_shared<std::vector<entry>>();
      assert( _key_value != nullptr );
   }

   variant_object& variant_object::operator=( variant_object&& obj )
   {
      if (this != &obj)
      {
         fc_swap(_key_value, obj._key_value );
         assert( _key_value != nullptr );
      }
      return *this;
   }

   variant_object& variant_object::operator=( const variant_object& obj )
   {
      if (this != &obj)
      {
         _key_value = obj._key_value;
      }
      return *this;
   }

   void to_variant( const variant_object& var,  variant& vo )
   {
      vo = variant(var);
   }

   void from_variant( const variant& var,  variant_object& vo )
   {
      vo = var.get_object();
   }

   /** replaces the value at \a key with \a var or insert's \a key if not found */
   variant_object& variant_object::set( const string& key, const variant& var )
   {
      auto itr = find( key.c_str() );
      if( itr != end() )
      {
         itr->set( var );
      }
      else
      {
         _key_value->emplace_back( key, var );
      }
      return *this;
   }

   /** Appends \a key and \a var without checking for duplicates, designed to
    *  simplify construction of dictionaries using (key,val)(key2,val2) syntax 
    */
   variant_object& variant_object::operator()( const string& key, const variant& var )
   {
      _key_value->emplace_back( key, var );
      return *this;
   }

   variant_object& variant_object::operator()( const variant_object& vo )
   {
      if( &vo == this )     // mvo(mvo) is no-op
         return *this;
      for( const variant_object::entry& e : vo )
         set( e.key(), e.value() );
      return *this;
   }

   void variant_object_builder::validate() const
   {
     std::set< string > keys;
     for( auto& key_value : *buffer._key_value.get() )
     {
       auto insert_info = keys.insert( key_value.key() );
       FC_ASSERT( insert_info.second, "Duplicated key in built variant_object" );
     }
   }

} // namesapce fc
