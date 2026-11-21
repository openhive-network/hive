/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <fc/thread/thread.hpp>
#include <fc/thread/mutex.hpp>
#include <fc/thread/scoped_lock.hpp>
#include <fc/thread/future.hpp>
#include <fc/log/logger.hpp>
#include <fc/io/enum_type.hpp>

#include <graphene/net/message_oriented_connection.hpp>
#include <graphene/net/stcp_socket.hpp>
#include <graphene/net/config.hpp>
#include <graphene/net/core_messages.hpp>

#include <atomic>
#include <cstring>
#include <memory>

#ifdef DEFAULT_LOGGER
# undef DEFAULT_LOGGER
#endif
#define DEFAULT_LOGGER "p2p"

#ifndef NDEBUG
# define VERIFY_CORRECT_THREAD() assert(_thread->is_current())
#else
# define VERIFY_CORRECT_THREAD() do {} while (0)
#endif

namespace graphene { namespace net {
  namespace detail
  {
    class message_oriented_connection_impl
    {
    private:
      message_oriented_connection* _self;
      message_oriented_connection_delegate *_delegate;
      stcp_socket _sock;
      fc::future<void> _read_loop_done;
      uint64_t _bytes_received;
      uint64_t _bytes_sent;

      fc::time_point _connected_time;
      fc::time_point _last_message_received_time;
      fc::time_point _last_message_sent_time;

      bool _send_message_in_progress;
      /// set once we close the socket ourselves, after which the read loop failing is expected
      bool _closed_locally;
      /// see allow_full_size_messages()
      size_t _buffer_size = GRAPHENE_NET_HANDSHAKE_BUFFER_SIZE;
      uint32_t _maximum_message_size = GRAPHENE_NET_MAXIMUM_HANDSHAKE_MESSAGE_SIZE;
#ifndef NDEBUG
      fc::thread* _thread;
#endif

      void read_loop();
      void start_read_loop();
      void send_padded_messages(const message_to_write* messages_to_send, size_t message_count);
    public:
      fc::tcp_socket& get_socket();
      void accept();
      void connect_to(const fc::ip::endpoint& remote_endpoint);
      void bind(const fc::ip::endpoint& local_endpoint);

      message_oriented_connection_impl(message_oriented_connection* self,
                                       message_oriented_connection_delegate* delegate = nullptr);
      ~message_oriented_connection_impl();

      void send_message(const message& message_to_send);
      void send_messages(const std::vector<message_to_write>& messages_to_send);
      void close_connection();
      void destroy_connection(const char* caller);
      void allow_full_size_messages();

      uint64_t get_total_bytes_sent() const;
      uint64_t get_total_bytes_received() const;

      fc::time_point get_last_message_sent_time() const;
      fc::time_point get_last_message_received_time() const;
      fc::time_point get_last_write_progress_time() const { return _sock.get_last_write_time(); }
      fc::time_point get_connection_time() const { return _connected_time; }
      fc::sha512 get_shared_secret() const;
    };

    message_oriented_connection_impl::message_oriented_connection_impl(message_oriented_connection* self,
                                                                       message_oriented_connection_delegate* delegate)
    : _self(self),
      _delegate(delegate),
      _bytes_received(0),
      _bytes_sent(0),
      _send_message_in_progress(false),
      _closed_locally(false)
#ifndef NDEBUG
      ,_thread(&fc::thread::current())
#endif
    {
    }
    message_oriented_connection_impl::~message_oriented_connection_impl()
    {
      VERIFY_CORRECT_THREAD();
      destroy_connection(__FUNCTION__);
    }

    fc::tcp_socket& message_oriented_connection_impl::get_socket()
    {
      VERIFY_CORRECT_THREAD();
      return _sock.get_socket();
    }

    void message_oriented_connection_impl::accept()
    {
      VERIFY_CORRECT_THREAD();
      _sock.accept();
      assert(!_read_loop_done.valid()); // check to be sure we never launch two read loops
      _read_loop_done = fc::async([=, this](){ read_loop(); }, "message read_loop");
    }

    void message_oriented_connection_impl::connect_to(const fc::ip::endpoint& remote_endpoint)
    {
      VERIFY_CORRECT_THREAD();
      _sock.connect_to(remote_endpoint);
      FC_ASSERT(!_read_loop_done.valid()); // check to be sure we never launch two read loops
      _read_loop_done = fc::async([=, this](){ read_loop(); }, "message read_loop");
    }

    void message_oriented_connection_impl::bind(const fc::ip::endpoint& local_endpoint)
    {
      VERIFY_CORRECT_THREAD();
      _sock.bind(local_endpoint);
    }

    void message_oriented_connection_impl::read_loop()
    {
      VERIFY_CORRECT_THREAD();
      // every async read reschedules this task, which on a busy p2p thread waits behind every other peer's
      // tasks; so each read takes whatever the kernel holds, up to the buffer's size (_buffer_size), and every
      // complete message in it is handled before the next read.  At most this many messages are handled before
      // other tasks get to run: a read whose bytes are already there may return without letting them run, so
      // the count goes on across reads until the loop yields
      const size_t messages_between_yields = 256;
      static_assert(GRAPHENE_NET_HANDSHAKE_BUFFER_SIZE % 16 == 0 && GRAPHENE_NET_CONNECTION_BUFFER_SIZE % 16 == 0,
                    "reads must be whole AES blocks");
      static_assert(sizeof(message_header) <= 16, "a message header must fit in the first AES block");

      _connected_time = fc::time_point::now();
      // a message handler may close the socket, after which asking it for the endpoint fails
      fc::optional<fc::ip::endpoint> remote_endpoint;

      fc::oexception exception_to_rethrow;
      bool call_on_connection_closed = false;

      try
      {
        remote_endpoint = get_socket().remote_endpoint();
        // decrypted bytes not yet handed out are [buffer_begin, buffer_end); every message takes a whole
        // number of AES blocks, so both stay multiples of 16
        size_t read_buffer_size = _buffer_size;
        std::unique_ptr<char[]> buffer(new char[read_buffer_size]);
        size_t buffer_begin = 0;
        size_t buffer_end = 0;
        size_t messages_since_last_yield = 0;
        auto grow_buffer = [&]() {
          std::unique_ptr<char[]> grown(new char[_buffer_size]);
          memcpy(grown.get(), buffer.get() + buffer_begin, buffer_end - buffer_begin);
          buffer = std::move(grown);
          buffer_end -= buffer_begin;
          buffer_begin = 0;
          read_buffer_size = _buffer_size;
        };
        auto buffer_at_least = [&](size_t byte_count) {
          if (buffer_end - buffer_begin >= byte_count)
            return;
          memmove(buffer.get(), buffer.get() + buffer_begin, buffer_end - buffer_begin);
          buffer_end -= buffer_begin;
          buffer_begin = 0;
          while (buffer_end < byte_count)
            buffer_end += _sock.readsome(buffer.get() + buffer_end, read_buffer_size - buffer_end);
        };

        message m;
        while( true )
        {
          if (read_buffer_size != _buffer_size)
            grow_buffer();
          buffer_at_least(16);
          memcpy((char*)&m, buffer.get() + buffer_begin, sizeof(message_header));

          // checked before anything is allocated for the message
          FC_ASSERT( m.size <= _maximum_message_size, "", ("m.size",m.size)("maximum_message_size",_maximum_message_size) );

          const size_t size_with_padding = 16 * ((sizeof(message_header) + m.size + 15) / 16);
          if (size_with_padding <= read_buffer_size)
          {
            buffer_at_least(size_with_padding);
            const char* payload = buffer.get() + buffer_begin + sizeof(message_header);
            m.data.assign(payload, payload + m.size);
            buffer_begin += size_with_padding;
          }
          else
          {
            // too big for the buffer: take what it holds, then read the rest straight into the message
            const size_t buffered_bytes = buffer_end - buffer_begin;
            m.data.resize(size_with_padding - sizeof(message_header));
            std::copy(buffer.get() + buffer_begin + sizeof(message_header), buffer.get() + buffer_end, m.data.begin());
            buffer_begin = buffer_end = 0;
            fc::time_point read_first_bytes_time = fc::time_point::now();
            _sock.read(&m.data[buffered_bytes - sizeof(message_header)], size_with_padding - buffered_bytes);
            dlog("read the last ${bytes} bytes of a ${type} message from ${peer} in ${read_duration}us",
                 ("bytes", size_with_padding - buffered_bytes)("type", (core_message_type_enum)m.msg_type)
                 ("peer", remote_endpoint)("read_duration", fc::time_point::now() - read_first_bytes_time));
            m.data.resize(m.size); // truncate off the padding bytes
          }
          _bytes_received += size_with_padding;
          dlog("read complete ${type} message from ${peer}, decoding and passing to node",
               ("type", (core_message_type_enum)m.msg_type)("peer", remote_endpoint));

          _last_message_received_time = fc::time_point::now();

          try
          {
            // message handling errors are warnings...
            _delegate->on_message(_self, m);
            dlog("node is done handling message from ${peer}", ("peer", remote_endpoint));
          }
          /// Dedicated catches needed to distinguish from general fc::exception
          catch ( const fc::canceled_exception& e ) { throw e; }
          catch ( const fc::eof_exception& e ) { throw e; }
          catch ( const fc::exception& e)
          {
            /// Here loop should be continued so exception should be just caught locally.
            if (!_closed_locally)
              wlog( "message transmission failed ${er}", ("er", e.to_detail_string() ) );
            throw;
          }

          if (!_closed_locally && ++messages_since_last_yield >= messages_between_yields)
          {
            fc::yield();
            messages_since_last_yield = 0;
          }
          // a connection closed by the handler, or by another task while this one yielded, gets no more
          // of its messages; the next read ends the loop
          if (_closed_locally)
            buffer_begin = buffer_end;
        }
      }
      catch ( const fc::canceled_exception& e )
      {
        wlog( "caught a canceled_exception in read_loop.  this should mean we're in the process of deleting this object already, so there's no need to notify the delegate: ${e}", ("e", e.to_detail_string() ) );
        throw;
      }
      catch ( const fc::eof_exception& e )
      {
        wlog( "disconnected ${e}", ("e", e.to_detail_string() ) );
        call_on_connection_closed = true;
      }
      catch ( const fc::exception& e )
      {
        call_on_connection_closed = true;
        // reading from a socket we closed ourselves (e.g. after rejecting the peer's hello) fails
        // with "Bad file descriptor" or "Operation canceled"; that is the normal end of the connection
        if (_closed_locally)
          dlog( "read loop for ${peer} ended after the connection was closed locally: ${er}", ("peer", remote_endpoint)("er", e.to_string() ) );
        else
        {
          elog( "disconnected ${er}", ("er", e.to_detail_string() ) );
          exception_to_rethrow = fc::unhandled_exception(FC_LOG_MESSAGE(warn, "disconnected: ${e}", ("e", e.to_detail_string())));
        }
      }
      catch ( const std::exception& e )
      {
        elog( "disconnected ${er}", ("er", e.what() ) );
        call_on_connection_closed = true;
        exception_to_rethrow = fc::unhandled_exception(FC_LOG_MESSAGE(warn, "disconnected: ${e}", ("e", e.what())));
      }
      catch ( ... )
      {
        elog( "unexpected exception" );
        call_on_connection_closed = true;
        exception_to_rethrow = fc::unhandled_exception(FC_LOG_MESSAGE(warn, "disconnected: ${e}", ("e", fc::except_str())));
      }

      if (call_on_connection_closed)
        _delegate->on_connection_closed(_self);

      if (exception_to_rethrow)
        throw *exception_to_rethrow;
    }

    void message_oriented_connection_impl::send_message(const message& message_to_send)
    {
      VERIFY_CORRECT_THREAD();
      const message_to_write bytes_to_send{message_to_send, message_to_send.data.data()};
      send_padded_messages(&bytes_to_send, 1);
    }

    void message_oriented_connection_impl::send_messages(const std::vector<message_to_write>& messages_to_send)
    {
      VERIFY_CORRECT_THREAD();
      if (!messages_to_send.empty())
        send_padded_messages(messages_to_send.data(), messages_to_send.size());
    }

    void message_oriented_connection_impl::send_padded_messages(const message_to_write* messages_to_send, size_t message_count)
    {
      VERIFY_CORRECT_THREAD();
      struct verify_no_send_in_progress {
        bool& var;
        verify_no_send_in_progress(bool& var) : var(var)
        {
          if (var)
            elog("Error: two tasks are calling message_oriented_connection::send_message() at the same time");
          assert(!var);
          var = true;
        }
        ~verify_no_send_in_progress() { var = false; }
      } _verify_no_send_in_progress(_send_message_in_progress);

      try
      {
        // each message is padded to a multiple of 16 bytes on its own, so a batch puts on the wire
        // exactly the bytes the same messages sent one by one would
        size_t total_size_with_padding = 0;
        for (size_t i = 0; i < message_count; ++i)
        {
          if( messages_to_send[i].header.size > MAX_MESSAGE_SIZE )
             elog("Trying to send a message larger than MAX_MESSAGE_SIZE. This probably won't work...");
          total_size_with_padding += 16 * ((sizeof(message_header) + messages_to_send[i].header.size + 15) / 16);
        }
        std::unique_ptr<char[]> padded_messages(new char[total_size_with_padding]);

        char* next_message = padded_messages.get();
        for (size_t i = 0; i < message_count; ++i)
        {
          const message_to_write& message_to_send = messages_to_send[i];
          size_t size_of_message_and_header = sizeof(message_header) + message_to_send.header.size;
          size_t size_with_padding = 16 * ((size_of_message_and_header + 15) / 16);
          memcpy(next_message, (char*)&message_to_send.header, sizeof(message_header));
          memcpy(next_message + sizeof(message_header), message_to_send.payload, message_to_send.header.size );
          memset(next_message + size_of_message_and_header, 0, size_with_padding - size_of_message_and_header);
          next_message += size_with_padding;
        }

        _sock.write(padded_messages.get(), total_size_with_padding);
        _sock.flush();
        _bytes_sent += total_size_with_padding;
        _last_message_sent_time = fc::time_point::now();
      } FC_RETHROW_EXCEPTIONS( warn, "unable to send message" );
    }

    void message_oriented_connection_impl::close_connection()
    {
      VERIFY_CORRECT_THREAD();
      _closed_locally = true;
      _sock.close();
    }

    void message_oriented_connection_impl::allow_full_size_messages()
    {
      VERIFY_CORRECT_THREAD();
      _maximum_message_size = MAX_MESSAGE_SIZE;
      _buffer_size = GRAPHENE_NET_CONNECTION_BUFFER_SIZE;
      _sock.set_buffer_size(GRAPHENE_NET_CONNECTION_BUFFER_SIZE);
    }

    void message_oriented_connection_impl::destroy_connection(const char* caller)
    {
      VERIFY_CORRECT_THREAD();

      fc::optional<fc::ip::endpoint> remote_endpoint;
      if (_sock.get_socket().is_open())
        remote_endpoint = _sock.get_socket().remote_endpoint();
      ilog( "in destroy_connection(${caller}) for `${endpoint}`", ("caller", caller)("endpoint", remote_endpoint) );

      if (_send_message_in_progress)
        elog("Error: message_oriented_connection is being destroyed while a send_message is in progress.  "
             "The task calling send_message() should have been canceled already");
      assert(!_send_message_in_progress);

      try
      {
        _read_loop_done.cancel_and_wait(__FUNCTION__);
      }
      catch ( const fc::exception& e )
      {
        wlog( "Exception thrown while canceling message_oriented_connection's read_loop, ignoring: ${e}", ("e",e) );
      }
      catch (...)
      {
        wlog( "Exception thrown while canceling message_oriented_connection's read_loop, ignoring" );
      }
    }

    uint64_t message_oriented_connection_impl::get_total_bytes_sent() const
    {
      VERIFY_CORRECT_THREAD();
      return _bytes_sent;
    }

    uint64_t message_oriented_connection_impl::get_total_bytes_received() const
    {
      VERIFY_CORRECT_THREAD();
      return _bytes_received;
    }

    fc::time_point message_oriented_connection_impl::get_last_message_sent_time() const
    {
      VERIFY_CORRECT_THREAD();
      return _last_message_sent_time;
    }

    fc::time_point message_oriented_connection_impl::get_last_message_received_time() const
    {
      VERIFY_CORRECT_THREAD();
      return _last_message_received_time;
    }

    fc::sha512 message_oriented_connection_impl::get_shared_secret() const
    {
      VERIFY_CORRECT_THREAD();
      return _sock.get_shared_secret();
    }

  } // end namespace graphene::net::detail


  message_oriented_connection::message_oriented_connection(message_oriented_connection_delegate* delegate) :
    my(new detail::message_oriented_connection_impl(this, delegate))
  {
  }

  message_oriented_connection::~message_oriented_connection()
  {
  }

  fc::tcp_socket& message_oriented_connection::get_socket()
  {
    return my->get_socket();
  }

  void message_oriented_connection::accept()
  {
    my->accept();
  }

  void message_oriented_connection::connect_to(const fc::ip::endpoint& remote_endpoint)
  {
    my->connect_to(remote_endpoint);
  }

  void message_oriented_connection::bind(const fc::ip::endpoint& local_endpoint)
  {
    my->bind(local_endpoint);
  }

  void message_oriented_connection::send_message(const message& message_to_send)
  {
    my->send_message(message_to_send);
  }

  void message_oriented_connection::send_messages(const std::vector<message_to_write>& messages_to_send)
  {
    my->send_messages(messages_to_send);
  }

  void message_oriented_connection::close_connection()
  {
    my->close_connection();
  }

  void message_oriented_connection::destroy_connection(const char* caller)
  {
    my->destroy_connection(caller);
  }

  void message_oriented_connection::allow_full_size_messages()
  {
    my->allow_full_size_messages();
  }

  uint64_t message_oriented_connection::get_total_bytes_sent() const
  {
    return my->get_total_bytes_sent();
  }

  uint64_t message_oriented_connection::get_total_bytes_received() const
  {
    return my->get_total_bytes_received();
  }

  fc::time_point message_oriented_connection::get_last_message_sent_time() const
  {
    return my->get_last_message_sent_time();
  }

  fc::time_point message_oriented_connection::get_last_message_received_time() const
  {
    return my->get_last_message_received_time();
  }
  fc::time_point message_oriented_connection::get_last_write_progress_time() const
  {
    return my->get_last_write_progress_time();
  }
  fc::time_point message_oriented_connection::get_connection_time() const
  {
    return my->get_connection_time();
  }
  fc::sha512 message_oriented_connection::get_shared_secret() const
  {
    return my->get_shared_secret();
  }

} } // end namespace graphene::net
