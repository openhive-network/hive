#pragma once
#include <fc/string.hpp>
#include <fc/optional.hpp>
#include <stdint.h>
#include <fc/filesystem.hpp>
#include <fc/variant_object.hpp>
#include <memory>

namespace fc {

  typedef fc::optional<fc::string>           ostring;
  typedef fc::optional<fc::path>             opath;

  namespace detail { class url_impl; }

  class mutable_url;
  
  /**
   *  Used to pass an immutable URL and
   *  query its parts.
   */
  class url 
  {
    public:
      url();
      explicit url( const string& u );
      url( const url& c );
      url( url&& c );
      url( mutable_url&& c );
      url( const mutable_url& c );
      ~url();
      
      url& operator=( const url& c );
      url& operator=( url&& c );

      url& operator=( const mutable_url& c );
      url& operator=( mutable_url&& c );
      
      bool operator==( const url& cmp )const;
      bool operator==( const mutable_url& cmp )const;
      
      operator string()const;
      
      //// file, ssh, tcp, http, ssl, etc...
      string                    proto()const; 
      ostring                   host()const;
      ostring                   user()const;
      ostring                   pass()const;
      opath                     path()const;
      fc::optional<uint16_t>    port()const;

    protected:
      friend class mutable_url;
      std::shared_ptr<detail::url_impl> my;
  };

  void to_variant( const url& u, fc::variant& v );
  void from_variant( const fc::variant& v, url& u );

  /**
   *  Used to create / manipulate a URL
   */
  class mutable_url : public url
  {
    public:
      mutable_url();
      explicit mutable_url( const string& mutable_url );
      mutable_url( const mutable_url& c );
      mutable_url( const url& c );
      mutable_url( mutable_url&& c );
      ~mutable_url();
      
      mutable_url& operator=( const url& c );
      mutable_url& operator=( const mutable_url& c );
      mutable_url& operator=( mutable_url&& c );
      
      bool operator==( const mutable_url& cmp )const;
      bool operator==( const url& cmp )const;
      
      void set_proto( string  p );
      void set_host( ostring  h );
      void set_user( ostring  u );
      void set_pass( ostring  p );
      void set_path( opath    p );
      void set_port( fc::optional<uint16_t> p );
  };

  void to_variant( const mutable_url& u, fc::variant& v );
  void from_variant( const fc::variant& v, mutable_url& u );

} // namespace fc

