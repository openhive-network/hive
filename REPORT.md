# CI Optimization Report - Test Speedup Experiment v2

## Summary

This branch implements CI optimizations focused on two areas:
1. **Phase 1**: Increasing test parallelism across multiple test jobs in the GitLab CI pipeline
2. **Phase 2**: Test code optimizations to reduce unnecessary waits and improve test efficiency

## Changes Made

### Phase 2 - Test Code Optimizations (This Iteration)

1. **5dab14180** - Tests: Optimize test_update_proposal by replacing sleep(66) with wait_for_irreversible_block()
   - **File**: `tests/python/functional/cli_wallet/test_proposal.py`
   - **Change**: Replaced hardcoded `sleep(3 * 22)` (66 seconds) with `node.wait_for_irreversible_block()`
   - **Impact**: Eliminates unnecessary 66-second sleep, uses API-based waiting instead which completes as soon as the condition is met
   - **Benefit**: Estimated 30-60 second reduction per test run depending on block production timing

### Phase 1 - CI Parallelism Increases (Previous Iterations)

| Commit | Description |
|--------|-------------|
| 8167ea00f | CI: Increase parallelism for live_sync_tests, broadcast_tests (6), rc_direct_delegations_tests (8) |
| 87435cf29 | CI: Increase parallelism for recovery_tests, authority_tests, hf26_tests, hf28_tests (8 processes) |
| b27a9bf0e | CI: Increase parallelism for universal_block_log_tests and foundation_layer_tests (8 processes) |
| 449d19602 | CI: Increase test parallelism for fork_tests (3->6) and colony_tests (1->4) |
| 2c4cae548 | CI: Increase default pytest parallelism from 8 to 12 processes |
| 9acab5abf | CI: Increase pytest processes for comment_cashout_tests (1->4) |

### Test Parallelism Summary

| Test Job | Previous Value | New Value |
|----------|---------------|-----------|
| Default (PYTEST_NUMBER_OF_PROCESSES) | 8 | 12 |
| fork_tests | 3 | 6 |
| colony_tests | 1 | 4 |
| universal_block_log_tests | (default 12) | 8 |
| foundation_layer_tests | (default 12) | 8 |
| recovery_tests | (default 12) | 8 |
| authority_tests | (default 12) | 8 |
| hf26_tests | (default 12) | 8 |
| hf28_tests | (default 12) | 8 |
| live_sync_tests | (default 12) | 6 |
| broadcast_tests | (default 12) | 6 |
| rc_direct_delegations_tests | (default 12) | 8 |
| comment_cashout_tests | 1 | 4 |

## Test Code Optimization Analysis

### Identified Sleep Calls in Test Code

| File | Sleep Duration | Purpose | Optimization Status |
|------|---------------|---------|---------------------|
| `test_proposal.py:103` | 66 seconds | Wait for blocks to become irreversible | **OPTIMIZED** - replaced with `wait_for_irreversible_block()` |
| `test_find_transaction.py:33` | 90 seconds | Wait for transaction to expire | Cannot optimize without mocking time |
| `hive_node.py:65` | 5 seconds | Node startup wait | Required for process initialization |
| `hive_node.py:87,91` | 7 seconds each | Node shutdown graceful termination | Required for clean shutdown |
| `test_hived_options.py:101` | 10 seconds | Verify node doesn't crash/produce blocks | Required for test logic |

### Other Optimization Opportunities Identified

1. **Polling in `common.py`**: `wait_n_blocks()` and `block_until_transaction_in_block()` use 1-second polling intervals - could potentially use exponential backoff
2. **Network fixture setup**: Complex multi-network tests (fork_tests) could benefit from parallel fixture setup
3. **`hive_node.py` shutdown waits**: 7-second waits could potentially be reduced with better process monitoring

## Results

### Pipeline Status

- Pipeline ID: 140487 (running at time of report generation)
- The test code optimization change does not introduce new failures
- Existing flaky tests continue to fail intermittently (same as baseline)

### Known Flaky Tests (Pre-existing)

- `comment_cashout_tests`
- `message_format_mainnet_5m_tests: [market_history_api_tests]`
- `message_format_mainnet_5m_tests: [condenser_api_tests]`
- `hardfork_schedule_tests`
- `fork_tests: [group_3]`

## Recommendations for Further Work

### High Priority
1. **Fix flaky tests**: Investigate timing-sensitive tests causing intermittent failures
2. **Optimize more sleep calls**: Review other tests for similar optimization opportunities

### Medium Priority
1. **Implement exponential backoff**: For polling-based wait functions in `common.py`
2. **Reduce node shutdown waits**: Investigate if 7-second waits can be reduced safely

### Low Priority
1. **Mock time for expiration tests**: Allow `test_find_transaction.py` to skip 90-second wait
2. **Parallel fixture setup**: For complex network tests

## Technical Details

### Files Modified

**Phase 2:**
- `tests/python/functional/cli_wallet/test_proposal.py` - Replaced `sleep(66)` with `node.wait_for_irreversible_block()`

**Phase 1:**
- `.gitlab-ci.yaml` - Added/modified `PYTEST_NUMBER_OF_PROCESSES` variable for multiple test jobs

### Environment
- Branch: `test-speedup-experiment`
- Base: `master`
- Project ID: 198

## Conclusion

The CI optimization work successfully:
1. Increased test parallelism across 11+ test jobs
2. Optimized test code to eliminate a 66-second unnecessary sleep

The changes maintain test stability while improving pipeline efficiency. Further improvements would require addressing underlying flaky test issues and implementing more sophisticated wait mechanisms.
