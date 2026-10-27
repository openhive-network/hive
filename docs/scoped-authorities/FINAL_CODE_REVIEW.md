# Code review notes

This file is a short map for reviewers. It describes the current branch, which adds four signed operations under one experimental HF30 activation. HF30 is only a local development identifier. It is not a proposed or assigned mainnet hardfork.

## What changed

Scoped grants live in Chainbase. A grantor can authorize an actor to transfer HIVE or HBD within per-transfer, fixed-period, lifetime, time, and recipient limits. The grantor can revoke it. A high-water ID record survives grant pruning, so IDs cannot be reused. At most 100 grant records are retained per grantor; revoked and expired records are pruned when that grantor creates another grant.

The batch operation moves one asset from up to 100 sources to up to 100 destinations. Inputs and outputs are sorted and unique. Each source either uses its own active authority or names a scoped grant. The operation checks conservation and recipient attribution before changing balances or grant counters.

The operations are:

- `create_scoped_authority_operation`
- `revoke_scoped_authority_operation`
- `scoped_transfer_operation`
- `compressed_batch_transfer_operation`

They are appended after the existing virtual-operation range. Existing operation tags remain fixed; the new signed operations are classified as regular operations. The production-configured evaluator rejects them. The full-chain `HIVE_PROTOCOL_SKIP_VOPS` build is not supported by current upstream call sites and remains unverified here.

## Where to read the code

- Operation fields, validation, authorities, and tags: `libraries/protocol/include/hive/protocol/hive_operations.hpp`, `libraries/protocol/hive_operations.cpp`, and `libraries/protocol/include/hive/protocol/operations.hpp`.
- Grant and grantor metadata objects and indexes: `libraries/chain/include/hive/chain/detail/state/scoped_authority_object.hpp`, `scoped_authority_object_multiindex.hpp`, and `scoped_authority_grantor_metadata_object.hpp`.
- Grant creation, revocation, scoped transfer, and batch evaluation: `libraries/chain/hive_evaluator_transfer.cpp`.
- HF30 setup: `libraries/chain/database_hardfork.cpp` and `libraries/protocol/hardfork.d/1_30.hf`.
- RC resource counts and payer selection: `libraries/chain/rc/resource_count.cpp` and `resource_user.cpp`.
- Grant inspection API: `libraries/plugins/apis/database_api/database_api_financial.cpp`.
- Consensus tests: `tests/unit/tests/scoped_authority_tests.cpp`.

## Security points to review

Grant creation and revocation use the grantor's normal active authority. A scoped transfer uses the actor's normal active authority. No grant adds the actor to the grantor's account authority. The grant applies only to the dedicated transfer path and scoped batch inputs.

Batch attribution walks sorted inputs and sorted outputs in order. Each input amount is allocated across outputs before any transfer is applied. For restricted inputs, every output receiving any portion must satisfy that grant's allowlist. All source balances, authorities, grants, limits, and recipients are checked before settlement. Normal transaction undo handles failed operations and block rollback.

The grant limit and batch bounds cap per-operation work. The 100-record cap also bounds pruning scans. High-water metadata is retained permanently for each grantor that has issued a grant.

## Verification on this branch

Recorded test results are in `SCOPED_AUTHORITIES_TESTING.md`. They include the C++ chain, focused feature tests, plugin/configuration tests, the RC fixture, a signed local RPC run, two-node synchronization, populated snapshot restore, genesis replay, and runtime rejection in an isolated production-configured node. The full regression suite was not rerun during the latest documentation-only change.

On the populated chain, source, snapshot-restored, and replayed API projections matched byte for byte at block 170, ID `000000aae264e0ff0248dd3cab78bb2a01e0382d`. The two development nodes matched at block 159, ID `0000009fae5d2791a746293a50983d9c614c343e`.

The confirmed-block RC benchmark compares separate transfers, standard transfers in one transaction, active batches, and scoped batches. It charges each input and output and includes grant-state work. Results are in `COMPRESSED_BATCH_RC_BENCHMARK.md` and the CSV beside it. They are fixture measurements, not a production pricing recommendation.

## Review status

This is a testnet prototype for technical review. It still needs independent security review, RC economics review, review of operation-tag behavior across supported build variants, and an activation plan assigned by Hive maintainers. Do not enable it on mainnet based on this branch.
