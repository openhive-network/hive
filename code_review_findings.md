# Chainbase & Chain Code Review — Findings Summary

**Date**: 2026-04-01
**Context**: Investigating non-deterministic segfault in `database::pay_curators()` during replay at ~24.9M blocks. Crash occurs in red-black tree iteration of `comment_vote_index`. Only reproducible with RocksDB comment archive, machine-dependent.

---

## CRITICAL Findings

### C1: Batch removal in `rocksdb_comment_archive::on_irreversible_block` causes segment manager churn

**File**: `libraries/chain/external_storage/rocksdb_comment_archive.cpp:100-141`

When `volatile_objects_limit` (10,000) objects accumulate, the loop removes 10,000+ `comment_object`s and 10,000+ `volatile_comment_object`s in a single call. Each removal calls `return_object_memory()` in the pool allocator. When entire blocks of BLOCK_SIZE objects are freed, the blocks are released back to the segment manager via `block_index.erase(*block)`. This causes massive churn on the segment manager's free list, which is **shared across ALL pool allocators**.

On the very next block, `process_comment_cashout()` creates/accesses `comment_vote_object`s from a *different* pool that draws from the same recently-churned segment manager. A corruption in the segment manager's free list would manifest as corrupted red-black tree nodes in the `comment_vote_index` — exactly matching the crash in `pay_curators`.

The `memory_comment_archive` processes comment-by-comment (no batching threshold). The `placeholder_comment_archive` does nothing. **Only the RocksDB archive has this 10,000-object batch pattern.**

---

## HIGH Findings

### H1: `rocksdb_comment_archive` uses `db.remove()` instead of `db.remove_no_undo()`

**File**: `libraries/chain/external_storage/rocksdb_comment_archive.cpp:131-132`
**Compare**: `libraries/chain/external_storage/memory_comment_archive.cpp:44` (uses `remove_no_undo`)

```cpp
db.remove( *_comment );   // should be remove_no_undo
db.remove( _current );    // should be remove_no_undo
```

During live sync, `on_irreversible_block` is called from `migrate_irreversible_state` AFTER `commit(new_last_irreversible)`. If there is still an active undo session for the head block, `db.remove()` will create undo records for objects being irreversibly removed — this is logically wrong and wastes memory. The undo record creation involves allocation from the undo stack's pool allocator, adding **additional segment manager pressure** during the batch removal. The `memory_comment_archive` correctly uses `remove_no_undo`.

### H2: `volatile_comment_object` creation during `process_comment_cashout` adds segment manager pressure

**File**: `libraries/chain/external_storage/rocksdb_comment_archive.cpp:51-59`

`db.create<volatile_comment_object>()` is called from `process_comment_cashout()` via `on_cashout()`, interleaved with `pay_curators()` calls. Each creation may trigger a new block allocation from the segment manager if the pool's current block is full. This allocation happens BETWEEN cashout_comment_helper calls.

The `memory_comment_archive` does NOT touch chainbase in `on_cashout` — it only appends to a `std::map` on the heap. The `placeholder_comment_archive` does nothing.

This interleaving of chainbase allocations from a different pool during the cashout loop is **unique to the RocksDB archive** and directly precedes the `pay_curators` crash site.

### H3: Nested `modify()` on same Boost.Multi_Index element — undefined behavior

**File**: `libraries/chain/database.cpp:1013-1021, 3125-3134, 1548-1568`

Multiple locations call `rc().regenerate_rc_mana()` or `rc().update_account_after_vest_change()` from inside a `modify()` lambda on the same account object. Both RC functions internally call `db.modify(account, ...)` on the same element. Boost.Multi_Index does not support reentrant/nested `modify()` on the same element.

```cpp
// database.cpp:1013-1021
modify( to_account, [&]( account_object& a )
{
    if( has_hardfork( HIVE_HARDFORK_0_20 ) )
    {
        util::update_manabar( cprops, a, new_vesting.get_amount() );
        rc().regenerate_rc_mana( to_account, _now ); // calls db.modify(to_account, ...) internally!
    }
    a.vesting_shares += new_vesting;
} );
```

### H4: Dangling reference dereference in `generic_index::modify` when uniqueness constraint fails

**File**: `libraries/chainbase/include/chainbase/chainbase.hpp:469-471`

```cpp
auto ok = _indices.modify( itr, safe_modifier);
if constexpr( value_type::has_dynamic_alloc_t::value )
    new_size = obj.get_dynamic_alloc();  // obj may be erased if ok==false
```

When `boost::multi_index::modify()` returns false, it erases the element. The code then dereferences the now-dangling `obj` reference to read `get_dynamic_alloc()` before checking the return value. This is use-after-free / undefined behavior.

---

## MEDIUM Findings

### M1: Fabricated `block_t` object used for `lower_bound` lookup — formal UB

**File**: `libraries/chainbase/include/chainbase/pool_allocator.hpp:259-260`

```cpp
block = reinterpret_cast<block_t*>(object);
const auto found = block_index.lower_bound(*block);
```

A `T*` pointer is reinterpret-cast to `block_t*` and then dereferenced. This creates a fake `block_t` reference from memory that is not actually a `block_t` object. It works because `chunks` is at offset 0, but this is formally undefined behavior (violation of object lifetime rules).

### M2: `FC_ASSERT` with `&&` string literal — condition is always true

**File**: `libraries/chain/database.cpp:3407`

```cpp
FC_ASSERT( symbol.asset_num == HIVE_ASSET_NUM_HBD && "invalid symbol" );
```

The string literal `"invalid symbol"` always evaluates to `true`, so this assertion **never fires**. Should be:

```cpp
FC_ASSERT( symbol.asset_num == HIVE_ASSET_NUM_HBD, "invalid symbol" );
```

### M3: `migrate_irreversible_state` partial state corruption risk

**File**: `libraries/chain/database.cpp:3067-3098`

If an exception occurs in `on_irreversible_block` (the batch removal), the undo state has already been committed. On restart, the node resumes from this inconsistent state — undo states are gone but comment removals may be incomplete.

### M4: Use-after-move in `unpack_from_snapshot` error path

**File**: `libraries/chainbase/include/chainbase/chainbase.hpp:423-426`

```cpp
auto insert_result = _indices.emplace(std::move(tmp));
if(!insert_result.second) {
    std::string s = preetify(fc::variant(tmp));  // use after move
```

### M5: Null pointer dereference in `get_environment_details()`

**File**: `libraries/chainbase/src/chainbase.cpp:464-465`

```cpp
const environment_check* const env = _segment->find< environment_check >( "environment" ).first;
return env->dump();  // no null check, unlike every other similar call
```

### M6: `assert`-only null checks compiled out in Release builds

**File**: `libraries/chainbase/src/chainbase.cpp:409-449`

Multiple shared memory lookups use `assert(env)` which is a no-op in Release builds. If shared memory is corrupted, these become null pointer dereferences with no diagnostic.

### M7: `pop_block_extended` has no termination guard

**File**: `libraries/chain/database.cpp:712-727`

If `end_block` is not in the current chain's history, the loop pops every block until `pop_block()` throws from an empty chain. No explicit guard against popping past LIB.

### M8: Type-unsafe `reinterpret_cast` in `shared_allocator_carrier`

**File**: `libraries/chainbase/include/chainbase/pool_allocator.hpp:398`

```cpp
return reinterpret_cast< impl_allocator_t<U>* >( carried_allocator );
```

`carried_allocator` is `void*` cast back with a type `U` determined by the caller. No compile-time or runtime safeguard that `U` matches the original type.

---

## LOW Findings

### L1: `block_list.size()-1` unsigned underflow when BLOCK_SIZE==1

**File**: `libraries/chainbase/include/chainbase/pool_allocator.hpp:272`

If `block_list` is empty, `size()-1` wraps to SIZE_MAX. Combined with a block that has `on_list_index == invalid_index`, this can lead to `pop_back()` on empty vector (UB). Only affects BLOCK_SIZE==1.

### L2: `allocated_count` uint32_t underflow on double-free

**File**: `libraries/chainbase/include/chainbase/pool_allocator.hpp:76-77`

`--allocated_count` wraps to UINT32_MAX on double-free, causing premature block deallocation.

### L3: `const_cast` on ordered set elements

**File**: `libraries/chainbase/include/chainbase/pool_allocator.hpp:262, 298`

Elements in `bip::set` are const_cast'd. Formally UB, but safe since sort key (`get_chunks_address()`) is never modified.

### L4: `_item_additional_allocation` unsigned underflow risk

**File**: `libraries/chainbase/include/chainbase/chainbase.hpp:485, 496`

If accounting gets out of sync (e.g., via H4), unsigned subtraction wraps to huge values.

### L5: Raw pointers in `_index_list` alias `unique_ptr`-owned objects

**File**: `libraries/chainbase/include/chainbase/chainbase.hpp:1430-1433`

Safe in current usage but fragile maintenance hazard.

### L6: `sysconf` return value unchecked in `get_total_system_memory()`

**File**: `libraries/chainbase/include/chainbase/chainbase.hpp:1143-1145`

`sysconf` returns -1 on error. Multiplying two -1 values produces 1 as unsigned, or -1 * positive wraps to huge value.

### L7: `posix_fadvise` return value unchecked

**File**: `libraries/chainbase/src/chainbase.cpp:242`

### L8: `close()` does not release file lock explicitly

**File**: `libraries/chainbase/src/chainbase.cpp:299-311`

### L9: `atol` on environment variable with no validation

**File**: `libraries/chain/database.cpp:76`

### L10: Non-atomic paired atomic loads with deprecated `memory_order_consume`

**File**: `libraries/chain/database.cpp:3559-3560`

### L11: `compiler_version` may lack null terminator if `__VERSION__` >= 256 chars

**File**: `libraries/chainbase/src/chainbase.cpp:31`

---

## Root Cause Hypothesis

The crash is almost certainly caused by the combination of **C1 + H1 + H2**:

1. **C1**: `rocksdb_comment_archive::on_irreversible_block` batch-removes 10,000+ objects, causing massive segment manager free-list churn.
2. **H1**: Using `db.remove()` instead of `db.remove_no_undo()` adds unnecessary undo-state allocations during the batch removal, increasing segment manager pressure.
3. **H2**: On the next block, `process_comment_cashout()` creates `volatile_comment_object`s (chainbase allocations) interleaved with `pay_curators()` calls. These allocations draw from the same segment manager whose free list was just heavily churned.
4. If the segment manager's internal free list has a corruption (a known risk with boost::interprocess under heavy allocation/deallocation churn), an allocation could return memory overlapping with existing live `comment_vote_object` nodes, corrupting the red-black tree.
5. The crash manifests in `pay_curators()` because that's the first code to traverse the corrupted `comment_vote_index` tree.

This explains:
- **Machine-dependent**: Segment manager layout varies by system
- **Non-deterministic**: Allocation patterns depend on memory state
- **Only with RocksDB archive**: Only archive with batch removal + chainbase allocation during cashout
- **~24.9M blocks**: When enough comments exist for the 10,000 volatile object threshold

## Recommended Remediation

1. **Switch to `remove_no_undo()` in `rocksdb_comment_archive::on_irreversible_block`** — matches `memory_comment_archive`, eliminates unnecessary undo allocation pressure (fixes H1)
2. **Reduce or eliminate the `volatile_objects_limit` batching** — process removals incrementally like `memory_comment_archive` instead of 10,000-object batches (mitigates C1)
3. **Use `placeholder_comment_archive` during replay** — no comment archiving needed when replaying from block_log (workaround)
4. **Add segment manager overlap detection** — verify new block allocations don't overlap existing blocks (diagnostic)
5. **Fix nested `modify()` UB** — extract RC calls outside the modify lambda (fixes H3)
6. **Fix dangling reference in `generic_index::modify`** — check `ok` before accessing `obj` (fixes H4)
