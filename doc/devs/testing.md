# Testing

The unit test target is `make chain_test`
This creates an executable `./tests/chain_test` that will run all unit tests.

Tests are broken in several categories:
```
basic_tests          // Tests of "basic" functionality
block_tests          // Tests of the block chain
live_tests           // Tests on live chain data (currently only past hardfork testing)
operation_tests      // Unit Tests of Hive operations
operation_time_tests // Tests of Hive operations that include a time based component (ex. vesting withdrawals)
serialization_tests  // Tests related of serialization
```

# Code Coverage Testing

If you have not done so, install lcov `brew install lcov`

```
cmake -D BUILD_HIVE_TESTNET=ON -D ENABLE_COVERAGE_TESTING=true -D CMAKE_BUILD_TYPE=Debug .
make
lcov --capture --initial --directory . --output-file base.info --no-external
tests/chain_test
lcov --capture --directory . --output-file test.info --no-external
lcov --add-tracefile base.info --add-tracefile test.info --output-file total.info
lcov -o interesting.info -r total.info tests/\*
mkdir -p lcov
genhtml interesting.info --output-directory lcov --prefix `pwd`
```

Now open `lcov/index.html` in a browser

# CI Testing

## Quick Test Mode

Quick Test mode allows running CI tests using pre-built binaries from a previous successful pipeline, skipping the ~20 minute C++ build step. This is useful when:

- You're only changing test code, not hived source
- You want fast feedback on test changes
- You're debugging flaky tests

### Usage

Run a pipeline with these variables:

| Variable | Required | Description |
|----------|----------|-------------|
| `QUICK_TEST` | Yes | Set to `true` to enable |
| `QUICK_TEST_BINARY_COMMIT` | No | Specific commit SHA to use (auto-detects if empty) |
| `QUICK_TEST_JOBS` | No | Additional test jobs to run (comma-separated) |
| `QUICK_TEST_BLOCK_LOG_IMAGE` | No | Block log image for test_tools_tests |

**Example - Basic quick test:**
```bash
glab ci run -b my-branch --variables QUICK_TEST:true
```

**Example - With specific binary commit:**
```bash
glab ci run -b my-branch --variables QUICK_TEST:true --variables QUICK_TEST_BINARY_COMMIT:abc1234f
```

**Example - Including test_tools_tests:**
```bash
glab ci run -b my-branch \
  --variables QUICK_TEST:true \
  --variables QUICK_TEST_JOBS:test_tools_tests \
  --variables QUICK_TEST_BLOCK_LOG_IMAGE:registry.gitlab.syncad.com/hive/hive/block-log-5m:latest
```

### What Runs

| Mode | Build Jobs | Test Jobs | Time |
|------|------------|-----------|------|
| Normal | All (~20 min) | All 50+ jobs | ~60+ min |
| Quick Test | None | Unit tests only | ~30 min |

By default, Quick Test runs only unit tests (`chain_test`, `plugin_test`). Use `QUICK_TEST_JOBS` to add more.

### Finding Available Binaries

To list recent testnet images available for Quick Test:
```bash
./scripts/ci-helpers/list-hive-binaries.sh
```

### Limitations

- Binaries must exist in the registry from a previous successful pipeline
- If your changes affect hived behavior, results may not reflect current code
- Auto-detection uses the most recent testnet image tag
