# Scoped Authorities Testing

**Status: implementation tests are being added. Baseline verification completed; feature verification remains pending.**

## Repository test framework

Consensus tests are Boost.Test unit tests under `tests/unit/tests`, built into `chain_test`; database and block fixtures live in `tests/unit/db_fixture`. Plugin API tests are in `tests/unit/plugin_tests`. Python integration tests and local test tooling are under `tests/python` and `tests/python/hive-local-tools`.

## Environment observed during initial audit

- GCC: 13.3.0
- CMake: unavailable (`cmake: command not found`)
- Ninja: unavailable
- Docker: unavailable
- Available memory: approximately 15 GiB; workspace filesystem has ample free space
- Git submodules: checked out, including `tests_api` and compression dictionaries

## Baseline verification

- Configured successfully with `cmake -S . -B build-baseline -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_HIVE_TESTNET=ON -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON`.
- Built the untouched testnet node with `ninja -C build-baseline -j4 hived` (success).
- Built the untouched consensus test target with `ninja -C build-baseline -j4 chain_test` (success).
- Ran `build-baseline/tests/unit/chain_test --run_test=config_tests`; all 3 test cases passed.

The documented CMake build cannot be configured in this environment as inspected. Install/build tooling was not changed. Recheck tools before attempting a build.

## Required test matrix

The implementation must add deterministic fixture tests for create authorization, under/equal/over per-transfer amounts, lifetime and period limits, period rollover, expiry boundaries, revocation, allowlist, asset mismatch, wrong actor, forged ID, multiple operations and transactions, transaction rollback, overflow, legacy transfer, unchanged active authorization, unauthorized operations, RC charging, hardfork boundary, replay, snapshot restoration, and grant isolation. Tests must check both resulting balances and all persistent counters.

## Commands (once CMake is available)

Use the repository's documented build presets/configuration and run the focused `chain_test` suite plus the full unit and Python integration suites. Exact build and local testnet commands must be recorded here after verifying them against this branch; none are claimed yet.

