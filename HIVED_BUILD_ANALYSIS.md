# Hived Full Build Performance Analysis

**Build Date:** 2026-01-06
**Compiler:** Clang++ 18.1.3
**Build Type:** Release with `-ftime-trace`
**Files Analyzed:** 816 compilation units

## Executive Summary

**Total Build Time:** 1 minute 23 seconds (wall clock)
- **Frontend (parsing):** 641.9 seconds (49.7%)
- **Backend (codegen/optimization):** 650.7 seconds (50.3%)
- **CPU Time:** 20 minutes 5 seconds (parallel compilation)

**Key Finding:** Backend and frontend are nearly balanced, suggesting both template instantiation AND header processing are major bottlenecks.

## Top 10 Slowest Files to Compile

### Frontend (Parsing)

| Rank | File | Parse Time | Main Bottleneck |
|------|------|------------|-----------------|
| 1 | database.cpp | 47.7s | Heavy template instantiation |
| 2 | database_hardfork.cpp | 44.4s | Operation processing |
| 3 | hive_evaluator_transfer.cpp | 34.7s | Operation evaluators |
| 4 | hive_evaluator_account.cpp | 30.0s | Account operations |
| 5 | index-03.cpp | 27.7s | Boost.MultiIndex |
| 6 | database_init.cpp | 24.3s | Genesis initialization |
| 7 | index-08.cpp | 20.2s | Index instantiations |
| 8 | index-11.cpp | 20.1s | Index instantiations |
| 9 | index-06.cpp | 17.6s | Index instantiations |
| 10 | hive_evaluator.cpp | 17.0s | General evaluators |

### Backend (Code Generation)

| Rank | File | Codegen Time | Main Bottleneck |
|------|------|--------------|-----------------|
| 1 | full_transaction.cpp | 34.5s | Serialization codegen |
| 2 | index-03.cpp | 31.8s | Container operations |
| 3 | index-01.cpp | 29.7s | Container operations |
| 4 | full_block.cpp | 29.5s | Serialization codegen |
| 5 | index-04.cpp | 29.3s | Container operations |
| 6 | database.cpp | 29.1s | Database operations |
| 7 | index-11.cpp | 28.4s | Container operations |
| 8 | block_log.cpp | 27.1s | Block I/O operations |
| 9 | index-06.cpp | 26.5s | Container operations |
| 10 | index-05.cpp | 26.3s | Container operations |

## Critical Template Bottlenecks

### Top 5 Most Expensive Templates (Total Time)

1. **`chainbase::generic_index<>`**: 181.0 seconds (123 instances, avg 1471ms)
   - Database index infrastructure
   - Heavy Boost.MultiIndex usage
   - **Priority: HIGH** - Core infrastructure

2. **`boost::container::dtl::tree<>`**: 148.7 seconds (576 instances, avg 258ms)
   - Underlying container for sets/maps
   - **Priority: MEDIUM** - Difficult to optimize

3. **`boost::intrusive::pointer_rebind<>`**: 144.5 seconds (10,632 instances, avg 13ms)
   - Pointer type metaprogramming
   - **Priority: LOW** - Many small instantiations

4. **`chainbase::pool_allocator_t<>`**: 143.8 seconds (822 instances, avg 174ms)
   - Custom allocator template
   - **Priority: MEDIUM** - Can be optimized

5. **`boost::container::set<>`**: 122.1 seconds (699 instances, avg 174ms)
   - Primary container type
   - **Priority: MEDIUM** - Usage can be reviewed

### Operation Serialization Templates

**CRITICAL FINDING:** `fc::static_variant` serialization is extremely expensive!

- `fc::extended_serialization_functor<fc::static_variant<hive::protocol...>>`: 54.0 seconds (47 instances)
- Each operation type adds ~750ms for `fc::impl::storage_ops<N, ...>`
- **66 operation types** × 750ms × 47 instantiations = massive overhead

**Example costs per operation:**
- `storage_ops<0, vote_operation, ...>`: 35.6s (47 instances, 757ms avg)
- `storage_ops<1, comment_operation, ...>`: 35.5s (47 instances, 756ms avg)
- `storage_ops<2, transfer_operation, ...>`: 35.5s (47 instances, 755ms avg)

This pattern continues for ALL 66 protocol operations!

## Most Expensive Headers

| Header | Total Time | Include Count | Avg Cost | Impact |
|--------|-----------|---------------|----------|--------|
| `hive/protocol/types.hpp` | 86.4s | 67 | 1290ms | **CRITICAL** |
| `hive/protocol/transaction.hpp` | 37.5s | 44 | 853ms | **HIGH** |
| `hive/protocol/block.hpp` | 36.2s | 40 | 905ms | **HIGH** |
| `fc/exception/exception.hpp` | 26.0s | 34 | 764ms | **HIGH** |
| `hive/protocol/authority.hpp` | 23.5s | 59 | 397ms | MEDIUM |

### Critical Include Chain

```
types.hpp (1290ms avg)
  ↓
transaction.hpp (853ms avg)
  ↓
block.hpp (905ms avg)
  ↓
(appears in evaluators, database, indices)
```

This chain is pulled into nearly every compilation unit!

## Specific Function Compilation Times

Top 10 slowest individual functions:

1. `block_log_artifacts::impl::open()`: 644ms
2. `pre_hf20_vote_evaluator()`: 634ms
3. `node_impl::dump_node_status()`: 597ms
4. `comment_evaluator::do_apply()`: 507ms
5. `node_impl::close()`: 462ms
6. `database::apply_hardfork()`: 403ms
7. `update_witness_schedule4()`: 360ms
8. `node_impl::terminate_inactive_connections_loop()`: 352ms
9. `hf20_vote_evaluator()`: 321ms
10. `split_block_log()`: 309ms

## Optimization Recommendations

### Immediate Impact (High Priority)

1. **Split `types.hpp` into focused headers**
   - Current: 86.4s total, 1290ms per include
   - Target: Create `types_fwd.hpp`, `types_basic.hpp`, `types_operations.hpp`
   - **Expected savings:** 40-50 seconds

2. **Reduce Operation Serialization Bloat**
   - Problem: Each operation × 47 instantiations × 750ms
   - Solution: Extern template declarations for common instantiations
   - **Expected savings:** 100+ seconds

3. **Optimize `chainbase::generic_index<>` instantiations**
   - Current: 181s total across 123 instances
   - Solution: Explicit instantiation in dedicated .cpp files
   - **Expected savings:** 50-70 seconds

### Medium Impact

4. **Create Precompiled Headers**
   - Candidates: `types.hpp`, `transaction.hpp`, `exception.hpp`
   - Would benefit 60+ compilation units
   - **Expected savings:** 60-80 seconds

5. **Further Split Large Files**
   - `database.cpp` (76.8s total): Split by functionality
   - `full_transaction.cpp` (34.5s): Extract serialization
   - `block_log.cpp` (27.1s): Split I/O operations
   - **Expected savings:** 30-40 seconds

6. **Reduce `boost::container` Template Depth**
   - Use type aliases to reduce nesting
   - Explicit instantiation for common types
   - **Expected savings:** 20-30 seconds

### Long-term Improvements

7. **Review Boost.MultiIndex Usage**
   - Consider simpler alternatives where possible
   - May reduce `generic_index<>` complexity

8. **Operation Visitor Pattern Optimization**
   - Current static_variant visiting is very expensive
   - Consider runtime dispatch for hot paths

## Comparison: Before and After Splitting

### database_api Plugin (Already Optimized)

**Before split:** Single 2000+ line file
**After split:** 4 domain files + impl header

**Result:** Parallel compilation, reduced per-file time

**Same strategy applicable to:**
- `database.cpp` → Split by: initialization, processing, validation, snapshots
- `hive_evaluator*.cpp` → Already split by operation type (GOOD!)
- Index files → Already split (index-01 through index-11) (GOOD!)

## Build Parallelization Effectiveness

**Wall clock:** 83.8 seconds
**CPU time:** 1205 seconds
**Parallelization factor:** 14.4x

This indicates excellent parallelization. The build system is well-configured!

## Template Instantiation Statistics

- **Total unique template instantiations:** ~50,000+
- **Most instantiated template:** `boost::intrusive::pointer_rebind<>` (10,632 times)
- **Most expensive average:** `chainbase::generic_index<>` (1471ms per instance)
- **Deepest nesting:** Operation serialization chains (6+ template levels)

## Memory Usage During Compilation

(Not directly measured, but inferred from template complexity)

**Estimated peak per-file:**
- `database.cpp`: ~2-3GB
- `full_transaction.cpp`: ~2-3GB
- Large index files: ~1.5-2GB

**Recommendation:** Build machine should have 16GB+ RAM for comfortable parallel builds.

## Key Takeaways

1. **Header bloat is real:** `types.hpp` alone costs 86 seconds across the build
2. **Operation serialization dominates:** Static variant machinery is the #1 bottleneck
3. **Chainbase indices are expensive:** Generic index instantiations cost 181 seconds
4. **Already well-parallelized:** Getting 14x speedup from parallel compilation
5. **Backend/Frontend balanced:** Both parsing and codegen need optimization

## Next Steps

1. ✅ Measure and document (DONE - this document)
2. Split `types.hpp` into focused headers
3. Add extern template declarations for operation serialization
4. Create precompiled header for stable dependencies
5. Re-measure and validate improvements

---

*Analysis generated using ClangBuildAnalyzer v1.6.0*
*Full data available in: `/tmp/hived_full_analysis.txt`*
