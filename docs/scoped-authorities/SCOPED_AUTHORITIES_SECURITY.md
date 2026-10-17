# Scoped Authorities Security Review

**Status: threat model for a development-only prototype. Focused and full C++ tests pass, but this source has not received an independent security audit.**

## Threat model and boundaries

The actor key may be fully compromised. The intended bound is the union of grants that account owners deliberately create for that actor. The owner retains unrestricted control of their own funds through existing authority paths. The grant constrains only scoped transfers; it does not freeze funds or intercept legacy transfers.

Creation/revocation use grantor active authority. Scoped transfer uses actor active authority. A grant must never be inserted into the account authority graph, satisfy an active/owner/posting requirement, or be treated as a substitute signature. No update operation is allowed in v1.

## Required attack review before activation

| Attack | Required defense |
|---|---|
| Per-transfer overrun / split transfers | Checked amount for each operation plus persistent aggregate counters |
| Several operations in a transaction/block | Evaluators mutate the same grant object in deterministic operation order; undo session covers all writes |
| Several grants or actors | Counters keyed by grant object; no actor-level shared counter or unintended aggregation |
| Forged ID / another grantor | Lookup by grantor and ID; actor and grantor fields compared against stored record |
| Revocation / expiry reuse | Tombstone remains revoked; half-open time interval evaluated on every use |
| Overflow, negative/zero amount, precision mismatch | Positive canonical asset validation and checked integer additions |
| Boundary timestamp | `valid_from <= block_time < expires_at`; fixed period index anchored at valid_from |
| Alternate operation path | Dedicated op only; no implicit grant support in transfer, savings, recurrent, vesting, escrow, market, or conversion evaluators |
| Authority recursion / signature substitution | Actor is an ordinary required active authority; no special verifier path or implicit account authority |
| Allowlist abuse / resource exhaustion | Validate names, canonical unique ordering, maximum count, per-grantor cap, RC charging |
| Replay, reorg, snapshot | Chainbase object/index registration, undo coverage, replay equality, snapshot schema checks |
| Serialization compatibility | Preserve every old regular and virtual operation tag; binary compatibility fixtures |
| Legacy authorization regression | Existing authority and transfer test suites unchanged and passing |

## Compressed batch transfer invariants

- Every direct input requires its own source active authority. Every scoped input uses the common actor's ordinary active authority plus the referenced grant; one actor signature cannot authorize a direct debit from an unrelated account.
- Input/output vectors are strictly sorted, unique, positive, bounded to 100 entries each, and balance exactly in one canonical asset. Source/destination overlap is rejected with a sorted linear merge check.
- Sorted source amounts are attributed in order to sorted destinations. Every destination portion attributed to a restricted input must satisfy that grant's allowlist. This rule prevents pooled inputs from laundering a restricted source into an unapproved recipient.
- All source balances, grant status, per-transfer/period/lifetime limits, and allowlists are validated before settlement. Balance and counter writes use the transaction undo session; block pop and transaction failure restore the state.
- A source appears at most once in an operation. A grant's one input amount is one per-transfer spend even when it funds several outputs, preventing output splitting from bypassing `max_per_transfer`.

The byte encoding reuses Hive's FC binary serializer with one shared symbol and memo; account names remain explicit. It does not use a custom decoder, Chainbase object IDs, or noncanonical references. The 100-output signed-size comparison showed 53.35% fewer bytes than 100 standard transfer operations; the broader confirmed-block byte and RC results are in [COMPRESSED_BATCH_RC_BENCHMARK.md](COMPRESSED_BATCH_RC_BENCHMARK.md). RC execution weight charges every input and output; scoped inputs also carry an additional transfer weight for grant state access, and signature costs remain the existing protocol costs.

Grant IDs cannot be recycled while the grant record is retained: revoke marks the record and does not erase it. The implementation currently retains expired and revoked records, so each grantor can create at most 100 grants over its lifetime. Deterministic pruning and safe ID reuse are not implemented; introducing either requires tombstones or a non-reusable allocation scheme.

RC transaction cost is charged to the transaction submitter through the existing Hive accounting. The confirmed-block benchmark compares resource vectors and RC costs against separate transfers and a multi-transfer transaction. Grant-creation cost is reported separately and is large enough that a one-use scoped grant is not RC efficient end to end. The measurements are fixture-specific and are not a production RC guarantee.

## Limitations and audit needs

This design does not prevent the owner from spending funds normally, does not reserve balances, and cannot guarantee a transfer will succeed if the grantor balance is insufficient. Fixed periods can permit spending up to the configured amount on both sides of a period boundary. It excludes savings, delayed/recurrent transfer, escrow, and other financial paths. Batch-specific source-to-recipient attribution adds protocol complexity and needs independent review, especially for mixed direct/scoped inputs and multiple grant use in one transaction.

Before any public hardfork proposal, independently audit integer bounds, authority semantics across mixed signatures, binary serialization, resource accounting, Chainbase rollback and snapshot behavior, API exposure, fork/replay determinism, and all supported node upgrade paths. This document is not an external security audit.

