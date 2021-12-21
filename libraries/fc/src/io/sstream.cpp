#include <fc/io/sstream.hpp>
#include <fc/fwd_impl.hpp>
#include <fc/exception/exception.hpp>
#include <fc/log/logger.hpp>
#include <sstream>

namespace fc {

  class custom_stream
  {
    private:

      uint32_t    buffer_size   = 0;
      uint32_t    content_size  = 0;

      uint32_t    idx_write     = 0;
      uint32_t    idx_read      = 0;

      std::string content;

      void _write( const char& c )
      {
        if( idx_write < buffer_size )
        {
          content[ idx_write ] = c;
        }
        else
        {
          content.push_back( c );
          ++buffer_size;
        }
        ++idx_write;
      }

      size_t _read( char* buf, size_t len, bool move )
      {
        if( buf == nullptr )
          return 0;

        size_t res = 0;
        for( size_t i = 0; i < len; ++i )
        {
          if( idx_read < content_size )
          {
            ++res;
            buf[i] = content[ idx_read ];
            if( move )
              ++idx_read;
          }
          else
            break;
        }

        return res;
      }

    public:

      custom_stream( uint32_t _buffer_size = 16'000'000 ): buffer_size( _buffer_size )
      {
        content.resize( buffer_size );
      }

      custom_stream( const std::string& input_str )
      {
        str( input_str );
      }

      void exceptions( std::ios_base::iostate except )
      {
        //nothing to do
      }

      const std::string str() const
      {
        return content.substr(0, content_size);
      }

      void str( const std::string& s )
      {
        content_size = s.size();

        if( s.size() > buffer_size )
        {
          buffer_size = s.size();
        }

        content.resize( buffer_size );

        idx_read  = 0;
        idx_write = 0;

        for( auto& c : s )
          _write( c );

        idx_write = 0;
      }

      void clear()
      {
        //nothing to do
      }

      void write( const char* buf, size_t len )
      {
        assert( buf && len > 0 );

        content_size += len;
        for( uint32_t i = 0; i < len; ++i )
          _write( buf[i] );
      }

      size_t readsome( char* buf, size_t len )
      {
        return _read( buf, len, true/*move*/ );
      }

      bool eof() const
      {
        return idx_read == content_size;
      }

      void flush()
      {
        //nothing to do
      }

      char peek()
      {
        char c = EOF;
        _read( &c, 1, false/*move*/ );
        return c;
      }
  };

  class stringstream::impl {
    public:
    impl( fc::string&s )
    :ss( s )
    { ss.exceptions( std::stringstream::badbit ); }

    impl( const fc::string&s )
    :ss( s )
    { ss.exceptions( std::stringstream::badbit ); }

    impl(){ss.exceptions( std::stringstream::badbit ); }
    
    custom_stream ss;
  };

  stringstream::stringstream( fc::string& s )
  :my(s) {
  }
  stringstream::stringstream( const fc::string& s )
  :my(s) {
  }
  stringstream::stringstream(){}
  stringstream::~stringstream(){}


  fc::string stringstream::str(){
    return my->ss.str();//.c_str();//*reinterpret_cast<fc::string*>(&st);
  }

  void stringstream::str(const fc::string& s) {
    my->ss.str(s);
  }

  void stringstream::clear() {
    my->ss.clear();
  }


  bool     stringstream::eof()const {
    return my->ss.eof();
  }
  size_t stringstream::writesome( const char* buf, size_t len ) {
    my->ss.write(buf,len);
    if( my->ss.eof() )
    {
       FC_THROW_EXCEPTION( eof_exception, "stringstream" );
    }
    return len;
  }
  size_t stringstream::writesome( const std::shared_ptr<const char>& buf, size_t len, size_t offset )
  {
    return writesome(buf.get() + offset, len);
  }

  size_t   stringstream::readsome( char* buf, size_t len ) {
    size_t r = static_cast<size_t>(my->ss.readsome(buf,len));
    if( my->ss.eof() || r == 0 )
    {
       FC_THROW_EXCEPTION( eof_exception, "stringstream" );
    }
    return r;
  }
  size_t   stringstream::readsome( const std::shared_ptr<char>& buf, size_t len, size_t offset )
  {
    return readsome(buf.get() + offset, len);
  }


  void     stringstream::close(){ my->ss.flush(); };
  void     stringstream::flush(){ my->ss.flush(); };

  /*
  istream&   stringstream::read( char* buf, size_t len ) {
    my->ss.read(buf,len);
    return *this;
  }
  istream& stringstream::read( int64_t&     v ) { my->ss >> v; return *this; }
  istream& stringstream::read( uint64_t&    v ) { my->ss >> v; return *this; }
  istream& stringstream::read( int32_t&     v ) { my->ss >> v; return *this; }
  istream& stringstream::read( uint32_t&    v ) { my->ss >> v; return *this; }
  istream& stringstream::read( int16_t&     v ) { my->ss >> v; return *this; }
  istream& stringstream::read( uint16_t&    v ) { my->ss >> v; return *this; }
  istream& stringstream::read( int8_t&      v ) { my->ss >> v; return *this; }
  istream& stringstream::read( uint8_t&     v ) { my->ss >> v; return *this; }
  istream& stringstream::read( float&       v ) { my->ss >> v; return *this; }
  istream& stringstream::read( double&      v ) { my->ss >> v; return *this; }
  istream& stringstream::read( bool&        v ) { my->ss >> v; return *this; }
  istream& stringstream::read( char&        v ) { my->ss >> v; return *this; }
  istream& stringstream::read( fc::string&  v ) { my->ss >> *reinterpret_cast<std::string*>(&v); return *this; }

  ostream& stringstream::write( const fc::string& s) {
    my->ss.write( s.c_str(), s.size() );
    return *this;
  }
  */

  char     stringstream::peek() 
  { 
    char c = my->ss.peek(); 
    if( my->ss.eof() )
    {
       FC_THROW_EXCEPTION( eof_exception, "stringstream" );
    }
    return c;
  }
} 


