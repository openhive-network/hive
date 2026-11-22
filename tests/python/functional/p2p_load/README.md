# p2p load harness

Floods a small testnet with colony transactions over a chosen p2p topology and measures how the nodes keep up:
included and received tx/s, underfilled blocks, skew-corrected block offsets, per-thread CPU, send syscalls, relay
latency and hived's own p2p counters. It is the in-repo replacement of the `w877`, `w877x` and `w1075` host directories behind hive#877
and its follow-ups (#992, #1060-#1077); reports cite a commit of this directory instead of a host path.

## What it needs

- A Linux host with Docker, `python3` (3.11 or newer) and `/dev/shm` large enough for every node's state
  (4 GiB shared file per node; up to about 2.8 GiB of it is used) and the replay templates (2.6 GiB each, at most 6
  kept; see below).
- A testnet hived (`-DBUILD_HIVE_TESTNET=ON`), and `get_dev_key` and `block_log_util` from a testnet build. `--tools`
  takes a flat directory or a build tree; by default they are looked up next to `--hived` and in its build tree.
- The `single_sign_universal_block_log` fixture: CI's testing block log image carries it, or generate it with
  `tests/python/functional/util/universal_block_logs/generate_universal_block_logs.py
  --universal-block-log-type single_sign --output-block-log-directory DIR`.
- The test-tools image the driver runs in: `.aidev/project.yaml`'s `aidev-tests` image by default,
  or `--image` / `P2P_LOAD_IMAGE`. The driver imports only test-tools from it, nothing from `hive_local_tools`.
- Passwordless `sudo` for `tc` (only with `--netem-ms`) and `bpftrace` (only with `--sends`).

Every command goes through `tests/python/functional/p2p_load/p2p-load` (host `python3`, standard library only):

```
P=tests/python/functional/p2p_load/p2p-load
COMMON="--block-log /path/to/single_sign_universal_block_log --tools /path/to/build --out-root ~/p2p-load-runs"
```

## A run

`p2p-load run NAME --hived PATH --tpb N [options]` (or `--reps R` runs `NAME-LABEL-1..R`):

1. takes the host lock (see the rules below) or refuses with exit code 2;
2. creates `/dev/shm/hive-p2p-load/NAME` for the node data and `OUT_ROOT/NAME` for the results;
3. installs the `tc netem` delay when asked, then starts the driver in container `p2p-load-NAME`;
4. the driver creates the nodes (`witness`, `observer`, `colony0..`, `relay1..`), wires the topology, gives every
   node the replayed block log's state (copied from a replay template, see below), checks each node's
   `config.ini`, starts them under libfaketime from the block log's head time in waves 1 s apart, each node one wave
   after its seeds and its faked clock started as much later (a node dials its seeds once as it starts and, refused,
   even when redialed, again only on hived's next 10 s connect loop tick: a 3-hop chain started all at once took
   30-40 s to link),
   redials every node's seeds until each seed link is up (links still down after 60 s fail the run), waits
   until every node is live, samples every 3 s and waits until `--blocks` blocks after the colonies started;
5. copies back only logs and configs (`OUT_ROOT/NAME/generated/<node>/`, no `blockchain/`, no `*.bin`) and removes
   the tmpfs directory, whatever happened. A disk-backed 8-peer run wrote 90-415 GB; this one writes megabytes.

`OUT_ROOT/NAME` then holds `spec.json` (what was asked), `out.json` (roles, binaries, p2p parameters and p2p
loggers per node, blocks, `/proc` samples of node threads and host CPU, link maps), `driver.log`, `generated/`, and `tc.txt` / `sends.txt` when used.
`OUT_ROOT/series.log` records each run's start and end with host, exit code, load average and wall time.

### Replay templates: replay once per binary, copy for every run

Replaying the fixture takes about 20 s per node; the old drivers replayed it on every node, one after another
(about 210 s of a 400 s 8-peer run). The driver now keeps each replayed state in
`/dev/shm/hive-p2p-load-templates/<key>/` and copies it (sparse, about 2.5 s for 10 nodes) into every node of later
runs. The key covers the hived binary's content, the fixture's block log and `alternate-chain-spec.json`, and the
node's plugins, shared file size and block log layout, so a run has one template per plugin set (witness, API
node, colony). A key without a template is replayed by one of its nodes, all such replays in parallel, and that
node's state becomes the template: a rebuilt binary or a new fixture replays on its first run and is reused after.
The 6 most recently used templates are kept. `--fresh-replay` skips templates and replays on every node in
parallel (for a host short of tmpfs); `rm -rf /dev/shm/hive-p2p-load-templates` frees them while no run is active.
Each report ends with the driver's phase times: state (replays, templates reused, copy), start to live, live
to the end of the run (idle and loaded blocks), close.

Options:

| option | meaning |
|---|---|
| `--topology mesh` | every node seeded to the witness |
| `--topology star` | the witness seeds every colony; the observer seeds the witness |
| `--topology strict` | star, but each colony and the observer keep only that one link (`p2p-max-connections = 1`) |
| `--topology chain` | every colony -> relay1 -> ... -> relayN -> witness -> observer; connection limits leave peer exchange no shortcut |
| `--topology diamond` | strict star into the witness, which reaches the observer only through each of `--relays` (at least 2) relays: every block reaches the observer from that many advertisers |
| `--tpb N` | colony transactions per block, split evenly over the colonies |
| `--colonies N`, `--relays N` | relays exist only in `chain` (between the colonies and the witness) and `diamond` (between the witness and the observer) |
| `--blocks N` | loaded blocks measured, counted from the colonies' start (default 30); `--colony-start-delay` blocks after the block log's head (default 12) come first: the nodes link up in about 4 of them, the rest are the idle blocks the skew needs |
| `--fresh-replay` | replay the block log on every node (in parallel) instead of copying replay templates |
| `--node-hived ROLE=PATH` | a different hived for one node (mixed versions); the others use `--hived` |
| `--flush-us`, `--fetch-us`, `--inflight`, `--p2p-parameter KEY=JSON` | p2p parameters, see below |
| `--late-observer K` | the observer starts K blocks after the colonies start, its faked clock at the others' current fake time |
| `--netem-ms D --netem-colonies 0,2` | delay each packet of those colonies' links by D ms each way (RTT + 2D) |
| `--sends S` | after 6 loaded blocks, count the witness's send syscalls per thread with bpftrace for S seconds |
| `--no-p2p-loggers` | leave the p2p_stats and p2p_relay_sample loggers off (see below), e.g. to measure what they cost |
| `--mix large` | colony replies and articles only, 4000..12000 byte bodies (about 255 transactions fill a 2 MiB block) |
| `--colony-mix N=MIX`, `--colony-late N=K` | colony N runs MIX instead of `--mix`, and starts K loaded blocks after the others: a size jump on one link in one run when its transactions share a relay with the others (`chain`) |
| `--remote-host H --remote-roles R,...` | run those nodes on host H, the rest here: see "Two hosts" below |

### p2p parameters: only what is given, only where the binary has it

An option that is not given is not written: the node runs its binary's default. A given option is written into
`p2p-parameters` only on nodes whose binary knows the key, read from that binary's `hived --help` default; a build
that predates an option gets no key for it (the report names what was left out). After the replay, the driver
compares each node's `config.ini` with what it meant to set and fails the run on any difference. `out.json`
records the given, applied and left-out parameters and every binary's defaults. This replaces the old drivers'
"set only when non-zero" rule, which ran a "delay 0" run at the build's 5 ms default once the default changed.

### p2p loggers: on wherever the binary has them

hived since hive#1079 writes a JSON counter report every 30 s to the `p2p_stats` logger and timestamps one in 64
transactions (picked by message hash, so the same ones on every node) to the `p2p_relay_sample` logger; both are off
unless configured. The driver adds `log-logger` entries for them at `info`, into the `p2p` log, on every node whose
binary carries the logger's name, and checks each `config.ini` for them after the replay. An older binary gets no
entry and runs as before. `out.json` records the loggers each node got.

Every node also runs its `default` logger at `debug`: hived logs each fast-confirm (`witness_block_approve`) it accepts
only there. The logger's other debug lines come at most once per block or on rare errors.

## Two hosts

`--remote-host HOST --remote-roles colony0,colony1,...` runs the named nodes on HOST and the others (always the
witness) on the host the command runs on, so the p2p traffic between the two groups crosses a real NIC and network
instead of loopback. Every other option works as on one host, except `--netem-ms` (it delays loopback links) and a
late observer on the remote host.

All the nodes of a host share its address, so a two-host run gives every node `maximum_connections_per_host` 0 (no
per-host connection limit) unless `--p2p-parameter` sets it; a binary that predates the limit gets no key for it.

```
# colonies on steem-2, witness and observer here (steem-4)
$P run star8-split $COMMON --hived $HIVED --label split --topology strict --colonies 8 --tpb 20000 \
    --remote-host steem-2 --remote-roles colony0,colony1,colony2,colony3,colony4,colony5,colony6,colony7
# 3-hop chain, colony0 and relay1 on steem-2: colony0 -> relay1 cross no host, relay1 -> relay2 does
$P run chain3-split $COMMON --hived $HIVED --label split --topology chain --relays 2 --tpb 1000 \
    --remote-host steem-2 --remote-roles colony0,relay1
```

What it needs: `ssh HOST` without a password from this host (an agent or a key), python3 3.11+, rsync and Docker on
both hosts, the test-tools image on HOST under the same reference as here (`--image`; a local tag works), and
the hosts able to reach each other's p2p ports (ephemeral ones).
Every path is the same on both hosts: the series copies (rsync, only what changed) this harness, the builds, the
tools and the block log to the same absolute paths on HOST before its first run, and the remote part writes its
results under the same `--out-root`.

For each series and each run:

1. **Both host locks.** The series takes this host's lock, then HOST's: `p2p-load hold-lock` there holds it for as
   long as its ssh connection lasts, so it goes with the series however the series ends. Either host busy: exit 2.
2. **Addresses.** Nodes listen on `0.0.0.0` and dial each other at the hosts' IPv4 addresses (HOST's from DNS, this
   host's the source address of the route to it; `--remote-address` / `--local-address` override). Ports are
   picked free on each host from 20000 up, below its ephemeral port range, so that no outgoing connection or
   node webserver takes one before its node listens. Seeds between nodes of one host use that host's address too.
3. **Clock offset.** Relay latency compares real-clock timestamps of nodes on different hosts. Before each series,
   and again before each run, the harness measures HOST's clock minus this one's: 50 probes over one ssh
   connection, the one with the shortest round trip taken (the error is at most half that round trip, printed as
   `+-`), next to the same offset as both hosts' NTP clients see it (chrony, or systemd-timesyncd). The series prints it; each run keeps it in
   `clock.json` and `out.json`'s `clock`.
4. **One start.** Each host's driver (container `p2p-load-NAME` on both) gives its own nodes the replayed state,
   from that host's own replay templates (each host keeps its templates in its own
   `/dev/shm/hive-p2p-load-templates`; `--fresh-replay` applies to both), then waits. Once both are ready the
   harness sends both the same moment, 5 s ahead, as each host's own clock reads it (HOST's corrected by the
   measured offset). The start waves count from then on both hosts: both drivers group every node of the run into
   the same waves and start their own nodes in theirs, each wave's faked clock that many seconds past the block
   log's head time, so the faked clocks of both hosts agree to within the clock offset's error. The seed redial and
   live wait follow on each host for its own nodes.
5. **End.** The remote part samples its nodes' CPU and its host's busy ticks with the head of one of its nodes, and
   ends when its nodes have the run's last block or the local part ends. Either part failing ends the other.
6. **Logs.** Each host copies its nodes' logs and configs to its run directory and removes its tmpfs directory;
   the remote run directory is then copied back to `OUT_ROOT/NAME/remote/` (`generated/`, `driver.log`, its own
   `out.json`), and its nodes, samples (marked `"part": "remote"`) and parameters are merged into
   `OUT_ROOT/NAME/out.json` (`remote_roles`, `remote` and `clock` say what ran where). The ssh session's stderr is `part.log`.

The report reads the remote nodes' logs from `remote/generated/`, shifts their relay sample timestamps by the
measured offset onto this host's clock, and prints a `two hosts:` line with each host's nodes and the offset, and
a `remote host:` load line next to the `host:` one (`BUSY REMOTE HOST` past `--busy-cores`; either host busy makes
the run a busy one for `compare`). The
block offsets need no correction: their skew correction already takes out each node's clock difference.

To compare a split run with the same run on one host, alternate them in one shell loop with `--label split` and
`--label single` (no `--remote-host`) and read them with `compare`: the comparison groups by label.

## Standard scenarios

```
# 8-peer saturated star: 8 colonies, each on one link to the witness, offering far more than a block holds
$P run star8-sat $COMMON --hived $HIVED --topology strict --colonies 8 --tpb 20000
# 2500 tx/block star (unsaturated: read the colony caveat below)
$P run star8-2500 $COMMON --hived $HIVED --topology strict --colonies 8 --tpb 2500
# 1 peer: a single saturated colony link
$P run star1 $COMMON --hived $HIVED --topology strict --colonies 1 --tpb 20000
# 200 ms RTT single link: colony0's link delayed 100 ms each way
$P run rtt200 $COMMON --hived $HIVED --topology strict --colonies 1 --tpb 2500 --netem-ms 100
# 3-hop chain: colony0 -> relay1 -> relay2 -> witness, 1000 tx/block
$P run chain3 $COMMON --hived $HIVED --topology chain --relays 2 --tpb 1000
# two advertisers per block: the witness reaches the observer through relay1 and relay2, under a saturated flood
$P run diamond $COMMON --hived $HIVED --topology diamond --relays 2 --colonies 8 --tpb 20000
# size jump on one sender: relay1 forwards colony0's default mix, and from loaded block 15 on also colony1's large one
$P run jump $COMMON --hived $HIVED --topology chain --relays 1 --colonies 2 --tpb 4000 --colony-mix 1=large --colony-late 1=15
# mixed versions: an older witness among current nodes
$P run mixed $COMMON --hived $HIVED --node-hived witness=$OLD_HIVED --topology strict --colonies 8 --tpb 20000
# late joiner: the observer joins 10 loaded blocks late and catches up under load
$P run late $COMMON --hived $HIVED --topology strict --colonies 8 --tpb 2500 --late-observer 10
```

### A/B builds

`p2p-load ab NAME --build A=PATH --build B=PATH --reps R [run options]` runs A, B, A, B, ... (`NAME-A-1`,
`NAME-B-1`, `NAME-A-2`, ...) under one host lock and prints each run's report, then the comparison table: the
mean of each headline metric per build with the per-run values. A run whose driver fails or whose report cannot be
built (no loaded blocks, for one) counts as failed: the series goes on, compares the runs that finished and exits 1.
For example #1073 against its parent `fb7e4d96`
on the 3-hop chain at 1000 tx/block (#1075 measured a colony-to-relay1 median of 51 against 134 ms):

```
$P ab chain3 $COMMON --build cand=$B1073/hived --build base=$B_FB7E4D96/hived --topology chain --relays 2 --tpb 1000 --reps 2
```

## Reports

`p2p-load report RUN_DIR...` (`--json` for machine-readable) and `p2p-load compare RUN_DIR...` (grouped by label)
work on any finished run directory and need no node and no test-tools.

- **short window**: a run measures `--blocks` blocks from the colonies' start block, the first two as ramp. The
  report flags it `SHORT WINDOW` when the loaded window has fewer blocks than that, or when the witness was already
  at the colonies' start block once every node was live (`late_start` in `out.json`: the colonies started late).
  `report --json` carries it as `window`, and `compare` counts loaded blocks and short windows per label; leave
  short runs out of conclusions.
- **busy host**: the harness hosts also carry aidev session stacks and other analyses' series, and host load alone
  moves block offsets: on steem-13 (hive#1090) a run hit by another workload spent 89-93 ms instead of 70 ms
  generating a block on the witness, applied blocks later on the observer, and landed 40-45 ms late, for either
  build. Every sample records the host's busy CPU ticks (`/proc/stat`, all but idle and iowait; the container sees
  the host's) next to those of the driver's process tree (every node, the colonies and the driver itself). Over the
  loaded window, the difference is the cores busy outside the run; the report prints it with its maximum over one
  3 s sample interval, the host's busy cores out of its CPU count and the run's own, and flags the run
  `BUSY HOST` when the window's mean exceeds `--busy-cores` (default 4: on steem-2/4/5/13, 84 s windows of quiet
  stretches stayed at 0.1-3.3 cores outside any harness run, other workloads' episodes at 6-12). `report --json`
  carries it as `host_load`. Once any run of a comparison is busy, `compare` counts busy runs per label, marks their values `*`
  and gives each label's mean without them (`quiet`) next to the mean over all runs; draw A/B conclusions from the
  quiet means, or name the busy runs. Runs recorded before the samples carried host ticks show none and are never
  flagged.
- **tx/s**: included (transactions in witness blocks) and received (`before.inc` of the witness's Block stats:
  transactions it applied between blocks), over the loaded window and per `--window-blocks` blocks (default 10).
  The loaded window starts two blocks after the first witness block with transactions.
- **underfilled blocks**: loaded-window blocks below `--full-txs` (default 90% of `--tpb`; pass the block capacity
  for saturated runs).
- **skew-corrected offsets** (the #992 method): per node and run, skew is the median, over the idle blocks before
  the load, of the node's arrival offset minus the witness's ready offset; a block's corrected offset is the node's
  ready offset minus that skew. A node without 5 idle blocks (a late joiner) shows `-`. The report gives the idle
  blocks each skew used and flags nodes below 8 (`FEW IDLE BLOCKS`): over 28 earlier runs (111 nodes) the median of
  the first 8 idle blocks after the nodes were live was at most 0.78 ms (median 0.04 ms) off the median of all 27-30,
  which is why the default start delay now leaves about 8-9 idle blocks, not 27. On a netem-delayed link
  the idle transfer time (about three one-way delays: advertisement, request, block) lands in the skew too, so
  there the corrected offset shows only what the load adds.
- **per-thread CPU**: every thread of every node, by thread name, in % of a core over the loaded window.
- **send syscalls** (`--sends`): `sendto` / `sendmsg` from every witness thread, `write` / `writev` from `p2p` and
  `asio`, as calls/s and bytes per call.
- **relay latency**: per node, ms from a sampled transaction's first broadcast to its arrival there (its first
  `received`, or its `broadcast` when it logs none), from the `p2p_relay_sample` logger's
  `relay sample received|broadcast <trx id> <message hash> at <us>` lines, or the older measurement patch's
  `RLY BRD id=... t=...` / `RLY RCV id=... t=...` lines; both stamp the real clock, not the faked one. Samples are
  matched across nodes by transaction id, and the report counts the ids found on every node that logged samples.
  Builds without either show none, and the report says so.
- **p2p counters** (`p2p_stats`): per node, the reports whose whole 30 s period falls in the loaded window (located
  on the node's own clock by the log times of its Block stats lines), taken together: fetch candidates examined per
  item requested, items requested and loop wakes per second and the share of time spent scheduling; handoff
  submissions and completions per second (compare completions with received tx/s), in-flight maximum and mean,
  completion latency buckets and in-flight cap waits (times transaction fetching stopped at the cap, and for how long); socket writes per second with messages and bytes per write;
  transaction requests per second, transactions per request and how many hit the byte cap. `compare` shows them for
  the witness and the relays, and the witness's p2p CPU per received transaction.
- **fast-confirms**: per node, how many of the fast-confirms the witness produced for loaded-window blocks it accepted,
  and the delay from the witness's own acceptance to the node's, skew-corrected like the block offsets (a node
  without enough idle blocks shows no delay). One still on its way when the run ended counts as missing, so compare
  late blocks' shares with care. `compare` gives the fewest accepted share and the slowest median over the nodes.
- **head-LIB**: per node, the block number minus the last irreversible block in each loaded-window block's Block stats
  (the irreversible block before that block's own fast-confirms), as median and maximum; `compare` gives the largest.

## Harness rules

- **One series per host.** `run` and `ab` hold an flock on `/dev/shm/hive-p2p-load.lock` for the whole series
  and refuse while another checkout holds it, or while a `p2p-load-*` container or one of the old harnesses'
  `w877*` containers runs. The lock goes with its process; a killed run cannot leave it behind. A two-host series
  holds the lock of both hosts.
- **tmpfs only.** Node data lives in `/dev/shm`; never point the harness at the steem hosts' RAID disks.
- **A/B on one host, alternating.** Compare builds only within one host and one series (`ab`), never across
  hosts or days; the order alternates so drift in host load spreads over both builds. Results record the host.
- Don't drop caches while others' replays run, and leave other sessions' containers and directories alone.

## Colony load-generator caveat (#1076)

Colony subtracts every pending transaction it reapplies after a block, peers' in-transit ones included, from what it
produces next. Faster relay therefore lowers the offered load, and at 2500 tx/block the included tx/s measures
colony's throttle, not the node. Compare throughput saturated (`--tpb 20000`), or check the colonies' summed
`after.appl` before blaming hived; unsaturated star throughput means something only once #1076's fix is in the
build under test.
