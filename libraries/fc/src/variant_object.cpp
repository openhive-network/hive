#include <fc/variant_object.hpp>
#include <fc/exception/exception.hpp>
#include <assert.h>


namespace fc
{
   // variant_object

   variant_object::iterator variant_object::begin() const
   {
      return _key_value.begin();
   }

   variant_object::iterator variant_object::end() const
   {
      return _key_value.end();
   }

   variant_object::iterator variant_object::find( const string& key )const
   {
      //time_logger_ex::instance().start("variant_object::iterator variant_object::find( const string& key )const");
      auto res = std::move( _key_value.find( key ) );
      //time_logger_ex::instance().stop();
      return res;
   }

   const variant& variant_object::operator[]( const string& key )const
   {
     variant_object::iterator result = find( key );

     if( result != _key_value.end() )
      return result->second;

     FC_THROW_EXCEPTION( key_not_found_exception, "Key ${key}", ("key",key) );
   }

   size_t variant_object::size() const
   {
      return _key_value.size();
   }

   variant_object::variant_object()
   {
      //time_logger_ex::instance().start("variant_object::variant_object()");
      //time_logger_ex::instance().stop();
   }

   variant_object::variant_object( string key, variant val )
   {
      //time_logger_ex::instance().start("variant_object::variant_object( string key, variant val )");
       _key_value.emplace(std::make_pair(fc::move(key), fc::move(val)));
      //time_logger_ex::instance().stop();
   }

   variant_object::variant_object( const variant_object& obj )
   {
      //time_logger_ex::instance().start("variant_object::variant_object( string key, variant val )");
      _key_value = obj._key_value;
      //time_logger_ex::instance().stop();
   }

   variant_object::variant_object( variant_object&& obj)
   {
      //time_logger_ex::instance().start("variant_object::variant_object( variant_object&& obj)");
      _key_value = fc::move(obj._key_value);
      obj._key_value.clear();
      //time_logger_ex::instance().stop();
   }

   variant_object::variant_object( const mutable_variant_object& obj )
   {
      //time_logger_ex::instance().start("variant_object::variant_object( const mutable_variant_object& obj )");
      _key_value = obj._key_value;
      //time_logger_ex::instance().stop();
   }

   variant_object::variant_object( mutable_variant_object&& obj )
   : _key_value(fc::move(obj._key_value))
   {
   }

   variant_object& variant_object::operator=( variant_object&& obj )
   {
      //time_logger_ex::instance().start("variant_object& variant_object::operator=( variant_object&& obj )");
      if (this != &obj)
      {
         fc_swap(_key_value, obj._key_value );
      }
      //time_logger_ex::instance().stop();
      return *this;
   }

   variant_object& variant_object::operator=( const variant_object& obj )
   {
      //time_logger_ex::instance().start("variant_object& variant_object::operator=( const variant_object& obj )");
      if (this != &obj)
      {
         _key_value = obj._key_value;
      }
      //time_logger_ex::instance().stop();
      return *this;
   }

   variant_object& variant_object::operator=( mutable_variant_object&& obj )
   {
      //time_logger_ex::instance().start("variant_object& variant_object::operator=( mutable_variant_object&& obj )");
      _key_value = fc::move(obj._key_value);
      obj._key_value.clear();
      //time_logger_ex::instance().stop();
      return *this;
   }

   variant_object& variant_object::operator=( const mutable_variant_object& obj )
   {
      //time_logger_ex::instance().start("variant_object& variant_object::operator=( const mutable_variant_object& obj )");
      _key_value = obj._key_value;
      //time_logger_ex::instance().stop();
      return *this;
   }


   void to_variant( const variant_object& var,  variant& vo )
   {
      //time_logger_ex::instance().start("void to_variant( const variant_object& var,  variant& vo )");
      vo = variant(var);
      //time_logger_ex::instance().stop();
   }

   void from_variant( const variant& var,  variant_object& vo )
   {
      vo = var.get_object();
   }

   // ---------------------------------------------------------------
   // mutable_variant_object

   mutable_variant_object::iterator mutable_variant_object::begin()
   {
      return _key_value.begin();
   }

   mutable_variant_object::iterator mutable_variant_object::end() 
   {
      return _key_value.end();
   }

   mutable_variant_object::const_iterator mutable_variant_object::begin() const
   {
      return _key_value.begin();
   }

   mutable_variant_object::const_iterator mutable_variant_object::end() const
   {
      return _key_value.end();
   }

   mutable_variant_object::const_iterator mutable_variant_object::find( const string& key )const
   {
      //time_logger_ex::instance().start("mutable_variant_object::iterator mutable_variant_object::find( const string& key )const");
      auto res = std::move( _key_value.find( key ) );
      //time_logger_ex::instance().stop();
      //time_logger_ex::instance().add_size( _key_value.size() );
      return res;

   }
   mutable_variant_object::iterator mutable_variant_object::find( const string& key )
   {
      //time_logger_ex::instance().start("mutable_variant_object::iterator mutable_variant_object::find( const string& key )");
      auto res = std::move( _key_value.find( key ) );
      //time_logger_ex::instance().stop();
      //time_logger_ex::instance().add_size( _key_value.size() );
      return res;
   }

   const variant& mutable_variant_object::operator[]( const string& key )const
   {
      return (*this)[key.c_str()];
   }

   const variant& mutable_variant_object::operator[]( const char* key )const
   {
      auto itr = find( key );
      if( itr != end() ) return itr->second;
      FC_THROW_EXCEPTION( key_not_found_exception, "Key ${key}", ("key",key) );
   }
   variant& mutable_variant_object::operator[]( const string& key )
   {
      return (*this)[key.c_str()];
   }

   variant& mutable_variant_object::operator[]( const char* key )
   {
      //time_logger_ex::instance().start("variant& mutable_variant_object::operator[]( const char* key )");
      auto itr = find( key );
      if( itr != end() )
      {
        //time_logger_ex::instance().stop();
        return itr->second;
      }
      _key_value.emplace( std::make_pair( key, variant() ) );
      //time_logger_ex::instance().stop();
      return _key_value.rbegin()->second;
   }

   size_t mutable_variant_object::size() const
   {
      return _key_value.size();
   }

   mutable_variant_object::mutable_variant_object() 
   {
      //time_logger_ex::instance().start("mutable_variant_object::mutable_variant_object()");
      //time_logger_ex::instance().stop();
   }

   mutable_variant_object::mutable_variant_object( string key, variant val )
   {
      //time_logger_ex::instance().start("mutable_variant_object::mutable_variant_object( string key, variant val )");
       _key_value.insert( std::make_pair( fc::move(key), fc::move(val) ) );
      //time_logger_ex::instance().stop();
   }

   mutable_variant_object::mutable_variant_object( const variant_object& obj )
   {
      //time_logger_ex::instance().start("mutable_variant_object::mutable_variant_object( const variant_object& obj )");
      _key_value = obj._key_value;
      //time_logger_ex::instance().stop();
   }

   mutable_variant_object::mutable_variant_object( const mutable_variant_object& obj )
   {
      //time_logger_ex::instance().start("mutable_variant_object::mutable_variant_object( const mutable_variant_object& obj )");
      _key_value = obj._key_value;
      //time_logger_ex::instance().stop();
   }

   mutable_variant_object::mutable_variant_object( mutable_variant_object&& obj )
      : _key_value(fc::move(obj._key_value))
   {
   }

   mutable_variant_object& mutable_variant_object::operator=( const variant_object& obj )
   {
      //time_logger_ex::instance().start("mutable_variant_object& mutable_variant_object::operator=( const variant_object& obj )");
      _key_value = obj._key_value;
      //time_logger_ex::instance().stop();
      return *this;
   }

   mutable_variant_object& mutable_variant_object::operator=( mutable_variant_object&& obj )
   {
      //time_logger_ex::instance().start("mutable_variant_object& mutable_variant_object::operator=( mutable_variant_object&& obj )");
      if (this != &obj)
      {
         _key_value = fc::move(obj._key_value);
      }
      //time_logger_ex::instance().stop();
      return *this;
   }

   mutable_variant_object& mutable_variant_object::operator=( const mutable_variant_object& obj )
   {
      //time_logger_ex::instance().start("mutable_variant_object& mutable_variant_object::operator=( const mutable_variant_object& obj )");
      if (this != &obj)
      {
         _key_value = obj._key_value;
      }
      //time_logger_ex::instance().stop();
      return *this;
   }

   void mutable_variant_object::reserve( size_t s )
   {
      //time_logger_ex::instance().start("void mutable_variant_object::reserve( size_t s )");
      //_key_value->reserve(s);
      //time_logger_ex::instance().stop();
   }

   void  mutable_variant_object::erase( const string& key )
   {
      //time_logger_ex::instance().start("void  mutable_variant_object::erase( const string& key )");
      _key_value.erase( key );
      //time_logger_ex::instance().stop();
   }

   /** replaces the value at \a key with \a var or insert's \a key if not found */
   mutable_variant_object& mutable_variant_object::set( string key, variant var )
   {
      //time_logger_ex::instance().start("mutable_variant_object& mutable_variant_object::set( string key, variant var )");

      auto _result = _key_value.emplace( std::make_pair( fc::move(key), fc::move(var) ) );
      if( !_result.second )
      {
        fc_swap( _result.first->second, var );
      }

      // auto itr = find( key.c_str() );
      // if( itr != end() )
      // {
      //    fc_swap( itr->second, var );
      // }
      // else
      // {
      //    _key_value->insert( std::make_pair( fc::move(key), fc::move(var) ) );
      // }

      //time_logger_ex::instance().stop();
      return *this;
   }

   /** Appends \a key and \a var 
    * |thout checking for duplicates, designed to
    *  simplify construction of dictionaries using (key,val)(key2,val2) syntax 
    */
   mutable_variant_object& mutable_variant_object::operator()( string key, variant var )
   {
      //time_logger_ex::instance().start("mutable_variant_object& mutable_variant_object::operator()( string key, variant var )");
      _key_value.insert( std::make_pair( fc::move(key), fc::move(var) ) );
      //time_logger_ex::instance().stop();
      return *this;
   }

   mutable_variant_object& mutable_variant_object::operator()( const variant_object& vo )
   {
      //time_logger_ex::instance().start("mutable_variant_object& mutable_variant_object::operator()( const variant_object& vo )");
      for( const variant_object::pair& e : vo )
         set( e.first, e.second );
      //time_logger_ex::instance().stop();
      return *this;
   }

   mutable_variant_object& mutable_variant_object::operator()( const mutable_variant_object& mvo )
   {
      //time_logger_ex::instance().start("mutable_variant_object& mutable_variant_object::operator()( const mutable_variant_object& mvo )");
      if( &mvo == this )     // mvo(mvo) is no-op
      {
         //time_logger_ex::instance().stop();
         return *this;
      }
      for( const mutable_variant_object::pair& e : mvo )
         set( e.first, e.second );
      //time_logger_ex::instance().stop();
      return *this;
   }

   void to_variant( const mutable_variant_object& var,  variant& vo )
   {
      //time_logger_ex::instance().start("void to_variant( const mutable_variant_object& var,  variant& vo )");
      vo = variant(var);
      //time_logger_ex::instance().stop();
   }

   void from_variant( const variant& var,  mutable_variant_object& vo )
   {
      //time_logger_ex::instance().start("void from_variant( const variant& var,  mutable_variant_object& vo )");
      vo = var.get_object();
      //time_logger_ex::instance().stop();
   }

} // namesapce fc
