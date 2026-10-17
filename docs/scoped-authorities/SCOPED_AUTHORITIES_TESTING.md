# Scoped Authorities Testing

**Status: development-only prototype. Testnet-configured `hived` and `chain_test` build. Do not connect this build to mainnet.**

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

The latest focused run passed all 24 scoped-authority and batch cases and 443 assertions. The opt-in RC benchmark was separately run with CSV output and passed all 904 assertions. Coverage includes block-confirmed create/transfer/revoke, per-transfer/period/lifetime accounting, exact period rollover, recipient restriction, actor-signature enforcement, wrong-asset and forged-ID rejection, attempted account-authority modification, transaction rollback, rejection at the exact expiration timestamp, revocation after 1/10/100 blocks, independent grants, multiple transactions in one block, and block-pop rollback. Batch coverage includes direct and scoped multi-input execution, 3-to-2 allocation, multiple isolated scoped grants, malformed/duplicate/unsorted entries, rollback after imbalance, block-pop rollback, 100-by-100 bounds, operation tags/authority classification, and serialized-size comparison.

The complete command is `build-baseline/tests/unit/chain_test --report_level=detailed`. The final full chain run after the sorted overlap-check optimization exited 0: 521 cases passed, 1 opt-in benchmark case was skipped, and 1,835,228 assertions passed; the benchmark was run separately with its environment flag. An earlier intermediate change expanded persistent RC statistics by operation tag and broke `basic_tests/chain_object_size` and `basic_tests/chain_object_checksum`. This was corrected: appended tags are folded into Hive's existing `multiop` RC statistics bucket, retaining the baseline `rc_stats_object` sizes (5632/5664) and checksum (`5ecebd9e709ff9f511dc2600e72c071e022223ca`). Per-operation resource calculation remains visitor-based. No expected object layout/checksum was changed to bless an incompatible layout.

`config_tests` passed all 3 cases and 51 assertions. The snapshot plugin's existing `snapshots_tests` suite passed all 12 cases and 963 assertions. Those tests do not cover scoped records; a separate populated one-grant snapshot round-trip is verified below. Final builds completed successfully with `ninja -C build-baseline -j4 hived chain_test plugin_test` and `ninja -C build-mainnet -j4 hived`. The full plugin suite exited 0 with 102 cases and 7,134 assertions; this includes 12 snapshot cases and 963 assertions. Explicit `config_tests` passed 3 cases and 51 assertions. The mainnet-configured binary was compiled, but a runtime RPC submission/rejection test against that binary remains unverified.

The current signed RPC demo used an isolated alternate chain in `/tmp/hive-scoped-batch-net`; its spec schedules HF30 at block 1. It was launched with witness production and database, condenser, wallet bridge, JSON RPC, and webserver APIs:

```sh
build-baseline/programs/hived/hived \
  --data-dir=/tmp/hive-scoped-batch-net \
  --alternate-chain-spec=/tmp/hive-scoped-batch-net/alternate-chain-spec.json \
  --shared-file-size=512M --block-log-split=0 \
  --plugin=debug_node --plugin=witness --plugin=database_api --plugin=condenser_api \
  --plugin=wallet_bridge_api --plugin=json_rpc --plugin=webserver \
  --witness='"initminer"' \
  --private-key=5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n \
  --enable-stale-production --required-participation=0 \
  --p2p-endpoint=127.0.0.1:1778 \
  --webserver-ws-endpoint=127.0.0.1:8090 \
  --webserver-http-endpoint=127.0.0.1:8091
```

The `cli_wallet` was opened against `ws://127.0.0.1:8090`, unlocked with a development-only local wallet, and used `sign_transaction` with the new operation names. It generated and broadcast normally signed transactions. Confirmed evidence:

- Alice created grant 1 for Bob, 5.000 TBD per transfer, 50.000 TBD per day, 250.000 TBD lifetime, merchant-only, expiring 2026-10-24. Transaction `72879e657ee571a1dbf411414d7433c1b58cc798` was included at block 48. `get_scoped_authorities` returned it with active status and zero counters.
- Bob signed a restricted transfer of 1.000 TBD to merchant. Transaction `b42f592e9fb9510e6f412dc20f98b5b272fee8f3` was included at block 50. Alice's HBD balance changed from 100.000 to 99.000 TBD; merchant's changed from 0 to 1.000 TBD; the grant lifetime and period counters became 1.000 TBD.
- A Bob-signed 6.000 TBD transfer was rejected at mempool application by the per-transfer assertion. The grant stayed at 1.000 TBD spent and no balance changed. After Alice's revoke transaction `f90b8e4c7654e41fec644c82a6547b4f2197b3b3` was included at block 55, another Bob-signed transfer was rejected by the revoked-state assertion; the counter remained 1.000 TBD.
- The node was cleanly stopped and restarted against the same data directory at block 75/76. RPC then reported grant 1 still revoked with 1.000 TBD historical spend and unchanged account balances. Grant 2 was also present with its counter and status intact. At block 97, after grant 2's expiry timestamp `2026-10-17T05:13:09`, Bob's signed 0.500 TBD transfer was rejected by the expiration assertion. Grant 2 remained queryable but inactive with zero spend; Alice, Bob, and merchant balances were unchanged. This verifies normal database restart persistence and post-expiry enforcement. Snapshot restoration and genesis replay were tested separately as described below.
- Alice signed `compressed_batch_transfer_operation` transaction `5bec9d138514ffecb02f2648fcd8263fed635719`, included at block 63. It debited 1.000 TESTS from Alice and credited 0.500 TESTS each to Bob and merchant. The resulting confirmed balances were Alice 99.000 TESTS, Bob 10.500 TESTS and merchant 0.500 TESTS.
- Bob signed a mixed batch using two independent scoped grants: Alice grant 3 sent 1.000 TBD to merchant and Bob grant 1 sent 1.000 TBD to Pedro. Transaction `3e1f6224b227388e49143ee2016e84b6207fe037` was included at block 132. RPC showed each grant's lifetime and period counters at 1.000 TBD; balances became Alice 98.000 TBD, Bob 4.000 TBD, merchant 2.000 TBD and Pedro 1.000 TBD. A follow-up batch requesting 1.001 TBD from Alice's 1.000 per-transfer grant was rejected; both grant counters and all four balances remained unchanged.
- Independent-node consensus check used a separate fresh chain with `--block-log-split=-1` (monolithic full block log), primary P2P endpoint `127.0.0.1:1788`, and peer endpoint `127.0.0.1:1789` seeded from the primary. Both nodes started from the same alternate-chain spec and synchronized from genesis. The primary included signed grant transaction `da5a36d100d9dd7ca0b9325d23c77bbd5f741c1a` at block 132, Bob's transfer `7d1eee08f2786619a93305cc7476642cfffac9b3` at block 136, and Alice's revoke `b46cbcf40e4672286c2254f3362bdd6ffb5df2b4` at block 140. Both nodes later reported head 142 with identical ID `0000008e88f5368244d318e57e77e3659765f100`, lifetime spend 1.000 TBD, `revoked: true`, Alice 9.000 TBD and merchant 1.000 TBD.

The development node reports chain ID `18dcf0a285365fc58b71f18b3d3fec954aa0c141c44e4e5cb4cf777b9eab274e`. Alternate-chain asset symbols are TESTS/TBD. RPC query example: `curl -sS -H 'Content-Type: application/json' -d '{"jsonrpc":"2.0","method":"database_api.get_scoped_authorities","params":{"grantor":"alice"},"id":1}' http://127.0.0.1:8091`.

Instrumentation in block production recorded revoke transaction `4ecf04a88d26eb29cb57561c8f26824f4c0ec8e3`, expiration `2016-01-01T01:00:06`, pending head #2 at `2016-01-01T00:00:06`, next block time `2016-01-01T00:00:09`, and produced block #3 ID `00000003985bbf3c01807a2664363747a3cf20d8`. The producer logged successful reapplication and inclusion of that transaction. The grant was revoked in pending state and remained revoked after the block. No exception or undo of that transaction occurred. The original reported disappearance was a test-fixture/assertion failure, not a Chainbase state loss: Boost's direct field assertion produced an invalid type diagnostic, and a later attempt in the same fixture exhausted Alice's RC while creating another grant. Tests now copy the field to `bool`, isolate expiry in a fresh fixture, and assert confirmed state. Historical spending remains 0.001 HBD across revoke and pop.

The signed grant/transfer/revoke/batch demonstration, populated-state normal restart, post-expiration rejection, signed multi-grant scoped-batch demonstration, and independent-node convergence on grant/counter/revocation state are verified. The separate mainnet-configured build also completed successfully. A populated scoped grant was dumped to a state snapshot and restored in an independent data directory; the restored node matched replayed state at block 157 / ID `0000009d4ad4b25bcb3e9e24ee60a90a7f3c09e2`, including grant 1 lifetime/period counters of 1.000 TBD and `revoked: true`. A clean database also replayed the same 157-block history from genesis and returned identical head and grant state. The snapshot/replay chain contained one grant, so multiple-grant state permutations remain unverified. Python integration remains unavailable: this checkout requires Python >=3.14 and post-release Hive packages from the configured GitLab package index; the machine has Python 3.12.3, and public PyPI offers beekeepy only through 1.28.6.

## Required test matrix

The current fixture coverage includes create authorization, exact/over per-transfer and period/lifetime limits, period rollover, expiration at the exact block timestamp, revocation across 100 blocks, allowlist enforcement, wrong signer, forged grant lookup, wrong-asset rejection, actor-signed attempted active-authority change, multi-operation transaction rollback with balance/counter assertions, three separate scoped transfers included in one block, independent-grant isolation, legacy transfer behavior, historical operation tags, and revoke block-pop rollback. Dedicated RC payer/accounting tests pass. Populated restart, single-grant snapshot restore, and genesis replay are verified as described above. Mainnet runtime operation rejection and multi-grant populated snapshot/replay remain untested.

## Validated build and test commands

Build: `ninja -C build-baseline -j4 hived chain_test`.

Focused suite: `build-baseline/tests/unit/chain_test --run_test=scoped_authority_tests --report_level=detailed`.

The final full chain, focused scoped, explicit config, and full plugin/snapshot suites passed as recorded above. Confirmed signed RPC scenarios and earlier two-node convergence are documented above; the most recent RC accounting changes were verified through consensus tests and the confirmed-block RC benchmark, but the complete signed demo and two-node scenario have not been repeated after those RC-only changes. The reproducible RC comparison is in [COMPRESSED_BATCH_RC_BENCHMARK.md](COMPRESSED_BATCH_RC_BENCHMARK.md).
