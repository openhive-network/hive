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

## Grant lifecycle behavior (archived pre-consolidation validation)

At the lifecycle-hardening change, the focused suite passed 29 cases and 745 assertions. The complete `chain_test` run passed 526 cases and 1,835,530 assertions; one opt-in test was skipped. The separate RC benchmark passed 904 assertions. The changed lifecycle cases create 100 grants over confirmed blocks, reject grant 101, revoke 50 and create 50 replacements, expire and prune a full set, verify pruning rollback after a failed transaction and block pop, migrate a legacy grantor's high-water ID from retained rows, reject reused IDs after pruning, and accept `UINT32_MAX` once without wraparound.

In the former two-slot implementation, grant-creation RC was priced by the number of existing grant rows scanned for that grantor, plus rows created by earlier create operations in the same transaction. It charges the 64-byte metadata estimate only once when a grantor's issuance record is first created. Thus an empty grantor is not billed for a hypothetical 100-row scan, while the bounded work at a full grant set is still priced. The benchmark's resource vectors are unchanged because its source grantors are new; confirmed RC amounts vary slightly with the measured pool conditions. The 100-grant lifecycle fixtures use larger development-only vesting and confirm grants in separate blocks so their RC use is funded and block-by-block regeneration is represented.

The latest focused run passed all 24 scoped-authority and batch cases and 443 assertions. The opt-in RC benchmark was separately run with CSV output and passed all 904 assertions. Coverage includes block-confirmed create/transfer/revoke, per-transfer/period/lifetime accounting, exact period rollover, recipient restriction, actor-signature enforcement, wrong-asset and forged-ID rejection, attempted account-authority modification, transaction rollback, rejection at the exact expiration timestamp, revocation after 1/10/100 blocks, independent grants, multiple transactions in one block, and block-pop rollback. Batch coverage includes direct and scoped multi-input execution, 3-to-2 allocation, multiple isolated scoped grants, malformed/duplicate/unsorted entries, rollback after imbalance, block-pop rollback, 100-by-100 bounds, operation tags/authority classification, and serialized-size comparison.

The complete command is `build-baseline/tests/unit/chain_test --report_level=detailed`. Historical run on commit `3eb2561a9bb8c50f4c9ba5aebe2dfc4800398bfa` exited 0: 521 cases passed, 1 opt-in benchmark case was skipped, and 1,835,226 assertions passed. Under the former two-slot development schedule, the full run exited 0 with 526 cases and 1,835,530 assertions passed, with one opt-in case skipped; the benchmark was run separately. An earlier intermediate change expanded persistent RC statistics by operation tag and broke `basic_tests/chain_object_size` and `basic_tests/chain_object_checksum`. This was corrected: appended tags are folded into Hive's existing `multiop` RC statistics bucket, retaining the baseline `rc_stats_object` sizes (5632/5664) and checksum (`5ecebd9e709ff9f511dc2600e72c071e022223ca`). Per-operation resource calculation remains visitor-based. No expected object layout/checksum was changed to bless an incompatible layout.

At the initial implementation stage, `config_tests` passed all 3 cases and 51 assertions. The snapshot plugin's existing `snapshots_tests` suite passed all 12 cases and 963 assertions; those tests do not cover scoped records. Earlier pre-consolidation builds also passed the full plugin suite (102 cases, 7,134 assertions) and started the production-configured binary in a fresh local data directory with only `127.0.0.1:1` configured as a seed. That binary reported fresh genesis at head 0/HF0. This was not an operation-level runtime rejection test; the final consolidation pass rebuilt the mainnet binary but did not repeat signed operation injection. Populated snapshot evidence below is explicitly archived from the predecessor two-slot schedule and does not validate the unified schedule.

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

Instrumentation in block production recorded revoke transaction `4ecf04a88d26eb29cb57561c8f26824f4c0ec8e3`, expiration `2016-01-01T01:00:06`, pending head #2 at `2016-01-01T00:00:06`, next block time `2016-01-01T00:00:09`, and produced block #3 ID `00000003985bbf3c01807a2664363747a3cf20d8`. The producer logged successful reapplication and inclusion of that transaction. The grant was revoked in pending state and remained revoked after the block. No exception or undo of that transaction occurred. The historical revocation persistence report was not a Chainbase loss. In the consolidated first-grant regression, the initial test setup created two grants without enough RC for the second creation; the consensus rejection caused Boost to report an opaque exception. The test now funds and vests Alice before repeated grant creation, and checks the confirmed grant record through the typed grantor index. The full scoped suite passes. Historical spending remains 0.001 HBD across revoke and pop.

The signed grant/transfer/revoke/batch demonstration, populated-state restart, post-expiration rejection, signed multi-grant scoped-batch demonstration, and independent-node convergence have been exercised. Consolidated single-activation snapshot and replay results will be recorded in the final verification section below. The pinned Python packages installed from the checked-in lockfile in a project-local Python 3.14.8 environment. The generic `functional/hived/test_node_exit_options.py` test passed 2/2 after building `block_log_util`; it does not exercise experimental operations.

## Required test matrix

The current fixture coverage includes create authorization, exact/over per-transfer and period/lifetime limits, period rollover, exact-timestamp expiration, revocation across 100 blocks, allowlists, wrong signer, forged grant lookup, wrong asset, attempted active-authority change, multi-operation rollback, independent-grant isolation, historical tags, grant lifecycle pruning/ID non-reuse, and block-pop rollback. Dedicated RC payer/accounting tests pass. The historical snapshot/replay result used the prior two-slot schedule; consolidated-chain checks are reported separately below.

## Validated build and test commands

Build: `ninja -C build-baseline -j4 hived chain_test`.

Focused suite: `build-baseline/tests/unit/chain_test --run_test=scoped_authority_tests --report_level=detailed`.

Final reruns on 2026-10-17 passed: full chain (521 passed, 1 opt-in benchmark skipped, 1,835,226 assertions), focused Scoped Authority suite (24 cases, 443 assertions), config (3 cases, 51 assertions), and full plugin suite (102 cases, 7,134 assertions). The opt-in confirmed-block RC benchmark was rerun and passed 904 assertions; its fresh raw CSV is committed at `compressed_batch_rc_benchmark.csv`. Confirmed signed RPC scenarios and earlier two-node convergence are documented above; the most recent RC accounting changes were verified through consensus tests and the confirmed-block RC benchmark, but the complete signed demo and two-node scenario have not been repeated after those RC-only changes. The reproducible RC comparison is in [COMPRESSED_BATCH_RC_BENCHMARK.md](COMPRESSED_BATCH_RC_BENCHMARK.md).


### Archived pre-consolidation regression results

After the grant lifecycle change, `ninja -C build-baseline -j2 chain_test` completed and the full `chain_test --report_level=short --log_level=error` run passed 526/527 cases (one opt-in benchmark skipped), with 1,835,530/1,835,530 assertions. The focused scoped suite passed 29 cases / 745 assertions; `--run_test=config_tests` passed 3 cases / 51 assertions. `ninja -C build-baseline -j2 plugin_test` and the full plugin run passed 102 cases / 7,134 assertions. The opt-in RC benchmark passed 904 assertions; its 24 rows matched `compressed_batch_rc_benchmark_preconsolidation.csv` for deterministic measurements. The latest recorded run times and RSS differ as expected.

The production-configured binary was run in an isolated local environment. All four experimental operations were rejected by the mainnet evaluator gates; no public peers or production keys were used. The skip-VOPS protocol library builds and tag checks pass, while full-chain compilation fails at an existing virtual-operation construction omitted from that variant. An untouched upstream/develop checkout fails at the same `database.cpp` path, so this is pre-existing upstream behavior. There is no full skip-VOPS node or alternate-variant binary round trip.

### Archived two-slot snapshot, replay, and node synchronization evidence

The historical test chain used the now-superseded two-slot schedule. It is recorded for reference only and does not validate replay under the consolidated schedule. It contained multiple grantors and actors, active and revoked grants, an expired/pruned grant, partially spent counters, and replacement IDs. Snapshot restore into an independent directory and a clean genesis replay reached block 140, ID `0000008c2b79323b72b19a1527f66dcd5e89c7b4`; the compared head, balances, retained grant IDs, counters, status, and high-water metadata were identical. A separate replay reached block 145, ID `000000917b9f0e64c12ea96d0990565ed1f86cf8`.

An independent follower synced through block 145, stopped, then restarted after the primary confirmed revoke transaction `61bf37cdacaa9a7e602ad6cd29da31001538a632` in block 149 and replacement-grant transaction `524906e22f00630f2e23368f822e7e560eba5ad7` in block 150. After synchronization both nodes matched at block 153, ID `000000994bfe0725d45ed8fa4dc996fba64b001e`; Alice's high-water ID was 5 and retained grant IDs were 2, 4, and 5. State comparison was through API projections plus block ID, not a bytewise Chainbase dump.

The isolated production-configured node at HF0 rejected create, revoke, and scoped transfer with `Scoped authorities are disabled in mainnet builds`; compressed batch transfer was rejected with `Compressed batch transfers are disabled in mainnet builds`. The requests reached evaluators using a local threshold-zero `temp` account and did not use production keys or peers.

The latest pre-consolidation alternate-chain full suite passed 526 of 527 cases (one opt-in benchmark skipped), with 1,835,530/1,835,530 assertions. Focused scoped tests passed 29 cases / 745 assertions; config passed 3 / 51; RC benchmark passed 904 assertions. The full plugin rerun exited 0: 102/102 cases and 7,134/7,134 assertions. These results are historical and are superseded for the consolidated branch by the rerun below. Full skip-VOPS chain support and byte-for-byte comparison with an upstream executable remain unverified.


### Consolidated HF30 rerun (2026-10-18)

On `feature/scoped-authorities-unified-hf30`, `ninja -C build-baseline -j4 hived chain_test plugin_test` completed successfully, and `ninja -C build-mainnet -j4 hived` completed successfully. The rebuilt alternate-chain full test suite passed 526/527 cases (one opt-in RC benchmark skipped) and 1,835,539/1,835,539 assertions. Focused Scoped Authority/Batch tests passed 29/29 cases and 754/754 assertions; `config_tests` passed 3/3 cases and 51/51 assertions. The opt-in confirmed-block RC benchmark passed 904/904 assertions and rewrote `compressed_batch_rc_benchmark.csv`; all deterministic resource-vector, size, and RC columns match the preceding unified-HF30 CSV. This run measured 67–13,003 μs and 79,992–81,572 KiB process high-water RSS; those machine-dependent fields differed from the preceding sample. The rebuilt full plugin suite completed with **98/102 cases passing** and **7,090/7,134 assertions passing**; four cases failed (44 assertions): `get_account_history_hf12`, `get_account_history_hf13`, `get_ops_in_block_hf12`, and `get_ops_in_block_hf13`. They compare fixed transaction IDs and proof-of-work fields produced by fixture transactions. A focused rerun of `get_account_history_hf12` reproduced an expected transaction ID `dcb5cfd6d6e908f76199cc061aa1a654bbac9bdf` versus generated ID `c9a83dd7a36231d9375025ef49184bbd90822526`. These plugin fixtures are outside this consolidation diff; the failure is not yet confirmed against an untouched upstream build, so it remains unresolved rather than being attributed to upstream or treated as passing.

The current evaluator and generated protocol configuration contain no HF31 activation or migration. The consolidated branch has not yet been exercised through a fresh populated snapshot restore, a clean genesis replay, or a two-node sync after this consolidation; the archived predecessor results below do not substitute for those checks. The mainnet-configured executable was rebuilt, but no signed mainnet-runtime operation injection was repeated during this pass.

A fresh isolated node was started from `/tmp/hive-unified-hf30/alternate-chain-spec.json` with only a development witness key and local endpoints. Its schedule contained hardfork 30 at block 1. RPC reported `last_hardfork: 30`, version `1.30.0`, and head block 2 with ID `0000000257712c3768ca0346cb6f8f5aa9f6ebc5`; the process shut down cleanly. This confirms fresh-chain single-HF30 startup, but does not substitute for populated grant snapshot/replay validation.

The plugin failures are isolated to four historical account-history/ops-in-block golden-fixture cases; no plugin implementation files changed in the consolidation. A full untouched-upstream comparison was not completed, so whether the golden mismatch predates this branch remains unverified. Do not update the expected PoW transaction IDs merely to make these cases pass.
