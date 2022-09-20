#pragma once
#include <fc/variant.hpp>
#include <fc/shared_ptr.hpp>
#include <fc/unique_ptr.hpp>

namespace fc
{
   using std::map;
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
         entry( const string& k, const variant& v );
         entry( entry&& e );
         entry( const entry& e);
         entry& operator=(const entry&);
         entry& operator=(entry&&);
                
         const string&        key()const;
         const variant& value()const;
         void  set( const variant& v );

         variant&       value();
             
      private:
         string  _key;
         variant _value;
      };

      typedef std::vector< entry >::iterator iterator;
      typedef std::vector< entry >::const_iterator const_iterator;

      /**
         * @name Immutable Interface
         *
         * Calling these methods will not result in copies of the
         * underlying type.
         */
      ///@{
      iterator begin();
      iterator end();
      const_iterator begin()const;
      const_iterator end()const;
      iterator find( const string& key );
      iterator find( const char* key );
      const_iterator find( const string& key )const;
      const_iterator find( const char* key )const;
      bool contains( const string& key ) const;
      bool contains( const char* key ) const;
      variant& operator[]( const string& key );
      variant& operator[]( const char* key );
      const variant& operator[]( const string& key )const;
      const variant& operator[]( const char* key )const;
      size_t size()const;
      void reserve( size_t s );
      void erase( const string& key );
      ///@}

      variant_object& operator()( const string& key, const variant& var );
      template< typename T >
      variant_object& operator()( const string& key, T&& var )
      {
         set( key, variant( fc::forward< T >( var ) ) );
         return *this;
      }
      /**
         * Copy a variant_object into this variant_object.
         */
      variant_object& operator()( const variant_object& vo );

      /** replaces the value at \a key with \a var or insert's \a key if not found */
      variant_object& set( const string& key, const variant& var );

      variant_object();

      /** initializes the first key/value pair in the object */
      variant_object( const string& key, const variant& val );

      template< typename T >
      explicit variant_object( T&& v ) : _key_value( std::make_shared<std::vector<entry> >() )
      {
         *this = variant( fc::forward< T >( v ) ).get_object();
      }

      template<typename T>
      variant_object( const map<string,T>& values )
      :_key_value( std::make_shared<std::vector<entry> >() ) {
         _key_value->reserve( values.size() );
         for( const auto& item : values ) {
            _key_value->emplace_back( item.first, fc::variant(item.second) );
         }
      }
       
      template<typename T>
      variant_object( const string& key, T&& val )
      :_key_value( std::make_shared<std::vector<entry> >() )
      {
         set( key, variant( forward< T >( val ) ) );
      }
      variant_object( const variant_object& );
      variant_object( variant_object&& );

      variant_object& operator=( variant_object&& );
      variant_object& operator=( const variant_object& );

   private:
      std::shared_ptr< std::vector< entry > > _key_value;
      friend class variant_object_builder;
   };
   /** @ingroup Serializable */
   void to_variant( const variant_object& var,  variant& vo );
   /** @ingroup Serializable */
   void from_variant( const variant& var,  variant_object& vo );

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
      variant_object_builder( const string& key, const variant& val )
         : buffer( key, val ) {}

      template<typename T>
      variant_object_builder( const string& key, T&& val )
         : buffer( key, std::forward<T>( val ) ) {}

      void validate() const;

      variant_object get() const
      {
#if defined IS_TEST_NET || !defined NDEBUG
         validate();
#endif
         return buffer;
      }

      void append( const string& key, const variant& var )
      {
         buffer._key_value->emplace_back( key, var );
      }

      variant_object_builder& operator()( const string& key, const variant& var )
      {
         append( key, var );
         return *this;
      }
      template<typename T>
      variant_object_builder& operator()( const string& key, T&& var )
      {
         append( key, variant( std::forward<T>( var ) ) );
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
