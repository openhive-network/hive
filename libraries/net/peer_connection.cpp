/*
 * Copyright (c) 2020-2026 Hive blockchain community contributors.
 * Hive is a decentralized, open-source blockchain project maintained by its community.
 *
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
#include <graphene/net/peer_connection.hpp>
#include <graphene/net/exceptions.hpp>
#include <graphene/net/config.hpp>
#include <hive/protocol/config.hpp>
#include <hive/chain/full_transaction.hpp>

#include <fc/thread/thread.hpp>

#include <boost/core/demangle.hpp>
#include <boost/scope_exit.hpp>

#ifdef DEFAULT_LOGGER
# undef DEFAULT_LOGGER
#endif
#define DEFAULT_LOGGER "p2p"

#ifndef NDEBUG
# define VERIFY_CORRECT_THREAD() assert(_thread->is_current())
#else
# define VERIFY_CORRECT_THREAD() do {} while (0)
#endif

namespace graphene { namespace net
  {
    namespace
    {
      /// transactions, and the inventories and fetch requests that relay them; everything else (blocks and
      /// their inventories, sync and connection control) is written without waiting for a batch to grow
      bool is_transaction_traffic(const message& m)
      {
        if (m.msg_type == trx_message_type)
          return true;
        if (m.msg_type != item_ids_inventory_message_type && m.msg_type != fetch_items_message_type)
          return false;
        // item_type is the first field of both messages
        uint32_t item_type = 0;
        if (m.data.size() < sizeof(item_type))
          return false;
        memcpy(&item_type, m.data.data(), sizeof(item_type));
        return item_type == trx_message_type;
      }
    }

    message_to_write peer_connection::queued_message::get_message_to_write(peer_connection_delegate* node, peer_connection* peer,
                                                                          std::deque<message>& built_messages)
    {
      const message& built_message = built_messages.emplace_back(get_message(node, peer));
      return message_to_write{built_message, built_message.data.data()};
    }

    void peer_connection::real_queued_message::patch_send_time()
    {
      if (message_send_time_field_offset != (size_t)-1)
      {
        // patch the current time into the message.  Since this operates on the packed version of the structure,
        // it won't work for anything after a variable-length field
        std::vector<char> packed_current_time = fc::raw::pack_to_vector(fc::time_point::now());
        assert(message_send_time_field_offset + packed_current_time.size() <= message_to_send.data.size());
        memcpy(message_to_send.data.data() + message_send_time_field_offset,
               packed_current_time.data(), packed_current_time.size());
      }
    }
    message peer_connection::real_queued_message::get_message(peer_connection_delegate*, peer_connection*)
    {
      patch_send_time();
      return message_to_send;
    }
    message_to_write peer_connection::real_queued_message::get_message_to_write(peer_connection_delegate*, peer_connection*,
                                                                                std::deque<message>&)
    {
      patch_send_time();
      return message_to_write{message_to_send, message_to_send.data.data()};
    }
    size_t peer_connection::real_queued_message::get_size_in_queue()
    {
      return message_to_send.data.size();
    }

    peer_connection::queued_transaction_message::queued_transaction_message(const std::shared_ptr<full_transaction_type>& full_transaction) :
      full_transaction(full_transaction)
    {
      FC_ASSERT(full_transaction, "Can't send a transaction_message when we don't have the full_transaction");
      may_wait_for_batch = true;
    }
    message peer_connection::queued_transaction_message::get_message(peer_connection_delegate*, peer_connection*)
    {
      return trx_message(full_transaction);
    }
    message_to_write peer_connection::queued_transaction_message::get_message_to_write(peer_connection_delegate*, peer_connection*,
                                                                                       std::deque<message>&)
    {
      // the same header and payload message(const trx_message&) builds, without copying the payload
      const hive::chain::serialized_transaction_data& serialized_transaction = full_transaction->get_serialized_transaction();
      message_to_write transaction_to_write;
      transaction_to_write.header.msg_type = trx_message::type;
      transaction_to_write.header.size = (uint32_t)full_transaction->get_transaction_size();
      transaction_to_write.payload = serialized_transaction.begin;
      return transaction_to_write;
    }
    size_t peer_connection::queued_transaction_message::get_size_in_queue()
    {
      // the payload is shared, but counting its size keeps the queue limit and the full-batch flush the same
      // as for a transaction queued as a copy
      return full_transaction->get_transaction_size();
    }
    message peer_connection::virtual_queued_block_message::get_message(peer_connection_delegate* node, peer_connection* peer)
    {
      prepare(node, peer);
      if (!full_block)
        return item_not_available_message(item_id(block_message_type, requested_item));
      if (peer->supports_compressed_blocks())
      {
        // the peer can handle some form of compressed data.
        if (peer->requires_alternate_compression_for_block(full_block))
        {
          //fc_ilog(fc::logger::get("default"), "sending ALTERNATE compressed block");
          return message(full_block->get_alternate_compressed_block());
        }
        else
          return message(full_block->get_compressed_block());
      }
      else
        return block_message(full_block);
    }

    void peer_connection::virtual_queued_block_message::prepare(peer_connection_delegate* node, peer_connection* peer)
    {
      if (looked_up)
        return;
      looked_up = true;
      try
      {
        full_block = node->get_block_to_send(peer, requested_item);
      }
      catch (const fc::canceled_exception&)
      {
        throw;
      }
      catch (const fc::exception& e)
      {
        // the peer is told we don't have the block, rather than the send queue stopping at this message
        wlog("Unable to look up block ${id} requested by peer ${peer}: ${e}",
             ("id", requested_item)("peer", peer->get_remote_endpoint())("e", e.to_string()));
      }
      catch (const std::exception& e)
      {
        wlog("Unable to look up block ${id} requested by peer ${peer}: ${e}",
             ("id", requested_item)("peer", peer->get_remote_endpoint())("e", e.what()));
      }
    }

    size_t peer_connection::virtual_queued_block_message::get_size_in_queue()
    {
      return sizeof(*this);
    }

    peer_connection::peer_connection(peer_connection_delegate* delegate) :
      _node(delegate),
      _message_connection(this),
      direction(peer_connection_direction::unknown),
      is_firewalled(firewalled_state::unknown),
      our_state(our_connection_state::disconnected),
      they_have_requested_close(false),
      their_state(their_connection_state::disconnected),
      we_have_requested_close(false),
      negotiation_status(connection_negotiation_status::disconnected),
      peer_needs_sync_items_from_us(true),
      we_need_sync_items_from_peer(true),
      inhibit_fetching_sync_blocks(false),
      transaction_fetching_inhibited_until(fc::time_point::min()),
      firewall_check_state(nullptr),
#ifndef NDEBUG
      _thread(&fc::thread::current()),
#endif
      _currently_handling_message(false)
    {
    }

    peer_connection_ptr peer_connection::make_shared(peer_connection_delegate* delegate)
    {
      // The lifetime of peer_connection objects is managed by shared_ptrs in node.  The peer_connection
      // is responsible for notifying the node when it should be deleted, and the process of deleting it
      // cleans up the peer connection's asynchronous tasks which are responsible for notifying the node
      // when it should be deleted.
      // To ease this vicious cycle, we slightly delay the execution of the destructor until the
      // current task yields.  In the (not uncommon) case where it is the task executing
      // connect_to or read_loop, this allows the task to finish before the destructor is forced
      // to cancel it.
      return peer_connection_ptr(new peer_connection(delegate));
      //, [](peer_connection* peer_to_delete){ fc::async([peer_to_delete](){delete peer_to_delete;}); });
    }

    void peer_connection::destroy(const char* caller)
    {
      VERIFY_CORRECT_THREAD();

#if 0 // this gets too verbose
#ifndef NDEBUG
      struct scope_logger {
        fc::optional<fc::ip::endpoint> endpoint;
        scope_logger(const fc::optional<fc::ip::endpoint>& endpoint) : endpoint(endpoint) { dlog("entering peer_connection::destroy() for peer ${endpoint}", ("endpoint", endpoint)); }
        ~scope_logger() { dlog("leaving peer_connection::destroy() for peer ${endpoint}", ("endpoint", endpoint)); }
      } send_message_scope_logger(get_remote_endpoint());
#endif
#endif

      try
      {
        dlog("calling close_connection()");
        close_connection();
        dlog("close_connection completed normally");
      }
      catch ( const fc::canceled_exception& )
      {
        assert(false && "the task that deletes peers should not be canceled because it will prevent us from cleaning up correctly");
      }
      catch ( ... )
      {
        dlog("close_connection threw");
      }

      try
      {
        dlog("canceling _send_queued_messages task");
        _send_queued_messages_done.cancel_and_wait(__FUNCTION__);
        dlog("cancel_and_wait completed normally");
      }
      catch( const fc::exception& e )
      {
        wlog("Unexpected exception from peer_connection's send_queued_messages_task : ${e}", ("e", e));
      }
      catch( ... )
      {
        wlog("Unexpected exception from peer_connection's send_queued_messages_task");
      }

      try
      {
        dlog("canceling accept_or_connect_task");
        accept_or_connect_task_done.cancel_and_wait(__FUNCTION__);
        dlog("accept_or_connect_task completed normally");
      }
      catch( const fc::exception& e )
      {
        wlog("Unexpected exception from peer_connection's accept_or_connect_task : ${e}", ("e", e));
      }
      catch( ... )
      {
        wlog("Unexpected exception from peer_connection's accept_or_connect_task");
      }

      _message_connection.destroy_connection(caller); // shut down the read loop
    }

    peer_connection::~peer_connection()
    {
      VERIFY_CORRECT_THREAD();
      destroy(__FUNCTION__);
    }

    fc::tcp_socket& peer_connection::get_socket()
    {
      VERIFY_CORRECT_THREAD();
      return _message_connection.get_socket();
    }

    void peer_connection::accept_connection()
    {
      VERIFY_CORRECT_THREAD();

      struct scope_logger {
        scope_logger() { dlog("entering peer_connection::accept_connection()"); }
        ~scope_logger() { dlog("leaving peer_connection::accept_connection()"); }
      } accept_connection_scope_logger;

      try
      {
        assert( our_state == our_connection_state::disconnected &&
                their_state == their_connection_state::disconnected );
        direction = peer_connection_direction::inbound;
        negotiation_status = connection_negotiation_status::accepting;
        _message_connection.accept();           // perform key exchange
        negotiation_status = connection_negotiation_status::accepted;
        _remote_endpoint = _message_connection.get_socket().remote_endpoint();

        // firewall-detecting info is pretty useless for inbound connections, but initialize
        // it the best we can
        fc::ip::endpoint local_endpoint = _message_connection.get_socket().local_endpoint();
        inbound_address = local_endpoint.get_address();
        inbound_port = local_endpoint.port();
        outbound_port = inbound_port;

        their_state = their_connection_state::just_connected;
        our_state = our_connection_state::just_connected;
        ilog( "established inbound connection from ${remote_endpoint}, sending hello", ("remote_endpoint", _message_connection.get_socket().remote_endpoint() ) );
      }
      catch ( const fc::exception& e )
      {
        wlog( "error accepting connection ${e}", ("e", e.to_detail_string() ) );
        throw;
      }
    }

    void peer_connection::connect_to( const fc::ip::endpoint& remote_endpoint, fc::optional<fc::ip::endpoint> local_endpoint )
    {
      VERIFY_CORRECT_THREAD();
      try
      {
        assert( our_state == our_connection_state::disconnected &&
                their_state == their_connection_state::disconnected );
        direction = peer_connection_direction::outbound;

        _remote_endpoint = remote_endpoint;
        if( local_endpoint )
        {
          // the caller wants us to bind the local side of this socket to a specific ip/port
          // This depends on the ip/port being unused, and on being able to set the
          // SO_REUSEADDR/SO_REUSEPORT flags, and either of these might fail, so we need to
          // detect if this fails.
          try
          {
            _message_connection.bind( *local_endpoint );
          }
          catch ( const fc::canceled_exception& )
          {
            throw;
          }
          catch ( const fc::exception& except )
          {
            wlog( "Failed to bind to desired local endpoint ${endpoint}, will connect using an OS-selected endpoint: ${except}", ("endpoint", *local_endpoint )("except", except ) );
          }
        }
        negotiation_status = connection_negotiation_status::connecting;
        _message_connection.connect_to( remote_endpoint );
        negotiation_status = connection_negotiation_status::connected;
        their_state = their_connection_state::just_connected;
        our_state = our_connection_state::just_connected;
        ilog( "established outbound connection to ${remote_endpoint}", ("remote_endpoint", remote_endpoint ) );
      }
      catch ( fc::exception& e )
      {
        elog( "fatal: error connecting to peer ${remote_endpoint}: ${e}", ("remote_endpoint", remote_endpoint )("e", e.to_detail_string() ) );
        throw;
      }
    } // connect_to()

    void peer_connection::on_message( message_oriented_connection* originating_connection, const message& received_message )
    {
      VERIFY_CORRECT_THREAD();
      _currently_handling_message = true;
      BOOST_SCOPE_EXIT(this_) {
        this_->_currently_handling_message = false;
      } BOOST_SCOPE_EXIT_END
      _node->on_message( this, received_message );
    }

    void peer_connection::on_connection_closed( message_oriented_connection* originating_connection )
    {
      VERIFY_CORRECT_THREAD();
      negotiation_status = connection_negotiation_status::closed;
      _node->on_connection_closed( this );
    }

    void peer_connection::send_queued_messages_task()
    {
      VERIFY_CORRECT_THREAD();
#ifndef NDEBUG
      struct counter {
        unsigned& _send_message_queue_tasks_counter;
        counter(unsigned& var) : _send_message_queue_tasks_counter(var) { /* dlog("entering peer_connection::send_queued_messages_task()"); */ assert(_send_message_queue_tasks_counter == 0); ++_send_message_queue_tasks_counter; }
        ~counter() { assert(_send_message_queue_tasks_counter == 1); --_send_message_queue_tasks_counter; /* dlog("leaving peer_connection::send_queued_messages_task()"); */ }
      } concurrent_invocation_counter(_send_message_queue_tasks_running);
#endif
      while (!_queued_messages.empty())
      {
        wait_for_transaction_batch_to_grow();

        // start the lookups (and the worker-thread compression) of the next few queued blocks before their turn;
        // only this task removes messages from the queue, so the ones prepared here are still queued or in the batch
        for (size_t i = 0; i < _queued_messages.size() && i < GRAPHENE_NET_BLOCK_MESSAGES_PREPARED_AHEAD; ++i)
          _queued_messages[i]->prepare(_node, this);

        // take everything queued, up to the batch bound, so it goes out in one socket write;
        // get_message_to_write() is still called here, at send time, because block messages are built lazily
        std::vector<std::unique_ptr<queued_message>> batch;
        std::vector<message_to_write> messages_to_send;
        // messages built at send time; the queued ones in `batch` and the shared transactions are written in place
        std::deque<message> built_messages;
        // every popped message, sent or dropped, leaves the queue's size exactly once, however this batch ends
        BOOST_SCOPE_EXIT(this_, &batch) {
          for (const std::unique_ptr<queued_message>& popped_message : batch)
            this_->_total_queued_messages_size -= popped_message->get_size_in_queue();
        } BOOST_SCOPE_EXIT_END
        size_t batch_size = 0;
        do
        {
          batch.emplace_back(std::move(_queued_messages.front()));
          _queued_messages.pop_front();
          if (!batch.back()->may_wait_for_batch)
            --_queued_messages_not_waiting_for_batch;
          batch.back()->transmission_start_time = fc::time_point::now();
          if (!build_queued_message(*batch.back(), batch.size() - 1, messages_to_send, built_messages))
            break; // send what was built before it; the rest of the queue goes in the next batch
          batch_size += sizeof(message_header) + messages_to_send.back().header.size;
        } while (!_queued_messages.empty() && batch_size < GRAPHENE_NET_MAXIMUM_BATCHED_SEND_BYTES);
        try
        {
          _message_connection.send_messages(messages_to_send);
          if (!messages_to_send.empty())
            _node->on_messages_written(messages_to_send.size(), batch_size);
        }
        catch (const fc::canceled_exception&)
        {
          dlog("message_oriented_connection::send_messages() was canceled, rethrowing canceled_exception");
          throw;
        }
        catch (const fc::exception& send_error)
        {
          elog("Error sending message: ${exception}.  Closing connection.", ("exception", send_error));
          try
          {
            close_connection();
          }
          catch (const fc::exception& close_error)
          {
            elog("Caught error while closing connection: ${exception}", ("exception", close_error));
          }
          return;
        }
        catch (const std::exception& e)
        {
          elog("message_oriented_exception::send_messages() threw a std::exception(): ${what}", ("what", e.what()));
        }
        catch (...)
        {
          elog("message_oriented_exception::send_messages() threw an unhandled exception");
        }
        const fc::time_point transmission_finish_time = fc::time_point::now();
        for (const std::unique_ptr<queued_message>& sent_message : batch)
          sent_message->transmission_finish_time = transmission_finish_time;
      }
      //dlog("leaving peer_connection::send_queued_messages_task() due to queue exhaustion");
    }

    bool peer_connection::build_queued_message(queued_message& message_to_build, size_t queue_position,
                                               std::vector<message_to_write>& messages_to_write, std::deque<message>& built_messages)
    {
      // a message that cannot be built (a block whose compression fails) is dropped on its own, so the
      // peer still gets everything else; if it asked for this item, it asks again or times out the request
      try
      {
        messages_to_write.emplace_back(message_to_build.get_message_to_write(_node, this, built_messages));
        return true;
      }
      catch (const fc::canceled_exception&)
      {
        throw;
      }
      catch (const fc::exception& e)
      {
        elog("Dropping queued ${type} at queue position ${position}, building it failed: ${e}",
             ("type", boost::core::demangle(typeid(message_to_build).name()))("position", queue_position)("e", e.to_detail_string()));
      }
      catch (const std::exception& e)
      {
        elog("Dropping queued ${type} at queue position ${position}, building it threw a std::exception: ${what}",
             ("type", boost::core::demangle(typeid(message_to_build).name()))("position", queue_position)("what", e.what()));
      }
      catch (...)
      {
        elog("Dropping queued ${type} at queue position ${position}, building it threw an unhandled exception",
             ("type", boost::core::demangle(typeid(message_to_build).name()))("position", queue_position));
      }
      return false;
    }

    bool peer_connection::send_flush_wait_is_over() const
    {
      return _queued_messages_not_waiting_for_batch > 0 ||
             _total_queued_messages_size >= GRAPHENE_NET_MAXIMUM_BATCHED_SEND_BYTES;
    }

    void peer_connection::wait_for_transaction_batch_to_grow()
    {
      if (_send_flush_delay.count() <= 0 || send_flush_wait_is_over())
        return;
      const fc::time_point flush_deadline = _queued_messages.front()->enqueue_time + _send_flush_delay;
      if (flush_deadline <= fc::time_point::now())
        return;

      // a local reference keeps the promise alive while send_queueable_message() fulfills and drops the member;
      // the wait does not hold the connection's send guard, and canceling this task ends it like any fc wait
      fc::promise<void>::ptr delay_ended = fc::promise<void>::create("graphene::net::peer_connection::send_flush_delay_ended");
      _send_flush_delay_ended = delay_ended;
      BOOST_SCOPE_EXIT(this_) { this_->_send_flush_delay_ended.reset(); } BOOST_SCOPE_EXIT_END
      try
      {
        delay_ended->wait_until(flush_deadline);
      }
      catch (const fc::timeout_exception&) //intentionally not logged, the delay elapsed
      {
      }
    }

    void peer_connection::send_queueable_message(std::unique_ptr<queued_message>&& message_to_send)
    {
      VERIFY_CORRECT_THREAD();
      // once the queue has reached its limit the connection is closed, so nothing queued after that would ever
      // be sent: it isn't queued at all, however much the peer's last request asked for
      if (_send_queue_limit_exceeded)
        return;
      const size_t message_size = message_to_send->get_size_in_queue();
      if (_total_queued_messages_size + message_size > GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES)
      {
        _send_queue_limit_exceeded = true;
        elog("send queue exceeded maximum size of ${max} bytes (current size ${current} bytes)",
             ("max", GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES)("current", _total_queued_messages_size + message_size));
        try
        {
          close_connection();
        }
        catch (const fc::exception& e)
        {
          elog("Caught error while closing connection: ${exception}", ("exception", e));
        }
        return;
      }
      _total_queued_messages_size += message_size;
      if (!message_to_send->may_wait_for_batch)
        ++_queued_messages_not_waiting_for_batch;
      _queued_messages.emplace_back(std::move(message_to_send));

      if( _send_queued_messages_done.valid() && _send_queued_messages_done.canceled() )
        FC_THROW_EXCEPTION(fc::exception, "Attempting to send a message on a connection that is being shut down");

      if (_send_flush_delay_ended && send_flush_wait_is_over())
      {
        _send_flush_delay_ended->set_value();
        _send_flush_delay_ended.reset();
      }

      if (!_send_queued_messages_done.valid() || _send_queued_messages_done.ready())
      {
        //dlog("peer_connection::send_message() is firing up send_queued_message_task");
        _send_queued_messages_done = fc::async([this](){ send_queued_messages_task(); }, "send_queued_messages_task");
      }
      //else
      //  dlog("peer_connection::send_message() doesn't need to fire up send_queued_message_task, it's already running");
    }

    void peer_connection::send_message(const message& message_to_send, size_t message_send_time_field_offset)
    {
      VERIFY_CORRECT_THREAD();
      //dlog("peer_connection::send_message() enqueueing message of type ${type} for peer ${endpoint}",
      //     ("type", message_to_send.msg_type)("endpoint", get_remote_endpoint()));
      std::unique_ptr<queued_message> message_to_enqueue(new real_queued_message(message_to_send, message_send_time_field_offset));
      message_to_enqueue->may_wait_for_batch = is_transaction_traffic(message_to_send);
      send_queueable_message(std::move(message_to_enqueue));
    }

    void peer_connection::set_send_flush_delay(fc::microseconds delay)
    {
      VERIFY_CORRECT_THREAD();
      _send_flush_delay = delay;
    }

    void peer_connection::send_block_message(const item_hash_t& requested_item)
    {
      VERIFY_CORRECT_THREAD();
      send_queueable_message(std::make_unique<virtual_queued_block_message>(requested_item));
    }

    void peer_connection::send_transaction_message(const std::shared_ptr<full_transaction_type>& full_transaction)
    {
      VERIFY_CORRECT_THREAD();
      send_queueable_message(std::make_unique<queued_transaction_message>(full_transaction));
    }

    void peer_connection::close_connection()
    {
      VERIFY_CORRECT_THREAD();
      negotiation_status = connection_negotiation_status::closing;
      if (connection_terminated_time != fc::time_point::min())
        connection_terminated_time = fc::time_point::now();
      _message_connection.close_connection();
    }

    void peer_connection::record_connection_closed_error(const fc::exception& reason, bool is_failure)
    {
      VERIFY_CORRECT_THREAD();
      if (!connection_closed_error)
      {
        connection_closed_error = reason;
        connection_closed_by_failure = is_failure;
      }
    }

    fc::exception peer_connection::get_connection_closed_reason() const
    {
      if (connection_closed_error)
        return *connection_closed_error;
      return fc::exception(FC_LOG_MESSAGE(info, "lost connection to peer"));
    }

    void peer_connection::record_connection_closed_in(potential_peer_record& record) const
    {
      record.last_error = get_connection_closed_reason();
      if (connection_closed_by_failure)
        record.last_failed_time = fc::time_point::now();
    }

    void peer_connection::destroy_connection(const char* caller)
    {
      VERIFY_CORRECT_THREAD();
      negotiation_status = connection_negotiation_status::closing;
      destroy(caller);
    }

    uint64_t peer_connection::get_total_bytes_sent() const
    {
      VERIFY_CORRECT_THREAD();
      return _message_connection.get_total_bytes_sent();
    }

    uint64_t peer_connection::get_total_bytes_received() const
    {
      VERIFY_CORRECT_THREAD();
      return _message_connection.get_total_bytes_received();
    }

    fc::time_point peer_connection::get_last_message_sent_time() const
    {
      VERIFY_CORRECT_THREAD();
      return _message_connection.get_last_message_sent_time();
    }

    fc::time_point peer_connection::get_last_message_received_time() const
    {
      VERIFY_CORRECT_THREAD();
      return _message_connection.get_last_message_received_time();
    }

    fc::optional<fc::ip::endpoint> peer_connection::get_remote_endpoint() const
    {
      VERIFY_CORRECT_THREAD();
      return _remote_endpoint;
    }
    fc::ip::endpoint peer_connection::get_local_endpoint()
    {
      VERIFY_CORRECT_THREAD();
      return _message_connection.get_socket().local_endpoint();
    }

    void peer_connection::set_remote_endpoint( fc::optional<fc::ip::endpoint> new_remote_endpoint )
    {
      VERIFY_CORRECT_THREAD();
      _remote_endpoint = new_remote_endpoint;
    }

    bool peer_connection::busy() const
    {
      VERIFY_CORRECT_THREAD();
      return !items_requested_from_peer.empty() || !sync_items_requested_from_peer.empty() || item_ids_requested_from_peer;
    }

    bool peer_connection::idle() const
    {
      VERIFY_CORRECT_THREAD();
      return !busy();
    }

    bool peer_connection::is_currently_handling_message() const
    {
      VERIFY_CORRECT_THREAD();
      return _currently_handling_message;
    }

    bool peer_connection::is_transaction_fetching_inhibited() const
    {
      VERIFY_CORRECT_THREAD();
      return transaction_fetching_inhibited_until > fc::time_point::now();
    }

    bool peer_connection::is_block_fetching_deprioritized() const
    {
      VERIFY_CORRECT_THREAD();
      return block_fetching_deprioritized_until > fc::time_point::now();
    }

    void peer_connection::deprioritize_block_fetching()
    {
      VERIFY_CORRECT_THREAD();
      block_fetching_deprioritized_until = fc::time_point::now() + fc::seconds(GRAPHENE_NET_BLOCK_BODY_MISMATCH_DISCONNECT_BREAKER_WINDOW_SEC);
    }

    void peer_connection::restore_block_fetching_priority()
    {
      VERIFY_CORRECT_THREAD();
      block_fetching_deprioritized_until = fc::time_point();
    }

    fc::sha512 peer_connection::get_shared_secret() const
    {
      VERIFY_CORRECT_THREAD();
      return _message_connection.get_shared_secret();
    }

    void peer_connection::clear_old_inventory()
    {
      VERIFY_CORRECT_THREAD();
      fc::time_point_sec oldest_inventory_to_keep(fc::time_point::now() - fc::minutes(GRAPHENE_NET_MAX_INVENTORY_SIZE_IN_MINUTES));

      // expire old items from block_inventory_advertised_to_peer
      auto oldest_inventory_to_keep_iter = block_inventory_advertised_to_peer.get<timestamp_index>().lower_bound(oldest_inventory_to_keep);
      auto begin_iter = block_inventory_advertised_to_peer.get<timestamp_index>().begin();
      unsigned number_of_elements_advertised_to_peer_to_discard = std::distance(begin_iter, oldest_inventory_to_keep_iter);
      block_inventory_advertised_to_peer.get<timestamp_index>().erase(begin_iter, oldest_inventory_to_keep_iter);

      // also expire items from block_inventory_peer_advertised_to_us
      oldest_inventory_to_keep_iter = block_inventory_peer_advertised_to_us.get<timestamp_index>().lower_bound(oldest_inventory_to_keep);
      begin_iter = block_inventory_peer_advertised_to_us.get<timestamp_index>().begin();
      unsigned number_of_elements_peer_advertised_to_discard = std::distance(begin_iter, oldest_inventory_to_keep_iter);
      block_inventory_peer_advertised_to_us.get<timestamp_index>().erase(begin_iter, oldest_inventory_to_keep_iter);

      // deferred advertisements this old have most likely fallen out of the peer's cache, so we couldn't fetch them anyway
      oldest_inventory_to_keep_iter = transaction_inventory_deferred.get<timestamp_index>().lower_bound(oldest_inventory_to_keep);
      begin_iter = transaction_inventory_deferred.get<timestamp_index>().begin();
      inventory_advertisements_dropped_since_report += std::distance(begin_iter, oldest_inventory_to_keep_iter);
      transaction_inventory_deferred.get<timestamp_index>().erase(begin_iter, oldest_inventory_to_keep_iter);
      if (number_of_elements_advertised_to_peer_to_discard > 0 || number_of_elements_peer_advertised_to_discard > 0)
        dlog("Expiring old block inventory for peer ${peer}: removing ${to_peer} blocks advertised to peer (${remain_to_peer} left), and ${to_us} advertised to us (${remain_to_us} left)",
             ("peer", get_remote_endpoint())
             ("to_peer", number_of_elements_advertised_to_peer_to_discard)("remain_to_peer", block_inventory_advertised_to_peer.size())
             ("to_us", number_of_elements_peer_advertised_to_discard)("remain_to_us", block_inventory_peer_advertised_to_us.size()));
    }

    // remember a transaction advertisement we can't record yet because the inventory is full;
    // when the deferred list is full as well, the advertisement is dropped
    void peer_connection::defer_transaction_advertisement(const item_id& item)
    {
      VERIFY_CORRECT_THREAD();
      if (transaction_inventory_deferred.size() >= GRAPHENE_NET_MAX_INVENTORY_SIZE_IN_MINUTES * GRAPHENE_NET_MAX_TRX_PER_SECOND * 60)
      {
        ++inventory_advertisements_dropped_since_report;
        return;
      }
      if (transaction_inventory_deferred.insert(timestamped_item_id(item, fc::time_point::now())).second)
        ++inventory_advertisements_deferred_since_report;
    }

    bool peer_connection::performing_firewall_check() const
    {
      return firewall_check_state && firewall_check_state->requesting_peer != node_id_t();
    }

    fc::optional<fc::ip::endpoint> peer_connection::get_endpoint_for_connecting() const
    {
      if (inbound_port)
        return fc::ip::endpoint(inbound_address, inbound_port);
      return fc::optional<fc::ip::endpoint>();
    }

    fc::optional<fc::ip::endpoint> peer_connection::get_endpoint_for_db() const
    {
      return direction == peer_connection_direction::inbound ? get_endpoint_for_connecting() : get_remote_endpoint();
    }

    bool peer_connection::supports_compressed_blocks() const
    {
      return core_protocol_version >= GRAPHENE_NET_PROTOCOL_COMPRESSED_BLOCKS_VERSION;
    }

    bool peer_connection::advertise_blocks_by_block_id() const
    {
      return core_protocol_version >= GRAPHENE_NET_PROTOCOL_ADVERTISE_BLOCKS_BY_BLOCK_ID_VERSION;
    }

    bool peer_connection::requires_alternate_compression_for_block(const std::shared_ptr<full_block_type>& full_block) const
    {
      assert(supports_compressed_blocks());
      if (full_block->has_compressed_block_data())
      {
        // the block is already compressed.  If this is compressed using a dictionary, can the peer understand it?
        const hive::chain::compressed_block_data& compressed_data = full_block->get_compressed_block();
        return compressed_data.compression_attributes.dictionary_number &&
               (!last_available_zstd_compression_dictionary_number ||
                *last_available_zstd_compression_dictionary_number < *compressed_data.compression_attributes.dictionary_number);
      }
      else
      {
        // the block isn't yet compressed.  Find out if this peer will understand compression
        // using the default dictionary we'd use, or if we need to compress without a dictionary
        // for this peer
        std::optional<uint8_t> default_dictionary = full_block->get_best_available_zstd_compression_dictionary_number();
        return default_dictionary && 
               (!last_available_zstd_compression_dictionary_number || 
                *last_available_zstd_compression_dictionary_number < *default_dictionary);
      }
    }

} } // end namespace graphene::net
