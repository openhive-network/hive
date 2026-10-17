# Scoped Authorities Testing

**Status: development-only prototype. The modified `hived` and `chain_test` build, and all 506 chain unit cases pass. This is not production-ready and must not be connected to mainnet.**

## Repository test framework

Consensus tests are Boost.Test unit tests under `tests/unit/tests`, built into `chain_test`; database and block fixtures live in `tests/unit/db_fixture`. Plugin API tests are in `tests/unit/plugin_tests`. Python integration tests and local test tooling are under `tests/python` and `tests/python/hive-local-tools`.

## Environment observed during initial audit

- GCC: 13.3.0
- CMake: `/usr/bin/cmake`
- Ninja: `/usr/bin/ninja`
- Docker: unavailable
- Available memory: approximately 15 GiB; workspace filesystem has ample free space
- Git submodules: checked out, including `tests_api` and compression dictionaries

## Baseline verification

- Configured successfully with `cmake -S . -B build-baseline -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_HIVE_TESTNET=ON -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON`.
- Built the untouched testnet node with `ninja -C build-baseline -j4 hived` (success).
- Built the untouched consensus test target with `ninja -C build-baseline -j4 chain_test` (success).
- Ran `build-baseline/tests/unit/chain_test --run_test=config_tests`; all 3 test cases passed.

The focused command is `build-baseline/tests/unit/chain_test --run_test=scoped_authority_tests --report_level=detailed`.

The latest focused run passed all 10 cases and 180 assertions. Coverage includes block-confirmed create/transfer/revoke, per-transfer/period/lifetime accounting, exact period rollover, recipient restriction, actor-signature enforcement, wrong-asset and forged-ID rejection, attempted account-authority modification, transaction rollback, rejection at the exact expiration timestamp, revocation after 1/10/100 blocks, independent grants, multiple transactions in one block, and block-pop rollback.

The complete command is `build-baseline/tests/unit/chain_test --report_level=detailed`. The latest run passed all 508 test cases and 1,834,977 assertions. The prior detailed run had two failing cases and three assertions: `basic_tests/chain_object_size` and `basic_tests/chain_object_checksum` expected the old `rc_stats_object` layout. Adding operation-tag-indexed RC stats through the appended scoped operation increases that object size; the expected size and decoded checksum are now updated and the full suite passes.

`config_tests` passed all 3 cases and 51 assertions. The build command `ninja -C build-baseline -j4 hived chain_test` completed successfully after the changes.

The standalone node used for the RPC smoke test was started with the alternate-chain spec at `/tmp/hive-scoped-authority-rpc-demo/alternate-chain-spec.json` and the following command (the alternate chain was configured to schedule HF30 at block 1):

```sh
build-baseline/programs/hived/hived \
  --data-dir=/tmp/hive-scoped-authority-rpc-demo \
  --alternate-chain-spec=/tmp/hive-scoped-authority-rpc-demo/alternate-chain-spec.json \
  --shared-file-size=512M --block-log-split=0 \
  --plugin=debug_node --plugin=database_api --plugin=json_rpc --plugin=webserver \
  --p2p-endpoint=127.0.0.1:1776 --webserver-http-endpoint=127.0.0.1:8091
```

The observed RPC query returned `{"grants":[]}` for `initminer`, before and after a clean restart. No grant transaction was submitted through this standalone process.

Instrumentation in block production recorded revoke transaction `4ecf04a88d26eb29cb57561c8f26824f4c0ec8e3`, expiration `2016-01-01T01:00:06`, pending head #2 at `2016-01-01T00:00:06`, next block time `2016-01-01T00:00:09`, and produced block #3 ID `00000003985bbf3c01807a2664363747a3cf20d8`. The producer logged successful reapplication and inclusion of that transaction. The grant was revoked in pending state and remained revoked after the block. No exception or undo of that transaction occurred. The original reported disappearance was a test-fixture/assertion failure, not a Chainbase state loss: Boost's direct field assertion produced an invalid type diagnostic, and a later attempt in the same fixture exhausted Alice's RC while creating another grant. Tests now copy the field to `bool`, isolate expiry in a fresh fixture, and assert confirmed state. Historical spending remains 0.001 HBD across revoke and pop.

The local node served `database_api.get_scoped_authorities` over HTTP and returned an empty grant list; the same empty-state node restarted cleanly and served the query again. This does not prove persistence of a non-empty grant across restart. No Python integration suite, signed RPC transaction demo, snapshot restoration, or full replay determinism check has completed. Source guards reject scoped operations in non-alternate-chain builds and the development hardfork schedule is available only in alternate-chain builds; a separate mainnet-configured build was not compiled.

## Required test matrix

The current fixture coverage includes create authorization, exact/over per-transfer and period/lifetime limits, period rollover, expiration at the exact block timestamp, revocation across 100 blocks, allowlist enforcement, wrong signer, forged grant lookup, wrong-asset rejection, actor-signed attempted active-authority change, multi-operation transaction rollback with balance/counter assertions, three separate scoped transfers included in one block, independent-grant isolation, legacy transfer behavior, historical operation tags, and revoke block-pop rollback. Dedicated RC charging, mainnet rejection in a mainnet-configured build, populated restart, snapshot restore, and replay determinism remain untested.

## Validated build and test commands

Build: `ninja -C build-baseline -j4 hived chain_test`.

Focused suite: `build-baseline/tests/unit/chain_test --run_test=scoped_authority_tests --report_level=detailed`.

The full chain suite and focused suite pass as recorded above. RPC response smoke testing passed for an empty grant set. Full signed transaction execution through a standalone testnet and Python integration results remain pending.

