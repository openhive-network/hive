# Security notes

This is an experimental consensus change. These notes describe the implemented boundaries and the checks exercised so far. They are not an independent security audit.

## Authority boundary

Grant creation and revocation require the grantor's active authority. A scoped transfer requires the actor's active authority. Existing Hive authority recursion verifies these authorities. A grant does not change owner, active, posting, recovery, or witness permissions, and it cannot authorize a legacy transfer or another financial operation.

The grant is immutable. The actor cannot raise limits, extend expiry, change recipients, create grants, revoke grants, or update account authorities through the scoped path. The grantor can still spend its own balance with normal account authority.

## Spending state

Limits use integer asset units. The period counter is based on the consensus head-block timestamp and a window anchored to `valid_from`. Expiry is exclusive: a transfer at `expires_at` is invalid. Lifetime spending is retained across periods. Revocation prevents new uses and leaves prior counters intact.

Rejected operations must not change balances, counters, or grant records. Evaluators validate before settlement and use the regular Chainbase transaction undo session. Tests cover failed transaction rollback and block pop. The lifecycle high-water record persists after revoked or expired grant records are pruned, preventing ID reuse.

At most 100 grant records are retained per grantor. Grant creation scans only those records and prunes only grants already revoked or expired at head time. Recipient lists and batch vectors are bounded. Cleanup does not run per block.

## Batch attribution

Inputs and outputs are sorted, unique, and may not overlap. The evaluator allocates sorted inputs across sorted outputs in order. A scoped source is charged against each recipient receiving any portion of its allocation. This prevents a restricted source from being hidden inside a pooled transfer. The tests exercise restricted/unrestricted mixtures, split outputs, reordering, independent grants, and failed-batch rollback.

The attribution rule is subtle and should receive independent review. In particular, reviewers should check repeated use of one grant, mixed active and scoped inputs, all transaction composition cases, and malformed serialized data.

## Resource accounting and persistent state

The RC model charges transaction bytes and signatures through existing accounting. The implementation adds work weights per input and output and grant-state work for scoped inputs. Scoped batches charge the actor; active-only batches use the first sorted input source. The existing `multiop` reporting bucket is reused to avoid changing the persistent RC statistics object layout. The weights remain estimates and need economics review.

The grant and high-water objects add persistent state. The live-record cap bounds scans but does not remove the one high-water metadata row per grantor that has created a grant. Snapshot and replay checks passed for the populated local chain described in the testing notes.

## Deployment boundary

The four operations are available only in the alternate-chain experimental HF30 path. The production-configured evaluator returns explicit disabled errors. Do not use this branch on mainnet. A future deployment requires a maintainer-assigned hardfork, migration and snapshot policy, review of operation tags in all supported builds, and an independent audit.
