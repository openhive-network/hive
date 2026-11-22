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
#include <atomic>
#include <sstream>
#include <iomanip>
#include <deque>
#include <unordered_set>
#include <unordered_map>
#include <list>
#include <forward_list>
#include <set>
#include <iostream>
#include <algorithm>
#include <limits>
#include <tuple>
#include <boost/tuple/tuple.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/core/bit.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/logic/tribool.hpp>
#include <boost/range/algorithm_ext/push_back.hpp>
#include <boost/range/algorithm/find.hpp>
#include <boost/range/numeric.hpp>
#include <boost/scope_exit.hpp>
#include <boost/system/error_code.hpp>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
#include <boost/accumulators/statistics/min.hpp>
#include <boost/accumulators/statistics/max.hpp>
#include <boost/accumulators/statistics/sum.hpp>
#include <boost/accumulators/statistics/count.hpp>

#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/stringize.hpp>

#include <ifaddrs.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <time.h>
#include <netinet/in.h>
#include <cerrno>
#include <cstring>

#include <fc/thread/thread.hpp>
#include <fc/thread/future.hpp>
#include <fc/thread/non_preemptable_scope_check.hpp>
#include <fc/thread/mutex.hpp>
#include <fc/thread/scoped_lock.hpp>
#include <fc/log/logger.hpp>
#include <fc/io/json.hpp>
#include <fc/io/enum_type.hpp>
#include <fc/crypto/rand.hpp>
#include <fc/network/rate_limiting.hpp>
#include <fc/network/ip.hpp>
#include <fc/network/udp_socket.hpp>
#include <fc/smart_ref_impl.hpp>
#include <fc/bitutil.hpp>

#include <graphene/net/node_configuration.hpp>
#include <graphene/net/node.hpp>
#include <graphene/net/peer_database.hpp>
#include <graphene/net/peer_connection.hpp>
#include <graphene/net/stcp_socket.hpp>
#include <graphene/net/config.hpp>
#include <graphene/net/exceptions.hpp>
#include <graphene/net/message_cache.hpp>

#include <hive/protocol/config.hpp>
#include <hive/plugins/statsd/utility.hpp>
#include <hive/chain/block_compression_dictionaries.hpp>
#include <hive/chain/blockchain_worker_thread_pool.hpp>

#include <fc/git_revision.hpp>

//#define ENABLE_DEBUG_ULOGS

#ifdef DEFAULT_LOGGER
# undef DEFAULT_LOGGER
#endif
#define DEFAULT_LOGGER "p2p"

#define P2P_IN_DEDICATED_THREAD 1

#define INVOCATION_COUNTER(name) \
    static unsigned total_ ## name ## _counter = 0; \
    static unsigned active_ ## name ## _counter = 0; \
    struct name ## _invocation_logger { \
      unsigned *total; \
      unsigned *active; \
      name ## _invocation_logger(unsigned *total, unsigned *active) : \
        total(total), active(active) \
      { \
        ++*total; \
        ++*active; \
        dlog("NEWDEBUG: Entering " #name ", now ${total} total calls, ${active} active calls", ("total", *total)("active", *active)); \
      } \
      ~name ## _invocation_logger() \
      { \
        --*active; \
        dlog("NEWDEBUG: Leaving " #name ", now ${total} total calls, ${active} active calls", ("total", *total)("active", *active)); \
      } \
    } invocation_logger(&total_ ## name ## _counter, &active_ ## name ## _counter)

//log these messages even at warn level when operating on the test network
#ifdef GRAPHENE_TEST_NETWORK
#define testnetlog wlog
#else
#define testnetlog(...) do {} while (0)
#endif

namespace graphene { namespace net {

  namespace detail
  {
    // when requesting items from peers, we want to prioritize any blocks before
    // transactions, but otherwise request items in the order we heard about them
    struct prioritized_item_id
    {
      item_id item;
      unsigned sequence_number;
      fc::time_point timestamp; // the time we last heard about this item in an inventory message

      prioritized_item_id(const item_id& item, unsigned sequence_number) :
        item(item),
        sequence_number(sequence_number),
        timestamp(fc::time_point::now())
      {}
      bool operator<(const prioritized_item_id& rhs) const
      {
        static_assert(graphene::net::block_message_type > graphene::net::trx_message_type,
                      "block_message_type must be greater than trx_message_type for prioritized_item_ids to sort correctly");
        if (item.item_type != rhs.item.item_type)
          return item.item_type > rhs.item.item_type;
        return (signed)(rhs.sequence_number - sequence_number) > 0;
      }
    };

/////////////////////////////////////////////////////////////////////////////////////////////////////////
    class statistics_gathering_node_delegate_wrapper : public node_delegate
    {
    private:
      node_delegate *_node_delegate;

      typedef boost::accumulators::accumulator_set<int64_t, boost::accumulators::stats<boost::accumulators::tag::min,
                                                                                       boost::accumulators::tag::rolling_mean,
                                                                                       boost::accumulators::tag::max,
                                                                                       boost::accumulators::tag::sum,
                                                                                       boost::accumulators::tag::count> > call_stats_accumulator;
#define NODE_DELEGATE_METHOD_NAMES (has_item) \
                                   (handle_message) \
                                   (handle_block) \
                                   (submit_transaction) \
                                   (get_block_ids) \
                                   (get_item) \
                                   (get_blockchain_synopsis) \
                                   (sync_status) \
                                   (connection_count_changed) \
                                   (get_block_number) \
                                   (get_block_time) \
                                   (get_head_block_id) \
                                   (estimate_last_known_fork_from_git_revision_timestamp) \
                                   (error_encountered) \
                                   (find_first_item_not_in_blockchain) \
                                   (get_chain_id)


#define DECLARE_ACCUMULATOR(r, data, method_name) \
      mutable call_stats_accumulator BOOST_PP_CAT(_, BOOST_PP_CAT(method_name, _execution_accumulator)); \
      mutable call_stats_accumulator BOOST_PP_CAT(_, BOOST_PP_CAT(method_name, _delay_before_accumulator)); \
      mutable call_stats_accumulator BOOST_PP_CAT(_, BOOST_PP_CAT(method_name, _delay_after_accumulator));
      BOOST_PP_SEQ_FOR_EACH(DECLARE_ACCUMULATOR, unused, NODE_DELEGATE_METHOD_NAMES)
#undef DECLARE_ACCUMULATOR

      class call_statistics_collector
      {
      private:
        fc::time_point _call_requested_time;
        fc::time_point _begin_execution_time;
        fc::time_point _execution_completed_time;
        const char* _method_name;
        call_stats_accumulator* _execution_accumulator;
        call_stats_accumulator* _delay_before_accumulator;
        call_stats_accumulator* _delay_after_accumulator;
      public:
        class actual_execution_measurement_helper
        {
          call_statistics_collector &_collector;
        public:
          actual_execution_measurement_helper(call_statistics_collector& collector) :
            _collector(collector)
          {
            _collector.starting_execution();
          }
          ~actual_execution_measurement_helper()
          {
            _collector.execution_completed();
          }
        };
        call_statistics_collector(const char* method_name,
                                  call_stats_accumulator* execution_accumulator,
                                  call_stats_accumulator* delay_before_accumulator,
                                  call_stats_accumulator* delay_after_accumulator) :
          _call_requested_time(fc::time_point::now()),
          _method_name(method_name),
          _execution_accumulator(execution_accumulator),
          _delay_before_accumulator(delay_before_accumulator),
          _delay_after_accumulator(delay_after_accumulator)
        {}
        ~call_statistics_collector()
        {
          fc::time_point end_time(fc::time_point::now());
          fc::microseconds actual_execution_time(_execution_completed_time - _begin_execution_time);
          fc::microseconds delay_before(_begin_execution_time - _call_requested_time);
          fc::microseconds delay_after(end_time - _execution_completed_time);
          fc::microseconds total_duration(actual_execution_time + delay_before + delay_after);
          (*_execution_accumulator)(actual_execution_time.count());
          (*_delay_before_accumulator)(delay_before.count());
          (*_delay_after_accumulator)(delay_after.count());
          if (total_duration > fc::milliseconds(500))
          {
            ilog("Call to method node_delegate::${method} took ${total_duration}us, longer than our target maximum of 500ms",
                 ("method", _method_name)
                 ("total_duration", total_duration.count()));
            ilog("Actual execution took ${execution_duration}us, with a ${delegate_delay}us delay before the delegate thread started "
                 "executing the method, and a ${p2p_delay}us delay after it finished before the p2p thread started processing the response",
                 ("execution_duration", actual_execution_time)
                 ("delegate_delay", delay_before)
                 ("p2p_delay", delay_after));
          }
        }
        void starting_execution()
        {
          _begin_execution_time = fc::time_point::now();
        }
        void execution_completed()
        {
          _execution_completed_time = fc::time_point::now();
        }
      };
    public:
      statistics_gathering_node_delegate_wrapper(node_delegate* delegate);

      fc::variant_object get_call_statistics();

      hive::protocol::chain_id_type get_new_chain_id() const override;
      hive::protocol::chain_id_type get_chain_id() const override;
      bool has_item( const net::item_id& id ) override;
      void handle_message( const message& ) override;
      block_handling_result handle_block(const std::shared_ptr<full_block_type>& full_block, bool sync_mode) override;
      void submit_transaction(const std::shared_ptr<full_transaction_type>& full_transaction,
                              transaction_validated_callback on_validated) override;
      std::vector<item_hash_t> get_block_ids(const std::vector<item_hash_t>& blockchain_synopsis,
                                             uint32_t& remaining_item_count,
                                             uint32_t limit = DEFAULT_MAX_BLOCK_IDS_TO_FETCH) override;
      std::shared_ptr<hive::chain::full_block_type> get_full_block(const block_id_type& id) override;
      std::vector<item_hash_t> get_blockchain_synopsis(const item_hash_t& reference_point,
                                                       uint32_t number_of_blocks_after_reference_point) override;
      void     sync_status( uint32_t item_type, uint32_t item_count ) override;
      void     connection_count_changed( uint32_t c ) override;
      uint32_t get_block_number(const item_hash_t& block_id) override;
      fc::time_point_sec get_block_time(const item_hash_t& block_id) override;
      fc::time_point_sec get_blockchain_now() override;
      item_hash_t get_head_block_id() const override;
      uint32_t get_last_irreversible_block_num() const override;
      uint32_t get_tail_block_num() const override;
      bool is_at_live_sync() override;
      pending_transaction_pool_size get_pending_transaction_pool_size() const override;
      uint32_t estimate_last_known_fork_from_git_revision_timestamp(uint32_t unix_timestamp) const override;
      void error_encountered(const std::string& message, const fc::oexception& error) override;
      std::deque<block_id_type>::const_iterator find_first_item_not_in_blockchain(const std::deque<block_id_type>& item_hashes_received) override;
    };

/////////////////////////////////////////////////////////////////////////////////////////////////////////
    class node_impl : public peer_connection_delegate
    {
    public:
#ifdef P2P_IN_DEDICATED_THREAD
      std::shared_ptr<fc::thread> _thread;
#endif // P2P_IN_DEDICATED_THREAD
      std::unique_ptr<statistics_gathering_node_delegate_wrapper> _delegate;

#define NODE_CONFIGURATION_FILENAME      "node_config.json"
#define POTENTIAL_PEER_DATABASE_FILENAME "peers.json"
      fc::path             _node_configuration_directory;
      node_configuration   _node_configuration;

      /// stores the endpoint we're listening on.  This will be the same as
      // _node_configuration.listen_endpoint, unless that endpoint was already
      // in use.
      fc::ip::endpoint     _actual_listening_endpoint;

      /// we determine whether we're firewalled by asking other nodes.  Store the result here:
      firewalled_state     _is_firewalled;
      /// if we're behind NAT, our listening endpoint address will appear different to the rest of the world.  store it here.
      fc::optional<fc::ip::endpoint> _publicly_visible_listening_endpoint;
      fc::time_point       _last_firewall_check_message_sent;

      /// used by the task that manages connecting to peers
      // @{
      std::list<potential_peer_record> _add_once_node_list; /// list of peers we want to connect to as soon as possible
      /// seed and add-node targets: dialing them, and their hellos on the connections we dialed, is not
      /// limited by maximum_connections_per_host (though they count against their host's limit)
      std::set<fc::ip::endpoint> _host_limit_exempt_endpoints;
      /// parsed from _node_configuration.host_limit_exempt_subnets: not limited by maximum_connections_per_host at all
      std::vector<address_subnet> _host_limit_exempt_subnets;
      fc::time_point _last_shared_inbound_host_warning_time;
      /// inbound connections closed at accept (get_inbound_connection_refusal()) since the last warning about them
      uint32_t _refused_inbound_connections_since_warning = 0;
      fc::time_point _last_refused_inbound_connection_warning_time;
      /// until when a relaxed per-host limit stays hard: host_limit_relax_delay after connect_to_p2p_network()
      fc::time_point _host_limit_hard_until = fc::time_point::maximum();
      fc::time_point _last_host_limit_relaxed_log_time;

      peer_database             _potential_peer_db;
      coalescing_loop_trigger   _connect_loop_trigger;
      bool                      _potential_peer_database_updated;
      fc::future<void>          _p2p_network_connect_loop_done;
      /// cached results of the per-address route probe (see can_route_to); entries expire so
      /// interface/route changes are picked up
      struct route_probe_result
      {
        bool           routable;
        fc::time_point expiration;
      };
      std::unordered_map<fc::ip::address, route_probe_result> _route_probe_cache;
      /// how many candidates the last connect-loop pass skipped for lack of a route, per family;
      /// used to log the situation once when it changes rather than on every pass
      uint32_t                  _unroutable_ipv4_candidates_last_pass = 0;
      uint32_t                  _unroutable_ipv6_candidates_last_pass = 0;
      // @}

      /// used by the task that fetches sync items during synchronization
      // @{
      fc::promise<void>::ptr    _retrigger_fetch_sync_items_loop_promise;
      bool                      _sync_items_to_fetch_updated;
      fc::future<void>          _fetch_sync_items_loop_done;

      typedef std::unordered_map<graphene::net::block_id_type, fc::time_point, p2p_item_hash> active_sync_requests_map;

      active_sync_requests_map              _active_sync_requests; /// list of sync blocks we've asked for from peers but have not yet received

      struct received_sync_item
      {
        std::shared_ptr<full_block_type> full_block;
        // the peer that delivered this copy, the only one to blame if its body turns out not to match its header
        // (weak: a block received from a peer that has since disconnected is still worth pushing)
        std::weak_ptr<peer_connection> delivering_peer;
      };
      struct compare_received_sync_items_by_block_id
      {
        typedef const item_hash_t& result_type;
        const item_hash_t& operator()(const received_sync_item& item) const
        {
          return item.full_block->get_block_id();
        }
      };
      typedef boost::multi_index_container<received_sync_item,
                                           boost::multi_index::indexed_by<boost::multi_index::ordered_unique<compare_received_sync_items_by_block_id>>> received_sync_items_by_id;

      std::set<item_hash_t>  _already_received_sync_item_ids; //set for fast lookup of already received sync items
      std::list<received_sync_item> _new_received_sync_items; /// list of sync blocks we've just received but haven't yet tried to process
      received_sync_items_by_id _received_sync_items; /// set of sync blocks we've received, but can't yet process because we are still missing blocks that come earlier in the chain
      // @}

      fc::future<void> _process_backlog_of_sync_blocks_done;
      bool _suspend_fetching_sync_blocks;

      /// used by the task that fetches items during normal operation
      // @{
      coalescing_loop_trigger _fetch_items_loop_trigger;
      fc::future<void>       _fetch_item_loop_done;

      struct item_id_index{};
      struct item_timestamp_index{};
      typedef boost::multi_index_container<prioritized_item_id,
                                           boost::multi_index::indexed_by<boost::multi_index::ordered_unique<boost::multi_index::identity<prioritized_item_id> >,
                                                                          boost::multi_index::hashed_unique<boost::multi_index::tag<item_id_index>,
                                                                                                            boost::multi_index::member<prioritized_item_id, item_id, &prioritized_item_id::item>,
                                                                                                            p2p_item_hash >,
                                                                          boost::multi_index::ordered_non_unique<boost::multi_index::tag<item_timestamp_index>,
                                                                                                                 boost::multi_index::member<prioritized_item_id, fc::time_point, &prioritized_item_id::timestamp> > >
                                           > items_to_fetch_set_type;
      unsigned _items_to_fetch_sequence_counter;
      /// list of items we know another peer has and we want; each is also in the fetch_candidates of the peers that advertised it
      items_to_fetch_set_type _items_to_fetch;
      peer_connection::timestamped_items_set_type _recently_failed_items; /// list of transactions we've recently pushed and had rejected by the delegate
      /// what our active peers advertised to us and we advertised to them of recent transactions
      transaction_inventory _transaction_inventory;
      // @}

      /// used by the task that advertises inventory during normal operation
      // @{
      fc::promise<void>::ptr        _retrigger_advertise_inventory_loop_promise;
      fc::future<void>              _advertise_inventory_loop_done;
      std::set<std::shared_ptr<full_block_type>> _new_block_inventory;
      std::set<std::shared_ptr<full_transaction_type>> _new_transaction_inventory;
      // @}

      fc::future<void>     _terminate_inactive_connections_loop_done;
      uint8_t _recent_block_interval_in_seconds; // a cached copy of the block interval, to avoid a thread hop to the blockchain to get the current value

      std::string          _user_agent_string;
      /** _node_public_key is a key automatically generated when the client is first run, stored in
       * node_config.json.  It doesn't really have much of a purpose yet, there was just some thought
       * that we might someday have a use for nodes having a private key (sent in hello messages)
       */
      node_id_t            _node_public_key;
      /**
       * _node_id is a random number generated each time the client is launched, used to prevent us
       * from connecting to the same client multiple times (sent in hello messages).
       * Since this was introduced after the hello_message was finalized, this is sent in the
       * user_data field.
       * While this shares the same underlying type as a public key, it is really just a random
       * number.
       */
      node_id_t            _node_id;

      fc::tcp_server       _tcp_server;
      fc::future<void>     _accept_loop_complete;

      /** Stores all connections which have not yet finished key exchange or are still sending initial handshaking messages
       * back and forth (not yet ready to initiate syncing) */
      std::unordered_set<peer_connection_ptr>                     _handshaking_connections;
      /** stores fully established connections we're either syncing with or in normal operation with */
      std::unordered_set<peer_connection_ptr>                     _active_connections;
      /** stores connections we've closed (sent closing message, not actually closed), but are still waiting for the remote end to close before we delete them */
      std::unordered_set<peer_connection_ptr>                     _closing_connections;
      /** stores connections we've closed, but are still waiting for the OS to notify us that the socket is really closed */
      std::unordered_set<peer_connection_ptr>                     _terminating_connections;

      boost::circular_buffer<item_hash_t> _most_recent_blocks_accepted; // the /n/ most recent blocks we've accepted (currently tuned to the max number of connections)
      disconnect_breaker _invalid_block_disconnect_breaker{GRAPHENE_NET_INVALID_BLOCK_DISCONNECT_BREAKER_COUNT,
                                                           fc::seconds(GRAPHENE_NET_INVALID_BLOCK_DISCONNECT_BREAKER_WINDOW_SEC)};
      disconnect_breaker _block_body_mismatch_disconnect_breaker{GRAPHENE_NET_BLOCK_BODY_MISMATCH_DISCONNECT_BREAKER_COUNT,
                                                                 fc::seconds(GRAPHENE_NET_BLOCK_BODY_MISMATCH_DISCONNECT_BREAKER_WINDOW_SEC)};
      // monotone mirrors of the chain state used to judge advertised/queued block ids without a
      // delegate round trip per decision; refreshed on every accepted block (staleness is
      // conservative: at worst we briefly treat a just-dead fork block as still fetchable)
      uint32_t _cached_head_block_num = 0;
      uint32_t _cached_last_irreversible_block_num = 0;
      fc::time_point _last_head_advance_time; // when _cached_head_block_num last increased
      fc::time_point _last_block_from_any_peer_time; // when a peer last gave us a block that we pushed; see record_block_delivered_by()
      fc::time_point _behind_since; // since when our head is behind with the chain not held; see expand_peers_while_starved()
      // the running peer expansion (see expand_peers_while_starved()): when it started, since when the peers we had
      // delivered no block when it did, and how many connections it holds slots for; a target of 0 while none runs
      fc::time_point _peer_expansion_started;
      fc::time_point _peer_expansion_starved_since;
      uint32_t _peer_expansion_target = 0;
      fc::time_point _peer_expansion_ended; // when the last peer expansion ended
      // until when a peer we replaced may not take a slot again, by node id and by the endpoint we dial it at
      std::map<node_id_t, fc::time_point> _replaced_peer_backoff_by_node_id;
      std::map<fc::ip::endpoint, fc::time_point> _replaced_peer_backoff_by_endpoint;
      // the node id that answered our hello at each endpoint we dialed, so another address of a node we are connected to
      // is not dialed (see is_endpoint_of_connected_node()); at most GRAPHENE_NET_MAX_PEERDB_SIZE entries
      std::map<fc::ip::endpoint, node_id_t> _node_id_by_dialed_endpoint;

      uint32_t _sync_item_type;
      uint32_t _total_number_of_unfetched_items; /// the number of items we still need to fetch while syncing
      std::vector<uint32_t> _hard_fork_block_numbers; /// list of all block numbers where there are hard forks

      blockchain_tied_message_cache<std::shared_ptr<full_block_type>> _block_message_cache; /// cache blocks we have received and might be required to provide to other peers via inventory requests
      blockchain_tied_message_cache<std::shared_ptr<full_transaction_type>> _transaction_message_cache{GRAPHENE_NET_TRANSACTION_MESSAGE_CACHE_EXPECTED_SIZE}; /// cache blocks we have received and might be required to provide to other peers via inventory requests

      fc::rate_limiting_group _rate_limiter;

      uint32_t _last_reported_number_of_connections; // number of connections last reported to the client (to avoid sending duplicate messages)

      fc::future<void> _fetch_updated_peer_lists_loop_done;

      boost::circular_buffer<uint32_t> _average_network_read_speed_seconds;
      boost::circular_buffer<uint32_t> _average_network_write_speed_seconds;
      boost::circular_buffer<uint32_t> _average_network_read_speed_minutes;
      boost::circular_buffer<uint32_t> _average_network_write_speed_minutes;
      boost::circular_buffer<uint32_t> _average_network_read_speed_hours;
      boost::circular_buffer<uint32_t> _average_network_write_speed_hours;
      unsigned _average_network_usage_second_counter;
      unsigned _average_network_usage_minute_counter;

      fc::time_point_sec _bandwidth_monitor_last_update_time;
      fc::future<void> _bandwidth_monitor_loop_done;

      fc::future<void> _dump_node_status_task_done;

      p2p_statistics _statistics;
      fc::future<void> _statistics_report_loop_done;
      fetch_expiry_warning_limiter _fetch_expiry_warnings{fc::seconds(GRAPHENE_NET_GATED_FETCH_EXPIRY_WARNING_INTERVAL_SEC)};
      // strike and disconnect-by-strikes warnings are logged at most once per GRAPHENE_NET_WITHHOLDING_WARNING_INTERVAL_SEC;
      // those in between are counted in the next one
      fc::time_point _last_withholding_warning_time;
      uint32_t _strikes_since_withholding_warning = 0;
      uint32_t _strike_disconnects_since_withholding_warning = 0;

      /* We have two alternate paths through the schedule_peer_for_deletion code -- one that
       * uses a mutex to prevent one fiber from adding items to the queue while another is deleting
       * items from it, and one that doesn't.  The one that doesn't is simpler and more efficient
       * code, but we're keeping around the version that uses the mutex because it crashes, and
       * this crash probably indicates a bug in our underlying threading code that needs
       * fixing.  To produce the bug, define USE_PEERS_TO_DELETE_MUTEX and then connect up
       * to the network and set your desired/max connection counts high
       */
//#define USE_PEERS_TO_DELETE_MUTEX 1
#ifdef USE_PEERS_TO_DELETE_MUTEX
      fc::mutex _peers_to_delete_mutex;
#endif
      std::list<peer_connection_ptr> _peers_to_delete;
      fc::future<void> _delayed_peer_deletion_task_done;

#ifdef ENABLE_P2P_DEBUGGING_API
      std::set<node_id_t> _allowed_peers;
#endif // ENABLE_P2P_DEBUGGING_API

      std::atomic_bool _node_is_shutting_down; // set to true when we begin our destructor, used to prevent us from starting new tasks while we're shutting down

      std::list<fc::future<void>> _handle_message_calls_in_progress;
      std::set<message_hash_type> _message_ids_currently_being_processed;

      /// the outcome of validating a transaction a peer sent us; it refers to the peer only by node id,
      /// because the peer may be gone by the time the outcome is applied
      struct transaction_completion
      {
        std::shared_ptr<full_transaction_type> full_transaction;
        message_hash_type message_hash;
        fc::time_point received_time;
        node_id_t originating_peer;
        fc::time_point submitted_time;
        fc::time_point validated_time;
        fc::exception_ptr error;
      };
      /// filled by the delegate's validation callbacks on the chain's writer thread, applied on our thread
      std::shared_ptr<completion_queue<transaction_completion>> _transaction_completions;
      in_flight_limit _transactions_in_flight;
      /// since when the transactions in flight have been at maximum_transactions_in_flight, while they are
      std::optional<fc::time_point> _in_flight_cap_reached_since;
      /// bytes of the transactions received from peers whose validation results have not been applied yet
      uint64_t _transaction_bytes_being_validated = 0;
      /// the mean size of the transactions peers sent us that validated, the size expected of a transaction
      /// requested from a peer that has not sent one yet (see expected_transaction_bytes())
      transaction_size_estimate _validated_transaction_size;
      std::set<block_id_type> _block_ids_currently_being_processed;
      fetch_turn_order _fetch_turn_order;
      fetch_minimum_schedule _fetch_minimum_schedule{fc::milliseconds(GRAPHENE_NET_TRANSACTION_FETCH_GATE_RECHECK_MS)};

      std::atomic_int        _activeCalls;
      fc::promise<void>::ptr _shutdownNotifier;

      appbase::application& theApp;
      hive::chain::blockchain_worker_thread_pool& _thread_pool;

      node_impl(const std::string& user_agent, appbase::application& app, hive::chain::blockchain_worker_thread_pool& thread_pool);
      virtual ~node_impl();
      void save_active_peers_to_peer_database();

      void save_node_configuration();

      bool can_route_to(const fc::ip::address& addr);
      void remember_own_endpoint(const fc::ip::endpoint& own_endpoint);
      void remember_own_listen_endpoints();
      void remember_own_endpoint_of_connection(peer_connection* peer);
      void p2p_network_connect_loop();
      void trigger_p2p_network_connect_loop();

      bool have_already_received_sync_item( const item_hash_t& item_hash );
      void request_sync_item_from_peer( const peer_connection_ptr& peer, const item_hash_t& item_to_request );
      void request_sync_items_from_peer( const peer_connection_ptr& peer, const std::vector<item_hash_t>& items_to_request );
      void update_last_requested_block_number_for_peers_on_this_fork(uint32_t last_requested_block_number, const item_hash_t& last_requested_block_id);
      void fetch_sync_items_loop();
      void trigger_fetch_sync_items_loop();

      /// a peer listed in a fetch pass, and the items it is to be asked for
      struct peer_and_items_to_fetch
      {
        peer_connection_ptr peer;
        bool include_transactions;
        size_t max_transactions;
        size_t max_transactions_per_request;
        size_t blocks = 0;
        size_t transactions = 0;
        bool done = false;
        std::vector<item_id> item_ids;
      };
      /// the peers listed in a pass of fetch_items_loop()
      struct fetch_pass
      {
        std::vector<peer_and_items_to_fetch> peers;
        /// each listed peer, and whether it ranks as an idle block source (see judge_fetch_candidate())
        std::unordered_map<const peer_connection*, bool> idle_block_sources;
      };
      void fetch_items_loop();
      fetch_pass list_peers_for_fetch_pass(fc::time_point pass_start, transaction_fetch_gate& gate,
                                           fc::time_point& next_peer_unblocked_time);
      void take_fetch_turns(fetch_pass& pass, transaction_fetch_gate& gate, fc::time_point& next_peer_unblocked_time);
      void send_fetch_requests(const fetch_pass& pass);
      /// the oldest time an item we heard about can be queued at and still be worth fetching
      fc::time_point oldest_item_timestamp_to_fetch() const;
      /// drops the items to fetch we heard about too long ago, returning how many
      size_t remove_expired_items_to_fetch();
      /// @p pass_peers: the peers listed in the fetch pass, each with whether it had nothing outstanding when the pass began
      fetch_candidate_verdict judge_fetch_candidate(const peer_connection_ptr& peer, const item_id& item,
                                                    const std::unordered_map<const peer_connection*, bool>& pass_peers);
      /// whether @p peer would be asked for transaction @p item: the transaction verdict of judge_fetch_candidate(),
      /// without taking the item
      bool is_transaction_candidate_wanted(const peer_connection* peer, const item_id& item) const;
      /// whether @p peer has a transaction candidate not yet looked at in this pass that it would be asked for;
      /// drops the unwanted ones before it
      bool has_transaction_to_fetch(const peer_connection_ptr& peer);
      /**
       * The expected bytes of the transactions requested from peers and not received yet, and the bytes of those
       * received and not validated yet: what is on its way into the pending pool.  A transaction leaves the first
       * part when it is received and the second when its validation result is applied on our thread.  The chain
       * publishes the pool size including an accepted transaction right after its result is queued for us, so
       * until we apply that result the transaction counts twice, here and in the published size; that only makes
       * the headroom smaller.  A result applied before the size is published undercounts by that transaction.
       */
      uint64_t transaction_bytes_on_their_way() const;
      /**
       * The bytes expected of a transaction requested from @p peer: its own recent mean once it has sent us one, and
       * until then the mean of the transactions peers sent us that validated.  A peer that never delivers one can't
       * make each of its requests count as a transaction of the largest size against the fetch gate's headroom
       */
      uint64_t expected_transaction_bytes(const peer_connection& peer) const;
      /**
       * Asks another peer that advertised it for each block whose outstanding requests are all past their deadlines
       * (see block_request_deadline()), while fewer than GRAPHENE_NET_MAX_OUTSTANDING_REQUESTS_PER_BLOCK are, and
       * deprioritizes the peers that kept us waiting for it.  Lowers @p next_check to when the next block comes due
       */
      void rerequest_overdue_blocks(fc::time_point now, fc::time_point& next_check);
      /// how many peers a block request for @p item is outstanding with
      uint32_t outstanding_block_requests(const item_id& item) const;
      /// whether a block request for @p item is outstanding with a peer other than @p peer (see rerequest_overdue_blocks())
      bool is_block_requested_from_another_peer(const peer_connection* peer, const item_id& item) const;
      void add_fetch_candidate(peer_connection* peer, const item_id& item);
      /// Adds @p item to the items to fetch, and to the fetch candidates of every peer that advertised it,
      /// if any did; returns whether one did
      bool queue_item_to_fetch_from_advertisers(const item_id& item);
      /// @p for_block: a block may be fetched now, so the loop runs at once instead of waiting out the minimum interval
      void trigger_fetch_items_loop(bool for_block);
      bool peer_advertised_a_queued_block(const peer_connection* peer) const;

      void advertise_inventory_loop();
      void trigger_advertise_inventory_loop();

      /// what a pass of terminate_inactive_connections_loop() decided to do with which connections
      struct inactive_connection_actions
      {
        std::vector<peer_connection_ptr> to_disconnect_gently;
        std::vector<peer_connection_ptr> to_disconnect_for_rejected_blocks;
        std::vector<peer_connection_ptr> to_disconnect_forcibly;
        std::vector<peer_connection_ptr> to_send_keepalive;
        std::vector<peer_connection_ptr> to_terminate;
        fc::microseconds inactivity_timeout; ///< the one active connections were judged by
      };
      void terminate_inactive_connections_loop();
      void find_peers_kept_by_stalled_breaker(fc::time_point now, inactive_connection_actions& actions);
      void judge_handshaking_connections(fc::time_point now, inactive_connection_actions& actions);
      active_connection_state get_active_connection_state(const peer_connection& peer, fc::time_point now,
                                                          fc::microseconds request_timeout) const;
      void judge_active_connections(fc::time_point now, inactive_connection_actions& actions);
      void close_for_request_timeout(const peer_connection_ptr& peer, inactive_connection_actions& actions,
                                     const fc::exception& reason);
      void judge_closing_connections(fc::time_point now, inactive_connection_actions& actions);

      void fetch_updated_peer_lists_loop();
      void update_bandwidth_data(uint32_t bytes_read_this_second, uint32_t bytes_written_this_second);
      void bandwidth_monitor_loop();
      void dump_node_status_task();
      void statistics_report_loop();

      bool is_accepting_new_connections(const peer_connection* peer);
      bool is_wanting_new_connections();
      uint32_t get_desired_number_of_connections() const;
      uint32_t get_number_of_connections();
      bool holds_connection_slot(const peer_connection& peer) const;
      uint32_t get_number_of_connections_holding_slots() const;
      const char* get_inbound_connection_refusal(const fc::ip::address& address);
      void warn_of_refused_inbound_connections();
      peer_connection_ptr get_peer_by_node_id(const node_id_t& id);

      bool is_already_connected_to_id(const node_id_t& node_id);
      /// remote addresses of the active and handshaking connections; with @p accepted_only, of those whose hello we accepted
      std::vector<fc::ip::address> get_connection_addresses(bool accepted_only, const peer_connection* excluded = nullptr);
      bool is_exempt_from_host_limit(const peer_connection* peer);
      void warn_if_inbound_connections_share_host(const fc::ip::address& refused_address);
      bool is_host_limit_relaxed() const;
      bool accept_hello_over_host_limit(const peer_connection* peer, const fc::ip::address& address);
      void log_host_limit_relaxed(const std::vector<fc::ip::address>& addresses);
      std::vector<connection_candidate> get_connection_candidates(uint32_t& unroutable_ipv4_candidates, uint32_t& unroutable_ipv6_candidates);
      connection_selection_limits get_connection_selection_limits(uint32_t free_slots) const;
      peer_connection_ptr find_active_connection_over_host_limit(const peer_connection* excluded);
      void dial_new_host_for_connection_over_host_limit();
      void give_way_to_connection_under_host_limit(const peer_connection* new_peer);
      connection_spread get_active_connection_spread();
      bool merge_address_info_with_potential_peer_database( const std::vector<address_info> addresses );
      void display_current_connections();
      uint32_t calculate_unsynced_block_count_from_all_peers();
      std::vector<item_hash_t> create_blockchain_synopsis_for_peer( const peer_connection* peer );
      void fetch_next_batch_of_item_ids_from_peer( peer_connection* peer, bool reset_fork_tracking_data_for_peer = false );

      fc::variant_object generate_hello_user_data();
      void parse_hello_user_data_for_peer( peer_connection* originating_peer, const fc::variant_object& user_data );

      void on_message( peer_connection* originating_peer,
                       const message& received_message ) override;

      void on_hello_message( peer_connection* originating_peer,
                             const hello_message& hello_message_received );

      void on_connection_accepted_message( peer_connection* originating_peer,
                                           const connection_accepted_message& connection_accepted_message_received );

      void on_connection_rejected_message( peer_connection* originating_peer,
                                           const connection_rejected_message& connection_rejected_message_received );

      void on_address_request_message( peer_connection* originating_peer,
                                       const address_request_message& address_request_message_received );

      void on_address_message( peer_connection* originating_peer,
                               const address_message& address_message_received );

      void on_fetch_blockchain_item_ids_message( peer_connection* originating_peer,
                                                 const fetch_blockchain_item_ids_message& fetch_blockchain_item_ids_message_received );
      bool is_item_id_being_processed(const item_hash_t& block_id);

      void on_blockchain_item_ids_inventory_message( peer_connection* originating_peer,
                                                     const blockchain_item_ids_inventory_message& blockchain_item_ids_inventory_message_received );

      void on_fetch_items_message( peer_connection* originating_peer,
                                   const fetch_items_message& fetch_items_message_received );

      void on_item_not_available_message( peer_connection* originating_peer,
                                          const item_not_available_message& item_not_available_message_received );
      void on_item_busy_message( peer_connection* originating_peer, const item_busy_message& item_busy_message_received );

      void on_item_ids_inventory_message( peer_connection* originating_peer,
                                          const item_ids_inventory_message& item_ids_inventory_message_received );
      void record_advertised_item(peer_connection* originating_peer, const item_id& advertised_item_id);
      void process_deferred_transaction_advertisements(peer_connection* peer);
      void report_inventory_limit_hits(peer_connection* peer);
      /// whether @p peer advertised @p item to us and still counts as having it
      bool peer_advertised_item(const peer_connection* peer, const item_id& item) const;
      /// @p transaction_record is the record of the transaction in question, or null if there is none
      static bool peer_advertised_transaction(const peer_connection* peer, const transaction_inventory::record* transaction_record);
      /// records that @p peer advertised @p item to us; returns whether it had not done so before
      bool record_item_advertised_to_us(peer_connection* peer, const item_id& item);
      void forget_item_advertised_to_us(peer_connection* peer, const item_id& item);
      uint32_t inventory_advertised_to_us_size(const peer_connection* peer) const;
      // we have a higher limit for blocks than transactions so we will still fetch blocks even when transactions are throttled
      bool is_inventory_advertised_to_us_list_full_for_transactions(const peer_connection* peer) const;
      bool is_inventory_advertised_to_us_list_full(const peer_connection* peer) const;
      void expire_old_transaction_inventory();
      void release_transaction_inventory_slot(peer_connection* peer);

      void on_closing_connection_message( peer_connection* originating_peer,
                                          const closing_connection_message& closing_connection_message_received );

      void on_current_time_request_message( peer_connection* originating_peer,
                                            const current_time_request_message& current_time_request_message_received );

      void on_current_time_reply_message( peer_connection* originating_peer,
                                          const current_time_reply_message& current_time_reply_message_received );

      void forward_firewall_check_to_next_available_peer(firewall_check_state_data* firewall_check_state);

      void on_check_firewall_message(peer_connection* originating_peer,
                                     const check_firewall_message& check_firewall_message_received);

      void on_check_firewall_reply_message(peer_connection* originating_peer,
                                           const check_firewall_reply_message& check_firewall_reply_message_received);

      void on_get_current_connections_request_message(peer_connection* originating_peer,
                                                      const get_current_connections_request_message& get_current_connections_request_message_received);

      void on_get_current_connections_reply_message(peer_connection* originating_peer,
                                                    const get_current_connections_reply_message& get_current_connections_reply_message_received);

      void on_connection_closed(peer_connection* originating_peer) override;

      void refresh_cached_chain_state();
      enum class block_id_classification
      {
        known,               // we already have this exact block (main chain or fork database)
        dead_fork,           // at or below our last irreversible block and not ours: can never apply
        fetchable,           // plausible block we don't have
        implausible_future   // claims a height so far past our head it can't be a real new block
      };
      block_id_classification classify_advertised_block_id(const item_hash_t& block_id_hash);
      bool stalled_head_resync_due(peer_connection* peer);
      uint32_t highest_block_number_known_for_peer(const peer_connection& peer);
      void restart_sync_if_head_stalled();
      void record_block_delivered_by(peer_connection* peer);
      void record_requested_delivery(peer_connection* peer);
      void record_unavailable_advertised_item(peer_connection* peer, const item_id& item);
      void record_late_block_copy(peer_connection* peer, const item_id& item);
      void record_withholding_strike(peer_connection* peer, const item_id& item);
      void expand_peers_while_starved();
      bool is_taken_up_for_peer_expansion(const peer_connection& peer) const;
      void end_peer_expansion(bool new_peer_delivered);
      std::vector<peer_connection_ptr> find_silent_peers_to_replace();
      void disconnect_silent_peer(peer_connection* peer);
      void back_off_replaced_peer(peer_connection* peer, uint32_t seconds);
      void remember_node_id_of_dialed_endpoint(const fc::ip::endpoint& endpoint, const node_id_t& node_id);
      bool is_endpoint_of_connected_node(const fc::ip::endpoint& endpoint);
      void forget_expired_replaced_peer_backoffs(fc::time_point now);
      fc::optional<fc::time_point> replaced_peer_backoff_end(const node_id_t& node_id, const fc::optional<fc::ip::endpoint>& endpoint);
      void count_failed_connection_attempt(peer_connection* peer, potential_peer_last_connection_disposition disposition,
                                           uint32_t counted_at_most = std::numeric_limits<uint32_t>::max());
      bool invalid_block_disconnect_breaker_tripped();
      bool block_body_mismatch_disconnect_breaker_tripped();
      void refetch_block_from_other_peers(peer_connection* peer_that_sent_bad_copy, const item_id& block_item_id);
      void send_sync_block_to_node_delegate(const received_sync_item& item);
      uint32_t get_number_of_handle_message_calls_in_progress();
      void discard_abandoned_sync_blocks();
      void process_backlog_of_sync_blocks();
      void trigger_process_backlog_of_sync_blocks();
      void process_block_during_sync(peer_connection* originating_peer, const std::shared_ptr<full_block_type>& full_block);
      /// @p late: the peer's copy completed after its request's deadline (see record_late_block_copy())
      void process_block_during_normal_operation(peer_connection* originating_peer, const std::shared_ptr<full_block_type>& full_block,
                                                 bool late);
      /// @p arrival_completed: when the message's last bytes arrived
      void process_block_message(peer_connection* originating_peer, const message& message_to_process, const message_hash_type& message_hash,
                                 fc::time_point arrival_completed);
      void process_trx_message(peer_connection* originating_peer, const trx_message& transaction_message_to_process);
      void apply_transaction_completions();
      void apply_transaction_completion(const transaction_completion& completion);

      void process_ordinary_message(peer_connection* originating_peer, const message& message_to_process, const message_hash_type& message_hash);

      void start_synchronizing();
      void start_synchronizing_with_peer(const peer_connection_ptr& peer);

      void new_peer_just_added(const peer_connection_ptr& peer); /// called after a peer finishes handshaking, kicks off syncing

      void close();

      void accept_connection_task(const peer_connection_ptr& new_peer);
      peer_connection_ptr create_peer_connection();
      void accept_loop();
      void send_hello_message(const peer_connection_ptr& peer);
      void send_connection_rejected_message(peer_connection* peer,
                                            const fc::ip::endpoint& remote_endpoint,
                                            rejection_reason_code reason_code,
                                            const std::string& reason_string);
      void connect_to_task(const peer_connection_ptr& new_peer, const fc::ip::endpoint& remote_endpoint);
      bool is_connection_to_endpoint_in_progress(const fc::ip::endpoint& remote_endpoint);

      void move_peer_to_active_list(const peer_connection_ptr& peer);
      void move_peer_to_closing_list(const peer_connection_ptr& peer);
      void move_peer_to_terminating_list(const peer_connection_ptr& peer);

      peer_connection_ptr get_connection_to_endpoint( const fc::ip::endpoint& remote_endpoint );

      void dump_node_status();

      void delayed_peer_deletion_task();
      void schedule_peer_for_deletion(const peer_connection_ptr& peer_to_delete);

      void disconnect_from_peer(peer_connection* originating_peer,
                                const std::string& reason_for_disconnect,
                                bool caused_by_error = false,
                                const fc::oexception& additional_data = fc::oexception());
      void disconnect_from_peer_needing_pruned_blocks(peer_connection* peer, uint32_t peers_head_block_num,
                                                      uint32_t our_tail_block_num);

      // methods implementing node's public interface
      void set_node_delegate(node_delegate* del);
      void load_configuration( const fc::path& configuration_directory );
      void listen_to_p2p_network( std::function<bool()> break_callback );
      void connect_to_p2p_network();
      void add_node( const fc::ip::endpoint& ep );
      void initiate_connect_to(const peer_connection_ptr& peer);
      void connect_to_endpoint(const fc::ip::endpoint& ep);
      void listen_on_endpoint(const fc::ip::endpoint& ep , bool wait_if_not_available);
      void accept_incoming_connections(bool accept);
      void listen_on_port( uint16_t port, bool wait_if_not_available );

      fc::ip::endpoint         get_actual_listening_endpoint() const;
      std::vector<peer_status> get_connected_peers() const;
      uint32_t                 get_connection_count() const;

      void broadcast(const std::shared_ptr<full_block_type>& full_block, const message_propagation_data& propagation_data);
      void broadcast(const std::shared_ptr<full_transaction_type>& full_transaction, const message_propagation_data& propagation_data);
      void advertise_block(const std::shared_ptr<full_block_type>& full_block);
      void broadcast(const std::shared_ptr<full_block_type>& full_block);
      void broadcast(const std::shared_ptr<hive::chain::full_transaction_type>& full_transaction);

      void sync_from(const item_id& current_head_block, const std::vector<uint32_t>& hard_fork_block_numbers);
      bool is_connected() const;
      std::vector<potential_peer_record> get_potential_peers() const;
      void set_advanced_node_parameters( const fc::variant_object& params );

      node_configuration         get_advanced_node_parameters()const;
      message_propagation_data   get_transaction_propagation_data( const graphene::net::transaction_id_type& transaction_id );
      message_propagation_data   get_block_propagation_data( const graphene::net::block_id_type& block_id );

      node_id_t                  get_node_id() const;
      void                       set_allowed_peers( const std::vector<node_id_t>& allowed_peers );
      void                       clear_peer_database();
      void                       set_total_bandwidth_limit( uint32_t upload_bytes_per_second, uint32_t download_bytes_per_second );
      fc::variant_object         get_call_statistics() const;
      std::shared_ptr<full_block_type> get_full_block_by_block_id(const block_id_type& block_id) override;
      void on_messages_written(size_t message_count, size_t byte_count) override;
      std::shared_ptr<full_block_type> get_block_to_send(peer_connection* peer, const item_hash_t& requested_item) override;
      bool is_transaction_to_advertise(const item_hash_t& message_hash) override;

      fc::variant_object         network_get_info() const;
      fc::variant_object         network_get_usage_stats() const;

      bool is_hard_fork_block(uint32_t block_number) const;
      uint32_t get_next_known_hard_fork_block_number(uint32_t block_number) const;

      bool node_is_shutting_down() const;

   private:
      class activity_tracer final
      {
      public:
        activity_tracer(const char* function, node_impl& node) : _node(node), _notify(false)
        {
          if (_node.node_is_shutting_down())
            FC_THROW_EXCEPTION(fc::canceled_exception, "${function} canceled because it is shutting down", (function));

          ++_node._activeCalls;
          /// At this point potential shutdownNotification shall be sent in destructor.
          _notify = true;
        }

        ~activity_tracer()
        {
          if (_notify && --_node._activeCalls == 0 && _node.node_is_shutting_down())
            _node._shutdownNotifier->set_value();
        }
      private:
        node_impl& _node;
        bool       _notify;
      }; /// activity_tracer

#ifdef P2P_IN_DEDICATED_THREAD
# define VERIFY_CORRECT_THREAD() assert(_thread->is_current())
#else
# define VERIFY_CORRECT_THREAD() do {} while (0)
#endif

      template<typename Functor>
      auto async_task(Functor&& f, const char* desc FC_TASK_NAME_DEFAULT_ARG, fc::priority prio = fc::priority()) -> fc::future<decltype(f())>
      {
        auto wrapper = [this, desc, f]() -> decltype(f())
        {
          VERIFY_CORRECT_THREAD();
          activity_tracer aTracer(desc, *this);
          return f();
        };
        return fc::async(wrapper, desc, prio);
      }
      template<typename Functor>
      auto schedule_task(Functor&& f, const fc::time_point& t, const char* desc FC_TASK_NAME_DEFAULT_ARG, fc::priority prio = fc::priority()) -> fc::future<decltype(f())>
      {
        auto wrapper = [this, desc, f]() -> decltype(f())
        {
          VERIFY_CORRECT_THREAD();
          activity_tracer aTracer(desc, *this);
          return f();
        };

        return fc::schedule(wrapper, t, desc, prio);
      }

      void send_message_timing_to_statsd(peer_connection* originating_peer, const message& received_message, const message_hash_type& message_hash);
    }; // end class node_impl

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    void node_impl_deleter::operator()(node_impl* impl_to_delete)
    {
#ifdef P2P_IN_DEDICATED_THREAD
      std::weak_ptr<fc::thread> weak_thread;
      if (impl_to_delete)
      {
        std::shared_ptr<fc::thread> impl_thread(impl_to_delete->_thread);
        weak_thread = impl_thread;
        impl_thread->async([impl_to_delete](){ delete impl_to_delete; }, "delete node_impl").wait();
        dlog("deleting the p2p thread");
      }
      if (weak_thread.expired())
        dlog("done deleting the p2p thread");
      else
        dlog("failed to delete the p2p thread, we must be leaking a smart pointer somewhere");
#else // P2P_IN_DEDICATED_THREAD
      delete impl_to_delete;
#endif // P2P_IN_DEDICATED_THREAD
    }

    node_impl::node_impl(const std::string& user_agent, appbase::application& app, hive::chain::blockchain_worker_thread_pool& thread_pool ) :
#ifdef P2P_IN_DEDICATED_THREAD
      _thread(std::make_shared<fc::thread>("p2p")),
#endif // P2P_IN_DEDICATED_THREAD
      _delegate(nullptr),
      _is_firewalled(firewalled_state::unknown),
      _potential_peer_database_updated(false),
      _sync_items_to_fetch_updated(false),
      _suspend_fetching_sync_blocks(false),
      _items_to_fetch_sequence_counter(0),
      _recent_block_interval_in_seconds(HIVE_BLOCK_INTERVAL),
      _user_agent_string(user_agent),
      _most_recent_blocks_accepted(GRAPHENE_NET_DEFAULT_MAX_CONNECTIONS),
      _total_number_of_unfetched_items(0),
      _rate_limiter(0, 0),
      _last_reported_number_of_connections(0),
      _average_network_read_speed_seconds(60),
      _average_network_write_speed_seconds(60),
      _average_network_read_speed_minutes(60),
      _average_network_write_speed_minutes(60),
      _average_network_read_speed_hours(72),
      _average_network_write_speed_hours(72),
      _average_network_usage_second_counter(0),
      _average_network_usage_minute_counter(0),
      _node_is_shutting_down(false),
      _activeCalls(0),
      theApp( app ),
      _thread_pool( thread_pool )
    {
      _rate_limiter.set_actual_rate_time_constant(fc::seconds(2));
      fc::rand_bytes(&_node_id.data[0], (int)_node_id.size());

      _shutdownNotifier = fc::promise<void>::create("Node shutdown notifier");

      // called on the writer thread, and only until close() closes the queue, so this node is still alive
      _transaction_completions = std::make_shared<completion_queue<transaction_completion>>([this]() {
        _thread->async([this, completions = _transaction_completions]() {
          // closing the queue and destroying this node happen on this thread, so an open queue means a live node
          if (!completions->is_closed())
            apply_transaction_completions();
        }, "apply_transaction_completions");
      });
    }

    node_impl::~node_impl()
    {
      VERIFY_CORRECT_THREAD();
      ilog( "cleaning up node" );
      _node_is_shutting_down = true;
      // a message handler waiting for room to submit a transaction would keep _activeCalls up
      _transactions_in_flight.close();

      if(_activeCalls.load() != 0)
      {
        ilog("Waiting for node become inactive (on_message/on_connection_closed/get_message_for_item)");
        _shutdownNotifier->wait();
        ilog("Node became inactive");
      }

      try
      {
        ilog( "close" );
        close();
      }
      catch ( const fc::exception& e )
      {
        wlog( "unexpected exception on close ${e}", ("e", e) );
      }
      ilog( "done" );
    }

    void node_impl::save_node_configuration()
    {
      VERIFY_CORRECT_THREAD();
      if( fc::exists(_node_configuration_directory ) )
      {
        fc::path configuration_file_name( _node_configuration_directory / NODE_CONFIGURATION_FILENAME );
        try
        {
          fc::json::save_to_file( _node_configuration, configuration_file_name );
        }
        catch (const fc::canceled_exception&)
        {
          throw;
        }
        catch ( const fc::exception& except )
        {
          elog( "error writing node configuration to file ${filename}: ${error}",
               ( "filename", configuration_file_name )("error", except.to_detail_string() ) );
        }
      }
    }

    void node_impl::save_active_peers_to_peer_database()
    {
      for (const peer_connection_ptr& active_peer : _active_connections)
      {
        fc::optional<fc::ip::endpoint> endpoint_for_db = active_peer->get_endpoint_for_db();
        if (endpoint_for_db)
        {
          fc::optional<potential_peer_record> updated_peer_record = _potential_peer_db.lookup_entry_for_endpoint(*endpoint_for_db);
          if (updated_peer_record)
          {
            updated_peer_record->last_seen_time = fc::time_point::now();
            _potential_peer_db.update_entry(*updated_peer_record);
          }
        }
      }
    }

    // Tests whether this host has a route to the given address.  A UDP connect() sends no
    // packets; it only performs a kernel route lookup and source-address selection, so it
    // fails exactly when the host cannot reach that destination: no interface of its family,
    // or neither a default route, a connected subnet nor any other route covering it, or an
    // explicit unreachable/blackhole route.  The lookup is done for the real destination
    // rather than a fixed placeholder such as TEST-NET-1: a host whose only routes are
    // connected subnets (a LAN-only testnet, a docker --internal network) can dial its
    // on-link peers but not a placeholder, and bogon filters commonly blackhole exactly the
    // placeholder prefixes while leaving real destinations reachable.  Nothing is sent.
    static bool probe_route_to_address(const fc::ip::address& addr)
    {
      try
      {
        fc::ip::endpoint probe_target(addr, 53); // the port plays no part in the route lookup
        fc::udp_socket probe_socket;
        probe_socket.open_for_endpoint(probe_target);
        probe_socket.connect(probe_target);
        return true;
      }
      catch (...)
      {
        return false;
      }
    }

    bool node_impl::can_route_to(const fc::ip::address& addr)
    {
      VERIFY_CORRECT_THREAD();
      // loopback is reachable through the loopback interface even when the host has no
      // other route for the family (e.g. ::1 on a host without global IPv6)
      if (addr.is_loopback_address())
        return true;

      const fc::time_point now = fc::time_point::now();
      auto cached = _route_probe_cache.find(addr);
      if (cached != _route_probe_cache.end() && now < cached->second.expiration)
        return cached->second.routable;

      // the cache is keyed by candidate addresses, so it can grow no larger than the peer
      // database plus the endpoints we are asked to firewall-check, but drop expired entries
      // once it gets large so churn in the peer database doesn't accumulate forever
      if (_route_probe_cache.size() >= 2 * GRAPHENE_NET_MAX_PEERDB_SIZE)
      {
        for (auto iter = _route_probe_cache.begin(); iter != _route_probe_cache.end(); )
          if (now >= iter->second.expiration)
            iter = _route_probe_cache.erase(iter);
          else
            ++iter;
        cached = _route_probe_cache.end();
      }

      const bool routable = probe_route_to_address(addr);
      const bool previously_routable = cached == _route_probe_cache.end() ? true : cached->second.routable;
      if (routable != previously_routable)
        dlog("route to ${addr} from this host: ${routable}", ("addr", addr)("routable", routable));
      _route_probe_cache[addr] = route_probe_result{routable, now + fc::seconds(GRAPHENE_NET_ROUTE_PROBE_CACHE_SEC)};
      return routable;
    }

    static bool is_wildcard_address(const fc::ip::address& addr)
    {
      static const fc::ip::address ipv4_any;                       // 0.0.0.0
      static const fc::ip::address ipv6_any{ fc::string("::") };   // ::
      return addr == ipv4_any || addr == ipv6_any;
    }

    static std::vector<fc::ip::address> get_interface_addresses()
    {
      std::vector<fc::ip::address> addresses;
      ifaddrs* interfaces = nullptr;
      if (getifaddrs(&interfaces) != 0)
      {
        wlog("unable to list this host's network interface addresses: ${error}", ("error", std::strerror(errno)));
        return addresses;
      }
      for (const ifaddrs* iface = interfaces; iface; iface = iface->ifa_next)
      {
        if (!iface->ifa_addr)
          continue;
        if (iface->ifa_addr->sa_family == AF_INET)
        {
          const sockaddr_in* ipv4 = reinterpret_cast<const sockaddr_in*>(iface->ifa_addr);
          addresses.emplace_back(ntohl(ipv4->sin_addr.s_addr));
        }
        else if (iface->ifa_addr->sa_family == AF_INET6)
        {
          const sockaddr_in6* ipv6 = reinterpret_cast<const sockaddr_in6*>(iface->ifa_addr);
          std::array<uint8_t, 16> bytes;
          std::memcpy(bytes.data(), ipv6->sin6_addr.s6_addr, bytes.size());
          addresses.emplace_back(fc::ip::ipv6_address(bytes));
        }
      }
      freeifaddrs(interfaces);
      return addresses;
    }

    // Peers pass our own address around in their address messages like any other, so without
    // remembering it we would store it and redial ourselves after each connected_to_self rejection
    void node_impl::remember_own_endpoint(const fc::ip::endpoint& own_endpoint)
    {
      VERIFY_CORRECT_THREAD();
      if (_potential_peer_db.add_own_endpoint(own_endpoint))
        ilog("${endpoint} is an address of this node, it will not be dialed or stored in the peer database",
             ("endpoint", own_endpoint));
    }

    void node_impl::remember_own_listen_endpoints()
    {
      VERIFY_CORRECT_THREAD();
      if (_actual_listening_endpoint.port() == 0)
        return;
      if (!is_wildcard_address(_actual_listening_endpoint.get_address()))
      {
        remember_own_endpoint(_actual_listening_endpoint);
        return;
      }
      // a wildcard listener (which is dual-stack, see listen_to_p2p_network) accepts connections on
      // every interface address; addresses that appear later are learned from connections instead
      for (const fc::ip::address& interface_address : get_interface_addresses())
        remember_own_endpoint(fc::ip::endpoint(interface_address, _actual_listening_endpoint.port()));
    }

    void node_impl::remember_own_endpoint_of_connection(peer_connection* peer)
    {
      VERIFY_CORRECT_THREAD();
      // the local address of a connection in either direction is one of ours: for an inbound one it
      // is exactly the address the peer dialed, and so the one it will advertise for us
      if (_actual_listening_endpoint.port() != 0)
      {
        const fc::optional<fc::ip::endpoint> local_endpoint = peer->get_local_endpoint();
        if (local_endpoint)
          remember_own_endpoint(fc::ip::endpoint(local_endpoint->get_address(), _actual_listening_endpoint.port()));
      }
    }

    void node_impl::p2p_network_connect_loop()
    {
      while (!_p2p_network_connect_loop_done.canceled() && !node_is_shutting_down())
      {
        _connect_loop_trigger.begin_iteration();
        try
        {
          dlog("Starting an iteration of p2p_network_connect_loop().");
          display_current_connections();

          // add-once peers bypass our checks on the maximum/desired number of connections (but they will still be counted against the totals once they're connected)
          if (!_add_once_node_list.empty())
          {
            std::list<potential_peer_record> add_once_node_list;
            add_once_node_list.swap(_add_once_node_list);
            dlog("Processing \"add once\" node list containing ${count} peers:", ("count", add_once_node_list.size()));
            for (const potential_peer_record& add_once_peer : add_once_node_list)
            {
              dlog("    ${peer}", ("peer", add_once_peer.endpoint));
            }
            for (const potential_peer_record& add_once_peer : add_once_node_list)
            {
              // see if we have an existing connection to that peer.  If we do, disconnect them and
              // then try to connect the next time through the loop
              peer_connection_ptr existing_connection_ptr = get_connection_to_endpoint( add_once_peer.endpoint );
              if(!existing_connection_ptr)
                connect_to_endpoint(add_once_peer.endpoint);
            }
            dlog("Done processing \"add once\" node list");
          }

          while (is_wanting_new_connections())
          {
            bool initiated_connection_this_pass = false;
            _potential_peer_database_updated = false;

            // Dial IPv6 candidates before IPv4 ones, past each family's reserved share
            // (select_connection_candidates).  A node reachable over both families
            // appears as two unrelated entries here; whichever connection finishes its
            // handshake first is kept and the other is dropped as a duplicate, so dialing
            // order biases dual-stack peers toward IPv6 without affecting single-family peers.
            uint32_t unroutable_ipv4_candidates = 0;
            uint32_t unroutable_ipv6_candidates = 0;
            std::vector<connection_candidate> candidates = get_connection_candidates(unroutable_ipv4_candidates, unroutable_ipv6_candidates);
            const std::vector<fc::ip::address> held_addresses = get_connection_addresses(false);
            if (_peer_expansion_target != 0)
            {
              // a peer expansion looks for peers that can serve us where the ones we hold can't: hosts we hold no
              // connection to first
              std::stable_partition(candidates.begin(), candidates.end(), [&held_addresses](const connection_candidate& candidate) {
                const fc::ip::address host = connection_host_of(candidate.endpoint.get_address());
                return std::none_of(held_addresses.begin(), held_addresses.end(),
                                    [&host](const fc::ip::address& held) { return connection_host_of(held) == host; });
              });
            }
            const std::vector<fc::ip::endpoint> selected = select_connection_candidates(
              candidates, held_addresses, get_connection_selection_limits(get_desired_number_of_connections() - get_number_of_connections_holding_slots()));
            if (is_host_limit_relaxed())
            {
              // a candidate that is not exempt and on a host at the limit was picked past it
              bool relaxed = false;
              std::vector<fc::ip::address> addresses = held_addresses;
              for (const fc::ip::endpoint& endpoint : selected)
              {
                const bool exempt = std::any_of(candidates.begin(), candidates.end(), [&endpoint](const connection_candidate& candidate) {
                  return candidate.endpoint == endpoint && candidate.exempt_from_host_limit;
                });
                relaxed = relaxed || (!exempt && exceeds_connections_per_host(endpoint.get_address(), addresses,
                                                                              _node_configuration.maximum_connections_per_host));
                addresses.push_back(endpoint.get_address());
              }
              if (relaxed)
                log_host_limit_relaxed(addresses);
            }
            for (const fc::ip::endpoint& endpoint : selected)
            {
              if (!is_wanting_new_connections())
                break;
              connect_to_endpoint(endpoint);
              initiated_connection_this_pass = true;
            }

            // say so (once per change, not per pass) when candidates are being left alone for
            // lack of a route: with only unroutable candidates a node quietly makes no progress,
            // and the per-candidate skip above is only logged at debug level
            if (unroutable_ipv4_candidates != _unroutable_ipv4_candidates_last_pass ||
                unroutable_ipv6_candidates != _unroutable_ipv6_candidates_last_pass)
            {
              if (unroutable_ipv4_candidates || unroutable_ipv6_candidates)
                ilog("not dialing ${ipv4} IPv4 and ${ipv6} IPv6 peer candidates: this host has no route to them (re-checked every ${secs}s)",
                     ("ipv4", unroutable_ipv4_candidates)("ipv6", unroutable_ipv6_candidates)("secs", GRAPHENE_NET_ROUTE_PROBE_CACHE_SEC));
              else
                ilog("all peer candidates are routable from this host again");
              _unroutable_ipv4_candidates_last_pass = unroutable_ipv4_candidates;
              _unroutable_ipv6_candidates_last_pass = unroutable_ipv6_candidates;
            }

            if (!initiated_connection_this_pass && !_potential_peer_database_updated)
              break;
          } /// while (is_wanting_new_connections())

          dial_new_host_for_connection_over_host_limit();

          display_current_connections();

          if(node_is_shutting_down())
          {
            ilog("Breaking p2p_network_connect_loop loop because node is shutting down");
            break;
          }
        }
        catch (const fc::canceled_exception&)
        {
          throw;
        }
        catch (const fc::exception& e)
        {
          edump((e));
        }
        // either we have connected to enough nodes, or we don't have any good candidates to connect to
        // right now: wait for something to change, or for retry delays to expire
        _connect_loop_trigger.wait(fc::milliseconds(GRAPHENE_NET_CONNECT_LOOP_MINIMUM_INTERVAL_MS),
                                   fc::time_point::now() + fc::seconds(GRAPHENE_NET_CONNECT_LOOP_MAXIMUM_WAIT_SEC));
      }// while(!canceled)
    }

    std::vector<connection_candidate> node_impl::get_connection_candidates(uint32_t& unroutable_ipv4_candidates,
                                                                           uint32_t& unroutable_ipv6_candidates)
    {
      VERIFY_CORRECT_THREAD();
      std::vector<connection_candidate> candidates;
      for (peer_database::iterator iter = _potential_peer_db.begin(); iter != _potential_peer_db.end(); ++iter)
      {
        dlog("potential peer to connect to: ${iter}",("iter",*iter));
        // skip peers we have no route to, but keep their entries: we still store and
        // advertise them to peers that may be able to use them, and the route may appear
        // later (the probe result is re-checked every GRAPHENE_NET_ROUTE_PROBE_CACHE_SEC)
        if (!can_route_to(iter->endpoint.get_address()))
        {
          dlog("skipping ${peer}: this host has no route to its address", ("peer", iter->endpoint));
          (iter->endpoint.get_address().is_ipv6() ? unroutable_ipv6_candidates : unroutable_ipv4_candidates)++;
          continue;
        }
        fc::microseconds delay_until_retry = fc::seconds((iter->number_of_failed_connection_attempts + 1) * _node_configuration.peer_connection_retry_timeout);
        if (_active_connections.size() == 0) //if no active connections, we should try more often to find a peer, so limit the amount of delay time
        {
          delay_until_retry = std::min(delay_until_retry,fc::seconds(180));
        }

        if (!is_connection_to_endpoint_in_progress(iter->endpoint) &&
            is_due_for_connection_attempt(*iter, delay_until_retry, fc::time_point::now()) &&
            !replaced_peer_backoff_end(node_id_t(), iter->endpoint) &&
            !is_endpoint_of_connected_node(iter->endpoint))
          candidates.push_back(connection_candidate{iter->endpoint,
                                                    _host_limit_exempt_endpoints.count(iter->endpoint) != 0 ||
                                                    is_in_any_subnet(iter->endpoint.get_address(), _host_limit_exempt_subnets)});
      }
      return candidates;
    }

    connection_selection_limits node_impl::get_connection_selection_limits(uint32_t free_slots) const
    {
      connection_selection_limits limits;
      limits.free_slots = free_slots;
      limits.desired_connections = _node_configuration.desired_number_of_connections;
      limits.maximum_connections_per_host = _node_configuration.maximum_connections_per_host;
      limits.reserved_address_family_percent = _node_configuration.reserved_address_family_percent;
      limits.relax_connections_per_host = is_host_limit_relaxed();
      return limits;
    }

    // A connection let past the defaulted per-host limit gives way to a host under it.  While we are full, one candidate
    // under the limit is dialed past the desired number (up to the maximum); once it is active,
    // give_way_to_connection_under_host_limit() closes a connection over the limit.  Dialing first means a
    // candidate that fails to connect costs nothing, and the node never drops below the desired number for it.  Only
    // active connections count toward the desired number here, so a handshaking one (a peer that never sends its hello)
    // does not hold the dial back; one dial of ours at a time is handshaking, so the extra stays one connection
    void node_impl::dial_new_host_for_connection_over_host_limit()
    {
      VERIFY_CORRECT_THREAD();
      if (node_is_shutting_down() || !is_host_limit_relaxed() ||
          _active_connections.size() < get_desired_number_of_connections() ||
          get_number_of_connections_holding_slots() >= _node_configuration.maximum_number_of_connections ||
          std::any_of(_handshaking_connections.begin(), _handshaking_connections.end(), [](const peer_connection_ptr& peer) {
            // a connection we dial is outbound once its task starts, an accepted one inbound
            return peer->direction != peer_connection_direction::inbound;
          }) ||
          !find_active_connection_over_host_limit(nullptr))
        return;
      uint32_t unroutable_ipv4_candidates = 0;
      uint32_t unroutable_ipv6_candidates = 0;
      connection_selection_limits limits = get_connection_selection_limits(1);
      limits.relax_connections_per_host = false;
      for (const fc::ip::endpoint& endpoint : select_connection_candidates(get_connection_candidates(unroutable_ipv4_candidates, unroutable_ipv6_candidates),
                                                                           get_connection_addresses(false), limits))
      {
        ilog("dialing ${peer}, on a host under the per-host limit, to take the slot of a connection over it", ("peer", endpoint));
        connect_to_endpoint(endpoint);
      }
    }

    void node_impl::trigger_p2p_network_connect_loop()
    {
      VERIFY_CORRECT_THREAD();
      _potential_peer_database_updated = true; // if we are looking for peers, this says to retry the loop
      _connect_loop_trigger.trigger(false);
    }

    bool node_impl::have_already_received_sync_item(const item_hash_t& item_hash)
    {
      VERIFY_CORRECT_THREAD();
      return (_already_received_sync_item_ids.find(item_hash) != _already_received_sync_item_ids.end());
    }

    void node_impl::request_sync_item_from_peer(const peer_connection_ptr& peer, const item_hash_t& item_to_request)
    {
      VERIFY_CORRECT_THREAD();
      dlog("requesting item ${item_hash} from peer ${endpoint}", ("item_hash", item_to_request)("endpoint", peer->get_remote_endpoint()));
      item_id item_id_to_request(graphene::net::block_message_type, item_to_request);
      _active_sync_requests.insert(active_sync_requests_map::value_type(item_to_request, fc::time_point::now()));
      peer->last_sync_item_received_time = fc::time_point::now();
      peer->sync_items_requested_from_peer.insert(item_to_request);
      peer->send_message(fetch_items_message(item_id_to_request.item_type, std::vector<item_hash_t>{item_id_to_request.item_hash}));
    }

    void node_impl::request_sync_items_from_peer(const peer_connection_ptr& peer, const std::vector<item_hash_t>& items_to_request)
    {
      VERIFY_CORRECT_THREAD();
      dlog("requesting ${item_count} item(s) ${items_to_request} from peer ${endpoint}",
           ("item_count", items_to_request.size())("items_to_request", items_to_request)("endpoint", peer->get_remote_endpoint()));
      for (const item_hash_t& item_to_request : items_to_request)
      {
        _active_sync_requests.insert(active_sync_requests_map::value_type(item_to_request, fc::time_point::now()));
        peer->last_sync_item_received_time = fc::time_point::now();
        peer->sync_items_requested_from_peer.insert(item_to_request);
      }
      peer->send_message(fetch_items_message(graphene::net::block_message_type, items_to_request));
    }

    void node_impl::update_last_requested_block_number_for_peers_on_this_fork(uint32_t last_requested_block_number, const item_hash_t& last_requested_block_id)
    {
      for (const peer_connection_ptr& peer : _active_connections)
      {
        //if peer is on fork with requested_block_id (check if the two peers share the same hash at that block number)
        uint32_t id_index = last_requested_block_number - peer->first_id_block_number;
        if (id_index < peer->ids_of_items_to_get.size() && peer->ids_of_items_to_get[id_index] == last_requested_block_id)
        {
          if (last_requested_block_number > peer->last_requested_block_number_for_peers_on_this_fork)
            peer->last_requested_block_number_for_peers_on_this_fork = last_requested_block_number;
          else
            ddump((last_requested_block_number)(peer->last_requested_block_number_for_peers_on_this_fork));
        }
      }
    }

    void node_impl::fetch_sync_items_loop()
    {
      while (!_fetch_sync_items_loop_done.canceled())
      {
        _sync_items_to_fetch_updated = false;
        dlog("beginning another iteration of the sync items loop");

        uint32_t idle_peer_count = 0;
        if (!_suspend_fetching_sync_blocks)
        {
          std::map<peer_connection_ptr, std::vector<item_hash_t>> sync_item_requests_to_send;
          {
            ASSERT_TASK_NOT_PREEMPTED();
            std::set<item_hash_t> sync_items_to_request;

            // as in fetch_items_loop() (see choose_block_source()), a peer kept despite sending us a block whose
            // body didn't match its header is asked for a block only if no other peer we sync from still has it
            // to request: those peers go first, and a deprioritized peer leaves them the blocks they offer
            std::vector<peer_connection_ptr> peers_in_request_order;
            std::vector<peer_connection_ptr> deprioritized_peers;
            for (const peer_connection_ptr& peer : _active_connections)
              (peer->is_block_fetching_deprioritized() ? deprioritized_peers : peers_in_request_order).push_back(peer);
            const size_t first_deprioritized_peer = peers_in_request_order.size();
            peers_in_request_order.insert(peers_in_request_order.end(), deprioritized_peers.begin(), deprioritized_peers.end());
            std::unordered_set<item_hash_t, p2p_item_hash> sync_items_left_to_preferred_peers;
            if (!deprioritized_peers.empty())
              for (size_t peer_index = 0; peer_index < first_deprioritized_peer; ++peer_index)
              {
                const peer_connection_ptr& peer = peers_in_request_order[peer_index];
                if (peer->inhibit_fetching_sync_blocks || !peer->we_need_sync_items_from_peer || peer->ids_of_items_to_get.empty())
                  continue;
                // blocks before the peer's search position on its fork are ones it will not request again
                const uint32_t first_to_search = std::max(peer->last_requested_block_number_for_peers_on_this_fork + 1, peer->first_id_block_number) -
                                                 peer->first_id_block_number;
                for (size_t i = first_to_search; i < peer->ids_of_items_to_get.size(); ++i)
                  sync_items_left_to_preferred_peers.insert(peer->ids_of_items_to_get[i]);
              }

            // for each idle peer that we're syncing with
            for (size_t peer_index = 0; peer_index < peers_in_request_order.size(); ++peer_index)
            {
              const peer_connection_ptr& peer = peers_in_request_order[peer_index];
              const bool peer_is_deprioritized = peer_index >= first_deprioritized_peer;
              if (peer->inhibit_fetching_sync_blocks)
                dlog("Skipping peer ${peer} because we've inhibited fetching sync blocks from them.  idle: ${idle}, we_need_sync_items_from_peer: ${we_need_sync_items_from_peer}",
                     ("peer", peer->get_remote_endpoint())("idle", peer->idle())("we_need_sync_items_from_peer", peer->we_need_sync_items_from_peer));
              else if (peer->idle())
              {
                dlog("peer ${peer} is idle", ("peer", peer->get_remote_endpoint()));
                ++idle_peer_count;
                if (peer->we_need_sync_items_from_peer && !peer->ids_of_items_to_get.empty())
                {
                  assert(peer->first_id_block_number);
                  if (peer->last_requested_block_number_for_peers_on_this_fork < peer->first_id_block_number - 1)
                    peer->last_requested_block_number_for_peers_on_this_fork = peer->first_id_block_number - 1;
                  uint32_t first_to_get = peer->last_requested_block_number_for_peers_on_this_fork - peer->first_id_block_number + 1;
                  if (first_to_get < peer->ids_of_items_to_get.size())
                  {
                    // loop through the items peer has that we don't yet have on our blockchain
                    unsigned i = first_to_get;
                    bool left_to_preferred_peer = false;
                    for (; i < peer->ids_of_items_to_get.size(); ++i)
                    {
                      const item_hash_t& item_to_potentially_request = peer->ids_of_items_to_get[i];
                      // if we don't already have this item in our temporary storage and we haven't requested from another syncing peer
                      if(_active_sync_requests.find(item_to_potentially_request) == _active_sync_requests.end() && // we've requested it in a previous iteration and we're still waiting for it to arrive
                         !have_already_received_sync_item(item_to_potentially_request) && // already received it, but not yet removed from our list of peer items to get
                         sync_items_to_request.find(item_to_potentially_request) == sync_items_to_request.end()) // we have already decided to request it from another peer during this iteration
                      {
                        if (peer_is_deprioritized && sync_items_left_to_preferred_peers.count(item_to_potentially_request))
                        {
                          left_to_preferred_peer = true;
                          break;
                        }
                        // then schedule a request from this peer
                        sync_item_requests_to_send[peer].push_back(item_to_potentially_request);
                        sync_items_to_request.insert(item_to_potentially_request);
                        if (sync_item_requests_to_send[peer].size() >= std::min<size_t>(_node_configuration.maximum_blocks_per_peer_during_syncing,
                                                                                         GRAPHENE_NET_MAX_BLOCKS_PER_FETCH_REQUEST))
                          break;
                      }
                    } //for each item to get
                    // the search position is shared by every peer on this fork: it must stay before a block left to
                    // a preferred peer, or that peer would never request it
                    if (!(left_to_preferred_peer && i == first_to_get))
                    {
                      if (left_to_preferred_peer || i == peer->ids_of_items_to_get.size()) //if we didn't find any items, we only searched one less than i
                        --i;
                      uint32_t last_searched_block_number = peer->first_id_block_number + i;
                      const item_hash_t& last_searched_item_id = peer->ids_of_items_to_get[i];
                      update_last_requested_block_number_for_peers_on_this_fork(last_searched_block_number, last_searched_item_id);
                      dlog("searched through ${count} ids from ${total_ids} available ids to find ${n} items to request", ("count", i - first_to_get + 1)("total_ids", peer->ids_of_items_to_get.size())("n", sync_item_requests_to_send[peer].size()));
                    }

                  } //if this peer has items we aren't currently asking for or already received
                } //if we need sync items from peer
              } //if peer idle
            } //for each active peer
          }// end non-preemptable section

          // make all the requests we scheduled in the loop above
          for( const auto& sync_item_request : sync_item_requests_to_send )
            request_sync_items_from_peer( sync_item_request.first, sync_item_request.second );
          sync_item_requests_to_send.clear();
        }
        else
          dlog("fetch_sync_items_loop is suspended pending backlog processing");

        if( !_sync_items_to_fetch_updated )
        {
          dlog( "idle_peer_count=${idle_peer_count}, can't request more sync items now, going to sleep",(idle_peer_count) );
          _retrigger_fetch_sync_items_loop_promise = fc::promise<void>::create("graphene::net::retrigger_fetch_sync_items_loop");
          _retrigger_fetch_sync_items_loop_promise->wait();
          _retrigger_fetch_sync_items_loop_promise.reset();
        }
      } // while( !canceled )
    }

    void node_impl::trigger_fetch_sync_items_loop()
    {
      VERIFY_CORRECT_THREAD();
      dlog( "Triggering fetch sync items loop now" );
      //we set this so that we will do another iteration of fetch_sync_items even if we are suspended in the middle of a previous run
      _sync_items_to_fetch_updated = true;
      if( _retrigger_fetch_sync_items_loop_promise )
        _retrigger_fetch_sync_items_loop_promise->set_value();
    }

    fc::time_point node_impl::oldest_item_timestamp_to_fetch() const
    {
      return fc::time_point::now() - fc::seconds(_recent_block_interval_in_seconds * GRAPHENE_NET_MESSAGE_CACHE_DURATION_IN_BLOCKS);
    }

    size_t node_impl::remove_expired_items_to_fetch()
    {
      // these have probably already fallen out of our peers' caches, we'll just ignore them.
      // this can happen during flooding, and the _items_to_fetch could otherwise get clogged
      // with a bunch of items that we'll never be able to request from any peer
      auto& items_by_timestamp = _items_to_fetch.get<item_timestamp_index>();
      const auto first_to_keep = items_by_timestamp.lower_bound(oldest_item_timestamp_to_fetch());
      size_t expired_items = 0;
      for (auto iter = items_by_timestamp.begin(); iter != first_to_keep; ++expired_items)
      {
        dlog("Unable to fetch item ${item} before its likely expiration time, removing it from our list of items to fetch", ("item", iter->item));
        iter = items_by_timestamp.erase(iter);
      }
      return expired_items;
    }

    // expired items are removed from _items_to_fetch before the candidates are judged, so they are dropped here
    fetch_candidate_verdict node_impl::judge_fetch_candidate(const peer_connection_ptr& peer, const item_id& item,
                                                             const std::unordered_map<const peer_connection*, bool>& pass_peers)
    {
      auto& items_by_id = _items_to_fetch.get<item_id_index>();
      auto iter = items_by_id.find(item);
      if (iter == items_by_id.end())
        return fetch_candidate_verdict::drop; // we got it, requested it from another peer, or it expired
      if (!peer_advertised_item(peer.get(), item))
        return fetch_candidate_verdict::drop; // the peer said it doesn't have it, sent us a bad copy, or advertised it too long ago
      const auto listed = pass_peers.find(peer.get());
      const bool idle = listed != pass_peers.end() && listed->second;
      if (item.item_type == graphene::net::block_message_type &&
          (peer->is_block_fetching_deprioritized() || !idle || !peer->delivered_block))
      {
        // a peer kept despite sending us a block whose body didn't match its header, or that kept us waiting
        // for a block or said it didn't have one it advertised, is asked only if no other peer advertised the
        // block, so it doesn't delay the genuine one.  A peer that never delivered us a block is asked only if no
        // peer that did can be: it may advertise blocks only to be asked for them.  A peer with transactions
        // outstanding would send the block only after their replies, so it is asked only if no idle peer (idle
        // when the pass began, and advertising transactions to us, see peer_connection::idle_block_source()) as
        // proven as it can be.  Other peers ask for it from their own candidates when
        // they can take a request: one listed in this pass that has it among them takes it in its turn, the
        // others only count as sources
        std::vector<block_source_candidate> candidates{{peer->is_block_fetching_deprioritized(), true, idle, peer->delivered_block}};
        for (const peer_connection_ptr& other_peer : _active_connections)
          if (other_peer != peer && peer_advertised_item(other_peer.get(), item))
          {
            const auto other_listed = pass_peers.find(other_peer.get());
            const bool taker = other_listed != pass_peers.end() && other_peer->fetch_candidates.has_block(item);
            candidates.push_back({other_peer->is_block_fetching_deprioritized(), taker, taker && other_listed->second,
                                  other_peer->delivered_block});
          }
        if (choose_block_source(candidates) != std::optional<size_t>(0))
          return fetch_candidate_verdict::keep; // in case the other sources disconnect without delivering it
      }
      items_by_id.erase(iter);
      return fetch_candidate_verdict::take;
    }

    // A pass of the fetch items loop:
    //  1. drops the expired items to fetch and asks other advertisers for overdue blocks (rerequest_overdue_blocks())
    //  2. sets the transaction fetch gate for the pass: the pending pool, the bytes on their way and the minimum
    //     (transaction_fetch_gate, fetch_minimum_schedule)
    //  3. lists the peers that can take a request and each one's transaction window (list_peers_for_fetch_pass(),
    //     transaction_fetch_window())
    //  4. has the listed peers take turns, one item each, judging their candidates (take_fetch_turns(),
    //     fetch_turn_order, judge_fetch_candidate())
    //  5. records the pass, sends the requests (send_fetch_requests(), group_fetch_requests()) and waits for the
    //     next one (next_fetch_pass())
    // The items to request are chosen first and sent afterwards, so the pass doesn't yield while it iterates.
    void node_impl::fetch_items_loop()
    {
      while (!_fetch_item_loop_done.canceled())
      {
        _fetch_items_loop_trigger.begin_iteration();
        const fc::time_point iteration_start = fc::time_point::now();
        dlog("beginning an iteration of fetch items (${count} items to fetch)",
             ("count", _items_to_fetch.size()));

        const size_t expired_items = remove_expired_items_to_fetch();
        fc::time_point next_peer_unblocked_time = fc::time_point::maximum();
        rerequest_overdue_blocks(iteration_start, next_peer_unblocked_time);
        // while the pending pool is over the threshold the pass asks for no more than the gate's minimum of
        // transactions, if one is due, blocks as always; below it for no more than fits under it with what is
        // already on its way
        transaction_fetch_gate gate(_delegate->get_pending_transaction_pool_size(), transaction_bytes_on_their_way(),
                                    _fetch_minimum_schedule.minimum_for_pass(iteration_start));

        fetch_pass pass = list_peers_for_fetch_pass(iteration_start, gate, next_peer_unblocked_time);
        take_fetch_turns(pass, gate, next_peer_unblocked_time);

        const fc::time_point next_minimum_due = _fetch_minimum_schedule.record_pass(gate, iteration_start);
        ++_statistics.fetch.wakes;
        if (gate.held_back())
          ++_statistics.fetch.transactions_gated;
        if (gate.bounded())
          ++_statistics.fetch.transactions_bounded;
        _statistics.fetch.items_expired += expired_items;
        // only now is it known whether the headroom withheld transactions in this pass
        if (const std::optional<size_t> expired_to_report = _fetch_expiry_warnings.record(expired_items, gate.closed() || gate.bounded(), iteration_start))
          wlog("Removed ${expired_items} items from the list to fetch due to expiration time.", ("expired_items", *expired_to_report));
        _statistics.fetch.scheduling_time += fc::time_point::now() - iteration_start;

        send_fetch_requests(pass);

        // next_peer_unblocked_time resumes the loop when a peer's transaction fetching is no longer inhibited.
        // Nothing tells the loop when the pending pool drains (by inclusion in blocks, expiry or drops at
        // max-mempool-size), so while live candidates wait for it the gate is checked again at an interval; once
        // they have all expired (trimmed in list_peers_for_fetch_pass()) the loop waits for a trigger.  API submissions may keep the pool
        // over the threshold for as long as they last (see transaction_fetch_gate_threshold()).  Replies that free
        // the headroom need not open a peer's window, so they may not wake the loop either
        const fetch_loop_wait wait = next_fetch_pass(gate, iteration_start,
                                                     fc::microseconds(_node_configuration.fetch_items_minimum_interval_microseconds),
                                                     next_peer_unblocked_time, next_minimum_due);
        _fetch_items_loop_trigger.wait(wait.minimum_interval, wait.wake_time);
      } // while (!canceled)
    }

    // Peers with no block or sync request outstanding are asked, each for items from its own fetch candidates: blocks
    // one request at a time, transactions while the peer's window has room, so the next batch may go out before
    // earlier replies have arrived.  A peer is listed if it has blocks among its candidates or the gate lets it be
    // asked for transactions
    node_impl::fetch_pass node_impl::list_peers_for_fetch_pass(fc::time_point pass_start, transaction_fetch_gate& gate,
                                                               fc::time_point& next_peer_unblocked_time)
    {
      // candidates as old as the expired items are dropped too, so that none the gate holds back outlives them
      const fc::time_point oldest_candidate_to_keep = oldest_item_timestamp_to_fetch();
      const uint32_t in_flight = _transactions_in_flight.in_flight();
      fetch_pass pass;
      for (const peer_connection_ptr& peer : _active_connections)
      {
        _fetch_turn_order.visit(peer.get(), pass.peers.size());
        peer->fetch_candidates.trim(oldest_candidate_to_keep);
        if (!peer->can_take_item_request() || peer->fetch_candidates.size() == 0)
          continue;
        const bool transaction_fetching_inhibited = peer->is_transaction_fetching_inhibited();
        // a busy peer is unblocked by its next delivery, which runs the loop again
        if (transaction_fetching_inhibited && peer->fetch_candidates.has_transactions() &&
            peer->transaction_fetching_inhibited_until > pass_start)
          next_peer_unblocked_time = std::min(peer->transaction_fetching_inhibited_until, next_peer_unblocked_time);
        const size_t window = transaction_fetch_window(transaction_fetching_inhibited, in_flight,
                                                       _node_configuration.maximum_transactions_in_flight,
                                                       peer->received_transaction_size.transactions_to_request(peer->items_requested_from_peer.transactions()));
        const size_t max_transactions = gate.transactions_to_request(window, [&]() { return has_transaction_to_fetch(peer); });
        if (max_transactions == 0 && !peer->fetch_candidates.has_blocks())
          continue;
        peer->fetch_candidates.begin_pass();
        pass.idle_block_sources.emplace(peer.get(), peer->idle_block_source(pass_start));
        pass.peers.push_back({peer, max_transactions > 0, max_transactions,
                              peer->received_transaction_size.max_transactions_per_request()});
      }
      return pass;
    }

    // The listed peers take turns, one item each, so items several of them advertised are spread evenly among them,
    // except that a block goes to a peer that delivered us blocks before, and one with nothing outstanding, when one
    // advertised it (see judge_fetch_candidate()); one asked and not delivering is replaced soon
    // (rerequest_overdue_blocks()).  The turns start after the peer that took the last transaction before (see
    // fetch_turn_order), so a headroom worth a few transactions per pass, or the gate's minimum, is spread over the
    // peers from pass to pass
    void node_impl::take_fetch_turns(fetch_pass& pass, transaction_fetch_gate& gate, fc::time_point& next_peer_unblocked_time)
    {
      _fetch_turn_order.take_turns(pass.peers.size(), [&](size_t turn)
      {
        peer_and_items_to_fetch& peer_and_items = pass.peers[turn];
        if (peer_and_items.done)
          return false;
        const peer_connection_ptr& peer = peer_and_items.peer;
        const bool include_transactions = peer_and_items.include_transactions &&
                                          gate.may_request_more([&]() { return has_transaction_to_fetch(peer); });
        const fetch_candidate_order order = include_transactions && gate.next_request_is_minimum() ? fetch_candidate_order::newest_first
                                                                                                    : fetch_candidate_order::oldest_first;
        const std::optional<item_id> item_id_to_fetch =
          peer->fetch_candidates.take_next(include_transactions, order,
                                           [&](const item_id& candidate) { return judge_fetch_candidate(peer, candidate, pass.idle_block_sources); },
                                           _statistics.fetch.candidates_examined);
        if (!item_id_to_fetch)
        {
          peer_and_items.done = true;
          return false;
        }
        const fc::time_point now = fc::time_point::now();
        if (item_id_to_fetch->item_type == graphene::net::block_message_type)
        {
          const fc::microseconds deadline = block_request_deadline(outstanding_block_requests(*item_id_to_fetch));
          peer->items_requested_from_peer.insert(*item_id_to_fetch, now, deadline);
          // nothing else need wake the loop when the block doesn't come (see rerequest_overdue_blocks())
          next_peer_unblocked_time = std::min(next_peer_unblocked_time, now + deadline);
        }
        else
          peer->items_requested_from_peer.insert(*item_id_to_fetch, now);
        // if the peer is dropped for not delivering it, the item is asked of the others that advertised it
        if (item_id_to_fetch->item_type == trx_message_type)
        {
          _transaction_inventory.keep(item_id_to_fetch->item_hash, now);
          gate.record_request(expected_transaction_bytes(*peer));
          _fetch_turn_order.record_transaction(peer.get());
        }
        peer_and_items.item_ids.push_back(*item_id_to_fetch);
        // the peer's blocks are all taken before its first transaction, so a peer that reaches
        // its transaction limit has nothing more to give in this pass
        if (item_id_to_fetch->item_type == graphene::net::block_message_type)
          peer_and_items.done = ++peer_and_items.blocks >= GRAPHENE_NET_MAX_BLOCKS_PER_PEER_DURING_NORMAL_OPERATION;
        else
          peer_and_items.done = ++peer_and_items.transactions >= peer_and_items.max_transactions;
        return true;
      });
    }

    // Blocks come first (they were taken first), then the transactions, in requests of at most
    // max_transactions_per_request each
    void node_impl::send_fetch_requests(const fetch_pass& pass)
    {
      for (const peer_and_items_to_fetch& peer_and_items : pass.peers)
      {
        _statistics.fetch.items_requested += peer_and_items.item_ids.size();
        for (const auto& items_by_type : group_fetch_requests(peer_and_items.item_ids, peer_and_items.max_transactions_per_request))
        {
          if (items_by_type.first == core_message_type_enum::trx_message_type)
          {
            _statistics.record_transaction_request(items_by_type.second.size(), peer_and_items.max_transactions_per_request);
            peer_and_items.peer->transactions_requested_since_report += items_by_type.second.size();
          }
          dlog("requesting ${count} items of type ${type} from peer ${endpoint}: ${hashes}",
               ("count", items_by_type.second.size())("type", (uint32_t)items_by_type.first)
               ("endpoint", peer_and_items.peer->get_remote_endpoint())
               ("hashes", items_by_type.second));
          if (items_by_type.first == core_message_type_enum::block_message_type)
          {
            for (const item_hash_t& id : items_by_type.second)
            {
              fc_dlog(fc::logger::get("sync"),
                      "requesting a block from peer ${endpoint} (message_id is ${id})",
                      ("endpoint", peer_and_items.peer->get_remote_endpoint())("id", id));
            }
          }

          peer_and_items.peer->send_message(fetch_items_message(items_by_type.first,
                                                                items_by_type.second));
        }
      }
    }

    bool node_impl::is_transaction_candidate_wanted(const peer_connection* peer, const item_id& item) const
    {
      const auto& items_by_id = _items_to_fetch.get<item_id_index>();
      return items_by_id.find(item) != items_by_id.end() && peer_advertised_item(peer, item);
    }

    bool node_impl::has_transaction_to_fetch(const peer_connection_ptr& peer)
    {
      return peer->fetch_candidates.has_transaction_to_take([&](const item_id& candidate) { return is_transaction_candidate_wanted(peer.get(), candidate); },
                                                           _statistics.fetch.candidates_examined);
    }

    uint64_t node_impl::transaction_bytes_on_their_way() const
    {
      uint64_t bytes = _transaction_bytes_being_validated;
      for (const peer_connection_ptr& peer : _active_connections)
        bytes += uint64_t(peer->items_requested_from_peer.transactions()) * expected_transaction_bytes(*peer);
      return bytes;
    }

    uint64_t node_impl::expected_transaction_bytes(const peer_connection& peer) const
    {
      return peer.received_transaction_size.recorded() ? peer.received_transaction_size.mean() : _validated_transaction_size.mean();
    }

    // A peer that withholds a block it advertised (it was asked because it was idle, or proven, when the block came)
    // would otherwise delay it until active_ignored_request_timeout ends its connection, and then the next such peer
    // could do the same.  Each further request has twice the deadline of the one before (block_request_deadline()):
    // a large block on a slow link takes seconds to arrive, and asking for it every second would only have the copies
    // compete for that link.  And a block is asked of no more than GRAPHENE_NET_MAX_OUTSTANDING_REQUESTS_PER_BLOCK
    // peers at a time, so several withholders in a row cost seconds, while a slow peer costs at most two more copies.
    // Deadlines run from when a request was written to the socket.  A request older than active_ignored_request_timeout
    // is kept only by a download still arriving (requested_items::timed_out()) and takes no place among the outstanding
    // ones: requests that would have timed out can't keep the block from being asked of another advertiser
    void node_impl::rerequest_overdue_blocks(fc::time_point now, fc::time_point& next_check)
    {
      VERIFY_CORRECT_THREAD();
      struct block_requests
      {
        fc::time_point all_overdue; // when the last of the outstanding requests passes its deadline
        uint32_t outstanding = 0;
        fc::time_point first_leaves_outstanding = fc::time_point::maximum();
      };
      const fc::microseconds request_timeout(_node_configuration.active_ignored_request_timeout_microseconds);
      std::unordered_map<item_id, block_requests, p2p_item_hash> requested_blocks;
      for (const peer_connection_ptr& peer : _active_connections)
        if (peer->items_requested_from_peer.blocks() != 0)
          for (const auto& item_and_time : peer->items_requested_from_peer)
            if (const item_id& item = item_and_time.first; item.item_type == block_message_type)
            {
              block_requests& requests = requested_blocks[item];
              requests.all_overdue = std::max(requests.all_overdue, peer->items_requested_from_peer.block_request_sent(item) +
                                                                    peer->items_requested_from_peer.block_deadline(item));
              if (now - item_and_time.second < request_timeout)
              {
                ++requests.outstanding;
                requests.first_leaves_outstanding = std::min(requests.first_leaves_outstanding, item_and_time.second + request_timeout);
              }
            }
      const auto& items_by_id = _items_to_fetch.get<item_id_index>();
      for (const auto& [item, requests] : requested_blocks)
      {
        // a request ending (a copy, an answer or a timeout) runs the fetch loop again
        if (requests.outstanding >= GRAPHENE_NET_MAX_OUTSTANDING_REQUESTS_PER_BLOCK)
        {
          next_check = std::min(next_check, requests.first_leaves_outstanding);
          continue;
        }
        if (now < requests.all_overdue)
        {
          next_check = std::min(next_check, requests.all_overdue);
          continue;
        }
        // queued again already (asked of the next source as soon as one can take it), arriving now, or received
        // from another peer (the request stays outstanding until the peer answers or times out)
        if (items_by_id.find(item) != items_by_id.end() ||
            _message_ids_currently_being_processed.find(item.item_hash) != _message_ids_currently_being_processed.end() ||
            _block_message_cache.contains_item(item.item_hash) ||
            std::find(_most_recent_blocks_accepted.begin(), _most_recent_blocks_accepted.end(), item.item_hash) != _most_recent_blocks_accepted.end() ||
            _delegate->has_item(item))
          continue;
        for (const peer_connection_ptr& peer : _active_connections)
          if (peer->items_requested_from_peer.find(item) != peer->items_requested_from_peer.end() && !peer->is_block_fetching_deprioritized())
          {
            dlog("peer ${endpoint} has not sent block ${id} within ${ms} ms of our request: asking for blocks elsewhere first",
                 ("endpoint", peer->get_remote_endpoint())("id", item.item_hash)
                 ("ms", peer->items_requested_from_peer.block_deadline(item).count() / 1000));
            peer->deprioritize_block_fetching();
          }
        if (queue_item_to_fetch_from_advertisers(item))
          dlog("asking another peer that advertised block ${id} for it (${outstanding} requests outstanding)",
               ("id", item.item_hash)("outstanding", requests.outstanding));
      }
    }

    uint32_t node_impl::outstanding_block_requests(const item_id& item) const
    {
      return static_cast<uint32_t>(std::count_if(_active_connections.begin(), _active_connections.end(), [&](const peer_connection_ptr& peer) {
        return peer->items_requested_from_peer.find(item) != peer->items_requested_from_peer.end();
      }));
    }

    bool node_impl::is_block_requested_from_another_peer(const peer_connection* peer, const item_id& item) const
    {
      return item.item_type == block_message_type &&
             std::any_of(_active_connections.begin(), _active_connections.end(), [&](const peer_connection_ptr& other) {
               return other.get() != peer && other->items_requested_from_peer.find(item) != other->items_requested_from_peer.end();
             });
    }

    void node_impl::add_fetch_candidate(peer_connection* peer, const item_id& item)
    {
      peer->fetch_candidates.push(item, fc::time_point::now(), oldest_item_timestamp_to_fetch());
    }

    bool node_impl::queue_item_to_fetch_from_advertisers(const item_id& item)
    {
      bool advertised = false;
      // a transaction's record is looked up once for all peers
      const transaction_inventory::record* transaction_record =
        item.item_type == trx_message_type ? _transaction_inventory.find(item.item_hash) : nullptr;
      for (const peer_connection_ptr& peer : _active_connections)
        if (item.item_type == trx_message_type ? peer_advertised_transaction(peer.get(), transaction_record)
                                               : peer->block_inventory_peer_advertised_to_us.find(item) != peer->block_inventory_peer_advertised_to_us.end())
        {
          add_fetch_candidate(peer.get(), item);
          advertised = true;
        }
      if (advertised)
        _items_to_fetch.insert(prioritized_item_id(item, _items_to_fetch_sequence_counter++));
      return advertised;
    }

    void node_impl::trigger_fetch_items_loop(bool for_block)
    {
      VERIFY_CORRECT_THREAD();
      _fetch_items_loop_trigger.trigger(for_block);
    }

    // blocks sort first in _items_to_fetch, so this looks only at the queued blocks
    bool node_impl::peer_advertised_a_queued_block(const peer_connection* peer) const
    {
      for (const prioritized_item_id& queued : _items_to_fetch)
      {
        if (queued.item.item_type != graphene::net::block_message_type)
          return false;
        if (peer->block_inventory_peer_advertised_to_us.find(queued.item) != peer->block_inventory_peer_advertised_to_us.end())
          return true;
      }
      return false;
    }

    void node_impl::advertise_inventory_loop()
    {
      while (!_advertise_inventory_loop_done.canceled())
      {
        dlog("beginning an iteration of advertise inventory");
        // swap inventory into local variable, clearing the node's copy
        std::set<std::shared_ptr<full_block_type>> block_inventory_to_advertise;
        std::set<std::shared_ptr<full_transaction_type>> transaction_inventory_to_advertise;

        block_inventory_to_advertise.swap(_new_block_inventory);
        transaction_inventory_to_advertise.swap(_new_transaction_inventory);

        // process all inventory to advertise and construct the inventory messages we'll send
        // first, then send them all in a batch (to avoid any fiber interruption points while
        // we're computing the messages)
        std::list<std::pair<peer_connection_ptr, item_ids_inventory_message>> inventory_messages_to_send;

        expire_old_transaction_inventory();
        const fc::time_point_sec now = fc::time_point::now();

        // don't send a peer anything we've already advertised to it or anything it has advertised to us.
        // Group the items we need to send by type, because we'll need to send one inventory message per type
        struct peer_and_items_to_advertise
        {
          peer_connection_ptr peer;
          std::map<uint32_t, std::vector<item_hash_t>> items_to_advertise_by_type;
          unsigned total_items = 0;
        };
        std::vector<peer_and_items_to_advertise> items_by_peer;
        // only advertise to peers who are in sync with us
        for (const peer_connection_ptr& peer : _active_connections)
          if (!peer->peer_needs_sync_items_from_us)
            items_by_peer.push_back({peer});

        for (peer_and_items_to_advertise& peer_and_items : items_by_peer)
        {
          const peer_connection_ptr& peer = peer_and_items.peer;
          for (const std::shared_ptr<full_block_type>& full_block_to_advertise : block_inventory_to_advertise)
          {
            fc::ripemd160 item_hash_for_block = peer->advertise_blocks_by_block_id() ? full_block_to_advertise->get_block_id() 
                                                                                     : full_block_to_advertise->get_legacy_block_message_hash();
            item_id item_to_advertise{graphene::net::block_message_type, item_hash_for_block};

            if (peer->block_inventory_advertised_to_peer.find(item_to_advertise) == peer->block_inventory_advertised_to_peer.end() &&
                peer->block_inventory_peer_advertised_to_us.find(item_to_advertise) == peer->block_inventory_peer_advertised_to_us.end())
            {
              peer_and_items.items_to_advertise_by_type[item_to_advertise.item_type].push_back(item_to_advertise.item_hash);
              peer->block_inventory_advertised_to_peer.insert(peer_connection::timestamped_item_id(item_to_advertise, now));
              ++peer_and_items.total_items;
              dlog("advertising block ${id} to peer ${endpoint}", ("id", item_to_advertise.item_hash)("endpoint", peer->get_remote_endpoint()));
            }
          }
        }

        // each transaction's record is looked up once, then every peer is checked by its slot
        for (const std::shared_ptr<full_transaction_type>& full_transaction_to_advertise : transaction_inventory_to_advertise)
        {
          const item_hash_t& item_hash = full_transaction_to_advertise->get_legacy_transaction_message_hash();
          const transaction_inventory::record& item_record = _transaction_inventory.get(item_hash, now);
          for (peer_and_items_to_advertise& peer_and_items : items_by_peer)
          {
            const peer_connection_ptr& peer = peer_and_items.peer;
            const transaction_inventory::slot_type slot = *peer->transaction_inventory_slot;
            if (!item_record.advertised_to_us(slot) && _transaction_inventory.record_advertised_to_peer(item_record, slot, now))
            {
              peer_and_items.items_to_advertise_by_type[graphene::net::trx_message_type].push_back(item_hash);
              ++peer_and_items.total_items;
              testnetlog("advertising transaction ${id} to peer ${endpoint}", ("id", item_hash)("endpoint", peer->get_remote_endpoint()));
              dlog("advertising transaction ${id} to peer ${endpoint}", ("id", item_hash)("endpoint", peer->get_remote_endpoint()));
            }
          }
        }

        for (const peer_and_items_to_advertise& peer_and_items : items_by_peer)
        {
          dlog("advertising ${count} new item(s) of ${types} type(s) to peer ${endpoint}",
               ("count", peer_and_items.total_items)("types", peer_and_items.items_to_advertise_by_type.size())
               ("endpoint", peer_and_items.peer->get_remote_endpoint()));
          for (const auto& items_group : peer_and_items.items_to_advertise_by_type)
            inventory_messages_to_send.push_back(std::make_pair(peer_and_items.peer, item_ids_inventory_message(items_group.first, items_group.second)));
        }

        for (const peer_connection_ptr& peer : _active_connections)
        {
          peer->clear_old_inventory();
          process_deferred_transaction_advertisements(peer.get());
        }

        // a transaction the peer's send queue has no room to advertise yet stays marked as advertised to it: its id
        // waits for the queue to drain (peer_connection::send_transaction_inventory())
        for (auto iter = inventory_messages_to_send.begin(); iter != inventory_messages_to_send.end(); ++iter)
          if (iter->second.item_type == graphene::net::trx_message_type)
            iter->first->send_transaction_inventory(iter->second.item_hashes_available);
          else
            iter->first->send_message(iter->second);
        inventory_messages_to_send.clear();

        if (_new_block_inventory.empty() && _new_transaction_inventory.empty())
        {
          _retrigger_advertise_inventory_loop_promise = fc::promise<void>::create("graphene::net::retrigger_advertise_inventory_loop");
          _retrigger_advertise_inventory_loop_promise->wait();
          _retrigger_advertise_inventory_loop_promise.reset();
        }
      } // while(!canceled)
    }

    void node_impl::trigger_advertise_inventory_loop()
    {
      VERIFY_CORRECT_THREAD();
      if( _retrigger_advertise_inventory_loop_promise )
        _retrigger_advertise_inventory_loop_promise->set_value();
    }

    //self-schedules every 1 second to look for peers that aren't responding well
    // A pass of the inactive connections check acts on the connections in this order:
    //  1. the breaker override (stall rule 1 in config.hpp): peers kept by the invalid-block breaker while our head stalls
    //  2. handshaking connections past their deadline or inactive (judge_handshaking_connection())
    //  3. active connections, by the highest ranked reason that holds (judge_active_connection())
    //  4. connections we asked to close that didn't, and terminated connections that never finished closing
    //  5. peer expansion (stall rule 3) and silent-peer replacement (stall rule 4), after the disconnects above so
    //     the peers they closed don't count toward their floor, and then the sync restart (stall rule 2)
    // Steps 1-4 only sort the connections into the lists of an inactive_connection_actions without yielding; the
    // closes that don't yield follow at once, the ones that send a reason (and so may yield) after that.
    void node_impl::terminate_inactive_connections_loop()
    {
      dlog("checking if we can terminate inactive connections");
      inactive_connection_actions actions;
      {
        ASSERT_TASK_NOT_PREEMPTED();
        const fc::time_point now = fc::time_point::now();
        find_peers_kept_by_stalled_breaker(now, actions);
        judge_handshaking_connections(now, actions);
        judge_active_connections(now, actions);
        judge_closing_connections(now, actions);

        // deleting doesn't yield in its current implementation, and once we start yielding, we may find that we've
        // moved that peer to another list (closed or active), which triggers assertions, maybe even errors
        for (const peer_connection_ptr& peer : actions.to_terminate)
        {
          assert(_terminating_connections.find(peer) != _terminating_connections.end());
          _terminating_connections.erase(peer);
          schedule_peer_for_deletion(peer);
        }
        // closing abruptly doesn't yield either
        for (const peer_connection_ptr& peer : actions.to_disconnect_forcibly)
        {
          move_peer_to_terminating_list(peer);
          peer->close_connection();
        }
      } // end ASSERT_TASK_NOT_PREEMPTED()

      // disconnecting sends the reason, so it may yield
      for (const peer_connection_ptr& peer : actions.to_disconnect_gently)
      {
        fc::exception detailed_error(FC_LOG_MESSAGE(warn, "Disconnecting due to inactivity",
                                                    ("last_message_received_seconds_ago", (peer->get_last_message_received_time() - fc::time_point::now()).count() / fc::seconds(1).count())
                                                    ("last_message_sent_seconds_ago", (peer->get_last_message_sent_time() - fc::time_point::now()).count() / fc::seconds(1).count())
                                                    ("active_disconnect_timeout", actions.inactivity_timeout.to_seconds())));
        disconnect_from_peer(peer.get(), "Disconnecting due to inactivity", false, detailed_error);
      }

      for (const peer_connection_ptr& peer : actions.to_disconnect_for_rejected_blocks)
      {
        fc::exception detailed_error(FC_LOG_MESSAGE(warn, "You delivered a block we rejected, and our chain has made no progress since"));
        disconnect_from_peer(peer.get(), "You delivered a block we rejected, and our chain has made no progress since", true, detailed_error);
      }

      expand_peers_while_starved();
      for (const peer_connection_ptr& peer : find_silent_peers_to_replace())
        disconnect_silent_peer(peer.get());

      for (const peer_connection_ptr& peer : actions.to_send_keepalive)
        peer->send_message(current_time_request_message(),
                           offsetof(current_time_request_message, request_sent_time));

      restart_sync_if_head_stalled();

      if (!node_is_shutting_down() && !_terminate_inactive_connections_loop_done.canceled())
         _terminate_inactive_connections_loop_done = schedule_task( [this](){ terminate_inactive_connections_loop(); },
                                                                   fc::time_point::now() + fc::seconds(1),
                                                                   "terminate_inactive_connections_loop" );
    }

    // The invalid-block circuit breaker keeps peers that would otherwise have been disconnected, on the theory that
    // mass rejections indicate a systemic event rather than genuinely bad peers.  That theory holds only while the
    // chain keeps advancing.  If our head has stalled and we are still holding peers whose blocks we rejected, the
    // breaker may be preserving a peer set that cannot feed us the chain (all slots occupied, connect loop satisfied,
    // no progress) -- revoke the deferral for the flagged peers so their slots open up for replacements.
    void node_impl::find_peers_kept_by_stalled_breaker(fc::time_point now, inactive_connection_actions& actions)
    {
      if (!breaker_stall_override_due(_cached_head_block_num, _last_head_advance_time, now))
        return;
      for (const peer_connection_ptr& active_peer : _active_connections)
        if (active_peer->block_rejected_while_breaker_tripped)
        {
          wlog("disconnecting peer ${peer}: it delivered a block we rejected while the disconnect circuit breaker "
               "was active, and our chain has made no progress in the ${timeout} seconds since",
               ("peer", active_peer->get_remote_endpoint())("timeout", GRAPHENE_NET_BREAKER_STALL_OVERRIDE_SEC));
          actions.to_disconnect_for_rejected_blocks.push_back(active_peer);
        }
    }

    void node_impl::judge_handshaking_connections(fc::time_point now, inactive_connection_actions& actions)
    {
      const uint32_t handshaking_timeout = _node_configuration.peer_inactivity_timeout;
      for (const peer_connection_ptr& handshaking_peer : _handshaking_connections)
      {
        const handshaking_connection_state state{handshaking_peer->connection_initiation_time,
                                                 handshaking_peer->get_last_message_received_time(),
                                                 handshaking_peer->get_last_message_sent_time()};
        switch (judge_handshaking_connection(state, now, fc::seconds(handshaking_timeout), fc::seconds(GRAPHENE_NET_HANDSHAKE_DEADLINE_SEC)))
        {
        case handshaking_connection_verdict::keep:
          continue;
        case handshaking_connection_verdict::past_deadline:
          wlog("Forcibly disconnecting from peer ${peer}, still handshaking ${deadline} seconds after the connection started. "
               "Negotiating status: ${status}, bytes sent: ${sent}, bytes received: ${received}",
               ("peer", handshaking_peer->get_remote_endpoint())("deadline", GRAPHENE_NET_HANDSHAKE_DEADLINE_SEC)
               ("status", handshaking_peer->negotiation_status)
               ("sent", handshaking_peer->get_total_bytes_sent())
               ("received", handshaking_peer->get_total_bytes_received()));
          handshaking_peer->connection_closed_error = fc::exception(FC_LOG_MESSAGE(warn, "Terminating connection still handshaking ${deadline} seconds after it started.  Negotiating status: ${status}",
                                                                                   ("deadline", GRAPHENE_NET_HANDSHAKE_DEADLINE_SEC)
                                                                                   ("status", handshaking_peer->negotiation_status)));
          break;
        case handshaking_connection_verdict::inactive:
          wlog("Forcibly disconnecting from handshaking peer ${peer} due to inactivity of at least ${handshaking_timeout} seconds",
               ("peer", handshaking_peer->get_remote_endpoint())(handshaking_timeout));
          wlog("Peer's negotiating status: ${status}, bytes sent: ${sent}, bytes received: ${received}",
               ("status", handshaking_peer->negotiation_status)
               ("sent", handshaking_peer->get_total_bytes_sent())
               ("received", handshaking_peer->get_total_bytes_received()));
          handshaking_peer->connection_closed_error = fc::exception(FC_LOG_MESSAGE(warn, "Terminating handshaking connection due to inactivity of ${timeout} seconds.  Negotiating status: ${status}, bytes sent: ${sent}, bytes received: ${received}",
                                                                                   ("peer", handshaking_peer->get_remote_endpoint())
                                                                                   ("timeout", handshaking_timeout)
                                                                                   ("status", handshaking_peer->negotiation_status)
                                                                                   ("sent", handshaking_peer->get_total_bytes_sent())
                                                                                   ("received", handshaking_peer->get_total_bytes_received())));
          break;
        }
        handshaking_peer->connection_closed_by_failure = true;
        actions.to_disconnect_forcibly.push_back(handshaking_peer);
      }
    }

    active_connection_state node_impl::get_active_connection_state(const peer_connection& peer, fc::time_point now,
                                                                   fc::microseconds request_timeout) const
    {
      active_connection_state state;
      state.initiated = peer.connection_initiation_time;
      state.last_message_received = peer.get_last_message_received_time();
      if (!peer.sync_items_requested_from_peer.empty())
        state.last_sync_item_received = peer.last_sync_item_received_time;
      if (peer.item_ids_requested_from_peer)
        state.sync_item_ids_requested = peer.item_ids_requested_from_peer->get<1>();
      const message_in_progress* block_download = peer.get_block_message_in_progress();
      state.ignored_item = peer.items_requested_from_peer.timed_out(now, request_timeout, block_download);
      state.send_progress = peer.get_send_progress_time();
      state.sync_idle = peer.we_need_sync_items_from_peer &&
                        peer.ids_of_items_being_processed.empty() && // we're not processing any blocks from this peer right now
                        !peer.is_currently_handling_message() &&     // we're not processing a message it sent us
                        !peer.item_ids_requested_from_peer &&        // not requesting any new ids
                        peer.ids_of_items_to_get.empty();            // all received ids have been sent to the blockchain
      return state;
    }

    // These numbers are just guesses.  If we and our peers get disconnected from the rest of the network, we will
    // not receive any blocks or transactions from the rest of the world, and that will probably make us disconnect
    // from our peers even though we have working connections to them (but they won't have sent us anything since
    // they aren't getting blocks either).  This might not be so bad because it could make us initiate more
    // connections and reconnect with the rest of the network, or it might just further isolate us.
    void node_impl::judge_active_connections(fc::time_point now, inactive_connection_actions& actions)
    {
      // the peer must have sent a message within 10 blocks
      const uint32_t active_disconnect_timeout = 10 * _recent_block_interval_in_seconds;
      actions.inactivity_timeout = fc::seconds(active_disconnect_timeout);
      const active_connection_limits limits{now, actions.inactivity_timeout, fc::seconds(active_disconnect_timeout / 2),
                                            fc::microseconds(_node_configuration.active_ignored_request_timeout_microseconds),
                                            fc::seconds(GRAPHENE_NET_SEND_STALL_DISCONNECT_SEC)};
      for (const peer_connection_ptr& active_peer : _active_connections)
      {
        const active_connection_state state = get_active_connection_state(*active_peer, now, limits.request_timeout);
        switch (judge_active_connection(state, limits))
        {
        case active_connection_verdict::keep:
          break;
        case active_connection_verdict::inactive:
          wlog("Closing connection with peer ${peer} due to inactivity of at least ${timeout} seconds",
               ("peer", active_peer->get_remote_endpoint())("timeout", active_disconnect_timeout));
          actions.to_disconnect_gently.push_back(active_peer);
          break;
        case active_connection_verdict::sync_items_ignored:
          fc_wlog(fc::logger::get("sync"),
                  "disconnecting peer ${peer} because they haven't made any progress on my remaining ${count} sync item requests",
                  ("peer", active_peer->get_remote_endpoint())("count", active_peer->sync_items_requested_from_peer.size()));
          wlog("Disconnecting peer ${peer} because they haven't made any progress on my remaining ${count} sync item requests",
               ("peer", active_peer->get_remote_endpoint())("count", active_peer->sync_items_requested_from_peer.size()));
          close_for_request_timeout(active_peer, actions,
                                    fc::exception(FC_LOG_MESSAGE(warn, "Disconnecting peer because they haven't made any progress on my remaining ${count} sync item requests",
                                                                 ("count", active_peer->sync_items_requested_from_peer.size()))));
          break;
        case active_connection_verdict::sync_item_ids_ignored:
          fc_wlog(fc::logger::get("sync"), "Disconnecting peer ${peer} because they didn't respond to my request for sync item ids after ${synopsis}",
                  ("peer", active_peer->get_remote_endpoint())
                  ("synopsis", active_peer->item_ids_requested_from_peer->get<0>()));
          wlog("Disconnecting peer ${peer} because they didn't respond to my request for sync item ids after ${synopsis}",
               ("peer", active_peer->get_remote_endpoint())
               ("synopsis", active_peer->item_ids_requested_from_peer->get<0>()));
          close_for_request_timeout(active_peer, actions,
                                    fc::exception(FC_LOG_MESSAGE(warn, "Disconnecting peer because they didn't respond to my request for sync item ids after my synopsis")));
          break;
        case active_connection_verdict::item_request_ignored:
          fc_wlog(fc::logger::get("sync"), "Disconnecting peer ${peer} because they didn't respond to my request for item ${id}",
                  ("peer", active_peer->get_remote_endpoint())("id", state.ignored_item->item_hash));
          wlog("Disconnecting peer ${peer} because they didn't respond to my request for item ${id}",
               ("peer", active_peer->get_remote_endpoint())("id", state.ignored_item->item_hash));
          close_for_request_timeout(active_peer, actions,
                                    fc::exception(FC_LOG_MESSAGE(warn, "Disconnecting peer because they didn't respond to my request for item ${id}",
                                                                 ("id", state.ignored_item->item_hash))));
          break;
        case active_connection_verdict::send_stalled:
          // inventory and replies waiting for its queue to drain neither close the connection nor make it look idle,
          // so a peer that sends us messages and reads nothing would otherwise keep its slot and what we hold for it
          wlog("Disconnecting peer ${peer} because nothing we had for it has been written to its socket in ${timeout} seconds",
               ("peer", active_peer->get_remote_endpoint())("timeout", GRAPHENE_NET_SEND_STALL_DISCONNECT_SEC));
          close_for_request_timeout(active_peer, actions,
                                    fc::exception(FC_LOG_MESSAGE(warn, "Disconnecting peer because nothing we had for it has been written to its socket in ${timeout} seconds",
                                                                 ("timeout", GRAPHENE_NET_SEND_STALL_DISCONNECT_SEC))));
          break;
        case active_connection_verdict::send_keepalive:
          wlog("Sending a keepalive message to peer ${peer} who hasn't sent us any messages in the last ${timeout} seconds",
               ("peer", active_peer->get_remote_endpoint())("timeout", limits.keepalive_timeout.to_seconds()));
          actions.to_send_keepalive.push_back(active_peer);
          break;
        case active_connection_verdict::sync_stalled:
          // it sent us a message recently and we need sync data from it, yet we aren't processing or asking for new
          // sync data.  We should get into this state only in a race, but if we do, we disconnect the peer to
          // re-establish the connection
          fc_wlog(fc::logger::get("sync"), "Disconnecting peer ${peer} because we think we need blocks from them but sync has stalled.",
                  ("peer", active_peer->get_remote_endpoint()));
          wlog("Disconnecting peer ${peer} because we think we need blocks from them but sync has stalled.",
               ("peer", active_peer->get_remote_endpoint()));
          actions.to_disconnect_forcibly.push_back(active_peer);
          break;
        }
      }
    }

    // we should probably disconnect nicely and give them a reason, but right now the logic for rescheduling the
    // requests only executes when the connection is fully closed, and we want to get those requests rescheduled as
    // soon as possible
    void node_impl::close_for_request_timeout(const peer_connection_ptr& peer, inactive_connection_actions& actions,
                                              const fc::exception& reason)
    {
      peer->connection_closed_error = reason;
      peer->connection_closed_by_failure = true;
      actions.to_disconnect_forcibly.push_back(peer);
    }

    void node_impl::judge_closing_connections(fc::time_point now, inactive_connection_actions& actions)
    {
      const fc::time_point closing_disconnect_threshold = now - fc::seconds(GRAPHENE_NET_PEER_DISCONNECT_TIMEOUT);
      for (const peer_connection_ptr& closing_peer : _closing_connections)
        if (closing_peer->connection_closed_time < closing_disconnect_threshold)
        {
          // we asked this peer to close their connection to us at least GRAPHENE_NET_PEER_DISCONNECT_TIMEOUT
          // seconds ago, but they haven't done it yet.  Terminate the connection now
          wlog("Forcibly disconnecting peer ${peer} who failed to close their connection in a timely manner",
               ("peer", closing_peer->get_remote_endpoint()));
          actions.to_disconnect_forcibly.push_back(closing_peer);
        }

      const fc::time_point failed_terminate_threshold = now - fc::seconds(GRAPHENE_NET_FAILED_TERMINATE_TIMEOUT_SECONDS);
      for (const peer_connection_ptr& peer : _terminating_connections)
        if (peer->get_connection_terminated_time() != fc::time_point::min() &&
            peer->get_connection_terminated_time() < failed_terminate_threshold)
        {
          wlog("Terminating connection with peer ${peer}, closing the connection didn't work", ("peer", peer->get_remote_endpoint()));
          actions.to_terminate.push_back(peer);
        }
    }

    void node_impl::fetch_updated_peer_lists_loop()
    {
      dlog("start fetch_updated_peer_lists_loop");
      std::list<peer_connection_ptr> original_active_peers(_active_connections.begin(), _active_connections.end());
      for( const peer_connection_ptr& active_peer : original_active_peers )
      {
        try
        {
          active_peer->send_message(address_request_message());
        }
        catch ( const fc::canceled_exception& )
        {
          throw;
        }
        catch (const fc::exception& e)
        {
          dlog("Caught exception while sending address request message to peer ${peer} : ${e}",
               ("peer", active_peer->get_remote_endpoint())("e", e));
        }
      }

      // this has nothing to do with updating the peer list, but we need to prune this list
      // at regular intervals, this is a fine place to do it.
      fc::time_point_sec oldest_failed_ids_to_keep(fc::time_point::now() - fc::minutes(GRAPHENE_NET_PRUNE_FAILED_IDS_MINUTES));
      auto oldest_failed_ids_to_keep_iter = _recently_failed_items.get<peer_connection::timestamp_index>().lower_bound(oldest_failed_ids_to_keep);
      auto begin_iter = _recently_failed_items.get<peer_connection::timestamp_index>().begin();
      _recently_failed_items.get<peer_connection::timestamp_index>().erase(begin_iter, oldest_failed_ids_to_keep_iter);

      if (!node_is_shutting_down() && !_fetch_updated_peer_lists_loop_done.canceled() )
         _fetch_updated_peer_lists_loop_done = schedule_task( [this](){ fetch_updated_peer_lists_loop(); },
                                                             fc::time_point::now() + fc::minutes(GRAPHENE_NET_FETCH_UPDATED_PEER_LISTS_INTERVAL_MINUTES),
                                                             "fetch_updated_peer_lists_loop" );
      dlog("done fetch_updated_peer_lists_loop (we're not really a loop)");
    }

    void node_impl::update_bandwidth_data(uint32_t bytes_read_this_second, uint32_t bytes_written_this_second)
    {
      VERIFY_CORRECT_THREAD();
      _average_network_read_speed_seconds.push_back(bytes_read_this_second);
      _average_network_write_speed_seconds.push_back(bytes_written_this_second);
      ++_average_network_usage_second_counter;
      if (_average_network_usage_second_counter >= 60)
      {
        _average_network_usage_second_counter = 0;
        ++_average_network_usage_minute_counter;
        uint32_t average_read_this_minute = (uint32_t)boost::accumulate(_average_network_read_speed_seconds, uint64_t(0)) / (uint32_t)_average_network_read_speed_seconds.size();
        _average_network_read_speed_minutes.push_back(average_read_this_minute);
        uint32_t average_written_this_minute = (uint32_t)boost::accumulate(_average_network_write_speed_seconds, uint64_t(0)) / (uint32_t)_average_network_write_speed_seconds.size();
        _average_network_write_speed_minutes.push_back(average_written_this_minute);
        if (_average_network_usage_minute_counter >= 60)
        {
          _average_network_usage_minute_counter = 0;
          uint32_t average_read_this_hour = (uint32_t)boost::accumulate(_average_network_read_speed_minutes, uint64_t(0)) / (uint32_t)_average_network_read_speed_minutes.size();
          _average_network_read_speed_hours.push_back(average_read_this_hour);
          uint32_t average_written_this_hour = (uint32_t)boost::accumulate(_average_network_write_speed_minutes, uint64_t(0)) / (uint32_t)_average_network_write_speed_minutes.size();
          _average_network_write_speed_hours.push_back(average_written_this_hour);
        }
      }
    }
    void node_impl::bandwidth_monitor_loop()
    {
      dlog("start bandwidth_monitor_loop");
      fc::time_point_sec current_time = fc::time_point::now();

      if (_bandwidth_monitor_last_update_time == fc::time_point_sec::min())
        _bandwidth_monitor_last_update_time = current_time;

      uint32_t seconds_since_last_update = current_time.sec_since_epoch() - _bandwidth_monitor_last_update_time.sec_since_epoch();
      seconds_since_last_update = std::max(UINT32_C(1), seconds_since_last_update);
      uint32_t bytes_read_this_second = _rate_limiter.get_actual_download_rate();
      uint32_t bytes_written_this_second = _rate_limiter.get_actual_upload_rate();
      for (uint32_t i = 0; i < seconds_since_last_update - 1; ++i)
        update_bandwidth_data(0, 0);
      update_bandwidth_data(bytes_read_this_second, bytes_written_this_second);
      _bandwidth_monitor_last_update_time = current_time;

      if (!node_is_shutting_down() && !_bandwidth_monitor_loop_done.canceled())
        _bandwidth_monitor_loop_done = schedule_task( [=, this](){ bandwidth_monitor_loop(); },
                                                     fc::time_point::now() + fc::seconds(GRAPHENE_NET_BANDWIDTH_MONITOR_INTERVAL_SECONDS),
                                                     "bandwidth_monitor_loop" );
      dlog("end bandwidth_monitor_loop (not a loop)");
    }

    void node_impl::dump_node_status_task()
    {
      dump_node_status();
      if (!node_is_shutting_down() && !_dump_node_status_task_done.canceled())
        _dump_node_status_task_done = schedule_task([=, this](){ dump_node_status_task(); },
                                                   fc::time_point::now() + fc::minutes(GRAPHENE_NET_DUMP_NODE_STATUS_INTERVAL_MINUTES),
                                                   "dump_node_status_task");
    }

    void node_impl::statistics_report_loop()
    {
      std::vector<uint64_t> transactions_requested_per_peer;
      transactions_requested_per_peer.reserve(_active_connections.size());
      for (const peer_connection_ptr& peer : _active_connections)
        transactions_requested_per_peer.push_back(std::exchange(peer->transactions_requested_since_report, 0));
      const fc::variant_object report = _statistics.take_report(fc::time_point::now(), _transactions_in_flight.in_flight(),
                                                                _items_to_fetch.size(), _delegate->get_pending_transaction_pool_size(),
                                                                std::move(transactions_requested_per_peer));
      fc_ilog(fc::logger::get(GRAPHENE_NET_STATISTICS_LOGGER), "p2p statistics: ${report}", (report));
      if (!node_is_shutting_down() && !_statistics_report_loop_done.canceled())
        _statistics_report_loop_done = schedule_task([this](){ statistics_report_loop(); },
                                                     fc::time_point::now() + fc::seconds(GRAPHENE_NET_STATISTICS_REPORT_INTERVAL_SEC),
                                                     "statistics_report_loop");
    }

    void node_impl::delayed_peer_deletion_task()
    {
      dlog("in delayed_peer_deletion_task with ${count} in queue", ("count", _peers_to_delete.size()));
#ifdef USE_PEERS_TO_DELETE_MUTEX
      fc::scoped_lock<fc::mutex> lock(_peers_to_delete_mutex);
      _peers_to_delete.clear();
      dlog("_peers_to_delete cleared");
#else
      while (!_peers_to_delete.empty())
      {
        std::list<peer_connection_ptr> peers_to_delete_copy;
        dlog("beginning an iteration of delayed_peer_deletion_task with ${count} in queue", ("count", _peers_to_delete.size()));
        peers_to_delete_copy.swap(_peers_to_delete);
      }
      dlog("leaving delayed_peer_deletion_task");
#endif
    }

    void node_impl::schedule_peer_for_deletion(const peer_connection_ptr& peer_to_delete)
    {
      VERIFY_CORRECT_THREAD();

      assert(_handshaking_connections.find(peer_to_delete) == _handshaking_connections.end());
      assert(_active_connections.find(peer_to_delete) == _active_connections.end());
      assert(_closing_connections.find(peer_to_delete) == _closing_connections.end());
      assert(_terminating_connections.find(peer_to_delete) == _terminating_connections.end());

#ifdef USE_PEERS_TO_DELETE_MUTEX
      dlog("scheduling peer for deletion: ${peer} (may block on a mutex here)", ("peer", peer_to_delete->get_remote_endpoint()));

      unsigned number_of_peers_to_delete;
      {
        fc::scoped_lock<fc::mutex> lock(_peers_to_delete_mutex);
        _peers_to_delete.emplace_back(peer_to_delete);
        number_of_peers_to_delete = _peers_to_delete.size();
      }
      dlog("peer scheduled for deletion: ${peer}", ("peer", peer_to_delete->get_remote_endpoint()));

      if (!node_is_shutting_down() &&
          (!_delayed_peer_deletion_task_done.valid() || _delayed_peer_deletion_task_done.ready()))
      {
        dlog("asyncing delayed_peer_deletion_task to delete ${size} peers", ("size", number_of_peers_to_delete));
        _delayed_peer_deletion_task_done = async_task([this](){ delayed_peer_deletion_task(); }, "delayed_peer_deletion_task" );
    }
      else
        dlog("delayed_peer_deletion_task is already scheduled (current size of _peers_to_delete is ${size})", ("size", number_of_peers_to_delete));
#else
      dlog("scheduling peer for deletion: ${peer} (this will not block)", ("peer", peer_to_delete->get_remote_endpoint()));
      _peers_to_delete.push_back(peer_to_delete);
      if (!node_is_shutting_down() &&
          (!_delayed_peer_deletion_task_done.valid() || _delayed_peer_deletion_task_done.ready()))
      {
        dlog("asyncing delayed_peer_deletion_task to delete ${size} peers", ("size", _peers_to_delete.size()));
        _delayed_peer_deletion_task_done = async_task([this](){ delayed_peer_deletion_task(); }, "delayed_peer_deletion_task" );
      }
      else
        dlog("delayed_peer_deletion_task is already scheduled (current size of _peers_to_delete is ${size})", ("size", _peers_to_delete.size()));

#endif
    }

    // whether the hello of @p peer can be accepted, which makes its connection hold a slot if it doesn't already
    bool node_impl::is_accepting_new_connections(const peer_connection* peer)
    {
      VERIFY_CORRECT_THREAD();
      return !node_is_shutting_down() && (!_p2p_network_connect_loop_done.valid() || !_p2p_network_connect_loop_done.canceled()) &&
         get_number_of_connections_holding_slots() + (holds_connection_slot(*peer) ? 0 : 1) <= _node_configuration.maximum_number_of_connections;
    }

    bool node_impl::is_wanting_new_connections()
    {
      VERIFY_CORRECT_THREAD();
      return !node_is_shutting_down() && !_p2p_network_connect_loop_done.canceled() &&
         get_number_of_connections_holding_slots() < get_desired_number_of_connections();
    }

    // desired_number_of_connections, or more while a peer expansion runs (see expand_peers_while_starved())
    uint32_t node_impl::get_desired_number_of_connections() const
    {
      return std::max(_node_configuration.desired_number_of_connections,
                      std::min(_peer_expansion_target, _node_configuration.maximum_number_of_connections));
    }

    uint32_t node_impl::get_number_of_connections()
    {
      VERIFY_CORRECT_THREAD();
      return (uint32_t)(_handshaking_connections.size() + _active_connections.size());
    }

    // An inbound connection holds no slot until we accept its hello, so connections that never send one can't keep
    // us from dialing or from accepting hellos; GRAPHENE_NET_MAXIMUM_HANDSHAKING_INBOUND_CONNECTIONS bounds them instead
    bool node_impl::holds_connection_slot(const peer_connection& peer) const
    {
      return peer.direction != peer_connection_direction::inbound ||
             peer.their_state == peer_connection::their_connection_state::connection_accepted;
    }

    uint32_t node_impl::get_number_of_connections_holding_slots() const
    {
      VERIFY_CORRECT_THREAD();
      return (uint32_t)(_active_connections.size() +
                        std::count_if(_handshaking_connections.begin(), _handshaking_connections.end(),
                                      [this](const peer_connection_ptr& peer) { return holds_connection_slot(*peer); }));
    }

    // Why an inbound connection from @p address is closed as soon as it is accepted, or null when it is taken on:
    // the handshaking connections that hold no slot are bounded node-wide and per host, so one host can't hold
    // them all (nor the memory they take) without making handshake progress
    const char* node_impl::get_inbound_connection_refusal(const fc::ip::address& address)
    {
      VERIFY_CORRECT_THREAD();
      std::vector<fc::ip::address> handshaking_addresses;
      for (const peer_connection_ptr& peer : _handshaking_connections)
        if (!holds_connection_slot(*peer) && peer->get_remote_endpoint())
          handshaking_addresses.push_back(peer->get_remote_endpoint()->get_address());
      if (handshaking_addresses.size() >= GRAPHENE_NET_MAXIMUM_HANDSHAKING_INBOUND_CONNECTIONS)
        return "too many inbound connections are handshaking";
      if (_node_configuration.maximum_connections_per_host == 0 || is_in_any_subnet(address, _host_limit_exempt_subnets))
        return nullptr;
      // a host at the per-host limit gets one at a time, which hears the limit in the rejection of its hello or is
      // let past it there (accept_hello_over_host_limit())
      const bool host_at_hard_limit =
        exceeds_connections_per_host(address, get_connection_addresses(true), _node_configuration.maximum_connections_per_host);
      if (exceeds_connections_per_host(address, handshaking_addresses,
                                       host_at_hard_limit ? 1 : GRAPHENE_NET_MAXIMUM_HANDSHAKING_CONNECTIONS_PER_HOST))
        return host_at_hard_limit ? "its host is at the per-host limit and has a connection handshaking"
                                  : "too many connections from its host are handshaking";
      return nullptr;
    }

    void node_impl::warn_of_refused_inbound_connections()
    {
      VERIFY_CORRECT_THREAD();
      ++_refused_inbound_connections_since_warning;
      const fc::time_point now = fc::time_point::now();
      if (_last_refused_inbound_connection_warning_time != fc::time_point() &&
          now - _last_refused_inbound_connection_warning_time < fc::seconds(GRAPHENE_NET_REFUSED_HANDSHAKE_WARNING_INTERVAL_SEC))
        return;
      wlog("closed ${count} inbound connection(s) at accept since the last warning: at most ${limit} inbound connections, "
           "${per_host} from one host, may be handshaking at once (see the debug log for each)",
           ("count", _refused_inbound_connections_since_warning)("limit", GRAPHENE_NET_MAXIMUM_HANDSHAKING_INBOUND_CONNECTIONS)
           ("per_host", GRAPHENE_NET_MAXIMUM_HANDSHAKING_CONNECTIONS_PER_HOST));
      _refused_inbound_connections_since_warning = 0;
      _last_refused_inbound_connection_warning_time = now;
    }

    peer_connection_ptr node_impl::get_peer_by_node_id(const node_id_t& node_id)
    {
      for (const peer_connection_ptr& active_peer : _active_connections)
        if (node_id == active_peer->node_id)
          return active_peer;
      for (const peer_connection_ptr& handshaking_peer : _handshaking_connections)
        if (node_id == handshaking_peer->node_id)
          return handshaking_peer;
      return peer_connection_ptr();
    }

    bool node_impl::is_already_connected_to_id(const node_id_t& node_id)
    {
      VERIFY_CORRECT_THREAD();
      if (node_id == _node_id)
      {
        dlog("is_already_connected_to_id returning true because the peer is us");
        return true;
      }
      for (const peer_connection_ptr& active_peer : _active_connections)
        if (node_id == active_peer->node_id)
        {
          dlog("is_already_connected_to_id returning true because the peer is already in our active list");
          return true;
        }
      for (const peer_connection_ptr& handshaking_peer : _handshaking_connections)
        if (node_id == handshaking_peer->node_id)
        {
          dlog("is_already_connected_to_id returning true because the peer is already in our handshaking list");
          return true;
        }
      return false;
    }

    std::vector<fc::ip::address> node_impl::get_connection_addresses(bool accepted_only, const peer_connection* excluded)
    {
      VERIFY_CORRECT_THREAD();
      std::vector<fc::ip::address> addresses;
      for (const std::unordered_set<peer_connection_ptr>* connections : { &_active_connections, &_handshaking_connections })
        for (const peer_connection_ptr& peer : *connections)
        {
          fc::optional<fc::ip::endpoint> remote_endpoint = peer->get_remote_endpoint();
          if (peer.get() == excluded || !remote_endpoint)
            continue;
          if (accepted_only && peer->their_state != peer_connection::their_connection_state::connection_accepted)
            continue;
          addresses.push_back(remote_endpoint->get_address());
        }
      return addresses;
    }

    connection_spread node_impl::get_active_connection_spread()
    {
      VERIFY_CORRECT_THREAD();
      std::vector<fc::ip::address> addresses;
      for (const peer_connection_ptr& peer : _active_connections)
        if (peer->get_remote_endpoint())
          addresses.push_back(peer->get_remote_endpoint()->get_address());
      return get_connection_spread(addresses);
    }

    bool node_impl::is_exempt_from_host_limit(const peer_connection* peer)
    {
      VERIFY_CORRECT_THREAD();
      fc::optional<fc::ip::endpoint> remote_endpoint = peer->get_remote_endpoint();
      if (!remote_endpoint)
        return false;
      if (is_in_any_subnet(remote_endpoint->get_address(), _host_limit_exempt_subnets))
        return true;
      // seeds and add-node targets only on the connections we dialed: an inbound one comes from an ephemeral port,
      // whichever node it is
      return peer->direction == peer_connection_direction::outbound &&
             _host_limit_exempt_endpoints.count(*remote_endpoint) != 0;
    }

    // A relaxed per-host limit (relax_per_host_limit) spreads connections over hosts while other hosts can fill the slots, and gives way
    // when none can: with candidates on few hosts (a testnet, a private network) it would leave the node short of
    // connections without saying why.  Not right after start, while the connect loop knows few hosts yet: the first
    // one it reaches would take most of the slots.  For peers that dial us it gives way less (see
    // accept_hello_over_host_limit()).  A hard limit is kept whatever it costs
    bool node_impl::is_host_limit_relaxed() const
    {
      return _node_configuration.relax_per_host_limit && _node_configuration.maximum_connections_per_host != 0 &&
             fc::time_point::now() >= _host_limit_hard_until;
    }

    void node_impl::log_host_limit_relaxed(const std::vector<fc::ip::address>& addresses)
    {
      VERIFY_CORRECT_THREAD();
      const fc::time_point now = fc::time_point::now();
      if (_last_host_limit_relaxed_log_time != fc::time_point() &&
          now - _last_host_limit_relaxed_log_time < fc::seconds(GRAPHENE_NET_HOST_LIMIT_RELAXED_LOG_INTERVAL_SEC))
        return;
      _last_host_limit_relaxed_log_time = now;
      ilog("per-host cap relaxed: ${peers} peers on ${hosts} hosts, desired ${desired}; the limit of ${limit} per host "
           "gives way while no other hosts fill the slots (set p2p-relax-per-host-limit = false to make it a hard limit)",
           ("peers", addresses.size())("hosts", get_connection_spread(addresses).hosts)
           ("desired", _node_configuration.desired_number_of_connections)("limit", _node_configuration.maximum_connections_per_host));
    }

    peer_connection_ptr node_impl::find_active_connection_over_host_limit(const peer_connection* excluded)
    {
      VERIFY_CORRECT_THREAD();
      std::vector<peer_connection_ptr> peers;
      for (const peer_connection_ptr& peer : _active_connections)
        if (peer->get_remote_endpoint())
          peers.push_back(peer);
      // the newest of a host is the one closed
      std::sort(peers.begin(), peers.end(), [](const peer_connection_ptr& a, const peer_connection_ptr& b) {
        return a->connection_initiation_time < b->connection_initiation_time;
      });
      std::vector<host_limited_connection> connections;
      for (const peer_connection_ptr& peer : peers)
        connections.push_back(host_limited_connection{peer->get_remote_endpoint()->get_address(),
                                                      peer.get() == excluded || is_exempt_from_host_limit(peer.get())});
      const fc::optional<size_t> selected = select_connection_over_host_limit(connections, _node_configuration.maximum_connections_per_host);
      return selected ? peers[*selected] : peer_connection_ptr();
    }

    // see dial_new_host_for_connection_over_host_limit()
    void node_impl::give_way_to_connection_under_host_limit(const peer_connection* new_peer)
    {
      VERIFY_CORRECT_THREAD();
      if (!is_host_limit_relaxed() || _active_connections.size() <= get_desired_number_of_connections() ||
          !new_peer->get_remote_endpoint())
        return;
      std::vector<fc::ip::address> others;
      for (const peer_connection_ptr& peer : _active_connections)
        if (peer.get() != new_peer && peer->get_remote_endpoint())
          others.push_back(peer->get_remote_endpoint()->get_address());
      // a connection over the limit itself doesn't take another one's slot
      if (!is_exempt_from_host_limit(new_peer) &&
          exceeds_connections_per_host(new_peer->get_remote_endpoint()->get_address(), others, _node_configuration.maximum_connections_per_host))
        return;
      const peer_connection_ptr over_limit = find_active_connection_over_host_limit(new_peer);
      if (!over_limit)
        return;
      ilog("closing ${peer}, a connection over the per-host limit of ${limit}: ${new_peer}, on a host under it, takes its slot",
           ("peer", over_limit->get_remote_endpoint())("limit", _node_configuration.maximum_connections_per_host)
           ("new_peer", new_peer->get_remote_endpoint()));
      // not caused by an error: the peer may dial us again, and we refuse it while we have enough connections
      disconnect_from_peer(over_limit.get(), "I'm making room for a peer on another host than yours");
    }

    // Whether a hello over the per-host limit is accepted all the same, while the limit is relaxed and we hold fewer
    // than the desired number of connections (counting those whose hello we accepted, as the limit does).  On a
    // connection the connect loop dialed past the limit, it picked it one step at a time already
    // (select_connection_candidates).  A peer that dials us gets no more than its host's share of the desired
    // connections, and half of them at most (accepts_inbound_hello_over_host_limit()): nodes behind a NAT or on a
    // shared /64 reach us past the limit, but one host dialing us first, right after a restart say, doesn't fill the
    // slots so that the connect loop would then dial nobody
    bool node_impl::accept_hello_over_host_limit(const peer_connection* peer, const fc::ip::address& address)
    {
      VERIFY_CORRECT_THREAD();
      if (!is_host_limit_relaxed())
        return false;
      std::vector<fc::ip::address> addresses = get_connection_addresses(true, peer);
      if (peer->direction == peer_connection_direction::outbound
            ? addresses.size() >= _node_configuration.desired_number_of_connections
            : !accepts_inbound_hello_over_host_limit(address, addresses, _node_configuration.desired_number_of_connections))
        return false;
      addresses.push_back(address);
      log_host_limit_relaxed(addresses);
      return true;
    }

    void node_impl::warn_if_inbound_connections_share_host(const fc::ip::address& refused_address)
    {
      VERIFY_CORRECT_THREAD();
      const fc::time_point now = fc::time_point::now();
      if (_last_shared_inbound_host_warning_time != fc::time_point() &&
          now - _last_shared_inbound_host_warning_time < fc::seconds(GRAPHENE_NET_SHARED_INBOUND_HOST_WARNING_INTERVAL_SEC))
        return;
      std::vector<fc::ip::address> inbound_addresses;
      for (const std::unordered_set<peer_connection_ptr>* connections : { &_active_connections, &_handshaking_connections })
        for (const peer_connection_ptr& peer : *connections)
          if (peer->direction == peer_connection_direction::inbound &&
              peer->their_state == peer_connection::their_connection_state::connection_accepted && peer->get_remote_endpoint())
            inbound_addresses.push_back(peer->get_remote_endpoint()->get_address());
      if (!inbound_connections_share_host(refused_address, inbound_addresses))
        return;
      _last_shared_inbound_host_warning_time = now;
      wlog("Most inbound peers come from ${host}, and the per-host limit of ${limit} refuses more of them. If ${host} is a proxy "
           "that rewrites source addresses (docker-proxy, rootless Docker), add it to p2p-host-limit-exempt or set "
           "p2p-max-connections-per-host = 0",
           ("host", connection_host_of(refused_address))("limit", _node_configuration.maximum_connections_per_host));
    }

    // merge addresses received from a peer into our database
    bool node_impl::merge_address_info_with_potential_peer_database(const std::vector<address_info> addresses)
    {
      VERIFY_CORRECT_THREAD();
      bool new_information_received = false;
      for (const address_info& address : addresses)
      {
        if (address.firewalled == graphene::net::firewalled_state::not_firewalled &&
            !_potential_peer_db.is_own_endpoint(address.remote_endpoint))
        {
          potential_peer_record updated_peer_record = _potential_peer_db.lookup_or_create_entry_for_endpoint(address.remote_endpoint);
          if (address.last_seen_time > updated_peer_record.last_seen_time)
            new_information_received = true;
          updated_peer_record.last_seen_time = std::max(address.last_seen_time, updated_peer_record.last_seen_time);
          _potential_peer_db.update_entry(updated_peer_record);
        }
      }
      return new_information_received;
    }

    void node_impl::display_current_connections()
    {
      VERIFY_CORRECT_THREAD();
      dlog("Currently have ${current} of [${desired}/${max}] connections",
           ("current", get_number_of_connections())
           ("desired", _node_configuration.desired_number_of_connections)
           ("max", _node_configuration.maximum_number_of_connections));
      dlog("   my id is ${id}", ("id", _node_id));

      for (const peer_connection_ptr& active_connection : _active_connections)
      {
        dlog("        active: ${endpoint} with ${id}   [${direction}]",
             ("endpoint", active_connection->get_remote_endpoint())
             ("id", active_connection->node_id)
             ("direction", active_connection->direction));
      }
      for (const peer_connection_ptr& handshaking_connection : _handshaking_connections)
      {
        dlog("   handshaking: ${endpoint} with ${id}  [${direction}]",
             ("endpoint", handshaking_connection->get_remote_endpoint())
             ("id", handshaking_connection->node_id)
             ("direction", handshaking_connection->direction));
      }
    }

    // the messages a peer may send before we have accepted its hello
    static bool is_handshake_message(uint32_t message_type)
    {
      switch (message_type)
      {
      case core_message_type_enum::hello_message_type:
      case core_message_type_enum::legacy_hello_message_type:
      case core_message_type_enum::connection_accepted_message_type:
      case core_message_type_enum::connection_rejected_message_type:
      case core_message_type_enum::legacy_connection_rejected_message_type:
      case core_message_type_enum::address_request_message_type:
      case core_message_type_enum::address_message_type:
      case core_message_type_enum::legacy_address_message_type:
      case core_message_type_enum::current_time_request_message_type:
      case core_message_type_enum::current_time_reply_message_type:
      case core_message_type_enum::closing_connection_message_type:
        return true;
      default:
        return false;
      }
    }

    // the messages that relay us items or ask us for them, which keep a peer from being replaced as silent (see
    // find_silent_peers_to_replace())
    static bool exchanges_items(uint32_t message_type)
    {
      switch (message_type)
      {
      case core_message_type_enum::block_message_type:
      case core_message_type_enum::compressed_block_message_type:
      case core_message_type_enum::item_ids_inventory_message_type:
      case core_message_type_enum::fetch_blockchain_item_ids_message_type:
      case core_message_type_enum::fetch_items_message_type:
        return true;
      default:
        return false;
      }
    }

    void node_impl::on_message(peer_connection* originating_peer, const message& received_message)
    {
      VERIFY_CORRECT_THREAD();

      activity_tracer aTracer(__FUNCTION__, *this);

      if (originating_peer->their_state != peer_connection::their_connection_state::connection_accepted &&
          !is_handshake_message(received_message.msg_type))
      {
        // a legitimate peer sends its hello first, so this is a peer skipping the hello checks (or one we
        // rejected that hasn't closed yet); not acting on it (nor hashing it) keeps it off the active list,
        // and the handshake deadline closes a peer that never sends a hello
        dlog("ignoring ${type} from peer ${endpoint}: we have not accepted its hello",
             ("type", graphene::net::core_message_type_enum(received_message.msg_type))
             ("endpoint", originating_peer->get_remote_endpoint()));
        return;
      }
      if (exchanges_items(received_message.msg_type))
        originating_peer->last_item_exchange_time = fc::time_point::now();
      message_hash_type message_hash = received_message.id();
      send_message_timing_to_statsd( originating_peer, received_message, message_hash );
      dlog("handling message ${type} ${hash} size ${size} from peer ${endpoint}",
           ("type", graphene::net::core_message_type_enum(received_message.msg_type))("hash", message_hash)
           ("size", received_message.size)
           ("endpoint", originating_peer->get_remote_endpoint()));
      switch (received_message.msg_type)
      {
      // New IPv6-aware message types (5018-5022)
      case core_message_type_enum::hello_message_type:
        originating_peer->peer_supports_ipv6 = true;
        on_hello_message(originating_peer, received_message.as<hello_message>());
        break;
      case core_message_type_enum::connection_rejected_message_type:
        on_connection_rejected_message(originating_peer, received_message.as<connection_rejected_message>());
        break;
      case core_message_type_enum::address_message_type:
        on_address_message(originating_peer, received_message.as<address_message>());
        break;
      case core_message_type_enum::check_firewall_message_type:
        on_check_firewall_message(originating_peer, received_message.as<check_firewall_message>());
        break;
      case core_message_type_enum::check_firewall_reply_message_type:
        on_check_firewall_reply_message(originating_peer, received_message.as<check_firewall_reply_message>());
        break;

      // Legacy IPv4-only message types (5006, 5008, 5010, 5014, 5015)
      // These are converted to new types and dispatched to the same handlers.
      // If a new-type message was already processed, the legacy one is skipped.
      case core_message_type_enum::legacy_hello_message_type:
      {
        if (originating_peer->peer_supports_ipv6)
          break; // already processed new hello_message (5018)
        auto legacy_msg = received_message.as<legacy_hello_message>();
        hello_message converted(legacy_msg.user_agent, legacy_msg.core_protocol_version,
                                legacy_msg.inbound_address.to_address(), legacy_msg.inbound_port,
                                legacy_msg.outbound_port, legacy_msg.node_public_key,
                                legacy_msg.signed_shared_secret, legacy_msg.user_data);
        on_hello_message(originating_peer, converted);
        break;
      }
      case core_message_type_enum::legacy_connection_rejected_message_type:
      {
        auto legacy_msg = received_message.as<legacy_connection_rejected_message>();
        connection_rejected_message converted(legacy_msg.user_agent, legacy_msg.core_protocol_version,
                                              legacy_msg.remote_endpoint.to_endpoint(),
                                              legacy_msg.reason_code, legacy_msg.reason_string);
        on_connection_rejected_message(originating_peer, converted);
        break;
      }
      case core_message_type_enum::legacy_address_message_type:
      {
        auto legacy_msg = received_message.as<legacy_address_message>();
        address_message converted;
        converted.addresses.reserve(legacy_msg.addresses.size());
        for (const auto& legacy_addr : legacy_msg.addresses)
          converted.addresses.emplace_back(legacy_addr.remote_endpoint.to_endpoint(),
                                           legacy_addr.last_seen_time,
                                           legacy_addr.latency,
                                           legacy_addr.node_id,
                                           legacy_addr.direction,
                                           legacy_addr.firewalled);
        on_address_message(originating_peer, converted);
        break;
      }
      case core_message_type_enum::legacy_check_firewall_message_type:
      {
        auto legacy_msg = received_message.as<legacy_check_firewall_message>();
        check_firewall_message converted;
        converted.node_id = legacy_msg.node_id;
        converted.endpoint_to_check = legacy_msg.endpoint_to_check.to_endpoint();
        on_check_firewall_message(originating_peer, converted);
        break;
      }
      case core_message_type_enum::legacy_check_firewall_reply_message_type:
      {
        auto legacy_msg = received_message.as<legacy_check_firewall_reply_message>();
        check_firewall_reply_message converted;
        converted.node_id = legacy_msg.node_id;
        converted.endpoint_checked = legacy_msg.endpoint_checked.to_endpoint();
        converted.result = legacy_msg.result;
        on_check_firewall_reply_message(originating_peer, converted);
        break;
      }

      // Unchanged message types
      case core_message_type_enum::connection_accepted_message_type:
        on_connection_accepted_message(originating_peer, received_message.as<connection_accepted_message>());
        break;
      case core_message_type_enum::address_request_message_type:
        on_address_request_message(originating_peer, received_message.as<address_request_message>());
        break;
      case core_message_type_enum::fetch_blockchain_item_ids_message_type:
        on_fetch_blockchain_item_ids_message(originating_peer, received_message.as<fetch_blockchain_item_ids_message>());
        break;
      case core_message_type_enum::blockchain_item_ids_inventory_message_type:
        on_blockchain_item_ids_inventory_message(originating_peer, received_message.as<blockchain_item_ids_inventory_message>());
        break;
      case core_message_type_enum::fetch_items_message_type:
        on_fetch_items_message(originating_peer, received_message.as<fetch_items_message>());
        break;
      case core_message_type_enum::item_not_available_message_type:
        on_item_not_available_message(originating_peer, received_message.as<item_not_available_message>());
        break;
      case core_message_type_enum::item_busy_message_type:
        on_item_busy_message(originating_peer, received_message.as<item_busy_message>());
        break;
      case core_message_type_enum::item_ids_inventory_message_type:
        on_item_ids_inventory_message(originating_peer, received_message.as<item_ids_inventory_message>());
        break;
      case core_message_type_enum::closing_connection_message_type:
        {
          closing_connection_message closing_message_received;
          try
          {
            closing_message_received = received_message.as<closing_connection_message>();
          }
          catch (const fc::exception& e)
          {
            // The peer is closing the connection regardless of whether we can read its reason for
            // doing so, so don't treat an undecodable goodbye as a protocol error (e.g. older peers
            // embed an exception whose serialization can nest deeper than our recursion limit).
            wlog("Unable to decode closing_connection_message from peer ${peer}, treating it as a close request "
                 "with no details: ${e}",
                 ("peer", originating_peer->get_remote_endpoint())("e", e.to_detail_string()));
            closing_message_received.reason_for_closing = "<unable to decode the closing_connection_message this peer sent us>";
          }
          on_closing_connection_message(originating_peer, closing_message_received);
          break;
        }
      case core_message_type_enum::block_message_type:
      case core_message_type_enum::compressed_block_message_type:
      {
        // the read loop goes on to the next message before the block is processed
        const message_in_progress* in_progress = originating_peer->get_block_message_in_progress();
        const fc::time_point arrival_completed = in_progress ? in_progress->last_progress : fc::time_point::now();
        fc::async( [=, this]() { process_block_message(originating_peer, received_message, message_hash, arrival_completed); }, "process_block_msg");
        break;
      }
      case core_message_type_enum::trx_message_type:
        process_trx_message(originating_peer, received_message.as_trx_message( _thread_pool ));
        break;
      case core_message_type_enum::current_time_request_message_type:
        on_current_time_request_message(originating_peer, received_message.as<current_time_request_message>());
        break;
      case core_message_type_enum::current_time_reply_message_type:
        on_current_time_reply_message(originating_peer, received_message.as<current_time_reply_message>());
        break;
      case core_message_type_enum::get_current_connections_request_message_type:
        on_get_current_connections_request_message(originating_peer, received_message.as<get_current_connections_request_message>());
        break;
      case core_message_type_enum::get_current_connections_reply_message_type:
        on_get_current_connections_reply_message(originating_peer, received_message.as<get_current_connections_reply_message>());
        break;

      default:
        // ignore any message in between core_message_type_first and _last that we don't handle above
        // to allow us to add messages in the future
        if (received_message.msg_type < core_message_type_enum::core_message_type_first ||
            received_message.msg_type > core_message_type_enum::core_message_type_last)
          fc::async([=, this](){ process_ordinary_message(originating_peer, received_message, message_hash); }, "process_ord_msg");
        break;
      }
    }

    uint32_t get_block_number(const item_hash_t& id)
    {
      return fc::endian_reverse_u32(id._hash[0]);
    }


    fc::variant_object node_impl::generate_hello_user_data()
    {
      VERIFY_CORRECT_THREAD();
      // for the time being, shoehorn a bunch of properties into the user_data variant object,
      // which lets us add and remove fields without changing the protocol.  Once we
      // settle on what we really want in there, we'll likely promote them to first
      // class fields in the hello message
      fc::mutable_variant_object user_data;
      user_data["fc_git_revision_sha"] = fc::git_revision_sha;
      user_data["fc_git_revision_unix_timestamp"] = fc::git_revision_unix_timestamp;
#if defined( __APPLE__ )
      user_data["platform"] = "osx";
#elif defined( __linux__ )
      user_data["platform"] = "linux";
#elif defined( _MSC_VER )
      user_data["platform"] = "win32";
#else
      user_data["platform"] = "other";
#endif
      user_data["bitness"] = sizeof(void*) * 8;

      user_data["node_id"] = _node_id;

      item_hash_t head_block_id = _delegate->get_head_block_id();
      user_data["last_known_block_hash"] = head_block_id;
      user_data["last_known_block_number"] = _delegate->get_block_number(head_block_id);
      user_data["last_known_block_time"] = _delegate->get_block_time(head_block_id);

      if (!_hard_fork_block_numbers.empty())
        user_data["last_known_fork_block_number"] = _hard_fork_block_numbers.back();

      user_data["chain_id"] = _delegate->get_new_chain_id();
      user_data["last_available_zstd_compression_dictionary_number"] = hive::chain::get_last_available_zstd_compression_dictionary_number();
      user_data[GRAPHENE_NET_HELLO_DEFERS_TRANSACTION_REPLIES] = true;
      user_data[GRAPHENE_NET_HELLO_UNDERSTANDS_ITEM_BUSY] = true;

      return user_data;
    }
    void node_impl::parse_hello_user_data_for_peer(peer_connection* originating_peer, const fc::variant_object& user_data)
    {
      VERIFY_CORRECT_THREAD();
      // try to parse data out of the user_agent string
      if (user_data.contains("graphene_git_revision_sha"))
        originating_peer->graphene_git_revision_sha = user_data["graphene_git_revision_sha"].as_string();
      if (user_data.contains("graphene_git_revision_unix_timestamp"))
        originating_peer->graphene_git_revision_unix_timestamp = fc::time_point_sec(user_data["graphene_git_revision_unix_timestamp"].as<uint32_t>());
      if (user_data.contains("fc_git_revision_sha"))
        originating_peer->fc_git_revision_sha = user_data["fc_git_revision_sha"].as_string();
      if (user_data.contains("fc_git_revision_unix_timestamp"))
        originating_peer->fc_git_revision_unix_timestamp = fc::time_point_sec(user_data["fc_git_revision_unix_timestamp"].as<uint32_t>());
      if (user_data.contains("platform"))
        originating_peer->platform = user_data["platform"].as_string();
      if (user_data.contains("bitness"))
        originating_peer->bitness = user_data["bitness"].as<uint32_t>();
      if (user_data.contains("node_id"))
        originating_peer->node_id = user_data["node_id"].as<node_id_t>();
      if (user_data.contains("last_known_fork_block_number"))
        originating_peer->last_known_fork_block_number = user_data["last_known_fork_block_number"].as<uint32_t>();
      if (user_data.contains("chain_id"))
        originating_peer->chain_id = user_data["chain_id"].as<hive::protocol::chain_id_type>();
      if (user_data.contains("last_available_zstd_compression_dictionary_number"))
        originating_peer->last_available_zstd_compression_dictionary_number = user_data["last_available_zstd_compression_dictionary_number"].as<uint8_t>();
      if (user_data.contains("last_known_block_number"))
        originating_peer->last_known_block_number = user_data["last_known_block_number"].as<uint32_t>();
      const auto says = [&](const char* key) { return user_data.contains(key) && user_data[key].as_bool(); };
      originating_peer->received_transaction_size.set_peer_defers_replies(says(GRAPHENE_NET_HELLO_DEFERS_TRANSACTION_REPLIES));
      originating_peer->understands_item_busy = says(GRAPHENE_NET_HELLO_UNDERSTANDS_ITEM_BUSY);
    }

    void node_impl::on_hello_message( peer_connection* originating_peer, const hello_message& hello_message_received )
    {
      VERIFY_CORRECT_THREAD();
      // this already_connected check must come before we fill in peer data below
      node_id_t peer_node_id = hello_message_received.node_public_key;
      try
      {
        peer_node_id = hello_message_received.user_data["node_id"].as<node_id_t>();
      }
      catch (const fc::exception&)
      {
        // either it's not there or it's not a valid session id.  either way, ignore.
      }
      remember_own_endpoint_of_connection(originating_peer);
      if (originating_peer->direction == peer_connection_direction::outbound && originating_peer->get_remote_endpoint())
        remember_node_id_of_dialed_endpoint(*originating_peer->get_remote_endpoint(), peer_node_id);

      bool already_connected_to_this_peer = is_already_connected_to_id(peer_node_id);
      if (already_connected_to_this_peer && !rejects_duplicate_hello(_node_id, peer_node_id))
      {
        dlog("already connected to node ${id}, but its node id is lower, so it chooses which connection survives",
             ("id", peer_node_id));
        already_connected_to_this_peer = false;
      }

      // validate the node id
      fc::sha256::encoder shared_secret_encoder;
      fc::sha512 shared_secret = originating_peer->get_shared_secret();
      shared_secret_encoder.write(shared_secret.data(), sizeof(shared_secret));
      fc::ecc::public_key expected_node_public_key(hello_message_received.signed_shared_secret, shared_secret_encoder.result());

      // store off the data provided in the hello message
      originating_peer->user_agent = hello_message_received.user_agent;
      originating_peer->node_public_key = hello_message_received.node_public_key;
      originating_peer->node_id = hello_message_received.node_public_key; // will probably be overwritten in parse_hello_user_data_for_peer()
      originating_peer->core_protocol_version = hello_message_received.core_protocol_version;
      originating_peer->inbound_address = hello_message_received.inbound_address;
      originating_peer->inbound_port = hello_message_received.inbound_port;
      originating_peer->outbound_port = hello_message_received.outbound_port;

      parse_hello_user_data_for_peer(originating_peer, hello_message_received.user_data);

      // if they didn't provide a last known fork, try to guess it
      if (originating_peer->last_known_fork_block_number == 0 &&
          originating_peer->graphene_git_revision_unix_timestamp)
      {
        uint32_t unix_timestamp = originating_peer->graphene_git_revision_unix_timestamp->sec_since_epoch();
        originating_peer->last_known_fork_block_number = _delegate->estimate_last_known_fork_from_git_revision_timestamp(unix_timestamp);
      }

      // log what kind of node this is while we still have all the identifying details in one
      // place; when diagnosing network incidents it is otherwise impossible to tell after the
      // fact which software/version a given peer endpoint was running
      ilog("Received hello from ${endpoint}: user_agent '${agent}', protocol version ${version}, platform ${platform}, "
           "graphene revision ${rev} (${rev_time}), last known fork block ${fork}, node_id ${node_id}",
           ("endpoint", originating_peer->get_remote_endpoint())
           ("agent", hello_message_received.user_agent)
           ("version", hello_message_received.core_protocol_version)
           ("platform", originating_peer->platform)
           ("rev", originating_peer->graphene_git_revision_sha)
           ("rev_time", originating_peer->graphene_git_revision_unix_timestamp)
           ("fork", originating_peer->last_known_fork_block_number)
           ("node_id", originating_peer->node_id));

      // now decide what to do with it
      if (originating_peer->their_state == peer_connection::their_connection_state::just_connected)
      {
        if (hello_message_received.node_public_key != expected_node_public_key.serialize())
        {
          wlog("Invalid signature in hello message from peer ${peer}", ("peer", originating_peer->get_remote_endpoint()));
          originating_peer->their_state = peer_connection::their_connection_state::connection_rejected;
          send_connection_rejected_message(originating_peer,
                                           originating_peer->get_socket().remote_endpoint(),
                                           rejection_reason_code::invalid_hello_message,
                                           "Invalid signature in hello message");
          // for this type of message, we're immediately disconnecting this peer
          fc::exception detailed_error(FC_LOG_MESSAGE(warn, "Invalid signature in hello message"));
          disconnect_from_peer(originating_peer, "Invalid signature in hello message", true, detailed_error);
          return;
        }
        if (hello_message_received.core_protocol_version < GRAPHENE_NET_MINIMUM_PROTOCOL_VERSION)
        {
          wlog("Received hello message from peer ${peer} reporting protocol version ${version}, below the minimum ${minimum} this node accepts",
               ("peer", originating_peer->get_remote_endpoint())
               ("version", hello_message_received.core_protocol_version)
               ("minimum", GRAPHENE_NET_MINIMUM_PROTOCOL_VERSION));
          originating_peer->their_state = peer_connection::their_connection_state::connection_rejected;
          send_connection_rejected_message(originating_peer,
                                           originating_peer->get_socket().remote_endpoint(),
                                           rejection_reason_code::client_too_old,
                                           "Your p2p protocol version is older than the minimum this node accepts, please upgrade");
          fc::exception detailed_error(FC_LOG_MESSAGE(warn, "Your p2p protocol version is older than the minimum this node accepts, please upgrade"));
          disconnect_from_peer(originating_peer, "Your p2p protocol version is older than the minimum this node accepts, please upgrade", true, detailed_error);
          return;
        }
        // note: we do NOT require last_known_fork_block_number to be reported.  Although the
        // receive side has parsed it (and the obsolete-client check below has consumed it) since
        // the BitShares era, hive's own hello has never actually sent it: generate_hello_user_data
        // only includes it when _hard_fork_block_numbers is non-empty, and hive's p2p_plugin has
        // always initialized that list empty (it likewise never sets graphene_git_revision_*, so
        // the estimate fallback above can never fire either).  Requiring the field would reject
        // every hived on the network.  Making the obsolete-client mechanism real -- or deleting
        // it as vestigial -- is tracked separately.
        if (originating_peer->last_known_fork_block_number != 0)
        {
          uint32_t next_fork_block_number = get_next_known_hard_fork_block_number(originating_peer->last_known_fork_block_number);
          if (next_fork_block_number != 0)
          {
            // we know about a fork they don't.  See if we've already passed that block.  If we have, don't let them
            // connect because we won't be able to give them anything useful
            uint32_t head_block_num = _delegate->get_block_number(_delegate->get_head_block_id());
            if (next_fork_block_number < head_block_num)
            {
#ifdef ENABLE_DEBUG_ULOGS
              ulog("Rejecting connection from peer because their version is too old.  Their version date: ${date}", ("date", originating_peer->graphene_git_revision_unix_timestamp));
#endif
              wlog("Received hello message from peer running a version of that can only understand blocks up to #${their_hard_fork}, but I'm at head block number #${my_block_number}",
                   ("their_hard_fork", next_fork_block_number)("my_block_number", head_block_num));
              std::ostringstream rejection_message;
              rejection_message << "Your client is outdated -- you can only understand blocks up to #" << next_fork_block_number << ", but I'm already on block #" << head_block_num;
              originating_peer->their_state = peer_connection::their_connection_state::connection_rejected;
              send_connection_rejected_message(originating_peer,
                                               originating_peer->get_socket().remote_endpoint(),
                                               rejection_reason_code::unspecified,
                                               rejection_message.str());
              // for this type of message, we're immediately disconnecting this peer, instead of trying to
              // allowing her to ask us for peers (any of our peers will be on the same chain as us, so there's no
              // benefit of sharing them)
              fc::exception detailed_error(FC_LOG_MESSAGE(warn, "Your client is too old, please upgrade"));
              disconnect_from_peer(originating_peer, "Your client is too old, please upgrade", true, detailed_error);
              return;
            }
          }
        }
        // Only the current chain id is accepted.  We used to also accept the pre-split (legacy)
        // chain id so that a node syncing from scratch could obtain early blocks from either
        // network while there were few nodes on the new one; that bootstrap rationale has long
        // expired, and accepting the legacy id let nodes of the other network occupy connection
        // slots and a sync exchange before eventually being dropped as unreachable-fork peers.
        if ( !originating_peer->chain_id || *originating_peer->chain_id != _delegate->get_new_chain_id() )
        {
            wlog("Received hello message from peer running a node for different blockchain. My chain-id: '${my_chain_id}'. Their chain-id: '${their_chain_id}'.",
               ("my_chain_id", _delegate->get_new_chain_id())("their_chain_id", originating_peer->chain_id) );

            originating_peer->their_state = peer_connection::their_connection_state::connection_rejected;
            send_connection_rejected_message(originating_peer,
                                             originating_peer->get_socket().remote_endpoint(),
                                             rejection_reason_code::different_chain,
                                             "Your client is running a different chain id");
            // for this type of message, we're immediately disconnecting this peer, instead of trying to
            // allowing her to ask us for peers (any of our peers will be on the same chain as us, so there's no
            // benefit of sharing them)
            fc::exception detailed_error(FC_LOG_MESSAGE(warn, "Your client is on a different chain, please specify different seed nodes"));
            disconnect_from_peer(originating_peer, "Your client is on a different chain, please specify different seed nodes", true, detailed_error);
            return;
        }
        if (already_connected_to_this_peer)
        {

          originating_peer->their_state = peer_connection::their_connection_state::connection_rejected;
          if (_node_id == originating_peer->node_id)
            send_connection_rejected_message(originating_peer,
                                             originating_peer->get_socket().remote_endpoint(),
                                             rejection_reason_code::connected_to_self,
                                             "I'm connecting to myself");
          else
            send_connection_rejected_message(originating_peer,
                                             originating_peer->get_socket().remote_endpoint(),
                                             rejection_reason_code::already_connected,
                                             "I'm already connected to you");
          wlog("Received a hello_message from peer ${peer} that I'm already connected to (with id ${id}), rejection",
               ("peer", originating_peer->get_remote_endpoint())
               ("id", originating_peer->node_id));
        }
        else if (!is_exempt_from_host_limit(originating_peer) &&
                 exceeds_connections_per_host(originating_peer->get_socket().remote_endpoint().get_address(),
                                              get_connection_addresses(true, originating_peer),
                                              _node_configuration.maximum_connections_per_host) &&
                 !accept_hello_over_host_limit(originating_peer, originating_peer->get_socket().remote_endpoint().get_address()))
        {
          // a reason the dialer counts as a failed attempt, so it backs off instead of redialing every
          // peer_connection_retry_timeout while this node holds its host's connections
          originating_peer->their_state = peer_connection::their_connection_state::connection_rejected;
          std::ostringstream rejection_message;
          rejection_message << "I'm already connected to " << _node_configuration.maximum_connections_per_host << " nodes on your host";
          send_connection_rejected_message(originating_peer,
                                           originating_peer->get_socket().remote_endpoint(),
                                           rejection_reason_code::too_many_connections_from_host,
                                           rejection_message.str());
          ilog("Received a hello_message from peer ${peer}, but I hold ${limit} connections to its host already, rejection",
               ("peer", originating_peer->get_remote_endpoint())
               ("limit", _node_configuration.maximum_connections_per_host));
          warn_if_inbound_connections_share_host(originating_peer->get_socket().remote_endpoint().get_address());
        }
        else if (const fc::optional<fc::time_point> backoff_end = replaced_peer_backoff_end(originating_peer->node_id, originating_peer->get_endpoint_for_db()))
        {
          // dialing us or dialed, a peer we replaced would start a fresh window in its old slot.
          // Any rejection but a duplicate connection counts as a failed attempt, so the dialer backs off too
          const int64_t backoff_seconds = (*backoff_end - fc::time_point::now()).to_seconds() + 1;
          originating_peer->their_state = peer_connection::their_connection_state::connection_rejected;
          std::ostringstream rejection_message;
          rejection_message << "You gave me nothing I could use; I won't take you back for " << backoff_seconds << " seconds";
          send_connection_rejected_message(originating_peer,
                                           originating_peer->get_socket().remote_endpoint(),
                                           rejection_reason_code::unspecified,
                                           rejection_message.str());
          ilog("Received a hello_message from peer ${peer}, which I replaced, ${seconds}s before its backoff ends, rejection",
               ("peer", originating_peer->get_remote_endpoint())("seconds", backoff_seconds));
        }
#ifdef ENABLE_P2P_DEBUGGING_API
        else if(!_allowed_peers.empty() &&
                _allowed_peers.find(originating_peer->node_id) == _allowed_peers.end())
        {
          originating_peer->their_state = peer_connection::their_connection_state::connection_rejected;
          send_connection_rejected_message(originating_peer,
                                           originating_peer->get_socket().remote_endpoint(),
                                           rejection_reason_code::blocked,
                                           "you are not in my allowed_peers list");
          wlog( "Received a hello_message from peer ${peer} who isn't in my allowed_peers list, rejection", ("peer", originating_peer->get_remote_endpoint() ) );
        }
#endif // ENABLE_P2P_DEBUGGING_API
        else
        {
          // whether we're planning on accepting them as a peer or not, they seem to be a valid node,
          // so add them to our database if they're not firewalled

          // in the hello message, the peer sent us the IP address and port it thought it was connecting from.
          // If they match the IP and port we see, we assume that they're actually on the internet and they're not
          // firewalled.
          fc::ip::endpoint peers_actual_outbound_endpoint = originating_peer->get_socket().remote_endpoint();
          if( peers_actual_outbound_endpoint.get_address() == originating_peer->inbound_address &&
              peers_actual_outbound_endpoint.port() == originating_peer->outbound_port )
          {
            if( originating_peer->inbound_port == 0 )
            {
              dlog( "peer does not appear to be firewalled, but they did not give an inbound port so I'm treating them as if they are." );
              originating_peer->is_firewalled = firewalled_state::firewalled;
            }
            else
            {
              // peer is not firewalled, add it to our database
              fc::optional<fc::ip::endpoint> endpoint_for_db = originating_peer->get_endpoint_for_db();
              if (endpoint_for_db)
              {
                potential_peer_record updated_peer_record = _potential_peer_db.lookup_or_create_entry_for_endpoint(*endpoint_for_db);
                _potential_peer_db.update_entry(updated_peer_record);
              }
              originating_peer->is_firewalled = firewalled_state::not_firewalled;
            }
          }
          else
          {
            dlog("peer firewall state unknown: they think their outbound endpoint is ${reported_endpoint}, but I see it as ${actual_endpoint}",
                 ("reported_endpoint", fc::ip::endpoint(originating_peer->inbound_address, originating_peer->outbound_port))
                 ("actual_endpoint", peers_actual_outbound_endpoint));
            originating_peer->is_firewalled = firewalled_state::unknown;
          }

          const uint32_t our_tail_block_num = _delegate->get_tail_block_num();
          if (originating_peer->last_known_block_number &&
              peer_needs_blocks_we_pruned(*originating_peer->last_known_block_number, our_tail_block_num))
          {
            // We can't help the peer sync, so don't take one of its connection slots.  Unlike a disconnect
            // after the handshake, a rejection still lets it ask us for addresses and makes it wait before
            // dialing us again. No reason code fits and none is needed (any rejection but a duplicate
            // connection counts as a failed attempt), so the explanation is only in the text.
            std::ostringstream rejection_message;
            rejection_message << "I can't help you sync: blocks older than #" << our_tail_block_num
                              << " have been pruned on this node and you are at block #" << *originating_peer->last_known_block_number;
            originating_peer->their_state = peer_connection::their_connection_state::connection_rejected;
            send_connection_rejected_message(originating_peer,
                                             originating_peer->get_socket().remote_endpoint(),
                                             rejection_reason_code::unspecified,
                                             rejection_message.str());
            ilog("Received a hello_message from peer ${peer} at block #${peer_block}, but blocks older than #${tail} have been pruned on this node, rejection",
                 ("peer", originating_peer->get_remote_endpoint())
                 ("peer_block", *originating_peer->last_known_block_number)
                 ("tail", our_tail_block_num));
          }
          else if (!is_accepting_new_connections(originating_peer))
          {
            originating_peer->their_state = peer_connection::their_connection_state::connection_rejected;
            send_connection_rejected_message(originating_peer,
                                             originating_peer->get_socket().remote_endpoint(),
                                             rejection_reason_code::not_accepting_connections,
                                             "not accepting any more incoming connections");
            wlog("Received a hello_message from peer ${peer}, but I'm not accepting any more connections, rejection",
                 ("peer", originating_peer->get_remote_endpoint()));
          }
          else
          {
            originating_peer->their_state = peer_connection::their_connection_state::connection_accepted;
            originating_peer->allow_full_size_messages();
            originating_peer->send_message(message(connection_accepted_message()));
            dlog("Received a hello_message from peer ${peer}, sending reply to accept connection",
                 ("peer", originating_peer->get_remote_endpoint()));
          }
        }
      }
      else
      {
        // we can wind up here if we've connected to ourself, and the source and
        // destination endpoints are the same, causing messages we send out
        // to arrive back on the initiating socket instead of the receiving
        // socket.  If we did a complete job of enumerating local addresses,
        // we could avoid directly connecting to ourselves, or at least detect
        // immediately when we did it and disconnect.

        // The only way I know of that we'd get an unexpected hello that we
        //  can't really guard against is if we do a simulatenous open, we
        // probably need to think through that case.  We're not attempting that
        // yet, though, so it's ok to just disconnect here.
        wlog("unexpected hello_message from peer, disconnecting");
        fc::exception detailed_error(FC_LOG_MESSAGE(warn, "Received a unexpected hello_message"));
        disconnect_from_peer(originating_peer, "Received a unexpected hello_message", true, detailed_error);
      }
    }

    void node_impl::on_connection_accepted_message(peer_connection* originating_peer, const connection_accepted_message& connection_accepted_message_received)
    {
      VERIFY_CORRECT_THREAD();
      dlog("Received a connection_accepted in response to my \"hello\" from ${peer}", ("peer", originating_peer->get_remote_endpoint()));
      originating_peer->negotiation_status = peer_connection::connection_negotiation_status::peer_connection_accepted;
      originating_peer->our_state = peer_connection::our_connection_state::connection_accepted;
      originating_peer->send_message(address_request_message());
      fc::time_point now = fc::time_point::now();
      if (_is_firewalled == firewalled_state::unknown &&
          _last_firewall_check_message_sent < now - fc::minutes(GRAPHENE_NET_FIREWALL_CHECK_MESSAGE_INTERVAL_MINUTES) &&
          originating_peer->core_protocol_version >= GRAPHENE_NET_PROTOCOL_FIREWALL_CHECK_VERSION)
      {
        wlog("I don't know if I'm firewalled.  Sending a firewall check message to peer ${peer}",
             ("peer", originating_peer->get_remote_endpoint()));
        originating_peer->firewall_check_state = new firewall_check_state_data;

        if (originating_peer->peer_supports_ipv6)
          originating_peer->send_message(check_firewall_message());
        else
          originating_peer->send_message(legacy_check_firewall_message());
        _last_firewall_check_message_sent = now;
      }
    }

    void node_impl::on_connection_rejected_message(peer_connection* originating_peer, const connection_rejected_message& connection_rejected_message_received)
    {
      VERIFY_CORRECT_THREAD();
      if (originating_peer->our_state == peer_connection::our_connection_state::just_connected)
      {
        ilog("Received a rejection from ${peer} in response to my \"hello\", reason: \"${reason}\"",
             ("peer", originating_peer->get_remote_endpoint())
             ("reason", connection_rejected_message_received.reason_string));
        originating_peer->record_connection_closed_error(fc::exception(FC_LOG_MESSAGE(info, "Peer rejected our connection, reason: ${reason} (${code})",
                                                                                      ("reason", connection_rejected_message_received.reason_string)
                                                                                      ("code", connection_rejected_message_received.reason_code))),
                                                         rejection_is_failure(connection_rejected_message_received.reason_code));

        // the endpoint the rejecting peer reports seeing us at is not taken as one of our own: it is only the
        // peer's word, and an address we remember as ours is erased from the peer database and never dialed.
        // An address of ours that peers pass around is learned when dialing it ends in connected_to_self below

        if (connection_rejected_message_received.reason_code == rejection_reason_code::connected_to_self)
        {
          remember_own_endpoint(originating_peer->get_socket().remote_endpoint());
          move_peer_to_closing_list(originating_peer->shared_from_this());
          originating_peer->close_connection();
        }
        else if (connection_rejected_message_received.reason_code == rejection_reason_code::already_connected)
        {
          // the peer is up and we already hold a connection to it through another address, so this
          // says nothing about the dialed address and must not grow its retry backoff.  A seed whose
          // host name resolves to both an IPv4 and an IPv6 address has one database entry per
          // address and both are dialed, so one such rejection per startup is expected; recording it
          // as a failure would push the losing family further back on every restart.  The entry
          // keeps the handshaking disposition connect_to_task gave it, i.e. a flat retry delay, and the node id
          // in its hello (remember_node_id_of_dialed_endpoint()) keeps it from being dialed while that node is connected
          dlog("not counting the already_connected rejection from ${peer} against its retry backoff; it is not dialed "
               "while node ${id} stays connected", ("peer", originating_peer->get_remote_endpoint())("id", originating_peer->node_id));
        }
        else
        {
          // update our database to record that we were rejected so we won't try to connect again for a while
          // this only happens on connections we originate, so we should already know that peer is not firewalled.
          // Count the rejection like a failed attempt so the retry backoff grows.  Without
          // this, a node that rejects us (full, at its per-host limit, incompatible, ...) is redialed every
          // peer_connection_retry_timeout forever -- against a node with a reduced
          // connection limit this was measured at ~75 attempts/minute network-wide,
          // sustained for as long as the condition lasted.  A per-host limit refusal grows it only so far: another
          // node behind our address (a NAT, a shared /64) may be what fills our host's share there, and then an
          // unbounded backoff would keep us away from an honest peer long after that node left
          count_failed_connection_attempt(originating_peer, last_connection_rejected,
                                          connection_rejected_message_received.reason_code == rejection_reason_code::too_many_connections_from_host
                                            ? GRAPHENE_NET_MAX_HOST_LIMIT_REJECTIONS_COUNTED : std::numeric_limits<uint32_t>::max());
        }

        originating_peer->negotiation_status = peer_connection::connection_negotiation_status::peer_connection_rejected;
        originating_peer->our_state = peer_connection::our_connection_state::connection_rejected;
        originating_peer->send_message(address_request_message());
      }
      else
        FC_THROW( "unexpected connection_rejected_message from peer" );
    }

    void node_impl::on_address_request_message(peer_connection* originating_peer, const address_request_message& address_request_message_received)
    {
      VERIFY_CORRECT_THREAD();
      dlog("Received an address request message");

      if (originating_peer->peer_supports_ipv6)
      {
        address_message reply;
        if (!_node_configuration.peer_advertising_disabled)
        {
          reply.addresses.reserve(_active_connections.size());
          for (const peer_connection_ptr& active_peer : _active_connections)
          {
            fc::optional<fc::ip::endpoint> endpoint_for_db = active_peer->get_endpoint_for_db();
            if (endpoint_for_db)
              reply.addresses.emplace_back(address_info(*endpoint_for_db,
                                                      fc::time_point::now(),
                                                      active_peer->round_trip_delay,
                                                      active_peer->node_id,
                                                      active_peer->direction,
                                                      active_peer->is_firewalled));
          }
        }
        originating_peer->send_message(reply);
      }
      else
      {
        legacy_address_message reply;
        if (!_node_configuration.peer_advertising_disabled)
        {
          reply.addresses.reserve(_active_connections.size());
          for (const peer_connection_ptr& active_peer : _active_connections)
          {
            fc::optional<fc::ip::endpoint> endpoint_for_db = active_peer->get_endpoint_for_db();
            if (endpoint_for_db && endpoint_for_db->get_address().is_ipv4())
              reply.addresses.emplace_back(legacy_address_info(fc::ip::legacy_endpoint(*endpoint_for_db),
                                                              fc::time_point::now(),
                                                              active_peer->round_trip_delay,
                                                              active_peer->node_id,
                                                              active_peer->direction,
                                                              active_peer->is_firewalled));
          }
        }
        originating_peer->send_message(reply);
      }
    }

    void node_impl::on_address_message(peer_connection* originating_peer, const address_message& address_message_received)
    {
      VERIFY_CORRECT_THREAD();
      dlog("Received an address message containing ${size} addresses", ("size", address_message_received.addresses.size()));
      for (const address_info& address : address_message_received.addresses)
      {
        dlog("    ${endpoint} last seen ${time}", ("endpoint", address.remote_endpoint)("time", address.last_seen_time));
      }
      // the peer's remote address without a socket is none we could match a local address against
      const fc::optional<fc::ip::endpoint> relayed_from = originating_peer->get_remote_endpoint();
      std::vector<graphene::net::address_info> updated_addresses;
      for (const address_info& address : address_message_received.addresses)
        if (relayed_from && accepts_exchanged_address(address.remote_endpoint.get_address(), relayed_from->get_address()))
        {
          updated_addresses.push_back(address);
          updated_addresses.back().last_seen_time = fc::time_point_sec(fc::time_point::now());
        }
        else
          dlog("not storing ${endpoint} from ${peer}: a local address from a peer outside its network",
               ("endpoint", address.remote_endpoint)("peer", relayed_from));
      bool new_information_received = merge_address_info_with_potential_peer_database(updated_addresses);
      if (new_information_received)
        trigger_p2p_network_connect_loop();

      if (_handshaking_connections.find(originating_peer->shared_from_this()) != _handshaking_connections.end())
      {
        // if we were handshaking, we need to continue with the next step in handshaking (which is either
        // ending handshaking and starting synchronization or disconnecting)
        if( originating_peer->our_state == peer_connection::our_connection_state::connection_rejected)
          disconnect_from_peer(originating_peer, "You rejected my connection request (hello message) so I'm disconnecting");
        else if (originating_peer->their_state == peer_connection::their_connection_state::connection_rejected)
          disconnect_from_peer(originating_peer, "I rejected your connection request (hello message) so I'm disconnecting");
        else if (originating_peer->their_state != peer_connection::their_connection_state::connection_accepted)
          dlog("not activating peer ${endpoint}: it sent addresses before a hello we accepted",
               ("endpoint", originating_peer->get_remote_endpoint()));
        else
        {
          fc::optional<fc::ip::endpoint> endpoint_for_db = originating_peer->get_endpoint_for_db();
          if (endpoint_for_db)
          {
            // mark the connection as successful in the database
            fc::optional<potential_peer_record> updated_peer_record = _potential_peer_db.lookup_entry_for_endpoint(*endpoint_for_db);
            if (updated_peer_record)
            {
              updated_peer_record->last_connection_disposition = last_connection_succeeded;
              // a completed handshake clears the retry backoff accumulated from earlier
              // failures or rejections
              updated_peer_record->number_of_failed_connection_attempts = 0;
              _potential_peer_db.update_entry(*updated_peer_record);
            }
          }

          originating_peer->negotiation_status = peer_connection::connection_negotiation_status::negotiation_complete;
          move_peer_to_active_list(originating_peer->shared_from_this());
          new_peer_just_added(originating_peer->shared_from_this());
        }
      }
      // else if this was an active connection, then this was just a reply to our periodic address requests.
      // we've processed it, there's nothing else to do
    }

    void node_impl::on_fetch_blockchain_item_ids_message(peer_connection* originating_peer,
                                                         const fetch_blockchain_item_ids_message& fetch_blockchain_item_ids_message_received)
    {
      VERIFY_CORRECT_THREAD();
      item_id peers_last_item_seen = item_id(fetch_blockchain_item_ids_message_received.item_type, item_hash_t());
      if (fetch_blockchain_item_ids_message_received.blockchain_synopsis.empty())
      {
        dlog("sync: received a request for item ids starting at the beginning of the chain from peer ${peer_endpoint} (full request: ${synopsis})",
             ("peer_endpoint", originating_peer->get_remote_endpoint())
             ("synopsis", fetch_blockchain_item_ids_message_received.blockchain_synopsis));
      }
      else
      {
        item_hash_t peers_last_item_hash_seen = fetch_blockchain_item_ids_message_received.blockchain_synopsis.back();
        dlog("sync: received a request for item ids after ${last_item_seen} from peer ${peer_endpoint} (full request: ${synopsis})",
             ("last_item_seen", peers_last_item_hash_seen)
             ("peer_endpoint", originating_peer->get_remote_endpoint())
             ("synopsis", fetch_blockchain_item_ids_message_received.blockchain_synopsis));
        peers_last_item_seen.item_hash = peers_last_item_hash_seen;
      }

      // If we prune our block log, a peer that needs blocks older than our tail can't sync from us.
      // Without this check it would find none of its blocks here and get an empty reply below, making
      // both sides treat each other as in sync.
      const uint32_t peers_head_block_num = get_block_number(peers_last_item_seen.item_hash);
      const uint32_t our_tail_block_num = _delegate->get_tail_block_num();
      if (peer_needs_blocks_we_pruned(peers_head_block_num, our_tail_block_num))
      {
        disconnect_from_peer_needing_pruned_blocks(originating_peer, peers_head_block_num, our_tail_block_num);
        return;
      }

      blockchain_item_ids_inventory_message reply_message;
      reply_message.item_type = fetch_blockchain_item_ids_message_received.item_type;
      reply_message.total_remaining_item_count = 0;
      try
      {
        reply_message.item_hashes_available = _delegate->get_block_ids(fetch_blockchain_item_ids_message_received.blockchain_synopsis,
                                                                       reply_message.total_remaining_item_count);
      }
      catch (const peer_is_on_an_unreachable_fork&)
      {
        //This was true when a full block synopsis was sent by the peer, now it's not
        //dlog("Peer is on a fork and there's no set of blocks we can provide to switch them to our fork");
        dlog("We don't have any blocks that fit within the reversible synopsis sent by peer: either it is too far ahead of us (likely case) or it is on a fork that can't be recovered from");
        // we reply with an empty list as if we had an empty blockchain;
        // we don't want to disconnect because they may be able to provide
        // us with blocks on their chain
      }
      catch (const peer_needs_pruned_blocks&)
      {
        // the check above lets through peers within GRAPHENE_NET_PRUNED_TAIL_GRACE_BLOCKS of our tail, which
        // still can't be sent ids of blocks we no longer have (e.g. a peer with no blocks on a young chain)
        disconnect_from_peer_needing_pruned_blocks(originating_peer, peers_head_block_num, _delegate->get_tail_block_num());
        return;
      }

      bool disconnect_from_inhibited_peer = false;
      // if our client doesn't have any items after the item the peer requested, it will send back
      // a list containing the last item the peer requested
      //wdump((reply_message)(fetch_blockchain_item_ids_message_received.blockchain_synopsis));
      if( reply_message.item_hashes_available.empty() )
        originating_peer->peer_needs_sync_items_from_us = false; /* I have no items in my blockchain */
      else if( !fetch_blockchain_item_ids_message_received.blockchain_synopsis.empty() &&
               reply_message.item_hashes_available.size() == 1 &&
               std::find(fetch_blockchain_item_ids_message_received.blockchain_synopsis.begin(),
                         fetch_blockchain_item_ids_message_received.blockchain_synopsis.end(),
                         reply_message.item_hashes_available.back() ) != fetch_blockchain_item_ids_message_received.blockchain_synopsis.end() )
      {
        /* the last item in the peer's list matches the last item in our list */
        originating_peer->peer_needs_sync_items_from_us = false;
        if (originating_peer->inhibit_fetching_sync_blocks)
          disconnect_from_inhibited_peer = true; // delay disconnecting until after we send our reply to this fetch_blockchain_item_ids_message
      }
      else
        originating_peer->peer_needs_sync_items_from_us = true;

      if (!originating_peer->peer_needs_sync_items_from_us)
      {
        dlog("sync: peer is already in sync with us");
        // if we thought we had all the items this peer had, but now it turns out that we don't
        // have the last item it requested to send from,
        // we need to kick off another round of synchronization
        if (!originating_peer->we_need_sync_items_from_peer &&
            !fetch_blockchain_item_ids_message_received.blockchain_synopsis.empty() &&
            !_delegate->has_item(peers_last_item_seen))
        {
          dlog("sync: restarting sync with peer ${peer}", ("peer", originating_peer->get_remote_endpoint()));
          start_synchronizing_with_peer(originating_peer->shared_from_this());
        }
      }
      else
      {
        dlog("sync: peer is out of sync, sending peer ${count} items ids: first: ${first_item_id}, last: ${last_item_id}",
             ("count", reply_message.item_hashes_available.size())
             ("first_item_id", reply_message.item_hashes_available.front())
             ("last_item_id", reply_message.item_hashes_available.back()));
        if (!originating_peer->we_need_sync_items_from_peer &&
            !fetch_blockchain_item_ids_message_received.blockchain_synopsis.empty() &&
            !_delegate->has_item(peers_last_item_seen))
        {
          dlog("sync: restarting sync with peer ${peer}", ("peer", originating_peer->get_remote_endpoint()));
          start_synchronizing_with_peer(originating_peer->shared_from_this());
        }
      }
      originating_peer->send_message(reply_message);

      if (disconnect_from_inhibited_peer)
      {
        // the peer has all of our blocks, and we don't want any of theirs, so disconnect them
        fc::exception detailed_error(FC_LOG_MESSAGE(warn, "you are on a fork that I'm unable to switch to"));
        disconnect_from_peer(originating_peer, "you are on a fork that I'm unable to switch to", true, detailed_error);
        return;
      }

      //if we are handshaking with a peer that connected to us
      if (originating_peer->direction == peer_connection_direction::inbound &&
          originating_peer->their_state == peer_connection::their_connection_state::connection_accepted &&
          _handshaking_connections.find(originating_peer->shared_from_this()) != _handshaking_connections.end())
      {
        // handshaking is done, move the connection to fully active status and start synchronizing
        dlog("peer ${endpoint} which was handshaking with us has started synchronizing with us, start syncing with it",
             ("endpoint", originating_peer->get_remote_endpoint()));
        fc::optional<fc::ip::endpoint> endpoint_for_db = originating_peer->get_endpoint_for_db();
        if (endpoint_for_db && originating_peer->is_firewalled != firewalled_state::firewalled)
        {
          // mark the connection as successful in the database
          potential_peer_record updated_peer_record = _potential_peer_db.lookup_or_create_entry_for_endpoint(*endpoint_for_db);
          updated_peer_record.last_connection_disposition = last_connection_succeeded;
          // a completed handshake clears the retry backoff accumulated from earlier
          // failures or rejections
          updated_peer_record.number_of_failed_connection_attempts = 0;
          _potential_peer_db.update_entry(updated_peer_record);
        }

        // transition it to our active list
        move_peer_to_active_list(originating_peer->shared_from_this());
        new_peer_just_added(originating_peer->shared_from_this());
      }
    }

    uint32_t node_impl::calculate_unsynced_block_count_from_all_peers()
    {
      VERIFY_CORRECT_THREAD();
      uint32_t max_number_of_unfetched_items = 0;
      for( const peer_connection_ptr& peer : _active_connections )
      {
        uint32_t this_peer_number_of_unfetched_items = (uint32_t)peer->ids_of_items_to_get.size() + peer->number_of_unfetched_item_ids;
        max_number_of_unfetched_items = std::max(max_number_of_unfetched_items,
                                                 this_peer_number_of_unfetched_items);
      }
      return max_number_of_unfetched_items;
    }

    // get a blockchain synopsis that makes sense to send to the given peer.
    // If the peer isn't yet syncing with us, this is just a synopsis of our active blockchain
    // If the peer is syncing with us, it is a synopsis of our active blockchain plus the
    //    blocks the peer has already told us it has
    std::vector<item_hash_t> node_impl::create_blockchain_synopsis_for_peer(const peer_connection* peer)
    {
      VERIFY_CORRECT_THREAD();
      const item_hash_t reference_point = peer->last_block_delegate_has_seen;

      uint32_t number_of_blocks_after_reference_point = peer->ids_of_items_to_get.size();
      std::vector<item_hash_t> synopsis = _delegate->get_blockchain_synopsis(reference_point, number_of_blocks_after_reference_point);
      if (number_of_blocks_after_reference_point)
      {
        // then the synopsis is incomplete, add the missing elements from ids_of_items_to_get
        uint32_t first_block_num_in_ids_to_get = _delegate->get_block_number(peer->ids_of_items_to_get.front());
        uint32_t true_high_block_num = first_block_num_in_ids_to_get + peer->ids_of_items_to_get.size() - 1;

        // in order to generate a seamless synopsis, we need to be using the same low_block_num as the
        // backend code; the first block in the synopsis will be the low block number it used
        uint32_t low_block_num = synopsis.empty() ? 1 : _delegate->get_block_number(synopsis.front());

        do
        {
          if (low_block_num >= first_block_num_in_ids_to_get)
          {
            auto idx = low_block_num - first_block_num_in_ids_to_get;
            FC_ASSERT(idx < peer->ids_of_items_to_get.size());
            synopsis.push_back(peer->ids_of_items_to_get[idx]);
          }
          low_block_num += (true_high_block_num - low_block_num + 2 ) / 2;
        }
        while (low_block_num <= true_high_block_num);
        assert(synopsis.back() == peer->ids_of_items_to_get.back());
      }
      dlog("leaving create_blockchain_synopsis_for_peer");
      return synopsis;
    }

    void node_impl::fetch_next_batch_of_item_ids_from_peer(peer_connection* peer, bool reset_fork_tracking_data_for_peer /* = false */)
    {
      VERIFY_CORRECT_THREAD();
      //don't fetch another batch of ids while waiting for a previously requested batch
      if (peer->item_ids_requested_from_peer)
        return;

      if (reset_fork_tracking_data_for_peer)
      {
        peer->last_block_delegate_has_seen = item_hash_t();
        peer->last_block_time_delegate_has_seen = _delegate->get_block_time(item_hash_t());
      }

      fc::oexception synopsis_exception;
      bool reference_point_pruned = false;
      try
      {
        std::vector<item_hash_t> blockchain_synopsis = create_blockchain_synopsis_for_peer(peer);
        item_hash_t last_item_seen = blockchain_synopsis.empty() ? item_hash_t() : blockchain_synopsis.back();
        dlog("sync: sending a request for the next items after ${last_item_seen} to peer ${peer}, (full request is ${blockchain_synopsis})",
             (last_item_seen)
             ("peer", peer->get_remote_endpoint())
             (blockchain_synopsis));
        peer->item_ids_requested_from_peer = boost::make_tuple(blockchain_synopsis, fc::time_point::now());
        peer->send_message(fetch_blockchain_item_ids_message(_sync_item_type, blockchain_synopsis));
      }
      catch (const block_older_than_undo_history& e)
      {
        synopsis_exception = e;
      }
      catch (const synopsis_reference_point_pruned& e)
      {
        // without a reference point the synopsis starts at our head, so this can't happen twice
        if (peer->last_block_delegate_has_seen != item_hash_t())
          reference_point_pruned = true;
        else
          synopsis_exception = e;
      }
      if (synopsis_exception)
      {
        fc::exception error_for_peer(FC_LOG_MESSAGE(error, "You are on a fork I'm unable to switch to"));
        disconnect_from_peer(peer, "You are on a fork I'm unable to switch to", true, error_for_peer);
      }
      else if (reference_point_pruned)
      {
        // We no longer have the last block we know the peer has, so we can't tell where its chain
        // joins ours. That doesn't put it on another fork: drop the ids it gave us and ask again,
        // this time starting from our head.
        dlog("sync: the last block we know peer ${peer} has was pruned, asking it for block ids again",
             ("peer", peer->get_remote_endpoint()));
        peer->ids_of_items_to_get.clear();
        peer->number_of_unfetched_item_ids = 0;
        fetch_next_batch_of_item_ids_from_peer(peer, true /*reset_fork_tracking_data_for_peer*/);
      }
    }

    bool node_impl::is_item_id_being_processed(const item_hash_t& block_id)
    {
      for (const peer_connection_ptr& peer : _active_connections)
        if (peer->ids_of_items_being_processed.find(block_id) != peer->ids_of_items_being_processed.end())
          return true;
      return false;
    }

    void node_impl::on_blockchain_item_ids_inventory_message(peer_connection* originating_peer,
                                                             const blockchain_item_ids_inventory_message& blockchain_item_ids_inventory_message_received)
    {
      VERIFY_CORRECT_THREAD();
      // ignore unless we asked for the data
      if( originating_peer->item_ids_requested_from_peer )
      {
        // verify that the peer's the block ids the peer sent is a valid response to our request;
        // It should either be an empty list of blocks, or a list of blocks that builds off of one of
        // the blocks in the synopsis we sent
        if (!blockchain_item_ids_inventory_message_received.item_hashes_available.empty())
        {
          // what's more, it should be a sequential list of blocks, verify that first
          uint32_t first_block_number_in_reponse = get_block_number(blockchain_item_ids_inventory_message_received.item_hashes_available.front());
          for (unsigned i = 1; i < blockchain_item_ids_inventory_message_received.item_hashes_available.size(); ++i)
          {
            uint32_t actual_num = get_block_number(blockchain_item_ids_inventory_message_received.item_hashes_available[i]);
            uint32_t expected_num = first_block_number_in_reponse + i;
            if (actual_num != expected_num)
            {
              wlog("Invalid response from peer ${peer_endpoint}.  The list of blocks they provided is not sequential, "
                   "the ${position}th block in their reply was block number ${actual_num}, "
                   "but it should have been number ${expected_num}",
                   ("peer_endpoint", originating_peer->get_remote_endpoint())
                   ("position", i)
                   ("actual_num", actual_num)
                   ("expected_num", expected_num));
              fc::exception error_for_peer(FC_LOG_MESSAGE(error,
                                                          "You gave an invalid response to my request for sync blocks.  The list of blocks you provided is not sequential, "
                                                          "the ${position}th block in their reply was block number ${actual_num}, "
                                                          "but it should have been number ${expected_num}",
                                                          ("position", i)
                                                          ("actual_num", actual_num)
                                                          ("expected_num", expected_num)));
              disconnect_from_peer(originating_peer, "You gave an invalid response to my request for sync blocks", true, error_for_peer);
              return;
            }
          }
          const std::vector<item_hash_t>& synopsis_sent_in_request = originating_peer->item_ids_requested_from_peer->get<0>();
          const item_hash_t& first_item_hash = blockchain_item_ids_inventory_message_received.item_hashes_available.front();

          if (synopsis_sent_in_request.empty())
          {
            // if we sent an empty synopsis, we were asking for all blocks, so the first block should be block 1
            if (get_block_number(first_item_hash) != 1)
            {
              wlog("Invalid response from peer ${peer_endpoint}.  We requested a list of sync blocks starting from the beginning of the chain, "
                   "but they provided a list of blocks starting with ${first_block}",
                   ("peer_endpoint", originating_peer->get_remote_endpoint())
                   ("first_block", first_item_hash));
              fc::exception error_for_peer(FC_LOG_MESSAGE(error, "You gave an invalid response for my request for sync blocks.  I asked for blocks starting from the beginning of the chain, "
                                                          "but you returned a list of blocks starting with ${first_item_hash}",
                                                          (first_item_hash)));
              disconnect_from_peer(originating_peer, "You gave an invalid response to my request for sync blocks", true, error_for_peer);
              return;
            }
          }
          else // synopsis was not empty, we expect a response building off one of the blocks we sent
          {
            if (std::find(synopsis_sent_in_request.rbegin(), synopsis_sent_in_request.rend(), first_item_hash) == synopsis_sent_in_request.rend())
            {
              wlog("Invalid response from peer ${peer_endpoint}.  We requested a list of sync blocks based on the synopsis ${synopsis}, but they "
                   "provided a list of blocks starting with ${first_block}",
                   ("peer_endpoint", originating_peer->get_remote_endpoint())
                   ("synopsis", synopsis_sent_in_request)
                   ("first_block", first_item_hash));
              fc::exception error_for_peer(FC_LOG_MESSAGE(error, "You gave an invalid response for my request for sync blocks.  I asked for blocks following something in "
                                                          "${synopsis_sent_in_request}, but you returned a list of blocks starting with ${first_item_hash} which wasn't one of your choices",
                                                          (synopsis_sent_in_request)(first_item_hash)));
              disconnect_from_peer(originating_peer, "You gave an invalid response to my request for sync blocks", true, error_for_peer);
              return;
            }
          }
        }
        originating_peer->item_ids_requested_from_peer.reset();

        // if exceptions are throw after clearing the item_ids_requested_from_peer (above),
        // it could leave our sync in a stalled state.  Wrap a try/catch around the rest
        // of the function so we can log if this ever happens.
        try
        {
          dlog( "sync: received a list of ${count} available items from ${peer_endpoint}",
               ( "count", blockchain_item_ids_inventory_message_received.item_hashes_available.size() )
               ( "peer_endpoint", originating_peer->get_remote_endpoint() ) );
          //for( const item_hash_t& item_hash : blockchain_item_ids_inventory_message_received.item_hashes_available )
          //{
          //  dlog( "sync:     ${hash}", ("hash", item_hash ) );
          //}

          // if the peer doesn't have any items after the one we asked for
          if( blockchain_item_ids_inventory_message_received.total_remaining_item_count == 0 &&
              ( blockchain_item_ids_inventory_message_received.item_hashes_available.empty() || // there are no items in the peer's blockchain.  this should only happen if our blockchain was empty when we requested, might want to verify that.
               ( blockchain_item_ids_inventory_message_received.item_hashes_available.size() == 1 &&
                _delegate->has_item( item_id(blockchain_item_ids_inventory_message_received.item_type,
                                             blockchain_item_ids_inventory_message_received.item_hashes_available.front() ) ) ) ) && // we've already seen the last item in the peer's blockchain
              originating_peer->ids_of_items_to_get.empty() &&
              originating_peer->number_of_unfetched_item_ids == 0 ) // <-- is the last check necessary?
          {
            dlog( "sync: peer said we're up-to-date, entering normal operation with this peer" );
            originating_peer->we_need_sync_items_from_peer = false;

            uint32_t new_number_of_unfetched_items = calculate_unsynced_block_count_from_all_peers();
            _total_number_of_unfetched_items = new_number_of_unfetched_items;
            if( new_number_of_unfetched_items == 0 )
              _delegate->sync_status( blockchain_item_ids_inventory_message_received.item_type, 0 );

            return;
          }

          std::deque<item_hash_t> item_hashes_received(blockchain_item_ids_inventory_message_received.item_hashes_available.begin(),
                                                       blockchain_item_ids_inventory_message_received.item_hashes_available.end());
          originating_peer->number_of_unfetched_item_ids = blockchain_item_ids_inventory_message_received.total_remaining_item_count;

          // flush any items this peer sent us that we've already received and processed from another peer
          if (!item_hashes_received.empty() && originating_peer->ids_of_items_to_get.empty())
          {
            auto first_item_not_in_blockchain_iter = _delegate->find_first_item_not_in_blockchain(item_hashes_received);
            if (first_item_not_in_blockchain_iter == item_hashes_received.end()) //all items in blockchain
            {
              originating_peer->last_block_delegate_has_seen = item_hashes_received.back();
              originating_peer->last_block_time_delegate_has_seen = _delegate->get_block_time(originating_peer->last_block_delegate_has_seen);
              item_hashes_received.clear();
            }
            else //some items are not in blockchain
            {
              if (first_item_not_in_blockchain_iter != item_hashes_received.begin()) //if some items are in the blockchain
              {
                originating_peer->last_block_delegate_has_seen = *std::prev(first_item_not_in_blockchain_iter);
                originating_peer->last_block_time_delegate_has_seen = _delegate->get_block_time(originating_peer->last_block_delegate_has_seen);
              }
              auto find_first_item_not_seen_iter = std::partition_point(first_item_not_in_blockchain_iter, item_hashes_received.cend(), [this](const item_hash_t& item) { return is_item_id_being_processed(item); });
              //track for peer all items it has that are already being processed
              std::copy(first_item_not_in_blockchain_iter, find_first_item_not_seen_iter, std::inserter(originating_peer->ids_of_items_being_processed, originating_peer->ids_of_items_being_processed.end()));
              item_hashes_received.erase(item_hashes_received.begin(), find_first_item_not_seen_iter);
            }
            dlog("after removing all items we have already seen or are processing, item_hashes_received.size() = ${size}", ("size", item_hashes_received.size()));

            if (!item_hashes_received.empty())
            {
              originating_peer->first_id_block_number = _delegate->get_block_number(item_hashes_received.front());
              originating_peer->reset_id_search_for_peer();
            }
          }
          else if (!item_hashes_received.empty())
          {
            // we received a list of items and we already have a list of items to fetch from this peer.
            // In the normal case, this list will immediately follow the existing list, meaning the
            // last hash of our existing list will match the first hash of the new list.

            // In the much less likely case, we've received a partial list of items from the peer, then
            // the peer switched forks before sending us the remaining list.  In this case, the first
            // hash in the new list may not be the last hash in the existing list (it may be earlier, or
            // it may not exist at all.

            // In either case, pop items off the back of our existing list until we find our first
            // item, then append our list.
            while (!originating_peer->ids_of_items_to_get.empty())
            {
              if (item_hashes_received.front() != originating_peer->ids_of_items_to_get.back())
              {
                --originating_peer->last_requested_block_number_for_peers_on_this_fork;
                originating_peer->ids_of_items_to_get.pop_back();
              }
              else
                break;
            }
            //ensure we don't go back more than we have item ids to search
            if (originating_peer->last_requested_block_number_for_peers_on_this_fork < originating_peer->first_id_block_number - 1)
              originating_peer->reset_id_search_for_peer();

            if (originating_peer->ids_of_items_to_get.empty())
            {
              // this happens when the peer has switched forks between the last inventory message and
              // this one, and there weren't any unfetched items in common
              // We don't know where in the blockchain the new front() actually falls, all we can
              // expect is that it is a block that we knew about because it should be one of the
              // blocks we sent in the initial synopsis.
              originating_peer->last_block_delegate_has_seen = item_hashes_received.front();
              originating_peer->last_block_time_delegate_has_seen = _delegate->get_block_time(item_hashes_received.front());
              item_hashes_received.pop_front();
              originating_peer->first_id_block_number = _delegate->get_block_number(item_hashes_received.front());
              originating_peer->reset_id_search_for_peer();
            }
            else
            {
              // the common simple case: the new list extends the old.  pop off the duplicate element
              originating_peer->ids_of_items_to_get.pop_back();
            }
          }

          if (!item_hashes_received.empty() && !originating_peer->ids_of_items_to_get.empty())
            assert(item_hashes_received.front() != originating_peer->ids_of_items_to_get.back());

          // append the remaining items to the peer's list
          boost::push_back(originating_peer->ids_of_items_to_get, item_hashes_received);

          originating_peer->number_of_unfetched_item_ids = blockchain_item_ids_inventory_message_received.total_remaining_item_count;

          // at any given time, there's a maximum number of blocks that can possibly be out there
          // [(now - genesis time) / block interval].  If they offer us more blocks than that,
          // they must be an attacker or have a buggy client.
          fc::time_point_sec minimum_time_of_last_offered_block =
              originating_peer->last_block_time_delegate_has_seen + // timestamp of the block immediately before the first unfetched block
              originating_peer->number_of_unfetched_item_ids * HIVE_BLOCK_INTERVAL;
          if (minimum_time_of_last_offered_block > _delegate->get_blockchain_now() + GRAPHENE_NET_FUTURE_SYNC_BLOCKS_GRACE_PERIOD_SEC)
          {
            wlog("Disconnecting from peer ${peer} who offered us an implausible number of blocks, their last block would be in the future (${timestamp})",
                 ("peer", originating_peer->get_remote_endpoint())
                 ("timestamp", minimum_time_of_last_offered_block));
            fc::exception error_for_peer(FC_LOG_MESSAGE(error, "You offered me a list of more sync blocks than could possibly exist.  Total blocks offered: ${blocks}, Minimum time of the last block you offered: ${minimum_time_of_last_offered_block}, Now: ${now}",
                                                        ("blocks", originating_peer->number_of_unfetched_item_ids)
                                                        (minimum_time_of_last_offered_block)
                                                        ("now", _delegate->get_blockchain_now())));
            disconnect_from_peer(originating_peer, "You offered me a list of more sync blocks than could possibly exist", true, error_for_peer);
            return;
          }

          uint32_t new_number_of_unfetched_items = calculate_unsynced_block_count_from_all_peers();
          if (new_number_of_unfetched_items != _total_number_of_unfetched_items)
            _delegate->sync_status(blockchain_item_ids_inventory_message_received.item_type,
                                   new_number_of_unfetched_items);
          _total_number_of_unfetched_items = new_number_of_unfetched_items;

          if (blockchain_item_ids_inventory_message_received.total_remaining_item_count != 0) //node says it has more item ids to supply
          {
            // if we have enough ids from node for now, start fetching blocks
            if (originating_peer->ids_of_items_to_get.size() > GRAPHENE_NET_MIN_BLOCK_IDS_TO_PREFETCH)
            {
              trigger_fetch_sync_items_loop();
            }
            else // keep fetching the peer's list of sync items until we get enough to switch into block-fetchimg mode
              fetch_next_batch_of_item_ids_from_peer(originating_peer);
          }
          else // the peer has told us about all of the item ids it knows
          {
            // if there are block on the list let's try to fetch them
            if (!originating_peer->ids_of_items_to_get.empty())
            {
              trigger_fetch_sync_items_loop();
            }
            else
            {
              // If we get here, the peer has sent us a non-empty list of items, but we have already
              // received all of the items from other peers.  Send a new request to the peer to
              // see if we're really in sync.
              fetch_next_batch_of_item_ids_from_peer(originating_peer);
            }
          }
        }
        catch (const fc::canceled_exception&)
        {
          throw;
        }
        catch (const fc::exception& e)
        {
          elog("Caught unexpected exception: ${e}", ("e", e));
          assert(false && "exceptions not expected here");
        }
        catch (const std::exception& e)
        {
          elog("Caught unexpected exception: ${e}", ("e", e.what()));
          assert(false && "exceptions not expected here");
        }
        catch (...)
        {
          elog("Caught unexpected exception, could break sync operation");
        }
      }
      else
      {
        wlog("sync: received a list of sync items available, but I didn't ask for any!");
      }
    }

    void node_impl::on_messages_written(size_t message_count, size_t byte_count)
    {
      VERIFY_CORRECT_THREAD();
      _statistics.record_write(message_count, byte_count);
    }

    // a transaction that has left the cache is in a block many blocks back or has expired, and a peer asking for it
    // would get item_not_available
    bool node_impl::is_transaction_to_advertise(const item_hash_t& message_hash)
    {
      VERIFY_CORRECT_THREAD();
      return _transaction_message_cache.contains_item(message_hash);
    }

    // this is only called when a peer wants a block from us
    std::shared_ptr<full_block_type> node_impl::get_full_block_by_block_id(const block_id_type& block_id)
    {
      activity_tracer aTracer(__FUNCTION__, *this);

      try
      {
        return _block_message_cache.get_item_by_contents_hash(block_id);
      }
      catch (const fc::key_not_found_exception&)
      {}

      std::shared_ptr<full_block_type> full_block = _delegate->get_full_block(block_id);
      return full_block;
    }

    std::shared_ptr<full_block_type> node_impl::get_block_to_send(peer_connection* peer, const item_hash_t& requested_item)
    {
      VERIFY_CORRECT_THREAD();
      std::shared_ptr<full_block_type> full_block;
      try
      {
        // first, assume item_hash is the hash of a message.  Search our cache for the matching message
        full_block = _block_message_cache.get_item(requested_item);
        dlog("returning the block from cache with id ${id} to peer ${endpoint}",
             ("id", full_block->get_block_id())("endpoint", peer->get_remote_endpoint()));
      }
      catch (const fc::key_not_found_exception&)
      {
        // it wasn't in our local cache
      }

      // if we didn't find it, either it has fallen out of our cache, or it wasn't the hash of a message
      // at all.  If someone was syncing to us, it would be the block_id instead.  We can't tell from
      // just looking at the hash, so check our cache by block_id, and if that fails, try to get it
      // from the fork_db/block_log
      if (!full_block)
        full_block = get_full_block_by_block_id(requested_item);
      if (!full_block)
        return full_block;

      // this block may need to be compressed or decompressed before sending to the peer.  If that's the case,
      // send it over to a worker thread to get started on it
      if (peer->supports_compressed_blocks())
      {
        if (peer->requires_alternate_compression_for_block(full_block))
          _thread_pool.enqueue_work(full_block, hive::chain::blockchain_worker_thread_pool::data_source_type::block_log_destined_for_p2p_alternate_compressed); // trigger alternate compression
        else
          _thread_pool.enqueue_work(full_block, hive::chain::blockchain_worker_thread_pool::data_source_type::block_log_destined_for_p2p_compressed); // trigger default compression
      }
      else
        _thread_pool.enqueue_work(full_block, hive::chain::blockchain_worker_thread_pool::data_source_type::block_log_destined_for_p2p_uncompressed); // decompress if necessary

      // if we're sending them a block newer than the newest we already knew they had, update our
      // record of the last block they've seen.  Serving an *older* block (e.g. one they are
      // backfilling) must not regress the estimate: it feeds synopsis construction and sync
      // decisions, and regressing it makes us treat an up-to-date peer as far behind
      if (full_block->get_block_num() > _delegate->get_block_number(peer->last_block_delegate_has_seen))
      {
        peer->last_block_delegate_has_seen = full_block->get_block_id();
        peer->last_block_time_delegate_has_seen = full_block->get_block_header().timestamp;
      }
      return full_block;
    }

    void node_impl::on_fetch_items_message(peer_connection* originating_peer, const fetch_items_message& fetch_items_message_received)
    {
      VERIFY_CORRECT_THREAD();
      const size_t max_items_per_request = fetch_items_message_received.item_type == block_message_type ? GRAPHENE_NET_MAX_BLOCKS_PER_FETCH_REQUEST :
                                           fetch_items_message_received.item_type == trx_message_type ? GRAPHENE_NET_MAX_TRANSACTIONS_PER_FETCH_REQUEST :
                                           std::numeric_limits<size_t>::max();
      if (fetch_items_message_received.items_to_fetch.size() > max_items_per_request)
      {
        wlog("peer ${endpoint} asked for ${count} items of type ${type} in one request, more than the ${max} allowed, disconnecting from peer",
             ("endpoint", originating_peer->get_remote_endpoint())("count", fetch_items_message_received.items_to_fetch.size())
             ("type", fetch_items_message_received.item_type)("max", max_items_per_request));
        fc::exception detailed_error(FC_LOG_MESSAGE(error, "You asked for ${count} items of type ${type} in one request, more than the ${max} allowed",
                                                    ("count", fetch_items_message_received.items_to_fetch.size())
                                                    ("type", fetch_items_message_received.item_type)("max", max_items_per_request)));
        disconnect_from_peer(originating_peer, "You asked for more items in one request than allowed", true, detailed_error);
        return;
      }

      dlog("received items request for ids ${ids} of type ${type} from peer ${endpoint}",
           ("ids", fetch_items_message_received.items_to_fetch)
           ("type", fetch_items_message_received.item_type)
           ("endpoint", originating_peer->get_remote_endpoint()));

      // the send queue closes the connection when it would grow over its limit (GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES),
      // and nothing queued after that is sent
      const auto connection_is_closing = [originating_peer]() {
        return originating_peer->negotiation_status == peer_connection::connection_negotiation_status::closing ||
               originating_peer->negotiation_status == peer_connection::connection_negotiation_status::closed;
      };

      if (fetch_items_message_received.item_type == block_message_type)
      {
        // only the ids are queued: each block is looked up when its turn to be sent is close (get_block_to_send()),
        // so the blocks held for a peer stay a few however many it asks for, and the queue limit is reached only by
        // a request for far more blocks than a sync request asks for.  An id asked for more than once in a request
        // is answered once
        std::unordered_set<item_hash_t, p2p_item_hash> ids_queued;
        for (const item_hash_t& item_hash : fetch_items_message_received.items_to_fetch)
        {
          if (connection_is_closing())
            break;
          if (ids_queued.insert(item_hash).second)
            originating_peer->send_block_message(item_hash);
        }
      }
      else if (fetch_items_message_received.item_type == trx_message_type)
      {
        // an id asked for more than once in a request is answered once: each answer is a full copy of the
        // transaction in the send queue
        std::unordered_set<item_hash_t, p2p_item_hash> ids_answered;
        std::vector<item_hash_t> ids_to_answer;
        std::vector<std::shared_ptr<full_transaction_type>> reply_transactions;
        for (const item_hash_t& item_hash : fetch_items_message_received.items_to_fetch)
        {
          if (!ids_answered.insert(item_hash).second)
            continue;
          ids_to_answer.push_back(item_hash);
          std::shared_ptr<full_transaction_type> full_transaction;
          try
          {
            full_transaction = _transaction_message_cache.get_item(item_hash);
            dlog("received item request from peer ${endpoint}, returning the transaction from cache with id ${id}",
                 ("id", full_transaction->get_transaction_id())("endpoint", originating_peer->get_remote_endpoint()));
          }
          catch (const fc::key_not_found_exception&)
          {
            // it wasn't in our local cache
          }
          reply_transactions.push_back(std::move(full_transaction));
        }
        assert(reply_transactions.size() == ids_to_answer.size());
        // replies that would take the send queue over its reply budget wait for it to drain rather than close the
        // connection (send_transaction_reply()), so the requester gets every transaction we have, however much
        // larger they are than it expected; only a peer with far more replies (or bytes of them) waiting than any
        // window is told no.  We have those, so a peer that understands it is told we are busy, which it doesn't
        // count as withholding
        size_t refused = 0;
        for (unsigned i = 0; i < reply_transactions.size() && !connection_is_closing(); ++i)
          if (!reply_transactions[i] || !originating_peer->send_transaction_reply(reply_transactions[i]))
          {
            const item_id item(trx_message_type, ids_to_answer[i]);
            if (reply_transactions[i])
              ++refused;
            if (reply_transactions[i] && originating_peer->understands_item_busy)
              originating_peer->send_message(item_busy_message(item));
            else
              originating_peer->send_message(item_not_available_message(item));
          }
        // a peer that asks for more than it reads gets this answer to every request, so the warning is per interval
        if (refused > 0)
        {
          originating_peer->not_available_answers_since_warning += refused;
          const fc::time_point now = fc::time_point::now();
          if (originating_peer->last_not_available_warning_time == fc::time_point() ||
              now - originating_peer->last_not_available_warning_time >= fc::seconds(GRAPHENE_NET_NOT_AVAILABLE_WARNING_INTERVAL_SEC))
          {
            wlog("answered ${refused} requested transactions as not available (or busy) to peer ${endpoint} since the last such warning (${count} "
                 "in its last request): ${max} replies or ${max_bytes} bytes of them are already waiting for its send queue to drain",
                 ("refused", originating_peer->not_available_answers_since_warning)("count", reply_transactions.size())
                 ("max", GRAPHENE_NET_MAX_DEFERRED_TRANSACTION_REPLIES_PER_PEER)
                 ("max_bytes", GRAPHENE_NET_MAX_DEFERRED_TRANSACTION_REPLY_BYTES_PER_PEER)("endpoint", originating_peer->get_remote_endpoint()));
            originating_peer->last_not_available_warning_time = now;
            originating_peer->not_available_answers_since_warning = 0;
          }
        }
      }
      else
        FC_THROW("Peer requested an item that wasn't a block or a transaction");
    }

    void node_impl::on_item_not_available_message( peer_connection* originating_peer, const item_not_available_message& item_not_available_message_received )
    {
      VERIFY_CORRECT_THREAD();
      const item_id& requested_item = item_not_available_message_received.requested_item;
      //if a regular item we asked for is not available
      auto regular_item_iter = originating_peer->items_requested_from_peer.find(requested_item);
      if (regular_item_iter != originating_peer->items_requested_from_peer.end())
      {
        originating_peer->items_requested_from_peer.erase( regular_item_iter );
        forget_item_advertised_to_us(originating_peer, requested_item);
        // before handing the request to another peer, re-check that we still need the item:
        // the chain may have advanced past it while it was in flight.  (Only possible for
        // block ids -- legacy message hashes carry no block number to reason about.)
        bool still_needed = !is_block_requested_from_another_peer(originating_peer, requested_item);
        if (still_needed && requested_item.item_type == block_message_type && originating_peer->advertise_blocks_by_block_id())
          still_needed = classify_advertised_block_id(requested_item.item_hash) == block_id_classification::fetchable;
        if (still_needed)
          queue_item_to_fetch_from_advertisers(requested_item);
        dlog("Peer ${peer} doesn't have the requested item ${item}", ("peer", originating_peer->get_remote_endpoint())("item", requested_item.item_hash));
        trigger_fetch_items_loop(requested_item.item_type == block_message_type ||
                                 peer_advertised_a_queued_block(originating_peer));
        // we ask a peer only for items it advertised
        record_unavailable_advertised_item(originating_peer, requested_item);
        return;
      }

      //a sync item was not available
      auto sync_item_iter = originating_peer->sync_items_requested_from_peer.find(requested_item.item_hash);
      if (sync_item_iter != originating_peer->sync_items_requested_from_peer.end())
      {

        if (originating_peer->peer_needs_sync_items_from_us)
        {
          originating_peer->inhibit_fetching_sync_blocks = true;
          wlog("inhibit_fetching_sync_blocks from ${peer} because it didn't have an item it claimed to have",("peer",originating_peer->get_remote_endpoint()));
          //we're keeping this peer, but we need to get the item from someone else
          _active_sync_requests.erase(*sync_item_iter);
          originating_peer->sync_items_requested_from_peer.erase(sync_item_iter);
          for (const peer_connection_ptr& peer : _active_connections)
            peer->reset_id_search_for_peer(); 
        }
        else
          disconnect_from_peer(originating_peer, "You are missing a sync item you claim to have, your database is probably corrupted. Try --rebuild-index.", true,
                               fc::exception(FC_LOG_MESSAGE(error, "You are missing a sync item you claim to have, your database is probably corrupted. Try --rebuild-index.",
                                                            (requested_item))));
        wlog("Peer doesn't have the requested sync item because he must have forked and lost it");
        trigger_fetch_sync_items_loop();
        return;
      }

      dlog("Peer doesn't have an item we're looking for, which is fine because we weren't looking for it");
    }

    // The peer has the transaction we asked it for, but its replies to us are backed up: no withholding, so it isn't
    // struck, and still counts as an advertiser.  The transaction is asked of the next advertiser that can take a
    // request, and this peer is asked for no more transactions until it delivers one.  Only a peer that defers its
    // replies (GRAPHENE_NET_HELLO_DEFERS_TRANSACTION_REPLIES) has replies to us backed up, so from any other, and for a
    // block (which is never answered so), the answer is taken as not available.  A peer busy for most of the time
    // (transaction_busy_time) is demoted for transactions as if it had answered not available, but not struck: busy
    // answers would otherwise let a peer hold a window of our requests until just before they time out, again and
    // again, for one delivery each time
    void node_impl::on_item_busy_message(peer_connection* originating_peer, const item_busy_message& item_busy_message_received)
    {
      VERIFY_CORRECT_THREAD();
      const item_id& requested_item = item_busy_message_received.requested_item;
      auto item_iter = originating_peer->items_requested_from_peer.find(requested_item);
      if (requested_item.item_type != trx_message_type || item_iter == originating_peer->items_requested_from_peer.end() ||
          !originating_peer->received_transaction_size.peer_defers_replies())
      {
        on_item_not_available_message(originating_peer, item_not_available_message(requested_item));
        return;
      }
      ++_statistics.withholding.busy_answers;
      const fc::time_point now = fc::time_point::now();
      originating_peer->transaction_busy_time.record_busy(item_iter->second, now);
      originating_peer->items_requested_from_peer.erase(item_iter);
      originating_peer->transaction_replies_backlogged = true;
      if (originating_peer->transaction_busy_time.mostly_busy(now) && originating_peer->transaction_fetching_inhibited_until <= now)
      {
        dlog("peer ${peer} was busy for most of the last ${window} s: asking it for no transactions for ${demotion} s",
             ("peer", originating_peer->get_remote_endpoint())("window", GRAPHENE_NET_TRANSACTION_BUSY_WINDOW_SEC)
             ("demotion", GRAPHENE_NET_UNAVAILABLE_ITEM_DEMOTION_SEC));
        originating_peer->transaction_fetching_inhibited_until = now + fc::seconds(GRAPHENE_NET_UNAVAILABLE_ITEM_DEMOTION_SEC);
      }
      dlog("peer ${peer} is busy: its replies to us are backed up, so transaction ${item} is asked of another advertiser",
           ("peer", originating_peer->get_remote_endpoint())("item", requested_item.item_hash));
      if (queue_item_to_fetch_from_advertisers(requested_item))
        trigger_fetch_items_loop(false);
    }

    void node_impl::on_item_ids_inventory_message(peer_connection* originating_peer, const item_ids_inventory_message& item_ids_inventory_message_received)
    {
      VERIFY_CORRECT_THREAD();

      if (item_ids_inventory_message_received.item_type != block_message_type && 
          item_ids_inventory_message_received.item_type != trx_message_type)
      {
        wlog("received inventory of items of unrecognized type type ${type} from peer ${endpoint}, ignoring",
             ("type", item_ids_inventory_message_received.item_type)("endpoint", originating_peer->get_remote_endpoint()));
        return;
      }

      // a connection that is closing keeps delivering what the peer sent before it saw our disconnect, but we
      // won't send it any request, so its inventory is no source to fetch from
      const peer_connection_ptr originating_peer_ptr = originating_peer->shared_from_this();
      if (_closing_connections.find(originating_peer_ptr) != _closing_connections.end() ||
          _terminating_connections.find(originating_peer_ptr) != _terminating_connections.end())
      {
        dlog("ignoring inventory of ${count} items from peer ${endpoint}: its connection is closing",
             ("count", item_ids_inventory_message_received.item_hashes_available.size())
             ("endpoint", originating_peer->get_remote_endpoint()));
        return;
      }

      if (item_ids_inventory_message_received.item_hashes_available.size() > GRAPHENE_NET_MAX_ITEM_IDS_PER_INVENTORY_MESSAGE)
      {
        wlog("peer ${endpoint} advertised ${count} items in one inventory message, more than the ${max} allowed, disconnecting from peer",
             ("endpoint", originating_peer->get_remote_endpoint())("count", item_ids_inventory_message_received.item_hashes_available.size())
             ("max", GRAPHENE_NET_MAX_ITEM_IDS_PER_INVENTORY_MESSAGE));
        fc::exception detailed_error(FC_LOG_MESSAGE(error, "You advertised ${count} items in one inventory message, more than the ${max} allowed",
                                                    ("count", item_ids_inventory_message_received.item_hashes_available.size())
                                                    ("max", GRAPHENE_NET_MAX_ITEM_IDS_PER_INVENTORY_MESSAGE)));
        disconnect_from_peer(originating_peer, "You advertised more items in one inventory message than allowed", true, detailed_error);
        return;
      }

      // expire old inventory so we'll be making decisions our about whether to fetch blocks below based only on recent inventory
      expire_old_transaction_inventory();
      originating_peer->clear_old_inventory();

      // Peers don't advertise items to a node they are syncing, but one that can't serve our sync (e.g. a node
      // that pruned the blocks we need) treats us as in sync and does. Until we catch up we can't use current
      // transactions anyway: their TaPoS refers to blocks we don't have yet. Fast-confirm transactions skip
      // TaPoS, so they would even be accepted and record approvals of blocks far ahead of our head, which keeps
      // our last irreversible block from advancing until we reach those blocks.
      if (item_ids_inventory_message_received.item_type == trx_message_type && !_delegate->is_at_live_sync())
      {
        dlog("ignoring inventory of ${count} transactions from peer ${endpoint}: we are not at live sync",
             ("count", item_ids_inventory_message_received.item_hashes_available.size())
             ("endpoint", originating_peer->get_remote_endpoint()));
        return;
      }

      dlog("received inventory of ${count} items from peer ${endpoint}",
           ("count", item_ids_inventory_message_received.item_hashes_available.size())("endpoint", originating_peer->get_remote_endpoint()));
      if (item_ids_inventory_message_received.item_type == trx_message_type && !item_ids_inventory_message_received.item_hashes_available.empty())
        originating_peer->last_transaction_advertisement_time = fc::time_point::now();
      // so the deferred list below is only non-empty while the inventory is still full
      process_deferred_transaction_advertisements(originating_peer);
      for (const item_hash_t& item_hash : item_ids_inventory_message_received.item_hashes_available)
      {
        item_id advertised_item_id(item_ids_inventory_message_received.item_type, item_hash);
        if (_message_ids_currently_being_processed.find(item_hash) != _message_ids_currently_being_processed.end())
        {
          // we're in the middle of processing this item, no need to fetch it again (record_advertised_item() won't).
          // But if our copy of a block turns out to have a bad body, refetch_block_from_other_peers() has to
          // know this peer has the block too: it won't advertise it again
          if (item_ids_inventory_message_received.item_type == graphene::net::block_message_type &&
              !is_inventory_advertised_to_us_list_full(originating_peer))
            record_advertised_item(originating_peer, advertised_item_id);
          continue;
        }

        // do some more checks to find out if we already have this item.  If it's in our cache of recent
        // messages, there's no reason to get it again.
        if (item_ids_inventory_message_received.item_type == graphene::net::trx_message_type)
        {
          if (_transaction_message_cache.contains_item(item_hash))
            continue;
        }
        else if (item_ids_inventory_message_received.item_type == graphene::net::block_message_type)
        {
          // how we interpret the item_hash depends on the peer's protocol version
          if (originating_peer->advertise_blocks_by_block_id())
          {
            // it's a block_id: judge it against the real chain state rather than the bounded
            // recent-message cache, which forgets anything older than ~20 blocks and therefore
            // mistook stale re-advertisements for new blocks
            bool skip_this_item = false;
            switch (classify_advertised_block_id(item_hash))
            {
            case block_id_classification::known:
              skip_this_item = true; // we already have it; nothing to fetch
              break;
            case block_id_classification::dead_fork:
              dlog("ignoring advertisement of block ${hash} from ${endpoint}: it is at or below our last irreversible block and is not ours",
                   ("hash", item_hash)("endpoint", originating_peer->get_remote_endpoint()));
              skip_this_item = true;
              break;
            case block_id_classification::implausible_future:
              if (stalled_head_resync_due(originating_peer))
              {
                wlog("peer ${endpoint} advertised block ${hash} far past our head, and our head has not advanced for "
                     "${seconds}s: we have probably fallen behind, restarting sync with them",
                     ("endpoint", originating_peer->get_remote_endpoint())("hash", item_hash)
                     ("seconds", (fc::time_point::now() - _last_head_advance_time).to_seconds()));
                start_synchronizing_with_peer(originating_peer->shared_from_this());
              }
              else
                wlog("ignoring advertisement of block ${hash} from ${endpoint}: it claims a height implausibly far past our head",
                     ("hash", item_hash)("endpoint", originating_peer->get_remote_endpoint()));
              skip_this_item = true;
              break;
            case block_id_classification::fetchable:
              break;
            }
            if (skip_this_item)
              continue;
          }
          else
          {
            // then it's a legacy_block_message_hash: it has no embedded block number, so the
            // recent-message cache remains the only dedupe available for these peers
            if (_block_message_cache.contains_item(item_hash))
              continue;
          }
        }

        // if the peer has flooded us with transactions, don't add these to the inventory to prevent our
        // inventory list from growing without bound.  Transactions are deferred until the inventory has
        // room again (also when earlier ones are still deferred, to keep their order), so they are delayed
        // rather than lost.  We try to allow fetching blocks even when we've stopped fetching transactions.
        if (item_ids_inventory_message_received.item_type == graphene::net::trx_message_type &&
            (!originating_peer->transaction_inventory_deferred.empty() ||
             is_inventory_advertised_to_us_list_full_for_transactions(originating_peer)))
        {
          originating_peer->defer_transaction_advertisement(advertised_item_id);
          continue;
        }
        if (is_inventory_advertised_to_us_list_full(originating_peer))
        {
          ++originating_peer->inventory_advertisements_dropped_since_report;
          continue;
        }

        record_advertised_item(originating_peer, advertised_item_id);
      }

      process_deferred_transaction_advertisements(originating_peer);
    }

    void node_impl::record_advertised_item(peer_connection* originating_peer, const item_id& advertised_item_id)
    {
      VERIFY_CORRECT_THREAD();
      const item_hash_t& item_hash = advertised_item_id.item_hash;
      advertised_item_status status;
      status.is_block = advertised_item_id.item_type == graphene::net::block_message_type;
      status.being_processed = _message_ids_currently_being_processed.find(item_hash) != _message_ids_currently_being_processed.end();
      for (const peer_connection_ptr& peer : _active_connections)
        if (peer->items_requested_from_peer.find(advertised_item_id) != peer->items_requested_from_peer.end())
        {
          status.requested = true;
          // don't just continue; we want to record the fact that this peer also offered us the item, so if we fail
          // to get it from `peer`, we can know to ask `originating_peer` instead
          break;
        }

      status.new_from_this_peer = record_item_advertised_to_us(originating_peer, advertised_item_id);
      status.recently_failed = _recently_failed_items.find(advertised_item_id) != _recently_failed_items.end();
      auto items_to_fetch_iter = _items_to_fetch.get<item_id_index>().find(advertised_item_id);
      status.queued = items_to_fetch_iter != _items_to_fetch.get<item_id_index>().end();

      const advertised_item_fetch_decision decision = decide_advertised_item_fetch(status);
      if (status.recently_failed && !status.being_processed && !status.requested)
        dlog("not adding ${item_hash} to our list of items to fetch because we've recently fetched a copy and it failed to push", (item_hash));
      if (decision.queue)
      {
        // peers that advertised it while it was requested from another peer or being processed become candidates too
        queue_item_to_fetch_from_advertisers(advertised_item_id);
        dlog("adding item ${item_hash} from inventory message to our list of items to fetch", (item_hash));
      }
      if (decision.refresh_timestamp)
      {
        _items_to_fetch.get<item_id_index>().modify(items_to_fetch_iter,
                                                    [](prioritized_item_id& item) { item.timestamp = fc::time_point::now(); });
        if (status.new_from_this_peer)
          add_fetch_candidate(originating_peer, advertised_item_id);
      }
      if (decision.wake_fetch_loop)
        trigger_fetch_items_loop(status.is_block);
    }

    // move transaction advertisements we deferred while the peer's inventory was full into it, as far
    // as there is room now, skipping the ones we got meanwhile
    void node_impl::process_deferred_transaction_advertisements(peer_connection* peer)
    {
      VERIFY_CORRECT_THREAD();
      auto& deferred_by_time = peer->transaction_inventory_deferred.get<peer_connection::timestamp_index>();
      // like fresh transaction inventory, they're of no use to us until we're at live sync
      // (they expire from the deferred list meanwhile)
      if (!deferred_by_time.empty() && _delegate->is_at_live_sync())
      {
        while (!deferred_by_time.empty() && !is_inventory_advertised_to_us_list_full_for_transactions(peer))
        {
          item_id deferred_item_id = deferred_by_time.begin()->item;
          deferred_by_time.erase(deferred_by_time.begin());
          if (_message_ids_currently_being_processed.find(deferred_item_id.item_hash) == _message_ids_currently_being_processed.end() &&
              !_transaction_message_cache.contains_item(deferred_item_id.item_hash))
            record_advertised_item(peer, deferred_item_id);
        }
      }
      report_inventory_limit_hits(peer);
    }

    void node_impl::report_inventory_limit_hits(peer_connection* peer)
    {
      VERIFY_CORRECT_THREAD();
      if (peer->inventory_advertisements_deferred_since_report == 0 && peer->inventory_advertisements_dropped_since_report == 0)
        return;
      fc::time_point now = fc::time_point::now();
      if (now - peer->last_inventory_limit_report_time < fc::seconds(GRAPHENE_NET_INVENTORY_LIMIT_REPORT_INTERVAL_SEC))
        return;
      wlog("peer ${peer} advertised more items than our inventory limit of ${limit} transactions per ${minutes} minute(s): "
           "skipped ${skipped} advertisement(s) since the last report, ${deferred} deferred until there is room and ${dropped} dropped; "
           "${still_deferred} transaction(s) still deferred",
           ("peer", peer->get_remote_endpoint())
           ("limit", GRAPHENE_NET_MAX_INVENTORY_SIZE_IN_MINUTES * GRAPHENE_NET_MAX_TRX_PER_SECOND * 60)
           ("minutes", GRAPHENE_NET_MAX_INVENTORY_SIZE_IN_MINUTES)
           ("skipped", peer->inventory_advertisements_deferred_since_report + peer->inventory_advertisements_dropped_since_report)
           ("deferred", peer->inventory_advertisements_deferred_since_report)
           ("dropped", peer->inventory_advertisements_dropped_since_report)
           ("still_deferred", peer->transaction_inventory_deferred.size()));
      peer->inventory_advertisements_deferred_since_report = 0;
      peer->inventory_advertisements_dropped_since_report = 0;
      peer->last_inventory_limit_report_time = now;
    }

    bool node_impl::peer_advertised_item(const peer_connection* peer, const item_id& item) const
    {
      if (item.item_type == trx_message_type)
        return peer->transaction_inventory_slot && _transaction_inventory.advertised_to_us(*peer->transaction_inventory_slot, item.item_hash);
      return peer->block_inventory_peer_advertised_to_us.find(item) != peer->block_inventory_peer_advertised_to_us.end();
    }

    bool node_impl::peer_advertised_transaction(const peer_connection* peer, const transaction_inventory::record* transaction_record)
    {
      return transaction_record && peer->transaction_inventory_slot && transaction_record->advertised_to_us(*peer->transaction_inventory_slot);
    }

    // a peer that is not an active connection holds no slot: we neither fetch from it nor advertise to it
    bool node_impl::record_item_advertised_to_us(peer_connection* peer, const item_id& item)
    {
      if (item.item_type == trx_message_type)
        return peer->transaction_inventory_slot &&
               _transaction_inventory.record_advertised_to_us(*peer->transaction_inventory_slot, item.item_hash, fc::time_point::now());
      return peer->block_inventory_peer_advertised_to_us.insert(peer_connection::timestamped_item_id(item, fc::time_point::now())).second;
    }

    void node_impl::forget_item_advertised_to_us(peer_connection* peer, const item_id& item)
    {
      if (item.item_type != trx_message_type)
        peer->block_inventory_peer_advertised_to_us.erase(item);
      else if (peer->transaction_inventory_slot)
        _transaction_inventory.forget_advertised_to_us(*peer->transaction_inventory_slot, item.item_hash);
    }

    uint32_t node_impl::inventory_advertised_to_us_size(const peer_connection* peer) const
    {
      const uint32_t transactions = peer->transaction_inventory_slot ? _transaction_inventory.advertised_to_us_count(*peer->transaction_inventory_slot) : 0;
      return transactions + (uint32_t)peer->block_inventory_peer_advertised_to_us.size();
    }

    bool node_impl::is_inventory_advertised_to_us_list_full_for_transactions(const peer_connection* peer) const
    {
      return inventory_advertised_to_us_size(peer) > GRAPHENE_NET_MAX_TRANSACTION_INVENTORY_PER_PEER;
    }

    bool node_impl::is_inventory_advertised_to_us_list_full(const peer_connection* peer) const
    {
      // allow the total inventory size to be the maximum number of transactions we'll store in the inventory (above)
      // plus the maximum number of blocks that would be generated in GRAPHENE_NET_MAX_INVENTORY_SIZE_IN_MINUTES (plus one,
      // to give us some wiggle room)
      return inventory_advertised_to_us_size(peer) >
        GRAPHENE_NET_MAX_TRANSACTION_INVENTORY_PER_PEER +
        (GRAPHENE_NET_MAX_INVENTORY_SIZE_IN_MINUTES + 1) * 60 / HIVE_BLOCK_INTERVAL;
    }

    void node_impl::expire_old_transaction_inventory()
    {
      _transaction_inventory.expire(fc::time_point_sec(fc::time_point::now() - fc::minutes(GRAPHENE_NET_MAX_INVENTORY_SIZE_IN_MINUTES)));
    }

    void node_impl::release_transaction_inventory_slot(peer_connection* peer)
    {
      if (peer->transaction_inventory_slot)
        _transaction_inventory.release_slot(*std::exchange(peer->transaction_inventory_slot, std::nullopt));
    }

    void node_impl::on_closing_connection_message( peer_connection* originating_peer, const closing_connection_message& closing_connection_message_received )
    {
      VERIFY_CORRECT_THREAD();
      originating_peer->they_have_requested_close = true;

      if( closing_connection_message_received.closing_due_to_error )
      {
        elog( "Peer ${peer} is disconnecting us because of an error: ${msg}, exception: ${error}",
             ( "peer", originating_peer->get_remote_endpoint() )
             ( "msg", closing_connection_message_received.reason_for_closing )
             ( "error", closing_connection_message_received.error ) );
        std::ostringstream message;
        message << "Peer " << fc::variant( originating_peer->get_remote_endpoint() ).as_string() <<
                  " disconnected us: " << closing_connection_message_received.reason_for_closing;
        fc::exception detailed_error(FC_LOG_MESSAGE(warn, "Peer ${peer} is disconnecting us because of an error: ${msg}, exception: ${error}",
                                                    ( "peer", originating_peer->get_remote_endpoint() )
                                                    ( "msg", closing_connection_message_received.reason_for_closing )
                                                    ( "error", closing_connection_message_received.error ) ));
        _delegate->error_encountered( message.str(),
                                      detailed_error );
        originating_peer->record_connection_closed_error( detailed_error, true );
      }
      else
      {
        wlog( "Peer ${peer} is disconnecting us because: ${msg}",
             ( "peer", originating_peer->get_remote_endpoint() )
             ( "msg", closing_connection_message_received.reason_for_closing ) );
        originating_peer->record_connection_closed_error( fc::exception( FC_LOG_MESSAGE( info, "Peer disconnected us: ${msg}",
                                                                                           ( "msg", closing_connection_message_received.reason_for_closing ) ) ),
                                                          false );
      }
      if( originating_peer->we_have_requested_close )
        originating_peer->close_connection();
      }

    void node_impl::on_connection_closed(peer_connection* originating_peer)
    {
      VERIFY_CORRECT_THREAD();

      activity_tracer aTracer(__FUNCTION__, *this);

      peer_connection_ptr originating_peer_ptr = originating_peer->shared_from_this();
      _rate_limiter.remove_tcp_socket( &originating_peer->get_socket() );

      //update peer_db record if possible
      fc::optional<fc::ip::endpoint> endpoint_for_db = originating_peer->get_endpoint_for_db();
      if (endpoint_for_db)
      {
        fc::optional<potential_peer_record> updated_peer_record = _potential_peer_db.lookup_entry_for_endpoint(*endpoint_for_db);
        if (updated_peer_record)
        {
          originating_peer->record_connection_closed_in(*updated_peer_record);
          _potential_peer_db.update_entry(*updated_peer_record);
        }
      }

      //remove peer from global lists
      _closing_connections.erase(originating_peer_ptr);
      _handshaking_connections.erase(originating_peer_ptr);
      _terminating_connections.erase(originating_peer_ptr);
      release_transaction_inventory_slot(originating_peer);
      if (_active_connections.find(originating_peer_ptr) != _active_connections.end())
      {
        _active_connections.erase(originating_peer_ptr);

        if (endpoint_for_db)
        {
          fc::optional<potential_peer_record> updated_peer_record = _potential_peer_db.lookup_entry_for_endpoint(*endpoint_for_db);
          if (updated_peer_record)
          {
            updated_peer_record->last_seen_time = fc::time_point::now();
            _potential_peer_db.update_entry(*updated_peer_record);
          }
        }
      }

      ilog("Remote peer ${endpoint} closed their connection to us", ("endpoint", originating_peer->get_remote_endpoint()));
      display_current_connections();
      trigger_p2p_network_connect_loop();

      // notify the node delegate so it can update the display
      if( _active_connections.size() != _last_reported_number_of_connections )
      {
        _last_reported_number_of_connections = (uint32_t)_active_connections.size();
        _delegate->connection_count_changed( _last_reported_number_of_connections );
      }

      // if we had delegated a firewall check to this peer, send it to another peer
      if (originating_peer->firewall_check_state)
      {
        if (originating_peer->firewall_check_state->requesting_peer != node_id_t())
        {
          // it's a check we're doing for another node
          firewall_check_state_data* firewall_check_state = originating_peer->firewall_check_state;
          originating_peer->firewall_check_state = nullptr;
          forward_firewall_check_to_next_available_peer(firewall_check_state);
        }
        else
        {
          // we were asking them to check whether we're firewalled.  we'll just let it
          // go for now
          delete originating_peer->firewall_check_state;
        }
      }

      // if we had requested any sync or regular items from this peer that we haven't
      // received yet, reschedule them to be fetched from another peer
      if (!originating_peer->sync_items_requested_from_peer.empty())
      {
        for (const auto& sync_item : originating_peer->sync_items_requested_from_peer)
          _active_sync_requests.erase(sync_item);
        for (const peer_connection_ptr& peer : _active_connections)
          peer->reset_id_search_for_peer();
        trigger_fetch_sync_items_loop();
      }

      originating_peer->fetch_candidates.clear();
      if (!originating_peer->items_requested_from_peer.empty())
      {
        bool block_requeued = false;
        for (const auto& item_and_time : originating_peer->items_requested_from_peer)
        {
          // as in on_item_not_available_message: don't reschedule fetches for blocks the chain
          // has already moved past while the request was in flight with the dead peer
          // nor for one asked of another peer already, when this one kept us waiting (rerequest_overdue_blocks())
          bool still_needed = !is_block_requested_from_another_peer(originating_peer, item_and_time.first);
          if (still_needed && item_and_time.first.item_type == block_message_type && originating_peer->advertise_blocks_by_block_id())
            still_needed = classify_advertised_block_id(item_and_time.first.item_hash) == block_id_classification::fetchable;
          if (still_needed && queue_item_to_fetch_from_advertisers(item_and_time.first))
            block_requeued = block_requeued || item_and_time.first.item_type == block_message_type;
        }
        trigger_fetch_items_loop(block_requeued);
      }

      discard_abandoned_sync_blocks();

      schedule_peer_for_deletion(originating_peer_ptr);
    } //on_connection_closed

    void node_impl::refresh_cached_chain_state()
    {
      VERIFY_CORRECT_THREAD();
      const uint32_t head_block_num = _delegate->get_block_number(_delegate->get_head_block_id());
      if (head_block_num > _cached_head_block_num)
      {
        _cached_head_block_num = head_block_num;
        _last_head_advance_time = fc::time_point::now();
      }
      _cached_last_irreversible_block_num = std::max(_cached_last_irreversible_block_num, _delegate->get_last_irreversible_block_num());
    }

    // Judge an advertised block id against the actual chain state.  Only valid for hashes that
    // really are block ids (protocol >= GRAPHENE_NET_PROTOCOL_ADVERTISE_BLOCKS_BY_BLOCK_ID_VERSION
    // peers); legacy block-message hashes have no embedded block number and must not be passed
    // here.  This replaces dedupe decisions previously made against the bounded recent-message
    // cache, whose ~20-block horizon made anything older look brand new -- the root enabler of
    // the stale-refetch cascades in the 2026-07-09 incident.
    node_impl::block_id_classification node_impl::classify_advertised_block_id(const item_hash_t& block_id_hash)
    {
      VERIFY_CORRECT_THREAD();
      const uint32_t block_num = _delegate->get_block_number(block_id_hash); // decoded from the id itself
      if (block_num == 0)
        return block_id_classification::fetchable; // not decodable; let the fetch machinery sort it out
      refresh_cached_chain_state();
      if (block_num > _cached_head_block_num + GRAPHENE_NET_FUTURE_BLOCK_IDS_GRACE_BLOCKS)
        return block_id_classification::implausible_future;
      if (_delegate->has_item(item_id(block_message_type, block_id_hash)))
        return block_id_classification::known;
      if (block_num <= _cached_last_irreversible_block_num)
        return block_id_classification::dead_fork;
      return block_id_classification::fetchable;
    }

    // Decide whether an implausible_future advertisement from this peer should restart sync
    // with it instead of being dropped.  The far-future filter is right when our head is
    // moving; when it has been stuck for GRAPHENE_NET_STALLED_HEAD_RESYNC_SEC and a peer is
    // still telling us about blocks past the grace window, we have most likely fallen behind
    // (a chain-thread stall of 2+ minutes at 3 s blocks is enough) and the filter would
    // otherwise drop every advert until a new peer connected.  Rate-limited per peer so a
    // peer fabricating far-future ids costs us one synopsis exchange per interval at most.
    bool node_impl::stalled_head_resync_due(peer_connection* peer)
    {
      VERIFY_CORRECT_THREAD();
      if (_cached_head_block_num == 0 || _last_head_advance_time == fc::time_point())
        return false; // no head observed yet; startup sync handles that
      const fc::time_point now = fc::time_point::now();
      if (now - _last_head_advance_time < fc::seconds(GRAPHENE_NET_STALLED_HEAD_RESYNC_SEC))
        return false; // head is moving (or only momentarily behind), the filter's assumption holds
      if (peer->we_need_sync_items_from_peer)
        return false; // a sync with this peer is already under way; let it finish
      if (now - peer->last_stalled_head_resync_time < fc::seconds(GRAPHENE_NET_STALLED_HEAD_RESYNC_SEC))
        return false; // already restarted with this peer recently
      peer->last_stalled_head_resync_time = now;
      return true;
    }

    // The highest block number we know the peer has: the head it reported in its hello, the last block of
    // ours it told us it has, or the last id it offered us for sync.  0 when it told us nothing
    uint32_t node_impl::highest_block_number_known_for_peer(const peer_connection& peer)
    {
      uint32_t highest = peer.last_known_block_number ? *peer.last_known_block_number : 0;
      if (peer.last_block_delegate_has_seen != item_hash_t())
        highest = std::max(highest, get_block_number(peer.last_block_delegate_has_seen));
      if (!peer.ids_of_items_to_get.empty())
        highest = std::max(highest, peer.first_id_block_number + (uint32_t)peer.ids_of_items_to_get.size() - 1 +
                                    peer.number_of_unfetched_item_ids);
      return highest;
    }

    // stalled_head_resync_due() restarts sync only on a far-future advertisement, but a peer that has
    // seen our synopsis and knows we are behind advertises nothing to us: it waits for our requests.
    // If sync with such peers stopped (e.g. it was set up while the chain was stalled and could not
    // accept blocks), nothing else asks them again.  So once our head has not advanced for
    // GRAPHENE_NET_STALLED_HEAD_RESYNC_SEC and nothing we asked for is outstanding, restart sync with
    // every active peer that may be ahead of us, at most once per that interval per peer.  A peer
    // whose head we know to be at or below ours is left alone, so an in-sync node on a quiet chain
    // doesn't restart sync over and over
    void node_impl::restart_sync_if_head_stalled()
    {
      VERIFY_CORRECT_THREAD();
      if (_active_connections.empty())
        return;
      refresh_cached_chain_state();
      if (_cached_head_block_num == 0 || _last_head_advance_time == fc::time_point())
        return; // no head observed yet; startup sync handles that
      const fc::time_point now = fc::time_point::now();
      if (now - _last_head_advance_time < fc::seconds(GRAPHENE_NET_STALLED_HEAD_RESYNC_SEC))
        return;
      // blocks still being handed to the chain mean it is busy (or stalled itself): asking peers for
      // more can't help until it takes them
      if (get_number_of_handle_message_calls_in_progress() != 0)
        return;
      std::vector<peer_connection_ptr> peers_to_resync;
      for (const peer_connection_ptr& peer : _active_connections)
      {
        if (peer->item_ids_requested_from_peer || !peer->sync_items_requested_from_peer.empty())
          return; // a sync request is outstanding: it either completes or times out the peer
        const uint32_t peer_head = highest_block_number_known_for_peer(*peer);
        if ((peer_head == 0 || peer_head > _cached_head_block_num) &&
            now - peer->last_stalled_head_resync_time >= fc::seconds(GRAPHENE_NET_STALLED_HEAD_RESYNC_SEC))
          peers_to_resync.push_back(peer);
      }
      if (peers_to_resync.empty())
        return;
      wlog("restarting sync: head stalled for ${seconds}s with ${count} peers",
           ("seconds", (now - _last_head_advance_time).to_seconds())("count", peers_to_resync.size()));
      for (const peer_connection_ptr& peer : peers_to_resync)
      {
        peer->last_stalled_head_resync_time = now;
        start_synchronizing_with_peer(peer);
      }
      // sync blocks already received are pushed from the backlog, which otherwise runs only when a
      // block arrives or the chain finishes one; the restarted sync won't fetch them again
      trigger_process_backlog_of_sync_blocks();
    }

    // The peer delivered a block that we pushed (not one only stored on a shorter fork): it is useful to us, and
    // some peer can serve us (see expand_peers_while_starved()).  Advertising a block, offering block ids or
    // claiming a head in a hello costs a peer nothing, and asking us for blocks gives us nothing, so none of them
    // counts: only a delivery does
    void node_impl::record_block_delivered_by(peer_connection* peer)
    {
      VERIFY_CORRECT_THREAD();
      peer->last_useful_data_time = fc::time_point::now();
      peer->delivered_block = true;
      _last_block_from_any_peer_time = peer->last_useful_data_time;
    }

    // The peer delivered an item we asked it for and it passed validation: a transaction the chain accepted
    // (apply_transaction_completion()), or a block we pushed onto the chain we follow, however late.  It withholds nothing, so its strikes (unavailable_item_strikes) are cleared.  Receiving an
    // item doesn't: a peer could otherwise clear its strikes with a fabricated transaction under an id it advertised.
    // Nor does a copy of a block we have already, or only store on a shorter fork: it gives us nothing
    void node_impl::record_requested_delivery(peer_connection* peer)
    {
      VERIFY_CORRECT_THREAD();
      peer->unavailable_item_strikes.clear();
    }

    // A node can hold a full set of peers that answer keepalives and give it nothing else, and the connect loop dials
    // nobody while the slots are full.  So once our head has been behind, with no peer delivering us a block that we
    // pushed (see record_block_delivered_by()), for peer_expansion_timeout, the node takes up to
    // GRAPHENE_NET_PEER_EXPANSION_EXTRA_CONNECTIONS connections past those it holds (see
    // get_desired_number_of_connections()) for one more timeout, and the connect loop dials candidates for them.  That
    // alone disconnects nobody: a head claimed in a hello, offered block ids or advertised blocks cost a peer nothing,
    // and dropping peers on their strength would let one peer get honest ones dropped.  Once a peer taken up for the
    // expansion delivers a block, end_peer_expansion() closes the peers we had that delivered none; if none does, it
    // closes the peers taken up that delivered nothing, and the ones we had are kept.  The next expansion comes a whole
    // timeout after one ends at the soonest.  The time counts only since we fell behind: a node in sync gets each
    // block from whichever peer is first, so most of its peers never deliver one.  Nor does it count while blocks we
    // handed the chain wait on it: the peers are not why we are behind then.  The timeout is longer than
    // GRAPHENE_NET_STALLED_HEAD_RESYNC_SEC, so restart_sync_if_head_stalled() asks the peers we have first
    void node_impl::expand_peers_while_starved()
    {
      VERIFY_CORRECT_THREAD();
      const fc::time_point now = fc::time_point::now();
      const fc::microseconds timeout = fc::seconds(_node_configuration.peer_expansion_timeout);
      if (_peer_expansion_target != 0)
      {
        const bool new_peer_delivered = std::any_of(_active_connections.begin(), _active_connections.end(), [this](const peer_connection_ptr& peer) {
          return peer->delivered_block && is_taken_up_for_peer_expansion(*peer);
        });
        if (new_peer_delivered || now - _peer_expansion_started >= timeout)
          end_peer_expansion(new_peer_delivered);
        return;
      }
      if (timeout.count() == 0)
        return;
      refresh_cached_chain_state();
      const fc::time_point head_block_time = _delegate->get_block_time(_delegate->get_head_block_id());
      const bool head_behind = head_block_time < now - fc::seconds(GRAPHENE_NET_IDLE_PEERS_HEAD_BEHIND_SEC);
      const bool chain_held = get_number_of_handle_message_calls_in_progress() != 0 &&
                              now - _last_head_advance_time >= fc::seconds(_recent_block_interval_in_seconds);
      if (!head_behind || chain_held)
      {
        _behind_since = fc::time_point();
        return;
      }
      if (_behind_since == fc::time_point())
        _behind_since = now;
      const fc::time_point starved_since = std::max(_behind_since, _last_block_from_any_peer_time);
      if (now - starved_since < timeout || (_peer_expansion_ended != fc::time_point() && now - _peer_expansion_ended < timeout))
        return;

      const uint32_t held = get_number_of_connections_holding_slots();
      _peer_expansion_started = now;
      _peer_expansion_starved_since = starved_since;
      _peer_expansion_target = std::min(std::max(held, _node_configuration.desired_number_of_connections) + GRAPHENE_NET_PEER_EXPANSION_EXTRA_CONNECTIONS,
                                        _node_configuration.maximum_number_of_connections);
      uint32_t unroutable_ipv4_candidates = 0;
      uint32_t unroutable_ipv6_candidates = 0;
      const uint32_t extra = _peer_expansion_target > held ? _peer_expansion_target - held : 0;
      const size_t candidates = select_connection_candidates(get_connection_candidates(unroutable_ipv4_candidates, unroutable_ipv6_candidates),
                                                             get_connection_addresses(false), get_connection_selection_limits(extra)).size();
      wlog("no peer delivered a block for ${seconds}s while our head #${head} is behind: taking up to ${extra} peers past the "
           "${held} connections held for ${timeout}s, ${candidates} candidates to dial now; peers we have are closed only if a new one delivers blocks",
           ("seconds", (now - starved_since).to_seconds())("head", _cached_head_block_num)(extra)(held)
           ("timeout", _node_configuration.peer_expansion_timeout)(candidates));
      trigger_p2p_network_connect_loop();
    }

    // whether @p peer connected while the running peer expansion did, so it holds one of its slots
    bool node_impl::is_taken_up_for_peer_expansion(const peer_connection& peer) const
    {
      return peer.connection_initiation_time >= _peer_expansion_started;
    }

    // Ends the running peer expansion (see expand_peers_while_starved()) and returns to desired_number_of_connections,
    // never below it.  When a peer taken up for it delivered a block (@p new_peer_delivered), the peers we had that
    // delivered no block since we were starved of them are closed and may not take a slot again for one
    // peer_expansion_timeout, the longest idle first; when none did, the peers taken up that delivered nothing are
    // closed.  A peer we are waiting for sync blocks from is judged by the request timeouts instead
    void node_impl::end_peer_expansion(bool new_peer_delivered)
    {
      VERIFY_CORRECT_THREAD();
      const fc::time_point now = fc::time_point::now();
      idle_peer_sweep sweep;
      sweep.now = now;
      sweep.window = now - _peer_expansion_starved_since;
      sweep.kept_at_least = std::max<size_t>(GRAPHENE_NET_IDLE_PEERS_KEPT_AT_LEAST, _node_configuration.desired_number_of_connections);
      std::vector<peer_connection_ptr> peers;
      std::vector<idle_peer_candidate> candidates;
      for (const peer_connection_ptr& peer : _active_connections)
      {
        idle_peer_candidate candidate;
        // a peer that never delivered a block is idle however recently it connected
        candidate.last_useful_data_time = peer->delivered_block ? peer->last_useful_data_time : fc::time_point();
        // settling closes peers we had, otherwise only peers taken up for the expansion are closed
        candidate.exempt = is_taken_up_for_peer_expansion(*peer) == new_peer_delivered || !peer->sync_items_requested_from_peer.empty();
        peers.push_back(peer);
        candidates.push_back(candidate);
      }
      const std::vector<size_t> selected = select_idle_peers_to_disconnect(candidates, sweep);
      const uint32_t timeout = _node_configuration.peer_expansion_timeout;
      if (new_peer_delivered)
        wlog("a peer taken up while no peer delivered us blocks delivered one: closing ${count} of the peers we had that delivered "
             "none for ${seconds}s, down to the ${desired} desired connections; they may not take a slot again for ${timeout}s",
             ("count", selected.size())("seconds", sweep.window.to_seconds())
             ("desired", _node_configuration.desired_number_of_connections)(timeout));
      else
        ilog("none of the peers taken up in ${timeout}s while no peer delivered us blocks delivered one: closing ${count} of them, "
             "keeping the peers we had; the next expansion is ${timeout}s away at the soonest",
             (timeout)("count", selected.size()));
      _peer_expansion_target = 0;
      _peer_expansion_started = fc::time_point();
      _peer_expansion_ended = now;
      for (size_t i : selected)
      {
        peer_connection* peer = peers[i].get();
        dlog("closing ${peer} at the end of a peer expansion: it delivered us no block", ("peer", peer->get_remote_endpoint()));
        if (new_peer_delivered)
        {
          back_off_replaced_peer(peer, timeout);
          fc::exception detailed_error(FC_LOG_MESSAGE(warn, "You gave me no blocks for ${seconds} seconds while I was behind and a new peer gave me some",
                                                      ("seconds", sweep.window.to_seconds())));
          disconnect_from_peer(peer, "You gave me nothing I could use while I was behind", true, detailed_error);
        }
        else
          disconnect_from_peer(peer, "I took you up to find blocks, and you gave me none");
        count_failed_connection_attempt(peer, last_connection_failed);
      }
    }

    // The peer answered item_not_available for @p item, which it advertised and we asked it for.  It is demoted (no
    // transactions are asked of it for GRAPHENE_NET_UNAVAILABLE_ITEM_DEMOTION_SEC, or a block only if no other peer has
    // it for GRAPHENE_NET_BLOCK_FETCHING_DEMOTION_SEC), which counts as a strike; a peer that is only busy says so
    // instead (on_item_busy_message()); at GRAPHENE_NET_MAX_UNAVAILABLE_ITEM_STRIKES strikes it is disconnected (see
    // unavailable_item_strikes: answers while it is demoted, the rest of one request say, are no new strike, and
    // strikes decay).  Advertising items it doesn't have and answering just before the request timeout would otherwise
    // let a peer hold our requests, and the fetch gate's headroom, for nothing, again and again
    void node_impl::record_unavailable_advertised_item(peer_connection* peer, const item_id& item)
    {
      VERIFY_CORRECT_THREAD();
      const fc::time_point now = fc::time_point::now();
      const fc::microseconds demotion = fc::seconds(GRAPHENE_NET_UNAVAILABLE_ITEM_DEMOTION_SEC);
      ++_statistics.withholding.not_available_answers;
      if (item.item_type == block_message_type)
        peer->deprioritize_block_fetching();
      else
        peer->transaction_fetching_inhibited_until = std::max(peer->transaction_fetching_inhibited_until, now + demotion);
      record_withholding_strike(peer, item);
    }

    // The peer completed its copy of @p item, a block we asked it for, after its request's deadline (see
    // block_request_deadline()).  It is deprioritized for block fetching for GRAPHENE_NET_BLOCK_FETCHING_DEMOTION_SEC,
    // so other advertisers are asked first next time, and takes no strike: a copy that is slow because our own link
    // is slow can't be told from one that is withheld, and a peer that is slow is asked later, not punished.  One that
    // never sends the block is dropped at its request's timeout (requested_items::timed_out())
    void node_impl::record_late_block_copy(peer_connection* peer, const item_id& item)
    {
      VERIFY_CORRECT_THREAD();
      ++_statistics.withholding.late_block_copies;
      peer->deprioritize_block_fetching();
      dlog("peer ${peer} sent block ${item} past its request's deadline: asking for blocks elsewhere first",
           ("peer", peer->get_remote_endpoint())("item", item.item_hash));
    }

    // Counts a strike against @p peer for answering item_not_available for @p item, which it advertised (see
    // unavailable_item_strikes): at GRAPHENE_NET_MAX_UNAVAILABLE_ITEM_STRIKES it is disconnected.  Every strike is
    // logged at debug level and counted in the p2p statistics; a warning names one at most once per
    // GRAPHENE_NET_WITHHOLDING_WARNING_INTERVAL_SEC, with how many strikes and disconnects came since the last one
    void node_impl::record_withholding_strike(peer_connection* peer, const item_id& item)
    {
      VERIFY_CORRECT_THREAD();
      const fc::time_point now = fc::time_point::now();
      if (!peer->unavailable_item_strikes.record(now))
        return;
      ++_statistics.withholding.strikes;
      ++_strikes_since_withholding_warning;
      const uint32_t strikes = peer->unavailable_item_strikes.count(now);
      const bool disconnecting = peer->unavailable_item_strikes.exhausted(now);
      if (disconnecting)
      {
        ++_statistics.withholding.disconnects;
        ++_strike_disconnects_since_withholding_warning;
      }
      dlog("peer ${peer} withheld item ${item} it advertised: it answered item_not_available (strike ${strikes} of ${max})",
           ("peer", peer->get_remote_endpoint())("item", item.item_hash)(strikes)("max", GRAPHENE_NET_MAX_UNAVAILABLE_ITEM_STRIKES));
      if (_last_withholding_warning_time == fc::time_point() ||
          now - _last_withholding_warning_time >= fc::seconds(GRAPHENE_NET_WITHHOLDING_WARNING_INTERVAL_SEC))
      {
        wlog("struck peer ${peer} for withholding an item it advertised: it answered item_not_available (strike ${strikes} of ${max}${disconnected}); "
             "${all_strikes} strikes and ${disconnects} disconnects for strikes since the last such warning",
             ("peer", peer->get_remote_endpoint())(strikes)("max", GRAPHENE_NET_MAX_UNAVAILABLE_ITEM_STRIKES)
             ("disconnected", disconnecting ? ", disconnecting it" : "")
             ("all_strikes", _strikes_since_withholding_warning)("disconnects", _strike_disconnects_since_withholding_warning));
        _last_withholding_warning_time = now;
        _strikes_since_withholding_warning = 0;
        _strike_disconnects_since_withholding_warning = 0;
      }
      if (!disconnecting)
        return;
      dlog("disconnecting peer ${peer}: it answered item_not_available for items it advertised ${strikes} times, ${seconds}s "
           "apart or more, and delivered none we asked for in between",
           ("peer", peer->get_remote_endpoint())(strikes)("seconds", GRAPHENE_NET_UNAVAILABLE_ITEM_DEMOTION_SEC));
      fc::exception detailed_error(FC_LOG_MESSAGE(warn, "You said you didn't have items you advertised ${strikes} times and delivered "
                                                  "none in between", (strikes)));
      disconnect_from_peer(peer, "You keep advertising items you don't have", true, detailed_error);
    }

    // A peer can answer keepalives and do nothing else for as long as it likes, and while our slots are full the
    // connect loop dials nobody, so a node in sync could keep such a peer forever.  So while our head is not behind
    // (peer expansion, expand_peers_while_starved(), covers the opposite), an active peer that for
    // silent_peer_disconnect_timeout relayed us no block and no block or transaction inventory, and asked us for no
    // block ids and no items (see exchanges_items()), is replaced.  Both directions count: a node in sync gets each
    // block from whichever peer is first, so a peer downstream of us, or of a producer we are near, only asks us for
    // what we advertise, and a peer syncing from us only asks for blocks and block ids.  Asking costs nothing, so a peer
    // that only asks keeps its slot; this only clears peers that exchange nothing.  Only as many peers are replaced as
    // the connect loop has candidates to dial in their place, and at least GRAPHENE_NET_IDLE_PEERS_KEPT_AT_LEAST or
    // half the desired connections are kept
    std::vector<peer_connection_ptr> node_impl::find_silent_peers_to_replace()
    {
      VERIFY_CORRECT_THREAD();
      std::vector<peer_connection_ptr> silent_peers;
      if (_node_configuration.silent_peer_disconnect_timeout == 0 || _active_connections.empty())
        return silent_peers;
      const fc::time_point now = fc::time_point::now();
      idle_peer_sweep sweep;
      sweep.now = now;
      sweep.window = fc::seconds(_node_configuration.silent_peer_disconnect_timeout);
      sweep.kept_at_least = std::max<size_t>(GRAPHENE_NET_IDLE_PEERS_KEPT_AT_LEAST, _node_configuration.desired_number_of_connections / 2);
      std::vector<peer_connection_ptr> peers;
      std::vector<idle_peer_candidate> candidates;
      for (const peer_connection_ptr& peer : _active_connections)
      {
        idle_peer_candidate candidate;
        candidate.last_useful_data_time = peer->last_item_exchange_time;
        peers.push_back(peer);
        candidates.push_back(candidate);
      }
      std::vector<size_t> selected = select_idle_peers_to_disconnect(candidates, sweep);
      if (selected.empty())
        return silent_peers;

      refresh_cached_chain_state();
      const fc::time_point head_block_time = _delegate->get_block_time(_delegate->get_head_block_id());
      if (head_block_time < now - fc::seconds(GRAPHENE_NET_IDLE_PEERS_HEAD_BEHIND_SEC))
        return silent_peers;
      uint32_t unroutable_ipv4_candidates = 0;
      uint32_t unroutable_ipv6_candidates = 0;
      const size_t replacements = select_connection_candidates(get_connection_candidates(unroutable_ipv4_candidates, unroutable_ipv6_candidates),
                                                               get_connection_addresses(false),
                                                               get_connection_selection_limits(static_cast<uint32_t>(selected.size()))).size();
      if (selected.size() > replacements)
        selected.resize(replacements);
      for (size_t i : selected)
        silent_peers.push_back(peers[i]);
      return silent_peers;
    }

    // Closes the connection to a silent peer (see find_silent_peers_to_replace()) and keeps it from taking a slot again
    // for one silent_peer_disconnect_timeout (see back_off_replaced_peer()).  Each one is logged: the window is long
    // and the floor and the candidates bound how many go at once
    void node_impl::disconnect_silent_peer(peer_connection* peer)
    {
      VERIFY_CORRECT_THREAD();
      const int64_t silent_seconds = (fc::time_point::now() - peer->last_item_exchange_time).to_seconds();
      wlog("disconnecting silent peer ${peer}: for ${seconds}s it relayed us no block or inventory and asked us for nothing while "
           "our head #${head} is not behind; its slot goes to a candidate, and it may not take one for ${window}s",
           ("peer", peer->get_remote_endpoint())("seconds", silent_seconds)("head", _cached_head_block_num)
           ("window", _node_configuration.silent_peer_disconnect_timeout));
      back_off_replaced_peer(peer, _node_configuration.silent_peer_disconnect_timeout);
      fc::exception detailed_error(FC_LOG_MESSAGE(warn, "You relayed me nothing and asked me for nothing for ${seconds} seconds",
                                                  ("seconds", silent_seconds)));
      disconnect_from_peer(peer, "You relayed me nothing and asked me for nothing", true, detailed_error);
      count_failed_connection_attempt(peer, last_connection_failed);
    }

    // Keeps a peer we replaced from taking a slot again, dialed or dialing us, for @p seconds: back sooner, its time
    // would start over and it could hold the slot again for a whole window
    void node_impl::back_off_replaced_peer(peer_connection* peer, uint32_t seconds)
    {
      VERIFY_CORRECT_THREAD();
      const fc::time_point now = fc::time_point::now();
      forget_expired_replaced_peer_backoffs(now);
      const fc::time_point backoff_end = now + fc::seconds(seconds);
      if (peer->node_id != node_id_t())
        _replaced_peer_backoff_by_node_id[peer->node_id] = backoff_end;
      // an inbound peer only claims the endpoint it listens at: one at another host could name a third node's
      const fc::optional<fc::ip::endpoint> endpoint = peer->get_endpoint_for_db();
      const fc::optional<fc::ip::endpoint> remote_endpoint = peer->get_remote_endpoint();
      if (endpoint && remote_endpoint && endpoint->get_address() == remote_endpoint->get_address())
        _replaced_peer_backoff_by_endpoint[*endpoint] = backoff_end;
    }

    // A node reachable at several addresses (a seed whose name resolves to an IPv4 and an IPv6 address, a node whose
    // addresses peers pass around) has one peer database entry per address.  Once we hold a connection to it, dialing
    // another of its addresses ends in an already_connected rejection, which is no failure and so is retried every
    // peer_connection_retry_timeout, a key exchange on both sides each time.  So the node id that answers at each
    // endpoint we dial is remembered, and the connect loop skips an endpoint whose node is connected; once that
    // connection is gone the endpoint is dialed as before.  Only endpoints we dialed are remembered: an inbound peer
    // only claims where it listens.  The map is bounded by forgetting endpoints no longer in the peer database
    void node_impl::remember_node_id_of_dialed_endpoint(const fc::ip::endpoint& endpoint, const node_id_t& node_id)
    {
      VERIFY_CORRECT_THREAD();
      if (node_id == _node_id)
        return; // connected_to_self remembers the endpoint as our own instead
      if (_node_id_by_dialed_endpoint.size() >= GRAPHENE_NET_MAX_PEERDB_SIZE && !_node_id_by_dialed_endpoint.count(endpoint))
      {
        for (auto iter = _node_id_by_dialed_endpoint.begin(); iter != _node_id_by_dialed_endpoint.end();)
          iter = _potential_peer_db.lookup_entry_for_endpoint(iter->first) ? std::next(iter) : _node_id_by_dialed_endpoint.erase(iter);
        if (_node_id_by_dialed_endpoint.size() >= GRAPHENE_NET_MAX_PEERDB_SIZE)
          _node_id_by_dialed_endpoint.erase(_node_id_by_dialed_endpoint.begin());
      }
      _node_id_by_dialed_endpoint[endpoint] = node_id;
    }

    // Whether the node that answered when we last dialed @p endpoint is connected, active or handshaking, over another connection
    bool node_impl::is_endpoint_of_connected_node(const fc::ip::endpoint& endpoint)
    {
      VERIFY_CORRECT_THREAD();
      const auto known = _node_id_by_dialed_endpoint.find(endpoint);
      if (known == _node_id_by_dialed_endpoint.end())
        return false;
      if (!get_peer_by_node_id(known->second))
        return false;
      dlog("not dialing ${endpoint}: node ${id}, which answered there, is connected", ("endpoint", endpoint)("id", known->second));
      return true;
    }

    void node_impl::forget_expired_replaced_peer_backoffs(fc::time_point now)
    {
      VERIFY_CORRECT_THREAD();
      for (auto iter = _replaced_peer_backoff_by_node_id.begin(); iter != _replaced_peer_backoff_by_node_id.end();)
        iter = iter->second <= now ? _replaced_peer_backoff_by_node_id.erase(iter) : std::next(iter);
      for (auto iter = _replaced_peer_backoff_by_endpoint.begin(); iter != _replaced_peer_backoff_by_endpoint.end();)
        iter = iter->second <= now ? _replaced_peer_backoff_by_endpoint.erase(iter) : std::next(iter);
    }

    // When the replaced-peer backoff of the peer with @p node_id, or the one we dial at @p endpoint, ends; none if neither has one
    fc::optional<fc::time_point> node_impl::replaced_peer_backoff_end(const node_id_t& node_id, const fc::optional<fc::ip::endpoint>& endpoint)
    {
      VERIFY_CORRECT_THREAD();
      const fc::time_point now = fc::time_point::now();
      fc::optional<fc::time_point> end;
      const auto by_node_id = _replaced_peer_backoff_by_node_id.find(node_id);
      if (by_node_id != _replaced_peer_backoff_by_node_id.end() && by_node_id->second > now)
        end = by_node_id->second;
      if (endpoint)
      {
        const auto by_endpoint = _replaced_peer_backoff_by_endpoint.find(*endpoint);
        if (by_endpoint != _replaced_peer_backoff_by_endpoint.end() && by_endpoint->second > now && (!end || by_endpoint->second > *end))
          end = by_endpoint->second;
      }
      return end;
    }

    // Counts a failed attempt against the peer's database entry, so the connect loop dials it again only after
    // (number_of_failed_connection_attempts + 1) * peer_connection_retry_timeout; the count grows past
    // @p counted_at_most no further.  A completed handshake clears it
    void node_impl::count_failed_connection_attempt(peer_connection* peer, potential_peer_last_connection_disposition disposition,
                                                    uint32_t counted_at_most)
    {
      VERIFY_CORRECT_THREAD();
      const fc::optional<fc::ip::endpoint> endpoint_for_db = peer->get_endpoint_for_db();
      if (!endpoint_for_db)
        return;
      fc::optional<potential_peer_record> updated_peer_record = _potential_peer_db.lookup_entry_for_endpoint(*endpoint_for_db);
      if (!updated_peer_record)
        return;
      updated_peer_record->last_connection_disposition = disposition;
      updated_peer_record->last_connection_attempt_time = fc::time_point::now();
      updated_peer_record->last_failed_time = updated_peer_record->last_connection_attempt_time;
      if (updated_peer_record->number_of_failed_connection_attempts < counted_at_most)
        updated_peer_record->number_of_failed_connection_attempts++;
      _potential_peer_db.update_entry(*updated_peer_record);
    }

    // Returns true when we have already disconnected so many peers over "invalid" blocks in
    // the recent window that our own judgment is suspect (see the config constants).  When it
    // returns false, the caller's impending disconnect is recorded against the window.
    bool node_impl::invalid_block_disconnect_breaker_tripped()
    {
      VERIFY_CORRECT_THREAD();
      return _invalid_block_disconnect_breaker.tripped(fc::time_point::now());
    }

    // Like invalid_block_disconnect_breaker_tripped(), for disconnects over a block copy whose body
    // doesn't match its header (see the config constants)
    bool node_impl::block_body_mismatch_disconnect_breaker_tripped()
    {
      VERIFY_CORRECT_THREAD();
      return _block_body_mismatch_disconnect_breaker.tripped(fc::time_point::now());
    }

    // A peer sent us a copy of a block whose body doesn't match its header.  The block id itself may
    // be genuine, so queue the block to be fetched again from another peer that advertised it
    void node_impl::refetch_block_from_other_peers(peer_connection* peer_that_sent_bad_copy, const item_id& block_item_id)
    {
      VERIFY_CORRECT_THREAD();
      peer_that_sent_bad_copy->block_inventory_peer_advertised_to_us.erase(block_item_id);
      for (const peer_connection_ptr& peer : _active_connections)
        if (peer->items_requested_from_peer.find(block_item_id) != peer->items_requested_from_peer.end())
          return; // another copy is already on its way
      if (_items_to_fetch.get<item_id_index>().find(block_item_id) == _items_to_fetch.get<item_id_index>().end() &&
          queue_item_to_fetch_from_advertisers(block_item_id))
        trigger_fetch_items_loop(true);
    }

    void node_impl::send_sync_block_to_node_delegate(const received_sync_item& item)
    {
      dlog("in send_sync_block_to_node_delegate()");

      const std::shared_ptr<full_block_type>& full_block = item.full_block;
      bool client_accepted_block = false;
      bool discontinue_fetching_blocks_from_peer = false;
      bool sync_block_could_not_link = false;
      bool sync_block_body_mismatch = false;
      const uint32_t block_number = full_block->get_block_num();
      const block_id_type& block_id = full_block->get_block_id();

      fc::oexception handle_message_exception;

      try
      {
        fc_dlog(fc::logger::get("sync"), "p2p pushing sync block #${block_number} ${block_id}", (block_number)(block_id));
        const block_handling_result result = _delegate->handle_block(full_block, true);

        if (block_number % 1000 == 0)
          ilog("Successfully pushed sync block ${block_number} ${block_id}", (block_number)(block_id));

        // a block only stored on a shorter fork was not validated, so it isn't recorded as accepted
        // (later copies are still recognized as known through has_item())
        if (result != block_handling_result::ignored)
          _most_recent_blocks_accepted.push_back(block_id);
        refresh_cached_chain_state();

        client_accepted_block = true;
        // as in process_block_during_normal_operation(): the delivering peer's copy matched its header, but a block
        // only stored on a shorter fork was not validated, so it says nothing about the peer
        if (const peer_connection_ptr delivering_peer = item.delivering_peer.lock())
          if (result != block_handling_result::ignored)
          {
            delivering_peer->restore_block_fetching_priority();
            record_block_delivered_by(delivering_peer.get());
            record_requested_delivery(delivering_peer.get());
          }
      }
      catch (const block_older_than_undo_history& e)
      {
        fc_wlog(fc::logger::get("p2p"), "Failed to push sync block #${block_number} ${block_id}: block is on a fork older than our undo history would allow us to switch to: ${e}",
                (block_number)(block_id)("e", (fc::exception)e));
        wlog("Failed to push sync block #${block_number} ${block_id}): block is on a fork older than our undo history would allow us to switch to.",
             (block_number)(block_id));
        handle_message_exception = e;
        discontinue_fetching_blocks_from_peer = true;
      }
      catch (const unlinkable_block_exception& e)
      {
        // This is almost always a benign race rather than peer misbehavior: while this block was
        // queued or in flight, our chain advanced past it (we received it first from another peer,
        // or it was on a micro-fork branch that has since been orphaned).  The fork database also
        // reports blocks that have fallen below its window as unlinkable.  Skip the block and keep
        // the peers: disconnecting everyone who had it in flight is how routine delivery races
        // amplified into the network-wide disconnect storm of 2026-07-09 (the live-mode path was
        // made tolerant then; this is the sync-mode counterpart).
        fc_wlog(fc::logger::get("sync"), "p2p could not link sync block #${block_number} ${block_id}, likely a stale delivery race; ignoring it: ${e}",
                (block_number)(block_id)("e", (fc::exception)e));
        wlog("Could not link sync block #${block_number} (id:${block_id}), likely a stale delivery race; ignoring it",
             (block_number)(block_id));
        handle_message_exception = e;
        sync_block_could_not_link = true;
      }
      catch (const block_body_mismatch_exception& e)
      {
        // the body of this copy doesn't match its header, but the block id may well be genuine: only
        // the peer that delivered this copy is at fault, every other peer may offer us the correct one
        fc_wlog(fc::logger::get("sync"), "p2p rejected sync block #${block_number} ${block_id}: its body does not match its header: ${e}",
                (block_number)(block_id)("e", (fc::exception)e));
        wlog("Rejected sync block #${block_number} (id:${block_id}): its body does not match its header",
             (block_number)(block_id));
        handle_message_exception = e;
        sync_block_body_mismatch = true;
      }
      catch (const fc::canceled_exception&)
      {
        throw;
      }
      catch (const p2p_node_shutting_down_exception&)
      {
        // the block wasn't processed because we're shutting down.  The is probably perfectly valid, so we
        // don't want to mark the peer as bad because of the error.
        // Since we're shutting down, I think it's ok to just return out of the function since we won't need
        // to trigger fetching new items ids or other similar tasks
        return;
      }
      catch (const fc::exception& e)
      {
        fc_wlog(fc::logger::get("sync"), "p2p failed to push sync block #${block_number} ${block_id}: client rejected sync block sent by peer.",
                (block_number) (block_id));
        wlog("Failed to push sync block #${block_number} ${block_id}): client rejected sync block sent by peer.",
             (block_number) (block_id));
        handle_message_exception = e;
      }

      // build up lists for any potentially-blocking operations we need to do, then do them at the end of this function
      std::set<peer_connection_ptr> peers_to_fetch_ids_from;
      std::set<peer_connection_ptr> peers_we_need_to_sync_to;
      std::map<peer_connection_ptr, std::pair<std::string, fc::oexception>> peers_to_disconnect; // map peer -> pair<reason_string, exception>
      if (client_accepted_block)
      {
        --_total_number_of_unfetched_items;
        dlog("sync: client accepted the block, we now have only ${count} items left to fetch before we're in sync",
             ("count", _total_number_of_unfetched_items));
        bool is_fork_block = is_hard_fork_block(block_number);
        for (const peer_connection_ptr& peer : _active_connections)
        {
          ASSERT_TASK_NOT_PREEMPTED(); // don't yield while iterating over _active_connections
          bool disconnecting_this_peer = false;
          if (is_fork_block)
          {
            // we just pushed a hard fork block.  Find out if this peer is running a client
            // that will be unable to process future blocks
            if (peer->last_known_fork_block_number != 0)
            {
              uint32_t next_fork_block_number = get_next_known_hard_fork_block_number(peer->last_known_fork_block_number);
              if (next_fork_block_number != 0 &&
                  next_fork_block_number <= block_number)
              {
                std::ostringstream disconnect_reason_stream;
                disconnect_reason_stream << "You need to upgrade your client due to hard fork at block " << block_number;
                peers_to_disconnect[peer] = std::make_pair(disconnect_reason_stream.str(),
                                              fc::oexception(fc::exception(FC_LOG_MESSAGE(error, "You need to upgrade your client due to hard fork at block ${block_number}",
                                                                                          (block_number)))));
#ifdef ENABLE_DEBUG_ULOGS
                ulog("Disconnecting from peer during sync because their version is too old.  Their version date: ${date}", ("date", peer->graphene_git_revision_unix_timestamp));
#endif
                disconnecting_this_peer = true;
              }
            }
          }
          if (!disconnecting_this_peer)
          {
            if (peer->ids_of_items_to_get.empty() && peer->ids_of_items_being_processed.empty())
            {
              if (!peer->peer_needs_sync_items_from_us && !peer->we_need_sync_items_from_peer)
              {
                // We just got a new valid sync block and we don't know for sure that our peers have it.
                // If peer is still syncing to us, we know they will ask us for
                // sync item ids at least one more time and we'll notify them about
                // the item then, so there's no need to do anything.  If we still need items
                // from them, we'll be asking them for more items at some point, and
                // that will clue them in that they are out of sync.  But if we're both in sync,
                // we need to kick off another round of synchronization with them so they can
                // find out about this new item.
                dlog("We will be restarting synchronization with peer ${peer}", ("peer", peer->get_remote_endpoint()));
                peers_we_need_to_sync_to.insert(peer);
              }
            }
            else
            {
              auto items_being_processed_iter = peer->ids_of_items_being_processed.find(block_id);
              if (items_being_processed_iter != peer->ids_of_items_being_processed.end())
              {
                peer->last_block_delegate_has_seen = block_id;
                peer->last_block_time_delegate_has_seen = full_block->get_block_header().timestamp;

                peer->ids_of_items_being_processed.erase(items_being_processed_iter);
                if (peer->idle())
                {
                  // if we know the peer has more block ids and we haven't filled our id buffer, get more ids
                  if (peer->number_of_unfetched_item_ids != 0 && peer->ids_of_items_to_get.size() < GRAPHENE_NET_MIN_BLOCK_IDS_TO_PREFETCH)
                    peers_to_fetch_ids_from.insert(peer);
                  // if we've processed all blocks from peer that we know about, ask again to find out if more and to tell them we may be in sync now
                  else if (peer->ids_of_items_to_get.empty() && peer->ids_of_items_being_processed.empty() && peer->number_of_unfetched_item_ids == 0)
                    peers_to_fetch_ids_from.insert(peer);
                }
              }
            }
          }

        } //for active peers
      }
      else if (sync_block_could_not_link)
      {
        // The block was stale (or on an orphaned branch) by the time we processed it -- see the
        // catch above.  This is not peer misbehavior: clean up the in-flight bookkeeping exactly
        // as if the block had been handled, and let the sync state machine ask the affected peers
        // for their next batch of item ids.  The fresh synopsis exchange naturally skips anything
        // we already have.
        if (_total_number_of_unfetched_items > 0)
          --_total_number_of_unfetched_items;
        for (const peer_connection_ptr& peer : _active_connections)
        {
          ASSERT_TASK_NOT_PREEMPTED(); // don't yield while iterating over _active_connections
          if (peer->ids_of_items_being_processed.erase(block_id) > 0 && peer->idle())
            peers_to_fetch_ids_from.insert(peer);
        }
      }
      else if (sync_block_body_mismatch)
      {
        // Disconnect only the peer that delivered the bad copy.  The block is still unfetched, so every
        // other peer that had it queued gets it back at the front of its list of items to get, and the
        // genuine copy is requested from one of them.  Where that list has moved on in the meantime,
        // the peer is cleaned up as in the stale-delivery path above, so it offers the block again
        const peer_connection_ptr delivering_peer = item.delivering_peer.lock();
        const bool disconnect_delivering_peer = delivering_peer &&
                                                _active_connections.find(delivering_peer) != _active_connections.end() &&
                                                !block_body_mismatch_disconnect_breaker_tripped();
        bool block_requeued = false;
        for (const peer_connection_ptr& peer : _active_connections)
        {
          ASSERT_TASK_NOT_PREEMPTED(); // don't yield while iterating over _active_connections
          if (peer == delivering_peer)
          {
            if (disconnect_delivering_peer)
            {
              peers_to_disconnect[peer] = std::make_pair(std::string("You sent us a block whose body does not match its header"),
                                                         fc::oexception(fc::exception(FC_LOG_MESSAGE(error, "You sent us a copy of block number ${block_number} with hash ${block_id} whose body does not match its header",
                                                                                                     (block_number)(block_id)))));
              continue;
            }
            wlog("block-body-mismatch disconnect circuit breaker tripped: keeping peer ${endpoint} despite rejected sync block #${block_number}",
                 ("endpoint", peer->get_remote_endpoint())(block_number));
            peer->block_rejected_while_breaker_tripped = true;
            peer->deprioritize_block_fetching();
            if (peer->ids_of_items_being_processed.erase(block_id) > 0 && peer->idle())
              peers_to_fetch_ids_from.insert(peer);
            continue;
          }
          if (peer->ids_of_items_being_processed.erase(block_id) == 0)
            continue;
          if (peer->we_need_sync_items_from_peer &&
              (peer->ids_of_items_to_get.empty() || peer->first_id_block_number == block_number + 1))
          {
            peer->ids_of_items_to_get.push_front(block_id);
            peer->first_id_block_number = block_number;
            peer->reset_id_search_for_peer();
            block_requeued = true;
          }
          else if (peer->idle())
            peers_to_fetch_ids_from.insert(peer);
        }
        if (block_requeued)
          trigger_fetch_sync_items_loop();
      }
      else //client did not accept block
      {
        fc::exception disconnect_exception(FC_LOG_MESSAGE(error, "You offered us a block that we reject as invalid.  The invalid block was number: ${block_number} with hash ${block_id}",
                                                          (block_number)(block_id)));
        const bool breaker_tripped = !discontinue_fetching_blocks_from_peer && invalid_block_disconnect_breaker_tripped();
        // invalid message received
        for (const peer_connection_ptr& peer : _active_connections)
        {
          ASSERT_TASK_NOT_PREEMPTED(); // don't yield while iterating over _active_connections

          if (peer->ids_of_items_being_processed.find(block_id) != peer->ids_of_items_being_processed.end())
          {
            if (discontinue_fetching_blocks_from_peer)
            {
              wlog("inhibiting fetching sync blocks from peer ${endpoint} because it is on a fork that's too old",
                   ("endpoint", peer->get_remote_endpoint()));
              peer->inhibit_fetching_sync_blocks = true;
            }
            else if (breaker_tripped)
            {
              // too many "invalid" rejections too quickly -- distrust our own judgment and keep
              // the peer; clean up bookkeeping the same way the stale-delivery path does
              wlog("invalid-block disconnect circuit breaker tripped: keeping peer ${endpoint} despite rejected sync block #${block_number}",
                   ("endpoint", peer->get_remote_endpoint())(block_number));
              peer->block_rejected_while_breaker_tripped = true;
              peer->ids_of_items_being_processed.erase(block_id);
              if (peer->idle())
                peers_to_fetch_ids_from.insert(peer);
            }
            else
              peers_to_disconnect[peer] = std::make_pair(std::string("You offered us a block that we reject as invalid"), disconnect_exception);
          }
        }
      }

      for (auto& peer_to_disconnect : peers_to_disconnect)
      {
        const peer_connection_ptr& peer = peer_to_disconnect.first;
        std::string reason_string;
        fc::oexception reason_exception;
        std::tie(reason_string, reason_exception) = peer_to_disconnect.second;
        wlog("disconnecting client ${endpoint} because it offered us the rejected block", ("endpoint", peer->get_remote_endpoint()));
        disconnect_from_peer(peer.get(), reason_string, true, reason_exception);
      }

      for (const peer_connection_ptr& peer : peers_to_fetch_ids_from)
        fetch_next_batch_of_item_ids_from_peer(peer.get());

      for (const peer_connection_ptr& peer : peers_we_need_to_sync_to)
        start_synchronizing_with_peer(peer);

      dlog("Leaving send_sync_block_to_node_delegate");

      if ( !node_is_shutting_down() &&
          (!_process_backlog_of_sync_blocks_done.valid() || _process_backlog_of_sync_blocks_done.ready()) &&
          get_number_of_handle_message_calls_in_progress() < _node_configuration.maximum_number_of_blocks_to_handle_at_one_time / 2 //DLN add hystersis to reduce number of times we process_backlog_of_sync_blocks
         )
        _process_backlog_of_sync_blocks_done = async_task([=, this](){ process_backlog_of_sync_blocks(); }, "process_backlog_of_sync_blocks");
    }

    uint32_t node_impl::get_number_of_handle_message_calls_in_progress()
    {
      // garbage-collect the list of async tasks before we count them
      for (auto calls_iter = _handle_message_calls_in_progress.begin();
            calls_iter != _handle_message_calls_in_progress.end();)
      {
        if (calls_iter->ready())
          calls_iter = _handle_message_calls_in_progress.erase(calls_iter);
        else
          ++calls_iter;
      }
      return _handle_message_calls_in_progress.size();
    }
    
    void node_impl::discard_abandoned_sync_blocks()
    {
      std::set<block_id_type> block_ids_still_referenced;
      for (const peer_connection_ptr& peer : _active_connections)
        for (const block_id_type& block_id : peer->ids_of_items_to_get)
          block_ids_still_referenced.insert(block_id);
      unsigned discarded_count = 0;
      for (auto iter = _received_sync_items.begin(); iter != _received_sync_items.end();)
      {
        auto block_id = iter->full_block->get_block_id();
        if (block_ids_still_referenced.find(block_id) == block_ids_still_referenced.end())
        {
          _already_received_sync_item_ids.erase(block_id);
          iter = _received_sync_items.erase(iter);
          ++discarded_count;
        }
        else
          ++iter;
      }
      // fc_ilog(fc::logger::get("default"), "Currently discarding ${discarded_count} unreferenced sync blocks", (discarded_count));
    }

    void node_impl::process_backlog_of_sync_blocks()
    {
      dlog("in process_backlog_of_sync_blocks");
      if (get_number_of_handle_message_calls_in_progress() >= _node_configuration.maximum_number_of_blocks_to_handle_at_one_time)
      {
        dlog("leaving process_backlog_of_sync_blocks because we've already sent blockchain enough blocks");
        return; // we will be rescheduled when the next block finishes its processing
      }
      dlog("currently ${count} blocks in the process of being handled", ("count", _handle_message_calls_in_progress.size()));

      if (_suspend_fetching_sync_blocks)
      {
        dlog("resuming processing sync block backlog because we only ${count} blocks in progress",
             ("count", _handle_message_calls_in_progress.size()));
        _suspend_fetching_sync_blocks = false;
      }

      // when syncing with multiple peers, it's possible that we'll have hundreds of blocks ready to push
      // to the client at once.  This can be slow, and we need to limit the number we push at any given
      // time to allow network traffic to continue so we don't end up disconnecting from peers
      //fc::time_point start_time = fc::time_point::now();
      //fc::time_point when_we_should_yield = start_time + fc::seconds(1);

      bool block_processed_this_iteration;
      unsigned blocks_processed = 0;

      std::set<peer_connection_ptr> peers_to_fetch_ids_from;
      std::set<peer_connection_ptr> peers_we_need_to_sync_to;
      std::map<peer_connection_ptr, fc::oexception> peers_with_rejected_block;

      do
      {
        std::copy(std::make_move_iterator(_new_received_sync_items.begin()), std::make_move_iterator(_new_received_sync_items.end()),
                  std::inserter(_received_sync_items, _received_sync_items.end()));
        _new_received_sync_items.clear();
        dlog("currently ${count} sync items to consider", ("count", _received_sync_items.size()));

        block_processed_this_iteration = false;
        for (const peer_connection_ptr& peer : _active_connections)
        {
          activity_tracer aTracer(__FUNCTION__, *this);

          if (!peer->ids_of_items_to_get.empty())
          {
            // check if we have a backlog block to push
            auto received_block_iter = _received_sync_items.find(peer->ids_of_items_to_get.front());
            if (received_block_iter != _received_sync_items.end())
            {
              const block_id_type& received_block_id = received_block_iter->full_block->get_block_id();
              // remove from front of any other peer's item to get list
              for (const peer_connection_ptr& peer : _active_connections)
              {
                ASSERT_TASK_NOT_PREEMPTED(); // don't yield while iterating over _active_connections
                if (!peer->ids_of_items_to_get.empty() &&
                    peer->ids_of_items_to_get.front() == received_block_id)
                {
                  peer->ids_of_items_to_get.pop_front();
                  ++peer->first_id_block_number; //since we popped front, we are at a higher block number
                  peer->ids_of_items_being_processed.insert(received_block_id);
                  //if we've fetched blocks for all ids we have from this peer, lets fetch more if possible (we need this to avoid stall condition)
                  if (peer->ids_of_items_to_get.empty() && 
                      peer->number_of_unfetched_item_ids != 0 &&
                      peer->we_need_sync_items_from_peer)
                    peers_to_fetch_ids_from.insert(peer);
                }
              }  
              // dlog("removed from front of any other peer");

              // we can get into an interesting situation near the end of synchronization.  We can be in
              // sync with one peer who is sending us the last block on the chain via a regular inventory
              // message, while at the same time still be synchronizing with a peer who is sending us the
              // block through the sync mechanism.  Further, we must request both blocks because
              // we don't know they're the same (for the peer in normal operation, it has only told us the
              // message id, for the peer in the sync case we only known the block_id).
              // As in process_block_during_normal_operation(), the chain is asked as well: blocks it only
              // stored on a shorter fork are not on the recently-accepted list
              const bool block_has_been_accepted =
                std::find(_most_recent_blocks_accepted.begin(), _most_recent_blocks_accepted.end(), received_block_id) != _most_recent_blocks_accepted.end() ||
                _delegate->has_item(item_id(block_message_type, received_block_id));
              if (!block_has_been_accepted)
              {
                // normal case, schedule this block to be sent to blockchain
                _already_received_sync_item_ids.erase(received_block_id);

                _handle_message_calls_in_progress.push_back(async_task([this, item=*received_block_iter](){
                  send_sync_block_to_node_delegate(item);
                }, "send_sync_block_to_node_delegate"));
                
                _received_sync_items.erase(received_block_iter);

                ++blocks_processed;
                block_processed_this_iteration = true;
              }
              else
              {
                dlog("Already received and accepted this block through normal inventory mechanism, treating it as accepted");
                _already_received_sync_item_ids.erase(received_block_id);
                _received_sync_items.erase(received_block_iter);
                for (const peer_connection_ptr& peer : _active_connections)
                {
                  auto items_being_processed_iter = peer->ids_of_items_being_processed.find(received_block_id);
                  if (items_being_processed_iter != peer->ids_of_items_being_processed.end())
                  {
                    peer->ids_of_items_being_processed.erase(items_being_processed_iter);
                    dlog("Removed item from ${endpoint}'s list of items being processed, still processing ${len} blocks",
                         ("endpoint", peer->get_remote_endpoint())("len", peer->ids_of_items_being_processed.size()));
                  }
                  // as when the chain accepts the last block we know the peer has: ask again whether it has more
                  if (peer->we_need_sync_items_from_peer && peer->idle() && peer->ids_of_items_to_get.empty() &&
                      peer->ids_of_items_being_processed.empty() && peer->number_of_unfetched_item_ids == 0)
                    peers_to_fetch_ids_from.insert(peer);
                }
                // the front of the peers' lists moved on, so look at it again.  Stopping here would leave the
                // next block unpushed: when every block that follows has already been received (e.g. refetched
                // by peers that connected while the chain was stalled applying the copies of a peer that left
                // since), no delivery and no completed block is left to run this function again
                block_processed_this_iteration = true;
              }
              break; //restart checking front of peers' item lists because we've popped the first item from some of them
            } //if we have a backlog block to push
          } //if peer item list not empty 
        } //for active peers, check if we can push their first block
                
        if (_handle_message_calls_in_progress.size() >= _node_configuration.maximum_number_of_blocks_to_handle_at_one_time)
        {
          dlog("stopping processing sync block backlog because we have ${count} blocks sent to the blockchain",
               ("count", _handle_message_calls_in_progress.size()));
          //ulog("stopping processing sync block backlog because we have ${count} blocks in progress, total on hand: ${received}",
          //     ("count", _handle_message_calls_in_progress.size())("received", _received_sync_items.size()));
          if (_received_sync_items.size() >= _node_configuration.maximum_number_of_sync_blocks_to_prefetch)
          {
            _suspend_fetching_sync_blocks = true;
            dlog("stopped prefetching sync blocks because we have backlog of ${received} blocks to consider", ("received", _received_sync_items.size()));
          }
          break;
        }
      } 
      while (block_processed_this_iteration);

#if 0
      //report if we have any peers that haven't given us the next block we need to process the backlog of received blocks
      if (_handle_message_calls_in_progress.size() < _node_configuration.maximum_number_of_blocks_to_handle_at_one_time &&
          _received_sync_items.size() > 1000)
      {
        std::set<item_hash_t> all_potential_first_items;
        for (const peer_connection_ptr& peer : _active_connections)
          if (!peer->ids_of_items_to_get.empty())
            all_potential_first_items.insert(peer->ids_of_items_to_get.front());
        for (const item_hash_t& id : all_potential_first_items)
          for (const peer_connection_ptr& peer : _active_connections)
            if (peer->sync_items_requested_from_peer.find(id) != peer->sync_items_requested_from_peer.end())
              wlog("We're waiting on a block from ${peer} (requested ${count} blocks from them, last received one ${time_since_last_block_received}ms ago)", 
                   ("peer", peer->get_remote_endpoint())("count", peer->sync_items_requested_from_peer.size())
                   ("time_since_last_block_received", (fc::time_point::now() - peer->last_sync_item_received_time).count() / 1000));
      }
#endif
      for (const peer_connection_ptr& peer : peers_to_fetch_ids_from)
        fetch_next_batch_of_item_ids_from_peer(peer.get());

      if (!_suspend_fetching_sync_blocks)
        trigger_fetch_sync_items_loop();
      dlog("leaving process_backlog_of_sync_blocks, ${count} processed", ("count", blocks_processed));
    }

    void node_impl::trigger_process_backlog_of_sync_blocks()
    {
      if (!node_is_shutting_down() &&
          (!_process_backlog_of_sync_blocks_done.valid() || _process_backlog_of_sync_blocks_done.ready()))
        _process_backlog_of_sync_blocks_done = async_task([=, this](){ process_backlog_of_sync_blocks(); }, "process_backlog_of_sync_blocks");
    }

    void node_impl::process_block_during_sync(peer_connection* originating_peer, const std::shared_ptr<full_block_type>& full_block)
    {
      dlog("start process_block_during_sync");
      // add it to the front of _received_sync_items, then process _received_sync_items to try to
      // pass as many messages as possible to the client.
      _new_received_sync_items.push_front(received_sync_item{full_block, originating_peer->weak_from_this()});
      _already_received_sync_item_ids.insert(full_block->get_block_id());
      trigger_process_backlog_of_sync_blocks();
    }

    void node_impl::process_block_during_normal_operation(peer_connection *originating_peer, const std::shared_ptr<full_block_type>& full_block,
                                                          bool late)
    {
      // we'll probably want to refer to these a lot
      const uint32_t block_num = full_block->get_block_num();
      const block_id_type& block_id = full_block->get_block_id();

      // TODO: this call to get_legacy_block_message_hash() is forcing the p2p thread to do an expensive (100-200us) hash computation early
      // in the block processing, before a blockchain worker thread has a chance to offload the work for us. 
      // After the next hf, when we can expect all peers to be on protocol GRAPHENE_NET_PROTOCOL_ADVERTISE_BLOCKS_BY_BLOCK_ID_VERSION,
      // get rid of this to reduce block processing latency
      const fc::ripemd160& legacy_block_message_hash = full_block->get_legacy_block_message_hash();

      fc::time_point message_receive_time = fc::time_point::now();
      dlog("received a block from peer ${endpoint}, passing it to client", ("endpoint", originating_peer->get_remote_endpoint()));
      std::set<peer_connection_ptr> peers_to_disconnect;
      std::string disconnect_reason;
      fc::oexception disconnect_exception;
      fc::oexception restart_sync_exception;
      try
      {
        // we can get into an intersting situation near the end of synchronization.  We can be in
        // sync with one peer who is sending us the last block on the chain via a regular inventory
        // message, while at the same time still be synchronizing with a peer who is sending us the
        // block through the sync mechanism.  Further, we must request both blocks because
        // we don't know they're the same (for the peer in normal operation, it has only told us the
        // message id, for the peer in the sync case we only known the block_id).
        fc::time_point message_validated_time;
        // the recently-accepted list is a fast bounded check, but it is not authoritative:
        // under heavy duplicate traffic its 200 entries can churn in under a minute, and the
        // 2026-07-09 forensics recorded nodes re-pushing blocks they had accepted less than a
        // minute earlier after exactly such a miss.  Fall back to asking the chain itself --
        // one fork-db/block-log lookup per delivered block is trivial next to a push
        const bool block_recently_accepted = std::find(_most_recent_blocks_accepted.begin(), _most_recent_blocks_accepted.end(), block_id) != _most_recent_blocks_accepted.end();
        bool block_has_been_accepted = block_recently_accepted || _delegate->has_item(item_id(block_message_type, block_id));
        if (!block_has_been_accepted &&
            _message_ids_currently_being_processed.find(block_id) == _message_ids_currently_being_processed.end())
        {
          _message_ids_currently_being_processed.insert(legacy_block_message_hash);
          _message_ids_currently_being_processed.insert(block_id);
          BOOST_SCOPE_EXIT(this_, &legacy_block_message_hash, &block_id) {
            this_->_message_ids_currently_being_processed.erase(legacy_block_message_hash);
            this_->_message_ids_currently_being_processed.erase(block_id);
          } BOOST_SCOPE_EXIT_END
          fc_ilog(fc::logger::get("sync"),
                  "p2p pushing block #${block_num} ${block_id} from ${peer} (message_id was ${legacy_block_message_hash})",
                  (block_num)(block_id)("peer", originating_peer->get_remote_endpoint())(legacy_block_message_hash));
          const block_handling_result result = _delegate->handle_block(full_block, false);
          message_validated_time = fc::time_point::now();
          // the peer's copy matched its header, so its earlier bad copy no longer makes it a source of last resort.  A
          // block only stored on a shorter fork was not validated, and is no block we could use: it neither lifts a
          // demotion nor makes the peer a source
          if (result != block_handling_result::ignored)
          {
            record_block_delivered_by(originating_peer);
            record_requested_delivery(originating_peer);
            if (late)
              record_late_block_copy(originating_peer, item_id(block_message_type, block_id));
            else
              originating_peer->restore_block_fetching_priority();
          }
          ilog("Successfully pushed block ${block_num} (id:${block_id})", (block_num)(block_id));
          // a block only stored on a shorter fork was not validated, so it isn't recorded as accepted
          // (later copies are still recognized as known through has_item())
          if (result != block_handling_result::ignored)
            _most_recent_blocks_accepted.push_back(block_id);

          // if we were planning on fetching any of the transactions that we just accepted in the block,
          // well, we already got them, so remove those from our items_to_fetch
          for (const std::shared_ptr<full_transaction_type>& full_transaction : full_block->get_full_transactions())
            _items_to_fetch.get<item_id_index>().erase(item_id(trx_message_type, full_transaction->get_legacy_transaction_message_hash()));

          // the same block may also be queued for fetching under its other alias (old peers
          // advertise blocks by legacy message hash, newer peers by block id, and we can't tell
          // they name the same block until we have it).  We just accepted it, so drop any
          // still-unrequested fetch entries for either alias
          _items_to_fetch.get<item_id_index>().erase(item_id(block_message_type, block_id));
          _items_to_fetch.get<item_id_index>().erase(item_id(block_message_type, legacy_block_message_hash));
        }
        else
        {
          fc_ilog(fc::logger::get("sync"),
                  "p2p NOT pushing block #${block_num} ${block_hash} from ${peer} because we recently pushed it",
                  ("block_num", block_num)("block_hash", block_id)
                  ("peer", originating_peer->get_remote_endpoint())("id", legacy_block_message_hash));
          dlog("Already received and accepted this block (presumably through sync mechanism), treating it as accepted");
          // a copy we have already gives us nothing, so it clears no strikes
          if (late)
            record_late_block_copy(originating_peer, item_id(block_message_type, block_id));
          if (!block_has_been_accepted) //if we haven't finished processing this block yet, don't talk to peers about it
            return;
          // the peer answered our request within its deadline, only later than another peer: that is no withholding,
          // so a good source is not left out of block fetching for the demotion's length, again each time another is
          // first.  Not for a block only stored on a shorter fork (has_item() knows it, but it isn't recorded as
          // accepted), nor for a peer that sent us a block whose body didn't match its header before: this copy isn't
          // validated
          if (!late && block_recently_accepted && !originating_peer->block_rejected_while_breaker_tripped)
            originating_peer->restore_block_fetching_priority();
        }

        dlog("client validated the block, advertising it to other peers");

        item_id legacy_block_message_item_id(core_message_type_enum::block_message_type, legacy_block_message_hash);
        item_id new_block_message_item_id(core_message_type_enum::block_message_type, block_id);
        fc::time_point_sec block_time = full_block->get_block_header().timestamp;

        for (const peer_connection_ptr& peer : _active_connections)
        {
          ASSERT_TASK_NOT_PREEMPTED(); // don't yield while iterating over _active_connections

          auto iter = peer->block_inventory_peer_advertised_to_us.find(peer->advertise_blocks_by_block_id() ? new_block_message_item_id : legacy_block_message_item_id);
          if (iter != peer->block_inventory_peer_advertised_to_us.end())
          {
            // this peer offered us the item.  It will eventually expire from the peer's
            // block_inventory_peer_advertised_to_us list after some time has passed (currently 2 minutes).
            // For now, it will remain there, which will prevent us from offering the peer this
            // block back when we rebroadcast the block below
            peer->last_block_delegate_has_seen = block_id;
            peer->last_block_time_delegate_has_seen = block_time;
          }
          peer->clear_old_inventory();
        }
        if (block_has_been_accepted)
        {
          // we didn't push this copy of the block -- an earlier copy was already accepted,
          // typically through the sync mechanism, which does not advertise.  Offer it to our
          // peers, but skip the rest of the broadcast bookkeeping: this copy was already cached
          // and recorded as accepted when the first copy arrived
          advertise_block(full_block);
        }
        else
        {
          message_propagation_data propagation_data{message_receive_time, message_validated_time, originating_peer->node_id};
          broadcast(full_block, propagation_data);
        }

        if (is_hard_fork_block(block_num))
        {
          // we just pushed a hard fork block.  Find out if any of our peers are running clients
          // that will be unable to process future blocks
          for (const peer_connection_ptr& peer : _active_connections)
          {
            if (peer->last_known_fork_block_number != 0)
            {
              uint32_t next_fork_block_number = get_next_known_hard_fork_block_number(peer->last_known_fork_block_number);
              if (next_fork_block_number != 0 && next_fork_block_number <= block_num)
              {
                peers_to_disconnect.insert(peer);
#ifdef ENABLE_DEBUG_ULOGS
                ulog("Disconnecting from peer because their version is too old.  Their version date: ${date}", ("date", peer->graphene_git_revision_unix_timestamp));
#endif
              }
            }
          }
          if (!peers_to_disconnect.empty())
          {
            std::ostringstream disconnect_reason_stream;
            disconnect_reason_stream << "You need to upgrade your client due to hard fork at block " << block_num;
            disconnect_reason = disconnect_reason_stream.str();
            disconnect_exception = fc::exception(FC_LOG_MESSAGE(error, "You need to upgrade your client due to hard fork at block ${block_num}", (block_num)));
          }
        }
      }
      catch (const fc::canceled_exception&)
      {
        throw;
      }
      catch (const unlinkable_block_exception& e)
      {
        restart_sync_exception = e;
      }
      catch (const block_body_mismatch_exception& e)
      {
        // the body of this copy doesn't match its header, but the block id may well be genuine: blame only
        // the peer that sent it, and fetch the block again from any other peer that advertised it
        wlog("Rejected block ${num} (id:${id}) sent by peer ${peer}: its body does not match its header",
             ("num", block_num)("id", block_id)("peer", originating_peer->get_remote_endpoint()));
        refetch_block_from_other_peers(originating_peer, item_id(block_message_type, block_id));
        refetch_block_from_other_peers(originating_peer, item_id(block_message_type, legacy_block_message_hash));
        if (block_body_mismatch_disconnect_breaker_tripped())
        {
          wlog("block-body-mismatch disconnect circuit breaker tripped: keeping peer ${peer} despite rejected block ${num} (id:${id})",
               ("peer", originating_peer->get_remote_endpoint())("num", block_num)("id", block_id));
          originating_peer->block_rejected_while_breaker_tripped = true;
          originating_peer->deprioritize_block_fetching();
        }
        else
        {
          disconnect_exception = e;
          disconnect_reason = "You sent me a block whose body does not match its header";
          peers_to_disconnect.insert(originating_peer->shared_from_this());
        }
      }
      catch (const fc::exception& e)
      {
        // client rejected the block.  Disconnect the peer that sent it to us
        wlog("Failed to push block ${num} (id:${id}), client rejected block sent by peer",
             ("num", block_num)
             ("id", block_id));

        if (invalid_block_disconnect_breaker_tripped())
        {
          // too many "invalid" rejections too quickly -- the likely explanation is a systemic
          // event or a local problem, not this many independently-misbehaving peers.  Keep the
          // peer and use the reconciliation path instead of the punitive one
          wlog("invalid-block disconnect circuit breaker tripped: keeping peer ${peer} despite rejected block ${num} (id:${id})",
               ("peer", originating_peer->get_remote_endpoint())("num", block_num)("id", block_id));
          originating_peer->block_rejected_while_breaker_tripped = true;
          restart_sync_exception = e;
        }
        else
        {
          disconnect_exception = e;
          disconnect_reason = "You offered me a block that I have deemed to be invalid";

          // Only the peer that actually delivered the rejected block is disconnected.  We used to
          // also disconnect every peer that merely had the same block queued for fetching; those
          // peers had sent us nothing yet, and that collective punishment turned single rejections
          // into the mass peer-list wipes seen in the 2026-07-09 incident.
          peers_to_disconnect.insert( originating_peer->shared_from_this() );
        }
      }

      if (restart_sync_exception)
      {
        wlog("Peer ${peer} sent me a block that didn't link to our blockchain.  Restarting sync mode with them to get the missing block. "
             "Error pushing block was: ${e}",
             ("peer", originating_peer->get_remote_endpoint())
             ("e", *restart_sync_exception));
        start_synchronizing_with_peer(originating_peer->shared_from_this());
      }

      for (const peer_connection_ptr& peer : peers_to_disconnect)
      {
        wlog("disconnecting client ${endpoint} because it offered us the rejected block", ("endpoint", peer->get_remote_endpoint()));
        disconnect_from_peer(peer.get(), disconnect_reason, true, disconnect_exception);
      }
    }

    void node_impl::process_block_message(peer_connection* originating_peer,
                                          const message& message_to_process,
                                          const message_hash_type& actual_message_hash,
                                          fc::time_point arrival_completed)
    {
      VERIFY_CORRECT_THREAD();
      // keep the peer from being deleted until we're done processing this message
      peer_connection_ptr peer = originating_peer->shared_from_this();

      // the message may be either a uncompressed or a compressed block message.  Either way
      // we just want to convert it to a full_block
      std::shared_ptr<full_block_type> full_block;
      fc::optional<message_hash_type> legacy_message_hash;
      switch (message_to_process.msg_type)
      {
      case core_message_type_enum::block_message_type:
        {
          graphene::net::block_message message(message_to_process.as_block_message( _thread_pool ));
          full_block = message.full_block;
          legacy_message_hash = actual_message_hash;
          // remove this check, just needed during initial development
          ++originating_peer->uncompressed_blocks_received_from_peer;
          break;
        }
      case core_message_type_enum::compressed_block_message_type:
        {
          graphene::net::compressed_block_message message(message_to_process.as_compressed_block_message( _thread_pool ));
          full_block = message.full_block;
          ++originating_peer->compressed_blocks_received_from_peer;
          break;
        }
      default:
        FC_THROW("unrecognized block message type ${type}", ("type", message_to_process.msg_type));
      }

      // find out whether we requested this item while we were synchronizing or during normal operation
      // (it's possible that we request an item during normal operation and then get kicked into sync
      // mode before we receive and process the item.  In that case, we should process the item as a normal
      // item to avoid confusing the sync code)
      auto sync_item_iter = originating_peer->sync_items_requested_from_peer.find(full_block->get_block_id());
      if (sync_item_iter != originating_peer->sync_items_requested_from_peer.end())
      {
        // it's a sync block
        originating_peer->sync_items_requested_from_peer.erase(sync_item_iter);
        // if exceptions are throw here after removing the sync item from the list (above),
        // it could leave our sync in a stalled state.  Wrap a try/catch around the rest
        // of the function so we can log if this ever happens.
        try
        {
          originating_peer->last_sync_item_received_time = fc::time_point::now();
          _active_sync_requests.erase(full_block->get_block_id());
          process_block_during_sync(originating_peer, full_block);
          if (originating_peer->idle())
          {
            // we have finished fetching a batch of items, so we either need to grab another batch of items
            // or we need to get another list of item ids.
            if (originating_peer->number_of_unfetched_item_ids > 0 &&
                originating_peer->ids_of_items_to_get.size() < GRAPHENE_NET_MIN_BLOCK_IDS_TO_PREFETCH)
              fetch_next_batch_of_item_ids_from_peer(originating_peer);
            else
              trigger_fetch_sync_items_loop();
          }
          return;
        }
        catch (const fc::canceled_exception& e)
        {
          throw;
        }
        catch (const fc::exception& e)
        {
          elog("Caught unexpected exception: ${e}", ("e", e));
          assert(false && "exceptions not expected here");
        }
        catch (const std::exception& e)
        {
          elog("Caught unexpected exception: ${e}", ("e", e.what()));
          assert(false && "exceptions not expected here");
        }
        catch (...)
        {
          elog("Caught unexpected exception, could break sync operation");
        }
      }
      else 
      {
        // it wasn't a sync block, check and see if it was sent during live mode.
        
        // old peers will advertise blocks using the message hash during live mode,
        // newer clients will use the block_id instead.
        fc::ripemd160 id_used_for_live_mode_blocks;
        if (originating_peer->advertise_blocks_by_block_id())
          id_used_for_live_mode_blocks = full_block->get_block_id();
        else if (legacy_message_hash)
          id_used_for_live_mode_blocks = *legacy_message_hash;
        else
          id_used_for_live_mode_blocks = full_block->get_legacy_block_message_hash();

        const item_id requested_item(graphene::net::block_message_type, id_used_for_live_mode_blocks);
        auto item_iter = originating_peer->items_requested_from_peer.find(requested_item);
        if (item_iter != originating_peer->items_requested_from_peer.end())
        {
          // judged by when the copy completed after the request left us (the request's time in our send queue is not
          // the peer's), against the deadline that had another advertiser asked for it
          const requested_items& requested = originating_peer->items_requested_from_peer;
          const bool late = arrival_completed - requested.block_request_sent(requested_item) > requested.block_deadline(requested_item);
          originating_peer->items_requested_from_peer.erase(item_iter);
          process_block_during_normal_operation(originating_peer, full_block, late);
          if (originating_peer->can_take_item_request())
            trigger_fetch_items_loop(peer_advertised_a_queued_block(originating_peer));
          return;
        }
      }

      // if we get here, we didn't request the message, we must have a misbehaving peer
      wlog("received a block ${block_id} I didn't ask for from peer ${endpoint}, disconnecting from peer",
           ("endpoint", originating_peer->get_remote_endpoint())
           ("block_id", full_block->get_block_id()));
      fc::exception detailed_error(FC_LOG_MESSAGE(error, "You sent me a block that I didn't ask for, block_id: ${block_id}",
                                                  ("block_id", full_block->get_block_id())
                                                  ("graphene_git_revision_sha", originating_peer->graphene_git_revision_sha)
                                                  ("graphene_git_revision_unix_timestamp", originating_peer->graphene_git_revision_unix_timestamp)
                                                  ("fc_git_revision_sha", originating_peer->fc_git_revision_sha)
                                                  ("fc_git_revision_unix_timestamp", originating_peer->fc_git_revision_unix_timestamp)));
      disconnect_from_peer(originating_peer, "You sent me a block that I didn't ask for", true, detailed_error);
    }

    void node_impl::process_trx_message(peer_connection* originating_peer,
                                        const trx_message& transaction_message_to_process)
    {
      VERIFY_CORRECT_THREAD();
      // keep the peer from being deleted until we're done processing this message
      peer_connection_ptr peer = originating_peer->shared_from_this();
      fc::time_point message_receive_time = fc::time_point::now();

      std::shared_ptr<full_transaction_type> full_transaction = transaction_message_to_process.full_transaction;

      // only process it if we asked for it
      const fc::ripemd160& message_hash = full_transaction->get_legacy_transaction_message_hash();
      if (is_relay_sample(message_hash))
        fc_ilog(fc::logger::get(GRAPHENE_NET_RELAY_SAMPLE_LOGGER), "relay sample received ${trx} ${hash} at ${time_us}",
                ("trx", full_transaction->get_transaction_id())("hash", message_hash)("time_us", raw_realtime_microseconds()));
      auto iter = originating_peer->items_requested_from_peer.find(item_id(trx_message_type, message_hash));
      if (iter == originating_peer->items_requested_from_peer.end())
      {
        wlog("received a transaction message I didn't ask for from peer ${endpoint}, disconnecting from peer", ("endpoint", originating_peer->get_remote_endpoint()));
        fc::exception detailed_error(FC_LOG_MESSAGE(error, "You sent me a message that I didn't ask for, message_hash: ${message_hash}", (message_hash)));
        disconnect_from_peer(originating_peer, "You sent me a message that I didn't request", true, detailed_error);
        return;
      }
      else
      {
        // wake the fetch loop only when this reply opens the peer's transaction window, so a peer kept
        // busy is asked again once per batch rather than per transaction
        const transaction_size_estimate& window = originating_peer->received_transaction_size;
        const bool window_had_room = window.transactions_to_request(originating_peer->items_requested_from_peer.transactions()) > 0;
        originating_peer->items_requested_from_peer.erase(iter);
        originating_peer->items_requested_from_peer.record_transaction_delivery(message_receive_time);
        originating_peer->transaction_busy_time.record_delivery(message_receive_time);
        originating_peer->received_transaction_size.record(full_transaction->get_transaction_size());
        _transaction_bytes_being_validated += full_transaction->get_transaction_size();
        // a busy peer's replies to us are draining
        const bool backlog_drained = std::exchange(originating_peer->transaction_replies_backlogged, false);
        if (originating_peer->idle() || backlog_drained)
          trigger_fetch_items_loop(peer_advertised_a_queued_block(originating_peer));
        else if (!window_had_room && originating_peer->can_take_item_request() &&
                 window.transactions_to_request(originating_peer->items_requested_from_peer.transactions()) > 0)
          trigger_fetch_items_loop(false);

        // Next: have the delegate validate the transaction; apply_transaction_completion() applies the outcome.
        // It counts as being processed until then, so no other peer is asked for it meanwhile
        _message_ids_currently_being_processed.insert(message_hash);
        try
        {
          // a transaction we asked for is submitted at once, whatever is in flight, so the peer's read loop never
          // waits for validation and its blocks are read as they come.  maximum_transactions_in_flight instead stops
          // the fetch passes from requesting more (transaction_fetch_window()) until enough have been validated
          _transactions_in_flight.acquire(0);
          const fc::time_point submitted_time = message_receive_time;
          const uint32_t in_flight_cap = _node_configuration.maximum_transactions_in_flight;
          if (in_flight_cap != 0 && _transactions_in_flight.in_flight() >= in_flight_cap && !_in_flight_cap_reached_since)
            _in_flight_cap_reached_since = message_receive_time;
          _statistics.record_submission(_transactions_in_flight.in_flight());
          try
          {
            dlog("passing message containing transaction ${trx} to client", ("trx", full_transaction->get_transaction_id()));
            transaction_completion completion{full_transaction, message_hash, message_receive_time, originating_peer->node_id, submitted_time};
            _delegate->submit_transaction(full_transaction,
              [completions = _transaction_completions, completion](const fc::exception_ptr& error) {
                transaction_completion validated = completion;
                validated.validated_time = fc::time_point::now();
                validated.error = error;
                completions->push(std::move(validated));
              });
          }
          catch (...)
          {
            _transactions_in_flight.release(1);
            throw;
          }
        }
        catch (const fc::canceled_exception&)
        {
          _message_ids_currently_being_processed.erase(message_hash);
          _transaction_bytes_being_validated -= full_transaction->get_transaction_size();
          throw;
        }
        catch (const fc::exception& e)
        {
          _message_ids_currently_being_processed.erase(message_hash);
          _transaction_bytes_being_validated -= full_transaction->get_transaction_size();
          dlog("client did not accept transaction message sent by peer ${peer} for validation, ${e}", ("peer", originating_peer->get_remote_endpoint())(e));
          // record it so we don't try to fetch this item again
          _recently_failed_items.insert(peer_connection::timestamped_item_id(item_id(trx_message_type, message_hash), fc::time_point::now()));
        }
        catch (...)
        {
          // clear the mark however submitting ends, as the block path does: an id left in the set makes us
          // skip every later advertisement of that transaction until restart
          _message_ids_currently_being_processed.erase(message_hash);
          _transaction_bytes_being_validated -= full_transaction->get_transaction_size();
          throw;
        }
      }
    }

    void node_impl::apply_transaction_completions()
    {
      VERIFY_CORRECT_THREAD();
      const std::deque<transaction_completion> completions = _transaction_completions->take_all();
      for (const transaction_completion& completion : completions)
      {
        _statistics.record_completion(completion.validated_time - completion.submitted_time);
        _transaction_bytes_being_validated -= completion.full_transaction->get_transaction_size();
        try
        {
          apply_transaction_completion(completion);
        }
        catch (const fc::canceled_exception&)
        {
          throw;
        }
        catch (const fc::exception& e)
        {
          elog("Unexpected exception applying the validation result of transaction ${trx}: ${e}",
               ("trx", completion.full_transaction->get_transaction_id())("e", e.to_detail_string()));
        }
        catch (const std::exception& e)
        {
          elog("Unexpected exception applying the validation result of transaction ${trx}: ${e}",
               ("trx", completion.full_transaction->get_transaction_id())("e", e.what()));
        }
      }
      _transactions_in_flight.release(completions.size());
      const uint32_t in_flight_cap = _node_configuration.maximum_transactions_in_flight;
      if (_in_flight_cap_reached_since && (in_flight_cap == 0 || _transactions_in_flight.in_flight() < in_flight_cap))
      {
        _statistics.record_limit_wait(fc::time_point::now() - *_in_flight_cap_reached_since);
        _in_flight_cap_reached_since.reset();
        trigger_fetch_items_loop(false);
      }
    }

    void node_impl::apply_transaction_completion(const transaction_completion& completion)
    {
      VERIFY_CORRECT_THREAD();
      _message_ids_currently_being_processed.erase(completion.message_hash);
      const item_id received_item_id(trx_message_type, completion.message_hash);

      if (completion.error)
      {
        dlog("client rejected transaction ${trx} sent by peer ${peer}, ${e}",
             ("trx", completion.full_transaction->get_transaction_id())("peer", completion.originating_peer)("e", completion.error->to_string()));
        // record it so we don't try to fetch this item again
        _recently_failed_items.insert(peer_connection::timestamped_item_id(received_item_id, fc::time_point::now()));
        return;
      }

      _validated_transaction_size.record(completion.full_transaction->get_transaction_size());
      _transaction_inventory.mark_received(completion.message_hash);
      if (const peer_connection_ptr originating_peer = get_peer_by_node_id(completion.originating_peer))
        record_requested_delivery(originating_peer.get());
      for (const peer_connection_ptr& active_peer : _active_connections)
      {
        ASSERT_TASK_NOT_PREEMPTED(); // don't yield while iterating over _active_connections
        process_deferred_transaction_advertisements(active_peer.get());
      }

      // finally, as the delegate validated the transaction, broadcast it to our other peers
      message_propagation_data propagation_data{completion.received_time, completion.validated_time, completion.originating_peer};
      broadcast(completion.full_transaction, propagation_data);
    }

    void node_impl::on_current_time_request_message(peer_connection* originating_peer,
                                                    const current_time_request_message& current_time_request_message_received)
    {
      VERIFY_CORRECT_THREAD();
      fc::time_point request_received_time(fc::time_point::now());
      current_time_reply_message reply(current_time_request_message_received.request_sent_time,
                                       request_received_time);
      originating_peer->send_message(reply, offsetof(current_time_reply_message, reply_transmitted_time));
    }

    void node_impl::on_current_time_reply_message(peer_connection* originating_peer,
                                                  const current_time_reply_message& current_time_reply_message_received)
    {
      VERIFY_CORRECT_THREAD();
      fc::time_point reply_received_time = fc::time_point::now();
      originating_peer->clock_offset = fc::microseconds(((current_time_reply_message_received.request_received_time - current_time_reply_message_received.request_sent_time) +
                                                         (current_time_reply_message_received.reply_transmitted_time - reply_received_time)).count() / 2);
      originating_peer->round_trip_delay = (reply_received_time - current_time_reply_message_received.request_sent_time) -
                                           (current_time_reply_message_received.reply_transmitted_time - current_time_reply_message_received.request_received_time);
    }

    void node_impl::forward_firewall_check_to_next_available_peer(firewall_check_state_data* firewall_check_state)
    {
      for (const peer_connection_ptr& peer : _active_connections)
      {
        if (firewall_check_state->expected_node_id != peer->node_id && // it's not the node who is asking us to test
            !peer->firewall_check_state && // the peer isn't already performing a check for another node
            firewall_check_state->nodes_already_tested.find(peer->node_id) == firewall_check_state->nodes_already_tested.end() &&
            peer->core_protocol_version >= GRAPHENE_NET_PROTOCOL_FIREWALL_CHECK_VERSION &&
            // a legacy peer can't be asked to check an IPv6 endpoint (not encodable in the legacy message)
            (peer->peer_supports_ipv6 || firewall_check_state->endpoint_to_test.get_address().is_ipv4()))
        {
          wlog("forwarding firewall check for node ${to_check} to peer ${checker}",
               ("to_check", firewall_check_state->endpoint_to_test)
               ("checker", peer->get_remote_endpoint()));
          firewall_check_state->nodes_already_tested.insert(peer->node_id);
          peer->firewall_check_state = firewall_check_state;
          if (peer->peer_supports_ipv6)
          {
            check_firewall_message check_request;
            check_request.endpoint_to_check = firewall_check_state->endpoint_to_test;
            check_request.node_id = firewall_check_state->expected_node_id;
            peer->send_message(check_request);
          }
          else
          {
            legacy_check_firewall_message check_request;
            check_request.endpoint_to_check = fc::ip::legacy_endpoint(firewall_check_state->endpoint_to_test);
            check_request.node_id = firewall_check_state->expected_node_id;
            peer->send_message(check_request);
          }
          return;
        }
      }
      wlog("Unable to forward firewall check for node ${to_check} to any other peers, returning 'unable'",
           ("to_check", firewall_check_state->endpoint_to_test));

      peer_connection_ptr originating_peer = get_peer_by_node_id(firewall_check_state->expected_node_id);
      if (originating_peer)
      {
        if (originating_peer->peer_supports_ipv6)
        {
          check_firewall_reply_message reply;
          reply.node_id = firewall_check_state->expected_node_id;
          reply.endpoint_checked = firewall_check_state->endpoint_to_test;
          reply.result = firewall_check_result::unable_to_check;
          originating_peer->send_message(reply);
        }
        else if (firewall_check_state->endpoint_to_test.get_address().is_ipv4())
        {
          legacy_check_firewall_reply_message reply;
          reply.node_id = firewall_check_state->expected_node_id;
          reply.endpoint_checked = fc::ip::legacy_endpoint(firewall_check_state->endpoint_to_test);
          reply.result = firewall_check_result::unable_to_check;
          originating_peer->send_message(reply);
        }
        else
          wlog("Not sending 'unable' firewall check reply to legacy peer ${peer}: endpoint ${endpoint} is not encodable in the legacy message",
               ("peer", originating_peer->get_remote_endpoint())
               ("endpoint", firewall_check_state->endpoint_to_test));
      }
      delete firewall_check_state;
    }

    void node_impl::on_check_firewall_message(peer_connection* originating_peer,
                                              const check_firewall_message& check_firewall_message_received)
    {
      VERIFY_CORRECT_THREAD();

      if (check_firewall_message_received.node_id == node_id_t() &&
          check_firewall_message_received.endpoint_to_check == fc::ip::endpoint())
      {
        // originating_peer is asking us to test whether it is firewalled
        // we're not going to try to connect back to the originating peer directly,
        // instead, we're going to coordinate requests by asking some of our peers
        // to try to connect to the originating peer, and relay the results back
        wlog("Peer ${peer} wants us to check whether it is firewalled", ("peer", originating_peer->get_remote_endpoint()));
        firewall_check_state_data* firewall_check_state = new firewall_check_state_data;
        // if they are using the same inbound and outbound port, try connecting to their outbound endpoint.
        // if they are using a different inbound port, use their outbound address but the inbound port they reported
        fc::ip::endpoint endpoint_to_check = originating_peer->get_socket().remote_endpoint();
        if (originating_peer->inbound_port != originating_peer->outbound_port)
          endpoint_to_check = fc::ip::endpoint(endpoint_to_check.get_address(), originating_peer->inbound_port);
        firewall_check_state->endpoint_to_test = endpoint_to_check;
        firewall_check_state->expected_node_id = originating_peer->node_id;
        firewall_check_state->requesting_peer = originating_peer->node_id;

        forward_firewall_check_to_next_available_peer(firewall_check_state);
      }
      else
      {
        // we're being asked to check another node
        // we can't perform the test if we're currently connected to that node, or if
        // we have no route to the endpoint (attempting it anyway would report
        // unable_to_connect, falsely branding them firewalled)
        if (is_already_connected_to_id(check_firewall_message_received.node_id) ||
            is_connection_to_endpoint_in_progress(check_firewall_message_received.endpoint_to_check) ||
            !can_route_to(check_firewall_message_received.endpoint_to_check.get_address()))
        {
          check_firewall_reply_message reply;
          reply.node_id = check_firewall_message_received.node_id;
          reply.endpoint_checked = check_firewall_message_received.endpoint_to_check;
          reply.result = firewall_check_result::unable_to_check;
          originating_peer->send_message(reply);
        }
        else
        {
          // we're not connected to them, so we need to set up a connection to them
          // to test.
          peer_connection_ptr peer_for_testing(create_peer_connection());
          peer_for_testing->firewall_check_state = new firewall_check_state_data;
          peer_for_testing->firewall_check_state->endpoint_to_test = check_firewall_message_received.endpoint_to_check;
          peer_for_testing->firewall_check_state->expected_node_id = check_firewall_message_received.node_id;
          peer_for_testing->firewall_check_state->requesting_peer = originating_peer->node_id;
          peer_for_testing->set_remote_endpoint(check_firewall_message_received.endpoint_to_check);
          initiate_connect_to(peer_for_testing);
        }
      }
    }

    void node_impl::on_check_firewall_reply_message(peer_connection* originating_peer,
                                                    const check_firewall_reply_message& check_firewall_reply_message_received)
    {
      VERIFY_CORRECT_THREAD();

      if (originating_peer->firewall_check_state &&
          originating_peer->firewall_check_state->requesting_peer != node_id_t())
      {
        // then this is a peer that is helping us check the firewalled state of one of our other peers
        // and they're reporting back
        // if they got a result, return it to the original peer.  if they were unable to check,
        // we'll try another peer.
        wlog("Peer ${reporter} reports firewall check status ${status} for ${peer}",
             ("reporter", originating_peer->get_remote_endpoint())
             ("status", check_firewall_reply_message_received.result)
             ("peer", check_firewall_reply_message_received.endpoint_checked));

        if (check_firewall_reply_message_received.result == firewall_check_result::unable_to_connect ||
            check_firewall_reply_message_received.result == firewall_check_result::connection_successful)
        {
          peer_connection_ptr original_peer = get_peer_by_node_id(originating_peer->firewall_check_state->requesting_peer);
          if (original_peer)
          {
            if (check_firewall_reply_message_received.result == firewall_check_result::connection_successful)
            {
              // if we previously thought this peer was firewalled, mark them as not firewalled
              if (original_peer->is_firewalled != firewalled_state::not_firewalled)
              {

                original_peer->is_firewalled = firewalled_state::not_firewalled;
                // there should be no old entry if we thought they were firewalled, so just create a new one
                fc::optional<fc::ip::endpoint> endpoint_for_db = originating_peer->get_endpoint_for_db();
                if (endpoint_for_db)
                {
                  potential_peer_record updated_peer_record = _potential_peer_db.lookup_or_create_entry_for_endpoint(*endpoint_for_db);
                  updated_peer_record.last_seen_time = fc::time_point::now();
                  _potential_peer_db.update_entry(updated_peer_record);
                }
              }
            }
            if (original_peer->peer_supports_ipv6)
            {
              original_peer->send_message(check_firewall_reply_message_received);
            }
            else if (check_firewall_reply_message_received.endpoint_checked.get_address().is_ipv4())
            {
              legacy_check_firewall_reply_message legacy_reply;
              legacy_reply.node_id = check_firewall_reply_message_received.node_id;
              legacy_reply.endpoint_checked = fc::ip::legacy_endpoint(check_firewall_reply_message_received.endpoint_checked);
              legacy_reply.result = check_firewall_reply_message_received.result;
              original_peer->send_message(legacy_reply);
            }
            else
              wlog("Not forwarding firewall check reply to legacy peer ${peer}: endpoint ${endpoint} is not encodable in the legacy message",
                   ("peer", original_peer->get_remote_endpoint())
                   ("endpoint", check_firewall_reply_message_received.endpoint_checked));
          }
          delete originating_peer->firewall_check_state;
          originating_peer->firewall_check_state = nullptr;
        }
        else
        {
          // they were unable to check for us, ask another peer
          firewall_check_state_data* firewall_check_state = originating_peer->firewall_check_state;
          originating_peer->firewall_check_state = nullptr;
          forward_firewall_check_to_next_available_peer(firewall_check_state);
        }
      }
      else if (originating_peer->firewall_check_state)
      {
        // this is a reply to a firewall check we initiated.
        wlog("Firewall check we initiated has returned with result: ${result}, endpoint = ${endpoint}",
             ("result", check_firewall_reply_message_received.result)
             ("endpoint", check_firewall_reply_message_received.endpoint_checked));
        if (check_firewall_reply_message_received.result == firewall_check_result::connection_successful)
        {
          _is_firewalled = firewalled_state::not_firewalled;
          _publicly_visible_listening_endpoint = check_firewall_reply_message_received.endpoint_checked;
          // not remembered as one of our own addresses (it is the replying peer's word, see on_connection_rejected_message)
        }
        else if (check_firewall_reply_message_received.result == firewall_check_result::unable_to_connect)
        {
          _is_firewalled = firewalled_state::firewalled;
          _publicly_visible_listening_endpoint = fc::optional<fc::ip::endpoint>();
        }
        delete originating_peer->firewall_check_state;
        originating_peer->firewall_check_state = nullptr;
      }
      else
      {
        wlog("Received a firewall check reply to a request I never sent");
      }

    }

    void node_impl::on_get_current_connections_request_message(peer_connection* originating_peer,
                                                               const get_current_connections_request_message& get_current_connections_request_message_received)
    {
      VERIFY_CORRECT_THREAD();
      get_current_connections_reply_message reply;

      if (!_average_network_read_speed_minutes.empty())
      {
        reply.upload_rate_one_minute = _average_network_write_speed_minutes.back();
        reply.download_rate_one_minute = _average_network_read_speed_minutes.back();

        size_t minutes_to_average = std::min(_average_network_write_speed_minutes.size(), (size_t)15);
        boost::circular_buffer<uint32_t>::iterator start_iter = _average_network_write_speed_minutes.end() - minutes_to_average;
        reply.upload_rate_fifteen_minutes = std::accumulate(start_iter, _average_network_write_speed_minutes.end(), 0u) / (uint32_t)minutes_to_average;
        start_iter = _average_network_read_speed_minutes.end() - minutes_to_average;
        reply.download_rate_fifteen_minutes = std::accumulate(start_iter, _average_network_read_speed_minutes.end(), 0u) / (uint32_t)minutes_to_average;

        minutes_to_average = std::min(_average_network_write_speed_minutes.size(), (size_t)60);
        start_iter = _average_network_write_speed_minutes.end() - minutes_to_average;
        reply.upload_rate_one_hour = std::accumulate(start_iter, _average_network_write_speed_minutes.end(), 0u) / (uint32_t)minutes_to_average;
        start_iter = _average_network_read_speed_minutes.end() - minutes_to_average;
        reply.download_rate_one_hour = std::accumulate(start_iter, _average_network_read_speed_minutes.end(), 0u) / (uint32_t)minutes_to_average;
      }

      fc::time_point now = fc::time_point::now();
      for (const peer_connection_ptr& peer : _active_connections)
      {
        ASSERT_TASK_NOT_PREEMPTED(); // don't yield while iterating over _active_connections

        current_connection_data data_for_this_peer;
        data_for_this_peer.connection_duration = now.sec_since_epoch() - peer->connection_initiation_time.sec_since_epoch();
        if (peer->get_remote_endpoint()) // should always be set for anyone we're actively connected to
          data_for_this_peer.remote_endpoint = *peer->get_remote_endpoint();
        data_for_this_peer.clock_offset = peer->clock_offset;
        data_for_this_peer.round_trip_delay = peer->round_trip_delay;
        data_for_this_peer.node_id = peer->node_id;
        data_for_this_peer.connection_direction = peer->direction;
        data_for_this_peer.firewalled = peer->is_firewalled;
        fc::mutable_variant_object user_data;
        if (peer->graphene_git_revision_sha)
          user_data["graphene_git_revision_sha"] = *peer->graphene_git_revision_sha;
        if (peer->graphene_git_revision_unix_timestamp)
          user_data["graphene_git_revision_unix_timestamp"] = *peer->graphene_git_revision_unix_timestamp;
        if (peer->fc_git_revision_sha)
          user_data["fc_git_revision_sha"] = *peer->fc_git_revision_sha;
        if (peer->fc_git_revision_unix_timestamp)
          user_data["fc_git_revision_unix_timestamp"] = *peer->fc_git_revision_unix_timestamp;
        if (peer->platform)
          user_data["platform"] = *peer->platform;
        if (peer->bitness)
          user_data["bitness"] = *peer->bitness;
        user_data["user_agent"] = peer->user_agent;

        user_data["last_known_block_hash"] = peer->last_block_delegate_has_seen;
        user_data["last_known_block_number"] = _delegate->get_block_number(peer->last_block_delegate_has_seen);
        user_data["last_known_block_time"] = peer->last_block_time_delegate_has_seen;

        data_for_this_peer.user_data = user_data;
        reply.current_connections.emplace_back(data_for_this_peer);
      }
      originating_peer->send_message(reply);
    }

    void node_impl::on_get_current_connections_reply_message(peer_connection* originating_peer,
                                                             const get_current_connections_reply_message& get_current_connections_reply_message_received)
    {
      VERIFY_CORRECT_THREAD();
    }


    // currently, there are no defined messages that would invoke this code, so this must be a message from a future version.
    // we shouldn't request items that we don't expect to be able to handle, so it must be unsolicited.
    void node_impl::process_ordinary_message(peer_connection* originating_peer,
                                             const message& message_to_process, const message_hash_type& message_hash)
    {
      VERIFY_CORRECT_THREAD();
      wlog("Received an unrecognized message type ${type} from peer ${endpoint}, ignoring it",
           ("type", message_to_process.msg_type)("endpoint", originating_peer->get_remote_endpoint()));
    }

    void node_impl::start_synchronizing_with_peer( const peer_connection_ptr& peer )
    {
      VERIFY_CORRECT_THREAD();
      peer->ids_of_items_to_get.clear();
      peer->number_of_unfetched_item_ids = 0;
      peer->we_need_sync_items_from_peer = true;
      peer->last_block_delegate_has_seen = item_hash_t();
      // the genesis time, not the epoch: the plausible number of sync blocks is measured from it
      peer->last_block_time_delegate_has_seen = _delegate->get_block_time(item_hash_t());
      peer->inhibit_fetching_sync_blocks = false;
      fetch_next_batch_of_item_ids_from_peer( peer.get() );
    }

    void node_impl::start_synchronizing()
    {
      for( const peer_connection_ptr& peer : _active_connections )
        start_synchronizing_with_peer( peer );
    }

    void node_impl::new_peer_just_added( const peer_connection_ptr& peer )
    {
      VERIFY_CORRECT_THREAD();
      peer->send_message(current_time_request_message(),
                         offsetof(current_time_request_message, request_sent_time));
      start_synchronizing_with_peer( peer );
      if( _active_connections.size() != _last_reported_number_of_connections )
      {
        _last_reported_number_of_connections = (uint32_t)_active_connections.size();
        _delegate->connection_count_changed( _last_reported_number_of_connections );
      }
    }

    void node_impl::close()
    {
      VERIFY_CORRECT_THREAD();

      _node_is_shutting_down = true;

      // transactions still being validated are discarded when their validation ends
      _transaction_completions->close();
      _transactions_in_flight.close();

      try
      {
        save_active_peers_to_peer_database();
      }
      catch (const fc::exception &e)
      {
        wlog("Exception thrown while saving active peers to peer database, ignoring exception: ${e}", ("e", e));
      }
      catch (...)
      {
        wlog( "Exception thrown while saving active peers to peer database, ignoring exception" );
      }

      try
      {
        _potential_peer_db.close();
      }
      catch ( const fc::exception& e )
      {
        wlog( "Exception thrown while closing P2P peer database, ignoring exception: ${e}", ("e", e) );
      }
      catch (...)
      {
        wlog( "Exception thrown while closing P2P peer database, ignoring exception" );
      }

      // First, stop accepting incoming network connections
      try
      {
        _tcp_server.close();
        dlog("P2P TCP server closed");
      }
      catch ( const fc::exception& e )
      {
        wlog( "Exception thrown while closing P2P TCP server, ignoring: ${e}", ("e", e) );
      }
      catch (...)
      {
        wlog( "Exception thrown while closing P2P TCP server, ignoring" );
      }

      try
      {
        _accept_loop_complete.cancel_and_wait("node_impl::close()");
        dlog("P2P accept loop terminated");
      }
      catch ( const fc::exception& e )
      {
        wlog( "Exception thrown while terminating P2P accept loop, ignoring: ${e}", ("e", e) );
      }
      catch (...)
      {
        wlog( "Exception thrown while terminating P2P accept loop, ignoring" );
      }

      // terminate all of our long-running loops (these run continuously instead of rescheduling themselves)
      try
      {
        _p2p_network_connect_loop_done.cancel_and_wait("node_impl::close()");
        dlog("P2P connect loop terminated");
      }
      catch ( const fc::canceled_exception& )
      {
        dlog("P2P connect loop terminated");
      }
      catch ( const fc::exception& e )
      {
        wlog( "Exception thrown while terminating P2P connect loop, ignoring: ${e}", ("e", e) );
      }
      catch (...)
      {
        wlog( "Exception thrown while terminating P2P connect loop, ignoring" );
      }

      try
      {
        _process_backlog_of_sync_blocks_done.cancel_and_wait("node_impl::close()");
        dlog("Process backlog of sync items task terminated");
      }
      catch ( const fc::canceled_exception& )
      {
        dlog("Process backlog of sync items task terminated");
      }
      catch ( const fc::exception& e )
      {
        wlog( "Exception thrown while terminating Process backlog of sync items task, ignoring: ${e}", ("e", e) );
      }
      catch (...)
      {
        wlog( "Exception thrown while terminating Process backlog of sync items task, ignoring" );
      }

      unsigned handle_message_call_count = 0;
      while( true )
      {
        auto it = _handle_message_calls_in_progress.begin();
        if( it == _handle_message_calls_in_progress.end() )
           break;
        if( it->ready() || it->error() || it->canceled() )
        {
           _handle_message_calls_in_progress.erase( it );
           continue;
        }
        ++handle_message_call_count;
        try
        {
          it->cancel_and_wait("node_impl::close()");
          dlog("handle_message call #${count} task terminated", ("count", handle_message_call_count));
        }
        catch ( const fc::canceled_exception& )
        {
          dlog("handle_message call #${count} task terminated", ("count", handle_message_call_count));
        }
        catch ( const fc::exception& e )
        {
          wlog("Exception thrown while terminating handle_message call #${count} task, ignoring: ${e}", ("e", e)("count", handle_message_call_count));
        }
        catch (...)
        {
          wlog("Exception thrown while terminating handle_message call #${count} task, ignoring",("count", handle_message_call_count));
        }
      }

      try
      {
        _fetch_sync_items_loop_done.cancel("node_impl::close()");
        // cancel() is currently broken, so we need to wake up the task to allow it to finish
        trigger_fetch_sync_items_loop();
        _fetch_sync_items_loop_done.wait();
        dlog("Fetch sync items loop terminated");
      }
      catch ( const fc::canceled_exception& )
      {
        dlog("Fetch sync items loop terminated");
      }
      catch ( const fc::exception& e )
      {
        wlog( "Exception thrown while terminating Fetch sync items loop, ignoring: ${e}", ("e", e) );
      }
      catch (...)
      {
        wlog( "Exception thrown while terminating Fetch sync items loop, ignoring" );
      }

      try
      {
        _fetch_item_loop_done.cancel("node_impl::close()");
        // cancel() is currently broken, so we need to wake up the task to allow it to finish
        trigger_fetch_items_loop(true);
        _fetch_item_loop_done.wait();
        dlog("Fetch items loop terminated");
      }
      catch ( const fc::canceled_exception& )
      {
        dlog("Fetch items loop terminated");
      }
      catch ( const fc::exception& e )
      {
        wlog( "Exception thrown while terminating Fetch items loop, ignoring: ${e}", ("e", e) );
      }
      catch (...)
      {
        wlog( "Exception thrown while terminating Fetch items loop, ignoring" );
      }

      try
      {
        _advertise_inventory_loop_done.cancel("node_impl::close()");
        // cancel() is currently broken, so we need to wake up the task to allow it to finish
        trigger_advertise_inventory_loop();
        _advertise_inventory_loop_done.wait();
        dlog("Advertise inventory loop terminated");
      }
      catch ( const fc::canceled_exception& )
      {
        dlog("Advertise inventory loop terminated");
      }
      catch ( const fc::exception& e )
      {
        wlog( "Exception thrown while terminating Advertise inventory loop, ignoring: ${e}", ("e", e) );
      }
      catch (...)
      {
        wlog( "Exception thrown while terminating Advertise inventory loop, ignoring" );
      }


      // Next, terminate our existing connections.  First, close all of the connections nicely.
      // This will close the sockets and may result in calls to our "on_connection_closing"
      // method to inform us that the connection really closed (or may not if we manage to cancel
      // the read loop before it gets an EOF).
      // operate off copies of the lists in case they change during iteration
      std::list<peer_connection_ptr> all_peers;
      boost::push_back(all_peers, _active_connections);
      boost::push_back(all_peers, _handshaking_connections);
      boost::push_back(all_peers, _closing_connections);

      do
      {
         ilog("Attempting to destroy connection on ${c} peers...", ("c", all_peers.size()));

         // and delete all of the peer_connection objects
         _active_connections.clear();
         _handshaking_connections.clear();
         _closing_connections.clear();

         for (const peer_connection_ptr& peer : all_peers)
         {
         try
         {
            peer->destroy_connection(__FUNCTION__);
         }
         catch ( const fc::exception& e )
         {
            wlog( "Exception thrown while closing peer connection, ignoring: ${e}", ("e", e) );
         }
         catch (...)
         {
            wlog( "Exception thrown while closing peer connection, ignoring" );
         }
         }

         all_peers.clear();
         boost::push_back(all_peers, _active_connections);
         boost::push_back(all_peers, _handshaking_connections);
         boost::push_back(all_peers, _closing_connections);
      }
      while(all_peers.empty() == false);

      {
#ifdef USE_PEERS_TO_DELETE_MUTEX
        fc::scoped_lock<fc::mutex> lock(_peers_to_delete_mutex);
#endif
        try
        {
          _delayed_peer_deletion_task_done.cancel_and_wait("node_impl::close()");
          dlog("Delayed peer deletion task terminated");
        }
        catch ( const fc::exception& e )
        {
          wlog( "Exception thrown while terminating Delayed peer deletion task, ignoring: ${e}", ("e", e) );
        }
        catch (...)
        {
          wlog( "Exception thrown while terminating Delayed peer deletion task, ignoring" );
        }
        _peers_to_delete.clear();
      }

      // Now that there are no more peers that can call methods on us, there should be no
      // chance for one of our loops to be rescheduled, so we can safely terminate all of
      // our loops now
      try
      {
        _terminate_inactive_connections_loop_done.cancel_and_wait("node_impl::close()");
        dlog("Terminate inactive connections loop terminated");
      }
      catch ( const fc::exception& e )
      {
        wlog( "Exception thrown while terminating Terminate inactive connections loop, ignoring: ${e}", ("e", e) );
      }
      catch (...)
      {
        wlog( "Exception thrown while terminating Terminate inactive connections loop, ignoring" );
      }
      /// Clear container to immediately deallocate held peer_connection::ptr objects
      _terminating_connections.clear();

      try
      {
        _fetch_updated_peer_lists_loop_done.cancel_and_wait("node_impl::close()");
        dlog("Fetch updated peer lists loop terminated");
      }
      catch ( const fc::exception& e )
      {
        wlog( "Exception thrown while terminating Fetch updated peer lists loop, ignoring: ${e}", ("e", e) );
      }
      catch (...)
      {
        wlog( "Exception thrown while terminating Fetch updated peer lists loop, ignoring" );
      }

      try
      {
        _bandwidth_monitor_loop_done.cancel_and_wait("node_impl::close()");
        dlog("Bandwidth monitor loop terminated");
      }
      catch ( const fc::exception& e )
      {
        wlog( "Exception thrown while terminating Bandwidth monitor loop, ignoring: ${e}", ("e", e) );
      }
      catch (...)
      {
        wlog( "Exception thrown while terminating Bandwidth monitor loop, ignoring" );
      }

      try
      {
        _dump_node_status_task_done.cancel_and_wait("node_impl::close()");
        dlog("Dump node status task terminated");
      }
      catch ( const fc::exception& e )
      {
        wlog( "Exception thrown while terminating Dump node status task, ignoring: ${e}", ("e", e) );
      }
      catch (...)
      {
        wlog( "Exception thrown while terminating Dump node status task, ignoring" );
      }

      try
      {
        _statistics_report_loop_done.cancel_and_wait("node_impl::close()");
        dlog("Statistics report loop terminated");
      }
      catch ( const fc::exception& e )
      {
        wlog( "Exception thrown while terminating Statistics report loop, ignoring: ${e}", ("e", e) );
      }
      catch (...)
      {
        wlog( "Exception thrown while terminating Statistics report loop, ignoring" );
      }
    } // node_impl::close()

    void node_impl::accept_connection_task( const peer_connection_ptr& new_peer )
    {
      try
      {
        new_peer->accept_connection(); // this blocks until the secure connection is fully negotiated
      }
      catch (const fc::canceled_exception&)
      {
        throw;
      }
      catch (...)
      {
        // the key exchange failed (accept_connection() has logged why): the remote closed the connection, or we
        // closed it when the handshake timed out.  The read loop only starts after a successful key exchange, so
        // on_connection_closed() is never called for this peer; release it here, as connect_to_task() does when
        // an outbound connection fails
        _handshaking_connections.erase(new_peer);
        _terminating_connections.erase(new_peer);
        assert(_active_connections.find(new_peer) == _active_connections.end());
        _active_connections.erase(new_peer);
        assert(_closing_connections.find(new_peer) == _closing_connections.end());
        _closing_connections.erase(new_peer);
        schedule_peer_for_deletion(new_peer);
        return;
      }
      send_hello_message(new_peer);
    }

    peer_connection_ptr node_impl::create_peer_connection()
    {
      VERIFY_CORRECT_THREAD();
      peer_connection_ptr new_peer(peer_connection::make_shared(this));
      new_peer->set_send_flush_delay(fc::microseconds(_node_configuration.transaction_flush_delay_microseconds));
      return new_peer;
    }

    void node_impl::accept_loop()
    {
      while ( !_accept_loop_complete.canceled() )
      {
        peer_connection_ptr new_peer(create_peer_connection());

        try
        {
          _tcp_server.accept( new_peer->get_socket() );
          if (node_is_shutting_down())
            return;
          const fc::ip::endpoint remote_endpoint = new_peer->get_socket().remote_endpoint();
          if (const char* refusal = get_inbound_connection_refusal(remote_endpoint.get_address()))
          {
            dlog("closing inbound connection from ${remote_endpoint} at accept: ${refusal}", (remote_endpoint)(refusal));
            warn_of_refused_inbound_connections();
            new_peer->get_socket().close();
          }
          else
          {
            new_peer->get_socket().set_receive_buffer_size(MAX_MESSAGE_SIZE);
            new_peer->get_socket().set_send_buffer_size(MAX_MESSAGE_SIZE);
            idump((new_peer->get_socket().get_no_delay()));
            new_peer->get_socket().set_no_delay(true);
            idump((new_peer->get_socket().get_no_delay()));
            ilog( "accepted inbound connection from ${remote_endpoint}", (remote_endpoint) );
            // known from now on, so the connection counts against its host's limits during the key exchange too
            new_peer->direction = peer_connection_direction::inbound;
            new_peer->set_remote_endpoint(remote_endpoint);
            new_peer->connection_initiation_time = fc::time_point::now();
            _handshaking_connections.insert( new_peer );
            _rate_limiter.add_tcp_socket( &new_peer->get_socket() );
            std::weak_ptr<peer_connection> new_weak_peer(new_peer);
            new_peer->accept_or_connect_task_done = async_task( [this, new_weak_peer]() {
              peer_connection_ptr new_peer(new_weak_peer.lock());
              assert(new_peer);
              if (!new_peer)
                return;
              accept_connection_task(new_peer);
            }, "accept_connection_task" );
          }

          // limit the rate at which we accept connections to mitigate DOS attacks
          fc::usleep( fc::milliseconds(10) );
        } FC_CAPTURE_AND_LOG( () )
      }
    } // accept_loop()

    void node_impl::send_connection_rejected_message(peer_connection* peer,
                                                     const fc::ip::endpoint& remote_endpoint,
                                                     rejection_reason_code reason_code,
                                                     const std::string& reason_string)
    {
      // The legacy message can only encode an IPv4 endpoint.  A peer on an IPv6 socket that
      // hasn't identified as IPv6-aware (nonconforming client) still gets the new message:
      // legacy nodes ignore unknown message types, and callers disconnect afterward anyway.
      if (peer->peer_supports_ipv6 || !remote_endpoint.get_address().is_ipv4())
      {
        connection_rejected_message rejection(_user_agent_string, core_protocol_version,
                                              remote_endpoint, reason_code, reason_string);
        peer->send_message(message(rejection));
      }
      else
      {
        legacy_connection_rejected_message rejection(_user_agent_string, core_protocol_version,
                                                     fc::ip::legacy_endpoint(remote_endpoint),
                                                     reason_code, reason_string);
        peer->send_message(message(rejection));
      }
    }

    void node_impl::send_hello_message(const peer_connection_ptr& peer)
    {
      VERIFY_CORRECT_THREAD();
      peer->negotiation_status = peer_connection::connection_negotiation_status::hello_sent;

      fc::sha256::encoder shared_secret_encoder;
      fc::sha512 shared_secret = peer->get_shared_secret();
      shared_secret_encoder.write(shared_secret.data(), sizeof(shared_secret));
      fc::ecc::compact_signature signature = _node_configuration.private_key.sign_compact(shared_secret_encoder.result());

      // in the hello messsage, we send three things:
      //  ip address
      //  outbound port
      //  inbound port
      // The peer we're connecting to will assume we're firewalled if the
      // ip address and outbound port we send don't match the values it sees on its remote endpoint
      //
      // if we know that we're behind a NAT that will allow incoming connections because our firewall
      // detection figured it out, send those values instead.

      fc::ip::endpoint local_endpoint(*peer->get_local_endpoint());
      uint16_t listening_port = _node_configuration.accept_incoming_connections ? _actual_listening_endpoint.port() : 0;

      if (_is_firewalled == firewalled_state::not_firewalled &&
          _publicly_visible_listening_endpoint)
      {
        local_endpoint = *_publicly_visible_listening_endpoint;
        listening_port = _publicly_visible_listening_endpoint->port();
      }

      fc::variant_object user_data = generate_hello_user_data();

      // Send new IPv6-aware hello first, then legacy hello.
      // New nodes process the new hello and skip the legacy one.
      // Old nodes ignore the new hello (unknown message type) and process the legacy one.
      hello_message hello(_user_agent_string,
                          core_protocol_version,
                          local_endpoint.get_address(),
                          listening_port,
                          local_endpoint.port(),
                          _node_public_key,
                          signature,
                          user_data);
      peer->send_message(message(hello));

      // The legacy hello can't encode a native IPv6 address, and a peer reached over IPv6
      // runs IPv6-aware code that already processed the new hello above.
      if (local_endpoint.get_address().is_ipv4())
      {
        legacy_hello_message legacy_hello(_user_agent_string,
                                          core_protocol_version,
                                          fc::ip::legacy_address(local_endpoint.get_address()),
                                          listening_port,
                                          local_endpoint.port(),
                                          _node_public_key,
                                          signature,
                                          user_data);
        peer->send_message(message(legacy_hello));
      }
    }

    void node_impl::connect_to_task(const peer_connection_ptr& new_peer,
                                    const fc::ip::endpoint& remote_endpoint)
    {
      VERIFY_CORRECT_THREAD();

      if (!new_peer->performing_firewall_check())
      {
        // create or find the database entry for the new peer
        // if we're connecting to them, we believe they're not firewalled
        potential_peer_record updated_peer_record = _potential_peer_db.lookup_or_create_entry_for_endpoint(remote_endpoint);
        updated_peer_record.last_connection_attempt_time = fc::time_point::now();;
        _potential_peer_db.update_entry(updated_peer_record);
      }
      else
      {
        wlog("connecting to peer ${peer} for firewall check", ("peer", new_peer->get_remote_endpoint()));
      }

      fc::oexception connect_failed_exception;
      fc::optional<fc::ip::endpoint> local_endpoint;

      try
      {
        // Bind the outbound socket's local side to our advertised listening port so
        // peers see us connecting from it. initiate_connect_to() opened the socket for
        // the remote peer's address family, so the local bind address must use that same
        // family or the bind fails (EINVAL for an IPv4 address on an IPv6 socket,
        // EAFNOSUPPORT for an IPv6 address on an IPv4 socket). When we listen on a
        // wildcard address, substitute the wildcard of the remote's family while keeping
        // the port; when we listen on a specific address, only bind it if the families
        // match, otherwise let the OS choose the local endpoint.
        if (_actual_listening_endpoint.port() != 0)
        {
          static const fc::ip::address ipv4_any;                       // 0.0.0.0
          static const fc::ip::address ipv6_any{ fc::string("::") };   // ::
          const fc::ip::address& listen_address = _actual_listening_endpoint.get_address();
          const bool remote_is_ipv6 = remote_endpoint.get_address().is_ipv6();
          if (listen_address == ipv4_any || listen_address == ipv6_any)
            local_endpoint = fc::ip::endpoint(remote_is_ipv6 ? ipv6_any : ipv4_any,
                                              _actual_listening_endpoint.port());
          else if (listen_address.is_ipv6() == remote_is_ipv6)
            local_endpoint = _actual_listening_endpoint;
          // else: specific listening address of the other family; let the OS pick the local endpoint
        }

        new_peer->connect_to(remote_endpoint, local_endpoint);  // blocks until the connection is established and secure connection is negotiated

        // we connected to the peer.  guess they're not firewalled....
        new_peer->is_firewalled = firewalled_state::not_firewalled;

        // connection succeeded, we've started handshaking.  record that in our database
        potential_peer_record updated_peer_record = _potential_peer_db.lookup_or_create_entry_for_endpoint(remote_endpoint);
        updated_peer_record.last_connection_disposition = last_connection_handshaking_failed;
        updated_peer_record.number_of_successful_connection_attempts++;
        updated_peer_record.last_seen_time = fc::time_point::now();
        _potential_peer_db.update_entry(updated_peer_record);
      }
      catch (const fc::exception& except)
      {
        connect_failed_exception = except;
      }

      if (connect_failed_exception && !new_peer->performing_firewall_check() &&
          local_endpoint && is_local_endpoint_in_use_error(*connect_failed_exception))
      {
        // our previous connection to this endpoint still holds the address/port pair in the kernel
        // (e.g. TIME_WAIT after we closed it); the peer did nothing wrong, so its record keeps its
        // failure history and the connect loop simply tries again on a later pass
        ilog("could not connect to ${peer} from ${local}: our previous connection to it still holds that address pair, will retry",
             ("peer", remote_endpoint)("local", *local_endpoint));
      }
      else if (connect_failed_exception && !new_peer->performing_firewall_check())
      {
        // connection failed.  record that in our database
        potential_peer_record updated_peer_record = _potential_peer_db.lookup_or_create_entry_for_endpoint(remote_endpoint);
        updated_peer_record.last_connection_disposition = last_connection_failed;
        updated_peer_record.number_of_failed_connection_attempts++;
        updated_peer_record.last_failed_time = fc::time_point::now();
        if (new_peer->connection_closed_error)
          updated_peer_record.last_error = *new_peer->connection_closed_error;
        else
          updated_peer_record.last_error = *connect_failed_exception;
        _potential_peer_db.update_entry(updated_peer_record);
      }

      if (new_peer->performing_firewall_check())
      {
        // we were connecting to test whether the node is firewalled, and we now know the result.
        // send a message back to the requester
        peer_connection_ptr requesting_peer = get_peer_by_node_id(new_peer->firewall_check_state->requesting_peer);
        if (requesting_peer)
        {
          check_firewall_reply_message reply;
          reply.endpoint_checked = new_peer->firewall_check_state->endpoint_to_test;
          reply.node_id = new_peer->firewall_check_state->expected_node_id;
          reply.result = connect_failed_exception ?
                           firewall_check_result::unable_to_connect :
                           firewall_check_result::connection_successful;
          wlog("firewall check of ${peer_checked} ${success_or_failure}, sending reply to ${requester}",
                ("peer_checked", new_peer->get_remote_endpoint())
                ("success_or_failure", connect_failed_exception ? "failed" : "succeeded" )
                ("requester", requesting_peer->get_remote_endpoint()));

          requesting_peer->send_message(reply);
        }
      }

      if (connect_failed_exception || new_peer->performing_firewall_check())
      {
        // if the connection failed or if this connection was just intended to check
        // whether the peer is firewalled, we want to disconnect now.
        _handshaking_connections.erase(new_peer);
        _terminating_connections.erase(new_peer);
        assert(_active_connections.find(new_peer) == _active_connections.end());
        _active_connections.erase(new_peer);
        assert(_closing_connections.find(new_peer) == _closing_connections.end());
        _closing_connections.erase(new_peer);

        display_current_connections();
        trigger_p2p_network_connect_loop();
        schedule_peer_for_deletion(new_peer);

        if (connect_failed_exception)
          throw *connect_failed_exception;
      }
      else
      {
        // connection was successful and we want to stay connected
        fc::ip::endpoint local_endpoint = *new_peer->get_local_endpoint();
        new_peer->inbound_address = local_endpoint.get_address();
        new_peer->inbound_port = _node_configuration.accept_incoming_connections ? _actual_listening_endpoint.port() : 0;
        new_peer->outbound_port = local_endpoint.port();

        new_peer->our_state = peer_connection::our_connection_state::just_connected;
        new_peer->their_state = peer_connection::their_connection_state::just_connected;
        send_hello_message(new_peer);
        dlog("Sent \"hello\" to peer ${peer}", ("peer", new_peer->get_remote_endpoint()));
      }
    }

    // methods implementing node's public interface
    void node_impl::set_node_delegate(node_delegate* del)
    {
      VERIFY_CORRECT_THREAD();
      _delegate.reset();
      if (del)
        _delegate.reset(new statistics_gathering_node_delegate_wrapper(del));
    }

    void node_impl::load_configuration( const fc::path& configuration_directory )
    {
      VERIFY_CORRECT_THREAD();
      _node_configuration_directory = configuration_directory;
      fc::path configuration_file_name( _node_configuration_directory / NODE_CONFIGURATION_FILENAME );
      bool node_configuration_loaded = false;
      if( fc::exists(configuration_file_name ) )
      {
        try
        {
          _node_configuration = fc::json::from_file( configuration_file_name, fc::json::format_validation_mode::full ).as<node_configuration>();
          ilog( "Loaded configuration from file ${filename}", ("filename", configuration_file_name ) );

          if( _node_configuration.private_key == fc::ecc::private_key() )
          {
            ilog( "generating new private key for this node" );
            _node_configuration.private_key = fc::ecc::private_key::generate();
          }

          // node_config.json holds the whole configuration of the last run, so a value saved there may be a default of
          // an older build rather than the operator's choice: these are chosen with each run's p2p-parameters
          // (set_advanced_node_parameters(), called after this)
          _node_configuration.relax_per_host_limit = node_configuration().relax_per_host_limit;
          _node_configuration.silent_peer_disconnect_timeout = node_configuration().silent_peer_disconnect_timeout;
          _node_configuration.peer_expansion_timeout = node_configuration().peer_expansion_timeout;

          node_configuration_loaded = true;
        }
        catch ( fc::parse_error_exception& parse_error )
        {
          elog( "malformed node configuration file ${filename}: ${error}",
               ( "filename", configuration_file_name )("error", parse_error.to_detail_string() ) );
        }
        catch ( fc::exception& except )
        {
          elog( "unexpected exception while reading configuration file ${filename}: ${error}",
               ( "filename", configuration_file_name )("error", except.to_detail_string() ) );
        }
      }

      if( !node_configuration_loaded )
      {
        _node_configuration = node_configuration();

#ifdef GRAPHENE_TEST_NETWORK
        uint32_t port = GRAPHENE_NET_TEST_P2P_PORT + GRAPHENE_TEST_NETWORK_VERSION;
#else
        uint32_t port = GRAPHENE_NET_DEFAULT_P2P_PORT;
#endif
        _node_configuration.listen_endpoint.set_port( port );
        _node_configuration.accept_incoming_connections = true;
        _node_configuration.wait_if_endpoint_is_busy = false;

        ilog( "generating new private key for this node" );
        _node_configuration.private_key = fc::ecc::private_key::generate();
      }

      _node_public_key = _node_configuration.private_key.get_public_key().serialize();

      fc::path potential_peer_database_file_name(_node_configuration_directory / POTENTIAL_PEER_DATABASE_FILENAME);
      try
      {
        _potential_peer_db.open(potential_peer_database_file_name);

        // push back the time on all peers loaded from the database so we will be able to retry them immediately
        for (peer_database::iterator itr = _potential_peer_db.begin(); itr != _potential_peer_db.end(); ++itr)
        {
          potential_peer_record updated_peer_record = *itr;
          ddump((updated_peer_record));
          updated_peer_record.last_connection_attempt_time = std::min<fc::time_point_sec>(updated_peer_record.last_connection_attempt_time,
                                                                                          fc::time_point::now() - fc::seconds(_node_configuration.peer_connection_retry_timeout));
          _potential_peer_db.update_entry(updated_peer_record);
        }
        trigger_p2p_network_connect_loop();
      }
      catch (fc::exception& except)
      {
        elog("unable to open peer database ${filename}: ${error}",
             ("filename", potential_peer_database_file_name)("error", except.to_detail_string()));
        throw;
      }
    }

    void node_impl::listen_to_p2p_network(std::function<bool()> break_callback)
    {
      VERIFY_CORRECT_THREAD();
      if (!_node_configuration.accept_incoming_connections)
      {
        wlog("accept_incoming_connections is false, p2p network will not accept any incoming connections");
        return;
      }

      assert(_node_public_key != fc::ecc::public_key_data());

      fc::ip::endpoint listen_endpoint = _node_configuration.listen_endpoint;
      if (listen_endpoint.port() != 0)
      {
        // if the user specified a port, we only want to bind to it if it's not already
        // being used by another application.  During normal operation, we set the
        // SO_REUSEADDR/SO_REUSEPORT flags so that we can bind outbound sockets to the
        // same local endpoint as we're listening on here.  On some platforms, setting
        // those flags will prevent us from detecting that other applications are
        // listening on that port.  We'd like to detect that, so we'll set up a temporary
        // tcp server without that flag to see if we can listen on that port.
        bool first = true;
        for (;;)
        {
          if (break_callback())
          {
            std::string break_message = "Node manually closed - probably SIGINT/SIGTERM signal appeared";
            ulog(break_message);
            wlog(break_message);
            return;
          }
          bool listen_failed = false;

          try
          {
            fc::tcp_server temporary_server;
            // Set SO_REUSEADDR (without SO_REUSEPORT) so the check succeeds when
            // the port is only in TIME_WAIT from a previous run, but still fails
            // if another process is actively listening on it.
            temporary_server.set_reuse_address(true, false);
            if (listen_endpoint.get_address() != fc::ip::address())
              temporary_server.listen(listen_endpoint);
            else
              temporary_server.listen(listen_endpoint.port());
            break;
          }
          catch ( const fc::exception&)
          {
            listen_failed = true;
          }

          if (listen_failed)
          {
            if (_node_configuration.wait_if_endpoint_is_busy)
            {
              std::ostringstream error_message_stream;
              if (first)
              {
                error_message_stream << "Unable to listen for connections on port " << listen_endpoint.port()
                                     << ", retrying in a few seconds\n";
                error_message_stream << "You can wait for it to become available, or restart this program using\n";
                error_message_stream << "the --p2p-port option to specify another port\n";
                first = false;
              }
              else
              {
                error_message_stream << "\nStill waiting for port " << listen_endpoint.port() << " to become available\n";
              }
              std::string error_message = error_message_stream.str();
              ulog(error_message);
              wlog(error_message);
              _delegate->error_encountered(error_message, fc::oexception());
              fc::usleep(fc::seconds(GRAPHENE_NET_PORT_WAIT_DELAY_SECONDS));
            }
            else // don't wait, just find a random port
            {
              wlog("unable to bind on the requested endpoint ${listen_endpoint}, which probably means that endpoint is already in use", (listen_endpoint));
              listen_endpoint.set_port(0);
            }
          } // if (listen_failed)
        }   // for(;;)
      } // if (listen_endpoint.port() != 0)
      else // port is 0
      {
        // if they requested a random port, we'll just assume it's available
        // (it may not be due to ip address, but we'll detect that in the next step)
      }

      _tcp_server.set_reuse_address();
      int server_receive_buffer_size = _tcp_server.set_receive_buffer_size(MAX_MESSAGE_SIZE);
      idump((server_receive_buffer_size));
      int server_send_buffer_size = _tcp_server.set_send_buffer_size(MAX_MESSAGE_SIZE);
      idump((server_send_buffer_size));
      idump((_tcp_server.get_no_delay()));
      _tcp_server.set_no_delay(true);
      idump((_tcp_server.get_no_delay()));

      try
      {
        if (listen_endpoint.get_address() != fc::ip::address())
          _tcp_server.listen(listen_endpoint);
        else
          _tcp_server.listen(listen_endpoint.port());
        _actual_listening_endpoint = _tcp_server.get_local_endpoint();
        ilog("listening for connections on endpoint ${_actual_listening_endpoint} (our first choice)", (_actual_listening_endpoint));
        remember_own_listen_endpoints();
      }
      catch (fc::exception& e)
      {
        FC_RETHROW_EXCEPTION(e, error, "unable to listen on ${listen_endpoint}", (listen_endpoint));
      }
    }

    void node_impl::connect_to_p2p_network()
    {
      VERIFY_CORRECT_THREAD();
      assert(_node_public_key != fc::ecc::public_key_data());

      assert(!_accept_loop_complete.valid() &&
             !_p2p_network_connect_loop_done.valid() &&
             !_fetch_sync_items_loop_done.valid() &&
             !_fetch_item_loop_done.valid() &&
             !_advertise_inventory_loop_done.valid() &&
             !_terminate_inactive_connections_loop_done.valid() &&
             !_fetch_updated_peer_lists_loop_done.valid() &&
             !_bandwidth_monitor_loop_done.valid() &&
             !_dump_node_status_task_done.valid() &&
             !_statistics_report_loop_done.valid());
      _host_limit_hard_until = fc::time_point::now() + fc::seconds(_node_configuration.host_limit_relax_delay);
      if (_node_configuration.accept_incoming_connections)
        _accept_loop_complete = async_task( [=, this](){ accept_loop(); }, "accept_loop");
      _p2p_network_connect_loop_done = async_task( [=, this]() { p2p_network_connect_loop(); }, "p2p_network_connect_loop" );
      _fetch_sync_items_loop_done = async_task( [=, this]() { fetch_sync_items_loop(); }, "fetch_sync_items_loop" );
      _fetch_item_loop_done = async_task( [=, this]() { fetch_items_loop(); }, "fetch_items_loop" );
      _advertise_inventory_loop_done = async_task( [=, this]() { advertise_inventory_loop(); }, "advertise_inventory_loop" );
      _terminate_inactive_connections_loop_done = async_task( [=, this]() { terminate_inactive_connections_loop(); }, "terminate_inactive_connections_loop" );
      _fetch_updated_peer_lists_loop_done = async_task([=, this](){ fetch_updated_peer_lists_loop(); }, "fetch_updated_peer_lists_loop");
      _bandwidth_monitor_loop_done = async_task([=, this](){ bandwidth_monitor_loop(); }, "bandwidth_monitor_loop");
      _dump_node_status_task_done = async_task([=, this](){ dump_node_status_task(); }, "dump_node_status_task");
      _statistics_report_loop_done = schedule_task([this](){ statistics_report_loop(); },
                                                   fc::time_point::now() + fc::seconds(GRAPHENE_NET_STATISTICS_REPORT_INTERVAL_SEC),
                                                   "statistics_report_loop");
    }

    void node_impl::add_node(const fc::ip::endpoint& ep)
    {
      VERIFY_CORRECT_THREAD();
      // if we're connecting to them, we believe they're not firewalled
      potential_peer_record updated_peer_record = _potential_peer_db.lookup_or_create_entry_for_endpoint(ep);

      // if we've recently connected to this peer, reset the last_connection_attempt_time to allow
      // us to immediately retry this peer
      updated_peer_record.last_connection_attempt_time = std::min<fc::time_point_sec>(updated_peer_record.last_connection_attempt_time,
                                                                                      fc::time_point::now() - fc::seconds(_node_configuration.peer_connection_retry_timeout));
      _add_once_node_list.push_back(updated_peer_record);
      _host_limit_exempt_endpoints.insert(ep);
      _potential_peer_db.update_entry(updated_peer_record);
      trigger_p2p_network_connect_loop();
    }

    void node_impl::initiate_connect_to(const peer_connection_ptr& new_peer)
    {
      if (new_peer->get_remote_endpoint() && new_peer->get_remote_endpoint()->get_address().is_ipv6())
        new_peer->get_socket().open_for_endpoint(*new_peer->get_remote_endpoint());
      else
        new_peer->get_socket().open();
      new_peer->get_socket().set_reuse_address();
      int peer_send_buffer_size = new_peer->get_socket().set_send_buffer_size(MAX_MESSAGE_SIZE);
      idump((peer_send_buffer_size));
      int peer_receive_buffer_size = new_peer->get_socket().set_receive_buffer_size(MAX_MESSAGE_SIZE);
      idump((peer_receive_buffer_size));
      idump((new_peer->get_socket().get_no_delay()));
      new_peer->get_socket().set_no_delay(true);
      idump((new_peer->get_socket().get_no_delay()));
  
      new_peer->connection_initiation_time = fc::time_point::now();
      _handshaking_connections.insert(new_peer);
      _rate_limiter.add_tcp_socket(&new_peer->get_socket());

      if (node_is_shutting_down())
        return;

      std::weak_ptr<peer_connection> new_weak_peer(new_peer);
      new_peer->accept_or_connect_task_done = async_task([this, new_weak_peer]()
      {
        peer_connection_ptr new_peer(new_weak_peer.lock());
        assert(new_peer);
        if (!new_peer)
          return;
        connect_to_task(new_peer, *new_peer->get_remote_endpoint());
      }, "connect_to_task");
    }

    void node_impl::connect_to_endpoint(const fc::ip::endpoint& remote_endpoint)
    {
      VERIFY_CORRECT_THREAD();
      if (is_connection_to_endpoint_in_progress(remote_endpoint))
        FC_THROW_EXCEPTION(already_connected_to_requested_peer, "already connected to requested endpoint ${endpoint}",
                           ("endpoint", remote_endpoint));

      if (_potential_peer_db.is_own_endpoint(remote_endpoint))
      {
        dlog("not connecting to ${endpoint}, it is an address of this node", ("endpoint", remote_endpoint));
        return;
      }

      dlog("node_impl::connect_to_endpoint(${endpoint})", ("endpoint", remote_endpoint));
      peer_connection_ptr new_peer(create_peer_connection());
      new_peer->set_remote_endpoint(remote_endpoint);
      initiate_connect_to(new_peer);
    }

    peer_connection_ptr node_impl::get_connection_to_endpoint( const fc::ip::endpoint& remote_endpoint )
    {
      VERIFY_CORRECT_THREAD();
      return find_unclosed_connection_to_endpoint( remote_endpoint, _active_connections, _handshaking_connections,
                                                   _closing_connections, _terminating_connections );
    }

    bool node_impl::is_connection_to_endpoint_in_progress( const fc::ip::endpoint& remote_endpoint )
    {
      VERIFY_CORRECT_THREAD();
      return get_connection_to_endpoint( remote_endpoint ) != peer_connection_ptr();
    }

    void node_impl::move_peer_to_active_list(const peer_connection_ptr& peer)
    {
      VERIFY_CORRECT_THREAD();
      _active_connections.insert(peer);
      peer->last_useful_data_time = fc::time_point::now();
      peer->last_item_exchange_time = peer->last_useful_data_time;
      if (!peer->transaction_inventory_slot)
        peer->transaction_inventory_slot = _transaction_inventory.acquire_slot();
      _handshaking_connections.erase(peer);
      _closing_connections.erase(peer);
      _terminating_connections.erase(peer);
      const connection_spread spread = get_active_connection_spread();
      fc_ilog(fc::logger::get("sync"), "New peer is connected (${peer}), now ${count} active peers on ${hosts} hosts (${ipv4} IPv4, ${ipv6} IPv6)",
              ("peer", peer->get_remote_endpoint())
              ("count", _active_connections.size())
              ("hosts", spread.hosts)("ipv4", spread.ipv4)("ipv6", spread.ipv6));
      give_way_to_connection_under_host_limit(peer.get());
    }

    void node_impl::move_peer_to_closing_list(const peer_connection_ptr& peer)
    {
      VERIFY_CORRECT_THREAD();
      _active_connections.erase(peer);
      release_transaction_inventory_slot(peer.get());
      _handshaking_connections.erase(peer);
      _closing_connections.insert(peer);
      _terminating_connections.erase(peer);
      const connection_spread spread = get_active_connection_spread();
      fc_ilog(fc::logger::get("sync"), "Peer connection closing (${peer}), now ${count} active peers on ${hosts} hosts (${ipv4} IPv4, ${ipv6} IPv6)",
              ("peer", peer->get_remote_endpoint())
              ("count", _active_connections.size())
              ("hosts", spread.hosts)("ipv4", spread.ipv4)("ipv6", spread.ipv6));
    }

    void node_impl::move_peer_to_terminating_list(const peer_connection_ptr& peer)
    {
      VERIFY_CORRECT_THREAD();
      _active_connections.erase(peer);
      release_transaction_inventory_slot(peer.get());
      _handshaking_connections.erase(peer);
      _closing_connections.erase(peer);
      _terminating_connections.insert(peer);
      const connection_spread spread = get_active_connection_spread();
      fc_ilog(fc::logger::get("sync"), "Peer connection terminating (${peer}), now ${count} active peers on ${hosts} hosts (${ipv4} IPv4, ${ipv6} IPv6)",
              ("peer", peer->get_remote_endpoint())
              ("count", _active_connections.size())
              ("hosts", spread.hosts)("ipv4", spread.ipv4)("ipv6", spread.ipv6));
    }


    // format a byte count like 409869 into "409.8 KiB"
    const unsigned formatted_network_bytes_length = 9; // "123.4 KiB" is 9 chars
    std::string format_network_bytes(uint64_t bytes)
    {
      std::ostringstream bytes_stream;
      bytes_stream << std::right << std::setw(5) << std::fixed << std::setprecision(1);
      if (bytes > 1099511627776)
        bytes_stream << (bytes / 1099511627776.0) << " TiB";
      else if (bytes > 1073741824)
        bytes_stream << (bytes / 1073741824.0) << " GiB";
      else if (bytes > 1048576)
        bytes_stream << (bytes / 1048576.0) << " MiB";
      else if (bytes > 1024)
        bytes_stream << (bytes / 1024.0) << " KiB";
      else
        bytes_stream << bytes << "   B";
      return bytes_stream.str();
    }

    // format a speed like 409869 into "409.8 KiB/s"
    const unsigned formatted_network_speed_length = 11; // "123.4 KiB/s" is 11 chars
    std::string format_network_speed(float speed_bps)
    {
      std::ostringstream speed_stream;
      speed_stream << std::right << std::setw(5) << std::fixed << std::setprecision(1);
      if (speed_bps > 1073741824) // yeah, right
        speed_stream << (speed_bps / 1073741824.0) << " GiB/s";
      else if (speed_bps > 1048576)
        speed_stream << (speed_bps / 1048576.0) << " MiB/s";
      else if (speed_bps > 1024)
        speed_stream << (speed_bps / 1024.0) << " KiB/s";
      else
        speed_stream << speed_bps << "   B/s";
      return speed_stream.str();
    }

    // draw a crude ascii bar chart
    std::vector<std::string> graph_values(const boost::circular_buffer<uint32_t>& read_speeds, 
                                          const boost::circular_buffer<uint32_t>& write_speeds, 
                                          unsigned width, const std::string& title,
                                          const std::string& x_axis_min_label, const std::string& x_axis_max_label)
    {
      const uint32_t height = 20;

      // combine read + write
      std::vector<uint32_t> combined_speeds;
      combined_speeds.reserve(read_speeds.size());
      std::transform(read_speeds.begin(), read_speeds.end(),
                     write_speeds.begin(),
                     std::back_inserter(combined_speeds),
                     std::plus<uint32_t>());

      uint32_t min_value = 0;
      uint32_t max_value = 0;
      if (!combined_speeds.empty())
      {
        auto minmax = std::minmax_element(combined_speeds.begin(), combined_speeds.end());
        min_value = *minmax.first;
        max_value = *minmax.second;
      }

      uint32_t y_min = min_value;
      uint32_t y_max = max_value;
      if (min_value == max_value)
        y_max = min_value ? 2 * min_value : height;

      float y_step = ((float)y_max - (float)y_min) / ((float)height - 1);
      std::vector<std::string> graph;

      // spit out the title
      std::ostringstream title_stream;
      title_stream << std::string(formatted_network_speed_length, ' ') << "  ";
      unsigned left_padding = 0;
      if (title.length() < width)
        left_padding = (width - title.length()) / 2;
      title_stream << std::string(left_padding, ' ');
      title_stream << title;
      if (title.length() + left_padding < width)
        title_stream << std::string(width - title.length() - left_padding, ' ');
      graph.push_back(title_stream.str());

      // then the body of the graph
      for (unsigned row_number = 0; row_number < height; ++row_number)
      {
        std::ostringstream row_stream;
        float threshold = (float)y_min + (y_step * (height - row_number - 1));
        row_stream << format_network_speed(threshold) << " |";
        row_stream << std::string(width - combined_speeds.size(), ' '); // pad to full width
        for (unsigned i = 0; i < combined_speeds.size(); ++i)
          row_stream << (read_speeds[i] >= threshold ? '#' : (combined_speeds[i] >= threshold ? '*' : ' '));
        graph.push_back(row_stream.str());
      }

      // and now the x axis
      std::ostringstream x_axis;
      x_axis << std::string(formatted_network_speed_length, ' ') << " +" << std::string(width, '-');
      graph.push_back(x_axis.str());
      // and its labels
      std::ostringstream x_axis_labels;
      x_axis_labels << std::string(formatted_network_speed_length, ' ') << "  " << x_axis_min_label;
      if (x_axis_min_label.length() + x_axis_max_label.length() < width)
        x_axis_labels << std::string(width - x_axis_min_label.length() - x_axis_max_label.length(), ' ');
      x_axis_labels << x_axis_max_label;
      graph.push_back(x_axis_labels.str());

      return graph;
    }

    void node_impl::dump_node_status()
    {
      VERIFY_CORRECT_THREAD();
      ilog( "----------------- PEER STATUS UPDATE --------------------" );
      ilog( " number of peers: ${active} active, ${handshaking}, ${closing} closing.  attempting to maintain ${desired} - ${maximum} peers",
           ( "active", _active_connections.size() )("handshaking", _handshaking_connections.size() )("closing",_closing_connections.size() )
           ( "desired", _node_configuration.desired_number_of_connections )("maximum", _node_configuration.maximum_number_of_connections ) );
      for( const peer_connection_ptr& peer : _active_connections )
      {
        ilog( "       active peer ${endpoint} peer_is_in_sync_with_us:${in_sync_with_us} we_are_in_sync_with_peer:${in_sync_with_them}",
             ( "endpoint", peer->get_remote_endpoint() )
             ( "in_sync_with_us", !peer->peer_needs_sync_items_from_us )("in_sync_with_them", !peer->we_need_sync_items_from_peer ) );
        if( peer->we_need_sync_items_from_peer )
          ilog( "              above peer has ${count} sync items we might need", ("count", peer->ids_of_items_to_get.size() ) );
        if (peer->inhibit_fetching_sync_blocks)
          ilog( "              we are not fetching sync blocks from the above peer (inhibit_fetching_sync_blocks == true)" );

      }
      for( const peer_connection_ptr& peer : _handshaking_connections )
      {
        ilog( "  handshaking peer ${endpoint} in state ours(${our_state}) theirs(${their_state})",
             ( "endpoint", peer->get_remote_endpoint() )("our_state", peer->our_state )("their_state", peer->their_state ) );
      }

      ilog( "--------- MEMORY USAGE ------------" );
      ilog( "node._active_sync_requests size: ${size}", ("size", _active_sync_requests.size() ) );
      ilog( "node._received_sync_items size: ${size}", ("size", _received_sync_items.size() ) );
      ilog( "node._new_received_sync_items size: ${size}", ("size", _new_received_sync_items.size() ) );
      ilog( "node._items_to_fetch size: ${size}", ("size", _items_to_fetch.size() ) );
      ilog( "node._new_inventory size: ${block_size} blocks, ${transaction_size} transactions",
            ("block_size", _new_block_inventory.size() )("transaction_size", _new_transaction_inventory.size() ) );
      ilog( "node._block_message_cache size: ${size}", ("size", _block_message_cache.size() ) );
      ilog( "node._transaction_message_cache size: ${size}", ("size", _transaction_message_cache.size() ) );
      ilog( "node._transaction_inventory size: ${size}", ("size", _transaction_inventory.size() ) );
      for( const peer_connection_ptr& peer : _active_connections )
      {
        ilog( "  peer ${endpoint}", ("endpoint", peer->get_remote_endpoint() ) );
        ilog( "    last requested block number for peers on this fork: ${block_num}",("block_num",peer->last_requested_block_number_for_peers_on_this_fork) );
        ilog( "    block number of first id to get: ${block_num}", ("block_num",peer->first_id_block_number) );
        ilog( "    peer.ids_of_items_to_get size: ${size}", ("size", peer->ids_of_items_to_get.size() ) );
        const transaction_inventory::slot_type slot = peer->transaction_inventory_slot.value_or(std::numeric_limits<transaction_inventory::slot_type>::max());
        ilog( "    peer inventory advertised to us: ${blocks} blocks, ${transactions} transactions",
              ("blocks", peer->block_inventory_peer_advertised_to_us.size())("transactions", _transaction_inventory.advertised_to_us_count(slot)) );
        ilog( "    peer inventory advertised to peer: ${blocks} blocks, ${transactions} transactions",
              ("blocks", peer->block_inventory_advertised_to_peer.size())("transactions", _transaction_inventory.advertised_to_peer_count(slot)) );
        if (!peer->item_ids_requested_from_peer)
          ilog( "    peer.time_since_inventory_requested: no request in progress" );
        else
          ilog( "    peer.time_since_inventory_requested: ${time}ms", ("time", (fc::time_point::now() - peer->item_ids_requested_from_peer->get<1>()).count() / 1000));
        ilog( "    peer.items_requested_from_peer size: ${size}", ("size", peer->items_requested_from_peer.size() ) );
        ilog( "    peer.sync_items_requested_from_peer size: ${size}", ("size", peer->sync_items_requested_from_peer.size() ) );
        ilog( "    peer.time_since_last_sync_item_received: ${time_since_last_sync_item_received}ms", ("time_since_last_sync_item_received", (fc::time_point::now() - peer->last_sync_item_received_time).count() / 1000));
        ilog( "    peer.blocks_received_from_peer: ${compressed} compressed, ${uncompressed} uncompressed",
              ("compressed", peer->compressed_blocks_received_from_peer)("uncompressed", peer->uncompressed_blocks_received_from_peer));
      }
      ilog( "--------- END MEMORY USAGE ------------" );

      ilog( "--------- NETWORK USAGE ---------------" );

      // first, log the stats per peer, most talkative first
      try
      {
        fc::time_point current_time = fc::time_point::now();
        struct peer_network_stats {
          fc::optional<fc::ip::endpoint> endpoint;
          uint64_t total_bytes_sent;
          uint64_t total_bytes_received;
          fc::microseconds connection_duration;
          float bytes_per_second() const { return connection_duration != fc::microseconds() ? ((float)(total_bytes_sent + total_bytes_received) / ((float)connection_duration.count() / fc::seconds(1).count())) : 0.f; }
        };
        std::vector<peer_network_stats> network_stats_by_peer;
        for (const peer_connection_ptr& peer : _active_connections)
          network_stats_by_peer.push_back({peer->get_remote_endpoint(), peer->get_total_bytes_sent(), peer->get_total_bytes_received(), current_time - peer->get_connection_time()});
        std::sort(network_stats_by_peer.begin(), network_stats_by_peer.end(), [](const peer_network_stats& lhs, const peer_network_stats& rhs) { return lhs.bytes_per_second() > rhs.bytes_per_second(); });
        ilog("Per peer (stats over the entire duration of their connection):");
        ilog("peer                 |      speed |        sent |    received | connection time");
        ilog("---------------------+------------+-------------+-------------+----------------");
        for (const peer_network_stats& stat : network_stats_by_peer)
        {
          std::string speed = format_network_speed(stat.bytes_per_second());
          int speed_padding = std::max<int>(formatted_network_speed_length - speed.length(), 0);
          std::string bytes_sent = format_network_bytes(stat.total_bytes_sent);
          int bytes_sent_padding = std::max<int>(3 + formatted_network_bytes_length - bytes_sent.length(), 0);
          std::string bytes_received = format_network_bytes(stat.total_bytes_received);
          int bytes_received_padding = std::max<int>(3 + formatted_network_bytes_length - bytes_received.length(), 0);
          std::string endpoint_string = fc::json::to_string(stat.endpoint);
          int endpoint_padding = std::max<int>(23 - endpoint_string.length(), 0);
          ilog("${endpoint}${endpoint_padding}|${speed_padding}${speed} |${bytes_sent_padding}${bytes_sent} |${bytes_received_padding}${bytes_received} | ${ago}",
               ("endpoint", stat.endpoint)("endpoint_padding", std::string(endpoint_padding, ' '))
               (speed)("speed_padding", std::string(speed_padding, ' '))
               (bytes_sent)("bytes_sent_padding", std::string(bytes_sent_padding, ' '))
               (bytes_received)("bytes_received_padding", std::string(bytes_received_padding, ' '))
               ("ago", get_approximate_relative_time_string(current_time - stat.connection_duration, current_time, "")));
        }
        ilog("                                                                               ");
      }
      catch (const fc::exception& e)
      {
        edump((e));
      }
      catch (...)
      {
      }

      if (!_average_network_read_speed_seconds.empty())
      {
        std::vector<std::string> graph = graph_values(_average_network_read_speed_seconds, _average_network_write_speed_seconds, 
                                                      60, "Network Speed by Second",
                                                      "a minute ago", "now");
        for (const std::string& line : graph)
          ilog("${line}", (line));
      }

      if (!_average_network_read_speed_minutes.empty())
      {
        ilog("${space}", ("space", std::string(formatted_network_speed_length + 2 + 60, ' ')));
        std::vector<std::string> graph = graph_values(_average_network_read_speed_minutes, _average_network_write_speed_minutes, 
                                                      60, "Average Network Speed by Minute",
                                                      "an hour ago", "now");
        for (const std::string& line : graph)
          ilog("${line}", (line));
      }

      if (!_average_network_read_speed_hours.empty())
      {
        ilog("${space}", ("space", std::string(formatted_network_speed_length + 2 + 60, ' ')));
        std::vector<std::string> graph = graph_values(_average_network_read_speed_hours, _average_network_write_speed_hours,
                                                      72, "Average Network Speed by Hour",
                                                      "three days ago", "now");
        for (const std::string& line : graph)
          ilog("${line}", (line));
      }

      ilog("${space}", ("space", std::string(formatted_network_speed_length + 2 + 60, ' ')));
      ilog("# = download, * = upload");

      ilog( "--------- END NETWORK USAGE -----------" );
    }

    void node_impl::disconnect_from_peer(peer_connection* peer_to_disconnect,
                                         const std::string& reason_for_disconnect,
                                         bool caused_by_error /* = false */,
                                         const fc::oexception& error /* = fc::oexception() */)
    {
      VERIFY_CORRECT_THREAD();
      move_peer_to_closing_list(peer_to_disconnect->shared_from_this());

      const fc::exception disconnect_reason = error ? *error : fc::exception(FC_LOG_MESSAGE(info, reason_for_disconnect.c_str()));
      peer_to_disconnect->record_connection_closed_error(disconnect_reason, caused_by_error);

      if (peer_to_disconnect->they_have_requested_close)
      {
        // the peer has already told us that it's ready to close the connection, so just close the connection
        peer_to_disconnect->close_connection();
      }
      else
      {
        // we're the first to try to want to close the connection
        fc::optional<fc::ip::endpoint> endpoint_for_db = peer_to_disconnect->get_endpoint_for_db();
        if (endpoint_for_db)
        {
          fc::optional<potential_peer_record> updated_peer_record = _potential_peer_db.lookup_entry_for_endpoint(*endpoint_for_db);
          if (updated_peer_record)
          {
            updated_peer_record->last_seen_time = fc::time_point::now();
            updated_peer_record->last_error = disconnect_reason;
            if (caused_by_error)
              updated_peer_record->last_failed_time = fc::time_point::now();
            _potential_peer_db.update_entry(*updated_peer_record);
          }
        }
        peer_to_disconnect->we_have_requested_close = true;
        peer_to_disconnect->connection_closed_time = fc::time_point::now();

        closing_connection_message closing_message( reason_for_disconnect, caused_by_error, error );
        peer_to_disconnect->send_message( closing_message );
      }

      // notify the user.  This will be useful in testing, but we might want to remove it later;
      // it makes good sense to notify the user if other nodes think she is behaving badly, but
      // if we're just detecting and dissconnecting other badly-behaving nodes, they don't really care.
      if (caused_by_error)
      {
        std::ostringstream error_message;
        error_message << "I am disconnecting peer " << fc::variant( peer_to_disconnect->get_remote_endpoint() ).as_string() <<
                         " for reason: " << reason_for_disconnect;
        _delegate->error_encountered(error_message.str(), fc::oexception());
        dlog(error_message.str());
      }
      else
        dlog("Disconnecting from ${peer} for ${reason}", ("peer",peer_to_disconnect->get_remote_endpoint()) ("reason",reason_for_disconnect));
      // peer_to_disconnect->close_connection();
    }

    void node_impl::disconnect_from_peer_needing_pruned_blocks(peer_connection* peer, uint32_t peers_head_block_num,
                                                               uint32_t our_tail_block_num)
    {
      VERIFY_CORRECT_THREAD();
      ilog("Disconnecting peer ${peer} at block #${peer_block}: it needs blocks older than #${tail}, which have been pruned on this node",
           ("peer", peer->get_remote_endpoint())("peer_block", peers_head_block_num)("tail", our_tail_block_num));
      std::ostringstream reason;
      reason << "I can't help you sync: blocks older than #" << our_tail_block_num << " have been pruned on this node";
      fc::exception detailed_error(FC_LOG_MESSAGE(warn, "You need blocks older than #${tail}, which have been pruned on this node "
                                                  "(your newest block is #${peer_block})",
                                                  ("tail", our_tail_block_num)("peer_block", peers_head_block_num)));
      // reported as caused by an error: that is what makes the peer, whatever its version, record the
      // failure and try its other peers before coming back to us
      disconnect_from_peer(peer, reason.str(), true, detailed_error);
    }

    void node_impl::listen_on_endpoint( const fc::ip::endpoint& ep, bool wait_if_not_available )
    {
      VERIFY_CORRECT_THREAD();
      _node_configuration.listen_endpoint = ep;
      _node_configuration.wait_if_endpoint_is_busy = wait_if_not_available;
      save_node_configuration();
    }

    void node_impl::accept_incoming_connections(bool accept)
    {
      VERIFY_CORRECT_THREAD();
      _node_configuration.accept_incoming_connections = accept;
      save_node_configuration();
    }

    void node_impl::listen_on_port( uint16_t port, bool wait_if_not_available )
    {
      VERIFY_CORRECT_THREAD();
      _node_configuration.listen_endpoint = fc::ip::endpoint( fc::ip::address(), port );
      _node_configuration.wait_if_endpoint_is_busy = wait_if_not_available;
      save_node_configuration();
    }

    fc::ip::endpoint node_impl::get_actual_listening_endpoint() const
    {
      VERIFY_CORRECT_THREAD();
      return _actual_listening_endpoint;
    }

    std::vector<peer_status> node_impl::get_connected_peers() const
    {
      VERIFY_CORRECT_THREAD();
      std::vector<peer_status> statuses;
      for (const peer_connection_ptr& peer : _active_connections)
      {
        ASSERT_TASK_NOT_PREEMPTED(); // don't yield while iterating over _active_connections

        peer_status this_peer_status;
        this_peer_status.version = 0;
        fc::optional<fc::ip::endpoint> endpoint = peer->get_remote_endpoint();
        if (endpoint)
          this_peer_status.host = *endpoint;
        fc::mutable_variant_object peer_details;
        peer_details["addr"] = endpoint ? (std::string)*endpoint : std::string();
        fc::optional<fc::ip::endpoint> local_endpoint = peer->get_local_endpoint();
        peer_details["addrlocal"] = local_endpoint ? (std::string)*local_endpoint : std::string();
        peer_details["services"] = "00000001";
        peer_details["lastsend"] = peer->get_last_message_sent_time().sec_since_epoch();
        peer_details["lastrecv"] = peer->get_last_message_received_time().sec_since_epoch();
        peer_details["bytessent"] = peer->get_total_bytes_sent();
        peer_details["bytesrecv"] = peer->get_total_bytes_received();
        peer_details["conntime"] = peer->get_connection_time();
        peer_details["pingtime"] = "";
        peer_details["pingwait"] = "";
        peer_details["version"] = "";
        peer_details["subver"] = peer->user_agent;
        peer_details["inbound"] = peer->direction == peer_connection_direction::inbound;
        peer_details["firewall_status"] = peer->is_firewalled;
        peer_details["startingheight"] = "";
        peer_details["banscore"] = "";
        peer_details["syncnode"] = "";

        if (peer->fc_git_revision_sha)
        {
          std::string revision_string = *peer->fc_git_revision_sha;
          if (*peer->fc_git_revision_sha == fc::git_revision_sha)
            revision_string += " (same as ours)";
          else
            revision_string += " (different from ours)";
          peer_details["fc_git_revision_sha"] = revision_string;

        }
        if (peer->fc_git_revision_unix_timestamp)
        {
          peer_details["fc_git_revision_unix_timestamp"] = *peer->fc_git_revision_unix_timestamp;
          std::string age_string = fc::get_approximate_relative_time_string( *peer->fc_git_revision_unix_timestamp);
          if (*peer->fc_git_revision_unix_timestamp == fc::time_point_sec(fc::git_revision_unix_timestamp))
            age_string += " (same as ours)";
          else if (*peer->fc_git_revision_unix_timestamp > fc::time_point_sec(fc::git_revision_unix_timestamp))
            age_string += " (newer than ours)";
          else
            age_string += " (older than ours)";
          peer_details["fc_git_revision_age"] = age_string;
        }

        if (peer->platform)
          peer_details["platform"] = *peer->platform;

        peer_details["user_agent"] = peer->user_agent;
        peer_details["core_protocol_version"] = peer->core_protocol_version;

        // provide these for debugging
        // warning: these are just approximations, if the peer is "downstream" of us, they may
        // have received blocks from other peers that we are unaware of
        peer_details["current_head_block"] = peer->last_block_delegate_has_seen;
        peer_details["current_head_block_number"] = _delegate->get_block_number(peer->last_block_delegate_has_seen);
        peer_details["current_head_block_time"] = peer->last_block_time_delegate_has_seen;
        peer_details["last_useful_data_time"] = peer->last_useful_data_time;
        peer_details["last_item_exchange_time"] = peer->last_item_exchange_time;
        peer_details["withholding_strikes"] = peer->unavailable_item_strikes.count(fc::time_point::now());
        peer_details["block_fetching_deprioritized"] = peer->is_block_fetching_deprioritized();
        peer_details["transaction_replies_backlogged"] = peer->transaction_replies_backlogged;
        peer_details["transaction_fetching_demoted"] = peer->transaction_fetching_inhibited_until > fc::time_point::now();

        this_peer_status.info = peer_details;
        statuses.push_back(this_peer_status);
      }
      return statuses;
    }

    uint32_t node_impl::get_connection_count() const
    {
      VERIFY_CORRECT_THREAD();
      return (uint32_t)_active_connections.size();
    }

    // queue a block for advertisement to our in-sync peers via the inventory loop.
    // Unlike broadcast(), this does not touch the message cache or the recently-accepted
    // list, so it is safe to call for a block that was already accepted earlier (a duplicate
    // delivery must not add duplicate entries to those bounded structures, and must not
    // advance the message cache's block clock -- doing so shrinks both duplicate-detection
    // windows exactly when duplicates are most common).
    void node_impl::advertise_block(const std::shared_ptr<full_block_type>& full_block)
    {
      VERIFY_CORRECT_THREAD();
      // Never advertise a block older than our peers' duplicate-detection horizon (their message
      // cache holds the last GRAPHENE_NET_MESSAGE_CACHE_DURATION_IN_BLOCKS blocks).  A peer can't
      // recognize an older advertisement as something it already has, so it would fetch and
      // re-push the block: wasted bandwidth and a chain-thread round trip at best, and
      // historically the seed of the 2026-07-09 disconnect storm.  Only fresh blocks -- the only
      // ones whose advertisement helps anyone -- pass.  Peers that genuinely lack older blocks
      // obtain them through the sync path instead.
      fc::time_point oldest_block_time_to_advertise =
        fc::time_point::now() - fc::seconds(uint32_t(_recent_block_interval_in_seconds) * GRAPHENE_NET_MESSAGE_CACHE_DURATION_IN_BLOCKS);
      if (fc::time_point(full_block->get_block_header().timestamp) < oldest_block_time_to_advertise)
      {
        dlog("not advertising block ${num} because it is older than the network's duplicate-detection window",
             ("num", full_block->get_block_num()));
        return;
      }
      _new_block_inventory.insert(full_block);
      trigger_advertise_inventory_loop();
    }

    void node_impl::broadcast(const std::shared_ptr<full_block_type>& full_block, const message_propagation_data& propagation_data)
    {
      VERIFY_CORRECT_THREAD();
      const block_id_type& block_id = full_block->get_block_id();
      idump((block_id));

      _most_recent_blocks_accepted.push_back(block_id);
      _block_message_cache.cache_message(full_block, propagation_data);
      refresh_cached_chain_state();
      advertise_block(full_block);

      /** Trigger block_accepted methods to potentially flush caches in both cases:
          - when node receives a block,
          - just produce (broadcast) block (as single block producer node in the network)
      */
      _block_message_cache.block_accepted();
      _transaction_message_cache.block_accepted();
    }

    void node_impl::broadcast(const std::shared_ptr<full_transaction_type>& full_transaction, const message_propagation_data& propagation_data)
    {
      VERIFY_CORRECT_THREAD();
      if (is_relay_sample(full_transaction->get_legacy_transaction_message_hash()))
        fc_ilog(fc::logger::get(GRAPHENE_NET_RELAY_SAMPLE_LOGGER), "relay sample broadcast ${trx} ${hash} at ${time_us}",
                ("trx", full_transaction->get_transaction_id())("hash", full_transaction->get_legacy_transaction_message_hash())
                ("time_us", raw_realtime_microseconds()));
      _transaction_message_cache.cache_message(full_transaction, propagation_data);
      _new_transaction_inventory.insert(full_transaction);
      trigger_advertise_inventory_loop();
    }

    void node_impl::broadcast(const std::shared_ptr<full_block_type>& full_block)
    {
      VERIFY_CORRECT_THREAD();
      // this version is called directly from the client
      message_propagation_data propagation_data{fc::time_point::now(), fc::time_point::now(), _node_id};
      broadcast(full_block, propagation_data);
    }

    void node_impl::broadcast(const std::shared_ptr<full_transaction_type>& full_transaction)
    {
      VERIFY_CORRECT_THREAD();
      // this version is called directly from the client
      message_propagation_data propagation_data{fc::time_point::now(), fc::time_point::now(), _node_id};
      broadcast(full_transaction, propagation_data);
    }

    void node_impl::sync_from(const item_id& current_head_block, const std::vector<uint32_t>& hard_fork_block_numbers)
    {
      VERIFY_CORRECT_THREAD();
      _most_recent_blocks_accepted.clear();
      _sync_item_type = current_head_block.item_type;
      _most_recent_blocks_accepted.push_back(current_head_block.item_hash);
      refresh_cached_chain_state();
      _hard_fork_block_numbers = hard_fork_block_numbers;
    }

    bool node_impl::is_connected() const
    {
      VERIFY_CORRECT_THREAD();
      return !_active_connections.empty();
    }

    std::vector<potential_peer_record> node_impl::get_potential_peers() const
    {
      VERIFY_CORRECT_THREAD();
      std::vector<potential_peer_record> result;
      // use explicit iterators here, for some reason the mac compiler can't used ranged-based for loops here
      for (peer_database::iterator itr = _potential_peer_db.begin(); itr != _potential_peer_db.end(); ++itr)
        result.push_back(*itr);
      return result;
    }

    void node_impl::set_advanced_node_parameters(const fc::variant_object& params)
    {
      VERIFY_CORRECT_THREAD();
      ilog( "set_advanced_node_parameters ${params}", ("params", params) );

      fc::from_variant( params, _node_configuration );
      // fc::from_variant assigns only the fields params has: a limit set without saying whether it relaxes is hard
      if( params.contains( "maximum_connections_per_host" ) && !params.contains( "relax_per_host_limit" ) )
        _node_configuration.relax_per_host_limit = false;

      std::vector<address_subnet> exempt_subnets;
      for (const std::string& subnet : _node_configuration.host_limit_exempt_subnets)
        exempt_subnets.push_back(address_subnet::from_string(subnet));
      _host_limit_exempt_subnets = std::move(exempt_subnets);

      if( _node_configuration.private_key == fc::ecc::private_key() )
      {
         ilog( "generating new private key for this node" );
         _node_configuration.private_key = fc::ecc::private_key::generate();
      }

      // Private key could have been overridden at this point. Update public key just in case
      _node_public_key = _node_configuration.private_key.get_public_key().serialize();

      if( _node_configuration.desired_number_of_connections > _node_configuration.maximum_number_of_connections )
      {
         wlog( "Reducing desired_number_of_connections from ${x0} to maximum_number_of_connections=${x1}",
            ("x0", _node_configuration.desired_number_of_connections)
            ("x1", _node_configuration.maximum_number_of_connections)
            );
      }

      while (_active_connections.size() > _node_configuration.maximum_number_of_connections)
        disconnect_from_peer(_active_connections.begin()->get(), "I have too many connections open");
      trigger_p2p_network_connect_loop();
    }

    node_configuration node_impl::get_advanced_node_parameters()const
    {
      VERIFY_CORRECT_THREAD();
      return _node_configuration;
    }

    message_propagation_data node_impl::get_transaction_propagation_data(const graphene::net::transaction_id_type& transaction_id)
    {
      VERIFY_CORRECT_THREAD();
      return _transaction_message_cache.get_message_propagation_data(transaction_id);
    }

    message_propagation_data node_impl::get_block_propagation_data(const graphene::net::block_id_type& block_id)
    {
      VERIFY_CORRECT_THREAD();
      return _block_message_cache.get_message_propagation_data(block_id);
    }

    node_id_t node_impl::get_node_id() const
    {
      VERIFY_CORRECT_THREAD();
      return _node_id;
    }
    void node_impl::set_allowed_peers(const std::vector<node_id_t>& allowed_peers)
    {
      VERIFY_CORRECT_THREAD();
#ifdef ENABLE_P2P_DEBUGGING_API
      _allowed_peers.clear();
      _allowed_peers.insert(allowed_peers.begin(), allowed_peers.end());
      std::list<peer_connection_ptr> peers_to_disconnect;
      if (!_allowed_peers.empty())
        for (const peer_connection_ptr& peer : _active_connections)
          if (_allowed_peers.find(peer->node_id) == _allowed_peers.end())
            peers_to_disconnect.push_back(peer);
      for (const peer_connection_ptr& peer : peers_to_disconnect)
        disconnect_from_peer(peer.get(), "My allowed_peers list has changed, and you're no longer allowed.  Bye.");
#endif // ENABLE_P2P_DEBUGGING_API
    }
    void node_impl::clear_peer_database()
    {
      VERIFY_CORRECT_THREAD();
      _potential_peer_db.clear();
    }

    void node_impl::set_total_bandwidth_limit( uint32_t upload_bytes_per_second, uint32_t download_bytes_per_second )
    {
      VERIFY_CORRECT_THREAD();
      _rate_limiter.set_upload_limit( upload_bytes_per_second );
      _rate_limiter.set_download_limit( download_bytes_per_second );
    }

    fc::variant_object node_impl::get_call_statistics() const
    {
      VERIFY_CORRECT_THREAD();
      return _delegate->get_call_statistics();
    }

    fc::variant_object node_impl::network_get_info() const
    {
      VERIFY_CORRECT_THREAD();
      fc::mutable_variant_object info;
      info["listening_on"] = _actual_listening_endpoint;
      info["node_public_key"] = _node_public_key;
      info["node_id"] = _node_id;
      info["firewalled"] = _is_firewalled;
      return info;
    }
    fc::variant_object node_impl::network_get_usage_stats() const
    {
      VERIFY_CORRECT_THREAD();
      std::vector<uint32_t> network_usage_by_second;
      network_usage_by_second.reserve(_average_network_read_speed_seconds.size());
      std::transform(_average_network_read_speed_seconds.begin(), _average_network_read_speed_seconds.end(),
                     _average_network_write_speed_seconds.begin(),
                     std::back_inserter(network_usage_by_second),
                     std::plus<uint32_t>());

      std::vector<uint32_t> network_usage_by_minute;
      network_usage_by_minute.reserve(_average_network_read_speed_minutes.size());
      std::transform(_average_network_read_speed_minutes.begin(), _average_network_read_speed_minutes.end(),
                     _average_network_write_speed_minutes.begin(),
                     std::back_inserter(network_usage_by_minute),
                     std::plus<uint32_t>());

      std::vector<uint32_t> network_usage_by_hour;
      network_usage_by_hour.reserve(_average_network_read_speed_hours.size());
      std::transform(_average_network_read_speed_hours.begin(), _average_network_read_speed_hours.end(),
                     _average_network_write_speed_hours.begin(),
                     std::back_inserter(network_usage_by_hour),
                     std::plus<uint32_t>());

      fc::mutable_variant_object result;
      result["usage_by_second"] = network_usage_by_second;
      result["usage_by_minute"] = network_usage_by_minute;
      result["usage_by_hour"] = network_usage_by_hour;
      return result;
    }

    bool node_impl::is_hard_fork_block(uint32_t block_number) const
    {
      return std::binary_search(_hard_fork_block_numbers.begin(), _hard_fork_block_numbers.end(), block_number);
    }
    uint32_t node_impl::get_next_known_hard_fork_block_number(uint32_t block_number) const
    {
      auto iter = std::upper_bound(_hard_fork_block_numbers.begin(), _hard_fork_block_numbers.end(),
                                   block_number);
      return iter != _hard_fork_block_numbers.end() ? *iter : 0;
    }

    bool node_impl::node_is_shutting_down() const
    {
      return _node_is_shutting_down || theApp.is_interrupt_request();
    }

    void node_impl::send_message_timing_to_statsd( peer_connection* originating_peer, const message& received_message, const message_hash_type& message_hash )
    {
      if( hive::plugins::statsd::util::statsd_enabled( theApp ) )
      {
        auto iter = originating_peer->items_requested_from_peer.find( item_id( received_message.msg_type, message_hash ) );
        if( iter != originating_peer->items_requested_from_peer.end() )
        {
          hive::plugins::statsd::util::get_statsd( theApp ).timing(
            "p2p",
            "latency",
            fc::variant( core_message_type_enum( received_message.msg_type ) ).as_string(),
            hive::plugins::statsd::util::timing_helper( fc::time_point::now() - iter->second ),
            0.1f
          );
        }
      }
    }

  }  // end namespace detail



  /////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // implement node functions, they call the matching function in to detail::node_impl in the correct thread //

#ifdef P2P_IN_DEDICATED_THREAD
# define INVOKE_IN_IMPL(method_name, ...) \
    return my->_thread->async([&](){ return my->method_name(__VA_ARGS__); }, "thread invoke for method " BOOST_PP_STRINGIZE(method_name)).wait()
#else
# define INVOKE_IN_IMPL(method_name, ...) \
    return my->method_name(__VA_ARGS__)
#endif // P2P_IN_DEDICATED_THREAD

  node::node(const std::string& user_agent, appbase::application& app, hive::chain::blockchain_worker_thread_pool& thread_pool) :
    my(new detail::node_impl(user_agent, app, thread_pool))
  {
  }

  node::~node()
  {
  }

  void node::set_node_delegate( node_delegate* del )
  {
    INVOKE_IN_IMPL(set_node_delegate, del);
  }

  void node::load_configuration( const fc::path& configuration_directory )
  {
    INVOKE_IN_IMPL(load_configuration, configuration_directory);
  }

  void node::listen_to_p2p_network( std::function<bool()> break_callback )
  {
    INVOKE_IN_IMPL(listen_to_p2p_network, break_callback);
  }

  void node::connect_to_p2p_network()
  {
    INVOKE_IN_IMPL(connect_to_p2p_network);
  }

  void node::add_node( const fc::ip::endpoint& ep )
  {
    INVOKE_IN_IMPL(add_node, ep);
  }

  void node::connect_to_endpoint( const fc::ip::endpoint& remote_endpoint )
  {
    INVOKE_IN_IMPL(connect_to_endpoint, remote_endpoint);
  }

  void node::listen_on_endpoint(const fc::ip::endpoint& ep , bool wait_if_not_available)
  {
    INVOKE_IN_IMPL(listen_on_endpoint, ep, wait_if_not_available);
  }

  void node::accept_incoming_connections(bool accept)
  {
    INVOKE_IN_IMPL(accept_incoming_connections, accept);
  }

  void node::listen_on_port( uint16_t port, bool wait_if_not_available )
  {
    INVOKE_IN_IMPL(listen_on_port, port, wait_if_not_available);
  }

  fc::ip::endpoint node::get_actual_listening_endpoint() const
  {
    INVOKE_IN_IMPL(get_actual_listening_endpoint);
  }

  std::vector<peer_status> node::get_connected_peers() const
  {
    INVOKE_IN_IMPL(get_connected_peers);
  }

  uint32_t node::get_connection_count() const
  {
    INVOKE_IN_IMPL(get_connection_count);
  }

  void node::broadcast(const std::shared_ptr<full_block_type>& full_block) 
  { 
    INVOKE_IN_IMPL(broadcast, full_block);
  }

  void node::broadcast(const std::shared_ptr<full_transaction_type>& full_transaction) 
  { 
    INVOKE_IN_IMPL(broadcast, full_transaction);
  }

  void node::sync_from(const item_id& current_head_block, const std::vector<uint32_t>& hard_fork_block_numbers)
  {
    INVOKE_IN_IMPL(sync_from, current_head_block, hard_fork_block_numbers);
  }

  bool node::is_connected() const
  {
    INVOKE_IN_IMPL(is_connected);
  }

  std::vector<potential_peer_record> node::get_potential_peers()const
  {
    INVOKE_IN_IMPL(get_potential_peers);
  }

  void node::set_advanced_node_parameters( const fc::variant_object& params )
  {
    INVOKE_IN_IMPL(set_advanced_node_parameters, params);
  }

  node_configuration node::get_advanced_node_parameters()const
  {
    INVOKE_IN_IMPL(get_advanced_node_parameters);
  }

  message_propagation_data node::get_transaction_propagation_data( const graphene::net::transaction_id_type& transaction_id )
  {
    INVOKE_IN_IMPL(get_transaction_propagation_data, transaction_id);
  }

  message_propagation_data node::get_block_propagation_data( const graphene::net::block_id_type& block_id )
  {
    INVOKE_IN_IMPL(get_block_propagation_data, block_id);
  }

  node_id_t node::get_node_id() const
  {
    INVOKE_IN_IMPL(get_node_id);
  }

  void node::set_allowed_peers( const std::vector<node_id_t>& allowed_peers )
  {
    INVOKE_IN_IMPL(set_allowed_peers, allowed_peers);
  }

  void node::clear_peer_database()
  {
    INVOKE_IN_IMPL(clear_peer_database);
  }

  void node::set_total_bandwidth_limit(uint32_t upload_bytes_per_second,
                                       uint32_t download_bytes_per_second)
  {
    INVOKE_IN_IMPL(set_total_bandwidth_limit, upload_bytes_per_second, download_bytes_per_second);
  }

  fc::variant_object node::get_call_statistics() const
  {
    INVOKE_IN_IMPL(get_call_statistics);
  }

  fc::variant_object node::network_get_info() const
  {
    INVOKE_IN_IMPL(network_get_info);
  }

  fc::variant_object node::network_get_usage_stats() const
  {
    INVOKE_IN_IMPL(network_get_usage_stats);
  }

  void node::close()
  {
    INVOKE_IN_IMPL(close);
  }

  struct simulated_network::node_info
  {
    node_delegate* delegate;
    fc::future<void> message_sender_task_done;
    typedef std::variant<std::shared_ptr<full_block_type>, std::shared_ptr<full_transaction_type>> block_or_transaction;
    std::queue<block_or_transaction> messages_to_deliver;
    node_info(node_delegate* delegate) : delegate(delegate) {}
  };

  simulated_network::~simulated_network()
  {
    for (node_info* network_node_info : network_nodes)
    {
      network_node_info->message_sender_task_done.cancel_and_wait("~simulated_network()");
      delete network_node_info;
    }
  }

  template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
  template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

  void simulated_network::message_sender(node_info* destination_node)
  {
    while (!destination_node->messages_to_deliver.empty())
    {
      try
      {
        const node_info::block_or_transaction& message_to_deliver = destination_node->messages_to_deliver.front();
        std::visit(overloaded {
                     [&](const std::shared_ptr<full_block_type>& full_block) {
                       std::vector<fc::uint160_t> contained_transaction_message_ids;
                       destination_node->delegate->handle_block(full_block, false);
                     },
                     [&](const std::shared_ptr<full_transaction_type>& full_transaction) {
                       destination_node->delegate->submit_transaction(full_transaction, [](const fc::exception_ptr&) {});
                     }
                   }, message_to_deliver);
      }
      catch (const fc::exception& e)
      {
        elog("${e}", (e));
      }
      destination_node->messages_to_deliver.pop();
    }
  }

  void simulated_network::broadcast(const std::shared_ptr<full_block_type>& full_block)
  {
    for (node_info* network_node_info : network_nodes)
    {
      network_node_info->messages_to_deliver.emplace(full_block);
      if (!network_node_info->message_sender_task_done.valid() || network_node_info->message_sender_task_done.ready())
        network_node_info->message_sender_task_done = fc::async([=, this](){ message_sender(network_node_info); }, "simulated_network_sender");
    }
  }

  void simulated_network::broadcast(const std::shared_ptr<hive::chain::full_transaction_type>& full_transaction)
  {
    for (node_info* network_node_info : network_nodes)
    {
      network_node_info->messages_to_deliver.emplace(full_transaction);
      if (!network_node_info->message_sender_task_done.valid() || network_node_info->message_sender_task_done.ready())
        network_node_info->message_sender_task_done = fc::async([=, this](){ message_sender(network_node_info); }, "simulated_network_sender");
    }
  }

  void simulated_network::add_node_delegate(node_delegate* node_delegate_to_add)
  {
    network_nodes.push_back(new node_info(node_delegate_to_add));
  }

  namespace detail
  {
#define ROLLING_WINDOW_SIZE 1000
#define INITIALIZE_ACCUMULATOR(r, data, method_name) \
      , BOOST_PP_CAT(_, BOOST_PP_CAT(method_name, _execution_accumulator))(boost::accumulators::tag::rolling_window::window_size = ROLLING_WINDOW_SIZE) \
      , BOOST_PP_CAT(_, BOOST_PP_CAT(method_name, _delay_before_accumulator))(boost::accumulators::tag::rolling_window::window_size = ROLLING_WINDOW_SIZE) \
      , BOOST_PP_CAT(_, BOOST_PP_CAT(method_name, _delay_after_accumulator))(boost::accumulators::tag::rolling_window::window_size = ROLLING_WINDOW_SIZE)


    statistics_gathering_node_delegate_wrapper::statistics_gathering_node_delegate_wrapper(node_delegate* delegate) :
      _node_delegate(delegate)
      BOOST_PP_SEQ_FOR_EACH(INITIALIZE_ACCUMULATOR, unused, NODE_DELEGATE_METHOD_NAMES)
    {}
#undef INITIALIZE_ACCUMULATOR

    fc::variant_object statistics_gathering_node_delegate_wrapper::get_call_statistics()
    {
      fc::mutable_variant_object statistics;
      std::ostringstream note;
      note << "All times are in microseconds, mean is the average of the last " << ROLLING_WINDOW_SIZE << " call times";
      statistics["_note"] = note.str();

#define ADD_STATISTICS_FOR_METHOD(r, data, method_name) \
      fc::mutable_variant_object BOOST_PP_CAT(method_name, _stats); \
      BOOST_PP_CAT(method_name, _stats)["min"] = boost::accumulators::min(BOOST_PP_CAT(_, BOOST_PP_CAT(method_name, _execution_accumulator))); \
      BOOST_PP_CAT(method_name, _stats)["mean"] = boost::accumulators::rolling_mean(BOOST_PP_CAT(_, BOOST_PP_CAT(method_name, _execution_accumulator))); \
      BOOST_PP_CAT(method_name, _stats)["max"] = boost::accumulators::max(BOOST_PP_CAT(_, BOOST_PP_CAT(method_name, _execution_accumulator))); \
      BOOST_PP_CAT(method_name, _stats)["sum"] = boost::accumulators::sum(BOOST_PP_CAT(_, BOOST_PP_CAT(method_name, _execution_accumulator))); \
      BOOST_PP_CAT(method_name, _stats)["delay_before_min"] = boost::accumulators::min(BOOST_PP_CAT(_, BOOST_PP_CAT(method_name, _delay_before_accumulator))); \
      BOOST_PP_CAT(method_name, _stats)["delay_before_mean"] = boost::accumulators::rolling_mean(BOOST_PP_CAT(_, BOOST_PP_CAT(method_name, _delay_before_accumulator))); \
      BOOST_PP_CAT(method_name, _stats)["delay_before_max"] = boost::accumulators::max(BOOST_PP_CAT(_, BOOST_PP_CAT(method_name, _delay_before_accumulator))); \
      BOOST_PP_CAT(method_name, _stats)["delay_before_sum"] = boost::accumulators::sum(BOOST_PP_CAT(_, BOOST_PP_CAT(method_name, _delay_before_accumulator))); \
      BOOST_PP_CAT(method_name, _stats)["delay_after_min"] = boost::accumulators::min(BOOST_PP_CAT(_, BOOST_PP_CAT(method_name, _delay_after_accumulator))); \
      BOOST_PP_CAT(method_name, _stats)["delay_after_mean"] = boost::accumulators::rolling_mean(BOOST_PP_CAT(_, BOOST_PP_CAT(method_name, _delay_after_accumulator))); \
      BOOST_PP_CAT(method_name, _stats)["delay_after_max"] = boost::accumulators::max(BOOST_PP_CAT(_, BOOST_PP_CAT(method_name, _delay_after_accumulator))); \
      BOOST_PP_CAT(method_name, _stats)["delay_after_sum"] = boost::accumulators::sum(BOOST_PP_CAT(_, BOOST_PP_CAT(method_name, _delay_after_accumulator))); \
      BOOST_PP_CAT(method_name, _stats)["count"] = boost::accumulators::count(BOOST_PP_CAT(_, BOOST_PP_CAT(method_name, _execution_accumulator))); \
      statistics[BOOST_PP_STRINGIZE(method_name)] = BOOST_PP_CAT(method_name, _stats);

      BOOST_PP_SEQ_FOR_EACH(ADD_STATISTICS_FOR_METHOD, unused, NODE_DELEGATE_METHOD_NAMES)
#undef ADD_STATISTICS_FOR_METHOD

      return statistics;
    }

// The node delegate is always invoked directly on the p2p thread: the old cross-thread
// marshalling (and the VERBOSE_NODE_DELEGATE_LOGGING debug variant that retained it) was
// removed after years of disuse.  Thread safety is the callee's responsibility -- the block
// log supports concurrent readers, the fork database takes its own locks, and chain
// mutations are serialized through the chain plugin's write queue.
#define INVOKE_AND_COLLECT_STATISTICS( method_name, ... ) \
   return _node_delegate->method_name(__VA_ARGS__);

    hive::protocol::chain_id_type statistics_gathering_node_delegate_wrapper::get_new_chain_id() const
    {
      INVOKE_AND_COLLECT_STATISTICS(get_new_chain_id);
    }

    hive::protocol::chain_id_type statistics_gathering_node_delegate_wrapper::get_chain_id() const
    {
      INVOKE_AND_COLLECT_STATISTICS(get_chain_id);
    }

    bool statistics_gathering_node_delegate_wrapper::has_item( const net::item_id& id )
    {
      INVOKE_AND_COLLECT_STATISTICS(has_item, id);
    }

    void statistics_gathering_node_delegate_wrapper::handle_message( const message& message_to_handle )
    {
      INVOKE_AND_COLLECT_STATISTICS(handle_message, message_to_handle);
    }

    block_handling_result statistics_gathering_node_delegate_wrapper::handle_block(const std::shared_ptr<full_block_type>& full_block, bool sync_mode)
    {
      INVOKE_AND_COLLECT_STATISTICS(handle_block, full_block, sync_mode);
    }

    void statistics_gathering_node_delegate_wrapper::submit_transaction(const std::shared_ptr<full_transaction_type>& full_transaction,
                                                                        transaction_validated_callback on_validated)
    {
      INVOKE_AND_COLLECT_STATISTICS(submit_transaction, full_transaction, std::move(on_validated));
    }

    std::vector<item_hash_t> statistics_gathering_node_delegate_wrapper::get_block_ids(const std::vector<item_hash_t>& blockchain_synopsis,
                                                                                       uint32_t& remaining_item_count,
                                                                                       uint32_t limit /* = 2000 */)
    {
      INVOKE_AND_COLLECT_STATISTICS(get_block_ids, blockchain_synopsis, remaining_item_count, limit);
    }

    std::shared_ptr<hive::chain::full_block_type> statistics_gathering_node_delegate_wrapper::get_full_block(const block_id_type& id)
    {
      INVOKE_AND_COLLECT_STATISTICS(get_full_block, id);
    }

    std::vector<item_hash_t> statistics_gathering_node_delegate_wrapper::get_blockchain_synopsis(const item_hash_t& reference_point, uint32_t number_of_blocks_after_reference_point)
    {
      INVOKE_AND_COLLECT_STATISTICS(get_blockchain_synopsis, reference_point, number_of_blocks_after_reference_point);
    }

    void statistics_gathering_node_delegate_wrapper::sync_status( uint32_t item_type, uint32_t item_count )
    {
      INVOKE_AND_COLLECT_STATISTICS(sync_status, item_type, item_count);
    }

    void statistics_gathering_node_delegate_wrapper::connection_count_changed( uint32_t c )
    {
      INVOKE_AND_COLLECT_STATISTICS(connection_count_changed, c);
    }

    uint32_t statistics_gathering_node_delegate_wrapper::get_block_number(const item_hash_t& block_id)
    {
      // this function doesn't need to block,
      ASSERT_TASK_NOT_PREEMPTED();
      return _node_delegate->get_block_number(block_id);
    }

    fc::time_point_sec statistics_gathering_node_delegate_wrapper::get_block_time(const item_hash_t& block_id)
    {
      INVOKE_AND_COLLECT_STATISTICS(get_block_time, block_id);
    }

    /** returns graphene::blockchain::now() */
    fc::time_point_sec statistics_gathering_node_delegate_wrapper::get_blockchain_now()
    {
      // this function doesn't need to block,
      ASSERT_TASK_NOT_PREEMPTED();
      return _node_delegate->get_blockchain_now();
    }

    item_hash_t statistics_gathering_node_delegate_wrapper::get_head_block_id() const
    {
      INVOKE_AND_COLLECT_STATISTICS(get_head_block_id);
    }

    uint32_t statistics_gathering_node_delegate_wrapper::get_last_irreversible_block_num() const
    {
      INVOKE_AND_COLLECT_STATISTICS(get_last_irreversible_block_num);
    }

    uint32_t statistics_gathering_node_delegate_wrapper::get_tail_block_num() const
    {
      INVOKE_AND_COLLECT_STATISTICS(get_tail_block_num);
    }

    bool statistics_gathering_node_delegate_wrapper::is_at_live_sync()
    {
      INVOKE_AND_COLLECT_STATISTICS(is_at_live_sync);
    }

    pending_transaction_pool_size statistics_gathering_node_delegate_wrapper::get_pending_transaction_pool_size() const
    {
      // this function doesn't need to block,
      ASSERT_TASK_NOT_PREEMPTED();
      return _node_delegate->get_pending_transaction_pool_size();
    }

    uint32_t statistics_gathering_node_delegate_wrapper::estimate_last_known_fork_from_git_revision_timestamp(uint32_t unix_timestamp) const
    {
      INVOKE_AND_COLLECT_STATISTICS(estimate_last_known_fork_from_git_revision_timestamp, unix_timestamp);
    }

    void statistics_gathering_node_delegate_wrapper::error_encountered(const std::string& message, const fc::oexception& error)
    {
      INVOKE_AND_COLLECT_STATISTICS(error_encountered, message, error);
    }

    std::deque<block_id_type>::const_iterator statistics_gathering_node_delegate_wrapper::find_first_item_not_in_blockchain(const std::deque<block_id_type>& item_hashes_received)
    {
      INVOKE_AND_COLLECT_STATISTICS(find_first_item_not_in_blockchain, item_hashes_received);
    }

#undef INVOKE_AND_COLLECT_STATISTICS

  } // end namespace detail

  bool rejects_duplicate_hello(const node_id_t& my_node_id, const node_id_t& peer_node_id)
  {
    return !(peer_node_id < my_node_id);
  }

  bool rejection_is_failure(rejection_reason_code reason_code)
  {
    return reason_code != rejection_reason_code::already_connected &&
           reason_code != rejection_reason_code::connected_to_self;
  }

  bool peer_needs_blocks_we_pruned(uint32_t peers_head_block_num, uint32_t our_tail_block_num)
  {
    return uint64_t(peers_head_block_num) + GRAPHENE_NET_PRUNED_TAIL_GRACE_BLOCKS < our_tail_block_num;
  }

  disconnect_breaker::disconnect_breaker(uint32_t count, fc::microseconds window)
    : _count(count), _window(window)
  {
  }

  bool disconnect_breaker::tripped(fc::time_point now)
  {
    const fc::time_point window_start = now - _window;
    while (!_recent_disconnect_times.empty() && _recent_disconnect_times.front() < window_start)
      _recent_disconnect_times.pop_front();
    if (_recent_disconnect_times.size() >= _count)
      return true;
    _recent_disconnect_times.push_back(now);
    return false;
  }

  bool unavailable_item_strikes::record(fc::time_point now)
  {
    const fc::time_point forgotten_before = now - fc::seconds(GRAPHENE_NET_UNAVAILABLE_ITEM_STRIKE_DECAY_SEC);
    while (!_strike_times.empty() && _strike_times.front() <= forgotten_before)
      _strike_times.pop_front();
    if (!_strike_times.empty() && now - _strike_times.back() < fc::seconds(GRAPHENE_NET_UNAVAILABLE_ITEM_DEMOTION_SEC))
      return false;
    _strike_times.push_back(now);
    return true;
  }

  void unavailable_item_strikes::clear()
  {
    _strike_times.clear();
  }

  uint32_t unavailable_item_strikes::count(fc::time_point now) const
  {
    const fc::time_point forgotten_before = now - fc::seconds(GRAPHENE_NET_UNAVAILABLE_ITEM_STRIKE_DECAY_SEC);
    return static_cast<uint32_t>(std::count_if(_strike_times.begin(), _strike_times.end(),
                                               [&](fc::time_point strike) { return strike > forgotten_before; }));
  }

  void transaction_busy_time::advance_window(fc::time_point now)
  {
    const fc::microseconds window = fc::seconds(GRAPHENE_NET_TRANSACTION_BUSY_WINDOW_SEC);
    if (now - _window_start < window)
      return;
    _window_start += fc::microseconds(window.count() * ((now - _window_start).count() / window.count()));
    _busy_in_window = fc::microseconds();
    if (_busy_since)
      _busy_since = std::max(*_busy_since, _window_start);
  }

  void transaction_busy_time::record_busy(fc::time_point requested, fc::time_point now)
  {
    if (_window_start == fc::time_point())
      _window_start = requested;
    advance_window(now);
    const fc::time_point since = std::max({requested, _counted_until, _window_start});
    if (!_busy_since || since < *_busy_since)
      _busy_since = since;
  }

  void transaction_busy_time::record_delivery(fc::time_point now)
  {
    advance_window(now);
    if (!_busy_since)
      return;
    _busy_in_window += now - *_busy_since;
    _busy_since.reset();
    _counted_until = now;
  }

  bool transaction_busy_time::mostly_busy(fc::time_point now)
  {
    advance_window(now);
    const fc::microseconds busy = _busy_in_window + (_busy_since ? now - *_busy_since : fc::microseconds());
    return 2 * busy.count() > fc::seconds(GRAPHENE_NET_TRANSACTION_BUSY_WINDOW_SEC).count();
  }

  void coalescing_loop_trigger::begin_iteration()
  {
    _iteration_start = fc::time_point::now();
    _triggered = false;
    _urgent = false;
  }

  void coalescing_loop_trigger::trigger(bool urgent)
  {
    // another plain trigger while one is pending changes nothing the waiting loop decides on, so it doesn't wake it
    const bool changes_wait = !_triggered || (urgent && !_urgent);
    _triggered = true;
    _urgent = _urgent || urgent;
    if (changes_wait && _wake_promise)
      _wake_promise->set_value();
  }

  void coalescing_loop_trigger::wait(fc::microseconds minimum_interval, fc::time_point wake_at)
  {
    for (;;)
    {
      fc::time_point wake_time = wake_at;
      if (_triggered)
      {
        if (_urgent)
          return;
        wake_time = std::min(wake_time, _iteration_start + minimum_interval);
      }
      if (wake_time <= fc::time_point::now())
        return;
      _wake_promise = fc::promise<void>::create("graphene::net::coalescing_loop_trigger");
      try
      {
        if (wake_time == fc::time_point::maximum())
          _wake_promise->wait();
        else
          _wake_promise->wait_until(wake_time);
      }
      catch (const fc::timeout_exception&)
      {
        // a deadline passed: the next pass of the loop returns
      }
      _wake_promise.reset();
    }
  }

  void in_flight_limit::acquire(uint32_t limit)
  {
    while (!_closed && limit != 0 && _in_flight >= limit)
    {
      if (!_released)
        _released = fc::promise<void>::create("graphene::net::in_flight_limit");
      // keep our own reference, wake_waiters() drops the member
      fc::promise<void>::ptr released = _released;
      released->wait();
    }
    if (_closed)
      FC_THROW_EXCEPTION(fc::canceled_exception, "no more items are accepted for processing, the node is shutting down");
    ++_in_flight;
  }

  void in_flight_limit::release(uint32_t count)
  {
    _in_flight -= std::min(count, _in_flight);
    if (count != 0)
      wake_waiters();
  }

  void in_flight_limit::close()
  {
    _closed = true;
    wake_waiters();
  }

  void in_flight_limit::wake_waiters()
  {
    if (_released)
      std::exchange(_released, nullptr)->set_value();
  }

  void fetch_candidate_queue::push(const item_id& item, fc::time_point now, fc::time_point oldest_to_keep)
  {
    trim(oldest_to_keep);
    (item.item_type == block_message_type ? _blocks : _transactions).push_back(entry{item, now});
  }

  void fetch_candidate_queue::trim(fc::time_point oldest_to_keep)
  {
    // both lanes are in the order entries were queued, so the stale ones are at their front
    for (auto [lane, kept] : { std::make_pair(&_blocks, &_blocks_kept), std::make_pair(&_transactions, &_transactions_kept) })
      while (!lane->empty() && lane->front().queued < oldest_to_keep)
      {
        lane->pop_front();
        if (*kept > 0)
          --*kept;
      }
    // once the entries kept from the front are gone, the ones kept from the back are trimmed next
    _transactions_newest_kept = std::min(_transactions_newest_kept, _transactions.size() - _transactions_kept);
  }

  void fetch_candidate_queue::begin_pass()
  {
    _blocks_kept = 0;
    _transactions_kept = 0;
    _transactions_newest_kept = 0;
  }

  bool fetch_candidate_queue::has_block(const item_id& item) const
  {
    return std::any_of(_blocks.begin(), _blocks.end(), [&](const entry& queued) { return queued.item == item; });
  }

  void fetch_candidate_queue::clear()
  {
    _blocks.clear();
    _transactions.clear();
    begin_pass();
  }

  transaction_size_estimate::transaction_size_estimate() :
    _mean(GRAPHENE_NET_INITIAL_TRANSACTION_SIZE_ESTIMATE)
  {}

  void transaction_size_estimate::record(size_t transaction_size)
  {
    _recorded = true;
    if (transaction_size > _mean)
      _mean += (transaction_size - _mean + 3) / 4;
    else
      _mean -= (_mean - transaction_size) / 16;
  }

  size_t transaction_size_estimate::max_transactions_per_request() const
  {
    return std::clamp<size_t>(GRAPHENE_NET_MAX_TRANSACTION_BYTES_PER_FETCH / std::max<size_t>(_mean, 1),
                              1, GRAPHENE_NET_MAX_TRANSACTIONS_PER_PEER_DURING_NORMAL_OPERATION);
  }

  size_t transaction_size_estimate::transactions_to_request(size_t in_flight) const
  {
    const size_t per_request = max_transactions_per_request();
    // a peer that queues each reply at once drops us when they take its queue over its limit: it is asked for no
    // more than it would have queued for us before requests were pipelined
    const size_t window = !_peer_defers_replies ? per_request : std::max(per_request,
                                   std::min<size_t>(GRAPHENE_NET_MAX_TRANSACTIONS_IN_FLIGHT_PER_PEER,
                                                    GRAPHENE_NET_MAX_TRANSACTION_BYTES_IN_FLIGHT_PER_PEER / std::max<size_t>(_mean, 1)));
    if (in_flight >= window)
      return 0;
    const size_t room = window - in_flight;
    if (in_flight > 0 && room < (per_request + 1) / 2)
      return 0;
    return room;
  }

  void requested_items::insert(const item_id& item, fc::time_point requested, fc::microseconds block_deadline)
  {
    if (!_items.emplace(item, requested).second)
      return;
    if (item.item_type == trx_message_type)
      ++_transactions;
    else
      _block_requests.emplace(item, block_request{block_deadline, fc::time_point()});
  }

  void requested_items::erase(const_iterator iter)
  {
    if (iter->first.item_type == trx_message_type)
      --_transactions;
    else
      _block_requests.erase(iter->first);
    _items.erase(iter);
  }

  fc::microseconds requested_items::block_deadline(const item_id& item) const
  {
    const auto iter = _block_requests.find(item);
    return iter != _block_requests.end() ? iter->second.deadline : fc::microseconds();
  }

  void requested_items::record_block_requests_written(fc::time_point queued, fc::time_point written)
  {
    for (auto& [item, request] : _block_requests)
      if (request.written == fc::time_point() && _items.at(item) <= queued)
        request.written = written;
  }

  fc::time_point requested_items::block_request_sent(const item_id& item) const
  {
    const auto iter = _block_requests.find(item);
    if (iter != _block_requests.end() && iter->second.written != fc::time_point())
      return iter->second.written;
    const auto requested = _items.find(item);
    return requested != _items.end() ? requested->second : fc::time_point();
  }

  std::optional<item_id> requested_items::timed_out(fc::time_point now, fc::microseconds timeout,
                                                    const message_in_progress* block_download) const
  {
    const fc::time_point threshold = now - timeout;
    const bool no_recent_transaction_delivery = _last_transaction_delivery < threshold;
    const bool no_recent_block_download_progress = !block_download || block_download->last_progress < threshold;
    const fc::time_point ceiling = now - fc::microseconds(timeout.count() * GRAPHENE_NET_MAX_TRANSACTION_REQUEST_TIMEOUTS_WHILE_DELIVERING);
    const fc::time_point block_ceiling = now - fc::microseconds(timeout.count() * GRAPHENE_NET_MAX_BLOCK_REQUEST_TIMEOUTS_WHILE_DOWNLOADING);
    for (const map_type::value_type& item_and_time : _items)
      if (item_and_time.second < threshold &&
          (item_and_time.first.item_type != trx_message_type ? no_recent_block_download_progress || item_and_time.second < block_ceiling
                                                              : no_recent_transaction_delivery || item_and_time.second < ceiling))
        return item_and_time.first;
    return std::nullopt;
  }

  bool transaction_inventory::record::test(slot_type slot, unsigned kind) const
  {
    const size_t word = record::kinds * (slot / 64) + kind;
    return word < bits.size() && (bits[word] >> (slot % 64)) & 1;
  }

  bool transaction_inventory::record::set(slot_type slot, unsigned kind) const
  {
    const size_t word = record::kinds * (slot / 64) + kind;
    if (word >= bits.size())
      bits.resize(record::kinds * (slot / 64 + 1), 0);
    const uint64_t mask = uint64_t(1) << (slot % 64);
    const bool was_clear = !(bits[word] & mask);
    bits[word] |= mask;
    return was_clear;
  }

  bool transaction_inventory::record::reset(slot_type slot, unsigned kind) const
  {
    const size_t word = record::kinds * (slot / 64) + kind;
    if (word >= bits.size())
      return false;
    const uint64_t mask = uint64_t(1) << (slot % 64);
    const bool was_set = bits[word] & mask;
    bits[word] &= ~mask;
    return was_set;
  }

  transaction_inventory::slot_type transaction_inventory::acquire_slot()
  {
    auto free_slot = std::find_if(_slots.begin(), _slots.end(), [](const slot_state& state) { return !state.in_use; });
    if (free_slot == _slots.end())
      free_slot = _slots.insert(_slots.end(), slot_state());
    free_slot->in_use = true;
    return (slot_type)(free_slot - _slots.begin());
  }

  void transaction_inventory::release_slot(slot_type slot)
  {
    FC_ASSERT(slot < _slots.size() && _slots[slot].in_use, "releasing transaction inventory slot ${slot}, which is not in use", (slot));
    for (const record& item_record : _records)
      for (unsigned kind = 0; kind < record::kinds; ++kind)
        item_record.reset(slot, kind);
    _slots[slot] = slot_state();
  }

  const transaction_inventory::record* transaction_inventory::find(const item_hash_t& item) const
  {
    const auto iter = _records.find(item);
    return iter == _records.end() ? nullptr : &*iter;
  }

  const transaction_inventory::record& transaction_inventory::get(const item_hash_t& item, fc::time_point_sec now)
  {
    const auto [iter, inserted] = _records.insert(record(item));
    if (inserted)
      iter->last_recorded = now;
    return *iter;
  }

  bool transaction_inventory::advertised_to_us(slot_type slot, const item_hash_t& item) const
  {
    const record* item_record = find(item);
    return item_record && item_record->advertised_to_us(slot);
  }

  bool transaction_inventory::record_advertised_to_us(slot_type slot, const item_hash_t& item, fc::time_point_sec now)
  {
    return set_bit_and_record_time(get(item, now), slot, record::advertised_to_us_kind, now);
  }

  void transaction_inventory::forget_advertised_to_us(slot_type slot, const item_hash_t& item)
  {
    if (const record* item_record = find(item))
      reset_bit(*item_record, slot, record::advertised_to_us_kind);
  }

  bool transaction_inventory::record_advertised_to_peer(const record& item_record, slot_type slot, fc::time_point_sec now)
  {
    return set_bit_and_record_time(item_record, slot, record::advertised_to_peer_kind, now);
  }

  void transaction_inventory::keep(const item_hash_t& item, fc::time_point_sec now)
  {
    if (const record* item_record = find(item))
      record_time(*item_record, now);
  }

  // like a set entry moved from one kind to the other, the moved bits don't make the record newer
  void transaction_inventory::mark_received(const item_hash_t& item)
  {
    const record* item_record = find(item);
    if (!item_record)
      return;
    for (size_t word = 0; word < item_record->bits.size(); word += record::kinds)
      for (uint64_t moving = item_record->bits[word + record::advertised_to_us_kind]; moving != 0; moving &= moving - 1)
      {
        const slot_type slot = (slot_type)(64 * (word / record::kinds) + boost::core::countr_zero(moving));
        reset_bit(*item_record, slot, record::advertised_to_us_kind);
        set_bit(*item_record, slot, record::advertised_to_peer_kind);
      }
  }

  void transaction_inventory::expire(fc::time_point_sec oldest_to_keep)
  {
    auto& by_time = _records.get<by_last_recorded>();
    while (!by_time.empty() && by_time.front().last_recorded < oldest_to_keep)
    {
      const record& expiring = by_time.front();
      for (size_t word = 0; word < expiring.bits.size(); ++word)
        for (uint64_t remaining = expiring.bits[word]; remaining != 0; remaining &= remaining - 1)
          --_slots[64 * (word / record::kinds) + boost::core::countr_zero(remaining)].counts[word % record::kinds];
      by_time.pop_front();
    }
  }

  uint32_t transaction_inventory::advertised_to_us_count(slot_type slot) const
  {
    return slot < _slots.size() ? _slots[slot].counts[record::advertised_to_us_kind] : 0;
  }

  uint32_t transaction_inventory::advertised_to_peer_count(slot_type slot) const
  {
    return slot < _slots.size() ? _slots[slot].counts[record::advertised_to_peer_kind] : 0;
  }

  void transaction_inventory::set_bit(const record& item_record, slot_type slot, unsigned kind)
  {
    if (item_record.set(slot, kind))
      ++_slots[slot].counts[kind];
  }

  // the record moves to the back of the expiry order, which stays sorted as long as the clock doesn't go back;
  // if it does, records stamped later wait behind it and expire late, never early
  bool transaction_inventory::set_bit_and_record_time(const record& item_record, slot_type slot, unsigned kind, fc::time_point_sec now)
  {
    if (!item_record.set(slot, kind))
      return false;
    ++_slots[slot].counts[kind];
    record_time(item_record, now);
    return true;
  }

  void transaction_inventory::record_time(const record& item_record, fc::time_point_sec now)
  {
    if (now > item_record.last_recorded)
    {
      item_record.last_recorded = now;
      auto& by_time = _records.get<by_last_recorded>();
      by_time.relocate(by_time.end(), by_time.iterator_to(item_record));
    }
  }

  void transaction_inventory::reset_bit(const record& item_record, slot_type slot, unsigned kind)
  {
    if (item_record.reset(slot, kind))
      --_slots[slot].counts[kind];
  }

  void p2p_statistics::record_submission(uint32_t in_flight)
  {
    ++_handoff.submissions;
    _handoff.max_in_flight = std::max(_handoff.max_in_flight, in_flight);
    _handoff.in_flight_sum += in_flight;
  }

  void p2p_statistics::record_limit_wait(fc::microseconds waited)
  {
    ++_handoff.limit_waits;
    _handoff.limit_wait_time += waited;
  }

  void p2p_statistics::record_completion(fc::microseconds latency)
  {
    ++_handoff.completions;
    const auto bound = std::upper_bound(completion_latency_bounds_us.begin(), completion_latency_bounds_us.end(),
                                        latency.count());
    ++_handoff.completion_latency[bound - completion_latency_bounds_us.begin()];
  }

  void p2p_statistics::record_write(size_t messages, size_t bytes)
  {
    ++_sending.writes;
    _sending.messages += messages;
    _sending.bytes += bytes;
  }

  void p2p_statistics::record_transaction_request(size_t transactions, size_t max_transactions)
  {
    ++_requests.transaction_requests;
    _requests.transactions_requested += transactions;
    if (transactions >= max_transactions && max_transactions < GRAPHENE_NET_MAX_TRANSACTIONS_PER_PEER_DURING_NORMAL_OPERATION)
      ++_requests.byte_cap_bound;
  }

  fc::variant_object p2p_statistics::take_report(fc::time_point now, uint32_t in_flight, size_t items_queued,
                                                 const pending_transaction_pool_size& pool,
                                                 std::vector<uint64_t> transactions_requested_per_peer)
  {
    const auto mean = [](uint64_t total, uint64_t count) { return count ? double(total) / double(count) : 0.0; };

    // with an even number of peers the median is the upper of the two middle counts
    std::vector<uint64_t>& per_peer = transactions_requested_per_peer;
    const auto middle = per_peer.begin() + per_peer.size() / 2;
    if (!per_peer.empty())
      std::nth_element(per_peer.begin(), middle, per_peer.end());
    fc::mutable_variant_object spread;
    spread("peers", per_peer.size())
          ("requested_peers", std::count_if(per_peer.begin(), per_peer.end(), [](uint64_t count) { return count > 0; }))
          ("min", per_peer.empty() ? 0 : *std::min_element(per_peer.begin(), per_peer.end()))
          ("median", per_peer.empty() ? 0 : *middle)
          ("max", per_peer.empty() ? 0 : *std::max_element(per_peer.begin(), per_peer.end()));

    fc::mutable_variant_object latency;
    for (size_t i = 0; i < _handoff.completion_latency.size(); ++i)
      latency(i < completion_latency_bounds_us.size() ? "lt" + std::to_string(completion_latency_bounds_us[i]) + "us"
                                                      : "ge" + std::to_string(completion_latency_bounds_us.back()) + "us",
              _handoff.completion_latency[i]);

    fc::variant_object report = fc::mutable_variant_object()
      ("seconds", (now - _period_start).to_seconds())
      ("fetch", fc::mutable_variant_object()
        ("wakes", fetch.wakes)
        ("candidates_examined", fetch.candidates_examined)
        ("items_requested", fetch.items_requested)
        ("items_expired", fetch.items_expired)
        ("items_queued", items_queued)
        ("scheduling_us", fetch.scheduling_time.count()))
      ("transaction_fetch_gate", fc::mutable_variant_object()
        ("threshold_bytes", transaction_fetch_gate_threshold(pool.max_bytes))
        ("pending_bytes", pool.pending_bytes)
        ("gated", is_transaction_fetching_gated(pool))
        ("gated_wakes", fetch.transactions_gated)
        ("bounded_wakes", fetch.transactions_bounded))
      ("handoff", fc::mutable_variant_object()
        ("submissions", _handoff.submissions)
        ("completions", _handoff.completions)
        ("in_flight", in_flight)
        ("max_in_flight", _handoff.max_in_flight)
        ("mean_in_flight", mean(_handoff.in_flight_sum, _handoff.submissions))
        ("completion_latency", std::move(latency))
        ("limit_waits", _handoff.limit_waits)
        ("limit_wait_us", _handoff.limit_wait_time.count()))
      ("send", fc::mutable_variant_object()
        ("writes", _sending.writes)
        ("messages_per_write", mean(_sending.messages, _sending.writes))
        ("bytes_per_write", mean(_sending.bytes, _sending.writes)))
      ("transaction_requests", fc::mutable_variant_object()
        ("requests", _requests.transaction_requests)
        ("transactions_per_request", mean(_requests.transactions_requested, _requests.transaction_requests))
        ("byte_cap_bound", _requests.byte_cap_bound)
        ("per_peer", std::move(spread)))
      ("withholding", fc::mutable_variant_object()
        ("not_available_answers", withholding.not_available_answers)
        ("busy_answers", withholding.busy_answers)
        ("late_block_copies", withholding.late_block_copies)
        ("strikes", withholding.strikes)
        ("disconnects", withholding.disconnects));

    fetch = fetch_scheduling();
    withholding = withholding_counts();
    _handoff = transaction_handoff();
    _sending = send_batching();
    _requests = request_sizing();
    _period_start = now;
    return report;
  }

  uint64_t transaction_fetch_gate_threshold(uint64_t max_bytes)
  {
    // a max-mempool-size of 1 or 2 bytes still gates
    return max_bytes == 0 ? 0 : std::max<uint64_t>(max_bytes / GRAPHENE_NET_TRANSACTION_FETCH_GATE_POOL_DIVISOR, 1);
  }

  bool is_transaction_fetching_gated(const pending_transaction_pool_size& pool)
  {
    const uint64_t threshold = transaction_fetch_gate_threshold(pool.max_bytes);
    return threshold > 0 && pool.pending_bytes > threshold;
  }

  transaction_fetch_gate::transaction_fetch_gate(const pending_transaction_pool_size& pool, uint64_t bytes_on_their_way,
                                                 size_t minimum_transactions) :
    _closed(is_transaction_fetching_gated(pool)),
    _minimum(minimum_transactions)
  {
    const uint64_t threshold = transaction_fetch_gate_threshold(pool.max_bytes);
    if (threshold == 0)
      _headroom = std::numeric_limits<uint64_t>::max();
    else
    {
      const uint64_t committed = pool.pending_bytes + bytes_on_their_way;
      _headroom = committed < threshold ? threshold - committed : 0;
    }
  }

  void transaction_fetch_gate::record_request(uint64_t expected_bytes)
  {
    if (next_request_is_minimum())
      _requested_minimum = true;
    ++_requested;
    if (_headroom != std::numeric_limits<uint64_t>::max())
      _headroom -= std::min(_headroom, expected_bytes);
  }

  void transaction_fetch_gate::record_withheld()
  {
    if (_closed)
      _held_back = true;
    else
      _bounded = true;
  }

  fc::time_point fetch_minimum_schedule::record_pass(const transaction_fetch_gate& gate, fc::time_point pass_start)
  {
    const bool was_due = is_due(pass_start);
    if (gate.requested_minimum())
      _last_minimum = pass_start;
    return (was_due ? pass_start : _last_minimum) + _interval;
  }

  fetch_expiry_warning_limiter::fetch_expiry_warning_limiter(fc::microseconds interval) :
    _interval(interval)
  {}

  std::optional<size_t> fetch_expiry_warning_limiter::record(size_t expired, bool withheld, fc::time_point now)
  {
    _unreported += expired;
    if (withheld)
      _last_withheld = now;
    const bool limited = _last_withheld && now < *_last_withheld + _interval;
    if (_unreported == 0 || (limited && now < _last_warning + _interval))
      return std::nullopt;
    _last_warning = now;
    return std::exchange(_unreported, 0);
  }

  bool is_relay_sample(const message_hash_type& message_hash, uint32_t one_in)
  {
    return message_hash._hash[0] % one_in == 0;
  }

  int64_t raw_realtime_microseconds()
  {
    timespec now;
    syscall(SYS_clock_gettime, CLOCK_REALTIME, &now);
    return int64_t(now.tv_sec) * 1000000 + now.tv_nsec / 1000;
  }

  std::optional<size_t> choose_block_source(const std::vector<block_source_candidate>& candidates)
  {
    std::optional<size_t> first_deprioritized_available;
    std::optional<size_t> best_preferred_available;
    int best_rank = -1;
    bool preferred_source_exists = false;
    for (size_t i = 0; i < candidates.size(); ++i)
    {
      if (!candidates[i].deprioritized)
      {
        const int rank = 2 * candidates[i].delivered + candidates[i].idle;
        if (candidates[i].can_take_request && rank > best_rank)
        {
          best_preferred_available = i;
          best_rank = rank;
        }
        preferred_source_exists = true;
      }
      else if (candidates[i].can_take_request && !first_deprioritized_available)
        first_deprioritized_available = i;
    }
    if (preferred_source_exists)
      return best_preferred_available;
    return first_deprioritized_available;
  }

  fc::microseconds block_request_deadline(uint32_t earlier_requests)
  {
    return fc::milliseconds(int64_t(GRAPHENE_NET_BLOCK_REREQUEST_TIMEOUT_MS) << std::min<uint32_t>(earlier_requests, 16));
  }

  bool accepts_inbound_hello_over_host_limit(const fc::ip::address& address, const std::vector<fc::ip::address>& accepted_addresses,
                                             uint32_t desired_connections)
  {
    if (accepted_addresses.size() >= desired_connections)
      return false;
    std::vector<fc::ip::address> addresses = accepted_addresses;
    addresses.push_back(address);
    const uint32_t hosts = get_connection_spread(addresses).hosts;
    const uint32_t share = (desired_connections + hosts - 1) / hosts;
    const fc::ip::address host = connection_host_of(address);
    const auto on_host = static_cast<uint32_t>(std::count_if(addresses.begin(), addresses.end(), [&](const fc::ip::address& held) {
      return connection_host_of(held) == host;
    }));
    return on_host <= share && on_host <= desired_connections / 2;
  }

  advertised_item_fetch_decision decide_advertised_item_fetch(const advertised_item_status& status)
  {
    advertised_item_fetch_decision decision;
    if (status.being_processed || status.requested || status.recently_failed)
      return decision;
    if (!status.queued)
    {
      decision.queue = true;
      decision.wake_fetch_loop = true;
      return decision;
    }
    decision.refresh_timestamp = true;
    decision.wake_fetch_loop = status.is_block && status.new_from_this_peer;
    return decision;
  }

  peer_connection_ptr find_unclosed_connection_to_endpoint(const fc::ip::endpoint& remote_endpoint,
                                                           const std::unordered_set<peer_connection_ptr>& active_connections,
                                                           const std::unordered_set<peer_connection_ptr>& handshaking_connections,
                                                           const std::unordered_set<peer_connection_ptr>& closing_connections,
                                                           const std::unordered_set<peer_connection_ptr>& terminating_connections)
  {
    for (const std::unordered_set<peer_connection_ptr>* connections :
         { &active_connections, &handshaking_connections, &closing_connections, &terminating_connections })
      for (const peer_connection_ptr& peer : *connections)
      {
        fc::optional<fc::ip::endpoint> endpoint_for_this_peer(peer->get_remote_endpoint());
        if (endpoint_for_this_peer && *endpoint_for_this_peer == remote_endpoint)
          return peer;
      }
    return peer_connection_ptr();
  }

  bool is_due_for_connection_attempt(const potential_peer_record& record, fc::microseconds delay_until_retry,
                                     fc::time_point now)
  {
    return (record.last_connection_disposition != last_connection_failed &&
            record.last_connection_disposition != last_connection_rejected &&
            record.last_connection_disposition != last_connection_handshaking_failed) ||
           (now - record.last_connection_attempt_time) > delay_until_retry;
  }

  bool is_local_endpoint_in_use_error(const fc::exception& connect_error)
  {
    // fc::asio reports a failed connect as the text of boost::system::system_error, e.g.
    // "Cannot assign requested address [system:99 at .../reactive_socket_connect_op.hpp:...]";
    // the error code itself does not survive, so recognize its message and "[category:value"
    const boost::system::error_code in_use(EADDRNOTAVAIL, boost::system::system_category());
    const std::string expected = in_use.message() + " [" + in_use.to_string();
    for (const fc::log_message& message : connect_error.get_log())
    {
      const fc::variant_object& data = message.get_data();
      if (!data.contains("message") || !data["message"].is_string())
        continue;
      const std::string& text = data["message"].get_string();
      if (text.compare(0, expected.size(), expected) == 0 &&
          (text.size() == expected.size() || text[expected.size()] == ']' || text[expected.size()] == ' '))
        return true;
    }
    return false;
  }

  fc::ip::address connection_host_of(const fc::ip::address& address)
  {
    if (address.is_ipv4())
      return address;
    if (address.is_ipv4_mapped_ipv6())
      return address.to_ipv4_address();
    if (address.is_loopback_address()) // ::1 is a host of its own; its /64 would be ::
      return address;
    const fc::array<uint8_t, 16>& bytes = address.get_ipv6().addr;
    if (bytes.data[0] == 0xFE && (bytes.data[1] & 0xC0) == 0x80) // link-local: every machine has fe80::/64
      return address;
    std::array<uint8_t, 16> prefix;
    std::copy(address.get_ipv6().addr.begin(), address.get_ipv6().addr.end(), prefix.begin());
    std::fill(prefix.begin() + 8, prefix.end(), uint8_t(0));
    return fc::ip::address(fc::ip::ipv6_address(prefix));
  }

  bool accepts_exchanged_address(const fc::ip::address& address, const fc::ip::address& relayed_from)
  {
    const auto unmapped = [](const fc::ip::address& a) { return a.is_ipv4_mapped_ipv6() ? a.to_ipv4_address() : a; };
    const fc::ip::address exchanged = unmapped(address);
    const fc::ip::address from = unmapped(relayed_from);
    // dialing the unspecified address reaches this machine, as loopback does
    const fc::ip::address unspecified = exchanged.is_ipv4() ? fc::ip::address(uint32_t(0)) : fc::ip::address(fc::ip::ipv6_address());
    if (exchanged.is_multicast_address() || exchanged == unspecified)
      return false;
    if (exchanged.is_loopback_address())
      return from.is_loopback_address();
    if (exchanged.is_private_address())
      return !from.is_public_address();
    return true;
  }

  bool exceeds_connections_per_host(const fc::ip::address& address, const std::vector<fc::ip::address>& held_addresses,
                                    uint32_t maximum_connections_per_host)
  {
    const fc::ip::address host = connection_host_of(address);
    if (maximum_connections_per_host == 0)
      return false;
    const size_t held_to_host = std::count_if(held_addresses.begin(), held_addresses.end(),
                                              [&host](const fc::ip::address& held) { return connection_host_of(held) == host; });
    return held_to_host >= maximum_connections_per_host;
  }

  address_subnet address_subnet::from_string(const std::string& text)
  {
    const size_t slash = text.find('/');
    address_subnet subnet;
    subnet.network = fc::ip::address(fc::string(text.substr(0, slash)));
    if (subnet.network.is_ipv4_mapped_ipv6())
      subnet.network = subnet.network.to_ipv4_address();
    const uint32_t address_bits = subnet.network.is_ipv4() ? 32 : 128;
    uint32_t prefix_length = address_bits;
    if (slash != std::string::npos)
    {
      const std::string length_text = text.substr(slash + 1);
      FC_ASSERT(!length_text.empty() && length_text.size() <= 3 &&
                std::all_of(length_text.begin(), length_text.end(), [](char c) { return c >= '0' && c <= '9'; }),
                "invalid prefix length in subnet ${subnet}", ("subnet", text));
      prefix_length = std::stoul(length_text);
    }
    FC_ASSERT(prefix_length <= address_bits, "prefix length over ${bits} in subnet ${subnet}", ("bits", address_bits)("subnet", text));
    subnet.prefix_length = static_cast<uint8_t>(prefix_length);
    return subnet;
  }

  bool address_subnet::contains(const fc::ip::address& address) const
  {
    const fc::ip::address candidate = address.is_ipv4_mapped_ipv6() ? address.to_ipv4_address() : address;
    if (candidate.is_ipv4() != network.is_ipv4())
      return false;
    if (candidate.is_ipv4())
    {
      const uint32_t mask = prefix_length == 0 ? 0 : ~uint32_t(0) << (32 - prefix_length);
      return (candidate.get_ipv4().addr & mask) == (network.get_ipv4().addr & mask);
    }
    const fc::array<uint8_t, 16>& candidate_bytes = candidate.get_ipv6().addr;
    const fc::array<uint8_t, 16>& network_bytes = network.get_ipv6().addr;
    for (uint32_t bit = 0; bit < prefix_length; bit += 8)
    {
      const uint32_t bits_in_byte = std::min<uint32_t>(8, prefix_length - bit);
      const uint8_t mask = static_cast<uint8_t>(0xFF << (8 - bits_in_byte));
      if ((candidate_bytes.data[bit / 8] & mask) != (network_bytes.data[bit / 8] & mask))
        return false;
    }
    return true;
  }

  bool is_in_any_subnet(const fc::ip::address& address, const std::vector<address_subnet>& subnets)
  {
    return std::any_of(subnets.begin(), subnets.end(), [&address](const address_subnet& subnet) { return subnet.contains(address); });
  }

  bool inbound_connections_share_host(const fc::ip::address& refused_address,
                                      const std::vector<fc::ip::address>& inbound_addresses)
  {
    const fc::ip::address host = connection_host_of(refused_address);
    const size_t on_host = 1 + std::count_if(inbound_addresses.begin(), inbound_addresses.end(),
                                             [&host](const fc::ip::address& inbound) { return connection_host_of(inbound) == host; });
    return on_host * 2 > inbound_addresses.size() + 1;
  }

  namespace
  {
    /// The hosts (connection_host_of) of @p addresses, in order of first appearance, with how many of them each has
    std::vector<std::pair<fc::ip::address, uint32_t>> count_connections_per_host(const std::vector<fc::ip::address>& addresses)
    {
      std::vector<std::pair<fc::ip::address, uint32_t>> hosts;
      for (const fc::ip::address& address : addresses)
      {
        const fc::ip::address host = connection_host_of(address);
        const auto counted = std::find_if(hosts.begin(), hosts.end(),
                                          [&host](const std::pair<fc::ip::address, uint32_t>& h) { return h.first == host; });
        if (counted == hosts.end())
          hosts.emplace_back(host, 1);
        else
          ++counted->second;
      }
      return hosts;
    }
  }

  std::vector<fc::ip::endpoint> select_connection_candidates(const std::vector<connection_candidate>& candidates,
                                                             const std::vector<fc::ip::address>& held_addresses,
                                                             const connection_selection_limits& limits)
  {
    std::vector<fc::ip::address> addresses(held_addresses);
    std::vector<fc::ip::endpoint> selected;
    std::vector<bool> taken(candidates.size(), false);

    // picks up to `count` candidates of the family (IPv6 when `ipv6`) in database order
    const auto take = [&](bool ipv6, uint32_t count)
    {
      for (size_t i = 0; i < candidates.size() && count > 0 && selected.size() < limits.free_slots; ++i)
      {
        const connection_candidate& candidate = candidates[i];
        const fc::ip::address host = connection_host_of(candidate.endpoint.get_address());
        if (taken[i] || host.is_ipv6() != ipv6)
          continue;
        if (!candidate.exempt_from_host_limit &&
            exceeds_connections_per_host(host, addresses, limits.maximum_connections_per_host))
          continue;
        taken[i] = true;
        addresses.push_back(host);
        selected.push_back(candidate.endpoint);
        --count;
      }
    };

    const connection_spread held = get_connection_spread(held_addresses);
    const uint32_t reserved = limits.desired_connections * std::min<uint32_t>(limits.reserved_address_family_percent, 50) / 100;
    const uint32_t ipv6_shortfall = reserved > held.ipv6 ? reserved - held.ipv6 : 0;
    const uint32_t ipv4_shortfall = reserved > held.ipv4 ? reserved - held.ipv4 : 0;
    // with fewer free slots than both shortfalls, the family further below its share goes first
    const bool ipv6_first = ipv6_shortfall >= ipv4_shortfall;
    take(ipv6_first, ipv6_first ? ipv6_shortfall : ipv4_shortfall);
    take(!ipv6_first, ipv6_first ? ipv4_shortfall : ipv6_shortfall);
    take(true, limits.free_slots);
    take(false, limits.free_slots);

    if (limits.relax_connections_per_host && limits.maximum_connections_per_host != 0 && selected.size() < limits.free_slots)
    {
      // only the first candidates left out take part, so a host offering many ports costs a pass no more than a few
      std::vector<size_t> left_out;
      for (size_t i = 0; i < candidates.size() && left_out.size() < GRAPHENE_NET_MAX_RELAXED_CANDIDATES_PER_PASS; ++i)
        if (!taken[i])
          left_out.push_back(i);
      std::vector<fc::ip::address> available_addresses(addresses);
      for (size_t i : left_out)
        available_addresses.push_back(candidates[i].endpoint.get_address());
      std::vector<uint32_t> available_per_host;
      for (const std::pair<fc::ip::address, uint32_t>& host : count_connections_per_host(available_addresses))
        available_per_host.push_back(host.second);
      const uint32_t relaxed = get_relaxed_connections_per_host(available_per_host, limits.maximum_connections_per_host,
                                                                limits.desired_connections);
      std::map<fc::ip::address, size_t> held_per_host;
      for (const fc::ip::address& address : addresses)
        ++held_per_host[connection_host_of(address)];
      // every candidate left is on a host at the limit (an exempt one was taken above), so a host under the step holds
      // one less than it: each gets one connection per step
      for (uint32_t step = limits.maximum_connections_per_host + 1; step <= relaxed && selected.size() < limits.free_slots; ++step)
        while (selected.size() < limits.free_slots)
        {
          fc::optional<size_t> fewest;
          size_t fewest_held = 0;
          for (size_t i : left_out)
          {
            if (taken[i])
              continue;
            const size_t held = held_per_host[connection_host_of(candidates[i].endpoint.get_address())];
            if (held < step && (!fewest || held < fewest_held))
            {
              fewest = i;
              fewest_held = held;
            }
          }
          if (!fewest)
            break;
          taken[*fewest] = true;
          ++held_per_host[connection_host_of(candidates[*fewest].endpoint.get_address())];
          selected.push_back(candidates[*fewest].endpoint);
        }
    }
    return selected;
  }

  uint32_t get_relaxed_connections_per_host(const std::vector<uint32_t>& available_per_host, uint32_t maximum_connections_per_host,
                                            uint32_t desired_connections)
  {
    if (maximum_connections_per_host == 0 || available_per_host.empty())
      return maximum_connections_per_host;
    for (uint32_t step = maximum_connections_per_host; step <= desired_connections; ++step)
    {
      uint64_t reached = 0;
      for (uint32_t available : available_per_host)
        reached += std::min(available, step);
      if (reached >= desired_connections)
        return step;
    }
    const uint32_t hosts = static_cast<uint32_t>(available_per_host.size());
    return std::max(maximum_connections_per_host, (desired_connections + hosts - 1) / hosts);
  }

  connection_spread get_connection_spread(const std::vector<fc::ip::address>& addresses)
  {
    connection_spread spread;
    std::vector<fc::ip::address> hosts;
    for (const fc::ip::address& address : addresses)
    {
      const fc::ip::address host = connection_host_of(address);
      (host.is_ipv6() ? spread.ipv6 : spread.ipv4)++;
      if (std::find(hosts.begin(), hosts.end(), host) == hosts.end())
        hosts.push_back(host);
    }
    spread.hosts = static_cast<uint32_t>(hosts.size());
    return spread;
  }

  fc::optional<size_t> select_connection_over_host_limit(const std::vector<host_limited_connection>& connections,
                                                         uint32_t maximum_connections_per_host)
  {
    fc::optional<size_t> selected;
    if (maximum_connections_per_host == 0)
      return selected;
    size_t most_held = maximum_connections_per_host;
    for (size_t i = 0; i < connections.size(); ++i)
    {
      if (connections[i].exempt)
        continue;
      const fc::ip::address host = connection_host_of(connections[i].address);
      const size_t held = std::count_if(connections.begin(), connections.end(),
                                        [&host](const host_limited_connection& c) { return connection_host_of(c.address) == host; });
      // >= keeps the last one of the host with the most
      if (held > maximum_connections_per_host && held >= most_held &&
          (!selected || held > most_held || connection_host_of(connections[*selected].address) == host))
      {
        selected = i;
        most_held = held;
      }
    }
    return selected;
  }

  std::vector<size_t> select_idle_peers_to_disconnect(const std::vector<idle_peer_candidate>& peers,
                                                      const idle_peer_sweep& sweep)
  {
    std::vector<size_t> idle;
    if (sweep.window.count() <= 0)
      return idle;
    for (size_t i = 0; i < peers.size(); ++i)
      if (!peers[i].exempt && sweep.now - peers[i].last_useful_data_time > sweep.window)
        idle.push_back(i);
    std::stable_sort(idle.begin(), idle.end(), [&](size_t a, size_t b) {
      return peers[a].last_useful_data_time < peers[b].last_useful_data_time;
    });
    const size_t most = peers.size() > sweep.kept_at_least ? peers.size() - sweep.kept_at_least : 0;
    if (idle.size() > most)
      idle.resize(most);
    return idle;
  }

  handshaking_connection_verdict judge_handshaking_connection(const handshaking_connection_state& connection, fc::time_point now,
                                                             fc::microseconds inactivity_timeout, fc::microseconds deadline)
  {
    if (connection.initiated < now - deadline)
      return handshaking_connection_verdict::past_deadline;
    const fc::time_point inactive_before = now - inactivity_timeout;
    if (connection.initiated < inactive_before && connection.last_message_received < inactive_before &&
        connection.last_message_sent < inactive_before)
      return handshaking_connection_verdict::inactive;
    return handshaking_connection_verdict::keep;
  }

  namespace
  {
    // the disconnect reasons for an active connection, one each, in the order judge_active_connection() ranks them

    bool is_inactive(const active_connection_state& connection, const active_connection_limits& limits)
    {
      const fc::time_point inactive_before = limits.now - limits.inactivity_timeout;
      return connection.initiated < inactive_before && connection.last_message_received < inactive_before;
    }

    bool ignores_sync_item_requests(const active_connection_state& connection, const active_connection_limits& limits)
    {
      return connection.last_sync_item_received && *connection.last_sync_item_received < limits.now - limits.request_timeout;
    }

    bool ignores_sync_item_ids_request(const active_connection_state& connection, const active_connection_limits& limits)
    {
      return connection.sync_item_ids_requested && *connection.sync_item_ids_requested < limits.now - limits.request_timeout;
    }

    bool ignores_item_request(const active_connection_state& connection, const active_connection_limits&)
    {
      return connection.ignored_item.has_value();
    }

    bool stalls_sending(const active_connection_state& connection, const active_connection_limits& limits)
    {
      return connection.send_progress < limits.now - limits.send_stall_timeout;
    }

    // the last two apply only to a connection older than the keepalive timeout
    bool is_quiet(const active_connection_state& connection, const active_connection_limits& limits)
    {
      const fc::time_point quiet_before = limits.now - limits.keepalive_timeout;
      return connection.initiated < quiet_before && connection.last_message_received < quiet_before;
    }

    bool stalls_sync(const active_connection_state& connection, const active_connection_limits& limits)
    {
      return connection.initiated < limits.now - limits.keepalive_timeout && connection.sync_idle;
    }
  }

  active_connection_verdict judge_active_connection(const active_connection_state& connection,
                                                    const active_connection_limits& limits)
  {
    using rule = bool (*)(const active_connection_state&, const active_connection_limits&);
    static const std::pair<rule, active_connection_verdict> ranked_rules[] = {
      {is_inactive, active_connection_verdict::inactive},
      {ignores_sync_item_requests, active_connection_verdict::sync_items_ignored},
      {ignores_sync_item_ids_request, active_connection_verdict::sync_item_ids_ignored},
      {ignores_item_request, active_connection_verdict::item_request_ignored},
      {stalls_sending, active_connection_verdict::send_stalled},
      {is_quiet, active_connection_verdict::send_keepalive},
      {stalls_sync, active_connection_verdict::sync_stalled}};
    for (const auto& [applies, verdict] : ranked_rules)
      if (applies(connection, limits))
        return verdict;
    return active_connection_verdict::keep;
  }

  bool breaker_stall_override_due(uint32_t head_block_number, fc::time_point last_head_advance, fc::time_point now)
  {
    return head_block_number > 0 && last_head_advance != fc::time_point() &&
           now - last_head_advance > fc::seconds(GRAPHENE_NET_BREAKER_STALL_OVERRIDE_SEC);
  }

  size_t transaction_fetch_window(bool inhibited, uint32_t in_flight, uint32_t in_flight_cap, size_t window_room)
  {
    if (inhibited || (in_flight_cap != 0 && in_flight >= in_flight_cap))
      return 0;
    return window_room;
  }

  std::vector<std::pair<uint32_t, std::vector<item_hash_t>>> group_fetch_requests(const std::vector<item_id>& items,
                                                                                  size_t max_transactions_per_request)
  {
    std::vector<std::pair<uint32_t, std::vector<item_hash_t>>> requests;
    for (const item_id& item : items)
    {
      if (requests.empty() || requests.back().first != item.item_type ||
          (item.item_type == core_message_type_enum::trx_message_type &&
           requests.back().second.size() >= max_transactions_per_request))
        requests.emplace_back(item.item_type, std::vector<item_hash_t>());
      requests.back().second.push_back(item.item_hash);
    }
    return requests;
  }

  fetch_loop_wait next_fetch_pass(const transaction_fetch_gate& gate, fc::time_point pass_start,
                                  fc::microseconds configured_minimum_interval, fc::time_point next_peer_unblocked,
                                  fc::time_point next_minimum_due)
  {
    fetch_loop_wait wait{configured_minimum_interval, next_peer_unblocked};
    if (gate.held_back() || gate.bounded())
      wait.wake_time = std::min(wait.wake_time, next_minimum_due);
    if (gate.closed())
      wait.minimum_interval = std::max(wait.minimum_interval, next_minimum_due - pass_start);
    return wait;
  }

} } // end namespace graphene::net
