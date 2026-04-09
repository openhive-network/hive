# Crash Reproduction on steemdev3 (sd3)

**Date**: 2026-04-01
**Server**: steemdev3.pl.syncad.com (syncad@steemdev3)
**Working dir**: /storage1/mario/mt-random-crash

## Build

- Source: /storage1/mario/00.src/hive (branch: mt-random-crash)
- Compiler: clang-15 (Ubuntu 22.04)
- Build type: Asan (AddressSanitizer + CHAINBASE_POISON_FREED_MEMORY)
- Build dir: /storage1/mario/00.src/hive/build_asan

## Replay Command

```bash
ASAN_OPTIONS="halt_on_error=1:print_stats=1:detect_leaks=0:log_path=/storage1/mario/mt-random-crash/asan_errors"

/storage1/mario/00.src/hive/build_asan/programs/hived/hived \
    --data-dir=/storage1/mario/mt-random-crash/datadir \
    --replay-blockchain \
    --exit-at-block 25000000 \
    --shared-file-size 24G \
    --plugin=chain \
    --plugin=p2p
```

Block log: /storage1/htm-mainnet/haf-datadir/blockchain (symlinked)

## Crash

- **Last successful checkpoint**: block 9,900,000 (39.6%) at 14:45:02 UTC
- **Crash time**: ~14:47 UTC (approximately block 9,950,000)
- **Replay duration**: ~2h10m
- **ASAN errors**: ZERO — no AddressSanitizer violations detected
- **Poison detection**: ZERO — no use-after-free at pool allocator level

## Stack Trace

```
SEGFAULT:
  fc::segfault_handler(int, siginfo_t*, void*)
  boost::multi_index::detail::ordered_index_node_impl::rebalance_for_extract()
      ↑ Red-black tree rebalancing during node removal
  multi_index_container<comment_vote_object>::erase_()
  index_base<comment_vote_object>::final_erase_()
  ordered_index_impl<by_id, comment_vote_object>::erase()
  generic_index<comment_vote_index>::remove()
  database::remove<comment_vote_object>()
  database::cashout_comment_helper()
  database::process_comment_cashout()
  database::_apply_block()
  [replay infrastructure...]
```

## Analysis

### Comparison with previous attempts

| Server | Block at crash | ASAN findings | Reproduced? |
|--------|---------------|---------------|-------------|
| container (hive-4.pl.syncad.com) | N/A | 0 errors in 25M blocks | NO |
| steemdev3 (sd3) | ~9,950,000 | 0 errors | YES |
| Original report | ~24,900,000 | N/A | YES |

### Key Insights

1. **ASAN cannot detect this bug** — The corruption occurs in mmap'd shared memory managed by boost::interprocess segment manager. ASAN only instruments heap allocations (malloc/free). The segment manager has its own allocator that ASAN knows nothing about.

2. **Pool-level poison didn't help** — CHAINBASE_POISON_FREED_MEMORY fills freed pool chunks with 0xDE, but the corrupted tree node pointers don't point to freed pool memory. They point to completely invalid locations, suggesting corruption at the segment manager level (below the pool allocator).

3. **Crash is in rebalance_for_extract, not increment** — Unlike the original report (which crashed during tree traversal in pay_curators), this crash occurs during tree rebalancing when REMOVING a comment_vote_object in cashout_comment_helper. This means the tree structure is already corrupted by the time the erase happens.

4. **Non-deterministic block number** — ~10M on sd3 vs ~24.9M on original machine. The crash depends on:
   - Memory layout (determined by machine/OS/allocator)
   - When the segment manager's internal state becomes corrupted
   - Which tree traversal/modification first encounters the corrupted node

5. **Crash occurs during process_comment_cashout, not on_irreversible_block** — The batch removal in on_irreversible_block is still the likely CAUSE (corrupts segment manager), but the crash MANIFESTS later during the next cashout operation that touches the corrupted comment_vote_index.

### Remaining questions

1. Does the crash happen at the same block on sd3 if we replay again? (Deterministic per-machine?)
2. Would reducing the volatile_objects_limit in rocksdb_comment_archive prevent the crash?
3. Would using placeholder_comment_archive (no RocksDB) prevent the crash?
4. Can we add segment manager validation to pinpoint the exact corruption moment?

## Log Files

- Replay log: /storage1/mario/mt-random-crash/asan-replay.log
- Replay output: /storage1/mario/mt-random-crash/replay_output.log
- Data dir: /storage1/mario/mt-random-crash/datadir
