#include <fc/io/sstream.hpp>
#include <fc/io/iostream.hpp>
#include <fc/log/logger.hpp>
#include <fc/thread/scoped_lock.hpp>
#include <fc/io/stdio.hpp>
#include <fc/thread/thread.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/thread/mutex.hpp>

#include <string.h>

#include <iostream>
#include <string>

namespace fc {

  struct cin_buffer {
    cin_buffer():eof(false),write_pos(0),read_pos(0),cinthread("cin"){
    
      cinthread.async( [=](){read();}, "cin_buffer::read" );
    }

    void     read() {
      char c;
      std::cin.read(&c,1);
      while( !std::cin.eof() ) {
        while( write_pos - read_pos > 0xfffff ) {
          fc::promise<void>::ptr wr( fc::promise<void>::create("cin_buffer::write_ready") );
          write_ready = wr;
          if( write_pos - read_pos <= 0xfffff ) {
            wr->wait();
          }
          write_ready.reset();
        }
        buf[write_pos&0xfffff] = c;
        ++write_pos;

        fc::promise<void>::ptr tmp;
        { // copy read_ready because it is accessed from multiple threads
          fc::scoped_lock<boost::mutex> lock( read_ready_mutex ); 
          tmp = read_ready; 
        }

        if( tmp && !tmp->ready() ) { 
          tmp->set_value(); 
        }
        std::cin.read(&c,1);
      }
      eof = true;
      fc::promise<void>::ptr tmp;
      {  // copy read_ready because it is accessed from multiple threads
        fc::scoped_lock<boost::mutex> lock( read_ready_mutex ); 
        tmp = read_ready;
      }
      if( tmp && !tmp->ready() ) { 
        tmp->set_exception( exception_ptr( new eof_exception() )); 
      }
    }
    boost::mutex              read_ready_mutex;
    fc::promise<void>::ptr read_ready;
    fc::promise<void>::ptr write_ready;
    
    volatile bool     eof;
    
    volatile uint64_t write_pos;
             char     buf[0xfffff+1]; // 1 mb buffer
    volatile uint64_t read_pos;
    fc::thread        cinthread;
  };

  cin_buffer& get_cin_buffer() {
    static cin_buffer* b = new cin_buffer();
    return *b;
  }

  size_t cin_t::readsome( char* buf, size_t len ) {
    cin_buffer& b = get_cin_buffer();
    int64_t avail = b.write_pos - b.read_pos;
    avail = (fc::min)(int64_t(len),avail);
    int64_t u = 0;

    if( !((avail>0) && (len>0)) ) {
      read( buf, 1 );
      ++buf;
      ++u;
      --len;
    }

    while( (avail>0) && (len>0) ) {
      *buf = b.buf[b.read_pos&0xfffff]; 
      ++b.read_pos;
      ++buf;
      --avail;
      --len;
      ++u;
    }
    return size_t(u);
  }
  size_t cin_t::readsome( const std::shared_ptr<char>& buf, size_t len, size_t offset ) { return readsome(buf.get() + offset, len); }

  cin_t::~cin_t() {
    /*
    cin_buffer& b = get_cin_buffer();
    if( b.read_ready ) {
        b.read_ready->wait();
    }
    */
  }
  istream& cin_t::read( char* buf, size_t len ) {
    cin_buffer& b = get_cin_buffer();
    do {
        while( !b.eof &&  (b.write_pos - b.read_pos)==0 ){ 
           // wait for more... 
           fc::promise<void>::ptr rr = fc::promise<void>::create("cin_buffer::read_ready");
           {  // copy read_ready because it is accessed from multiple threads
             fc::scoped_lock<boost::mutex> lock( b.read_ready_mutex ); 
             b.read_ready = rr;
           }
           if( b.write_pos - b.read_pos == 0 ) {
             rr->wait();
           }
         //  b.read_ready.reset();
           {  // copy read_ready because it is accessed from multiple threads
             fc::scoped_lock<boost::mutex> lock( b.read_ready_mutex ); 
             b.read_ready.reset();
           }
        }
        if( b.eof ) FC_THROW_EXCEPTION( eof_exception, "cin" );
        size_t r = readsome( buf, len );
        buf += r;
        len -= r;

        auto tmp = b.write_ready; // copy write_writey because it is accessed from multiple thwrites
        if( tmp && !tmp->ready() ) { 
          tmp->set_value(); 
        }
    } while( len > 0 && !b.eof );
    if( b.eof ) FC_THROW_EXCEPTION( eof_exception, "cin" );
    return *this;
  }

  bool cin_t::eof()const { return get_cin_buffer().eof; }

  
  std::shared_ptr<cin_t>  cin_ptr = std::make_shared<cin_t>();
  cin_t&  cin  = *cin_ptr;

} // namespace fc
