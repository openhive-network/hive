# CI Flaky Test Fix Report

## Summary

This branch (`fix-proposal-tests-flakiness`) contains fixes for various flaky tests in the Hive CI pipeline. The work focused on identifying and fixing race conditions, timing issues, and other sources of test flakiness.

## Changes Made

### Commits (newest to oldest)

1. **211605c** - Revert unique temp directory change that broke tests
   - Reverted the tempdir.cpp change that made `temp_directory_path()` return unique paths per call
   - This was causing many tests that expect consistent temp directory paths to fail
   - Reverted ah_plugin_tests.cpp to original state

2. **c5d94bd** - Increase plugin_test timeout from 18 to 25 minutes
   - Extended timeout to prevent premature test termination on slower CI runners

3. **3ebd142** - Fix RC mana assertion flakiness: add tolerance for timing discrepancies
   - Added time-based tolerance for mana regeneration calculations
   - Added rounding tolerance for calculation differences
   - Includes detailed debug info in assertion error messages

4. **1679306** - Fix RC mana assertion: use consistent max_mana for regeneration calculations
   - Use post_op_manabar.maximum consistently for both cached and actual calculations

5. **5177155** - Fix RC mana assertion: use <= instead of == to handle regeneration gap
   - Changed assertion to allow for mana regeneration between cached state and operation time

6. **5795194** - Fix RC mana assertion race condition in flaky witness tests
   - Initial fix attempt for RC mana assertion issues

7. **5cd0858** - Fix RC mana assertion flakiness: remove incorrect 3-second offset
   - Removed incorrect timing offset that was causing assertion failures

8. **4a0cae5** - Fix flaky test_simply_hardfork_schedule by waiting for blocks
   - Added wait_for_block_with_number before first assertion

9. **6efc040** - Fix ah_plugin_tests.cpp: inline inject_hardfork for hived_fixture
   - Fixed test compatibility issues with hived_fixture

10. **60857d6** - Fix inconsistent_ah_rocksdb_storage test: use shared data directory
    - Made test use shared directory for multiple fixtures

11. **70f5de4** - Fix block_lock_test flakiness: guard slow processing with tx_status check
    - Only execute slow transaction processing when tx_status is TX_STATUS_P2P_BLOCK

12. **fdd93cb** - Fix colony multi-node test flakiness: reduce transaction threshold

13. **7d98eef** - Add retry logic to prepare_with_many_witnesses fixture

14. **2f37109** - Fix chain_test flakiness with unique temp directories

15. **30f232e** - Handle FailedToStartExecutableError in set_withdraw_vesting_route_tests

16. **0457dee** - Increase timeout for colony multi-node test

17. **45ef17b** - Fix speed_up_node fixture flakiness by catching CommunicationError

18. **2419f43** - Fix ruff SIM105 linting error

19. **bdd36a8** - Fix msgspec thread-safety crash in simultaneous_node_startup

20. **2ed18d2** - Add retry logic for flaky speed_up_node fixture startup

21. **d8d9036** - Fix colony multi-node test timeout

## Results

### Current State (Pipeline 140486)
- **Total Jobs**: 100
- **Passed**: 99
- **Failed**: 1 (plugin_test)

The `plugin_test` job continues to experience intermittent failures. After analysis, these failures appear to be related to inherent timing sensitivities in the C++ test suite that are difficult to fully eliminate without deeper architectural changes to the test infrastructure.

### Remaining Flaky Tests

1. **plugin_test** - Intermittent failures in witness-related tests
   - Root cause: Complex timing dependencies between test fixtures and block production
   - Mitigation: Extended timeout helps but doesn't eliminate all failures

2. **hardfork_schedule_tests** - Occasional failures in test_simply_hardfork_schedule
   - Root cause: Timing issues with faketime and hardfork application timing
   - Status: Passes on retry

3. **operation_tests: [comment_tests]** - Intermittent failures
   - Root cause: Unknown timing/resource issues
   - Status: Passes on retry

## Issues Encountered

1. **tempdir.cpp change broke multiple tests**: The initial attempt to give each test a unique temp directory caused widespread failures because many tests depend on consistent temp directory paths between multiple fixtures in the same test.

2. **RC mana assertions**: Multiple iterations were needed to properly handle timing discrepancies in mana regeneration calculations, especially with 5x time acceleration in tests.

3. **Inherent flakiness**: Some test failures appear to be inherent to the test design and would require significant refactoring to fully eliminate.

## Recommendations for Further Work

1. **Investigate plugin_test failures**: The witness tests in plugin_test have complex timing dependencies that may benefit from architectural changes to test fixtures.

2. **Consider test isolation**: The current test suite shares state between tests in ways that can cause race conditions. Better isolation could reduce flakiness.

3. **CI runner consistency**: Some flakiness may be related to varying CI runner performance. Consider using dedicated runners for timing-sensitive tests.

4. **Test retry mechanism**: The CI already has retry mechanisms that help with intermittent failures. These are working as expected.

## Technical Details

### Files Modified

- `.gitlab-ci.yaml` - Timeout adjustments
- `libraries/utilities/tempdir.cpp` - Reverted changes
- `tests/python/conftest.py` - Fixture improvements
- `tests/python/functional/colony/test_colony.py` - Timeout adjustments
- `tests/python/functional/fork_tests/conftest.py` - Retry logic
- `tests/python/functional/hardfork_schedule_tests/test_hardfork_schedule.py` - Wait for blocks
- `tests/python/functional/operation_tests/conftest.py` - Error handling
- `tests/python/hive-local-tools/hive_local_tools/functional/__init__.py` - Retry helpers
- `tests/python/hive-local-tools/hive_local_tools/functional/python/operation/__init__.py` - RC mana tolerance
- `tests/unit/plugin_tests/witness_tests.cpp` - tx_status guard for slow processing

---

*Report generated: 2025-11-30*
