# Final code review: experimental HF30 features

## Scope and review result

This review compares `feature/scoped-authorities` with `upstream/develop` at the seven-commit implementation boundary. It covers the Scoped Authorities and Compressed Batch Transfers changes, plus the limited cleanup made during this review. No changes have been pushed. HF30 remains an alternate-chain development feature; this review is not a production approval.

The review found two needless evaluator steps and three extra trailing blank lines in feature documentation. The evaluator cleanup removed a post-mutation database lookup/assert that could not establish anything beyond the successful return from `database::modify`, and removed a reference kept alive only with `(void)`. Account lookup and all validation remain. No consensus semantics or public serialization fields changed. No broader refactor was justified.

## Exact change statistics

Reference: `upstream/develop` (`9d600761564865ac9b4accb3ec59f204c5ee4797`). Implementation HEAD before this review: `222ee085f05b555970aae7e29058cd4603cb590a`.

| Measure | Before cleanup | After cleanup and this report |
|---|---:|---:|
| Commits ahead of upstream | 7 | 8 after review commit |
| Changed files | 40 | 41 |
| New files | 15 | 16 |
| Deleted files | 0 | 0 |
| Added lines | 2,346 | 2,556 |
| Deleted lines | 30 | 30 |
| Net lines | 2,316 | 2,526 |
| Production C++ source additions | 528 | 532 |
| Header additions | 252 | 252 |
| Test additions | 1,037 | 1,037 |
| Markdown documentation additions | 486 | 692 (includes this 209-line review) |
| Benchmark CSV additions | 25 | 25 |
| Documentation/data total | 511 | 717 |
| New regular protocol operations | 4 | 4 |
| New persistent consensus object types | 1 | 1 |
| New scoped-authority test cases | 24 declared | 24 declared (one opt-in RC benchmark is disabled in ordinary suite runs) |

Before-cleanup counts are from `git diff --numstat upstream/develop...HEAD`, not estimates. After cleanup, excluding this new review, the implementation diff is +2,347/−30 lines (net +2,317); the full final diff including this 207-line report is +2,554/−30 (net +2,524). The per-file inventory below is generated from the final diff. Build trees are ignored and are not counted. The benchmark CSV is tracked documentation data, not a generated build artifact.

### Per-file inventory

`+` and `−` are exact textual line counts versus upstream; the purpose and security rating classify the file’s role in this change.

| File path | Added lines | Deleted lines | Purpose | Security importance |
|---|---:|---:|---|---|
| `docs/scoped-authorities/COMPRESSED_BATCH_RC_BENCHMARK.md` | 75 | 0 | RC benchmark method, resource accounting, results and reproduction command | Low |
| `docs/scoped-authorities/COMPRESSED_BATCH_TRANSFER.md` | 34 | 0 | Batch wire behavior, limits, validation and API examples | Low |
| `docs/scoped-authorities/FINAL_CODE_REVIEW.md` | 209 | 0 | Complete inventory, walkthrough, audit findings and evidence | Low |
| `docs/scoped-authorities/HF30_UPSTREAM_PROTOCOL_PROPOSAL.md` | 56 | 0 | Experimental protocol proposal and upstream review requirements | Low |
| `docs/scoped-authorities/SCOPED_AUTHORITIES_API.md` | 71 | 0 | Grant create, inspect, transfer and revoke request examples | Low |
| `docs/scoped-authorities/SCOPED_AUTHORITIES_CHANGELOG.md` | 39 | 0 | Feature file inventory and implementation history | Low |
| `docs/scoped-authorities/SCOPED_AUTHORITIES_HARDFORK.md` | 24 | 0 | Alternate-chain HF30 activation and deployment compatibility notes | Low |
| `docs/scoped-authorities/SCOPED_AUTHORITIES_SECURITY.md` | 47 | 0 | Threat model, privilege boundaries and known security limitations | Low |
| `docs/scoped-authorities/SCOPED_AUTHORITIES_SPEC.md` | 61 | 0 | Scoped grant schema, operations, semantics and protocol constraints | Low |
| `docs/scoped-authorities/SCOPED_AUTHORITIES_TESTING.md` | 76 | 0 | Build, test, local node and known validation evidence | Low |
| `docs/scoped-authorities/compressed_batch_rc_benchmark.csv` | 25 | 0 | Raw per-shape/per-mode RC and serialized-size measurements | Low |
| `libraries/chain/CMakeLists.txt` | 1 | 0 | Compile and register new consensus state index | High |
| `libraries/chain/database_hardfork.cpp` | 8 | 0 | Development-only HF30 configuration and activation | Critical |
| `libraries/chain/database_scoped_authority.cpp` | 20 | 0 | Register and instantiate the Chainbase grant index | Critical |
| `libraries/chain/hive_evaluator_transfer.cpp` | 235 | 0 | Consensus grant/revoke/transfer/batch validation and state changes | Critical |
| `libraries/chain/include/hive/chain/detail/state/scoped_authority_object.hpp` | 59 | 0 | Persistent grant record and deterministic indexes | Critical |
| `libraries/chain/include/hive/chain/detail/state/scoped_authority_object_multiindex.hpp` | 32 | 0 | Persistent grant record and deterministic indexes | Critical |
| `libraries/chain/include/hive/chain/hive_object_types.hpp` | 5 | 0 | Protocol integration | Medium |
| `libraries/chain/include/hive/chain/rc/rc_stats.hpp` | 10 | 1 | Keep operation statistics within the stable RC stats layout | High |
| `libraries/chain/index.cpp` | 2 | 0 | Compile and register new consensus state index | High |
| `libraries/chain/rc/rc_objects.cpp` | 11 | 2 | Keep operation statistics within the stable RC stats layout | High |
| `libraries/chain/rc/resource_count.cpp` | 34 | 0 | Per-operation RC resource accounting | High |
| `libraries/chain/rc/resource_user.cpp` | 12 | 0 | Select RC payer for active/scoped batches | High |
| `libraries/plugins/apis/account_history_api/account_history_api.cpp` | 2 | 0 | Index and filter new operation types in account history | Medium |
| `libraries/plugins/apis/account_history_api/include/hive/plugins/account_history_api/account_history_api.hpp` | 4 | 0 | Index and filter new operation types in account history | Medium |
| `libraries/plugins/apis/database_api/database_api.cpp` | 1 | 0 | Register and serialize grant inspection API structures | Medium |
| `libraries/plugins/apis/database_api/database_api_financial.cpp` | 44 | 0 | Read-only grant inspection API implementation | Medium |
| `libraries/plugins/apis/database_api/include/hive/plugins/database_api/database_api.hpp` | 1 | 0 | Register and serialize grant inspection API structures | Medium |
| `libraries/plugins/apis/database_api/include/hive/plugins/database_api/database_api_args.hpp` | 12 | 0 | Register and serialize grant inspection API structures | Medium |
| `libraries/plugins/apis/database_api/include/hive/plugins/database_api/database_api_impl.hpp` | 1 | 0 | Register and serialize grant inspection API structures | Medium |
| `libraries/plugins/apis/database_api/include/hive/plugins/database_api/database_api_objects.hpp` | 25 | 0 | Register and serialize grant inspection API structures | Medium |
| `libraries/protocol/forward_impacted.cpp` | 49 | 0 | Affected-account and impacted-balance visitor integration | High |
| `libraries/protocol/hardfork.d/0-preamble.hf` | 1 | 1 | Development-only HF30 configuration and activation | Critical |
| `libraries/protocol/hardfork.d/1_30.hf` | 16 | 0 | Development-only HF30 configuration and activation | Critical |
| `libraries/protocol/hive_operations.cpp` | 114 | 0 | Protocol field validation, ordering and conservation checks | Critical |
| `libraries/protocol/include/hive/protocol/config.hpp` | 11 | 2 | Development-only HF30 configuration and activation | Critical |
| `libraries/protocol/include/hive/protocol/hive_operations.hpp` | 85 | 0 | Protocol operation schema, authority declaration, reflection and stable tags | Critical |
| `libraries/protocol/include/hive/protocol/operations.hpp` | 7 | 0 | Protocol operation schema, authority declaration, reflection and stable tags | Critical |
| `tests/unit/plugin_tests/condenser_api_account_history_tests.cpp` | 12 | 12 | Update plugin golden output for operation encoding/fixture output | Medium |
| `tests/unit/plugin_tests/condenser_api_ops_in_block_tests.cpp` | 12 | 12 | Update plugin golden output for operation encoding/fixture output | Medium |
| `tests/unit/tests/scoped_authority_tests.cpp` | 1013 | 0 | Consensus tests for grants, limits, batches, rollback, tags and RC | High |

The largest additions are `tests/unit/tests/scoped_authority_tests.cpp` (1,013 lines), `libraries/chain/hive_evaluator_transfer.cpp` (239 lines), `libraries/protocol/hive_operations.cpp` (114 lines), and `libraries/protocol/include/hive/protocol/hive_operations.hpp` (85 lines). The test file is large because it contains consensus fixtures and adversarial cases; it merits continued decomposition only if upstream reviewers find that it impedes navigation.

## Plain-English walkthrough

A Scoped Authority is a stored spending contract. Alice signs a grant naming Bob (or a service account such as `hivedebit`), the asset, limits, validity interval, and optional recipients. Bob does not receive Alice’s active key or authority. He signs a restricted transfer with his own active authority. Consensus resolves the grant and checks the proposed transfer before changing Alice’s balance or the grant counters.

A compressed batch is one transaction that names several source accounts and several destinations for one asset. Each ordinary source must authorize its own debit. A scoped source instead names a grant, and the delegated actor authorizes that input. The validator conserves the exact integer amount and deterministically attributes each source to destinations in sorted order, applying that grant’s recipient allowlist to the attributed portions. All checks happen before balance or counter writes.

## Scoped Authorities: implementation flow

### Create

`create_scoped_authority_operation` is defined and reflected in `libraries/protocol/include/hive/protocol/hive_operations.hpp`; its limits, symbols, timestamps, duration, and canonical sorted recipient list are checked in `libraries/protocol/hive_operations.cpp::create_scoped_authority_operation::validate`.

Its required active authority is the grantor. `create_scoped_authority_evaluator::do_apply` in `libraries/chain/hive_evaluator_transfer.cpp` confirms that both accounts exist, enforces the grantor’s 100-record cap and never-reused grant ID, checks future expiration, and creates the Chainbase object. A grant can be created only after the development HF gate on alternate-chain builds; production-configured evaluator builds reject it.

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
- **State/rollback:** grant IDs are unique for the grantor’s retained record lifetime; records are not pruned; total record count per grantor is bounded. Mutations use normal Chainbase create/modify and transaction undo. No scheduler or local clock is used.
- **Arithmetic/periods:** protocol bounds and remaining-limit subtraction precede additions; period rollover is lazy and timestamp-derived. The state invariant checks protect against malformed persisted counters. Fixed-window anchoring makes the first period begin at `valid_from`.
- **Batch attribution:** the ordered allocation is explicit and tested for restricted/unrestricted mixtures, separate grants, split recipients, and ordering. Overlap is rejected. The design is auditable but should receive focused independent review because recipient authorization depends on this rule.
- **RC:** usage is charged per entry, but execution weights are estimates and require protocol economics review. RC operation-stat aggregation uses the historic multiop slot to avoid changing persistent `rc_stats_object` array size; per-operation values are not visible as their own bucket. Validate upstream reporting expectations.
- **API/history:** read-only grant API enumerates at most the per-grantor cap; the history bit filters add high-order enum bits, which consumers must preserve as 64-bit values. Existing plugin golden fixtures change transaction IDs and PoW fields because the alternate-chain test configuration advances its chain version to 1.30; this reflects development-chain block generation, not a reordering of historical transaction operation tags. The refreshed plugin suite will validate the checked-in fixture values.
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
- Snapshot/replay coverage for this branch includes the previously recorded single-populated-grant run. A multi-grant populated snapshot/replay was not performed here.

## Diff summary and upstream readiness

The final exact diff against upstream is 41 files, 16 added and 25 modified, +2,556/−30 lines (net +2,526). The diff contains 784 production C++ source/header additions and five deletions, 1,037 test additions and 24 deletions, 692 Markdown documentation lines and 25 benchmark CSV lines, and 18 other build/hardfork/config additions with one deletion. Seven commits were present before cleanup; this review adds one local commit and does not push it.

Recommendation: suitable for upstream technical discussion as an experimental proposal, not for merge or activation. Before proposing production consensus activation, Hive maintainers should review operation-tag strategy, persisted object/snapshot migration, RC weights and payer economics, grant growth policy, and recipient attribution. Independent security audit, populated multi-grant snapshot/replay testing, mainnet-runtime rejection testing, separate `HIVE_PROTOCOL_SKIP_VOPS` build coverage, fresh signed-RPC/two-node verification, and an agreed future hardfork plan remain prerequisites.
