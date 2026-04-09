# Investigation Summary: Non-deterministic crash in hived replay

## The bug
Non-deterministic segfault during blockchain replay. Manifestations vary between runs:
- Segfault in `boost::multi_index::rebalance_for_extract` (tree node pointer corruption)
- Segfault in `ordered_index_node_impl::increment` (corrupted offset_ptr during traversal)
- `free(): invalid next size` (heap corruption symptoms)
- Uniqueness constraint violations on `comment_object`
- Crashes hit different blocks each run (range: 3M – 16M)
- Affects multiple indices: `comment_vote_index` and `comment_object`

## Most common crash signature
```
cashout_comment_helper → erase → rebalance_for_extract on comment_vote_index
```
or
```
rocksdb_comment_archive::on_irreversible_block → erase → rebalance_for_extract on comment_object
```

## What was tried this session

| Approach | Result |
|---|---|
| **Fix nested `modify()` UB** in `vote_evaluator` (refactored 2 inner modifies out of outer modify) | Committed (`156f8aac1`). Did NOT fix crash — was on different indices than the corrupted ones. |
| **Neighborhood RB-tree integrity check** O(1) before/after every modify/remove on all ordered indices | Committed (`f415a2d17`, `133dd25cc`). Did NOT catch corruption — corruption is distant from elements being touched. |
| **Full tree walk validation** O(n) every 10K removes | Walk itself crashed at 3.3M on bad offset_ptr in `increment()`, proving corruption exists silently very early. |
| **`MALLOC_CHECK_=3`** | Survived longest (16.2M blocks) but still crashed. Confirms corruption is NOT in glibc heap. |

## Key technical findings

1. **Tree node layout**: `index_node_base` (value at base) → `ordered_index_node_impl` layers above (parent/left/right as `offset_ptr`). Writing past `sizeof(value_type)` corrupts tree pointers while leaving data intact.
2. **Tree nodes use `offset_ptr`** (not raw pointers) — confirmed in `boost/multi_index/detail/ord_index_node.hpp:90`. Segment remapping is NOT the cause.
3. **Data ordering is intact** — neighborhood checks pass. Only the tree's internal `offset_ptr` links are corrupted.
4. **Replay is single-threaded** for DB ops (user confirmed). Race conditions ruled out.
5. **Pool allocator is correctly sized** for full nodes. Overlap/canary checks found zero issues across previous sessions.

## Conclusion / Hypothesis

Something writes past the boundary of a chain object's data area into adjacent boost::multi_index node metadata in shared memory. The corruption affects offset_ptr tree links but not key data, which is why ordering checks pass and traversal crashes. None of the standard memory debugging tools (ASAN, MALLOC_CHECK, canaries, overlap detection, UBSan) can see writes inside mmap'd shared memory.

## State on sd3

- Source: `/storage1/mario/00.src/hive` (branch `mt-random-crash`, with patches applied)
- Binaries built: `hived_fix_nested_modify`, `hived_tree_check`, `hived_tree_check2`
- Last replay: crashed at 16.2M blocks (`replay-malloc-check.log`)

## Next recommended step

**Use `rr` (record-replay debugger)**: install via apt, record one crashing replay, then on the recording set a hardware watchpoint on the corrupted `offset_ptr` address and `reverse-continue` to find the exact instruction that wrote the bad value. This is the only remaining technique likely to definitively identify the corruption source given that all standard memory checkers can't see into mmap'd memory.

Caveat: `rr` adds 10–30x overhead, so reaching the 16M crash block could take hours.
