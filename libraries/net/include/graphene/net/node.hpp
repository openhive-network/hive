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

#include <graphene/net/config.hpp>
#include <graphene/net/core_messages.hpp>
#include <graphene/net/message.hpp>
#include <graphene/net/node_configuration.hpp>
#include <graphene/net/peer_database.hpp>

#include <hive/protocol/types.hpp>

#include <fc/exception/exception.hpp>
#include <fc/thread/future.hpp>
#include <fc/time.hpp>
#include <fc/variant_object.hpp>

#include <boost/container/small_vector.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/tag.hpp>

#include <array>
#include <deque>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace appbase {
  class application;
}

namespace graphene { namespace net {

  using fc::variant_object;
  using hive::protocol::chain_id_type;

  class peer_connection;
  typedef std::shared_ptr<peer_connection> peer_connection_ptr;

  namespace detail
  {
    class node_impl;
    struct node_impl_deleter
    {
      void operator()(node_impl*);
    };
  }

  // during network development, we need to track message propagation across the network
  // using a structure like this:
  struct message_propagation_data
  {
    fc::time_point received_time;
    fc::time_point validated_time;
    node_id_t originating_peer;
  };

  /// What node_delegate::handle_block() did with a block it did not reject
  enum class block_handling_result
  {
    applied,        ///< the block extended our current chain
    switched_forks, ///< the block made us switch to the fork it is on
    ignored         ///< the block was stored on a shorter fork without being validated
  };

  /**
   * Called exactly once, from the thread that validated it, when a transaction handed over by
   * node_delegate::submit_transaction() has been validated.  @p error is null if the transaction was accepted.
   * It must not block.
   */
  typedef std::function<void(const fc::exception_ptr& error)> transaction_validated_callback;

  /// The client's pending transaction pool: how many bytes of transactions it holds and the most it may hold
  struct pending_transaction_pool_size
  {
    uint64_t pending_bytes = 0;
    uint64_t max_bytes = 0;     ///< max-mempool-size
  };

  /**
   * The pending pool size above which the node requests no more than GRAPHENE_NET_MIN_TRANSACTIONS_PER_GATED_FETCH
   * transactions per GRAPHENE_NET_TRANSACTION_FETCH_GATE_RECHECK_MS: a third of @p max_bytes, which leaves the rest of the pool for API submissions, fork
   * switches and replies already on their way, and at least 1.  0 when @p max_bytes is 0: the pool then keeps only
   * what fits in the reapply window, so it needs no gate.
   *
   * The pool counts transactions from every source.  API submissions are not limited by max-mempool-size (only
   * the reapply after a block drops what exceeds it), so while API inflow keeps the pool above the threshold
   * after every block the node fetches, and so relays, only that minimum of transactions from its peers, for as
   * long as that inflow lasts.  Blocks are fetched as always, and the node's own transactions are still broadcast.
   */
  uint64_t transaction_fetch_gate_threshold(uint64_t max_bytes);

  /// Whether @p pool holds more than transaction_fetch_gate_threshold(), so that the node requests only the minimum of transactions per pass
  bool is_transaction_fetching_gated(const pending_transaction_pool_size& pool);

  /**
   * The node-wide transaction fetch gate (is_transaction_fetching_gated()) for one pass of choosing what to
   * request from peers, and whether it actually kept any peer from being asked for transactions in that pass.
   *
   * While the gate is open it also bounds what the pass requests by the headroom below the threshold: the
   * threshold less the pool's published size and the bytes of transactions already on their way into the pool
   * (requested but not received, or received but not yet validated).  The published size changes only once the
   * chain has processed what it was handed, so without this bound every peer's window would be requested at once
   * whenever the pool fell just below the threshold, and the pool would overshoot it by all of them.  A pass may
   * go past the headroom by at most one transaction.
   *
   * Whether closed or out of headroom, the gate lets a pass request @p minimum_transactions in all, so transactions
   * it can't tell apart before fetching them, such as fast-confirms, keep flowing however full the pool is, and the
   * bytes estimated to be on their way can't hold an empty pool at no fetching at all.  fetch_minimum_schedule says
   * which passes get a minimum.
   */
  class transaction_fetch_gate
  {
  public:
    transaction_fetch_gate(const pending_transaction_pool_size& pool, uint64_t bytes_on_their_way,
                           size_t minimum_transactions = GRAPHENE_NET_MIN_TRANSACTIONS_PER_GATED_FETCH);

    /**
     * How many transactions to request from a peer whose window has room for @p window: @p window, or none while
     * the gate is closed or no headroom is left and the pass has requested its minimum (until then
     * may_request_more() bounds what the pass takes).  The gate holds transactions back only when there was room
     * to request them and @p has_candidates (called as `bool()`, only then, and only until one peer has been found
     * to have some) says the peer has a transaction candidate that would have been requested.
     */
    template<typename HasCandidates>
    size_t transactions_to_request(size_t window, HasCandidates&& has_candidates)
    {
      if (may_request_any())
        return window;
      if (window > 0 && !withheld() && has_candidates())
        record_withheld();
      return 0;
    }

    /**
     * Whether the pass may request another transaction from a peer: false once the requests recorded with
     * record_request() have used up the headroom, or the gate is closed, and the pass has requested its minimum.
     * The gate withholds a transaction only when @p has_candidates (called as `bool()`, only then, and only until
     * one peer has been found to have some) says the peer has one that would have been requested.
     */
    template<typename HasCandidates>
    bool may_request_more(HasCandidates&& has_candidates)
    {
      if (may_request_any())
        return true;
      if (!withheld() && has_candidates())
        record_withheld();
      return false;
    }

    /// Whether the next transaction the pass may request is one of its minimum rather than within the headroom
    bool next_request_is_minimum() const { return _closed || _headroom == 0; }

    /// Counts a transaction of @p expected_bytes requested in this pass against the headroom and the minimum
    void record_request(uint64_t expected_bytes);

    bool closed() const { return _closed; }
    /// whether the closed gate withheld any transactions in this pass
    bool held_back() const { return _held_back; }
    /// whether the open gate withheld any transactions in this pass because the headroom was used up
    bool bounded() const { return _bounded; }
    /// whether the pass requested any transaction of its minimum (next_request_is_minimum())
    bool requested_minimum() const { return _requested_minimum; }

  private:
    bool may_request_any() const { return (!_closed && _headroom > 0) || _requested < _minimum; }
    bool withheld() const { return _held_back || _bounded; }
    void record_withheld();

    bool _closed;
    uint64_t _headroom; ///< no limit (the maximum) when the gate is disabled
    size_t _minimum;
    size_t _requested = 0;
    bool _held_back = false;
    bool _bounded = false;
    bool _requested_minimum = false;
  };

  /**
   * Which fetch passes may request the transaction fetch gate's minimum (GRAPHENE_NET_MIN_TRANSACTIONS_PER_GATED_FETCH):
   * a pass that begins at least @p interval after the last one that requested some of its minimum
   * (transaction_fetch_gate::requested_minimum()).  Passes run whenever inventory, a reply or a block arrives, at most
   * every p2p-fetch-items-minimum-interval-us, so a minimum on every pass would make a gated node ask for a
   * transaction or two at a time, hundreds of times a second, each reply waking the loop again.
   */
  class fetch_minimum_schedule
  {
  public:
    explicit fetch_minimum_schedule(fc::microseconds interval) : _interval(interval) {}

    /// The minimum to give the gate of a pass that begins at @p pass_start
    size_t minimum_for_pass(fc::time_point pass_start) const
    {
      return is_due(pass_start) ? GRAPHENE_NET_MIN_TRANSACTIONS_PER_GATED_FETCH : 0;
    }

    /**
     * Records the pass that began at @p pass_start with @p gate, and returns when the next pass whose gate gets a
     * minimum is due: @p interval after this pass if it was due one (whether it requested any or not, so a gate
     * that withholds is checked again no sooner than that), otherwise @p interval after the last pass that
     * requested some.
     */
    fc::time_point record_pass(const transaction_fetch_gate& gate, fc::time_point pass_start);

  private:
    bool is_due(fc::time_point pass_start) const { return pass_start - _last_minimum >= _interval; }

    fc::microseconds _interval;
    fc::time_point _last_minimum = fc::time_point::min();
  };

  /**
   * Decides when to warn that items to fetch were removed because they expired.  A node whose transaction fetch
   * gate withholds no transactions warns on every call that removed some.  While the gate withholds them, closed or
   * bounded by its headroom (transaction_fetch_gate::closed(), transaction_fetch_gate::bounded()), advertised
   * transactions expire unfetched on pass after pass, so from a pass that withheld them until @p interval after it
   * the limiter warns at most once per @p interval, with the sum since the last warning.  Passes that withhold
   * nothing in between (a pass whose headroom was enough) do not end that.
   */
  class fetch_expiry_warning_limiter
  {
  public:
    explicit fetch_expiry_warning_limiter(fc::microseconds interval);

    /// Records @p expired items removed by a pass at @p now that @p withheld transactions or not; returns how many to warn about now, if any
    std::optional<size_t> record(size_t expired, bool withheld, fc::time_point now);

  private:
    fc::microseconds _interval;
    size_t _unreported = 0;
    fc::time_point _last_warning;
    std::optional<fc::time_point> _last_withheld;
  };

   /**
    *  @class node_delegate
    *  @brief used by node reports status to client or fetch data from client
    */
   #define  DEFAULT_MAX_BLOCK_IDS_TO_FETCH 10000
   class node_delegate
   {
      public:
         virtual ~node_delegate(){}


        /*
          New `chain_id` is needed during switching from HF23 into HF24,
          because some nodes can be after HF24, some nodes can be before HF24.

          In such case a communication between "before-HF24"-node and "after-HF24"-node is impossible,
          because 'chain_id' for such nodes is different.

          The simplest solution is softening a condition regarding 'chain_id'( see usage in 'node_impl::on_hello_message' method ).
        */
         virtual hive::protocol::chain_id_type get_new_chain_id() const = 0;
         virtual hive::protocol::chain_id_type get_chain_id() const = 0;

         /**
          *  If delegate has the item, the network has no need to fetch it.
          */
         virtual bool has_item( const net::item_id& id ) = 0;

         /**
          *  @brief Called when a new block comes in from the network
          *
          *  @param sync_mode true if the message was fetched through the sync process, false during normal operation
          *  @returns whether the block was applied, made the blockchain switch forks, or was only stored
          *           on a shorter fork (and therefore not validated)
          *
          *  @throws block_body_mismatch_exception if the block's transactions don't match the merkle root
          *          in its header (only this copy is bad, the block id itself may be genuine)
          *  @throws exception if error validating the item, otherwise the item is
          *          safe to broadcast on.
          */
         virtual block_handling_result handle_block(const std::shared_ptr<full_block_type>& full_block, bool sync_mode) = 0;

         /**
          *  @brief Called when a new transaction comes in from the network; hands it over for validation
          *         without waiting for the result
          *
          *  Transactions are validated in the order they are submitted.  Once one is validated, @p on_validated
          *  is called; if it reports no error, the transaction is safe to broadcast on.
          *
          *  @throws exception if the transaction can't be submitted (e.g. the client is shutting down);
          *          @p on_validated is not called then
          */
         virtual void submit_transaction(const std::shared_ptr<full_transaction_type>& full_transaction,
                                         transaction_validated_callback on_validated) = 0;

         /**
          *  @brief Called when a new message comes in from the network other than a
          *         block or a transaction.  Currently there are no other possible
          *         messages, so this should never be called.
          *
          *  @throws exception if error validating the item, otherwise the item is
          *          safe to broadcast on.
          */
         virtual void handle_message(const message& message_to_process) = 0;

         /**
          *  Assuming all data elements are ordered in some way, this method should
          *  return up to limit ids that occur *after* from_id.
          *  On return, remaining_item_count will be set to the number of items
          *  in our blockchain after the last item returned in the result,
          *  or 0 if the result contains the last item in the blockchain
          */
         virtual std::vector<item_hash_t> get_block_ids(const std::vector<item_hash_t>& blockchain_synopsis,
                                                        uint32_t& remaining_item_count,
                                                        uint32_t limit = DEFAULT_MAX_BLOCK_IDS_TO_FETCH) = 0;

         /**
          *  Given the block id, return the block itself
          *  returns nullptr if the block is not present in our blockchain/fork database
          */
         virtual std::shared_ptr<full_block_type> get_full_block(const block_id_type&) = 0;

         /**
          * Returns a synopsis of the blockchain used for syncing.
          * This consists of a list of selected item hashes from our current preferred
          * blockchain, exponentially falling off into the past.  Horrible explanation.
          *
          * If the blockchain is empty, it will return the empty list.
          * If the blockchain has one block, it will return a list containing just that block.
          * If it contains more than one block:
          *   the first element in the list will be the hash of the highest numbered block that
          *       we cannot undo
          *   the second element will be the hash of an item at the half way point in the undoable
          *       segment of the blockchain
          *   the third will be ~3/4 of the way through the undoable segment of the block chain
          *   the fourth will be at ~7/8...
          *     &c.
          *   the last item in the list will be the hash of the most recent block on our preferred chain
          */
         virtual std::vector<item_hash_t> get_blockchain_synopsis(const item_hash_t& reference_point,
                                                                  uint32_t number_of_blocks_after_reference_point) = 0;

         /**
          *  Call this after the call to handle_message succeeds.
          *
          *  @param item_type the type of the item we're synchronizing, will be the same as item passed to the sync_from() call
          *  @param item_count the number of items known to the node that haven't been sent to handle_item() yet.
          *                    After `item_count` more calls to handle_item(), the node will be in sync
          */
         virtual void     sync_status( uint32_t item_type, uint32_t item_count ) = 0;

         /**
          *  Call any time the number of connected peers changes.
          */
         virtual void     connection_count_changed( uint32_t c ) = 0;

         virtual uint32_t get_block_number(const item_hash_t& block_id) = 0;

         /**
          * Returns the time a block was produced (if block_id = 0, returns genesis time).
          * If we don't know about the block, returns time_point_sec::min()
          */
         virtual fc::time_point_sec get_block_time(const item_hash_t& block_id) = 0;

         /** returns graphene::blockchain::now() */
         virtual fc::time_point_sec get_blockchain_now() = 0;

         virtual item_hash_t get_head_block_id() const = 0;

         /** returns the number of the last irreversible block; blocks at or below this height
          * that are not on our chain can never become part of it */
         virtual uint32_t get_last_irreversible_block_num() const = 0;

         /** returns the number of the oldest block we can provide to peers; it is 1 unless older
          * blocks have been pruned from our block log */
         virtual uint32_t get_tail_block_num() const = 0;

         /** returns true when our head block is recent enough to consider us in live sync rather
          * than catching up with the network. It is evaluated on every call, so unlike the
          * chain's own switch to live mode it becomes false again when we fall behind */
         virtual bool is_at_live_sync() = 0;

         /** returns the size of the pending transaction pool. Called from the p2p thread often, so it must not block:
          * it returns the size as last published by the client, never a size from the middle of applying a block */
         virtual pending_transaction_pool_size get_pending_transaction_pool_size() const = 0;

         virtual uint32_t estimate_last_known_fork_from_git_revision_timestamp(uint32_t unix_timestamp) const = 0;

         virtual void error_encountered(const std::string& message, const fc::oexception& error) = 0;

         virtual std::deque<block_id_type>::const_iterator find_first_item_not_in_blockchain(const std::deque<block_id_type>& item_hashes_received) = 0;
   };

   /**
    *  Information about connected peers that the client may want to make
    *  available to the user.
    */
   struct peer_status
   {
      uint32_t         version;
      fc::ip::endpoint host;
      /** info contains the fields required by bitcoin-rpc's getpeerinfo call, we will likely
          extend it with our own fields. */
      fc::variant_object info;
   };

   /**
    *  @class node
    *  @brief provides application independent P2P broadcast and data synchronization
    *
    *  Unanswered questions:
    *    when does the node start establishing network connections and accepting peers?
    *    we don't have enough info to start synchronizing until sync_from() is called,
    *    would we have any reason to connect before that?
    */
   class node : public std::enable_shared_from_this<node>
   {
      public:
        node(const std::string& user_agent, appbase::application& app, hive::chain::blockchain_worker_thread_pool& thread_pool);
        virtual ~node();

        void close();

        void set_node_delegate(node_delegate* del);

        void load_configuration(const fc::path& configuration_directory);

        virtual void listen_to_p2p_network(std::function<bool()> break_callback);
        virtual void connect_to_p2p_network();

        /**
         *  Add endpoint to internal level_map database of potential nodes
         *  to attempt to connect to.  This database is consulted any time
         *  the number connected peers falls below the target.
         */
        void add_node(const fc::ip::endpoint& ep);

        /**
         *  Attempt to connect to the specified endpoint immediately.
         */
        virtual void connect_to_endpoint( const fc::ip::endpoint& ep );

        /**
         *  Specifies the network interface and port upon which incoming
         *  connections should be accepted.
         */
        void listen_on_endpoint(const fc::ip::endpoint& ep, bool wait_if_not_available);

        /**
         *  Call with true to enable listening for incoming connections
         */
        void accept_incoming_connections(bool accept);

        /**
         *  Specifies the port upon which incoming connections should be accepted.
         *  @param port the port to listen on
         *  @param wait_if_not_available if true and the port is not available, enter a
         *                               sleep and retry loop to wait for it to become
         *                               available.  If false and the port is not available,
         *                               just choose a random available port
         */
        void listen_on_port(uint16_t port, bool wait_if_not_available);

        /**
         * Returns the endpoint the node is listening on.  This is usually the same
         * as the value previously passed in to listen_on_endpoint, unless we
         * were unable to bind to that port.
         */
        virtual fc::ip::endpoint get_actual_listening_endpoint() const;

        /**
         *  @return a list of peers that are currently connected.
         */
        std::vector<peer_status> get_connected_peers() const;

        /** return the number of peers we're actively connected to */
        virtual uint32_t get_connection_count() const;

        /**
         *  Add message to outgoing inventory list, notify peers that
         *  I have a message ready.
         */
        virtual void broadcast(const std::shared_ptr<full_block_type>& full_block);
        virtual void broadcast(const std::shared_ptr<hive::chain::full_transaction_type>& full_transaction);

        /**
         *  Node starts the process of fetching all items after item_id of the
         *  given item_type.   During this process messages are not broadcast.
         */
        virtual void sync_from(const item_id& current_head_block, const std::vector<uint32_t>& hard_fork_block_numbers);

        bool is_connected() const;

        void set_advanced_node_parameters(const fc::variant_object& params);
        node_configuration get_advanced_node_parameters()const;
        message_propagation_data get_transaction_propagation_data(const hive::protocol::transaction_id_type& transaction_id);
        message_propagation_data get_block_propagation_data(const hive::protocol::block_id_type& block_id);
        node_id_t get_node_id() const;
        void set_allowed_peers(const std::vector<node_id_t>& allowed_peers);

        /**
         * Instructs the node to forget everything in its peer database, mostly for debugging
         * problems where nodes are failing to connect to the network
         */
        void clear_peer_database();

        void set_total_bandwidth_limit(uint32_t upload_bytes_per_second, uint32_t download_bytes_per_second);

        fc::variant_object network_get_info() const;
        fc::variant_object network_get_usage_stats() const;

        std::vector<potential_peer_record> get_potential_peers() const;

        fc::variant_object get_call_statistics() const;
      private:
        std::unique_ptr<detail::node_impl, detail::node_impl_deleter> my;
   };

    class simulated_network : public node
    {
    public:
      ~simulated_network();
      simulated_network(const std::string& user_agent, appbase::application& app, hive::chain::blockchain_worker_thread_pool& thread_pool) : node(user_agent, app, thread_pool) {}
      void listen_to_p2p_network(std::function<bool()> break_callback) override {}
      void connect_to_p2p_network() override {}
      void connect_to_endpoint(const fc::ip::endpoint& ep) override {}

      fc::ip::endpoint get_actual_listening_endpoint() const override { return fc::ip::endpoint(); }

      void sync_from(const item_id& current_head_block, const std::vector<uint32_t>& hard_fork_block_numbers) override {}
      void broadcast(const std::shared_ptr<full_block_type>& full_block) override;
      void broadcast(const std::shared_ptr<hive::chain::full_transaction_type>& full_transaction) override;
      void add_node_delegate(node_delegate* node_delegate_to_add);

      virtual uint32_t get_connection_count() const override { return 8; }
    private:
      struct node_info;
      void message_sender(node_info* destination_node);
      std::list<node_info*> network_nodes;
    };


   typedef std::shared_ptr<node> node_ptr;
   typedef std::shared_ptr<simulated_network> simulated_network_ptr;

  /**
   * Whether a hello from @p peer_node_id must be rejected as a duplicate when this node, @p my_node_id,
   * already holds another connection to that node id (or the id is our own).
   *
   * Of two nodes that open several connections to each other at once (a dual-stack seed dialed on
   * both address families, or both nodes dialing each other), only the one with the lower node id
   * rejects duplicates, keeping whichever connection's hello reached it first.  The other node
   * accepts every hello from it and lets those rejections close the extra connections, so both
   * nodes keep the same connection and at least one always survives.
   */
  bool rejects_duplicate_hello(const node_id_t& my_node_id, const node_id_t& peer_node_id);

  /**
   * Whether a connection_rejected_message with @p reason_code means the rejecting peer failed us.
   * already_connected and connected_to_self do not: the peer is up and answered, the connection
   * was merely redundant.
   */
  bool rejection_is_failure(rejection_reason_code reason_code);

  /**
   * Whether a peer whose newest block is @p peers_head_block_num needs blocks older than our tail
   * block @p our_tail_block_num (1 unless we prune our block log), so that we can't help it sync.
   * A peer lagging behind our tail by at most GRAPHENE_NET_PRUNED_TAIL_GRACE_BLOCKS blocks is still
   * served: it catches up through normal block propagation.
   */
  bool peer_needs_blocks_we_pruned(uint32_t peers_head_block_num, uint32_t our_tail_block_num);

  /**
   * Circuit breaker for punitive disconnects: once @p count disconnects have been recorded within
   * the last @p window, further ones are refused until the oldest recorded one leaves the window.
   */
  class disconnect_breaker
  {
  public:
    disconnect_breaker(uint32_t count, fc::microseconds window);

    /// Whether a disconnect at @p now must be refused.  When it is not, it is recorded against the window.
    bool tripped(fc::time_point now);

  private:
    const uint32_t _count;
    const fc::microseconds _window;
    std::deque<fc::time_point> _recent_disconnect_times;
  };

  /**
   * Wakes a loop that is triggered by events, letting the triggers that arrive within a minimum interval
   * of the start of its last iteration share one later iteration.  An urgent trigger never waits.
   * All calls must come from the loop's thread.
   */
  class coalescing_loop_trigger
  {
  public:
    /// Marks the start of an iteration: it handles every trigger made before this call
    void begin_iteration();

    void trigger(bool urgent);

    /**
     * Returns when the next iteration is due: at once after an urgent trigger, otherwise for a trigger once
     * @p minimum_interval has passed since begin_iteration(), and in any case at @p wake_at (fc::time_point::maximum()
     * for none).  A zero @p minimum_interval makes every trigger run the next iteration at once.
     */
    void wait(fc::microseconds minimum_interval, fc::time_point wake_at);

  private:
    fc::time_point _iteration_start;
    bool _triggered = false;
    bool _urgent = false;
    fc::promise<void>::ptr _wake_promise;
  };

  /**
   * Hands results produced on other threads to one consumer, in the order they were pushed.  The first push
   * after the consumer last took the results calls the scheduling function given at construction, so results
   * that arrive before the consumer gets to them share one take.  Once closed, pushes are discarded and the
   * scheduling function is not called any more.  Thread-safe.
   */
  template<typename Completion>
  class completion_queue
  {
  public:
    /// @p schedule_take must arrange for take_all() to be called; it is called with the queue's lock held
    explicit completion_queue(std::function<void()> schedule_take) : _schedule_take(std::move(schedule_take)) {}

    void push(Completion completion)
    {
      std::lock_guard<std::mutex> guard(_mutex);
      if (_closed)
        return;
      _completions.push_back(std::move(completion));
      if (!_take_scheduled)
      {
        _take_scheduled = true;
        _schedule_take();
      }
    }

    /// Every result pushed since the last call, oldest first
    std::deque<Completion> take_all()
    {
      std::lock_guard<std::mutex> guard(_mutex);
      _take_scheduled = false;
      return std::exchange(_completions, {});
    }

    /// Discards the results not taken yet and every later push.  Once it returns, the scheduling function is not called again.
    void close()
    {
      std::lock_guard<std::mutex> guard(_mutex);
      _closed = true;
      _completions.clear();
    }

    bool is_closed() const
    {
      std::lock_guard<std::mutex> guard(_mutex);
      return _closed;
    }

  private:
    const std::function<void()> _schedule_take;
    mutable std::mutex _mutex;
    std::deque<Completion> _completions;
    bool _take_scheduled = false;
    bool _closed = false;
  };

  /**
   * Counts the items handed over for processing whose results have not been applied yet, and makes the tasks
   * that would hand over more wait while the count is at the limit.  All calls must come from one thread.
   */
  class in_flight_limit
  {
  public:
    /**
     * Waits, letting other tasks run, until fewer than @p limit items are in flight (0 for no limit), then counts one more.
     * @throws fc::canceled_exception once closed
     */
    void acquire(uint32_t limit);

    /// Counts @p count items fewer in flight
    void release(uint32_t count);

    /// Wakes the waiting tasks; acquire() throws from now on
    void close();

    uint32_t in_flight() const { return _in_flight; }

  private:
    void wake_waiters();

    uint32_t _in_flight = 0;
    bool _closed = false;
    fc::promise<void>::ptr _released;
  };

  /// What fetch_candidate_queue::take_next() does with the entry it looks at
  enum class fetch_candidate_verdict
  {
    take, ///< request the item from the peer now
    drop, ///< the item can't or needn't be requested from the peer any more
    keep  ///< not now: look at the entry again in the next pass
  };

  /// The end of its lane fetch_candidate_queue::take_next() takes transactions from
  enum class fetch_candidate_order
  {
    oldest_first,
    /// for the transaction fetch gate's minimum (GRAPHENE_NET_MIN_TRANSACTIONS_PER_GATED_FETCH): the newest ids are
    /// the likeliest to be fast-confirms, which their witnesses broadcast as soon as the block arrives
    newest_first
  };

  /**
   * The items one peer advertised to us that we want to fetch, so that choosing what to request from the peer
   * looks at nothing else.  Blocks come before transactions, each lane in the order the items were added.
   * Entries are judged only when reached, so an entry for an item that is no longer wanted costs one look;
   * entries older than the fetch window are trimmed whenever one is added, which bounds the queue by what the
   * peer advertised within the window, and when trim() is called.
   */
  class fetch_candidate_queue
  {
  public:
    /// Adds @p item, queued (again) at @p now; entries queued before @p oldest_to_keep are discarded
    void push(const item_id& item, fc::time_point now, fc::time_point oldest_to_keep);

    /// Discards the entries queued before @p oldest_to_keep
    void trim(fc::time_point oldest_to_keep);

    /// Starts a pass: entries kept in the previous one are looked at again
    void begin_pass();

    /**
     * Within a pass, the next entry @p judge (called as `fetch_candidate_verdict(const item_id&)`) says to take,
     * looking at transactions only if @p include_transactions, from the end of their lane @p order says; none once
     * the pass has looked at every entry.  Taken and dropped entries are removed, kept ones stay for the next pass.
     * @p examined counts every entry looked at.
     */
    template<typename Judge>
    std::optional<item_id> take_next(bool include_transactions, fetch_candidate_order order, Judge&& judge, uint64_t& examined)
    {
      if (std::optional<item_id> item = take_oldest_from(_blocks, _blocks_kept, 0, judge, examined))
        return item;
      if (!include_transactions)
        return std::nullopt;
      if (order == fetch_candidate_order::newest_first)
        return take_newest_from(_transactions, _transactions_kept, _transactions_newest_kept, judge, examined);
      return take_oldest_from(_transactions, _transactions_kept, _transactions_newest_kept, judge, examined);
    }

    template<typename Judge>
    std::optional<item_id> take_next(bool include_transactions, Judge&& judge, uint64_t& examined)
    {
      return take_next(include_transactions, fetch_candidate_order::oldest_first, judge, examined);
    }

    /**
     * Whether a transaction entry not yet looked at in this pass is one @p wanted (called as
     * `bool(const item_id&)`) says would be taken; the entries before it that are not wanted are dropped, as
     * take_next() would drop them.  @p examined counts every entry looked at.
     */
    template<typename Wanted>
    bool has_transaction_to_take(Wanted&& wanted, uint64_t& examined)
    {
      while (_transactions_kept + _transactions_newest_kept < _transactions.size())
      {
        ++examined;
        if (wanted(_transactions[_transactions_kept].item))
          return true;
        _transactions.erase(_transactions.begin() + _transactions_kept);
      }
      return false;
    }

    bool has_blocks() const { return !_blocks.empty(); }
    /// whether block @p item is among the entries; looks through the blocks, which are few
    bool has_block(const item_id& item) const;
    bool has_transactions() const { return !_transactions.empty(); }
    size_t size() const { return _blocks.size() + _transactions.size(); }
    void clear();

  private:
    struct entry
    {
      item_id item;
      fc::time_point queued;
    };

    // entries kept in this pass are the first `kept` and the last `newest_kept` ones of their lane
    template<typename Judge>
    static std::optional<item_id> take_oldest_from(std::deque<entry>& lane, size_t& kept, size_t newest_kept, Judge& judge,
                                                   uint64_t& examined)
    {
      while (kept + newest_kept < lane.size())
      {
        ++examined;
        const item_id item = lane[kept].item;
        switch (judge(item))
        {
        case fetch_candidate_verdict::take:
          lane.erase(lane.begin() + kept);
          return item;
        case fetch_candidate_verdict::drop:
          lane.erase(lane.begin() + kept);
          break;
        case fetch_candidate_verdict::keep:
          ++kept;
          break;
        }
      }
      return std::nullopt;
    }

    template<typename Judge>
    static std::optional<item_id> take_newest_from(std::deque<entry>& lane, size_t kept, size_t& newest_kept, Judge& judge,
                                                   uint64_t& examined)
    {
      while (kept + newest_kept < lane.size())
      {
        ++examined;
        const auto position = lane.end() - 1 - newest_kept;
        const item_id item = position->item;
        switch (judge(item))
        {
        case fetch_candidate_verdict::take:
          lane.erase(position);
          return item;
        case fetch_candidate_verdict::drop:
          lane.erase(position);
          break;
        case fetch_candidate_verdict::keep:
          ++newest_kept;
          break;
        }
      }
      return std::nullopt;
    }

    std::deque<entry> _blocks;
    std::deque<entry> _transactions;
    size_t _blocks_kept = 0;
    size_t _transactions_kept = 0;
    size_t _transactions_newest_kept = 0;
  };

  /**
   * The order in which the peers of a fetch pass take their turns, one item each, round after round until none
   * takes one.  The peers are listed in connection order, which stays the same from pass to pass, and the turns
   * start with the first listed peer after the one that took the last transaction of an earlier pass, wrapping
   * round.  So when the headroom (transaction_fetch_gate) is worth only a few transactions per pass it reaches
   * every peer with candidates over successive passes, instead of always the first ones in connection order.
   * Peers are identified by address only; a peer that disconnected is never visited, and the turns then start
   * with the first listed peer.
   */
  class fetch_turn_order
  {
  public:
    /// Called for every connected peer in connection order, before it is listed for the pass or not, with how many
    /// peers were listed before it
    void visit(const void* peer, size_t listed_before)
    {
      // the first listed peer after the taker is the one at the number of peers listed by the time the next peer
      // is visited (or the listing ends), whether the taker itself was listed or not
      if (_after_taker)
        _first_turn = listed_before;
      _after_taker = peer == _last_transaction_taker;
    }

    /**
     * Runs the turns of the @p listed peers: @p take_turn (called as `bool(size_t)` with the index of a listed
     * peer) lets that peer take one item and returns whether it took one.  Ends the pass's listing.
     */
    template<typename TakeTurn>
    void take_turns(size_t listed, TakeTurn&& take_turn)
    {
      if (_after_taker)
        _first_turn = listed;
      const size_t first = listed > 0 ? _first_turn % listed : 0;
      _first_turn = 0;
      _after_taker = false;
      for (bool item_taken = true; item_taken;)
      {
        item_taken = false;
        for (size_t turn = 0; turn < listed; ++turn)
          item_taken |= take_turn((first + turn) % listed);
      }
    }

    /// Records that @p peer took a transaction; the last one recorded decides where the next pass starts
    void record_transaction(const void* peer) { _last_transaction_taker = peer; }

  private:
    const void* _last_transaction_taker = nullptr;
    size_t _first_turn = 0;
    bool _after_taker = false;
  };

  /**
   * A peer's recent mean transaction size, from the transactions it sent us, and how many transactions one
   * request may ask it for so the expected reply stays within GRAPHENE_NET_MAX_TRANSACTION_BYTES_PER_FETCH.
   * Until the peer sends a transaction its mean is taken to be GRAPHENE_NET_INITIAL_TRANSACTION_SIZE_ESTIMATE.
   * A transaction larger than the mean moves it a quarter of the way, a smaller one a sixteenth, so a burst of
   * large transactions shrinks the next request quickly and requests grow back over a few round trips.
   */
  class transaction_size_estimate
  {
  public:
    transaction_size_estimate();

    void record(size_t transaction_size);
    size_t mean() const { return _mean; }
    /// whether the peer queues our replies only as its send queue drains (GRAPHENE_NET_HELLO_DEFERS_TRANSACTION_REPLIES);
    /// false until set
    void set_peer_defers_replies(bool defers) { _peer_defers_replies = defers; }
    bool peer_defers_replies() const { return _peer_defers_replies; }
    /// at least 1, at most GRAPHENE_NET_MAX_TRANSACTIONS_PER_PEER_DURING_NORMAL_OPERATION
    size_t max_transactions_per_request() const;
    /**
     * How many more transactions the peer may be asked for while @p in_flight are outstanding with it, in requests
     * of at most max_transactions_per_request() each: as many as keeps the outstanding ones within
     * GRAPHENE_NET_MAX_TRANSACTIONS_IN_FLIGHT_PER_PEER and their expected bytes within
     * GRAPHENE_NET_MAX_TRANSACTION_BYTES_IN_FLIGHT_PER_PEER, though always one full request; within one full request
     * unless the peer defers its replies (see set_peer_defers_replies()).  While some are
     * outstanding it is 0 until there is room for half a request, so a peer kept busy is asked in batches rather
     * than a few transactions at a time.
     */
    size_t transactions_to_request(size_t in_flight) const;

  private:
    size_t _mean;
    bool _peer_defers_replies = false;
  };

  /**
   * The items requested from one peer during normal operation that it has not sent or refused yet, each with the
   * time it was requested.  Counts the transactions among them, so the fetch loop sees at once how much of the
   * peer's transaction window is taken and whether a block is outstanding, and keeps when the peer last delivered
   * a transaction we asked for, which is what an outstanding transaction's timeout is measured from (see timed_out()).
   */
  class requested_items
  {
  public:
    using map_type = std::unordered_map<item_id, fc::time_point, p2p_item_hash>;
    using const_iterator = map_type::const_iterator;

    void insert(const item_id& item, fc::time_point requested);
    void erase(const_iterator iter);
    const_iterator find(const item_id& item) const { return _items.find(item); }
    const_iterator begin() const { return _items.begin(); }
    const_iterator end() const { return _items.end(); }
    bool empty() const { return _items.empty(); }
    size_t size() const { return _items.size(); }
    size_t transactions() const { return _transactions; }
    size_t blocks() const { return _items.size() - _transactions; }

    /// the peer delivered a transaction we asked it for; saying it doesn't have one is no progress
    void record_transaction_delivery(fc::time_point when) { _last_transaction_delivery = when; }
    /**
     * One of the items the peer has kept us waiting for longer than @p timeout at @p now, if any.  A transaction
     * waits from when it was requested or the peer last delivered one (record_transaction_delivery()), whichever is
     * later, so a peer delivering a large window at link speed is not taken for one that ignores us, but no longer
     * than GRAPHENE_NET_MAX_TRANSACTION_REQUEST_TIMEOUTS_WHILE_DELIVERING timeouts from its request.  A block waits
     * from when it was requested, whatever the peer sends meanwhile.
     */
    std::optional<item_id> timed_out(fc::time_point now, fc::microseconds timeout) const;

  private:
    map_type _items;
    size_t _transactions = 0;
    fc::time_point _last_transaction_delivery;
  };

  /**
   * What the peers are known to have of the transactions relayed within the inventory window: which peers
   * advertised each transaction to us and which we advertised it to.  One record per transaction, found with
   * one hash lookup, holds a bitmap of each kind over peer slots, so recording or checking a peer is a bit
   * operation rather than a lookup in a set of the peer's own.
   *
   * A peer holds a slot from acquire_slot() to release_slot(), which clears the slot from every record, so a
   * reused slot starts with no knowledge.  Slots are reused lowest first and a record's bitmaps are widened
   * only when a bit past their width is set, so they take one word per kind, inside the record, up to 64 peers.
   *
   * A record expires once the inventory window has passed since a peer was last recorded in it (a peer's
   * knowledge outlives its own advertisement by however much later the last other peer was recorded).
   * Records are kept in that order, so expire() looks only at the expired ones.  All calls from one thread.
   */
  class transaction_inventory
  {
  public:
    using slot_type = uint32_t;

    class record
    {
    public:
      explicit record(const item_hash_t& item) : item(item) {}

      bool advertised_to_us(slot_type slot) const { return test(slot, advertised_to_us_kind); }
      bool advertised_to_peer(slot_type slot) const { return test(slot, advertised_to_peer_kind); }

    private:
      friend class transaction_inventory;
      static constexpr unsigned advertised_to_us_kind = 0;
      static constexpr unsigned advertised_to_peer_kind = 1;
      static constexpr unsigned kinds = 2;

      bool test(slot_type slot, unsigned kind) const;
      /// whether the bit was clear
      bool set(slot_type slot, unsigned kind) const;
      /// whether the bit was set
      bool reset(slot_type slot, unsigned kind) const;

      item_hash_t item;
      // the records' container only indexes `item`, so the rest may change in place.
      // Word kinds * w + kind holds the bits of slots 64 * w to 64 * w + 63
      mutable boost::container::small_vector<uint64_t, kinds> bits;
      mutable fc::time_point_sec last_recorded;
    };

    slot_type acquire_slot();
    void release_slot(slot_type slot);

    /// The record of @p item, or null if there is none.  Valid until the next expire() or release_slot()
    const record* find(const item_hash_t& item) const;
    /// The record of @p item, created if there is none.  Valid until the next expire() or release_slot()
    const record& get(const item_hash_t& item, fc::time_point_sec now);

    bool advertised_to_us(slot_type slot, const item_hash_t& item) const;
    /// Returns whether the peer in @p slot had not advertised @p item to us yet
    bool record_advertised_to_us(slot_type slot, const item_hash_t& item, fc::time_point_sec now);
    /// The peer in @p slot doesn't have @p item after all
    void forget_advertised_to_us(slot_type slot, const item_hash_t& item);
    /// Returns whether we had not advertised @p item_record to the peer in @p slot yet
    bool record_advertised_to_peer(const record& item_record, slot_type slot, fc::time_point_sec now);
    /// Keeps the record of @p item, if there is one, as if a peer had been recorded in it at @p now: while a peer
    /// we asked for it may still deliver it, the record must still say who else advertised it
    void keep(const item_hash_t& item, fc::time_point_sec now);
    /// We received and validated @p item: every peer that advertised it to us counts as having been
    /// advertised it instead, so it no longer counts against the peer's limit of advertisements to us
    /// but we still don't advertise it back
    void mark_received(const item_hash_t& item);

    /// Forgets the records nobody was recorded in since before @p oldest_to_keep
    void expire(fc::time_point_sec oldest_to_keep);

    uint32_t advertised_to_us_count(slot_type slot) const;
    uint32_t advertised_to_peer_count(slot_type slot) const;
    size_t size() const { return _records.size(); }

  private:
    struct slot_state
    {
      bool in_use = false;
      std::array<uint32_t, record::kinds> counts{};
    };
    struct by_item{};
    struct by_last_recorded{};
    typedef boost::multi_index_container<
      record,
      boost::multi_index::indexed_by<
        boost::multi_index::hashed_unique<boost::multi_index::tag<by_item>,
                                          boost::multi_index::member<record, item_hash_t, &record::item>,
                                          p2p_item_hash>,
        boost::multi_index::sequenced<boost::multi_index::tag<by_last_recorded>>>> record_set_type;

    void set_bit(const record& item_record, slot_type slot, unsigned kind);
    bool set_bit_and_record_time(const record& item_record, slot_type slot, unsigned kind, fc::time_point_sec now);
    void record_time(const record& item_record, fc::time_point_sec now);
    void reset_bit(const record& item_record, slot_type slot, unsigned kind);

    record_set_type _records;
    std::vector<slot_state> _slots;
  };

  /**
   * What the node's p2p work cost and how it was batched since the last report, for the periodic
   * GRAPHENE_NET_STATISTICS_LOGGER report.  Counting is plain arithmetic, cheap enough to do whether or not
   * the report is written.  All calls must come from one thread.
   */
  class p2p_statistics
  {
  public:
    /// fetch_items_loop()
    struct fetch_scheduling
    {
      uint64_t wakes = 0;
      uint64_t candidates_examined = 0;
      uint64_t items_requested = 0;
      uint64_t items_expired = 0;
      uint64_t transactions_gated = 0; ///< wakes in which the gate (transaction_fetch_gate::held_back()) kept transactions from being requested
      uint64_t transactions_bounded = 0; ///< wakes in which the gate's headroom (transaction_fetch_gate::bounded()) kept transactions from being requested
      fc::microseconds scheduling_time;
    };

    /// completion latency bucket upper bounds in microseconds; the last bucket has none
    static constexpr std::array<int64_t, 6> completion_latency_bounds_us{100, 250, 500, 1000, 2000, 5000};

    /// transactions received from peers and handed to the client for validation
    struct transaction_handoff
    {
      uint64_t submissions = 0;
      uint64_t completions = 0;
      uint32_t max_in_flight = 0;
      uint64_t in_flight_sum = 0; ///< in flight after each submission, for the mean
      std::array<uint64_t, completion_latency_bounds_us.size() + 1> completion_latency{}; ///< submit -> client result
      uint64_t limit_waits = 0;   ///< submissions that waited for the in-flight limit
      fc::microseconds limit_wait_time;
    };

    /// peer_connection socket writes
    struct send_batching
    {
      uint64_t writes = 0;
      uint64_t messages = 0;
      uint64_t bytes = 0;
    };

    /// fetch_items_message requests for transactions
    struct request_sizing
    {
      uint64_t transaction_requests = 0;
      uint64_t transactions_requested = 0;
      uint64_t byte_cap_bound = 0; ///< requests cut short by GRAPHENE_NET_MAX_TRANSACTION_BYTES_PER_FETCH
    };

    fetch_scheduling fetch;

    /// @p in_flight counts the submitted transaction
    void record_submission(uint32_t in_flight);
    void record_limit_wait(fc::microseconds waited);
    void record_completion(fc::microseconds latency);
    void record_write(size_t messages, size_t bytes);
    /// @p max_transactions is the most the request could ask for: the per-peer limit, or less when the byte cap is lower
    void record_transaction_request(size_t transactions, size_t max_transactions);

    const transaction_handoff& handoff() const { return _handoff; }
    const send_batching& sending() const { return _sending; }
    const request_sizing& requests() const { return _requests; }

    /**
     * Everything recorded since the last report (or since construction), as of @p now, with @p in_flight
     * transactions in flight now, @p items_queued items waiting to be fetched and the pending pool at @p pool;
     * counting starts over.  @p transactions_requested_per_peer holds, for each active peer, the transactions
     * requested from it in the period (the caller resets those counts); the report gives their spread.
     */
    fc::variant_object take_report(fc::time_point now, uint32_t in_flight, size_t items_queued,
                                   const pending_transaction_pool_size& pool,
                                   std::vector<uint64_t> transactions_requested_per_peer);

  private:
    fc::time_point _period_start = fc::time_point::now();
    transaction_handoff _handoff;
    send_batching _sending;
    request_sizing _requests;
  };

  /// Whether the transaction with @p message_hash is a relay sample: one in @p one_in, the same ones on every node
  bool is_relay_sample(const message_hash_type& message_hash, uint32_t one_in = GRAPHENE_NET_RELAY_SAMPLE_ONE_IN);

  /**
   * Microseconds since the epoch on the real-time clock, read with the raw clock_gettime system call so that a
   * library intercepting the call (libfaketime gives each test node its own offset) cannot shift it
   */
  int64_t raw_realtime_microseconds();

  /// A peer that advertised a block we want to fetch
  struct block_source_candidate
  {
    bool deprioritized = false;    ///< peer_connection::is_block_fetching_deprioritized()
    bool can_take_request = false; ///< we may send the peer another request now
    bool idle = false;             ///< nothing is outstanding with the peer, not even transactions
  };

  /**
   * Which of @p candidates, every peer that advertised a block, to request the block from now; none means wait.
   * Candidates come in the order we would rather ask them (least loaded first).  A deprioritized peer is
   * asked only when no other peer advertised the block: while one did but cannot take a request yet, the
   * block waits for it.  Among the others an idle one is asked before one with transactions outstanding,
   * whose replies the peer would send ahead of the block; one with transactions outstanding is asked only
   * if no idle one can take the request.  The returned index is always that of a candidate that can take a request.
   */
  std::optional<size_t> choose_block_source(const std::vector<block_source_candidate>& candidates);

  /// What we know about an item a peer just advertised to us; the peer is recorded as having it in any case
  struct advertised_item_status
  {
    bool is_block = false;
    bool being_processed = false;    ///< a copy of it is being pushed to the client now
    bool requested = false;          ///< we requested it from a peer and are waiting for it
    bool recently_failed = false;    ///< a copy we fetched recently failed to push
    bool queued = false;             ///< it is in our list of items to fetch, not requested yet
    bool new_from_this_peer = false; ///< the peer had not advertised it to us before
  };

  /// How an advertisement described by advertised_item_status bears on fetching the item
  struct advertised_item_fetch_decision
  {
    bool queue = false;             ///< add it to our list of items to fetch
    bool refresh_timestamp = false; ///< it's queued already; this peer will keep it in its cache longer
    bool wake_fetch_loop = false;
  };

  /**
   * An item being processed, already requested or recently failed is not fetched (again).  A block that is
   * queued already wakes the fetch loop when the advertisement gives it a new source: the iteration that
   * queued it may have found no peer it could ask.  A repeated advertisement or a queued transaction does not.
   */
  advertised_item_fetch_decision decide_advertised_item_fetch(const advertised_item_status& status);

  /**
   * The connection to @p remote_endpoint that has not finished closing, or null if there is none.
   * Connections still closing or terminating count as well as active and handshaking ones: until
   * their socket is gone, a new outbound connection bound to our listening port would need the
   * same local and remote address/port pair and fail with "Cannot assign requested address".
   */
  peer_connection_ptr find_unclosed_connection_to_endpoint(const fc::ip::endpoint& remote_endpoint,
                                                           const std::unordered_set<peer_connection_ptr>& active_connections,
                                                           const std::unordered_set<peer_connection_ptr>& handshaking_connections,
                                                           const std::unordered_set<peer_connection_ptr>& closing_connections,
                                                           const std::unordered_set<peer_connection_ptr>& terminating_connections);

  /**
   * Whether the connect loop may dial the peer in @p record now, as far as its connection history
   * goes: a peer whose last connection failed, was rejected or failed handshaking waits
   * @p delay_until_retry after the last attempt.  The caller must still skip a peer it is
   * connected to (find_unclosed_connection_to_endpoint).
   */
  bool is_due_for_connection_attempt(const potential_peer_record& record, fc::microseconds delay_until_retry,
                                     fc::time_point now);

  /**
   * Whether @p connect_error, thrown by connecting a socket, means the local address/port pair is
   * still taken for that remote endpoint (EADDRNOTAVAIL): with the socket bound to our listening
   * port, our own previous connection to the same endpoint still lingers in the kernel (FIN_WAIT,
   * TIME_WAIT).  That is a condition of this host, not a failure of the remote peer.
   */
  bool is_local_endpoint_in_use_error(const fc::exception& connect_error);

  /**
   * The host a connection to @p address counts against for the per-host connection limit: an IPv4
   * address (an IPv4-mapped IPv6 address counts as the IPv4 address it maps), or the /64 prefix of
   * an IPv6 address, the block one machine is normally given.  ::1 and link-local addresses (fe80::/10),
   * which every machine on a link has its own of in the same /64, stay the full address.
   * fc::ip::address keeps no scope id, so the same link-local address seen on two links is one host.
   */
  fc::ip::address connection_host_of(const fc::ip::address& address);

  /**
   * Whether one more connection to @p address would exceed @p maximum_connections_per_host, given the
   * remote addresses of the connections already held (@p held_addresses, not including the new one).
   * Nothing is when @p maximum_connections_per_host is 0.  Exemptions (address_subnet, seeds) are the
   * caller's to apply.
   */
  bool exceeds_connections_per_host(const fc::ip::address& address, const std::vector<fc::ip::address>& held_addresses,
                                    uint32_t maximum_connections_per_host);

  /// An address block, e.g. 10.0.0.0/8 or 2001:db8::/32; one address is a block of its full length
  struct address_subnet
  {
    fc::ip::address network;
    uint8_t prefix_length = 0;

    /**
     * Parses "address" or "address/prefix_length".  Throws fc::exception for an unparsable address, or
     * a prefix length over the family's address length.
     */
    static address_subnet from_string(const std::string& text);

    /// An IPv4-mapped IPv6 address is matched as the IPv4 address it maps
    bool contains(const fc::ip::address& address) const;
  };

  bool is_in_any_subnet(const fc::ip::address& address, const std::vector<address_subnet>& subnets);

  /**
   * Whether a refused hello from @p refused_address comes from the host most inbound connections do:
   * counting the refused one, more than half of @p inbound_addresses (the remote addresses of the inbound
   * connections held) and it are on its host.  That is what a proxy rewriting source addresses looks like.
   */
  bool inbound_connections_share_host(const fc::ip::address& refused_address,
                                      const std::vector<fc::ip::address>& inbound_addresses);

  /// A peer the connect loop could dial now: routable, due for an attempt and not connected
  struct connection_candidate
  {
    fc::ip::endpoint endpoint;
    bool exempt_from_host_limit = false; ///< a seed or add-node target or an exempt address: dialed whatever its host holds already
  };

  struct connection_selection_limits
  {
    uint32_t free_slots = 0;                      ///< how many connections to start at most
    uint32_t desired_connections = 0;             ///< the reserved family share is a percentage of these
    uint32_t maximum_connections_per_host = 0;    ///< 0 for no limit
    uint32_t reserved_address_family_percent = 0; ///< capped at 50
    /// the per-host limit is the default one, not set by the operator: slots no candidate under it can fill go to the others
    bool relax_connections_per_host = false;
  };

  /**
   * Which of @p candidates (in peer database order) to dial, in dialing order.  @p held_addresses are the
   * remote addresses of the connections held already, active and handshaking.
   *
   * No candidate is picked whose host would then exceed the per-host limit (exceeds_connections_per_host),
   * unless it is exempt; an exempt connection still counts against its host's limit for the others.  Each
   * family is first given candidates until it holds its reserved share of the desired connections, or runs
   * out of candidates; the remaining slots go to IPv6 candidates first, then IPv4 ones.  So with candidates of
   * both families, neither is shut out by the other filling the slots first.  With relax_connections_per_host, the
   * slots still free after that go to the candidates the per-host limit left out, one step at a time: at step
   * limit + 1, then limit + 2, ..., each host holding fewer than the step gets one more connection, the host then
   * holding the fewest first (the first in database order among equals).  The steps stop at
   * get_relaxed_connections_per_host() of the hosts' connections held plus candidates.
   */
  std::vector<fc::ip::endpoint> select_connection_candidates(const std::vector<connection_candidate>& candidates,
                                                             const std::vector<fc::ip::address>& held_addresses,
                                                             const connection_selection_limits& limits);

  /**
   * How many connections a host may hold while the default per-host limit is relaxed.  @p available_per_host holds,
   * for each host, how many connections it could take (held plus candidates).  That is the lowest step, from
   * @p maximum_connections_per_host up, at which the hosts together reach @p desired_connections; when no step
   * does, each host's share of them, ceil(@p desired_connections / hosts), so that the host with the most candidates
   * does not take them all while the slots stay short anyway.  Never below @p maximum_connections_per_host; 0 when it
   * is 0 (no limit).
   */
  uint32_t get_relaxed_connections_per_host(const std::vector<uint32_t>& available_per_host, uint32_t maximum_connections_per_host,
                                            uint32_t desired_connections);

  /**
   * Whether a hello from @p address, whose host holds @p maximum_connections_per_host or more of
   * @p accepted_addresses (the remote addresses of the connections whose hello was accepted, not the new one), is
   * accepted all the same while the default limit is relaxed: with fewer than @p desired_connections accepted, its
   * host under get_relaxed_connections_per_host() (each host's connections held, one more for this one), and no
   * other host over the limit holding fewer than it, so that one host at a time goes a step further.
   */
  bool accepts_connection_over_host_limit(const fc::ip::address& address, const std::vector<fc::ip::address>& accepted_addresses,
                                          uint32_t maximum_connections_per_host, uint32_t desired_connections);

  /// How a set of connections spreads over hosts (connection_host_of) and address families
  struct connection_spread
  {
    uint32_t hosts = 0;
    uint32_t ipv4 = 0;
    uint32_t ipv6 = 0;
  };

  connection_spread get_connection_spread(const std::vector<fc::ip::address>& addresses);

  /// A connection held, as the per-host limit sees it
  struct host_limited_connection
  {
    fc::ip::address address;
    bool exempt = false; ///< a seed or add-node target or an exempt address: counts against its host, but is never over the limit
  };

  /**
   * Which of @p connections (its index) to close so that a connection to a host under the per-host limit can take its
   * slot, or none: a connection that is not exempt, on a host holding more than @p maximum_connections_per_host
   * connections, the one with the most; the last such one of that host in @p connections.  None when
   * @p maximum_connections_per_host is 0.
   */
  fc::optional<size_t> select_connection_over_host_limit(const std::vector<host_limited_connection>& connections,
                                                         uint32_t maximum_connections_per_host);

  /// What the idle-peer sweep knows about one active peer
  struct idle_peer_candidate
  {
    /// peer_connection::last_useful_data_time, or since when our head is behind if that is later
    fc::time_point last_useful_data_time;
    bool exempt = false; ///< it is fed by us and claims no head past ours, or a request to it is still pending
  };

  /// The node-wide state the idle-peer sweep judges peers in; it runs only while our head is behind
  struct idle_peer_sweep
  {
    fc::time_point now;
    fc::microseconds window;  ///< idle_peer_disconnect_timeout; 0 replaces nobody
    size_t kept_at_least = 0; ///< active connections, idle or not, left after the sweep at least
    bool block_source_seen = false; ///< within the window a peer delivered us a block that we pushed; a claimed head doesn't count
  };

  /**
   * Which of @p peers (their indices) to disconnect as idle, the longest idle first.  A peer is idle when it is not
   * exempt and gave us no useful data for longer than the window.  Nobody is idle unless some peer delivered us a block
   * within the window (on a halted chain nobody has anything new, and nobody is replaced), and no more are picked than
   * leave sweep.kept_at_least of all @p peers.
   */
  std::vector<size_t> select_idle_peers_to_disconnect(const std::vector<idle_peer_candidate>& peers,
                                                      const idle_peer_sweep& sweep);

} } // graphene::net

FC_REFLECT(graphene::net::message_propagation_data, (received_time)(validated_time)(originating_peer));
FC_REFLECT(graphene::net::peer_status, (version)(host)(info));
