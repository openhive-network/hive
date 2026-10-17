# Protocol Improvement Proposal: Scoped Authorities and Compressed Batch Transfers

**Status:** experimental proposal for technical review. This document does not assign HF30 as a Hive mainnet hardfork and does not propose activating the implementation without upstream review, an independent security audit, and normal witness/governance approval.

## Summary

This proposal describes two consensus extensions developed together in an isolated Hive repository branch: immutable, grantor-authorized spending grants, and an atomic single-asset account batch transfer. The prototype appends four signed operations without changing historical operation tags, stores grants in Chainbase, and gates evaluation to the alternate-chain development configuration. It is not production-ready.

## Problem

Existing owner, active, and posting authorities do not express an independently scoped transfer permission with asset, recipient, amount, duration, and revocation constraints. Applications needing that permission otherwise hold unrestricted active authority or place enforcement outside consensus. Separately, repeated transfers duplicate transaction and operation overhead, and separate transactions repeat signature and TAPOS overhead.

## Scoped Authorities specification

The four prototype operations are `create_scoped_authority_operation`, `revoke_scoped_authority_operation`, `scoped_transfer_operation`, and `compressed_batch_transfer_operation`. A grant contains grantor, actor, grant ID, one asset's per-transfer, fixed-period, and lifetime ceilings, validity timestamps, an optional canonical recipient allowlist, spent counters, period index, and revoked state. Grant creation and revocation require the grantor's active authority. A scoped transfer requires the actor's ordinary active authority; it does not make the actor an authority of the grantor. Grants are immutable. Under alternate-chain-only HF31, IDs must increase, a per-grantor high-water record prevents reuse after pruning, and revoked/expired rows are lazily pruned on later grant creation under a 100-row bound. HF30 retains its prior validation rules for replay. The supported assets are liquid HIVE and HBD only. Savings, recurrent transfers, vesting, conversions, market operations, and other operation types remain outside the grant.

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

The experimental source appends the four operation tags after historical virtual-operation tags in the full variant; tests pin historical and new tags and check the new operations are signed/non-virtual. `HIVE_PROTOCOL_SKIP_VOPS` produces the alternate regular-operation tag layout while preserving historical regular tags. This scheme is branch-specific and must be reviewed against all supported build variants before upstream adoption. Mainnet-configured evaluator code rejects application of these operations unconditionally. HF30 enables the operations and HF31 changes grant lifecycle rules only in alternate-chain builds; neither slot is an official mainnet assignment. A future implementation must receive an upstream-assigned hardfork identifier and activation mechanism.

The Chainbase grant and grantor-metadata indexes change consensus state layout. HF31 derives high-water metadata lazily from preexisting retained grants before committing cleanup. Existing state snapshots and nodes still need explicit compatibility validation; a clean replay may be required unless a reviewed migration is implemented. RC statistics object sizing remains unchanged by reusing the multi-operation bucket. Populated multi-grant snapshot and replay compatibility must be demonstrated before upstream acceptance.

## Evidence and known limitations

The branch has passed consensus, plugin, configuration, and explicit opt-in RC benchmark runs. The pinned Python 3.14 environment is installed in the project-local Poetry virtual environment; the generic node exit/stop smoke tests passed 2/2 when `HIVE_BUILD_ROOT_PATH` pointed at the modified build. These tests do not exercise experimental operations. Multi-grant snapshot/replay, runtime mainnet rejection, full-chain skip-VOPS compilation (the macro removes VOPs required by existing chain call sites), alternate-variant serialization round-trip, and fresh signed-RPC/two-node lifecycle verification remain gaps; current evidence and exact latest test totals are in [testing notes](SCOPED_AUTHORITIES_TESTING.md).

## Suggested review path

1. Request protocol and database review from Hive maintainers, including operation-tag behavior with and without virtual operations.
2. Obtain an independent audit focused on authorization, recipient attribution, RC pricing, and state rollback.
3. Add reproducible populated multi-grant snapshot, replay, and mainnet-runtime rejection tests.
4. Run a public alternate-chain testnet with independent witnesses/nodes and publish raw benchmark inputs and outputs.
5. Resolve database migration/replay policy and RC reporting semantics.
6. Only then prepare a separately numbered hardfork proposal through the normal Hive process. Do not deploy based on the experimental HF30 branch identifier.
