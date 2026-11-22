# libraries/net: the p2p layer

This document describes how the p2p layer moves transactions and blocks, which limits bound it, in what order it
disconnects peers, and which options an operator can set. It describes limits and their purpose; it is not a record
of the attacks some of them answer. Each limit is defined in `include/graphene/net/config.hpp`, grouped by the
subsystem that owns it, and each `node_configuration` option in `include/graphene/net/node_configuration.hpp`.
Keep this document, `config.hpp` and the code in step: a new limit or option gets a row in the tables below.

## Files

| File | What it holds |
|---|---|
| `node.cpp` | `node_impl`: the loops (connect, fetch items, fetch sync items, advertise inventory, terminate inactive connections), message handlers, and the policy functions and classes declared in `node.hpp` |
| `peer_connection.cpp` | one connection: send queue, held-back replies and inventory, what was requested from the peer and advertised by it |
| `message_oriented_connection.cpp`, `stcp_socket.cpp` | framing and the encrypted socket |
| `peer_database.cpp` | known peer addresses and their last connection outcome |
| `core_messages.cpp` | message types |

All of `node_impl` and `peer_connection` runs on the p2p thread (fc cooperative tasks). Transaction validation runs
on the chain's threads and reports back through a completion queue; blocks are pushed to the chain through the
delegate.

## Transaction and block paths

```
 Peer A (sender)                                         Node B (receiver)
 ---------------                                         -----------------
 new transaction validated
   |
   v
 advertise_inventory_loop
   |  ids per peer
   v
 send_transaction_inventory --(queue <= budget)--> [send queue] ==item_ids_inventory==> on_item_ids_inventory_message
   |  else held back (ids/age bounded)                                                     |  record per peer (inventory window)
   |                                                                                        v
   |                                                                          _items_to_fetch + peer fetch candidates
   |                                                                                        |
   |                                                                                        v
   |                                                                fetch_items_loop pass:
   |                                                                  gate (pending pool, bytes on their way, minimum)
   |                                                                  window (per-peer items/bytes, in-flight cap)
   |                                                                  turns (one item per peer per turn)
   |                                                                  judge candidate (block source ranking)
   |                                                                                        |
 on_fetch_items_message <==================== fetch_items ==================================+
   |  transactions: send_transaction_reply
   |    queued while queue <= budget, else held back (count/bytes bounded; busy answer past that)
   |  blocks: send_block_message (placeholder, built near the front, ahead of transaction traffic)
   v
 [send queue] --batched writes--> socket ==trx / block==> process_trx_message --> submit to chain (never waits)
                                                          process_block_message --> push block to chain
                                                                  |
                                                                  v
                                            validated: cache, advertise to our peers (the path repeats)
```

Blocks are also advertised by id and fetched the same way; a block is asked of an idle, proven advertiser first and
re-asked of another advertiser when its request passes its deadline (`rerequest_overdue_blocks()`). During sync,
`fetch_sync_items_loop` requests ranges of blocks by id from peers ahead of us instead.

## What is held back, and where

| Structure | Side | Bound | Age bound |
|---|---|---|---|
| inventory to advertise, per peer (`peer_connection::_deferred_transaction_inventory`) | sender | `MAX_DEFERRED_TRANSACTION_INVENTORY_IDS_PER_PEER` | `MAX_DEFERRED_TRANSACTION_INVENTORY_AGE_MS` |
| replies to send, per peer (`peer_connection::_deferred_transaction_replies`) | sender | `MAX_DEFERRED_TRANSACTION_REPLIES_PER_PEER`, `MAX_DEFERRED_TRANSACTION_REPLY_BYTES_PER_PEER` | the slow-reader rule (60 s without a write) |
| inventory advertised to us that we couldn't record yet (`transaction_inventory_deferred`) | receiver | `MAX_TRANSACTION_INVENTORY_PER_PEER` | `MAX_INVENTORY_SIZE_IN_MINUTES` |
| fetch candidates, per peer (`fetch_candidate_queue`) | receiver | what the peer advertised | the message cache duration |
| items to fetch, node-wide (`_items_to_fetch`) | receiver | what peers advertised | the message cache duration |
| the gated minimum (`fetch_minimum_schedule`) | receiver | `MIN_TRANSACTIONS_PER_GATED_FETCH` | per `TRANSACTION_FETCH_GATE_RECHECK_MS` |

## The transaction budget

`GRAPHENE_NET_TRANSACTION_BUDGET_BYTES` (512 KiB, half of the 1 MiB send queue limit) is the one value behind four
limits: the replies and the inventory a sender queues for a peer, and the bytes one of our requests, and all our
requests to a peer, may expect back. A queued block counts only its small placeholder against the send queue: it is
built when it nears the front, so block bytes never use the queue's limit and need no share of it. The other half of
the queue is headroom for the messages built when queued (sync item id inventories, address lists, requests), for
the one transaction a reply may queue past the budget, and for older peers that queue a whole reply at once.

## Transactions in flight

`maximum_transactions_in_flight` (default 10,000) is a safety cap on the transactions received from peers and not
yet validated. A transaction we asked for is always submitted for validation at once, so the peer's read loop never
waits and its blocks are read as they arrive. At the cap the fetch passes request no more transactions
(`transaction_fetch_window()`) until validation results bring the count back under it; only replies already
requested still arrive, so the count exceeds the cap by at most what the peers' windows held. Normally the fetch
gate's headroom (which counts the bytes being validated) keeps the count far below the cap; the cap matters when the
gate is disabled (`max-mempool-size` 0) or validation stops while the gated minimum keeps fetching. The statistics
report counts the times the cap was reached and for how long (`handoff.limit_waits`, `handoff.limit_wait_us`).

## Disconnect reasons, ranked

`terminate_inactive_connections_loop()` runs every second and acts in this order:

1. **Breaker override** (stall rule 1): peers kept by the invalid-block circuit breaker are disconnected once our
   head has not advanced for `BREAKER_STALL_OVERRIDE_SEC` (`breaker_stall_override_due()`).
2. **Handshaking connections** (`judge_handshaking_connection()`): past `HANDSHAKE_DEADLINE_SEC`, else neither side
   sent anything for `peer_inactivity_timeout`.
3. **Active connections** (`judge_active_connection()`), the first that holds:
   1. inactive: no message from the peer for ten block intervals (disconnected with a reason)
   2. sync items ignored: no progress on requested sync blocks for `active_ignored_request_timeout`
   3. sync item ids ignored: no answer to a sync item id request for `active_ignored_request_timeout`
   4. item request ignored: a requested block or transaction overdue (`requested_items::timed_out()`)
   5. send stalled, the slow-reader rule: something to send and nothing written for `SEND_STALL_DISCONNECT_SEC`
      (held-back replies and inventory count, since one of them is queued whenever the queue empties)
   6. keepalive (not a disconnect): no message for half the inactivity timeout, so we ask for the time
   7. sync stalled: we need sync blocks from the peer and nothing from it is processed, requested or queued
4. **Closing connections** that didn't close within `PEER_DISCONNECT_TIMEOUT`, and terminated ones that didn't finish
   within `FAILED_TERMINATE_TIMEOUT_SECONDS`.
5. **Idle replacement** (stall rule 3, off by default, only while our head is behind), **silent-peer replacement**
   (stall rule 4, only while our head is not behind) and then the **sync restart** (stall rule 2), after the
   disconnects above. Silent-peer replacement drops a peer that for `silent_peer_disconnect_timeout` (30 min) relayed
   us no block and no block or transaction inventory and asked us for no block ids and no items, only while a
   candidate can take its slot and never below the idle floor (`IDLE_PEERS_KEPT_AT_LEAST`, or half the desired
   connections); keepalives and addresses don't count, requests do, so a peer syncing from us or downstream of us is
   kept. Like an idle one, a replaced silent peer may not take a slot again for one window.

Other disconnects happen where the cause is seen, not in this loop: the send queue passing its 1 MiB limit closes
the connection at once; withholding strikes (`MAX_UNAVAILABLE_ITEM_STRIKES`), invalid blocks and blocks whose body
doesn't match their header (each behind its breaker), unrequested items, oversized inventory or requests, and the
per-host limit at hello.

The first three stall rules share a 60 s interval but don't overlap: the breaker override only drops peers flagged by
the breaker; the sync restart only asks the peers we keep; idle replacement only runs when enabled, with a window that
must be longer than the restart's so the restart is tried first. Silent-peer replacement and idle replacement split on
the same 60 s head age (`IDLE_PEERS_HEAD_BEHIND_SEC`), so at most one of them applies at a time.

The connect loop doesn't dial an address whose node is connected: the node id that answers our hello at each address
we dial is remembered (for as many addresses as the peer database holds), and while that node is connected, active or
handshaking, over another address, the address is skipped. So a node reachable over IPv4 and IPv6 is dialed at its
other address at most once while it stays connected, instead of every `peer_connection_retry_timeout` (an
already_connected rejection is no failure and grows no backoff); once its connection is gone the address is dialed as
before.

## Idle replacement and the per-host cap

Idle replacement (`idle_peer_disconnect_timeout`) stays off by default. Live checks of the feature were problematic,
so it remains an operator opt-in (180 enables it) and is not part of the default behaviour. A value saved in
`node_config.json` is ignored.

The per-host cap (`maximum_connections_per_host`, default 2) limits the connections held to one IPv4 address or IPv6
/64. Whether it gives way is the explicit switch `relax_per_host_limit` (`p2p-relax-per-host-limit`):

* relaxed (the default): `host_limit_relax_delay` seconds after start, the cap gives way one step at a time to peers
  we dial while peers on other hosts can't fill the desired connections, and to peers dialing us while we hold fewer
  than desired, up to their host's even share and half of the desired connections at most; connections past the
  cap give way to peers on new hosts.
* hard: the cap holds whatever it costs.

Setting `maximum_connections_per_host` (option or `p2p-parameters`) without `relax_per_host_limit` makes the cap
hard, as before the switch existed; set both to get a relaxed cap of another size.

For operators running several nodes behind one address (one machine, a NAT or a shared /64): other nodes count them
as one host, so with the default cap at most two of them connect to any one outside node while that node's other
hosts fill its slots. Nodes of one operator that should connect to each other freely list each other's addresses in
`p2p-host-limit-exempt` (loopback is exempt by default) or are dialed as seeds or add-node targets, which the cap
doesn't refuse. A node behind a proxy that rewrites inbound source addresses sees every inbound peer on the proxy's
host and must exempt the proxy's address or set the cap to 0.

## Operator options

| hived option | `node_configuration` field (`p2p-parameters`) | Default | Unit | Meaning |
|---|---|---|---|---|
| `p2p-endpoint` | `listen_endpoint` | none | address | where to accept connections |
| `p2p-max-connections` | `maximum_number_of_connections` | 200 | connections | refuse inbound connections past it |
| - | `desired_number_of_connections` | 20 | connections | dial until this many |
| `p2p-seed-node` | - | built-in list | address | peers to dial at start; not limited by the per-host cap |
| `p2p-max-connections-per-host` | `maximum_connections_per_host` | 2 | connections | per-host cap; 0 for none |
| `p2p-relax-per-host-limit` | `relax_per_host_limit` | true (false when the cap is set without it) | bool | whether the cap gives way |
| `p2p-host-limit-exempt` | `host_limit_exempt_subnets` | loopback | subnets | not limited by the per-host cap |
| - | `host_limit_relax_delay` | 60 | s | the cap stays hard this long after start |
| - | `reserved_address_family_percent` | 25 | % of desired | share kept for each address family |
| - | `peer_connection_retry_timeout` | 30 | s | redial backoff base |
| - | `peer_inactivity_timeout` | 5 | s | handshake inactivity |
| - | `idle_peer_disconnect_timeout` | 0 (off) | s | idle replacement window |
| - | `silent_peer_disconnect_timeout` | 1800 | s | silent-peer replacement window; 0 = off |
| - | `active_ignored_request_timeout_microseconds` | 6,000,000 | us | request timeout |
| `p2p-transaction-flush-delay-us` | `transaction_flush_delay_microseconds` | 1000 | us | how long transaction traffic waits to share a write |
| `p2p-fetch-items-minimum-interval-us` | `fetch_items_minimum_interval_microseconds` | 5000 | us | minimum time between transaction-triggered fetch passes |
| `p2p-max-transactions-in-flight` | `maximum_transactions_in_flight` | 10000 | transactions | safety cap on transactions awaiting validation; stops fetching, never reading |
| - | `maximum_number_of_blocks_to_handle_at_one_time` | 200 | blocks | sync blocks in processing |
| - | `maximum_number_of_sync_blocks_to_prefetch` | 20000 | blocks | sync prefetch |
| - | `maximum_blocks_per_peer_during_syncing` | 200 | blocks | one sync request |
| - | `peer_advertising_disabled` | false | bool | don't share peer addresses |
| - | `accept_incoming_connections`, `wait_if_endpoint_is_busy`, `private_key` | true, true, generated | - | listening and node identity |
| `max-mempool-size` (chain) | - | chain default | bytes | the fetch gate closes above a third of it |

`node_config.json` in the p2p data directory saves the whole configuration of the last run and is read at start,
before `p2p-parameters`; `idle_peer_disconnect_timeout`, `silent_peer_disconnect_timeout` and `relax_per_host_limit`
saved there are ignored.

## Pending measurement: the flush delay

`transaction_flush_delay_microseconds` (1 ms) and `fetch_items_minimum_interval_microseconds` (5 ms) together add
about 6 ms per hop at low load, where batching gains nothing. The A/B that decides the flush delay's default (1 ms
against 0 at mainnet-like load, n=2, mixed versions, r200 and a low-load layout, measuring per-hop relay latency) is
pending a live run on the p2p_load harness; until it is recorded here the default stays 1 ms.

## Limits

Basis: **derived** from other constants (change those instead); **measured** from a recorded measurement;
**reasoned** in its comment, not measured; **unvalidated**, a guess nobody has measured or argued from data yet;
**inherited/reasoned**, a graphene-era value kept as it was; **n/a** for identifiers and log rates. Values are shown
without the `GRAPHENE_NET_` prefix inside expressions. The unvalidated values are candidates for the next live check.

#### Protocol versions

| Name | Value | Unit | Purpose | Basis |
|---|---|---|---|---|
| `GRAPHENE_NET_PROTOCOL_VERSION` | `107` | - | protocol version we announce | n/a |
| `GRAPHENE_NET_PROTOCOL_COMPRESSED_BLOCKS_VERSION` | `107` | - | first version with compressed blocks | n/a |
| `GRAPHENE_NET_PROTOCOL_ADVERTISE_BLOCKS_BY_BLOCK_ID_VERSION` | `107` | - | first version advertising blocks by id | n/a |
| `GRAPHENE_NET_PROTOCOL_FIREWALL_CHECK_VERSION` | `106` | - | first version with the firewall check | n/a |
| `GRAPHENE_NET_MINIMUM_PROTOCOL_VERSION` | `106` | - | oldest peer version accepted | n/a |
| `GRAPHENE_NET_HELLO_DEFERS_TRANSACTION_REPLIES` | `"defers_transaction_replies"` | hello key | peer queues replies as its queue drains; enables our byte window | n/a |
| `GRAPHENE_NET_HELLO_QUEUES_BLOCKS_FIRST` | `"queues_blocks_first"` | hello key | peer queues blocks ahead of transactions; late copies may be struck | n/a |
| `GRAPHENE_NET_HELLO_UNDERSTANDS_ITEM_BUSY` | `"understands_item_busy"` | hello key | peer understands item_busy_message | n/a |
| `ENABLE_P2P_DEBUGGING_API` | `1` | flag | debugging code for tests | n/a |
| `MAX_MESSAGE_SIZE` | `(1024*1024*2 + 20)` | bytes | largest message read | inherited/reasoned |

#### Connection admission

| Name | Value | Unit | Purpose | Basis |
|---|---|---|---|---|
| `GRAPHENE_NET_TEST_SEED_IP` | `"104.236.44.210"` | address | unused | n/a |
| `GRAPHENE_NET_TEST_P2P_PORT` | `1700` | port | testnet default port | n/a |
| `GRAPHENE_NET_DEFAULT_P2P_PORT` | `1776` | port | default port | n/a |
| `GRAPHENE_NET_DEFAULT_DESIRED_CONNECTIONS` | `20` | connections | default desired_number_of_connections | inherited/reasoned |
| `GRAPHENE_NET_DEFAULT_MAX_CONNECTIONS` | `200` | connections | default maximum_number_of_connections | inherited/reasoned |
| `GRAPHENE_NET_DEFAULT_PEER_CONNECTION_RETRY_TIME` | `30` | s | default peer_connection_retry_timeout | inherited/reasoned |
| `GRAPHENE_NET_MAX_PEERDB_SIZE` | `1000` | records | peer database size; also bounds the node ids remembered per dialed address | inherited/reasoned |
| `GRAPHENE_NET_FETCH_UPDATED_PEER_LISTS_INTERVAL_MINUTES` | `15` | min | address request interval | inherited/reasoned |
| `GRAPHENE_NET_FIREWALL_CHECK_MESSAGE_INTERVAL_MINUTES` | `5` | min | firewall check interval | inherited/reasoned |
| `GRAPHENE_NET_PORT_WAIT_DELAY_SECONDS` | `5` | s | wait for a busy listen port | inherited/reasoned |
| `GRAPHENE_NET_CONNECT_LOOP_MINIMUM_INTERVAL_MS` | `1000` | ms | bounds how often the connect loop runs on triggers | reasoned |
| `GRAPHENE_NET_CONNECT_LOOP_MAXIMUM_WAIT_SEC` | `10` | s | connect loop runs at least this often | reasoned |
| `GRAPHENE_NET_ROUTE_PROBE_CACHE_SEC` | `60` | s | route lookup cache | inherited/reasoned |
| `GRAPHENE_NET_PEER_HANDSHAKE_INACTIVITY_TIMEOUT` | `5` | s | default peer_inactivity_timeout (handshake) | inherited/reasoned |
| `GRAPHENE_NET_HANDSHAKE_DEADLINE_SEC` | `15` | s | handshake must finish within it | reasoned |
| `GRAPHENE_NET_MAXIMUM_HANDSHAKING_INBOUND_CONNECTIONS` | `32` | connections | inbound connections before hello, node-wide | unvalidated |
| `GRAPHENE_NET_MAXIMUM_HANDSHAKING_CONNECTIONS_PER_HOST` | `2` | connections | the same, per host | unvalidated |
| `GRAPHENE_NET_REFUSED_HANDSHAKE_WARNING_INTERVAL_SEC` | `60` | s | log rate | n/a |
| `GRAPHENE_NET_MAXIMUM_HANDSHAKE_MESSAGE_SIZE` | `(64 * 1024)` | bytes | largest message before hello | reasoned |
| `GRAPHENE_NET_HANDSHAKE_BUFFER_SIZE` | `(2 * 1024)` | bytes | buffers before hello | measured: heap per handshaking connection about 62 KB against 211-232 KB with the full buffers |
| `GRAPHENE_NET_CONNECTION_BUFFER_SIZE` | `(64 * 1024)` | bytes | buffers after hello | reasoned |
| `GRAPHENE_NET_DEFAULT_MAX_CONNECTIONS_PER_HOST` | `2` | connections | default maximum_connections_per_host | unvalidated |
| `GRAPHENE_NET_DEFAULT_RESERVED_ADDRESS_FAMILY_PERCENT` | `25` | % of desired | default reserved_address_family_percent | unvalidated |
| `GRAPHENE_NET_SHARED_INBOUND_HOST_WARNING_INTERVAL_SEC` | `600` | s | log rate | n/a |
| `GRAPHENE_NET_HOST_LIMIT_RELAXED_LOG_INTERVAL_SEC` | `600` | s | log rate | n/a |
| `GRAPHENE_NET_DEFAULT_HOST_LIMIT_RELAX_DELAY_SEC` | `60` | s | default host_limit_relax_delay | unvalidated |
| `GRAPHENE_NET_MAX_RELAXED_CANDIDATES_PER_PASS` | `64` | candidates | bounds a relaxed connect pass | reasoned |
| `GRAPHENE_NET_MAX_HOST_LIMIT_REJECTIONS_COUNTED` | `2` | attempts | caps backoff for host-full rejections | reasoned |
| `GRAPHENE_NET_PRUNED_TAIL_GRACE_BLOCKS` | `20` | blocks | peer head lag behind our pruned tail | inherited/reasoned |

#### Send queue

| Name | Value | Unit | Purpose | Basis |
|---|---|---|---|---|
| `GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES` | `(1024 * 1024)` | bytes | send queue limit; past it the connection closes | reasoned: inherited from graphene |
| `GRAPHENE_NET_TRANSACTION_BUDGET_BYTES` | `(MAXIMUM_QUEUED_MESSAGES_IN_BYTES / 2)` | bytes | the one transaction budget | derived: half of GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES |
| `GRAPHENE_NET_MAXIMUM_QUEUED_TRANSACTION_REPLY_BYTES` | `TRANSACTION_BUDGET_BYTES` | bytes | replies queued only below it | derived: GRAPHENE_NET_TRANSACTION_BUDGET_BYTES |
| `GRAPHENE_NET_MAXIMUM_QUEUED_TRANSACTION_INVENTORY_BYTES` | `TRANSACTION_BUDGET_BYTES` | bytes | inventory queued only below it | derived: GRAPHENE_NET_TRANSACTION_BUDGET_BYTES |
| `GRAPHENE_NET_MAX_TRANSACTION_INVENTORY_IDS_PER_MESSAGE` | `(MAXIMUM_QUEUED_TRANSACTION_INVENTORY_BYTES / 4 / 20)` | ids | ids per inventory we send | derived: 20 bytes an id |
| `GRAPHENE_NET_MAX_DEFERRED_TRANSACTION_INVENTORY_IDS_PER_PEER` | `(MAXIMUM_QUEUED_TRANSACTION_INVENTORY_BYTES / 20)` | ids | held-back inventory per peer | derived: 20 bytes an id |
| `GRAPHENE_NET_MAX_DEFERRED_TRANSACTION_INVENTORY_AGE_MS` | `6000` | ms | held-back inventory age | reasoned: two 3 s block intervals |
| `GRAPHENE_NET_DROPPED_TRANSACTION_INVENTORY_REPORT_INTERVAL_SEC` | `10` | s | log rate | n/a |
| `GRAPHENE_NET_MAX_DEFERRED_TRANSACTION_REPLIES_PER_PEER` | `(2 * MAX_TRANSACTIONS_IN_FLIGHT_PER_PEER)` | transactions | held-back replies per peer | derived: 2 * GRAPHENE_NET_MAX_TRANSACTIONS_IN_FLIGHT_PER_PEER |
| `GRAPHENE_NET_MAX_DEFERRED_TRANSACTION_REPLY_BYTES_PER_PEER` | `(8 * 1024 * 1024)` | bytes | held-back reply bytes per peer | reasoned |
| `GRAPHENE_NET_BLOCK_MESSAGES_PREPARED_AHEAD` | `4` | messages | blocks looked up ahead of sending | reasoned |
| `GRAPHENE_NET_MAXIMUM_BATCHED_SEND_BYTES` | `(64 * 1024)` | bytes | one socket write batch | reasoned: the size of one connection buffer |

#### Transaction fetching

| Name | Value | Unit | Purpose | Basis |
|---|---|---|---|---|
| `GRAPHENE_NET_MAX_TRANSACTIONS_PER_PEER_DURING_NORMAL_OPERATION` | `400` | transactions | one request | reasoned |
| `GRAPHENE_NET_MAX_TRANSACTIONS_IN_FLIGHT_PER_PEER` | `(2 * MAX_TRANSACTIONS_PER_PEER_DURING_NORMAL_OPERATION)` | transactions | per-peer item window | derived: two requests |
| `GRAPHENE_NET_MAX_TRANSACTION_BYTES_IN_FLIGHT_PER_PEER` | `TRANSACTION_BUDGET_BYTES` | bytes | per-peer byte window | derived: GRAPHENE_NET_TRANSACTION_BUDGET_BYTES |
| `GRAPHENE_NET_MAX_TRANSACTION_BYTES_PER_FETCH` | `TRANSACTION_BUDGET_BYTES` | bytes | one request | derived: GRAPHENE_NET_TRANSACTION_BUDGET_BYTES |
| `GRAPHENE_NET_INITIAL_TRANSACTION_SIZE_ESTIMATE` | `HIVE_MAX_TRANSACTION_SIZE` | bytes | size assumed before a peer sent any | derived: HIVE_MAX_TRANSACTION_SIZE |
| `GRAPHENE_NET_TRANSACTION_FETCH_GATE_POOL_DIVISOR` | `3` | - | gate threshold is max-mempool-size / this | unvalidated |
| `GRAPHENE_NET_TRANSACTION_FETCH_GATE_RECHECK_MS` | `100` | ms | gate re-check and minimum interval | unvalidated |
| `GRAPHENE_NET_MIN_TRANSACTIONS_PER_GATED_FETCH` | `20` | transactions | gated minimum per interval | unvalidated |
| `GRAPHENE_NET_GATED_FETCH_EXPIRY_WARNING_INTERVAL_SEC` | `30` | s | log rate | n/a |
| `GRAPHENE_NET_MAX_INVENTORY_SIZE_IN_MINUTES` | `1` | min | advertised-to-us inventory window | inherited/reasoned |
| `GRAPHENE_NET_MAX_TRX_PER_SECOND` | `1000` | tx/s | assumed network rate | reasoned: inherited from graphene |
| `GRAPHENE_NET_MAX_TRANSACTION_INVENTORY_PER_PEER` | `(MAX_INVENTORY_SIZE_IN_MINUTES * MAX_TRX_PER_SECOND * 60)` | ids | inventory a peer may advertise per window | derived: the window times GRAPHENE_NET_MAX_TRX_PER_SECOND |
| `GRAPHENE_NET_MAX_ITEM_IDS_PER_INVENTORY_MESSAGE` | `MAX_TRANSACTION_INVENTORY_PER_PEER` | ids | largest inventory message accepted | derived: the window times GRAPHENE_NET_MAX_TRX_PER_SECOND |
| `GRAPHENE_NET_MAX_TRANSACTIONS_PER_FETCH_REQUEST` | `(2 * MAX_TRANSACTIONS_IN_FLIGHT_PER_PEER)` | transactions | largest request accepted | derived |
| `GRAPHENE_NET_INVENTORY_LIMIT_REPORT_INTERVAL_SEC` | `10` | s | log rate | n/a |

#### Block fetching

| Name | Value | Unit | Purpose | Basis |
|---|---|---|---|---|
| `GRAPHENE_NET_MAX_BLOCKS_PER_PEER_DURING_NORMAL_OPERATION` | `100` | blocks | blocks per peer per pass | inherited/reasoned |
| `GRAPHENE_NET_MAX_BLOCKS_PER_PEER_DURING_SYNCING` | `200` | blocks | default maximum_blocks_per_peer_during_syncing | inherited/reasoned |
| `GRAPHENE_NET_MAX_BLOCKS_PER_FETCH_REQUEST` | `MAX_BLOCKS_PER_PEER_DURING_SYNCING` | blocks | largest block request accepted | derived |
| `GRAPHENE_NET_MIN_BLOCK_IDS_TO_PREFETCH` | `20000` | ids | sync id prefetch | inherited/reasoned |
| `GRAPHENE_NET_MAX_NUMBER_OF_BLOCKS_TO_HANDLE_AT_ONE_TIME` | `200` | blocks | sync blocks in processing | inherited/reasoned |
| `GRAPHENE_NET_MAX_NUMBER_OF_BLOCKS_TO_PREFETCH` | `(100* MAX_NUMBER_OF_BLOCKS_TO_HANDLE_AT_ONE_TIME)` | blocks | sync blocks prefetched | inherited/reasoned |
| `GRAPHENE_NET_FUTURE_SYNC_BLOCKS_GRACE_PERIOD_SEC` | `(60 * 60)` | s | clock skew allowed in sync | inherited/reasoned |
| `GRAPHENE_NET_FUTURE_BLOCK_IDS_GRACE_BLOCKS` | `20` | blocks | advertised ids past our head | inherited/reasoned |
| `GRAPHENE_NET_BLOCK_REREQUEST_TIMEOUT_MS` | `1000` | ms | first block request deadline (doubles) | reasoned |
| `GRAPHENE_NET_MAX_OUTSTANDING_REQUESTS_PER_BLOCK` | `3` | requests | concurrent requests for one block | reasoned |
| `GRAPHENE_NET_BLOCK_COPY_GRACE_DEADLINES` | `2` | deadlines | late copy grace | reasoned |

#### Withholding

| Name | Value | Unit | Purpose | Basis |
|---|---|---|---|---|
| `GRAPHENE_NET_UNAVAILABLE_ITEM_DEMOTION_SEC` | `10` | s | demotion pair: transactions; strike spacing | reasoned |
| `GRAPHENE_NET_MAX_UNAVAILABLE_ITEM_STRIKES` | `3` | strikes | disconnect at this many | reasoned |
| `GRAPHENE_NET_UNAVAILABLE_ITEM_STRIKE_DECAY_SEC` | `600` | s | a strike is forgotten after it | reasoned |
| `GRAPHENE_NET_BLOCK_FETCHING_DEMOTION_SEC` | `60` | s | demotion pair: blocks | reasoned |
| `GRAPHENE_NET_WITHHOLDING_WARNING_INTERVAL_SEC` | `60` | s | log rate | n/a |
| `GRAPHENE_NET_NOT_AVAILABLE_WARNING_INTERVAL_SEC` | `60` | s | log rate | n/a |

#### Liveness

| Name | Value | Unit | Purpose | Basis |
|---|---|---|---|---|
| `GRAPHENE_NET_ACTIVE_IGNORED_REQUEST_TIMEOUT_SECONDS` | `6` | s | documents the active_ignored_request_timeout default (6 s); not used in code | reasoned |
| `GRAPHENE_NET_MAX_TRANSACTION_REQUEST_TIMEOUTS_WHILE_DELIVERING` | `3` | timeouts | transaction request bound while delivering | reasoned |
| `GRAPHENE_NET_MAX_BLOCK_REQUEST_TIMEOUTS_WHILE_DOWNLOADING` | `2` | timeouts | block request bound while downloading | reasoned |
| `GRAPHENE_NET_SEND_STALL_DISCONNECT_SEC` | `60` | s | the slow-reader rule | reasoned |
| `GRAPHENE_NET_PEER_DISCONNECT_TIMEOUT` | `20` | s | peer asked to close must close within it | inherited/reasoned |
| `GRAPHENE_NET_FAILED_TERMINATE_TIMEOUT_SECONDS` | `120` | s | terminated connection deleted after it | inherited/reasoned |
| `GRAPHENE_NET_INVALID_BLOCK_DISCONNECT_BREAKER_COUNT` | `5` | disconnects | invalid-block breaker trips at this many | reasoned |
| `GRAPHENE_NET_INVALID_BLOCK_DISCONNECT_BREAKER_WINDOW_SEC` | `60` | s | its window | reasoned |
| `GRAPHENE_NET_BLOCK_BODY_MISMATCH_DISCONNECT_BREAKER_COUNT` | `5` | disconnects | body-mismatch breaker trips at this many | inherited/reasoned |
| `GRAPHENE_NET_BLOCK_BODY_MISMATCH_DISCONNECT_BREAKER_WINDOW_SEC` | `60` | s | its window | inherited/reasoned |
| `GRAPHENE_NET_BREAKER_STALL_OVERRIDE_SEC` | `60` | s | stall rule 1: breaker override | reasoned |
| `GRAPHENE_NET_STALLED_HEAD_RESYNC_SEC` | `60` | s | stall rule 2: sync restart | reasoned |
| `GRAPHENE_NET_DEFAULT_IDLE_PEER_DISCONNECT_TIMEOUT_SEC` | `0` | s | stall rule 3: default idle_peer_disconnect_timeout (0 = off) | unvalidated: off by default |
| `GRAPHENE_NET_IDLE_PEERS_HEAD_BEHIND_SEC` | `60` | s | idle replacement only while this far behind | inherited/reasoned |
| `GRAPHENE_NET_IDLE_PEERS_KEPT_AT_LEAST` | `2` | connections | idle replacement floor | inherited/reasoned |
| `GRAPHENE_NET_IDLE_PEER_DISCONNECT_WARNINGS_PER_INTERVAL` | `10` | warnings | log rate | n/a |
| `GRAPHENE_NET_IDLE_PEER_DISCONNECT_WARNING_INTERVAL_SEC` | `60` | s | log rate | n/a |
| `GRAPHENE_NET_DEFAULT_SILENT_PEER_DISCONNECT_TIMEOUT_SEC` | `(30 * 60)` | s | stall rule 4: default silent_peer_disconnect_timeout | unvalidated |

#### Caches

| Name | Value | Unit | Purpose | Basis |
|---|---|---|---|---|
| `GRAPHENE_NET_MESSAGE_CACHE_DURATION_IN_BLOCKS` | `20` | blocks | message cache lifetime | inherited/reasoned |
| `GRAPHENE_NET_TRANSACTION_MESSAGE_CACHE_EXPECTED_SIZE` | `(256 * 1024)` | transactions | cache index presize | derived |
| `GRAPHENE_NET_PRUNE_FAILED_IDS_MINUTES` | `15` | min | failed item ids kept | inherited/reasoned |
| `GRAPHENE_NET_BANDWIDTH_MONITOR_INTERVAL_SECONDS` | `1` | s | bandwidth sample interval | inherited/reasoned |
| `GRAPHENE_NET_DUMP_NODE_STATUS_INTERVAL_MINUTES` | `1` | min | status dump interval | inherited/reasoned |
| `GRAPHENE_NET_STATISTICS_REPORT_INTERVAL_SEC` | `30` | s | p2p_stats report interval | inherited/reasoned |
| `GRAPHENE_NET_STATISTICS_LOGGER` | `"p2p_stats"` | logger | p2p_stats logger name | n/a |
| `GRAPHENE_NET_RELAY_SAMPLE_ONE_IN` | `64` | transactions | relay sample rate | inherited/reasoned |
| `GRAPHENE_NET_RELAY_SAMPLE_LOGGER` | `"p2p_relay_sample"` | logger | relay sample logger name | n/a |
