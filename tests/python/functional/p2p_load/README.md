# p2p load harness

Floods a small testnet with colony transactions over a chosen p2p topology and measures how the nodes keep up:
included and received tx/s, underfilled blocks, skew-corrected block offsets, per-thread CPU, send syscalls and
relay latency. It is the in-repo replacement of the `w877`, `w877x` and `w1075` host directories behind hive#877
and its follow-ups (#992, #1060-#1077); reports cite a commit of this directory instead of a host path.

## What it needs

- A Linux host with Docker, `python3` (3.11 or newer) and `/dev/shm` large enough for every node's state
  (4 GiB shared file per node; about 1-2 GiB of it is used).
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
4. the driver creates the nodes (`witness`, `observer`, `colony0..`, `relay1..`), wires the topology, replays the
   block log on every node, checks each node's `config.ini`, starts them together under libfaketime from the
   block log's head time, samples every 3 s and waits until `--blocks` blocks after the colonies started;
5. copies back only logs and configs (`OUT_ROOT/NAME/generated/<node>/`, no `blockchain/`, no `*.bin`) and removes
   the tmpfs directory, whatever happened. A disk-backed 8-peer run wrote 90-415 GB; this one writes megabytes.

`OUT_ROOT/NAME` then holds `spec.json` (what was asked), `out.json` (roles, binaries, p2p parameters per node,
blocks, `/proc` samples, link maps), `driver.log`, `generated/`, and `tc.txt` / `sends.txt` when used.
`OUT_ROOT/series.log` records each run's start and end with host, exit code and load average.

Options:

| option | meaning |
|---|---|
| `--topology mesh` | every node seeded to the witness |
| `--topology star` | the witness seeds every colony; the observer seeds the witness |
| `--topology strict` | star, but each colony and the observer keep only that one link (`p2p-max-connections = 1`) |
| `--topology chain` | colony0 -> relay1 -> ... -> relayN -> witness -> observer; connection limits leave peer exchange no shortcut |
| `--tpb N` | colony transactions per block, split evenly over the colonies |
| `--colonies N`, `--relays N` | relays exist only in `chain`, which has one colony |
| `--blocks N` | loaded blocks measured, counted from the colonies' start (default 30); `--colony-start-delay` idle blocks (default 30) come first |
| `--node-hived ROLE=PATH` | a different hived for one node (mixed versions); the others use `--hived` |
| `--flush-us`, `--fetch-us`, `--inflight`, `--p2p-parameter KEY=JSON` | p2p parameters, see below |
| `--late-observer K` | the observer starts K blocks after the colonies start, its faked clock at the others' current fake time |
| `--netem-ms D --netem-colonies 0,2` | delay each packet of those colonies' links by D ms each way (RTT + 2D) |
| `--sends S` | after 6 loaded blocks, count the witness's send syscalls per thread with bpftrace for S seconds |
| `--mix large` | colony replies and articles only, 4000..12000 byte bodies (about 255 transactions fill a 2 MiB block) |

### p2p parameters: only what is given, only where the binary has it

An option that is not given is not written: the node runs its binary's default. A given option is written into
`p2p-parameters` only on nodes whose binary knows the key, read from that binary's `hived --help` default; a build
that predates an option gets no key for it (the report names what was left out). After the replay, the driver
compares each node's `config.ini` with what it meant to set and fails the run on any difference. `out.json`
records the given, applied and left-out parameters and every binary's defaults. This replaces the old drivers'
"set only when non-zero" rule, which ran a "delay 0" run at the build's 5 ms default once the default changed.

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
# mixed versions: an older witness among current nodes
$P run mixed $COMMON --hived $HIVED --node-hived witness=$OLD_HIVED --topology strict --colonies 8 --tpb 20000
# late joiner: the observer joins 10 loaded blocks late and catches up under load
$P run late $COMMON --hived $HIVED --topology strict --colonies 8 --tpb 2500 --late-observer 10
```

### A/B builds

`p2p-load ab NAME --build A=PATH --build B=PATH --reps R [run options]` runs A, B, A, B, ... (`NAME-A-1`,
`NAME-B-1`, `NAME-A-2`, ...) under one host lock and prints each run's report, then the comparison table: the
mean of each headline metric per build with the per-run values. For example #1073 against its parent `fb7e4d96`
on the 3-hop chain at 1000 tx/block (#1075 measured a colony-to-relay1 median of 51 against 134 ms):

```
$P ab chain3 $COMMON --build cand=$B1073/hived --build base=$B_FB7E4D96/hived --topology chain --relays 2 --tpb 1000 --reps 2
```

## Reports

`p2p-load report RUN_DIR...` (`--json` for machine-readable) and `p2p-load compare RUN_DIR...` (grouped by label)
work on any finished run directory and need no node and no test-tools.

- **tx/s**: included (transactions in witness blocks) and received (`before.inc` of the witness's Block stats:
  transactions it applied between blocks), over the loaded window and per `--window-blocks` blocks (default 10).
  The loaded window starts two blocks after the first witness block with transactions.
- **underfilled blocks**: loaded-window blocks below `--full-txs` (default 90% of `--tpb`; pass the block capacity
  for saturated runs).
- **skew-corrected offsets** (the #992 method): per node and run, skew is the median, over the idle blocks before
  the load, of the node's arrival offset minus the witness's ready offset; a block's corrected offset is the node's
  ready offset minus that skew. A node without 5 idle blocks (a late joiner) shows `-`. On a netem-delayed link
  the idle transfer time (about three one-way delays: advertisement, request, block) lands in the skew too, so
  there the corrected offset shows only what the load adds.
- **per-thread CPU**: every thread of every node, by thread name, in % of a core over the loaded window.
- **send syscalls** (`--sends`): `sendto` / `sendmsg` from every witness thread, `write` / `writev` from `p2p` and
  `asio`, as calls/s and bytes per call.
- **relay latency**: per node, ms from a sampled transaction's first broadcast to its arrival there, from
  `RLY BRD id=... t=...` / `RLY RCV id=... t=...` lines (t in microseconds of a clock shared by the nodes). Builds
  of this tree do not log them yet: the permanent p2p counters of hive#1079 are to provide them, and the report
  says so when a run has none. Until then the relay part of the A/B example above needs builds that log them.

## Harness rules

- **One series per host.** `run` and `ab` hold an flock on `/dev/shm/hive-p2p-load.lock` for the whole series
  and refuse while another checkout holds it, or while a `p2p-load-*` container or one of the old harnesses'
  `w877*` containers runs. The lock goes with its process; a killed run cannot leave it behind.
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
