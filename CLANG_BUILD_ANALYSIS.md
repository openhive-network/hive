# Clang Build Analysis for Hive

## ClangBuildAnalyzer Setup

### Installation
ClangBuildAnalyzer has been installed to `/tmp/ClangBuildAnalyzer/build/ClangBuildAnalyzer`

### Usage Workflow

1. **Start capture session:**
   ```bash
   /tmp/ClangBuildAnalyzer/build/ClangBuildAnalyzer --start <build_dir>
   ```

2. **Build with time tracing** (requires Clang with `-ftime-trace` flag):
   ```bash
   cmake -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang \
         -DCMAKE_CXX_FLAGS="-ftime-trace" ..
   ninja
   ```

3. **Stop capture and process:**
   ```bash
   /tmp/ClangBuildAnalyzer/build/ClangBuildAnalyzer --stop <build_dir> <output_file.bin>
   ```

4. **Analyze results:**
   ```bash
   /tmp/ClangBuildAnalyzer/build/ClangBuildAnalyzer --analyze <output_file.bin>
   ```

## Current Clang Build Status

### Fixed Issues (as of 2026-01-06)

1. **fc/asio.hpp line 84**: Changed from direct promise constructor call to factory method
   - **Before:** `promise<size_t>::ptr p(new promise<size_t>("fc::asio::read"));`
   - **After:** `promise<size_t>::ptr p = promise<size_t>::create("fc::asio::read");`
   - **Reason:** Promise constructor is protected; must use static `create()` factory method

2. **fc/src/network/rate_limiting.cpp line 219**: Made destructor virtual
   - **Before:** `~rate_limiting_group_impl();`
   - **After:** `virtual ~rate_limiting_group_impl();`
   - **Reason:** Class inherits from base with virtual functions; needs virtual destructor

### Fixed - Chainbase Inheritance Issue

**Location:** `chainbase/pool_allocator.hpp:18`

**Problem:**
- `pool_allocator_t` used **private inheritance**: `class pool_allocator_t : private std::conditional_t<...>`
- `allocator_helper_t::get_generic_allocator()` needs to cast to base allocator type
- Clang's strict access control rejected cast to private base (GCC allowed it)

**Solution:** Changed to **public inheritance**
- **Before:** `class pool_allocator_t : private std::conditional_t<...>`
- **After:** `class pool_allocator_t : public std::conditional_t<...>`

**Result:** Full Clang build now succeeds! ✓

## Compilation Time Analysis Results

### account_history_rocksdb_plugin.cpp Analysis

**Total compilation time:** 36.8 seconds
- **Frontend (parsing):** 7.96 seconds (22%)
- **Backend (codegen):** 28.82 seconds (78%)

The backend dominates! Code generation and optimization take 3.6x longer than parsing.

**Top Time Consumers:**

1. **Template Instantiations (18.1 seconds total):**
   - `std::shared_ptr` family: 27.3 seconds across 6000+ instantiations
   - `std::make_shared`: 8.4 seconds (1439 instantiations)
   - `std::unique_ptr`: 2.4 seconds (328 instantiations)
   - `fc::static_variant<>::visit`: 1.1 seconds (expensive operation serialization)
   - `boost::asio::async_initiate`: 1.1 seconds

2. **Expensive Headers:**
   - `boost/lexical_cast.hpp`: 35.9 seconds (included 82 times!)
   - `fc/time.hpp`: 33.4 seconds (79 includes)
   - `fc/variant.hpp`: 28.7 seconds (79 includes)
   - `fc/log/logger.hpp`: 25.2 seconds (77 includes)
   - `fc/exception/exception.hpp`: 24.7 seconds (72 includes)

3. **Include Chain Analysis:**
   Most expensive chain: `exception.hpp → logger.hpp → time.hpp → reflect.hpp → lexical_cast.hpp`

## Alternative Analysis Methods (if Clang unavailable)

You can still analyze compilation bottlenecks using GCC preprocessing:

### Preprocessed Line Count Analysis

```bash
g++ -E <all_include_flags> source_file.cpp 2>&1 | wc -l
```

**Example for account_history_rocksdb_plugin.cpp:**
- **Preprocessed lines:** 518,109 lines
- This massive expansion indicates heavy template/header bloat

### Manual Header Dependency Analysis

Use `g++ -H` to see header inclusion tree:
```bash
g++ -H <compile_flags> -c source_file.cpp 2>&1 | less
```

### Include-What-You-Use (IWYU)

Alternative tool for analyzing includes:
```bash
sudo apt-get install iwyu
include-what-you-use <compile_flags> source_file.cpp
```

## Recommendations for Reducing Compilation Time

Based on the preprocessing analysis:

1. **Header Bloat in account_history_rocksdb_plugin.cpp**
   - 518K preprocessed lines indicates excessive header includes
   - Many Boost headers are included transitively
   - Consider forward declarations instead of full includes

2. **Apply Similar Splitting Strategy**
   - Like `database_api`, split large plugin implementations into focused compilation units
   - Separate by functional domain (storage, indexing, queries, etc.)

3. **Use Precompiled Headers (PCH)**
   - Common stable headers (Boost, FC, protocol) could be precompiled
   - CMake supports PCH via `target_precompile_headers()`

4. **Reduce Template Instantiations**
   - Explicit template instantiations in separate .cpp files
   - Avoid including heavy template headers unnecessarily

## Files Modified for Clang Compatibility

- `/storage1/src/hive/libraries/fc/include/fc/asio.hpp` - Fixed promise constructor call
- `/storage1/src/hive/libraries/fc/src/network/rate_limiting.cpp` - Made destructor virtual
