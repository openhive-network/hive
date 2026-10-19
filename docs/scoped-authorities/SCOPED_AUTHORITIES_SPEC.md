# Scoped Authorities Protocol Specification

**Status: the alternate-chain prototype activates Scoped Authorities, Compressed Batch Transfers, and grant lifecycle rules together at one provisional development-only HF30. The identifier is not an official mainnet hardfork assignment. Deployment remains subject to upstream review and independent security validation.**

## Goals and non-goals

Scoped Authorities let an account delegate a narrowly defined ability to make direct liquid HIVE or HBD transfers. Consensus state, signed transactions, block time, and normal balance checks are the sole enforcement inputs. Existing owner, active, and posting authorities remain unchanged. A scoped grant does not authorize legacy `transfer_operation` or any operation other than the dedicated scoped transfer.

Version one excludes savings, recurrent transfers, escrow, vesting, conversions, orders, proposals, account/recovery changes, and every other operation. It uses no price conversion and no floating point.

## Protocol design

Four regular operations are implemented: `create_scoped_authority_operation`, `revoke_scoped_authority_operation`, `scoped_transfer_operation`, and `compressed_batch_transfer_operation`. No update operation is provided. Changing limits requires revoking the old grant and creating a new grant with a higher grant identifier; each grant has independent counters.

Creation and revocation require the grantor's **active** authority. This matches the authority used for liquid transfers and allows recovery through normal owner-to-active authority upgrade semantics. Owner authorization can satisfy an active requirement under current transaction verification rules; the owner key is not separately required. Scoped transfer requires the actor's normal active account authority, never the grantor's authority. It has no `account_auths` mechanism and is not added to authority recursion or signature verification shortcuts.

The grant record contains the grantor and actor account names, caller-supplied `uint32_t` grant ID, asset-specific per-transfer/lifetime/period amounts, period seconds, valid-from and expiration timestamps, sorted recipient account names (empty means unrestricted), lifetime spent, current period index and period spent, and a revoked bit. A unique index on `(grantor, grant_id)` and lookup index on `(actor, grantor, grant_id)` support deterministic access. Each grantor has one persistent high-water ID record created atomically with its first grant. IDs must strictly increase, may be skipped, and cannot wrap or be reused. A grantor can retain at most 100 live grants; revoked and expired rows are pruned during later grant creation. The cap bounds scans to at most 100 rows, and pruning never removes a usable grant. Recipient lists are bounded at 32 accounts.

## Operation schemas (JSON shape)

```json
{"type":"create_scoped_authority_operation","value":{
  "grantor":"alice","actor":"bob","grant_id":1,
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

Revocation identifies `(grantor, grant_id)` and requires grantor active authority. Its static-variant JSON shape is `{"type":"revoke_scoped_authority_operation","value":{"grantor":"alice","grant_id":1}}`. These JSON shapes reflect protocol field names; wallet/RPC signing interoperability remains to be demonstrated.

## Spending and timestamp semantics

All amounts use positive canonical `asset` integer units. For every successful scoped transfer, consensus validates the asset, destination, grant identity, actor signature, time interval, grant status, balance, and each spending ceiling before changing state. Lifetime and period additions use checked arithmetic; a failed check throws and the transaction's normal undo session rolls back all balance and counter writes.

The valid interval is half-open: `valid_from <= head_block_time < expires_at`. The fixed period index is `(head_block_time - valid_from) / period_seconds`; the period counter logically starts at zero when the index changes. Time is consensus block time. Lifetime spent never resets. Multiple operations in one transaction, multiple transactions in one block, and multiple grants use their own persistent counters in operation order.

Exact-expiration transfers fail. At the unified HF30 activation, grant creation deterministically removes that grantor's revoked rows and rows where `expires_at <= head_block_time`. Cleanup is lazy, bounded by the 100-live-grant cap, and runs in the creating transaction's undo session. The high-water metadata survives deletion, including deletion of the grantor's last grant. Queries omit pruned records and return `last_issued_grant_id`; historical operations remain in transaction and account history. Creation RC includes a scan weight for each retained row and charges the metadata estimate only on first creation for that grantor.

## Hardfork and compatibility

This checkout contains HF28 as its mainnet maximum (`HIVE_NUM_HARDFORKS` is 28 outside alternate-chain builds). Alternate-chain HF30 is the single provisional activation for all four operations and all grant lifecycle semantics. Mainnet builds do not include HF30 in their configured maximum and reject the operations in evaluator code.

Existing regular operation tags are 0–49 and existing virtual-operation tags are 50–92. The four new regular operations are appended with tags 93–96. The normal-variant unit test checks these tags and non-virtual classification. The normal variant has a legacy-transfer binary golden fixture. The skip-VOPS `hive_protocol` target builds, but the full node cannot build with that macro because existing chain paths construct virtual operations omitted from the variant; an alternate-variant serialization round trip remains unverified. Before HF30, all four scoped and batch operations are rejected. HF30 itself requires no migration: issuance metadata is initialized during the first grant creation.

## Security invariants

1. Only grantor active-authorized creation/revocation changes grant policy.
2. Actor active authority authorizes only the dedicated operation and cannot confer authority over the grantor.
3. A transfer consumes exactly one matching grant's counters.
4. Every limit and destination check precedes transfer success; state is transactionally undoable.
5. Existing transfer and other operations remain governed by their existing authority paths.
6. No client clock, oracle, background worker, or off-chain state affects acceptance.
7. Expiration, revocation, and usage are based on deterministic state and block order.
8. HF30 pruning removes only revoked or expired records; the permanent per-grantor high-water mark prevents identifier reuse.
