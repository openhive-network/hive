# ENABLE_STD_ALLOCATOR Implementation Summary

## What was done

Replaced all `std::conditional_t` and template-parameter-based allocator mode
selection with `#ifdef ENABLE_STD_ALLOCATOR` preprocessor blocks. Added Msan
build type alongside Asan. Added a standalone test that exercises the
`std::allocator` code path.

## Files changed (vs develop)

### CMake
- **cmake/hive_build_types.cmake** — Added Msan build type. Extracted duplicate
  Asan/Msan setup into `define_sanitizer_build_type()` function.
- **cmake/hive_options.cmake** — Added `ENABLE_STD_ALLOCATOR` CMake option,
  auto-enabled for Asan/Msan builds.
- **libraries/vendor/CMakeLists.txt** — Enable RocksDB ASan support; force
  source build for Asan/Msan (skip preinstalled).

### Chainbase allocator headers
- **allocator_helpers.hpp** — Added `#ifdef ENABLE_STD_ALLOCATOR` blocks for
  `allocator`, `shared_string`, `t_vector`, `t_flat_set`, `t_set`, `t_flat_map`,
  `t_map`, `t_deque`. Added `_ENABLE_STD_ALLOCATOR` macro. Added 2-param
  template matcher in `get_allocator_helper_t` for `shared_allocator_carrier`.
- **pool_allocator.hpp** — Replaced all `std::conditional_t` and SFINAE
  constructors with `#ifdef`. Removed `USE_MANAGED_MAPPED_FILE` template
  parameter (was 4th param, now 3-param template). Same for
  `shared_allocator_carrier` (was 3-param, now 2-param). Replaced
  `shared_pool_allocator_t::impl_ptr_t` conditional with `#ifdef`.
  Replaced `multi_index_allocator` and `undo_state_allocator` conditionals
  with `#ifdef`.

### Chainbase core
- **chainbase.hpp** — Guard `shared_string` comparison operators, shared memory
  index creation with `#ifndef ENABLE_STD_ALLOCATOR`. Add heap-based
  `index_type` allocation for std::allocator mode.
- **chainbase.cpp** — Guard `environment_check` shared-memory members, `open()`
  mmap logic, and shm accessor functions. Provide no-op stubs in
  std::allocator mode.
- **forward_declarations.hpp** — Guard shared_string pack/unpack declarations.

### Chain library
- **database.hpp/cpp** — Guard `get_comment`/`find_comment` overloads that take
  `std::string` (redundant when `shared_string == std::string`).
- **hive_object_types.hpp** — Guard shared_string serialization, FC_REFLECT for
  shared_string, `unpack_from_vector` for buffer_type.
- **buffer_type.hpp** — Guard `FC_REFLECT_TYPENAME(buffer_type)`.

### Tests
- **test_std_allocator.cpp** — New standalone test (8 test cases) compiled with
  `-DENABLE_STD_ALLOCATOR`. Tests pool_allocator_t directly without
  chainbase::database. Uses header-only Boost.Test.
- **tests/unit/CMakeLists.txt** — Add `std_allocator_test` target, exclude from
  chain_test glob.
- **.gitlab-ci.yaml** — Run `std_allocator_test` as part of chain_test CI job.

### Other
- **programs/util/test_shared_mem.cpp** — Guard shared-memory-specific code.
- **tests/unit/db_fixture/clean_database_fixture.cpp** — Guard bip-specific
  template instantiation.
- **tests/unit/plugin_tests/** — Disable plugin tests under
  ENABLE_STD_ALLOCATOR (they depend on shared memory).
- **tests/unit/tests/basic_tests.cpp** — Guard `chain_object_size` test.
- **libraries/fc** — Submodule update.

## Possible simplifications

1. **Remove `_ENABLE_STD_ALLOCATOR`, `_ENABLE_MULTI_INDEX_POOL_ALLOCATOR`,
   `_ENABLE_UNDO_STATE_POOL_ALLOCATOR` true/false macros** from
   `allocator_helpers.hpp`. They were used by `std::conditional_t` which is now
   gone. `_ENABLE_STD_ALLOCATOR` is still referenced by
   `pool_allocator_t::use_managed_mapped_file` — replace with
   `!defined(ENABLE_STD_ALLOCATOR)` or a constexpr. The other two are completely
   unused.

2. **`pool_allocator_t::use_managed_mapped_file` member** — still exists as
   `static constexpr bool use_managed_mapped_file = !_ENABLE_STD_ALLOCATOR`.
   Only consumed by `shared_pool_allocator_t` constructor (to match
   `shared_allocator_carrier` template). Since `shared_allocator_carrier` lost
   its `USE_MANAGED_MAPPED_FILE` parameter, this member may no longer be needed.
   Check if anything else reads it.

3. **`shared_pool_allocator_t` constructor** takes
   `shared_allocator_carrier<T, POOL_ALLOCATOR::block_size>`. The
   `POOL_ALLOCATOR::block_size` is accessed — but `use_managed_mapped_file` is
   not accessed in the constructor anymore. If nothing else reads
   `use_managed_mapped_file`, it can be removed.

4. **Squash commits** — there are 16 commits on this branch. Many are
   incremental fixes (CI fix, move test file, rename parameter back, etc.).
   Before merge, squash into logical units:
   - One commit for CMake changes (Msan build type + ENABLE_STD_ALLOCATOR option
     + hive_build_types refactor)
   - One commit for allocator #ifdef refactoring (pool_allocator.hpp +
     allocator_helpers.hpp)
   - One commit for chainbase/chain ENABLE_STD_ALLOCATOR guards
   - One commit for the std_allocator_test

5. **The 4-param `get_generic_allocator` overload** in `allocator_helpers.hpp`
   (matching `<typename, uint32_t, bool, bool>`) was removed because
   `pool_allocator_t` lost its 4th parameter. The original 3-param overload
   (matching `<typename, uint32_t, bool>`) now matches `pool_allocator_t`. The
   new 2-param overload (matching `<typename, uint32_t>`) matches
   `shared_allocator_carrier`. Verify both are actually called — if
   `shared_allocator_carrier` never goes through `get_allocator_helper_t`, the
   2-param overload is dead code.
