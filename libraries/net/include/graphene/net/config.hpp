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

#define GRAPHENE_NET_PROTOCOL_VERSION                              107
#define GRAPHENE_NET_PROTOCOL_COMPRESSED_BLOCKS_VERSION            107 // support for compressed blocks added in 107
#define GRAPHENE_NET_PROTOCOL_ADVERTISE_BLOCKS_BY_BLOCK_ID_VERSION 107 // share blocks by block_id instead of by block_message hash added in 107
#define GRAPHENE_NET_PROTOCOL_FIREWALL_CHECK_VERSION               106 // support for the firewall check was added in 106

// the minimum protocol version we accept a connection from.  Version 106 shipped many years ago
// (steem era); every known client since has reported its protocol version, chain id, and last
// known hardfork block, so requiring them costs nothing and closes the loophole where a peer
// reporting none of them silently bypassed every compatibility check
#define GRAPHENE_NET_MINIMUM_PROTOCOL_VERSION                      106

// circuit breaker for invalid-block punitive disconnects: if we find ourselves kicking more
// than COUNT peers within WINDOW seconds for offering "invalid" blocks, the far more likely
// explanation is a systemic event (fork storm, local latency, a bug of ours) than that many
// simultaneous attackers -- stop disconnecting and fall back to reconciliation until the
// window drains.  The 2026-07-09 incident was exactly this signature: hundreds of honest
// peers punished in minutes, every rejection a false positive.
#define GRAPHENE_NET_INVALID_BLOCK_DISCONNECT_BREAKER_COUNT        5
#define GRAPHENE_NET_INVALID_BLOCK_DISCONNECT_BREAKER_WINDOW_SEC   60

// a separate breaker with the same semantics for disconnects over a block whose body doesn't
// match its header's merkle root.  Such a copy says nothing about the block itself, only about
// the peer that sent it, so it doesn't count toward the breaker above; and a burst of them
// (which could be a local decoding bug) stops these disconnects instead of isolating the node
#define GRAPHENE_NET_BLOCK_BODY_MISMATCH_DISCONNECT_BREAKER_COUNT        5
#define GRAPHENE_NET_BLOCK_BODY_MISMATCH_DISCONNECT_BREAKER_WINDOW_SEC   60

// how far past our current head an advertised block id may claim to be and still be worth
// fetching.  Legitimate live adverts are at most a block or two ahead; this allows for
// generous clock skew and momentary head lag while rejecting fabricated far-future ids
#define GRAPHENE_NET_FUTURE_BLOCK_IDS_GRACE_BLOCKS                 20

// how far a peer's head may lag behind the oldest block we still have (our tail block, above 1
// only when we prune our block log) and still be treated as a peer we can serve.  A node that
// keeps no block log (block-log-split = 0) has its last irreversible block as the tail, and with
// one-block irreversibility that is its head block or close to it, so peers in live sync are
// often momentarily behind it; they get the missing blocks through normal block propagation.
// Peers further behind are rejected at the handshake or disconnected with an explanation,
// instead of being sent block ids we don't have
#define GRAPHENE_NET_PRUNED_TAIL_GRACE_BLOCKS                      20

// if the invalid-block disconnect breaker has been keeping peers whose blocks we rejected,
// but our head hasn't advanced for this long, the breaker's systemic-blip assumption is
// falsified -- those peers may be all that's occupying our slots while none of them can
// feed us the chain.  Revoke the deferral and disconnect them so the connect loop can
// find replacements
#define GRAPHENE_NET_BREAKER_STALL_OVERRIDE_SEC                    60

// how long the connect loop trusts a route-lookup result for a peer address before probing
// again, so that interface/route changes (link coming up, a default route appearing) are
// picked up without re-probing every candidate on every pass
#define GRAPHENE_NET_ROUTE_PROBE_CACHE_SEC                         60

// the far-future filter above assumes head lag is momentary.  If our head has not advanced
// for this long while an established peer keeps advertising blocks past the grace window,
// the likelier explanation is that we fell behind (e.g. the chain thread stalled long enough
// for the network to get more than GRAPHENE_NET_FUTURE_BLOCK_IDS_GRACE_BLOCKS ahead) than
// that the peer is fabricating ids.  Since adverts are dropped before they are recorded and
// sync otherwise only restarts on a new connection, such a node would stay behind until its
// next new peer.  Instead we restart sync with the advertising peer, at most once per this
// interval per peer.  Also the minimum spacing between such restarts for one peer.
#define GRAPHENE_NET_STALLED_HEAD_RESYNC_SEC                       60

/**
 * Define this to enable debugging code in the p2p network interface.
 * This is code that would never be executed in normal operation, but is
 * used for automated testing (creating artificial net splits,
 * tracking where messages came from and when)
 */
#define ENABLE_P2P_DEBUGGING_API                             1

/**
 * 2MiB
 */
#define MAX_MESSAGE_SIZE                                     (1024*1024*2 + 20) //20 for block_type_id size
#define GRAPHENE_NET_DEFAULT_PEER_CONNECTION_RETRY_TIME      30 // seconds

/**
 * The connect loop runs again when it is triggered (a peer was added or learned, a connection closed or
 * failed), but at most once per GRAPHENE_NET_CONNECT_LOOP_MINIMUM_INTERVAL_MS: a peer that accepts the
 * handshake and drops us, or whose address pair is still held by our previous connection, is due again
 * at once and triggers the loop each time, so the interval bounds how often such a peer is redialed.
 * Without a trigger it runs again GRAPHENE_NET_CONNECT_LOOP_MAXIMUM_WAIT_SEC after an iteration, for
 * state changes that do not trigger it (a peer's retry delay expiring).
 */
#define GRAPHENE_NET_CONNECT_LOOP_MINIMUM_INTERVAL_MS        1000
#define GRAPHENE_NET_CONNECT_LOOP_MAXIMUM_WAIT_SEC           10

#define GRAPHENE_NET_PEER_HANDSHAKE_INACTIVITY_TIMEOUT       5

#define GRAPHENE_NET_PEER_DISCONNECT_TIMEOUT                 20

#define GRAPHENE_NET_TEST_SEED_IP                            "104.236.44.210" // autogenerated
#define GRAPHENE_NET_TEST_P2P_PORT                           1700
#define GRAPHENE_NET_DEFAULT_P2P_PORT                        1776
#define GRAPHENE_NET_DEFAULT_DESIRED_CONNECTIONS             20
#define GRAPHENE_NET_DEFAULT_MAX_CONNECTIONS                 200

#define GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES        (1024 * 1024)
/**
 * The transactions a peer asks for are queued for it only while its send queue holds at most this many bytes;
 * the rest of the reply waits outside the queue and is queued, in order, as the queue drains.  However large
 * the transactions it asked for turn out to be, a reply never takes the queue over
 * GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES, and the other half stays free for blocks and other traffic.
 */
#define GRAPHENE_NET_MAXIMUM_QUEUED_TRANSACTION_REPLY_BYTES  (GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES / 2)
/**
 * How many requested transactions may wait for room in one peer's send queue
 * (see GRAPHENE_NET_MAXIMUM_QUEUED_TRANSACTION_REPLY_BYTES); a peer asking for more than that while they wait
 * is told the rest are not available.  Twice what we ourselves keep outstanding with a peer
 * (GRAPHENE_NET_MAX_TRANSACTIONS_IN_FLIGHT_PER_PEER), so a peer requesting as we do never gets there.
 */
#define GRAPHENE_NET_MAX_DEFERRED_TRANSACTION_REPLIES_PER_PEER (2 * GRAPHENE_NET_MAX_TRANSACTIONS_IN_FLIGHT_PER_PEER)
/**
 * How many bytes of requested transactions may wait for room in one peer's send queue, as
 * GRAPHENE_NET_MAX_DEFERRED_TRANSACTION_REPLIES_PER_PEER bounds their count: past it, further transactions the peer
 * asks for are not available.  Our own full item window after a jump to 8 KB transactions (6.4 MB) still waits in
 * full, while a peer that stops reading holds this much at most rather than its count of the largest transactions.
 */
#define GRAPHENE_NET_MAX_DEFERRED_TRANSACTION_REPLY_BYTES_PER_PEER (8 * 1024 * 1024)
/**
 * A block a peer asks for is queued for sending as its id, and looked up only once it is one of
 * this many messages at the front of the peer's send queue: the lookups (and the compression the
 * worker threads may do for that peer) run ahead of the message being sent, while the blocks held
 * for one peer stay this few however many it asks for
 */
#define GRAPHENE_NET_BLOCK_MESSAGES_PREPARED_AHEAD           4

/**
 * A peer's queued messages are written to its socket together until the batch reaches this many
 * bytes; the message that crosses the bound is still part of the batch.
 */
#define GRAPHENE_NET_MAXIMUM_BATCHED_SEND_BYTES              (64 * 1024)

/**
 * When we receive a message from the network, we advertise it to
 * our peers and save a copy in a cache were we will find it if
 * a peer requests it.  We expire out old items out of the cache
 * after this number of blocks go by.
 *
 * Recently lowered from 30 to match the default expiration time
 * the web wallet imposes on transactions.
 */
#define GRAPHENE_NET_MESSAGE_CACHE_DURATION_IN_BLOCKS        20

/**
 * Number of transactions the transaction message cache's message hash index is sized for
 * when the node starts, so it does not rehash on the p2p thread while the cache fills.
 * It covers GRAPHENE_NET_MESSAGE_CACHE_DURATION_IN_BLOCKS blocks of about 13k
 * transactions each (over 4k tx/s); a busier network still works, the index just
 * grows once more.  Costs one bucket pointer per item, 2 MB.
 */
#define GRAPHENE_NET_TRANSACTION_MESSAGE_CACHE_EXPECTED_SIZE (256 * 1024)

/**
 * We prevent a peer from offering us a list of blocks which, if we fetched them
 * all, would result in a blockchain that extended into the future.
 * This parameter gives us some wiggle room, allowing a peer to give us blocks
 * that would put our blockchain up to an hour in the future, just in case
 * our clock is a bit off.
 */
#define GRAPHENE_NET_FUTURE_SYNC_BLOCKS_GRACE_PERIOD_SEC     (60 * 60)

/* recently lowered from 2 minutes -> 1 minute, to match the cache
 * duration/default wallet expiration time */
#define GRAPHENE_NET_MAX_INVENTORY_SIZE_IN_MINUTES           1

#define GRAPHENE_NET_MAX_BLOCKS_PER_PEER_DURING_SYNCING      200

/**
 * How often (at most) we log, per peer, that advertisements were deferred or
 * dropped because the peer exceeded our inventory limit
 */
#define GRAPHENE_NET_INVENTORY_LIMIT_REPORT_INTERVAL_SEC     10

/**
 * How often the node writes its p2p statistics (fetch scheduling, transaction handoff, send batching,
 * request sizing) to the GRAPHENE_NET_STATISTICS_LOGGER logger, when that logger is enabled for info
 */
#define GRAPHENE_NET_STATISTICS_REPORT_INTERVAL_SEC          30
#define GRAPHENE_NET_STATISTICS_LOGGER                       "p2p_stats"

/**
 * One in this many transactions, chosen by message hash so every node picks the same ones, is logged with
 * a real-clock timestamp when received and when broadcast, to the GRAPHENE_NET_RELAY_SAMPLE_LOGGER logger
 * when that logger is enabled for info
 */
#define GRAPHENE_NET_RELAY_SAMPLE_ONE_IN                     64
#define GRAPHENE_NET_RELAY_SAMPLE_LOGGER                     "p2p_relay_sample"

/**
 * During normal operation, how many blocks will be fetched from each
 * peer at a time.  A request that reaches this many blocks asks for
 * no transactions.
 */
#define GRAPHENE_NET_MAX_BLOCKS_PER_PEER_DURING_NORMAL_OPERATION  100

/**
 * During normal operation, how many transactions one request will fetch
 * from a peer (GRAPHENE_NET_MAX_TRANSACTIONS_IN_FLIGHT_PER_PEER bounds all
 * those outstanding with it).  This will only come into play when the network
 * is being flooded -- typically transactions will be fetched as soon
 * as we find out about them, so only one item will be requested
 * at a time.  GRAPHENE_NET_MAX_TRANSACTION_BYTES_PER_FETCH lowers it for
 * a peer sending large transactions.
 */
#define GRAPHENE_NET_MAX_TRANSACTIONS_PER_PEER_DURING_NORMAL_OPERATION  400

/**
 * How many transactions may be outstanding with one peer during normal operation, over all the requests
 * in flight with it.  The next request may go out before earlier replies have fully arrived, so a
 * high-latency link carries up to this many transactions per round trip rather than one request's worth.
 * It also bounds what a stalled peer holds back until its requests time out or it disconnects.
 */
#define GRAPHENE_NET_MAX_TRANSACTIONS_IN_FLIGHT_PER_PEER  (2 * GRAPHENE_NET_MAX_TRANSACTIONS_PER_PEER_DURING_NORMAL_OPERATION)

/**
 * How many bytes of transactions we expect all the requests outstanding with one peer to bring back, at
 * most, unless one full request (see GRAPHENE_NET_MAX_TRANSACTION_BYTES_PER_FETCH) expects more.  The
 * expectation is the peer's mean transaction size, which learns of larger transactions only as their replies
 * arrive, so the peer may owe us several times this after a jump in transaction size.  The peer does not drop
 * us for it: it queues our replies only up to GRAPHENE_NET_MAXIMUM_QUEUED_TRANSACTION_REPLY_BYTES and the rest
 * as its queue drains (older peers drop us when their queue passes GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES,
 * which no window based on the mean can rule out).  So this only bounds what a slow or stalled peer holds back;
 * it lets a mix of transactions of a few hundred bytes fill the whole item window
 * (GRAPHENE_NET_MAX_TRANSACTIONS_IN_FLIGHT_PER_PEER), two full requests per round trip.
 * Applies only to a peer that says in its hello that it defers replies so (GRAPHENE_NET_HELLO_DEFERS_TRANSACTION_REPLIES):
 * an older peer keeps no more than one full request outstanding, as before pipelining, so a jump in transaction size
 * takes its queue over the limit no sooner than it did then.
 */
#define GRAPHENE_NET_MAX_TRANSACTION_BYTES_IN_FLIGHT_PER_PEER  (512 * 1024)

/**
 * While the pending pool holds more than a third of max-mempool-size (graphene::net::is_transaction_fetching_gated())
 * the node requests no transactions, and checks again this often whether the pool has drained below that while
 * transactions wait to be fetched.  The pool drains when a block is applied, so a fraction of the block interval
 * is enough.
 */
#define GRAPHENE_NET_TRANSACTION_FETCH_GATE_RECHECK_MS  100

/**
 * The key of the hello's user_data by which a node says it queues the transactions a peer asks for only as its send
 * queue drains (peer_connection::send_transaction_reply()), so a peer may keep several requests outstanding with it
 * (GRAPHENE_NET_MAX_TRANSACTION_BYTES_IN_FLIGHT_PER_PEER).  Older nodes ignore keys they don't know.
 */
#define GRAPHENE_NET_HELLO_DEFERS_TRANSACTION_REPLIES        "defers_transaction_replies"

/**
 * A requested transaction times out (and the peer is dropped) once active_ignored_request_timeout_microseconds have
 * passed both since we asked for it and since the peer last delivered one we asked for, so a peer delivering a large
 * window over a slow link keeps it, but one answering that it doesn't have them does not.  A block times out from its
 * request, whatever the peer delivers meanwhile.  However steadily the peer delivers others, a transaction still
 * outstanding this many timeouts after we asked for it times out too.  With the default 6 s timeout that is 18 s,
 * well within GRAPHENE_NET_MAX_INVENTORY_SIZE_IN_MINUTES, so the peer is dropped while the records of who else
 * advertised what it held are still there to ask them.  18 s receives what a peer may owe us at the estimated
 * transaction size (its queued reply budget and our byte window, 1 MiB) at about 0.5 Mbit/s, and the most it may
 * defer after a jump in size (GRAPHENE_NET_MAX_DEFERRED_TRANSACTION_REPLY_BYTES_PER_PEER) at about 4 Mbit/s; a
 * slower link is dropped and what it held is fetched from the others.
 */
#define GRAPHENE_NET_MAX_TRANSACTION_REQUEST_TIMEOUTS_WHILE_DELIVERING 3

/**
 * How many bytes of transactions we expect a single transaction request to bring back, at most.
 * The peer queues its whole reply at once and drops us when its queue passes
 * GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES, so this leaves it room for other traffic and for
 * transactions larger than the peer's recent mean.
 */
#define GRAPHENE_NET_MAX_TRANSACTION_BYTES_PER_FETCH         (512 * 1024)

/**
 * The transaction size assumed for a peer that has not sent us a transaction yet: the largest
 * transaction, so the first requests stay within GRAPHENE_NET_MAX_TRANSACTION_BYTES_PER_FETCH whatever the
 * peer sends.  Needs hive/protocol/config.hpp where it is used.
 */
#define GRAPHENE_NET_INITIAL_TRANSACTION_SIZE_ESTIMATE       HIVE_MAX_TRANSACTION_SIZE

/**
 * Instead of fetching all item IDs from a peer, then fetching all blocks
 * from a peer, we will interleave them.  Fetch at least this many block IDs,
 * then switch into block-fetching mode until the number of blocks we know about
 * but haven't yet fetched drops below this
 */
#define GRAPHENE_NET_MIN_BLOCK_IDS_TO_PREFETCH               20000

#define GRAPHENE_NET_MAX_NUMBER_OF_BLOCKS_TO_HANDLE_AT_ONE_TIME 200
#define GRAPHENE_NET_MAX_NUMBER_OF_BLOCKS_TO_PREFETCH           (100* GRAPHENE_NET_MAX_NUMBER_OF_BLOCKS_TO_HANDLE_AT_ONE_TIME)

#define GRAPHENE_NET_MAX_TRX_PER_SECOND                      1000

/**
 * Set the ignored request time out to 1 second.  When we request a block
 * or transaction from a peer, this timeout determines how long we wait for them
 * to reply before we give up and ask another peer for the item.
 * Ideally this should be significantly shorter than the block interval, because
 * we'd like to realize the block isn't coming and fetch it from a different
 * peer before the next block comes in.  At the current target of 3 second blocks,
 * 1 second seems reasonable.  When we get closer to our eventual target of 1 second
 * blocks, this will need to be re-evaluated (i.e., can we set the timeout to 500ms
 * and still handle normal network & processing delays without excessive disconnects)
 */
#define GRAPHENE_NET_ACTIVE_IGNORED_REQUEST_TIMEOUT_SECONDS    6

#define GRAPHENE_NET_FAILED_TERMINATE_TIMEOUT_SECONDS          120

#define GRAPHENE_NET_PRUNE_FAILED_IDS_MINUTES                  15

#define GRAPHENE_NET_FETCH_UPDATED_PEER_LISTS_INTERVAL_MINUTES 15

#define GRAPHENE_NET_BANDWIDTH_MONITOR_INTERVAL_SECONDS        1

#define GRAPHENE_NET_DUMP_NODE_STATUS_INTERVAL_MINUTES         1

#define GRAPHENE_NET_FIREWALL_CHECK_MESSAGE_INTERVAL_MINUTES   5

#define GRAPHENE_NET_PORT_WAIT_DELAY_SECONDS                   5

#define GRAPHENE_NET_MAX_PEERDB_SIZE                           1000
