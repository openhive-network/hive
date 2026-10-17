# Scoped Authorities Protocol Specification

**Status: proposal; no consensus implementation is present in this checkout.**

## Goals and non-goals

Scoped Authorities would let an account delegate a narrowly defined ability to make direct liquid HIVE or HBD transfers. Consensus state, signed transactions, block time, and normal balance checks are the sole enforcement inputs. Existing owner, active, and posting authorities remain unchanged. A scoped grant does not authorize legacy `transfer_operation` or any operation other than the dedicated scoped transfer.

Version one excludes savings, recurrent transfers, escrow, vesting, conversions, orders, proposals, account/recovery changes, and every other operation. It uses no price conversion and no floating point.

## Protocol design

Three regular operations are proposed: `create_scoped_authority_operation`, `revoke_scoped_authority_operation`, and `scoped_transfer_operation`. No update operation is provided. Changing limits requires revoking the old grant and creating a new grant; the new grant has independent counters and an explicit new grant identifier. This prevents a limit update from resetting counters implicitly.

Creation and revocation require the grantor's **active** authority. This matches the authority used for liquid transfers and allows recovery through normal owner-to-active authority upgrade semantics. Owner authorization can satisfy an active requirement under current transaction verification rules; the owner key is not separately required. Scoped transfer requires the actor's normal active account authority, never the grantor's authority. It has no `account_auths` mechanism and is not added to authority recursion or signature verification shortcuts.

The proposed grant record contains a monotonically allocated chain-local ID, grantor and actor account IDs, asset symbol, per-transfer/lifetime/period amounts, period seconds, valid-from and expiration timestamps, sorted recipient account IDs (empty means unrestricted), lifetime spent, current period index and period spent, and a revoked bit. A unique index on `(grantor, grant_id)` and lookup index on `(actor, grantor, grant_id)` support deterministic access. Creation is bounded per grantor; allowlists have a protocol maximum. The precise constants require maintainer review before implementation.

## Operation schemas (proposed JSON)

```json
{"type":"create_scoped_authority_operation","value":{
  "grantor":"alice","actor":"bob","grant_id":1,"symbol":"HBD",
  "max_per_transfer":{"amount":"5000","precision":3,"nai":"@@000000013"},
  "max_lifetime":{"amount":"250000","precision":3,"nai":"@@000000013"},
  "max_per_period":{"amount":"50000","precision":3,"nai":"@@000000013"},
  "period_seconds":86400,"valid_from":"2026-11-01T00:00:00",
  "expires_at":"2026-11-08T00:00:00","allowed_recipients":["merchant1"]}}
```

```json
{"type":"scoped_transfer_operation","value":{
  "grantor":"alice","actor":"bob","grant_id":1,"to":"merchant1",
  "amount":{"amount":"5000","precision":3,"nai":"@@000000013"},"memo":"invoice"}}
```

Revocation identifies `(grantor, grant_id)` and requires grantor active authority. The exact JSON encoding must be confirmed against operation serializer conventions during implementation.

## Spending and timestamp semantics

All amounts use positive canonical `asset` integer units. For every successful scoped transfer, consensus validates the asset, destination, grant identity, actor signature, time interval, grant status, balance, and each spending ceiling before changing state. Lifetime and period additions use checked arithmetic; a failed check throws and the transaction's normal undo session rolls back all balance and counter writes.

The valid interval is half-open: `valid_from <= head_block_time < expires_at`. The fixed period index is `(head_block_time - valid_from) / period_seconds`; the period counter logically starts at zero when the index changes. Time is consensus block time. Lifetime spent never resets. Multiple operations in one transaction, multiple transactions in one block, and multiple grants use their own persistent counters in operation order.

Exact-expiration transfers fail. Expired and revoked objects remain stored as tombstones so IDs cannot be reused and API status is inspectable; per-account grant caps bound persistent state. Tombstone pruning is not part of v1.

## Hardfork and compatibility

This checkout contains HF28 as its mainnet maximum (`HIVE_NUM_HARDFORKS` is 28 outside alternate-chain builds); HF29 is defined for alternate-chain builds and is not the feature's activation point. A future hardfork slot and version require explicit maintainer assignment. The feature must not be tied to HF29 or activated on mainnet by this branch.

Existing regular operation tags are 0–49 and existing virtual-operation tags follow. Inserting operations before virtual operations shifts their binary tags and is not acceptable without a separately reviewed serialization migration. The implementation must add operations without changing any preexisting tag, and verify binary round-trip plus historical replay. Before the assigned activation all new operations must be rejected and no grant index/state may alter legacy behavior. At activation, the grant index is empty; there is no migration of existing accounts.

## Security invariants

1. Only grantor active-authorized creation/revocation changes grant policy.
2. Actor active authority authorizes only the dedicated operation and cannot confer authority over the grantor.
3. A transfer consumes exactly one matching grant's counters.
4. Every limit and destination check precedes transfer success; state is transactionally undoable.
5. Existing transfer and other operations remain governed by their existing authority paths.
6. No client clock, oracle, background worker, or off-chain state affects acceptance.
7. Expiration, revocation, and usage are based on deterministic state and block order.

