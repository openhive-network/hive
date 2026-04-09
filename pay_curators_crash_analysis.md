# Segfault in `pay_curators` During Replay - Analysis

## Crash Location

The segfault occurs in `database::pay_curators()` at `database_comment.cpp:159-162` during a **read-only iteration** of the `comment_vote_index` (by_comment_voter):

```cpp
const auto& cvidx = get_index<comment_vote_index, by_comment_voter>();
auto itr = cvidx.lower_bound( comment.get_id() );
while( itr != cvidx.end() && itr->get_comment() == comment.get_id() )
{
  proxy_set.insert( &( *itr ) );
  ++itr;  // <- SEGFAULT: boost red-black tree node increment hits invalid memory
}
```

The `ordered_index_node_impl::increment()` follows parent/left/right pointers in the red-black tree to find the next in-order node. The crash means **the tree structure is already corrupted** when this iteration begins.

## Stack Trace

```
boost::multi_index::detail::ordered_index_node_impl::increment
hive::chain::database::pay_curators
hive::chain::database::cashout_comment_helper
hive::chain::database::process_comment_cashout
hive::chain::database::_apply_block
hive::chain::database::apply_block
...
hive::chain::block_log::for_each_block
hive::chain::block_log_wrapper::process_blocks
hive::plugins::chain::detail::chain_plugin_impl::reindex_internal
```

Crash occurs at ~24.9M blocks during replay (24.65% of 101M). This is ~3.3M blocks after HF19 activation (~21.6M), which is when comment cashout with vote removal and comment archival begins.

## Code Flow

`process_comment_cashout()` (line 464) iterates over all comments ready for payout in a single block:

```
for each comment C ready for cashout:
  1. cashout_comment_helper(C):
     a. pay_curators(C)          -> iterates comment_vote_index, collects raw pointers
     b. pay author/beneficiaries -> calls create_vesting2(), modify(), virtual ops
     c. REMOVE all votes for C   -> lines 366-372: erase from comment_vote_index
  2. on_cashout(C)               -> archives comment (RocksDB: creates volatile_comment_object)
  3. remove(comment_cashout C)   -> removes cashout object
  _current = cidx.begin()        -> restart cashout iteration
```

Then at end of `_apply_block`:
```
4. migrate_irreversible_state():
   a. commit(new_lib)            -> commits undo state
   b. on_irreversible_block()    -> RocksDB: batch-removes comment_objects + volatile_comment_objects
```

## Primary Suspect: RocksDB Comment Archive

The problem did not occur before the RocksDB comment storage implementation. This is the strongest clue. The analysis below focuses on what this feature changes.

### Three Comment Archive Implementations

| | `placeholder_comment_archive` | `memory_comment_archive` | `rocksdb_comment_archive` |
|---|---|---|---|
| `on_cashout()` | No-op (counts) | Stores `comment_id` in `std::map` | **Creates `volatile_comment_object` in chainbase** |
| `on_irreversible_block()` | No-op | `db.remove_no_undo(comment)` | **`db.remove(comment)` + `db.remove(volatile)` + RocksDB I/O** |
| Chainbase impact | None | Removes comment_objects | Creates + removes volatile_comment_objects AND removes comment_objects |
| Batching | N/A | Per-block | **Batched: waits for 10,000+ volatile objects** |

### What RocksDB Archive Changes in the Block Processing

#### 1. Additional chainbase object creation during `process_comment_cashout`

`rocksdb_comment_archive::on_cashout()` (rocksdb_comment_archive.cpp:35-63) creates a `volatile_comment_object` in chainbase shared memory for EVERY cashed-out comment:

```cpp
db.create< volatile_comment_object >( [&]( volatile_comment_object& o )
{
    o.comment_id = _comment.get_id();
    o.parent_comment = _comment.get_parent_id();
    o.depth = _comment.get_depth();
    o.set_author_and_permlink_hash( _comment.get_author_and_permlink_hash() );
    o.block_number = _block_num;
});
```

This is called BETWEEN cashout of comment A and comment B (after `cashout_comment_helper(A)` returns, before `cashout_comment_helper(B)` starts). The `db.create` allocates from the `volatile_comment_object` pool allocator, which may request new memory blocks from the shared segment manager.

The `memory_comment_archive` does NOT touch chainbase in `on_cashout` -- it only stores an ID in a `std::map` on the heap.

#### 2. Batch removal during `migrate_irreversible_state`

`rocksdb_comment_archive::on_irreversible_block()` (rocksdb_comment_archive.cpp:100-141) is called at the END of `_apply_block`, within `migrate_irreversible_state`. It has a **batching threshold** of 10,000 volatile objects:

```cpp
if( _volatile_idx.size() < volatile_objects_limit )  // volatile_objects_limit = 10,000
    return;  // accumulate more before processing
```

When triggered, it processes ALL accumulated volatile objects in one go:

```cpp
while( _itr != _volatile_idx.end() && _itr->block_number <= block_num )
{
    move_to_external_storage_impl( block_num, _current );  // serialize + buffer to RocksDB
    const auto* _comment = db.find_comment( _current.comment_id );
    FC_ASSERT( _comment );
    db.remove( *_comment );    // removes from comment_index (2 trees)
    db.remove( _current );     // removes from volatile_comment_index (3 trees)
    ++count;
}
provider->flushWriteBuffer();  // flush batch to RocksDB disk
```

This means a SINGLE `_apply_block` call can trigger removal of **10,000+ comment_objects and 10,000+ volatile_comment_objects** from chainbase, causing massive churn on the shared segment manager.

#### 3. RocksDB background threads

The RocksDB database is opened with `options.IncreaseParallelism()` (rocksdb_storage_provider.cpp), which creates background compaction threads (typically `hardware_concurrency` threads). These threads run concurrently with chain processing.

While RocksDB threads don't directly access chainbase shared memory, they:
- Perform disk I/O on the same filesystem
- Allocate heap memory via the system allocator
- Could affect page cache behavior on the system

### During Replay: Key Timing

During replay (`reindex_internal`), `skip_undo_block` is set (chain_plugin.cpp:1225-1227):
- Every block is immediately irreversible (`set_last_irreversible_block_num(head_block_num())`)
- **No undo sessions** are created (apply_block_extended:2132-2134)
- `on_irreversible_block` is called for EVERY block
- `db.remove()` and `db.remove_no_undo()` behave identically (no undo to track)
- The 10,000 volatile object batch threshold means ~every 3-5 blocks (depending on comment density), a massive batch removal occurs

### Hypothesis: Segment Manager Corruption from Heavy Churn

The pool allocator (`pool_allocator.hpp`) manages typed memory blocks within the shared mmap'd file. When objects are deallocated and a block becomes fully empty, the block is **released back to the segment manager** (line 277: `block_index.erase(*block)`). This causes the `bip::set<block_t>` (interprocess set in shared memory) to restructure AND the segment manager to reclaim the memory.

When the batch removal in `on_irreversible_block` releases thousands of blocks from BOTH the `comment_object` pool and `volatile_comment_object` pool, the segment manager's free list undergoes heavy modifications. If a new allocation is then requested from a DIFFERENT pool (e.g., `comment_vote_object` during the next block's `process_comment_cashout`), the segment manager provides memory from its recently-modified free list.

The **segment manager's free list is a shared data structure across ALL pools**. Heavy churn on it from one pool could corrupt it in a way that affects allocations for another pool. This would explain:
- Why the crash is in `comment_vote_index` (different pool) not in `comment_index` or `volatile_comment_index`
- Why it only happens on some machines (memory layout sensitivity)
- Why it appeared with the RocksDB archive (it introduces the `volatile_comment_object` pool + heavy batch removal pattern)

### Alternative Hypothesis: RocksDB Background Thread Interference

`IncreaseParallelism()` spawns background compaction threads. While these don't directly access chainbase:
- RocksDB's `flushWriteBuffer()` (called from `on_irreversible_block`) triggers synchronous writes to disk, which may interact with the OS page cache
- Background compaction runs concurrently and does heavy I/O
- On some machines/filesystems, this could interfere with chainbase's mmap'd file consistency (especially on ZFS, ref: commit `4e3352a7f`)

## Why It Only Manifests on Certain Machines

1. **Shared memory layout**: The segment manager's internal free list structure depends on allocation history. Different machines start from different snapshots, leading to different memory layouts.
2. **Filesystem behavior**: RocksDB I/O + chainbase mmap interact differently on ext4 vs ZFS vs XFS (page cache, write-back, fsync semantics).
3. **Memory pressure**: Machines with less RAM may experience more page eviction, causing mmap'd pages to be re-read from disk and potentially exposing stale data.
4. **CPU/timing**: RocksDB background compaction thread scheduling varies across machines.

## Key Files

| File | Lines | Role |
|------|-------|------|
| `libraries/chain/database_comment.cpp` | 129-195 | `pay_curators()` - crash site |
| `libraries/chain/database_comment.cpp` | 197-377 | `cashout_comment_helper()` - removes votes after paying |
| `libraries/chain/database_comment.cpp` | 379-527 | `process_comment_cashout()` - outer loop |
| `libraries/chain/external_storage/rocksdb_comment_archive.cpp` | 35-63 | `on_cashout()` - creates volatile_comment_object |
| `libraries/chain/external_storage/rocksdb_comment_archive.cpp` | 100-141 | `on_irreversible_block()` - batch removal |
| `libraries/chain/include/hive/chain/external_storage/comment_rocksdb_objects.hpp` | 19-61 | volatile_comment_object + index definition |
| `libraries/chain/include/hive/chain/detail/state/comment_object_multiindex.hpp` | 8-27 | comment_vote_index definition |
| `libraries/chainbase/include/chainbase/pool_allocator.hpp` | 218-292 | Pool allocator alloc/dealloc with segment manager interaction |
| `libraries/chain/external_storage/rocksdb_storage_provider.cpp` | 44-48 | RocksDB options with `IncreaseParallelism()` |
| `libraries/chain/block_log.cpp` | 922-1000 | `for_each_block` with I/O thread + worker pool |

## Recommended Investigation Steps

1. **Disable RocksDB archive during replay**: Use `placeholder_comment_archive` or `memory_comment_archive` to confirm the crash disappears. This validates the RocksDB connection.

2. **Reduce batch size**: Lower `volatile_objects_limit` from 10,000 to 1 (process immediately) to see if the batch removal is the trigger. If crash disappears, the heavy segment manager churn is the culprit.

3. **Instrument segment manager**: Add checks around segment manager allocate/deallocate to detect corruption (e.g., write canary values at block boundaries).

4. **Disable RocksDB background threads**: Set `options.SetMaxBackgroundJobs(0)` or use `options.env = rocksdb::Env::Default()` without parallelism to eliminate background thread interference.

5. **Try `remove_no_undo` in RocksDB archive**: Change `db.remove(*_comment)` and `db.remove(_current)` to `db.remove_no_undo()` in `on_irreversible_block` (as `memory_comment_archive` does). During replay, this should be equivalent, but it avoids any undo path code.

6. **Add tree validation**: Insert `comment_vote_index` consistency checks before and after `on_irreversible_block` to pinpoint when corruption occurs.

7. **Address sanitizer**: Build with `-fsanitize=address` and replay to detect out-of-bounds access or use-after-free in the pool allocator or segment manager.

8. **Monitor segment manager free list**: Log segment manager free memory before/after the batch removal in `on_irreversible_block` to detect anomalies.
