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
#pragma once

#include <graphene/net/node.hpp>
#include <graphene/net/peer_database.hpp>
#include <graphene/net/message_oriented_connection.hpp>
#include <graphene/net/stcp_socket.hpp>
#include <graphene/net/config.hpp>

#include <boost/tuple/tuple.hpp>

#include <deque>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/hashed_index.hpp>

#include <deque>
#include <queue>
#include <boost/container/deque.hpp>
#include <fc/thread/future.hpp>

namespace graphene { namespace net
  {
    struct firewall_check_state_data
    {
      node_id_t        expected_node_id;
      fc::ip::endpoint endpoint_to_test;

      // if we're coordinating a firewall check for another node, these are the helper
      // nodes we've already had do the test (if this structure is still relevant, that
      // that means they have all had indeterminate results
      std::set<node_id_t> nodes_already_tested;

      // If we're a just a helper node, this is the node we report back to
      // when we have a result
      node_id_t        requesting_peer;
    };

    class peer_connection;
    class peer_connection_delegate
    {
    public:
      virtual void on_message(peer_connection* originating_peer,
                              const message& received_message) = 0;
      virtual void on_connection_closed(peer_connection* originating_peer) = 0;
      virtual std::shared_ptr<full_block_type> get_full_block_by_block_id(const block_id_type& block_id) = 0;
      /// @p message_count messages of @p byte_count bytes in all (headers included) went out to a peer in one batched write
      virtual void on_messages_written(size_t /*message_count*/, size_t /*byte_count*/) {}
      /// Looks up the block @p peer asked for as @p requested_item (a block id or a legacy block message hash),
      /// when its turn to be sent is close (see virtual_queued_block_message); null when we don't have it
      virtual std::shared_ptr<full_block_type> get_block_to_send(peer_connection* peer, const item_hash_t& requested_item) = 0;
    };

    class peer_connection;
    typedef std::shared_ptr<peer_connection> peer_connection_ptr;
    class peer_connection : public message_oriented_connection_delegate,
                            public std::enable_shared_from_this<peer_connection>
    {
    public:
      enum class our_connection_state
      {
        disconnected,
        just_connected, // if in this state, we have sent a hello_message
        connection_accepted, // remote side has sent us a connection_accepted, we're operating normally with them
        connection_rejected // remote side has sent us a connection_rejected, we may be exchanging address with them or may just be waiting for them to close
      };
      enum class their_connection_state
      {
        disconnected,
        just_connected, // we have not yet received a hello_message
        connection_accepted, // we have sent them a connection_accepted
        connection_rejected // we have sent them a connection_rejected
      };
      enum class connection_negotiation_status
      {
        disconnected,
        connecting,
        connected,
        accepting,
        accepted,
        hello_sent,
        peer_connection_accepted,
        peer_connection_rejected,
        negotiation_complete,
        closing,
        closed
      };
    private:
      peer_connection_delegate*      _node;
      fc::optional<fc::ip::endpoint> _remote_endpoint;
      // cached once the socket is connected, since the socket can't report it after it is closed
      fc::optional<fc::ip::endpoint> _local_endpoint;
      message_oriented_connection    _message_connection;

    public:
      /* a base class for messages on the queue, to hide the fact that some
       * messages are complete messages and some are only hashes of messages.
       */
      struct queued_message
      {
        fc::time_point enqueue_time;
        fc::time_point transmission_start_time;
        fc::time_point transmission_finish_time;
        /// transaction traffic, which may wait up to the send flush delay to share a socket write
        bool may_wait_for_batch = false;
        /// a block or a block inventory, queued after the block traffic already queued but ahead of the transaction
        /// traffic queued behind that, so it does not wait for transactions queued before it
        bool is_block_traffic = false;
        /// an inventory of transactions, dropped rather than queued over GRAPHENE_NET_MAXIMUM_QUEUED_TRANSACTION_INVENTORY_BYTES
        bool is_transaction_inventory = false;

        queued_message(fc::time_point enqueue_time = fc::time_point::now()) :
          enqueue_time(enqueue_time)
        {}

        virtual message get_message(peer_connection_delegate* node, peer_connection* peer) = 0;
        /** returns the bytes to write for this message, which stay valid while this object and
         * `built_messages` live; the default builds get_message() and keeps it in `built_messages`
         */
        virtual message_to_write get_message_to_write(peer_connection_delegate* node, peer_connection* peer,
                                                      std::deque<message>& built_messages);
        /** called by the send task while the message is one of the next few to be sent, so whatever it needs
         * before it can be sent can start before its turn comes; may be called more than once
         */
        virtual void prepare(peer_connection_delegate* /* node */, peer_connection* /* peer */) {}
        /** returns roughly the number of bytes of memory the message is consuming while
         * it is sitting on the queue; the same value for the whole time it is queued
         */
        virtual size_t get_size_in_queue() = 0;
        virtual ~queued_message() {}
      };

    private:

      /* when you queue up a 'real_queued_message', a full copy of the message is
       * stored on the heap until it is sent
       */
      struct real_queued_message : queued_message
      {
        message        message_to_send;
        size_t         message_send_time_field_offset;

        real_queued_message(message message_to_send,
                            size_t message_send_time_field_offset = (size_t)-1) :
          message_to_send(std::move(message_to_send)),
          message_send_time_field_offset(message_send_time_field_offset)
        {}

        message get_message(peer_connection_delegate* node, peer_connection* peer) override;
        /// patches the send time in place and writes the queued copy, so the send makes no further copy
        message_to_write get_message_to_write(peer_connection_delegate* node, peer_connection* peer,
                                              std::deque<message>& built_messages) override;
        size_t get_size_in_queue() override;
      private:
        void patch_send_time();
      };

      /* when you queue up a 'queued_transaction_message', only a reference to the transaction is queued;
       * the bytes written are the transaction's own serialization, shared by every peer it is sent to
       */
      struct queued_transaction_message : queued_message
      {
        std::shared_ptr<full_transaction_type> full_transaction;

        queued_transaction_message(const std::shared_ptr<full_transaction_type>& full_transaction);

        message get_message(peer_connection_delegate* node, peer_connection* peer) override;
        message_to_write get_message_to_write(peer_connection_delegate* node, peer_connection* peer,
                                              std::deque<message>& built_messages) override;
        size_t get_size_in_queue() override;
      };

      /* when you queue up a 'virtual_queued_block_message', we just queue up the id of the block the
       * peer asked for.  The block is looked up through the node only when it is one of the next
       * GRAPHENE_NET_BLOCK_MESSAGES_PREPARED_AHEAD messages to be sent (prepare()), so a request for
       * many blocks holds their ids, not the blocks.  If we don't have the block then, the peer gets
       * an item_not_available_message in its place.
       */
      struct virtual_queued_block_message : queued_message
      {
        item_hash_t requested_item;
        bool looked_up = false;
        std::shared_ptr<full_block_type> full_block;

        virtual_queued_block_message(const item_hash_t& requested_item) :
          requested_item(requested_item)
        {
          is_block_traffic = true;
        }

        message get_message(peer_connection_delegate* node, peer_connection* peer) override;
        void prepare(peer_connection_delegate* node, peer_connection* peer) override;
        size_t get_size_in_queue() override;
      };


      size_t _total_queued_messages_size = 0;
      std::deque<std::unique_ptr<queued_message>> _queued_messages;
      /// queued messages that must not wait for a batch to grow (blocks, sync and control messages)
      size_t _queued_messages_not_waiting_for_batch = 0;
      /// the number of queued messages up to and including the last queued block traffic, where block traffic is queued next
      size_t _queued_block_traffic_end = 0;
      bool _send_queue_limit_exceeded = false; // set once the queue would have grown over GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES
      /// transaction inventories dropped over GRAPHENE_NET_MAXIMUM_QUEUED_TRANSACTION_INVENTORY_BYTES since they were last logged
      size_t _transaction_inventories_dropped_since_report = 0;
      size_t _transaction_inventory_bytes_dropped_since_report = 0;
      fc::time_point _last_dropped_transaction_inventory_report_time;
      /// requested transactions waiting for the queue to drain below GRAPHENE_NET_MAXIMUM_QUEUED_TRANSACTION_REPLY_BYTES, oldest first
      std::deque<std::shared_ptr<full_transaction_type>> _deferred_transaction_replies;
      size_t _deferred_transaction_reply_bytes = 0;
      /// when _deferred_transaction_replies last stopped being empty
      fc::time_point _transaction_replies_deferred_since;
      fc::future<void> _send_queued_messages_done;
      fc::microseconds _send_flush_delay;
      /// set while the send task waits for transaction traffic to accumulate; fulfilled to end the wait early
      fc::promise<void>::ptr _send_flush_delay_ended;
    public:
      fc::time_point connection_initiation_time;
      fc::time_point connection_closed_time;
      fc::time_point connection_terminated_time;
      peer_connection_direction direction = peer_connection_direction::unknown;
      //connection_state state;
      firewalled_state is_firewalled = firewalled_state::unknown;
      fc::microseconds clock_offset;
      fc::microseconds round_trip_delay;

      our_connection_state our_state = our_connection_state::disconnected;
      bool they_have_requested_close = false;
      their_connection_state their_state = their_connection_state::disconnected;
      bool we_have_requested_close = false;

      connection_negotiation_status negotiation_status = connection_negotiation_status::disconnected;
      fc::oexception connection_closed_error;
      /** whether the close described by connection_closed_error is a failure; an unrecorded reason
          means the connection dropped unexpectedly, which is one */
      bool connection_closed_by_failure = true;

      fc::time_point get_connection_time()const { return _message_connection.get_connection_time(); }
      fc::time_point get_connection_terminated_time()const { return connection_terminated_time; }

      /// data about the peer node
      /// @{
      /** node_public_key from the hello message, zero-initialized before we get the hello */
      node_id_t        node_public_key;
      /** the unique identifier we'll use to refer to the node with.  zero-initialized before
       * we receive the hello message, at which time it will be filled with either the "node_id"
       * from the user_data field of the hello, or if none is present it will be filled with a
       * copy of node_public_key */
      node_id_t        node_id;
      uint32_t         core_protocol_version = 0;
      std::string      user_agent;
      fc::optional<std::string> graphene_git_revision_sha;
      fc::optional<fc::time_point_sec> graphene_git_revision_unix_timestamp;
      fc::optional<std::string> fc_git_revision_sha;
      fc::optional<fc::time_point_sec> fc_git_revision_unix_timestamp;
      fc::optional<std::string> platform;
      fc::optional<uint32_t> bitness;
      fc::optional<hive::protocol::chain_id_type> chain_id;
      fc::optional<uint8_t> last_available_zstd_compression_dictionary_number;
      /// head block number the peer reported in its hello
      fc::optional<uint32_t> last_known_block_number;

      // for inbound connections, these fields record what the peer sent us in
      // its hello message.  For outbound, they record what we sent the peer
      // in our hello message
      fc::ip::address inbound_address;
      uint16_t inbound_port = 0;
      uint16_t outbound_port = 0;

      // true if the peer sent us a new-style hello (message type 5018),
      // indicating it understands IPv6-aware message types
      bool peer_supports_ipv6 = false;
      /// @}

      typedef std::unordered_map<item_id, fc::time_point> item_to_time_map_type;

      /// blockchain synchronization state data
      /// @{
      uint32_t first_id_block_number = 0; //block number of first item in ids_of_items_being_processed
      uint32_t last_requested_block_number_for_peers_on_this_fork = 0; //one block before starting block number to search peer's ids for sync blocks to fetch
      boost::container::deque<item_hash_t> ids_of_items_to_get; /// id of items in the blockchain that this peer has told us about
      std::set<item_hash_t> ids_of_items_being_processed; /// list of all items this peer has offered use that we've already handed to the client but the client hasn't finished processing
      uint32_t number_of_unfetched_item_ids = 0; /// number of items in the blockchain that follow ids_of_items_to_get but the peer hasn't yet told us their ids
      bool peer_needs_sync_items_from_us = false;
      bool we_need_sync_items_from_peer = false;
      fc::optional<boost::tuple<std::vector<item_hash_t>, fc::time_point> > item_ids_requested_from_peer; /// we check this to detect a timed-out request and in busy()
      fc::time_point last_sync_item_received_time; /// the time we received the last sync item or the time we sent the last batch of sync item requests to this peer
      std::set<item_hash_t> sync_items_requested_from_peer; /// ids of blocks we've requested from this peer during sync.  fetch from another peer if this peer disconnects
      item_hash_t last_block_delegate_has_seen; /// the hash of the last block  this peer has told us about that the peer knows
      fc::time_point_sec last_block_time_delegate_has_seen;
      bool inhibit_fetching_sync_blocks = false;
      // this peer delivered a block we rejected while the invalid-block disconnect circuit
      // breaker was suppressing punitive disconnects.  If the chain later stalls, the
      // inactivity sweep revokes the breaker's deferral and disconnects flagged peers
      bool block_rejected_while_breaker_tripped = false;
      // see is_block_fetching_deprioritized()
      fc::time_point block_fetching_deprioritized_until;
      // the last time we restarted sync with this peer because our head was stalled while
      // it advertised blocks far past it (see GRAPHENE_NET_STALLED_HEAD_RESYNC_SEC)
      fc::time_point last_stalled_head_resync_time;
      /// @}
      void reset_id_search_for_peer() { last_requested_block_number_for_peers_on_this_fork = first_id_block_number - 1; }
      /// latency timing data
      std::unordered_map< item_hash_t, fc::time_point > pending_item_request_times;
      /// @}

      /// non-synchronization state data
      /// @{
      struct timestamped_item_id
      {
        item_id            item;
        fc::time_point_sec timestamp;
        timestamped_item_id(const item_id& item, const fc::time_point_sec timestamp) :
          item(item),
          timestamp(timestamp)
        {}
      };
      struct timestamp_index{};
      typedef boost::multi_index_container<timestamped_item_id,
                                           boost::multi_index::indexed_by<boost::multi_index::hashed_unique<boost::multi_index::member<timestamped_item_id, item_id, &timestamped_item_id::item>,
                                                                                                            std::hash<item_id> >,
                                                                          boost::multi_index::ordered_non_unique<boost::multi_index::tag<timestamp_index>,
                                                                                                                 boost::multi_index::member<timestamped_item_id, fc::time_point_sec, &timestamped_item_id::timestamp> > > > timestamped_items_set_type;
      // blocks only: what the peer knows of transactions is in the node's transaction_inventory, under this slot,
      // which the peer holds while it is an active connection
      timestamped_items_set_type block_inventory_peer_advertised_to_us;
      timestamped_items_set_type block_inventory_advertised_to_peer;
      std::optional<transaction_inventory::slot_type> transaction_inventory_slot;
      // transactions this peer advertised while its inventory of advertisements to us was full; they are recorded
      // (and fetched) once it has room, or dropped when they get too old to still be in the peer's cache.  Kept
      // per peer, in the order they came: unlike the inventory, it is empty unless the peer floods us
      timestamped_items_set_type transaction_inventory_deferred;
      // advertisements we couldn't record since the last report: deferred are in transaction_inventory_deferred,
      // dropped are gone (deferred list full, deferred too long, or block advertisements over the limit)
      uint32_t inventory_advertisements_deferred_since_report = 0;
      uint32_t inventory_advertisements_dropped_since_report = 0;
      fc::time_point last_inventory_limit_report_time;

      requested_items items_requested_from_peer;  /// items we've requested from this peer during normal operation.  fetch from another peer if this peer disconnects
      /// items in the node's list of items to fetch that this peer advertised, to choose from once it can take a request
      fetch_candidate_queue fetch_candidates;
      /// sizes the transaction requests we send this peer and bounds those outstanding (see transaction_size_estimate)
      transaction_size_estimate received_transaction_size;
      uint32_t compressed_blocks_received_from_peer = 0;   // fields to track which peers are sending us compressed vs uncompressed blocks
      uint32_t uncompressed_blocks_received_from_peer = 0;
      /// @}

      // if they're flooding us with transactions, we set this to avoid fetching for a few seconds to let the
      // blockchain catch up
      fc::time_point transaction_fetching_inhibited_until;

      uint32_t last_known_fork_block_number = 0;

      fc::future<void> accept_or_connect_task_done;

      firewall_check_state_data *firewall_check_state = nullptr;
#ifndef NDEBUG
    private:
      fc::thread* _thread = nullptr;
      unsigned _send_message_queue_tasks_running = 0; // temporary debugging
#endif
      bool _currently_handling_message = false; // true while we're in the middle of handling a message from the remote system
    private:
      peer_connection(peer_connection_delegate* delegate);
      void destroy(const char* caller);
    public:
      static peer_connection_ptr make_shared(peer_connection_delegate* delegate); // use this instead of the constructor
      virtual ~peer_connection();

      fc::tcp_socket& get_socket();
      void accept_connection();
      void connect_to(const fc::ip::endpoint& remote_endpoint, fc::optional<fc::ip::endpoint> local_endpoint = fc::optional<fc::ip::endpoint>());

      void on_message(message_oriented_connection* originating_connection, const message& received_message) override;
      void on_connection_closed(message_oriented_connection* originating_connection) override;

      void send_queueable_message(std::unique_ptr<queued_message>&& message_to_send);
      /// queues the block the peer asked for as @p requested_item; it is looked up shortly before it is sent
      void send_block_message(const item_hash_t& requested_item);
      /// sends a trx_message for the transaction, written from its cached serialization without a per-peer copy
      void send_transaction_message(const std::shared_ptr<full_transaction_type>& full_transaction);
      /** sends the transaction as the reply to the peer's request for it, after the replies sent before it; it waits
       * outside the send queue while the queue holds more than GRAPHENE_NET_MAXIMUM_QUEUED_TRANSACTION_REPLY_BYTES with
       * it, so replies never close the connection by taking the queue over its limit.  Returns false, sending
       * nothing, when GRAPHENE_NET_MAX_DEFERRED_TRANSACTION_REPLIES_PER_PEER replies are waiting already, or waiting
       * with it would take more than GRAPHENE_NET_MAX_DEFERRED_TRANSACTION_REPLY_BYTES_PER_PEER
       */
      bool send_transaction_reply(const std::shared_ptr<full_transaction_type>& full_transaction);
      /// the bytes of the transactions waiting to be queued by send_transaction_reply()
      size_t get_deferred_transaction_reply_bytes() const { return _deferred_transaction_reply_bytes; }
      /// while transaction replies wait (see send_transaction_reply()), when they started waiting or the queue was
      /// last written to the socket, whichever is later; fc::time_point::maximum() while none wait
      fc::time_point get_transaction_reply_send_progress_time() const;
      void send_message(const message& message_to_send, size_t message_send_time_field_offset = (size_t)-1);
      void send_item(const item_id& item_to_send);
      /// Lets queued transaction traffic wait up to `delay` (measured from the oldest queued message) so
      /// more of it goes out in one socket write; any other queued message, or a full batch, ends the
      /// wait at once.  Zero, the default, writes as soon as the send task runs
      void set_send_flush_delay(fc::microseconds delay);
      void close_connection();
      void destroy_connection(const char* caller);
      /// Records why the connection is closing, and whether that is a failure of the peer, unless a
      /// reason is already recorded; the first reason (a rejection, or whichever side asked to close
      /// first) is the one worth keeping
      void record_connection_closed_error(const fc::exception& reason, bool is_failure);
      /// The recorded closing reason, or "lost connection to peer" when neither side gave one
      fc::exception get_connection_closed_reason() const;
      /// Stores the closing reason in the peer's database record, and marks the record failed only
      /// when the close was a failure rather than a deliberate or graceful disconnect
      void record_connection_closed_in(potential_peer_record& record) const;

      uint64_t get_total_bytes_sent() const;
      uint64_t get_total_bytes_received() const;

      fc::time_point get_last_message_sent_time() const;
      fc::time_point get_last_message_received_time() const;

      fc::optional<fc::ip::endpoint> get_remote_endpoint() const;
      fc::optional<fc::ip::endpoint> get_local_endpoint() const;
      void set_remote_endpoint(fc::optional<fc::ip::endpoint> new_remote_endpoint);

      bool busy() const;
      bool idle() const;
      /// Whether the fetch loop may send the peer another request: nothing but transactions is outstanding with it.
      /// Transactions are then requested only as far as received_transaction_size.transactions_to_request() allows
      bool can_take_item_request() const;
      bool is_currently_handling_message() const;

      bool is_transaction_fetching_inhibited() const;
      /// Whether we ask this peer for a block only if no other peer offered it (see choose_block_source()):
      /// it sent us a block copy whose body did not match its header and the block-body-mismatch circuit
      /// breaker kept it connected.  Lasts until restore_block_fetching_priority() or, if the peer delivers no
      /// block we accept, for the breaker's window, so a peer corrupted once is not held back forever
      bool is_block_fetching_deprioritized() const;
      void deprioritize_block_fetching();
      /// called once the peer delivered a block we accepted: its copies match their headers again
      void restore_block_fetching_priority();
      fc::sha512 get_shared_secret() const;
      /// expires the peer's block inventory and deferred transaction advertisements
      void clear_old_inventory();
      void defer_transaction_advertisement(const item_id& item);
      bool performing_firewall_check() const;
      fc::optional<fc::ip::endpoint> get_endpoint_for_connecting() const;
      fc::optional<fc::ip::endpoint> get_endpoint_for_db() const;

      bool supports_compressed_blocks() const;
      bool advertise_blocks_by_block_id() const;
      bool requires_alternate_compression_for_block(const std::shared_ptr<full_block_type>& full_block) const;
    private:
      void send_queued_messages_task();
      /// queues the deferred transaction replies the queue has room for, and at least one if it is empty; true if it queued any
      bool queue_deferred_transaction_replies();
      /// counts a transaction inventory of @p message_size bytes that is not queued, and logs the count now and then
      void drop_transaction_inventory(size_t message_size);
      bool send_flush_wait_is_over() const;
      void wait_for_transaction_batch_to_grow();
      /// appends the message's bytes to messages_to_write and returns true, or logs and returns false if it cannot be built
      bool build_queued_message(queued_message& message_to_build, size_t queue_position,
                                std::vector<message_to_write>& messages_to_write, std::deque<message>& built_messages);
      void accept_connection_task();
      void connect_to_task(const fc::ip::endpoint& remote_endpoint);
    };
    typedef std::shared_ptr<peer_connection> peer_connection_ptr;

 } } // end namespace graphene::net

// not sent over the wire, just reflected for logging
FC_REFLECT_ENUM(graphene::net::peer_connection::our_connection_state, (disconnected)
                                                                 (just_connected)
                                                                 (connection_accepted)
                                                                 (connection_rejected))
FC_REFLECT_ENUM(graphene::net::peer_connection::their_connection_state, (disconnected)
                                                                   (just_connected)
                                                                   (connection_accepted)
                                                                   (connection_rejected))
FC_REFLECT_ENUM(graphene::net::peer_connection::connection_negotiation_status, (disconnected)
                                                                          (connecting)
                                                                          (connected)
                                                                          (accepting)
                                                                          (accepted)
                                                                          (hello_sent)
                                                                          (peer_connection_accepted)
                                                                          (peer_connection_rejected)
                                                                          (negotiation_complete)
                                                                          (closing)
                                                                          (closed) )

FC_REFLECT( graphene::net::peer_connection::timestamped_item_id, (item)(timestamp));
