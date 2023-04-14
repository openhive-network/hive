/** This source adapted from https://github.com/kmicklas/variadic-static_variant
 *
 * Copyright (C) 2013 Kenneth Micklas
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **/
#pragma once
#include <stdexcept>
#include <typeinfo>
#include <fc/exception/exception.hpp>
#include <fc/uint128.hpp>

namespace fc {

template< typename static_variant >
struct serialization_functor
{
  bool operator()( const fc::variant& v, static_variant& s ) const
  {
    return false;
  }
};

template< typename static_variant >
struct variant_creator_functor
{
  template<typename T>
  variant operator()( const T& v ) const
  {
    auto name = trim_typename_namespace( fc::get_typename< T >::name() );
    return mutable_variant_object( "type", name )( "value", v );
  }
};

// Implementation details, the user should not import this:
namespace impl {

template<int64_t N, typename... Ts>
struct storage_ops;

template<typename X, typename... Ts>
struct position;

template<typename... Ts>
struct type_info;

template<typename StaticVariant>
struct copy_construct
{
   typedef void result_type;
   StaticVariant& sv;
   copy_construct( StaticVariant& s ):sv(s){}
   template<typename T>
   void operator()( const T& v )const
   {
      sv.init(v);
   }
};

template<typename StaticVariant>
struct move_construct
{
   typedef void result_type;
   StaticVariant& sv;
   move_construct( StaticVariant& s ):sv(s){}
   template<typename T>
   void operator()( T& v )const
   {
      sv.init( std::move(v) );
   }
};

template<int64_t N, typename T, typename... Ts>
struct storage_ops<N, T&, Ts...> {
    static void del(int64_t n, void *data) {}
    static void con(int64_t n, void *data) {}

    template<typename visitor>
    static typename visitor::result_type apply(int64_t n, void *data, visitor& v) {}

    template<typename visitor>
    static typename visitor::result_type apply(int64_t n, void *data, const visitor& v) {}

    template<typename visitor>
    static typename visitor::result_type apply(int64_t n, const void *data, visitor& v) {}

    template<typename visitor>
    static typename visitor::result_type apply(int64_t n, const void *data, const visitor& v) {}
};

template<int64_t N, typename T, typename... Ts>
struct storage_ops<N, T, Ts...> {
    static void del(int64_t n, void *data) {
        if(n == N) reinterpret_cast<T*>(data)->~T();
        else storage_ops<N + 1, Ts...>::del(n, data);
    }
    static void con(int64_t n, void *data) {
        if(n == N) new(reinterpret_cast<T*>(data)) T();
        else storage_ops<N + 1, Ts...>::con(n, data);
    }

    template<typename visitor>
    static typename visitor::result_type apply(int64_t n, void *data, visitor& v) {
        if(n == N) return v(*reinterpret_cast<T*>(data));
        else return storage_ops<N + 1, Ts...>::apply(n, data, v);
    }

    template<typename visitor>
    static typename visitor::result_type apply(int64_t n, void *data, const visitor& v) {
        if(n == N) return v(*reinterpret_cast<T*>(data));
        else return storage_ops<N + 1, Ts...>::apply(n, data, v);
    }

    template<typename visitor>
    static typename visitor::result_type apply(int64_t n, const void *data, visitor& v) {
        if(n == N) return v(*reinterpret_cast<const T*>(data));
        else return storage_ops<N + 1, Ts...>::apply(n, data, v);
    }

    template<typename visitor>
    static typename visitor::result_type apply(int64_t n, const void *data, const visitor& v) {
        if(n == N) return v(*reinterpret_cast<const T*>(data));
        else return storage_ops<N + 1, Ts...>::apply(n, data, v);
    }
};

template<int64_t N>
struct storage_ops<N> {
    static void del(int64_t n, void *data) {
       FC_THROW_EXCEPTION( fc::assert_exception, "Internal error: static_variant tag is invalid.");
    }
    static void con(int64_t n, void *data) {
       FC_THROW_EXCEPTION( fc::assert_exception, "Internal error: static_variant tag is invalid." );
    }

    template<typename visitor>
    static typename visitor::result_type apply(int64_t n, void *data, visitor& v) {
       FC_THROW_EXCEPTION( fc::assert_exception, "Internal error: static_variant tag is invalid." );
    }
    template<typename visitor>
    static typename visitor::result_type apply(int64_t n, void *data, const visitor& v) {
       FC_THROW_EXCEPTION( fc::assert_exception, "Internal error: static_variant tag is invalid." );
    }
    template<typename visitor>
    static typename visitor::result_type apply(int64_t n, const void *data, visitor& v) {
       FC_THROW_EXCEPTION( fc::assert_exception, "Internal error: static_variant tag is invalid." );
    }
    template<typename visitor>
    static typename visitor::result_type apply(int64_t n, const void *data, const visitor& v) {
       FC_THROW_EXCEPTION( fc::assert_exception, "Internal error: static_variant tag is invalid." );
    }
};

template<typename X>
struct position<X> {
    static const int64_t pos = -1;
};

template<typename X, typename... Ts>
struct position<X, X, Ts...> {
    static const int64_t pos = 0;
};

template<typename X, typename T, typename... Ts>
struct position<X, T, Ts...> {
    static const int64_t pos = position<X, Ts...>::pos != -1 ? position<X, Ts...>::pos + 1 : -1;
};

template<typename T, typename... Ts>
struct type_info<T&, Ts...> {
    static const bool no_reference_types = false;
    static const bool no_duplicates = position<T, Ts...>::pos == -1 && type_info<Ts...>::no_duplicates;
    static const size_t size = type_info<Ts...>::size > sizeof(T&) ? type_info<Ts...>::size : sizeof(T&);
    static const size_t count = 1 + type_info<Ts...>::count;
};

template<typename T, typename... Ts>
struct type_info<T, Ts...> {
    static const bool no_reference_types = type_info<Ts...>::no_reference_types;
    static const bool no_duplicates = position<T, Ts...>::pos == -1 && type_info<Ts...>::no_duplicates;
    static const size_t size = type_info<Ts...>::size > sizeof(T) ? type_info<Ts...>::size : sizeof(T&);
    static const size_t count = 1 + type_info<Ts...>::count;
};

template<>
struct type_info<> {
    static const bool no_reference_types = true;
    static const bool no_duplicates = true;
    static const size_t count = 0;
    static const size_t size = 0;
};

struct type_name_printer
{
  typedef void result_type;

  explicit type_name_printer(std::string& buffer) : _buffer(buffer) {}
  
  template<typename T>
  void operator()(const T& item)
  {
    _buffer = fc::get_typename<T>::name();
  }

  private:
    std::string& _buffer;
};

} // namespace impl

template<typename... Types>
class static_variant {
    static_assert(impl::type_info<Types...>::no_reference_types, "Reference types are not permitted in static_variant.");
    static_assert(impl::type_info<Types...>::no_duplicates, "static_variant type arguments contain duplicate types.");

    int64_t                 _tag;
    alignas(Types...) char  storage[impl::type_info<Types...>::size];  // to be sure address of 'storage' is properly aligned
      
    template<typename X>
    void init(const X& x) {
        _tag = impl::position<X, Types...>::pos;
        new(storage) X(x);
    }

    template<typename X>
    void init(X&& x) {
        _tag = impl::position<X, Types...>::pos;
        new(storage) X( std::move(x) );
    }

    template<typename StaticVariant>
    friend struct impl::copy_construct;
    template<typename StaticVariant>
    friend struct impl::move_construct;

public:
    std::string get_stored_type_name( bool strip_namespace = false ) const
    {
      std::string type_name("Uninitialized static_variant");
      impl::type_name_printer printer(type_name);
      visit(printer);

      if( strip_namespace )
        return type_name.c_str() + type_name.find_last_of( ":" ) + 1;
      else
        return type_name;
    }

    /// Expose it outside for further processing of specified Types.
    using type_infos = impl::type_info<Types...>;

    template<typename X>
    struct tag
    {
       static_assert(
         impl::position<X, Types...>::pos != -1,
         "Type not in static_variant."
       );
       static const int64_t value = impl::position<X, Types...>::pos;
    };
    static_variant()
    {
       _tag = 0;
       impl::storage_ops<0, Types...>::con(0, storage);
    }

    template<typename... Other>
    static_variant( const static_variant<Other...>& cpy )
    {
       cpy.visit( impl::copy_construct<static_variant>(*this) );
    }
    static_variant( const static_variant& cpy )
    {
       cpy.visit( impl::copy_construct<static_variant>(*this) );
    }

    static_variant( static_variant&& mv )
    {
       mv.visit( impl::move_construct<static_variant>(*this) );
    }

    template<typename X>
    static_variant(const X& v) {
        static_assert(
            impl::position<X, Types...>::pos != -1,
            "Type not in static_variant."
        );
        init(v);
    }
    ~static_variant() {
       impl::storage_ops<0, Types...>::del(_tag, storage);
    }


    template<typename X>
    static_variant& operator=(const X& v) {
        static_assert(
            impl::position<X, Types...>::pos != -1,
            "Type not in static_variant."
        );
        this->~static_variant();
        init(v);
        return *this;
    }
    static_variant& operator=( const static_variant& v )
    {
       if( this == &v ) return *this;
       this->~static_variant();
       v.visit( impl::copy_construct<static_variant>(*this) );
       return *this;
    }
    static_variant& operator=( static_variant&& v )
    {
       if( this == &v ) return *this;
       this->~static_variant();
       v.visit( impl::move_construct<static_variant>(*this) );
       return *this;
    }
    friend bool operator == ( const static_variant& a, const static_variant& b )
    {
       return a.which() == b.which();
    }
    friend bool operator < ( const static_variant& a, const static_variant& b )
    {
       return a.which() < b.which();
    }

    template<typename X>
    X& get() {
        static_assert(
            impl::position<X, Types...>::pos != -1,
            "Type not in static_variant."
        );
        if(_tag == impl::position<X, Types...>::pos) {
            void* tmp(storage);
            return *reinterpret_cast<X*>(tmp);
        } else {
            FC_THROW_EXCEPTION( fc::assert_exception, "static_variant does not contain a value of type ${t}. Stored value has type: ${s}.",
              ("s", get_stored_type_name())("t",fc::get_typename<X>::name()) );
        }
    }
    template<typename X>
    const X& get() const {
        static_assert(
            impl::position<X, Types...>::pos != -1,
            "Type not in static_variant."
        );
        if(_tag == impl::position<X, Types...>::pos) {
            const void* tmp(storage);
            return *reinterpret_cast<const X*>(tmp);
        } else {
          FC_THROW_EXCEPTION(fc::assert_exception, "static_variant does not contain a value of type ${t}. Stored value has type: ${s}.",
            ("s", get_stored_type_name())("t", fc::get_typename<X>::name()));
        }
    }
    template<typename visitor>
    typename visitor::result_type visit(visitor& v) {
        return impl::storage_ops<0, Types...>::apply(_tag, storage, v);
    }

    template<typename visitor>
    typename visitor::result_type visit(const visitor& v) {
        return impl::storage_ops<0, Types...>::apply(_tag, storage, v);
    }

    template<typename visitor>
    typename visitor::result_type visit(visitor& v)const {
        return impl::storage_ops<0, Types...>::apply(_tag, storage, v);
    }

    template<typename visitor>
    typename visitor::result_type visit(const visitor& v)const {
        return impl::storage_ops<0, Types...>::apply(_tag, storage, v);
    }

    static int64_t count() { return static_cast< int64_t >( impl::type_info<Types...>::count ); }
    void set_which( int64_t w ) {
      FC_ASSERT( w < count() && w >= 0 );
      this->~static_variant();
      _tag = w;
      impl::storage_ops<0, Types...>::con(_tag, storage);
    }

    int64_t which() const {return _tag;}
};

template<typename Result>
struct visitor {
    typedef Result result_type;
};

   template< typename static_variant >
   struct from_static_variant
   {
      variant& var;
      from_static_variant( variant& dv ):var(dv){}

      typedef void result_type;
      template<typename T> void operator()( const T& v )const
      {
        var = variant_creator_functor< static_variant >()( v );
      }
   };

   struct to_static_variant
   {
      const variant& var;
      to_static_variant( const variant& dv ):var(dv){}

      typedef void result_type;
      template<typename T> void operator()( T& v )const
      {
         from_variant( var, v );
      }
   };

   template<typename... T> void to_variant( const fc::static_variant<T...>& s, fc::variant& v )
   {
      s.visit( from_static_variant< fc::static_variant<T...> >( v ) );
   }

   struct get_static_variant_name
   {
      string& name;
      get_static_variant_name( string& n )
         : name( n ) {}

      typedef void result_type;

      template< typename T > void operator()( const T& v )const
      {
         name = trim_typename_namespace( fc::get_typename< T >::name() );
      }
   };

   template<typename... T> void from_variant( const fc::variant& v, fc::static_variant<T...>& s )
   {
      static std::map< string, int64_t > to_tag = []()
      {
         std::map< string, int64_t > name_map;
         for( int i = 0; i < fc::static_variant<T...>::count(); ++i )
         {
            fc::static_variant<T...> tmp;
            tmp.set_which(i);
            string n;
            tmp.visit( get_static_variant_name( n ) );
            name_map[n] = i;
         }
         return name_map;
      }();

      if( serialization_functor< fc::static_variant<T...> >()( v, s ) )
        return;

      if( !v.is_object() )
        FC_THROW_EXCEPTION( bad_cast_exception, "Input data have to treated as object." );

      auto v_object = v.get_object();

      FC_ASSERT( v_object.contains( "type" ), "Type field doesn't exist." );
      FC_ASSERT( v_object.contains( "value" ), "Value field doesn't exist." );

      int64_t which = -1;

      if( v_object[ "type" ].is_integer() )
      {
         which = v_object[ "type" ].as_int64();
      }
      else
      {
         auto itr = to_tag.find( v_object[ "type" ].as_string() );
         FC_ASSERT( itr != to_tag.end(), "Invalid object name: ${n}", ("n", v_object[ "type" ]) );
         which = itr->second;
      }

      s.set_which( which );
      s.visit( fc::to_static_variant( v_object[ "value" ] ) );
   }

   template< typename... T > struct get_comma_separated_typenames;

   template<>
   struct get_comma_separated_typenames<>
   {
      static const char* names() { return ""; }
   };

   template< typename T >
   struct get_comma_separated_typenames<T>
   {
      static const char* names()
      {
         static const std::string n = get_typename<T>::name();
         return n.c_str();
      }
   };

   template< typename T, typename... Ts >
   struct get_comma_separated_typenames<T, Ts...>
   {
      static const char* names()
      {
         static const std::string n =
            std::string( get_typename<T>::name() )+","+
            std::string( get_comma_separated_typenames< Ts... >::names() );
         return n.c_str();
      }
   };

   template< typename... T >
   struct get_typename< static_variant< T... > >
   {
      static const char* name()
      {
         static const std::string n = std::string( "fc::static_variant<" )
            + get_comma_separated_typenames<T...>::names()
            + ">";
         return n.c_str();
      }
   };

} // namespace fc
