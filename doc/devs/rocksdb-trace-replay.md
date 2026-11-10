# Comparing RocksDB configurations on hived's own access pattern

A hived replay measures RocksDB only as a small part of the whole work, and takes hours per
configuration. Instead, hived can record the operations one of its RocksDB databases performs over a
block range (RocksDB's own operation tracing), and `db_bench` can replay that recording on a copy of
the database as it was when the recording started, under any set of RocksDB options. The replay
does nothing but RocksDB work, takes minutes, and repeats exactly.

Three parts:

1. hived records the trace and a checkpoint of the database (`comments-rocksdb-trace-dir`,
   `comments-rocksdb-trace-blocks`);
2. `hive_db_bench` is RocksDB's `db_bench` able to open hived's databases (`scripts/build_hive_db_bench.sh`);
3. `scripts/rocksdb_trace_replay.py` replays a trace under several configurations and reports, per
   configuration, the replay time, read and write latency percentiles and the database size.

## 1. Recording a trace

Set both options (config.ini or command line):

```
comments-rocksdb-trace-dir = rocksdb-traces
comments-rocksdb-trace-blocks = 20000000-21000000
```

The trace starts when the last irreversible block reaches the first block of the range and ends
when it reaches the last one. In a replay every block becomes irreversible as it is applied, so the
trace covers the archive's work for the blocks in the range. If hived stops before the end of the
range (interrupt, `--exit-at-block`, a fatal error that closes the database), the trace is ended
at that point; a node that starts already past the range traces nothing. A process records one
trace at most.

A relative directory is relative to `shared-file-dir`. Everything goes into `<dir>/COMMENT/`
(`COMMENT` is the database's name), which must not exist yet - hived refuses to start otherwise:

| file              | contents |
|-------------------|----------|
| `checkpoint/`     | the comment archive exactly as the first traced operation found it (a RocksDB checkpoint) |
| `trace`           | the RocksDB trace file |
| `trace_info.json` | database name, requested range, `start_block` / `end_block` actually traced, `ended` and `end_reason`, RocksDB version |

`ended` becomes true only once `EndTrace` succeeded; a trace that is not marked ended was cut off
(for example hived was killed) and replays only up to that point.

Keep the trace directory on the archive's file system: the checkpoint then hard links the SST
files instead of copying them. Starting the trace flushes the archive's memtables once (the
checkpoint needs them on disk). The trace file grows with every read and write by about the size of
the keys and values involved; RocksDB stops tracing at its default limit of 64 GB.

Example: trace blocks 20M-21M of a mainnet replay, stopping right after the range:

```
hived -d <data dir> --replay-blockchain --exit-at-block=21000000 \
  --comments-rocksdb-trace-dir=rocksdb-traces --comments-rocksdb-trace-blocks=20000000-21000000
```

The trace is made in the comment archive's `rocksdb_storage_provider`, through
`rocksdb_storage_provider::enableTrace()` (see `rocksdb_trace.hpp`), and follows the last
irreversible block that `update_lib` receives, so any other database built on
`rocksdb_storage_provider` can be traced the same way by enabling it before the database opens.

## 2. Building hive_db_bench

```
scripts/build_hive_db_bench.sh [<build dir>]
```

builds `programs/util/hive_db_bench` in a build directory of its own (default `build-db-bench`).
The CMake option behind it, `HIVE_BUILD_DB_BENCH`, is off by default: it compiles RocksDB from
source (not the preinstalled one) and needs gflags, which it fetches (v2.2.2) when CMake does not
find it installed. gflags is linked into `hive_db_bench` only, never into hived.

Stock `db_bench` cannot replay on hived's databases: it opens column families named
`column_family_name_<N>`, and hived's column families have their own names and comparators
(`hive::chain::HashComparator` for the comment archive) unknown to RocksDB. `hive_db_bench` is
`db_bench` with hive's comparators registered and, through db_bench's `ToolHooks`, opens an
existing database with its own column families, each with the comparator its OPTIONS file names.
Pass `-num_column_families=<the database's count>` (the harness does). All `db_bench` flags work as
usual; every column family gets the options of the first column family of `-options_file`.

## 3. Replaying under several configurations

```
scripts/rocksdb_trace_replay.py <data dir>/blockchain/rocksdb-traces/COMMENT scripts/rocksdb_trace_configs \
  --db-bench build-db-bench/programs/util/hive_db_bench --json results.json --drop-caches
```

For each configuration it copies the checkpoint into `--work-dir` (default `<trace dir>/replays`,
SST files hard linked) and writes the options file. Table options (block size, filter, index,
compression, format_version) only apply to SST files RocksDB writes, so it then rewrites the copy
under the configuration:

```
hive_db_bench -benchmarks=compact -use_existing_db=true -db=<copy> -options_file=<options> \
  -num_column_families=<N> -cache_type=lru_cache -cache_size=1073741824 [<[db_bench] flags>]
```

`compact` rewrites the default column family, the one the comment archive keeps its comments in,
down to the last level, so every configuration, the baseline too, replays on a fully compacted
database rather than on the shape hived's archive had. `--no-rewrite` skips this step and replays
on the files as hived wrote them; then only the files written during the replay, and options not
stored in files (the block cache), follow the configuration. Then, in a second `db_bench` process
(a cold block cache, and with `--drop-caches` a cold page cache), it replays the trace:

```
hive_db_bench -benchmarks=replay -use_existing_db=true -db=<copy> -trace_file=<trace> \
  -options_file=<options> -num_column_families=<N> -trace_replay_fast_forward=1e9 \
  -trace_replay_threads=1 -statistics=true -cache_type=lru_cache -cache_size=1073741824 [<[db_bench] flags>]
```

and reports the time of the rewrite, the replay time (`db_bench`'s own, without opening the database; `wall s` includes it),
the latency percentiles of gets, multigets and writes from RocksDB's statistics, and the size of
the database (all files, and SST files alone) after the replay. The copy is deleted afterwards
unless `--keep`; the `db_bench` logs and the options file stay in `<work dir>/<configuration>/`.
`--json` saves everything, including `trace_info.json`. A configuration that fails (for example
LZ4 in a build without it) is reported as failed and the others still run.

The replay runs on one thread with no pauses, so it measures RocksDB's cost for the chain thread's
sequence of operations. Pass `--drop-caches` (as root) to start every configuration with a cold page
cache; otherwise the configurations after the first find the hard linked SST files already cached.

### Configuration files

A configuration file is a fragment of a RocksDB OPTIONS file applied on top of the checkpoint's own
OPTIONS file, which records the options hived ran with:

```
[TableOptions/BlockBasedTable]
format_version = 7
block_size = 32768

[CFOptions]
compression = kNoCompression

[db_bench]
cache_size = 4294967296
```

`[CFOptions]` and `[TableOptions/BlockBasedTable]` without a name apply to every column family,
`[CFOptions "default"]` to one; `[DBOptions]` to the database. `[db_bench]` sets `db_bench` flags;
the block cache is set there (`cache_size`, `cache_type`) because `db_bench` replaces the block
cache of the options file with its own.

`scripts/rocksdb_trace_configs/` seeds the comparison. `baseline` is hived's configuration -
format_version 6, 4 KiB blocks, 10 bit bloom filter, no partitioned index or filters, 1 GiB LRU
block cache, Snappy - and each other file changes one thing:

| file | change |
|------|--------|
| `format_version_7.ini` | format_version 7 |
| `block_size_16k.ini`, `block_size_32k.ini` | 16 / 32 KiB blocks |
| `ribbon_filter.ini` | ribbon filter instead of bloom |
| `partitioned_index_filters.ini` | two level index, partitioned filters |
| `block_cache_256m.ini`, `block_cache_4g.ini` | 256 MiB / 4 GiB block cache |
| `compression_none.ini`, `compression_lz4.ini` | no compression / LZ4 (fails where RocksDB is built without LZ4, as hive's is) |

## What RocksDB costs hived in total

The `baseline` replay time with `--no-rewrite` is RocksDB's own time for the traced blocks; hived's
log gives the time the replay took for the same blocks (the `... of ... blocks` progress lines), so
their ratio is RocksDB's share of that range. To see what the comment archive costs a replay as a
whole, replay the same range with `comment-archive = MEMORY` and with `comment-archive = ROCKSDB`
and compare the times (`comment-archive` exists in testnet and mirrornet builds only).
