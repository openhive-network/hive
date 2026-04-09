# Segfault in `pay_curators` During Replay - Summary

## Problem

hived crashes with a segfault during replay at ~24.9M blocks (24.65% of 101M) in `database::pay_curators()` when iterating the `comment_vote_index`. The crash is non-deterministic and only occurs on certain machines, suggesting a memory corruption issue. The problem appeared after the RocksDB comment storage implementation was introduced.

## Crash Point

`database_comment.cpp:159-162` -- read-only iteration of `comment_vote_index` (by_comment_voter):

```cpp
while( itr != cvidx.end() && itr->get_comment() == comment.get_id() )
{
  proxy_set.insert( &( *itr ) );
  ++itr;  // SEGFAULT in ordered_index_node_impl::increment
}
```

The red-black tree is already corrupted when this iteration begins.

## What RocksDB Comment Archive Changes

The RocksDB archive introduces two operations the other archives do not:

1. **`on_cashout`**: Creates a `volatile_comment_object` in chainbase shared memory (`db.create<volatile_comment_object>(...)`) for every cashed-out comment. The `memory_comment_archive` only stores an ID in a heap-allocated `std::map` -- no chainbase operations.

2. **`on_irreversible_block`**: Batch-removes 10,000+ `comment_object`s and 10,000+ `volatile_comment_object`s from chainbase in a single `_apply_block` call, plus flushes to RocksDB disk. The batching threshold (`volatile_objects_limit = 10,000`) means these removals are infrequent but massive.

Additionally, the RocksDB database is opened with `options.IncreaseParallelism()`, spawning background compaction threads that run concurrently with chain processing.

## Most Likely Root Cause

The batch removal in `on_irreversible_block` causes heavy churn on the **shared segment manager** (the allocator backing all chainbase pool allocators). When 10,000+ blocks are released from the `comment_object` and `volatile_comment_object` pools back to the segment manager, its internal free list is heavily modified. A subsequent allocation from a different pool (e.g., `comment_vote_object` during the next block's `process_comment_cashout`) draws from this recently-churned free list. A corruption in the segment manager's free list could cause memory to be misallocated, corrupting the `comment_vote_index` tree.

This explains:
- Why the crash is in `comment_vote_index` (different pool), not in `comment_index` or `volatile_comment_index`
- Why it only happens on some machines (segment manager layout varies with allocation history)
- Why it appeared with the RocksDB archive (it introduces the `volatile_comment_object` pool and the heavy batch removal pattern)

## Recommended Investigation Steps

1. **Disable RocksDB archive during replay** -- use `placeholder_comment_archive` to confirm the crash disappears.
2. **Reduce `volatile_objects_limit`** from 10,000 to 1 (process immediately) -- if the crash disappears, the batch removal pattern is the trigger.
3. **Disable RocksDB background threads** -- remove `IncreaseParallelism()` to eliminate concurrent I/O interference.
4. **Switch to `remove_no_undo`** in `on_irreversible_block` (as `memory_comment_archive` does).
5. **Build with `-fsanitize=address`** and replay to detect out-of-bounds access or use-after-free.
6. **Add tree validation** before/after `on_irreversible_block` to pinpoint when corruption occurs.

## Key Files

| File | Role |
|------|------|
| `libraries/chain/database_comment.cpp:129-195` | `pay_curators()` -- crash site |
| `libraries/chain/database_comment.cpp:366-372` | Vote removal in `cashout_comment_helper()` |
| `libraries/chain/database_comment.cpp:379-527` | `process_comment_cashout()` -- outer loop |
| `libraries/chain/external_storage/rocksdb_comment_archive.cpp:35-63` | `on_cashout()` -- creates volatile_comment_object |
| `libraries/chain/external_storage/rocksdb_comment_archive.cpp:100-141` | `on_irreversible_block()` -- batch removal |
| `libraries/chain/external_storage/rocksdb_storage_provider.cpp:44-48` | RocksDB options with `IncreaseParallelism()` |
| `libraries/chainbase/include/chainbase/pool_allocator.hpp:218-292` | Pool allocator with segment manager interaction |
