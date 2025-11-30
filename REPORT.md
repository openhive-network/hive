# CI Optimization Task Report - hive-test-speedup-v4

## Summary

This task focused on optimizing the CI pipeline for the Hive project to reduce build and test execution times. The primary goal was to identify and implement optimizations that speed up the CI pipeline without reducing test coverage.

**Final Outcome:** The attempted optimization (increasing PYTEST_NUMBER_OF_PROCESSES from 8 to 12) was **reverted** due to consistent test failures in jobs that connect to hived services.

## Changes Made

### Iteration 1: Increase Pytest Parallelism (REVERTED)
**Commit:** `2ecb4310d` - CI: Increase default PYTEST_NUMBER_OF_PROCESSES from 8 to 12 for faster test execution

**Change Details:**
- Modified `.gitlab-ci.yaml` line 10
- Changed `PYTEST_NUMBER_OF_PROCESSES: 8` to `PYTEST_NUMBER_OF_PROCESSES: 12`
- This affects all pytest-based test jobs that use the default parallelism setting

**Pipeline Results:**
- Pipeline 140514: **FAILED** - 5 test jobs failed
- Pipeline 140518: **FAILED** - 4 test jobs failed (retry)

**Failing Jobs (Consistent across runs):**
- `message_format_mainnet_5m_tests: [condenser_api_tests]`
- `message_format_mainnet_5m_tests: [market_history_api_tests]`

**Analysis:**
The `message_format_mainnet_5m_tests` jobs connect to a hived service and likely cannot handle 12 concurrent test processes making connections. These tests don't override `PYTEST_NUMBER_OF_PROCESSES`, so they inherit the global default.

**Revert Commit:** `<pending>` - Revert PYTEST_NUMBER_OF_PROCESSES back to 8

## Analysis Findings

### Current Pipeline Structure
The CI pipeline has the following stages:
1. `static_code_analysis` - Pre-commit checks
2. `build` - Multiple build jobs (hived images, beekeeper, block logs, API packages)
3. `test` - ~50+ parallel test jobs
4. `cleanup` - Cache cleanup
5. `publish` - Package deployment
6. `deploy` - Docker image and wheel deployment

### Key Parallelism Settings
| Setting | Value | Notes |
|---------|-------|-------|
| Default `PYTEST_NUMBER_OF_PROCESSES` | 8 | Global default, affects most test jobs |
| `fork_tests` | 3 processes | Limited due to resource-intensive network simulations |
| `comment_cashout_tests` | 1 process | Appears to have ordering dependencies |
| `colony_tests` | 1 process | Sequential test requirements |

### Why Increasing Default Parallelism Failed

1. **Shared Service Connections**: Tests like `message_format_mainnet_5m_tests` connect to a shared `hived-service` Docker container
2. **Connection Limits**: The hived service may have connection limits or resource constraints that prevent 12 concurrent test processes
3. **No Per-Job Override**: These jobs don't override `PYTEST_NUMBER_OF_PROCESSES`, inheriting the global default
4. **Network Bottleneck**: Increased parallelism causes more concurrent HTTP connections to the hived API endpoint

### Recommended Future Optimizations

**A. Selective Parallelism Increase**
Instead of changing the global default, increase parallelism only for jobs that don't connect to shared services:
```yaml
# Only for isolated test jobs
operation_tests:
  variables:
    PYTEST_NUMBER_OF_PROCESSES: 12
```

**B. Fix message_format tests to specify parallelism**
```yaml
message_format_mainnet_5m_tests:
  variables:
    PYTEST_NUMBER_OF_PROCESSES: 4  # Lower limit for service-connected tests
```

**C. Enable PYTEST_LOG_DURATIONS for profiling**
```yaml
variables:
  PYTEST_LOG_DURATIONS: 1
```
This would output test timing data to identify the slowest tests for targeted optimization.

**D. Increase fork_tests parallelism (Conservative)**
```yaml
fork_tests:
  variables:
    PYTEST_NUMBER_OF_PROCESSES: 4  # Currently 3
```

**E. Build caching improvements**
- The `generate_testing_block_logs` job runs 10 parallel matrix jobs that could benefit from better caching
- Docker layer caching is partially implemented but could be expanded

**F. Job dependency optimization**
Some test jobs wait for builds they don't directly need. Reviewing `needs:` dependencies could allow earlier job starts.

### Flaky Tests Observed

During testing, we observed different test failures between runs:
- Pipeline 140514: colony_tests, hardfork_schedule_tests, foundation_layer_tests
- Pipeline 140518: operation_tests (proposal_tests, limit_order_tests)
- Develop branch (140235): Also showed similar flaky failures

This indicates the test suite has some inherent flakiness that should be addressed independently of parallelism changes.

## Files Modified
- `.gitlab-ci.yaml` - Main CI configuration (change reverted)
- `REPORT.md` - This report

## Pipeline History

| Pipeline | SHA | Status | Failed Jobs |
|----------|-----|--------|-------------|
| 140508 | 2ecb4310d | Canceled | N/A |
| 140514 | 3f3b40375 | Failed | 5 jobs |
| 140518 | e8751c03b | Failed | 4 jobs |

## Technical Details

### Branch Information
- **Branch:** `ci-speedup-hive-v4`
- **Base:** `develop`
- **Project:** 198 (hive/hive)
- **MR:** https://gitlab.syncad.com/hive/hive/-/merge_requests/1706

### Commits
1. `2ecb4310d` - CI: Increase default PYTEST_NUMBER_OF_PROCESSES from 8 to 12 (REVERTED)
2. `f1ac29bdf` - Add initial task report
3. `3f3b40375` - Trigger CI pipeline (retry after cancellation)
4. `e8751c03b` - CI: Retry pipeline (previous run had flaky test failures)
5. `<pending>` - Revert PYTEST_NUMBER_OF_PROCESSES and update report

## Conclusion

The attempt to increase default pytest parallelism from 8 to 12 processes was unsuccessful due to test jobs that connect to shared hived services. These jobs cannot handle the increased concurrent connections.

**Recommendation:** To safely increase parallelism in the future:
1. Audit all test jobs to identify which ones connect to shared services
2. Apply increased parallelism selectively to isolated test jobs only
3. Add explicit `PYTEST_NUMBER_OF_PROCESSES` overrides to service-connected jobs with lower values

---
*Generated by CI Optimization Task on 2025-11-30*
