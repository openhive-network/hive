# Scoped Authorities Security Review

**Status: design threat model only. No implementation has been audited or tested.**

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

Grant IDs must not be recycled; otherwise a stale signed transfer could unexpectedly target a replacement policy. A monotonically allocated ID or unique non-reusable identifier is required. If grantor chooses IDs, an ever-created-ID index/tombstone is necessary to prevent reuse.

RC for grant creation and transfer should follow existing transaction submitter accounting. The scoped actor pays for its transaction; grantor pays only for its own create/revoke transactions. This must be confirmed against post-HF29 consensus RC paths in the implementation.

## Limitations and audit needs

This design does not prevent the owner from spending funds normally, does not reserve balances, and cannot guarantee a transfer will succeed if the grantor balance is insufficient. Fixed periods can permit spending up to the configured amount on both sides of a period boundary. It excludes savings, delayed/recurrent transfer, escrow, and other financial paths.

Before any public hardfork proposal, independently audit integer bounds, authority semantics across mixed signatures, binary serialization, resource accounting, Chainbase rollback and snapshot behavior, API exposure, fork/replay determinism, and all supported node upgrade paths. This document is not an external security audit.

