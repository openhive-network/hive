#pragma once
#include <fc/variant.hpp>
#include <fc/shared_ptr.hpp>
#include <fc/unique_ptr.hpp>

namespace fc
{
   using std::map;
   class mutable_variant_object;
   class variant_object_builder;
   
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
      /** @brief a key/value pair */
      class entry 
      {
      public:
         entry();
         entry( string k, variant v );
         entry( entry&& e );
         entry( const entry& e);
         entry& operator=(const entry&);
         entry& operator=(entry&&);
                
         const string&        key()const;
         const variant& value()const;
         void  set( variant v );

         variant&       value();
             
      private:
         string  _key;
         variant _value;
      };

      typedef std::vector< entry >::const_iterator iterator;

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
      iterator find( const char* key )const;
      const variant& operator[]( const string& key )const;
      const variant& operator[]( const char* key )const;
      size_t size()const;
      bool   contains( const char* key ) const { return find(key) != end(); }
      ///@}

      variant_object();

      /** initializes the first key/value pair in the object */
      variant_object( string key, variant val );

      template<typename T>
      variant_object( const map<string,T>& values )
      :_key_value( new std::vector<entry>() ) {
         _key_value->reserve( values.size() );
         for( const auto& item : values ) {
            _key_value->emplace_back( entry( item.first, fc::variant(item.second) ) );
         }
      }
       
      template<typename T>
      variant_object( string key, T&& val )
      :_key_value( std::make_shared<std::vector<entry> >() )
      {
         *this = variant_object( std::move(key), variant(forward<T>(val)) );
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
      std::shared_ptr< std::vector< entry > > _key_value;
      friend class mutable_variant_object;
      friend class variant_object_builder;
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
      /** @brief a key/value pair */
      typedef variant_object::entry  entry;

      typedef std::vector< entry >::iterator       iterator;
      typedef std::vector< entry >::const_iterator const_iterator;

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
      iterator find( const char* key )const;
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
      /**
         *
         * @return end() if key is not found
         */
      iterator             find( const string& key );
      iterator             find( const char* key );


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
      :_key_value( new std::vector<entry>() )
      {
          *this = variant(fc::forward<T>(v)).get_object();
      }

      mutable_variant_object();

      template<typename T>
      mutable_variant_object( const map<string,T>& values )
      :_key_value( new std::vector<entry>() ) {
         _key_value->reserve( values.size() );
         for( const auto& item : values ) {
            _key_value->emplace_back( variant_object::entry( item.first, fc::variant(item.second) ) );
         }
      }

      /** initializes the first key/value pair in the object */
      mutable_variant_object( string key, variant val );
      template<typename T>
      mutable_variant_object( string key, T&& val )
      :_key_value( new std::vector<entry>() )
      {
         set( std::move(key), variant(fc::forward<T>(val)) );
      }

      mutable_variant_object( mutable_variant_object&& );
      mutable_variant_object( const mutable_variant_object& );
      mutable_variant_object( const variant_object& );

      mutable_variant_object& operator=( mutable_variant_object&& );
      mutable_variant_object& operator=( const mutable_variant_object& );
      mutable_variant_object& operator=( const variant_object& );


   private:
      std::unique_ptr< std::vector< entry > > _key_value;
      friend class variant_object;
   };
   /** @ingroup Serializable */
   void to_variant( const mutable_variant_object& var,  variant& vo );
   /** @ingroup Serializable */
   void from_variant( const variant& var,  mutable_variant_object& vo );

   template<typename T>
   void to_variant( const std::map<string, T>& var,  variant& vo )
   {
       vo = variant_object( var );
   }

   template<typename T>
   void from_variant( const variant& var,  std::map<string, T>& vo )
   {
      const auto& obj = var.get_object();
      vo.clear();
      for( auto itr = obj.begin(); itr != obj.end(); ++itr )
         vo[itr->key()] = itr->value().as<T>();
   }


   /**
   *  @brief Builder for variant_object that does not waste time on checking each write.
   *
   *  Additional key-value pairs are inserted without explicitly enforcing uniqueness
   *  (except for debug and testnet builds). The uniqueness must be ensured by the caller.
   */
   class variant_object_builder
   {
   public:
      variant_object_builder() {}
      variant_object_builder( string key, variant val )
         : buffer( std::move( key ), std::move( val ) ) {}

      template<typename T>
      variant_object_builder( string key, T&& val )
         : buffer( std::move( key ), std::forward<T>( val ) ) {}

      void validate() const;

      variant_object get() const
      {
#if defined IS_TEST_NET || !defined NDEBUG
         validate();
#endif
         return buffer;
      }

      void append( string key, variant var )
      {
         buffer._key_value->emplace_back( std::move( key ), std::move( var ) );
      }

      variant_object_builder& operator()( string key, variant var )
      {
         append( std::move( key ), std::move( var ) );
         return *this;
      }
      template<typename T>
      variant_object_builder& operator()( string key, T&& var )
      {
         append( std::move( key ), variant( std::forward<T>( var ) ) );
         return *this;
      }

      void reserve( size_t s )
      {
         buffer._key_value->reserve( s );
      }

   private:
      variant_object buffer;
   };
} // namespace fc
FC_REFLECT_TYPENAME(fc::variant_object)
