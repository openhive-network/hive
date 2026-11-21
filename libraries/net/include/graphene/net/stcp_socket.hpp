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
#pragma once
#include <fc/network/tcp_socket.hpp>
#include <fc/crypto/aes.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/time.hpp>

namespace graphene { namespace net {

/**
 *  Uses ECDH to negotiate a aes key for communicating
 *  with other nodes on the network.
 */
class stcp_socket : public virtual fc::iostream
{
  public:
    stcp_socket();
    ~stcp_socket();
    fc::tcp_socket&  get_socket() { return _sock; }
    void             accept();

    void             connect_to( const fc::ip::endpoint& remote_endpoint );
    void             bind( const fc::ip::endpoint& local_endpoint );

    virtual size_t   readsome( char* buffer, size_t max );
    virtual size_t   readsome( const std::shared_ptr<char>& buf, size_t len, size_t offset );
    virtual bool     eof()const;

    virtual size_t   writesome( const char* buffer, size_t len );
    virtual size_t   writesome( const std::shared_ptr<const char>& buf, size_t len, size_t offset );

    virtual void     flush();
    virtual void     close();

    using istream::get;
    void             get( char& c ) { read( &c, 1 ); }
    fc::sha512       get_shared_secret() const { return _shared_secret; }
    /** the most one read or write takes, and the size of each of the read and write buffers (a multiple of 16);
     *  a buffer of another size is replaced by the next read or write that uses it */
    void             set_buffer_size( size_t buffer_size );
    /// when a write last put bytes into the socket
    fc::time_point   get_last_write_time() const { return _last_write_time; }
  private:
    void do_key_exchange();

    fc::sha512           _shared_secret;
    fc::ecc::private_key _priv_key;
    fc::array<char,8>    _buf;
    //uint32_t             _buf_len;
    fc::tcp_socket       _sock;
    fc::aes_encoder      _send_aes;
    fc::aes_decoder      _recv_aes;
    size_t                _buffer_size;
    std::shared_ptr<char> _read_buffer;
    size_t                _read_buffer_size = 0;
    std::shared_ptr<char> _write_buffer;
    size_t                _write_buffer_size = 0;
    fc::time_point        _last_write_time;
#ifndef NDEBUG
    bool _read_buffer_in_use;
    bool _write_buffer_in_use;
#endif
};

typedef std::shared_ptr<stcp_socket> stcp_socket_ptr;

} } // graphene::net
