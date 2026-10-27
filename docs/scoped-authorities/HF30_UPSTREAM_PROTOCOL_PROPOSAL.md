# Proposal for technical review: scoped authorities and batch transfers

**Status:** experimental prototype. HF30 is a local alternate-chain placeholder, not a Hive mainnet hardfork assignment. This branch is not ready for activation.

## Problem

Hive account authorities do not express a permission that lets another account transfer a fixed amount of HIVE or HBD to selected recipients for a limited time. Applications either rely on an unrestricted active key or enforce limits outside consensus.

Repeated transfers also repeat operation, asset, memo, and transaction overhead. This prototype adds a single-asset operation that combines account debits and credits while checking every source's authorization.

## Protocol changes

The proposal adds four signed operations:

- `create_scoped_authority_operation` creates an immutable grant.
- `revoke_scoped_authority_operation` disables a grant.
- `scoped_transfer_operation` spends from one grant.
- `compressed_batch_transfer_operation` performs a bounded multi-input, multi-output transfer.

A grant names its owner and actor, ID, HIVE or HBD asset, per-transfer limit, fixed-period limit, lifetime limit, validity interval, and optional recipient list. Creation and revocation require the grantor's active authority. A restricted transfer requires the actor's active authority. It does not add the actor to the grantor's authorities.

Grant IDs increase monotonically. Per-grantor high-water metadata persists after old records are pruned. A grantor can retain up to 100 grant records. Later grant creation prunes revoked or expired rows; no active grant is deleted to make room. The high-water value prevents reuse after pruning.

Batch inputs and outputs are sorted unique account-name entries with positive integer amounts. The operation has one shared asset and memo. Ordinary inputs need each source's active authority; scoped inputs name a grant and use the actor's authority. Sources are attributed to sorted destinations in order. Each restricted grant must allow every destination receiving any part of its attributed input. All limits, signatures, balances, and conservation are checked before state changes.

The wire format uses Hive's existing FC serializer. It does not use a custom varint or compression codec. Savings come from one transaction and operation envelope, a shared symbol and memo, compact vectors, and, for scoped inputs, actor authorization instead of separate source signatures.

## RC and benchmark

The evaluator charges work for every input and output and additional work for each scoped grant lookup and counter update. A scoped batch charges its actor. An active-only batch charges its first sorted source. The new operations use the existing RC statistics bucket to preserve the persistent RC statistics object layout.

The confirmed-block fixture compared separate transfers, standard transfers in one transaction, active batches, and scoped batches. For 100 inputs and 100 outputs, the measured sizes were 13,600, 12,313, 10,926, and 4,501 bytes. The corresponding normalized RC costs were 1,851,013,800; 1,668,494,261; 1,490,607,397; and 599,733,022. These results include batching, encoding, and signature-count differences. They are fixture measurements, not a production cost guarantee. Scoped grant setup is priced separately and can outweigh savings for one-time use. Method and raw values are in [the benchmark report](COMPRESSED_BATCH_RC_BENCHMARK.md).

## Compatibility and activation

The four operation tags follow the historical virtual-operation range. Historical tags remain fixed, and the new operations are regular signed operations. Tests pin tags and compare a legacy transfer's bytes against untouched upstream. The operation-tag arrangement still needs review in every supported build configuration. A full-chain `HIVE_PROTOCOL_SKIP_VOPS` build is not supported by existing virtual-operation construction sites.

All rules are behind one alternate-chain HF30 gate. Production-configured evaluators reject all four operations. HF30 is not the mainnet deployment number. A production proposal needs a maintainer-assigned hardfork and an agreed replay, snapshot, and migration policy. The new Chainbase objects require modified nodes for any chain that contains grants.

## Evidence and open review

The consolidated chain has passed recorded C++ consensus, plugin, configuration, focused feature, and RC benchmark tests. Signed local RPC tests confirmed grant creation, transfers, batches, revocation, expiration, pruning, and replacement IDs. A populated snapshot and a genesis replay matched the source state at block 170. Two local nodes converged after restart and synchronization. A production-configured node rejected all four operations in an isolated network namespace. Exact results and limits are in [the test notes](SCOPED_AUTHORITIES_TESTING.md).

Before any activation, reviewers should examine operation tags, authority handling, source-to-recipient attribution, grant lifecycle and bounds, RC weights, state growth, rollback, snapshot restoration, and replay. An independent security audit and RC economics review are required. The prototype is not production-ready.
