# Historical code review: pre-consolidation prototype

> This review snapshot describes the predecessor branch, which staged grant lifecycle rules in a second development slot. It is retained as history only. The current consolidation branch has one provisional HF30 activation; current source, statistics, and validation supersede this historical inventory.

## Scope and review result

The following tables and findings capture the pre-consolidation implementation and are not a description of current activation rules. They are preserved for audit history. The current branch consolidates all operations and lifecycle behavior under one alternate-chain HF30 gate; no source path depends on a later experimental hardfork.

The review found two needless evaluator steps and three extra trailing blank lines in feature documentation. The evaluator cleanup removed a post-mutation database lookup/assert that could not establish anything beyond the successful return from `database::modify`, and removed a reference kept alive only with `(void)`. Account lookup and all validation remain. No consensus semantics or public serialization fields changed. No broader refactor was justified.

## Exact change statistics

Reference: `upstream/develop` (`9d600761564865ac9b4accb3ec59f204c5ee4797`). Implementation HEAD before this review: `222ee085f05b555970aae7e29058cd4603cb590a`.

| Measure | Initial prototype | Pre-consolidation lifecycle branch |
|---|---:|---:|
| Commits ahead of upstream | 8 | 9 after local verification commit |
| Changed files | 41 | 47 |
| New files | 16 | 19 |
| Modified files | 25 | 28 |
| Deleted files | 0 | 0 |
| Added lines | 2,556 | 2,998 |
| Deleted lines | 30 | 39 |
| Net lines | 2,526 | 2,959 |
| Production C++ source additions | 532 | 646 |
| Header additions | 252 | 316 |
| Test additions | 1,037 | 1,214 |
| Markdown additions | 692 | 738 |
| Benchmark CSV additions | 25 | 50 |
| Other build/config additions | 18 | 34 |
| New regular protocol operations | 4 | 4 |
| New persistent consensus object types | 1 | 2 |
| Scoped-authority test cases | 24 at prior review | 29 (including one opt-in RC benchmark) |

These are historical pre-consolidation statistics. They do not describe the unified branch. The per-file inventory below likewise records the predecessor implementation and is superseded by the consolidation statistics at the end of this document.

### Per-file inventory

`+`/`−` counts below are regenerated from the staged diff against `upstream/develop`; build products and untracked test artifacts are excluded.

| File path | Added lines | Deleted lines | Purpose | Security importance |
|---|---:|---:|---|---|
| `docs/scoped-authorities/COMPRESSED_BATCH_RC_BENCHMARK.md` | 77 | 0 | RC benchmark method, resource accounting, results and reproduction command | Low |
| `docs/scoped-authorities/COMPRESSED_BATCH_TRANSFER.md` | 34 | 0 | Batch wire behavior, limits, validation and API examples | Low |
| `docs/scoped-authorities/FINAL_CODE_REVIEW.md` | 239 | 0 | Complete inventory, walkthrough, audit findings and evidence | Low |
| `docs/scoped-authorities/HF30_UPSTREAM_PROTOCOL_PROPOSAL.md` | 56 | 0 | Experimental protocol proposal and upstream review requirements | Low |
| `docs/scoped-authorities/SCOPED_AUTHORITIES_API.md` | 71 | 0 | Grant create, inspect, transfer and revoke request examples | Low |
| `docs/scoped-authorities/SCOPED_AUTHORITIES_CHANGELOG.md` | 39 | 0 | Feature file inventory and implementation history | Low |
| `docs/scoped-authorities/SCOPED_AUTHORITIES_HARDFORK.md` | 24 | 0 | Alternate-chain HF30 activation and deployment compatibility notes | Low |
| `docs/scoped-authorities/SCOPED_AUTHORITIES_SECURITY.md` | 47 | 0 | Threat model, privilege boundaries and known security limitations | Low |
| `docs/scoped-authorities/SCOPED_AUTHORITIES_SPEC.md` | 62 | 0 | Scoped grant schema, operations, semantics and protocol constraints | Low |
| `docs/scoped-authorities/SCOPED_AUTHORITIES_TESTING.md` | 89 | 0 | Build, test, local node and known validation evidence | Low |
| `docs/scoped-authorities/compressed_batch_rc_benchmark.csv` | 25 | 0 | Raw per-shape/per-mode RC and serialized-size measurements | Low |
| `docs/scoped-authorities/compressed_batch_rc_benchmark_preconsolidation.csv` | 25 | 0 | Raw pre-consolidation RC benchmark results | Low |
| `libraries/chain/CMakeLists.txt` | 1 | 0 | Compile and register new consensus state index | High |
| `libraries/chain/database_hardfork.cpp` | 16 | 0 | Development-only HF30 configuration and activation | Critical |
| `libraries/chain/database_scoped_authority.cpp` | 26 | 0 | Register and instantiate the Chainbase grant index | Critical |
| `libraries/chain/hive_evaluator_transfer.cpp` | 285 | 0 | Consensus grant/revoke/transfer/batch validation and state changes | Critical |
| `libraries/chain/include/hive/chain/detail/state/scoped_authority_grantor_metadata_object.hpp` | 31 | 0 | Persistent per-grantor grant-ID high-water metadata | Critical |
| `libraries/chain/include/hive/chain/detail/state/scoped_authority_object.hpp` | 59 | 0 | Persistent grant record and deterministic indexes | Critical |
| `libraries/chain/include/hive/chain/detail/state/scoped_authority_object_multiindex.hpp` | 55 | 0 | Persistent grant record and deterministic indexes | Critical |
| `libraries/chain/include/hive/chain/hive_object_types.hpp` | 9 | 0 | Protocol integration | Medium |
| `libraries/chain/include/hive/chain/rc/rc_stats.hpp` | 10 | 1 | Keep operation statistics within the stable RC stats layout | High |
| `libraries/chain/include/hive/chain/rc/rc_utility.hpp` | 2 | 1 | Pre-consolidation grant lifecycle implementation and verification | High |
| `libraries/chain/include/hive/chain/rc/resource_count.hpp` | 2 | 1 | Pre-consolidation grant lifecycle implementation and verification | High |
| `libraries/chain/index.cpp` | 2 | 0 | Compile and register new consensus state index | High |
| `libraries/chain/rc/rc_objects.cpp` | 11 | 2 | Keep operation statistics within the stable RC stats layout | High |
| `libraries/chain/rc/rc_utility.cpp` | 1 | 1 | Pre-consolidation grant lifecycle implementation and verification | High |
| `libraries/chain/rc/resource_count.cpp` | 77 | 6 | Per-operation RC resource accounting | High |
| `libraries/chain/rc/resource_user.cpp` | 12 | 0 | Select RC payer for active/scoped batches | High |
| `libraries/plugins/apis/account_history_api/account_history_api.cpp` | 2 | 0 | Index and filter new operation types in account history | Medium |
| `libraries/plugins/apis/account_history_api/include/hive/plugins/account_history_api/account_history_api.hpp` | 4 | 0 | Index and filter new operation types in account history | Medium |
| `libraries/plugins/apis/database_api/database_api.cpp` | 1 | 0 | Register and serialize grant inspection API structures | Medium |
| `libraries/plugins/apis/database_api/database_api_financial.cpp` | 50 | 0 | Read-only grant inspection API implementation | Medium |
| `libraries/plugins/apis/database_api/include/hive/plugins/database_api/database_api.hpp` | 1 | 0 | Register and serialize grant inspection API structures | Medium |
| `libraries/plugins/apis/database_api/include/hive/plugins/database_api/database_api_args.hpp` | 13 | 0 | Register and serialize grant inspection API structures | Medium |
| `libraries/plugins/apis/database_api/include/hive/plugins/database_api/database_api_impl.hpp` | 1 | 0 | Register and serialize grant inspection API structures | Medium |
| `libraries/plugins/apis/database_api/include/hive/plugins/database_api/database_api_objects.hpp` | 25 | 0 | Register and serialize grant inspection API structures | Medium |
| `libraries/protocol/forward_impacted.cpp` | 49 | 0 | Affected-account and impacted-balance visitor integration | High |
| `libraries/protocol/hardfork.d/0-preamble.hf` | 1 | 1 | Development-only HF30 configuration and activation | Critical |
| `libraries/protocol/hardfork.d/1_30.hf` | 16 | 0 | Development-only HF30 configuration and activation | Critical |
| `libraries/protocol/hardfork.d/1_31.hf` (removed during consolidation) | 16 | 0 | Alternate-chain-only grant lifecycle hardfork definition | Critical |
| `libraries/protocol/hive_operations.cpp` | 114 | 0 | Protocol field validation, ordering and conservation checks | Critical |
| `libraries/protocol/include/hive/protocol/config.hpp` | 12 | 2 | Development-only HF30 configuration and activation | Critical |
| `libraries/protocol/include/hive/protocol/hive_operations.hpp` | 85 | 0 | Protocol operation schema, authority declaration, reflection and stable tags | Critical |
| `libraries/protocol/include/hive/protocol/operations.hpp` | 7 | 0 | Protocol operation schema, authority declaration, reflection and stable tags | Critical |
| `tests/unit/plugin_tests/condenser_api_account_history_tests.cpp` | 12 | 12 | Update plugin golden output for operation encoding/fixture output | Medium |
| `tests/unit/plugin_tests/condenser_api_ops_in_block_tests.cpp` | 12 | 12 | Update plugin golden output for operation encoding/fixture output | Medium |
| `tests/unit/tests/scoped_authority_tests.cpp` | 1190 | 0 | Consensus tests for grants, limits, batches, rollback, tags and RC | High |

The largest net additions are `tests/unit/tests/scoped_authority_tests.cpp` (1190 lines), `libraries/chain/hive_evaluator_transfer.cpp` (285 lines), `docs/scoped-authorities/FINAL_CODE_REVIEW.md` (239 lines), `libraries/protocol/hive_operations.cpp` (114 lines). The evaluator and tests are consensus-sensitive and should remain easy to navigate; further decomposition should be considered only if it improves reviewability.

## Plain-English walkthrough

A Scoped Authority is a stored spending contract. Alice signs a grant naming Bob (or a service account such as `hivedebit`), the asset, limits, validity interval, and optional recipients. Bob does not receive Alice’s active key or authority. He signs a restricted transfer with his own active authority. Consensus resolves the grant and checks the proposed transfer before changing Alice’s balance or the grant counters.

A compressed batch is one transaction that names several source accounts and several destinations for one asset. Each ordinary source must authorize its own debit. A scoped source instead names a grant, and the delegated actor authorizes that input. The validator conserves the exact integer amount and deterministically attributes each source to destinations in sorted order, applying that grant’s recipient allowlist to the attributed portions. All checks happen before balance or counter writes.

## Scoped Authorities: implementation flow

### Create

`create_scoped_authority_operation` is defined and reflected in `libraries/protocol/include/hive/protocol/hive_operations.hpp`; its limits, symbols, timestamps, duration, and canonical sorted recipient list are checked in `libraries/protocol/hive_operations.cpp::create_scoped_authority_operation::validate`.

Its required active authority is the grantor. `create_scoped_authority_evaluator::do_apply` in `libraries/chain/hive_evaluator_transfer.cpp` confirms that both accounts exist, enforces the grantor's record limit and grant-ID rule, checks future expiration, and creates the Chainbase object. In the predecessor branch, separate experimental gates changed grant lifecycle rules; the unified branch applies the 100-live-grant cap, pruning, and persistent high-water IDs at HF30. A grant can be created only after the development HF gate on alternate-chain builds; production-configured evaluator builds reject it.

### Authenticate and transfer

`scoped_transfer_operation` requests the actor’s active authority, not the grantor’s. The transaction authority checker continues to use Hive’s standard signature and account-authority recursion rules. The evaluator verifies that the grant belongs to the named grantor, is assigned to the actor, is not revoked, is within `[valid_from, expires_at)`, allows the asset and recipient, and has sufficient per-transfer, current-period, and lifetime allowance and grantor balance.

Balances are changed through the existing `_db.adjust_balance` transfer mechanism. The evaluator updates `period_spent` and `lifetime_spent` only after validation and balance adjustment. The transaction’s normal undo session rolls both back if any later operation fails; block undo restores the previous state on a pop.

### Time and accounting

The period index is integer division of `(consensus head timestamp − valid_from)` by `period_seconds`. `valid_from` is checked before subtraction; no wall clock is consulted. A period counter is treated as zero when the computed period differs from the stored period index, and the object is advanced only when a successful transfer occurs. Lifetime spending is never reset. Expiration is a strict upper boundary: a grant is unusable when `now == expires_at`.

Asset arithmetic uses the integer `share_type` amount. Validation caps grants and batch sums at `HIVE_MAX_SHARE_SUPPLY`; evaluator checks counters are nonnegative and not already above their limits, then compares against remaining allowance before addition. HIVE and HBD are separate symbols and no price conversion is performed.

### Revoke and persisted state

`revoke_scoped_authority_operation` requests grantor active authority. The evaluator finds the `(grantor, grant_id)` record and sets `revoked`; it leaves all spend counters intact. The record is immutable except counters and revoke status, preventing grant updates from bypassing prior spending.

`scoped_authority_object` persists actor, grantor, identifier, asset limits, period duration/index, validity timestamps, recipient list, lifetime and period counters, and revoked status. `scoped_authority_index` provides deterministic lookup by `(grantor, grant_id)` and actor lookup by `(actor, grantor, grant_id)`. New object type and index registration are in `hive_object_types.hpp`, `database_scoped_authority.cpp`, and `index.cpp`. The account grant cap bounds retention: expired and revoked entries are retained, but one grantor cannot accumulate more than 100.

### RC payer and charging

Per-operation RC resource measurement is in `libraries/chain/rc/resource_count.cpp`. Creation charges a persistent-state estimate based on a fixed record allowance plus the bounded recipient list. Revoke and direct transfer charge transfer execution. A batch charges execution weight per input/output and an additional transfer weight for each scoped grant input, plus the existing transaction byte, authority/signature, and market-operation resources.

`resource_user.cpp` selects the actor as payer for a batch containing scoped input; a batch using only active inputs charges its first sorted source. `rc_stats.cpp` maps new high operation tags to the stable multi-operation statistics slot. This affects reporting only; it does not replace resource-vector calculation. The report records the benchmark methodology and raw measurements in the existing benchmark docs/CSV. Do not interpret fixture costs as a production RC guarantee: the test fixture’s vesting/RC pools and estimator weights are development conditions.

## Compressed Batch Transfers

### Exact protocol representation

The wire operation is the reflected `compressed_batch_transfer_operation` in `hive_operations.hpp`, with reflected fields in this exact order: `actor`, `symbol`, `inputs`, `outputs`, `memo`. The batch input record is `(from, amount, grant_id)`; the output record is `(to, amount)`. The single shared `asset_symbol_type` is packed by Hive’s existing symbol serializer (four bytes for the current HIVE/HBD NAI symbols). Account names use Hive’s `fixed_string<16>` raw serializer, which serializes the logical string with an FC length prefix and bytes. Vector lengths use FC `unsigned_int`; the amount is signed 64-bit integer serialization, and grant IDs are 32-bit integer serialization. The operation’s variant tag is first, as for all operations. The enclosing transaction supplies its normal reference block, expiration, extensions, and signatures.

No local database IDs are sent. No bespoke varint/delta encoding or account dictionary was added. FC serialization, operation reflection, and existing canonical validation define the bytes. Input and output order must be strict lexicographic account-name order; duplicates are rejected. This encoding is deterministic across nodes using the same Hive serialization mode and build protocol.

### Validation and execution

`compressed_batch_transfer_operation::validate` in `hive_operations.cpp` enforces 1–100 inputs and outputs, valid account names, positive bounded amounts, strictly increasing source/destination lists, equal input/output sums, permitted HIVE/HBD symbol, actor presence iff any scoped input exists, and no source/destination overlap. The sorted overlap check is linear.

`compressed_batch_transfer_evaluator::do_apply` in `hive_evaluator_transfer.cpp` resolves each source, validates normal active authority via the transaction authority set or validates that source’s independent grant and actor. It verifies balance sufficiency, grant status, asset, time, limits, and each attributed allowlist. Only after all checks does it debit every source, credit every destination, and update each grant counter. The amount conservation rule is exact; no fees, minting, or conversion are part of this operation.

### Attribution rule and examples

Attribution is a deterministic two-pointer walk over the strictly sorted vectors. In account-name order, each source contributes to the first destination with remaining demand; when that output is filled, the next source continues at the next output. A scoped grant’s recipient allowlist is checked for every output receiving a positive portion from that source. Source/destination overlap is prohibited, avoiding the special case where the same account is both a net source and destination.

- **1 → 3:** Alice contributes 100 units; destinations `carlos`, `diana`, `pedro` require 30, 20, 50. Alice’s 100 is attributed in that order; if she uses a scoped grant, all three recipients must be allowed.
- **1 → 100:** Alice’s amount is apportioned across the 100 sorted output accounts in order. Each touched recipient is checked against Alice’s grant; the operation remains bounded to 100 entries.
- **3 → 2:** sorted Alice and Bob source amounts are attributed sequentially to the first and second outputs. If Alice’s grant only allows Carlos and her source portion reaches Diana, validation fails before any debit.
- **100 → 100:** each sorted source funds outputs from the current output index forward until its amount is exhausted. Work is bounded by the entries plus attribution intersections (at most `inputs + outputs` positive segments because every iteration exhausts either a source or an output). Each scoped input is checked and accounted independently.

A restricted source cannot implicitly fund a forbidden recipient through pooled settlement: attribution is part of consensus validation, not inferred from final net balances. Reordering an entry is rejected as noncanonical; changing valid order is a different transaction and cannot avoid the per-source allowlist check.

### Why fewer bytes / remaining costs

The format stores one symbol and memo per batch rather than one per legacy transfer, and one outer transaction/signature envelope can cover many outputs. With active multi-input batches, each source still needs authority and a signature; the byte savings do not erase that cost. A scoped batch can use one actor signature for grants from multiple grantors, but each grant is separately validated, updated, and charged. An earlier isolated size check recorded 3,878 bytes for 100 standard transfers versus 1,809 for the equivalent batch, but did not use the later RC benchmark methodology. In the current controlled benchmark, the 1→100 row is 5,678 bytes for one standard multi-transfer transaction versus 2,115 for an active batch; the 100→100 row is 12,313 versus 10,926, or 4,501 for the scoped batch. The CSV is authoritative for the measured comparisons.

RC savings arise from fewer transaction envelopes/signatures and fewer repeated operation metadata bytes, partially offset by per-input/output evaluator weights. They are not a claim that 100 balance movements cost one movement. Remaining bottlenecks are signature/authority checks for active multi-source batches, account and grant index reads, balance/counter writes, history/index work, and Chainbase undo/state costs. The benchmark uses one timing sample per shape and reports fixture wall time/RSS; it is not a controlled throughput benchmark.

## Integration points and code paths

| Purpose | Code path |
|---|---|
| Define operations and authority requirements | `libraries/protocol/include/hive/protocol/hive_operations.hpp` |
| Validate fields, ordering and conservation | `libraries/protocol/hive_operations.cpp` |
| Preserve historical tags and classify signed operations | `libraries/protocol/include/hive/protocol/operations.hpp`; `libraries/protocol/operations.cpp::is_virtual_operation` |
| Apply create/revoke/scoped transfer/batch | `libraries/chain/hive_evaluator_transfer.cpp` |
| Persist grant schema and indexes | `libraries/chain/include/hive/chain/detail/state/scoped_authority_object*.hpp`; `libraries/chain/database_scoped_authority.cpp` |
| Gate development activation | `libraries/protocol/hardfork.d/1_30.hf`; `libraries/chain/database_hardfork.cpp`; evaluator guard |
| Calculate RC and select payer | `libraries/chain/rc/resource_count.cpp`; `libraries/chain/rc/resource_user.cpp`; `libraries/chain/rc/rc_objects.cpp` |
| Read grant state | `libraries/plugins/apis/database_api/database_api_financial.cpp::get_scoped_authorities` |
| Include operations in account history | `libraries/protocol/forward_impacted.cpp`; account-history API visitor/filter |

## Critical audit findings

- **Operation tags:** four operations are appended after all existing regular and virtual alternatives. Existing alternatives are not reordered. They derive from `base_operation`; `is_virtual_operation` calls the operation’s `is_virtual()` implementation rather than assuming tag position. The tagged test pins legacy values, scoped tags, and non-virtual classification. `HIVE_PROTOCOL_SKIP_VOPS` omits the existing VOP alternatives in that build as before; new tags are appended in that configuration too. Cross-build binary protocols still depend on matching Hive serialization/build configuration as they did before.
- **Mainnet isolation:** HF30 is included only in the alternate-chain hardfork count; mainnet remains at HF28. Scoped evaluator guards compile to an unconditional hardfork assertion in non-alternate builds. This is an implementation safeguard, not proof from a live production node; do not connect experimental software to mainnet or enable the feature without an assigned approved hardfork.
- **Authority:** grant create/revoke require grantor active authority; scoped transfer requires actor active authority; ordinary batch inputs require every source active authority. Standard Hive key/account recursion remains in force. No grantor active authority is added for scoped operations.
- **State/rollback:** Under the unified HF30 rules, records are pruned only when revoked or expired, and high-water ID metadata persists permanently. Mutations use normal Chainbase create/modify/remove and transaction undo. No scheduler or local clock is used.
- **Arithmetic/periods:** protocol bounds and remaining-limit subtraction precede additions; period rollover is lazy and timestamp-derived. The state invariant checks protect against malformed persisted counters. Fixed-window anchoring makes the first period begin at `valid_from`.
- **Batch attribution:** the ordered allocation is explicit and tested for restricted/unrestricted mixtures, separate grants, split recipients, and ordering. Overlap is rejected. The design is auditable but should receive focused independent review because recipient authorization depends on this rule.
- **RC:** usage is charged per batch input/output and scoped grant access. Grant creation is charged for retained rows scanned plus first-time metadata creation under the unified activation. These weights remain estimates and require protocol economics review. RC operation-stat aggregation uses the historic multiop slot to avoid changing persistent `rc_stats_object` array size; per-operation values are not visible as their own bucket. Validate upstream reporting expectations.
- **API/history:** read-only grant API enumerates at most the per-grantor cap; the history bit filters add high-order enum bits, which consumers must preserve as 64-bit values. The final plugin rerun found four historical account-history/ops-in-block golden-fixture failures involving fixed transaction IDs and generated proof-of-work fields. They are not in the consolidation diff, and an untouched-upstream run was not completed; they remain unresolved and are not attributed to upstream without that comparison.
- **Replay/snapshot:** the object is a new Chainbase type appended to the object type enum and requires nodes to use the modified binary for state that contains grants. Earlier evidence covered a one-grant snapshot and replay, not a complete production migration. The object-size change is expected for the new object type; this does not establish compatibility with old binaries or snapshots that contain grants.
- **Resource bounds:** max grants 100 per grantor, 32 recipients per grant, and 100 batch inputs/outputs bound database and evaluator work. Batch validation uses sorted vectors and linear attribution/overlap walks. Account history and normal transaction payload limits still apply.

No additional change was made to consensus semantics during this review. The branch remains experimental and needs independent security and economics audits.

## Code-style cleanup

- Removed the unused `grantor` reference plus `(void)grantor` in grant creation; the account existence lookup remains.
- Removed a second lookup and assertion immediately after revocation’s successful `database::modify`; this was redundant and was not a substitute for a persistence test.
- Removed surplus blank lines at EOF in three feature Markdown files.
- Preserved comments that explain timestamp replay semantics, source-to-recipient attribution, experimental gating, operation-tag stability, and RC state-layout constraints.
- No helper or abstraction refactor was applied: the security-critical evaluator is cohesive and existing transfer balance functions are reused.

## Verification

All results below were executed after the source cleanup on this checkout.

- `ninja -C build-baseline -j4 hived chain_test plugin_test`: passed.
- `ninja -C build-mainnet -j4 hived`: passed with the production configuration.
- Full `build-baseline/tests/unit/chain_test --report_level=detailed`: 521 of 522 cases passed, one opt-in test skipped, and 1,835,228 of 1,835,228 assertions passed. The three configuration cases were included: 3/3 cases and 51 assertions passed.
- Focused `scoped_authority_tests`: 24/24 cases and 443 assertions passed. The full-suite skip is expected because the benchmark case needs an output CSV path; the focused run included it with the guard path and the separate benchmark below exercised it fully.
- Full `build-baseline/tests/unit/plugin_test --report_level=detailed`: 102/102 cases and 7,134/7,134 assertions passed.
- The opt-in RC benchmark completed with 904/904 assertions. A fresh 24-row CSV was written under `/tmp/hive-scoped-review-final/rc-after-cleanup.csv`; compared with the checked-in CSV, all shape, byte, signature, resource-vector, charged-RC, grant-setup-RC, and balance-write columns matched. Wall-clock time and process RSS varied, as expected for single-run measurements. The checked-in CSV was not overwritten.
- Python node-exit integration smoke tests passed 2/2 using Python 3.14.8 and the isolated Poetry environment, with `HIVE_BUILD_ROOT_PATH` set to `build-baseline`. The initial run without that variable failed before node startup because the harness could not locate `hived`; the configured rerun passed. Two upstream dependency deprecation warnings remain.
- `ctest --test-dir build-baseline -N` lists no registered CTest cases; the Boost test executables were run directly.
- The `HIVE_PROTOCOL_SKIP_VOPS` configuration was source-audited but no separate build with that macro was available. The production-configured build compiled successfully, but runtime submission rejection of the four experimental operations was not exercised. Prior development signed-RPC/two-node evidence is recorded in feature documentation, but no fresh signed-RPC or two-node run was performed as part of this cleanup review.
- The snapshot, replay, and node-synchronization results later in this historical review used the predecessor two-slot schedule and must not be treated as unified-activation verification.

## Diff summary and upstream readiness

The statistics in this historical section describe the predecessor branch, not the consolidated diff. Current branch statistics and validation are recorded in the consolidation update at the end of the document.

Recommendation: suitable for upstream technical discussion as an experimental proposal, not for merge or activation. Before proposing production consensus activation, Hive maintainers should review operation-tag strategy, persisted object/snapshot migration, RC weights and payer economics, grant growth policy, and recipient attribution. Independent security audit, broader production-grade replay/snapshot coverage, an agreed future hardfork plan, and full-chain support for any desired `HIVE_PROTOCOL_SKIP_VOPS` configuration remain prerequisites.

## Historical lifecycle implementation (pre-consolidation)

This section records the follow-up change after the earlier code review; the current statistics and post-consolidation test results are summarized at the end of this document.

The original lifetime record cap let revoked and expired grants permanently consume slots. The lifecycle implementation replaced that with bounded pruning and a permanent Chainbase high-water ID record for each grantor. New IDs must be strictly greater, may skip values, never wrap, and cannot be reused after pruning. The maximum ID can be used once; afterward no further grant ID is representable. That predecessor implementation lazily migrated old grant rows; the consolidated implementation removes that migration path because no grant state exists before its single activation.

On a later grant creation, the evaluator scans only that grantor's bounded retained rows and removes rows already revoked or with `expires_at <= head_block_time`. It never removes an active grant. The 100-record cap bounds cleanup work and retained live grants. Index deletion, metadata creation/update, grant creation, balance/counter changes, and failures all use Chainbase transaction undo. Pruned rows disappear from the inspection API; the API reports `last_issued_grant_id`, while transaction and account-history records remain.

The first RC revision charged every creation for the maximum possible 100-row scan. Focused testing showed that this made even an empty grantor's first creation exceed the default fixture account's RC. The final accounting instead charges one execution weight per existing retained row, adds earlier create operations in the same transaction to that count, and charges the 64-byte metadata estimate once per grantor when first created. Pruning may reduce later actual scans within a transaction, so later creation costs can conservatively overestimate work; they do not underprice it. The focused test suite and full chain suite pass with these prices. The predecessor confirmed-block CSV is `compressed_batch_rc_benchmark_preconsolidation.csv`; transfer rows match the prior benchmark for its fresh grantors.

Final lifecycle verification for this change: `ninja -C build-baseline -j2 chain_test` succeeded; the focused suite passed 29 cases and 745 assertions; the full suite passed 526 cases and 1,835,530 assertions with one opt-in case skipped; the separate compressed-batch RC benchmark passed 904 assertions. Tests cover creation of 100 grants, rejection at 101, replacement after revocation, pruning after expiry, failed-transaction and block-pop rollback, rejected ID reuse after pruning, and maximum-ID exhaustion. Both HIVE and HBD appear among the live grants. These are C++ fixture results; they do not constitute fresh populated snapshot/replay or signed-RPC tests for metadata state.

Before consolidation, the alternate-chain `chain_test` binary was rebuilt and run: 526 of 527 cases passed, one opt-in case was skipped, and all 1,835,530 assertions passed; the focused scoped suite passed 29 cases and 745 assertions. The full plugin suite passed 102/102 cases and 7,134/7,134 assertions. The configuration filter passed 3/3 cases and 51/51 assertions. The standalone RC benchmark passed 904/904 assertions and its 24 deterministic rows matched the checked-in pre-consolidation CSV for serialized sizes, signatures, resource vectors, RC charges, grant setup, and balance writes. Pre-consolidation snapshot/replay and isolated production-configured rejection were subsequently exercised as recorded below. Full-node `HIVE_PROTOCOL_SKIP_VOPS` remains unsupported in both this branch and upstream develop.


## Historical verification addendum (pre-consolidation)

The predecessor branch activated lifecycle behavior in a separate second slot and lazily initialized metadata. This design was removed in consolidation. The current branch starts with one permanent grantor metadata row at the first grant and requires increasing IDs. Grant ID zero is not available for a newly issued grant under the unified activation; `UINT32_MAX` is valid once and permanently exhausts that grantor's ID space. On grant creation only, up to 100 retained rows are inspected; revoked or already-expired records are removed, and no live record is pruned. The resulting metadata survives removal of the grantor's last grant. Failed transaction and block pop restore both the removed rows and high-water metadata through normal Chainbase undo.

`resource_count.cpp` accounts execution cost for the bounded rows scanned and earlier grant creations in the same transaction; it adds an estimated 64-byte persistent-state cost when creating metadata for the first time. This is conservative when earlier creates in a transaction prune rows. It is still a protocol pricing estimate, not a measured CPU-to-RC conversion.

Pre-consolidation verification covered the consensus, plugin, configuration, and RC suites. Fresh populated snapshot/replay, signed-RPC, two-node, and production-configured rejection results are now recorded below. The skip-VOPS protocol library compiles, but full-chain compilation fails in an existing VOP-producing chain path in both this branch and untouched upstream develop; no skip-VOPS node binary or alternate-variant serialization round trip is claimed.

## Historical persistence and isolation evidence (pre-consolidation)

The chain described in this historical section used the predecessor two-slot activation schedule and is retained only as prior evidence. Signed RPC transactions created grants for multiple grantors and actors, spent from a grant, revoked grants, allowed a short grant to expire, and created replacement grants after pruning. Alice's IDs advanced through 4 before the snapshot; grant 3 had expired and been pruned, while grant 1 retained a 1.000 TBD lifetime counter. The snapshot was restored into an independent data directory. A clean node replayed the same block log from genesis. At block 140, both restored and replayed nodes had block ID `0000008c2b79323b72b19a1527f66dcd5e89c7b4`; compared projections of head data, balances, grant records/counters/status, and high-water IDs matched exactly. A separate full replay reached block 145, ID `000000917b9f0e64c12ea96d0990565ed1f86cf8`.

For independent-node/restart verification, a follower synchronized through block 145, was stopped, and the primary included Alice's revoke (`61bf37cdacaa9a7e602ad6cd29da31001538a632`, block 149) and replacement grant (`524906e22f00630f2e23368f822e7e560eba5ad7`, block 150). After restart and synchronization, both nodes matched at block 153, ID `000000994bfe0725d45ed8fa4dc996fba64b001e`. Alice's high-water ID was 5 and the retained grant IDs were 2, 4, and 5; the pruned/revoked grant could not be queried or reused. The compared consensus projections matched. This was a local development chain and an API/state projection comparison, not a formal independent implementation audit.

The production-configured `build-mainnet` node ran in an isolated directory with only local endpoints and no public peers or production keys. At mainnet HF0, evaluator-level runtime checks rejected create, revoke, and scoped transfer with `Scoped authorities are disabled in mainnet builds`; compressed batch transfer was rejected with `Compressed batch transfers are disabled in mainnet builds`. The probes used a local `temp` account with threshold-zero authority so the transactions reached the evaluator without signatures. This verifies build-time production isolation, not acceptance of every historical operation through a mainnet chain.

For `HIVE_PROTOCOL_SKIP_VOPS`, `hive_protocol` builds successfully. Building the existing `database.cpp` object in both this checkout and untouched upstream develop fails at the same code path (`changed_recovery_account_operation` is omitted from the variant but constructed in chain code). This establishes the failure as pre-existing for a full-chain skip-VOPS configuration. No complete node or alternate-variant binary transaction round trip is claimed. Existing-operation tags are covered by protocol tests; a byte-for-byte serialization comparison against an executable from upstream develop was not run in this validation.

The latest pre-consolidation full chain run completed with 526 of 527 cases passing, one opt-in benchmark case skipped, and 1,835,530/1,835,530 assertions. For the consolidated branch, the rebuilt full chain suite passed 526/527 cases (one opt-in RC benchmark skipped) and 1,835,539/1,835,539 assertions; focused scoped/batch tests passed 29/29 cases and 754/754 assertions; configuration tests passed 3/3 cases and 51/51 assertions; the RC benchmark passed 904/904 assertions. The consolidated full plugin run passed 98/102 cases and 7,090/7,134 assertions. Its four failures were `get_account_history_hf12`, `get_account_history_hf13`, `get_ops_in_block_hf12`, and `get_ops_in_block_hf13`; expected PoW transaction IDs differ from generated IDs. The focused HF12 rerun generated `c9a83dd7a36231d9375025ef49184bbd90822526` where the fixture expects `dcb5cfd6d6e908f76199cc061aa1a654bbac9bdf`. Baseline status is unverified, so these failures remain open.
