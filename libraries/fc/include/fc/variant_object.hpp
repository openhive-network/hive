#pragma once
#include <fc/variant.hpp>
#include <fc/shared_ptr.hpp>
#include <fc/unique_ptr.hpp>

namespace fc
{
   using std::map;
   class mutable_variant_object;
   
   /**
    *  @ingroup Serializable
    *
    *  @brief An order-perserving dictionary of variant's.  
    *
    *  Keys are kept in the order they are inserted.
    *  This dictionary implements copy-on-write
    *
    *  @note This class is not optimized for random-access on large
    *        sets of key-value pairs.
    */
   class variant_object
   {
   public:

      using pair          = std::pair<string, variant>;
      using items_type    = std::map<string, variant>;
      using p_items_type  = std::shared_ptr< items_type >;

      using iterator      = items_type::const_iterator;

      /**
         * @name Immutable Interface
         *
         * Calling these methods will not result in copies of the
         * underlying type.
         */
      ///@{
      iterator begin()const;
      iterator end()const;
      iterator find( const string& key )const;
      const variant& operator[]( const string& key )const;
      size_t size()const;
      bool   contains( const char* key ) const { return find(key) != end(); }
      ///@}

      variant_object();

      /** initializes the first key/value pair in the object */
      variant_object( string key, variant val );

      template<typename T>
      variant_object( const map<string,T>& values )
      {
         time_logger_ex::instance().start("variant_object( const map<string,T>& values )");
         _key_value = p_items_type( new items_type() );
         for( const auto& item : values ) {
            _key_value->emplace( std::make_pair( item.first, fc::variant(item.second) ) );
         }
         time_logger_ex::instance().stop();
      }
       
      template<typename T>
      variant_object( string key, T&& val )
      {
         time_logger_ex::instance().start("variant_object( string key, T&& val )");
         _key_value = p_items_type( new items_type() );
         *this = variant_object( std::move(key), variant(forward<T>(val)) );
         time_logger_ex::instance().stop();
      }
      variant_object( const variant_object& );
      variant_object( variant_object&& );

      variant_object( const mutable_variant_object& );
      variant_object( mutable_variant_object&& );

      variant_object& operator=( variant_object&& );
      variant_object& operator=( const variant_object& );

      variant_object& operator=( mutable_variant_object&& );
      variant_object& operator=( const mutable_variant_object& );

   private:
      p_items_type _key_value;
      friend class mutable_variant_object;
   };
   /** @ingroup Serializable */
   void to_variant( const variant_object& var,  variant& vo );
   /** @ingroup Serializable */
   void from_variant( const variant& var,  variant_object& vo );


  /**
   *  @ingroup Serializable
   *
   *  @brief An order-perserving dictionary of variant's.  
   *
   *  Keys are kept in the order they are inserted.
   *  This dictionary implements copy-on-write
   *
   *  @note This class is not optimized for random-access on large
   *        sets of key-value pairs.
   */
   class mutable_variant_object
   {
   public:

      using pair          = variant_object::pair;
      using items_type    = variant_object::items_type;
      using p_items_type  = std::unique_ptr< items_type >;

      using iterator      = items_type::iterator;

      /**
         * @name Immutable Interface
         *
         * Calling these methods will not result in copies of the
         * underlying type.
         */
      ///@{
      iterator begin()const;
      iterator end()const;
      iterator find( const string& key )const;
      const variant& operator[]( const string& key )const;
      const variant& operator[]( const char* key )const;
      size_t size()const;
      ///@}
      variant& operator[]( const string& key );
      variant& operator[]( const char* key );

      /**
         * @name mutable Interface
         *
         * Calling these methods will result in a copy of the underlying type 
         * being created if there is more than one reference to this object.
         */
      ///@{
      void                 reserve( size_t s);
      iterator             begin();
      iterator             end();
      void                 erase( const string& key );

      /** replaces the value at \a key with \a var or insert's \a key if not found */
      mutable_variant_object& set( string key, variant var );
      /** Appends \a key and \a var without checking for duplicates, designed to
         *  simplify construction of dictionaries using (key,val)(key2,val2) syntax 
         */
      /**
      *  Convenience method to simplify the manual construction of
      *  variant_object's
      *
      *  Instead of:
      *    <code>mutable_variant_object("c",c).set("a",a).set("b",b);</code>
      *
      *  You can use:
      *    <code>mutable_variant_object( "c", c )( "b", b)( "c",c )</code>
      *
      *  @return *this;
      */
      mutable_variant_object& operator()( string key, variant var );
      template<typename T>
      mutable_variant_object& operator()( string key, T&& var )
      {
         set(std::move(key), variant( fc::forward<T>(var) ) );
         return *this;
      }
      /**
       * Copy a variant_object into this mutable_variant_object.
       */
      mutable_variant_object& operator()( const variant_object& vo );
      /**
       * Copy another mutable_variant_object into this mutable_variant_object.
       */
      mutable_variant_object& operator()( const mutable_variant_object& mvo );
      ///@}


      template<typename T>
      explicit mutable_variant_object( T&& v )
      :_key_value( new items_type() )
      {
         time_logger_ex::instance().start("explicit mutable_variant_object( T&& v )");
          *this = variant(fc::forward<T>(v)).get_object();
         time_logger_ex::instance().stop();
      }

      mutable_variant_object();

      template<typename T>
      mutable_variant_object( const map<string,T>& values ) {
         time_logger_ex::instance().start("mutable_variant_object( const map<string,T>& values )");
         _key_value.reset( new items_type() );
         for( const auto& item : values ) {
            _key_value->emplace( std::make_pair( item.first, fc::variant(item.second) ) );
         }
         time_logger_ex::instance().stop();
      }

      /** initializes the first key/value pair in the object */
      mutable_variant_object( string key, variant val );
      template<typename T>
      mutable_variant_object( string key, T&& val )
      {
         time_logger_ex::instance().start("mutable_variant_object( string key, T&& val )");
         _key_value.reset( new items_type );
         set( std::move(key), variant(forward<T>(val)) );
         time_logger_ex::instance().stop();
      }

      mutable_variant_object( mutable_variant_object&& );
      mutable_variant_object( const mutable_variant_object& );
      mutable_variant_object( const variant_object& );

      mutable_variant_object& operator=( mutable_variant_object&& );
      mutable_variant_object& operator=( const mutable_variant_object& );
      mutable_variant_object& operator=( const variant_object& );


   private:
      p_items_type _key_value;
      friend class variant_object;
   };
   /** @ingroup Serializable */
   void to_variant( const mutable_variant_object& var,  variant& vo );
   /** @ingroup Serializable */
   void from_variant( const variant& var,  mutable_variant_object& vo );

   template<typename T>
   void to_variant( const std::map<string, T>& var,  variant& vo )
   {
      time_logger_ex::instance().start("void to_variant( const std::map<string, T>& var,  variant& vo )");
       vo = variant_object( var );
      time_logger_ex::instance().stop();
   }

   template<typename T>
   void from_variant( const variant& var,  std::map<string, T>& vo )
   {
      const auto& obj = var.get_object();
      vo.clear();
      for( auto itr = obj.begin(); itr != obj.end(); ++itr )
        vo.insert( *itr );
   }

} // namespace fc
FC_REFLECT_TYPENAME(fc::variant_object)
