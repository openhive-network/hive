# Protocol Improvement Proposal: Scoped Authorities and Compressed Batch Transfers

**Status:** experimental proposal for technical review. The branch uses HF30 as one provisional alternate-chain activation for all described changes. This does not assign HF30 as a Hive mainnet hardfork and does not propose activation without upstream review, an independent security audit, and normal witness/governance approval.

## Summary

This proposal describes one consensus upgrade containing two related extensions: immutable, grantor-authorized spending grants and atomic single-asset account batch transfers. The unified prototype appends four signed operations without changing historical operation tags, stores grants and issuance metadata in Chainbase, and gates evaluation to the alternate-chain development configuration. Grant pruning and permanent grant-ID non-reuse are active from the first grant at the same activation. The implementation is not production-ready.

## Problem

Existing owner, active, and posting authorities do not express an independently scoped transfer permission with asset, recipient, amount, duration, and revocation constraints. Applications needing that permission otherwise hold unrestricted active authority or place enforcement outside consensus. Separately, repeated transfers duplicate transaction and operation overhead, and separate transactions repeat signature and TAPOS overhead.

## Scoped Authorities specification

The four operations are `create_scoped_authority_operation`, `revoke_scoped_authority_operation`, `scoped_transfer_operation`, and `compressed_batch_transfer_operation`. A grant contains grantor, actor, grant ID, one asset's per-transfer, fixed-period, and lifetime ceilings, validity timestamps, an optional canonical recipient allowlist, spent counters, period index, and revoked state. Grant creation and revocation require the grantor's active authority. A scoped transfer requires the actor's ordinary active authority; it does not make the actor an authority of the grantor. Grants are immutable. From the unified activation, IDs must increase, a per-grantor high-water record prevents reuse after pruning, and revoked/expired rows are lazily pruned on later grant creation under a 100-row bound. Metadata is created with the first grant; no intermediate migration exists. The supported assets are liquid HIVE and HBD only. Savings, recurrent transfers, vesting, conversions, market operations, and other operation types remain outside the grant.

Limits use integer asset units and consensus block time. A period is anchored to `valid_from`; when its index changes, only the period counter resets. Lifetime spending remains cumulative. A transfer at or after expiration fails, as does a transfer after revocation. The owner may still use ordinary account authority to move funds independently of a grant.

## Compressed Batch Transfers specification

`compressed_batch_transfer_operation` carries one symbol, a memo, sorted unique positive inputs, and sorted unique positive outputs. Each active-authority input names its source account and requires that account's ordinary active authority. A scoped input names a grant ID; the operation's actor must match every referenced grant and supplies its own active authority. Input and output sums must match exactly for the shared asset. The evaluator validates all sources, recipients, authorities, limits, and balances before applying debits, credits, and scoped counters within the transaction's undo session.

The prototype uses Hive's existing FC binary serializer. The wire payload shares the asset symbol and memo and stores explicit account names; it does not encode database-local account IDs or introduce a custom decompressor. Inputs and outputs are canonically ordered. For scoped inputs, outputs are allocated in canonical order across canonical inputs, and each source's allocated amounts are checked against its allowlist. This deterministic attribution rule is central to preserving recipient restrictions and requires particular upstream scrutiny.

## Authorization and security model

Authorities are derived from the operation's required active authorities and verified by the normal Hive transaction authority machinery. Active batch inputs require each source account's authorization; a scoped batch uses the actor's authority for grant-authorized inputs. A grant cannot create or expand another grant. Transactions are atomic under normal database undo behavior. State limits, input/output bounds, canonical ordering, positive amount checks, asset checks, and checked allowance arithmetic constrain resource and accounting abuse.

Key audit questions include composition of multiple grants for one source, mixed active/scoped inputs, overlap and self-transfer handling, source-to-recipient attribution, signature substitution, authority recursion, and rollback through block pop/replay. The current tests cover many of these cases, but do not replace a formal security review.

## RC economics and benchmark evidence

The prototype prices transaction bytes through existing Hive RC accounting and adds per-input/per-output execution weights; scoped inputs add work for grant lookup and counter mutation. Scoped batch transactions charge the actor; active-only batches select the first canonical source. New operations use the existing multi-operation statistics bucket to avoid changing persistent RC statistics object layout; resource vectors remain operation-specific, but reporting does not expose a separate average per experimental operation.

The confirmed-block fixture benchmark compared separate transfers, a transaction with standard transfers, active compressed batches, and scoped compressed batches. At 100 inputs to 100 outputs, the measured signed transaction sizes were 13,600 / 12,313 / 10,926 / 4,501 bytes respectively, and same-pool RC costs were 1,850,881,200 / 1,668,368,778 / 1,490,469,862 / 599,713,861. Scoped batches use one actor signature rather than 100 source signatures in this shape. These results combine batching, serialization, and signature-count effects; they do not claim that 100 balance changes cost the same as one transfer. Scoped grant creation has material RC cost (approximately 18.84 billion to 1.843 trillion in the measured shapes), so a one-use grant is not RC-efficient end to end. Raw data and reproduction instructions are in [the benchmark report](COMPRESSED_BATCH_RC_BENCHMARK.md) and its CSV.

Using the CSV's `grant_setup_rc` and one batch's same-pool RC savings, a rough amortization estimate is `ceil(grant_setup_rc / per-batch RC savings)`. The six shapes require 30–2,491 repeated batches to repay setup versus separate transfers, and 104–8,671 batches versus one standard multi-transfer transaction. These estimates are sensitive to grants per batch: the 100-input case carries setup cost for 100 grants. They are not a promise of real-network economics; grant expiry, period allowances, available RC, and changed pool conditions affect reuse.

## Compatibility and activation

The experimental source appends the four operation tags after historical virtual-operation tags in the full variant; tests pin historical and new tags and check the new operations are signed/non-virtual. `HIVE_PROTOCOL_SKIP_VOPS` produces the alternate regular-operation tag layout while preserving historical regular tags. This scheme is branch-specific and must be reviewed against all supported build variants before upstream adoption. Mainnet-configured evaluator code rejects application of these operations unconditionally. All features activate together only in alternate-chain builds at the provisional HF30 slot. A future implementation must receive an upstream-assigned hardfork identifier and activation mechanism.

The Chainbase grant and grantor-metadata indexes change consensus state layout. HF30 activation itself performs no migration; metadata is created directly with the first grant. Snapshots and block histories made for the earlier two-slot experiment are not compatible with the consolidated schedule. Fresh genesis replay and populated snapshot restoration must be tested for the unified protocol. RC statistics object sizing remains unchanged by reusing the multi-operation bucket.

## Evidence and known limitations

The consolidated HF30 chain has now been exercised with signed local RPC transactions, populated snapshot restore, genesis replay, independent-node synchronization, and isolated production-configured evaluator rejection. Exact results and test-count provenance are in [the testing notes](SCOPED_AUTHORITIES_TESTING.md). These checks are prototype evidence only. Full-chain `HIVE_PROTOCOL_SKIP_VOPS` support remains unavailable because existing code paths construct omitted virtual-operation types; this limitation is not hidden by changing the operation variant. Serialization of a legacy transfer was byte-compared against untouched upstream, but broader historical transaction fixtures and every operation variant still warrant upstream review. No evidence here establishes production readiness.

### Final local end-to-end results (2026-10-19)

A newly initialized alternate chain activated the unified experimental HF30 once at block 1. Signed grant creation, a scoped transfer, active and scoped multi-input batches, revocation, expiry, lazy pruning, and replacement grants were confirmed in blocks. Over-limit, unauthorized-recipient, and expired uses were rejected during pending application. The final state at block 170 (`000000aae264e0ff0248dd3cab78bb2a01e0382d`) had Alice/Bob/Carol high-water IDs 3/2/2 and retained Alice's partially spent grant, Bob's replacement grant, Carol's partially spent grant, and Carol's revoked grant.

A populated snapshot restored in an independent node and a separate replay from genesis both produced API projections byte-identical to the source, including account authorities, balances, retained grants, counters, status, and high-water IDs. A second independent node was stopped while its peer advanced through pruning/replacement creation, then restarted and synchronized to the same block ID and state at block 159. A production-configured node was tested inside an isolated network namespace with no external route; all four experimental operation types reached evaluator execution and returned their explicit mainnet-disabled errors. These checks do not replace a larger multi-witness testnet, replay from a long-lived chain, or independent audit.

## Suggested review path

1. Request protocol and database review from Hive maintainers, including operation-tag behavior with and without virtual operations.
2. Obtain an independent audit focused on authorization, recipient attribution, RC pricing, and state rollback.
3. Add reproducible populated multi-grant snapshot, replay, and mainnet-runtime rejection tests.
4. Run a public alternate-chain testnet with independent witnesses/nodes and publish raw benchmark inputs and outputs.
5. Resolve database migration/replay policy and RC reporting semantics.
6. Only then prepare a hardfork proposal through the normal Hive process, using the identifier and activation mechanism assigned by Hive maintainers. Do not deploy based on the experimental HF30 branch identifier.
